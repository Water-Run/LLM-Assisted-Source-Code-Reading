/*
** ============================================================================
** 文件概要说明
** ============================================================================
** 文件名: lua.h
** 版本: Lua 5.5.0
** 作用: Lua C API 的主头文件
**
** 这是 Lua 解释器与 C 语言交互的核心接口文件。它定义了:
** 1. Lua 的基本数据类型 (nil, boolean, number, string, table, function 等)
** 2. Lua 虚拟栈操作函数 (push/pop/get/set 等)
** 3. Lua 状态机 (lua_State) 的管理函数
** 4. C 函数与 Lua 交互的机制
** 5. 垃圾回收控制接口
** 6. 调试接口
**
** 重要概念:
** - lua_State: Lua 虚拟机的状态,所有操作都需要这个参数
** - 虚拟栈: Lua 与 C 交互的主要方式,通过栈传递数据
** - 索引机制: 正索引从栈底开始(1,2,3...),负索引从栈顶开始(-1,-2,-3...)
** - 伪索引: 特殊的索引值,用于访问注册表等特殊位置
**
** C 语言知识点:
** - #ifndef/#define/#endif: 头文件保护,防止重复包含
** - typedef: 类型别名定义
** - 函数指针: 如 lua_CFunction, lua_Alloc 等
** - 可变参数: va_list, ... 等
** - 宏定义: #define 用于定义常量和简单函数
** ============================================================================
*/

/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
** See Copyright Notice at the end of this file
*/

/*
** 头文件保护机制 - 防止同一个头文件被多次包含
** #ifndef 检查 lua_h 是否未定义
** #define 定义 lua_h
** #endif 结束条件编译
** 这是 C 语言的标准做法,避免重复定义导致编译错误
*/
#ifndef lua_h
#define lua_h

/*
** 包含标准 C 库头文件
** stdarg.h: 提供可变参数列表支持 (va_list, va_start, va_end 等)
** stddef.h: 提供基础类型定义 (size_t, ptrdiff_t, NULL 等)
*/
#include <stdarg.h>
#include <stddef.h>

/* 版权和作者信息 - 使用宏定义便于维护 */
#define LUA_COPYRIGHT LUA_RELEASE "  Copyright (C) 1994-2025 Lua.org, PUC-Rio"
#define LUA_AUTHORS "R. Ierusalimschy, L. H. de Figueiredo, W. Celes"

/*
** Lua 版本号定义
** 采用三段式版本号: 主版本.次版本.发布版本
** 例如: 5.5.0 表示主版本 5, 次版本 5, 发布版本 0
*/
#define LUA_VERSION_MAJOR_N 5   /* 主版本号 */
#define LUA_VERSION_MINOR_N 5   /* 次版本号 */
#define LUA_VERSION_RELEASE_N 0 /* 发布版本号 */

/*
** 版本号的数值表示
** LUA_VERSION_NUM: 主版本*100 + 次版本 = 505
** 这样可以方便地进行数值比较,例如 #if LUA_VERSION_NUM >= 503
*/
#define LUA_VERSION_NUM (LUA_VERSION_MAJOR_N * 100 + LUA_VERSION_MINOR_N)

/*
** 完整版本号的数值表示
** LUA_VERSION_RELEASE_NUM: (主版本*100 + 次版本)*100 + 发布版本 = 50500
** 提供更精细的版本比较能力
*/
#define LUA_VERSION_RELEASE_NUM (LUA_VERSION_NUM * 100 + LUA_VERSION_RELEASE_N)

/*
** 包含 Lua 配置文件
** luaconf.h 包含了平台相关的配置和类型定义
** 例如 LUA_NUMBER, LUA_INTEGER 等类型的具体定义
*/
#include "luaconf.h"

/*
** 预编译代码的标记 ('<esc>Lua')
** \x1b 是 ESC 字符的十六进制表示 (ASCII 27)
** 这个签名用于识别 Lua 编译后的字节码文件
** 当加载二进制 chunk 时,Lua 会检查这个签名确保文件格式正确
*/
/* mark for precompiled code ('<esc>Lua') */
#define LUA_SIGNATURE "\x1bLua"

/*
** 多返回值标记
** 在 lua_pcall 和 lua_call 中使用
** 表示函数可以返回任意数量的结果
** 例如: lua_call(L, 0, LUA_MULTRET) 表示接受所有返回值
*/
/* option for multiple returns in 'lua_pcall' and 'lua_call' */
#define LUA_MULTRET (-1)

/*
** ============================================================================
** 伪索引 (Pseudo-indices)
** ============================================================================
**
** 伪索引是特殊的索引值,不指向栈上的元素,而是指向特殊的位置:
**
** 1. 注册表 (Registry): 一个全局的表,C 代码可以在其中存储 Lua 值
**    - 索引: LUA_REGISTRYINDEX
**    - 用途: 存储全局数据,避免污染 Lua 的全局环境
**
** 2. Upvalues: 闭包的上值,通过 lua_upvalueindex(i) 访问
**    - i 从 1 开始计数
**    - 用于实现 C 闭包,存储函数的私有数据
**
** 设计说明:
** - 栈大小限制为 INT_MAX/2 (整数最大值的一半)
** - 在 INT_MAX/2 之后保留一些空间用于溢出检测
** - 减 1000 是为了确保伪索引不会与栈索引冲突
*/
/*
** Pseudo-indices
** (The stack size is limited to INT_MAX/2; we keep some free empty
** space after that to help overflow detection.)
*/
/*
** 注册表索引
** 计算方式: -(INT_MAX/2 + 1000)
** 这是一个很大的负数,确保不会与普通栈索引冲突
*/
#define LUA_REGISTRYINDEX (-(INT_MAX / 2 + 1000))

/*
** Upvalue 索引计算宏
** 参数 i: upvalue 的编号 (从 1 开始)
** 返回值: 对应的伪索引
** 例如: lua_upvalueindex(1) 返回第一个 upvalue 的索引
**
** 工作原理: 从 LUA_REGISTRYINDEX 向下递减
** 第 1 个 upvalue: LUA_REGISTRYINDEX - 1
** 第 2 个 upvalue: LUA_REGISTRYINDEX - 2
** 以此类推
*/
#define lua_upvalueindex(i) (LUA_REGISTRYINDEX - (i))

/*
** ============================================================================
** 线程状态码 (Thread Status)
** ============================================================================
** 这些常量表示 Lua 协程 (coroutine) 的执行状态
*/
/* thread status */
#define LUA_OK 0        /* 正常状态,无错误 */
#define LUA_YIELD 1     /* 协程挂起状态 (yield) */
#define LUA_ERRRUN 2    /* 运行时错误 */
#define LUA_ERRSYNTAX 3 /* 语法错误 */
#define LUA_ERRMEM 4    /* 内存分配错误 */
#define LUA_ERRERR 5    /* 错误处理函数本身出错 */

/*
** ============================================================================
** Lua 状态机结构体 (前向声明)
** ============================================================================
** lua_State 是 Lua 虚拟机的核心数据结构
** 这里使用前向声明,实际定义在实现文件中
**
** 每个 lua_State 代表一个独立的 Lua 执行环境:
** - 有自己的栈
** - 有自己的全局环境
** - 可以有多个 lua_State (用于多线程或协程)
**
** C 语言知识点:
** typedef struct lua_State lua_State;
** 这是结构体的前向声明,允许在定义之前使用指针类型
*/
typedef struct lua_State lua_State;

/*
** ============================================================================
** Lua 基本类型常量
** ============================================================================
** 这些常量定义了 Lua 的所有基本数据类型
** 使用 lua_type() 函数可以获取一个值的类型
*/
/*
** basic types
*/
#define LUA_TNONE (-1) /* 无效索引,不是真正的类型 */

#define LUA_TNIL 0           /* nil 类型 - 表示空值或不存在 */
#define LUA_TBOOLEAN 1       /* boolean 类型 - true 或 false */
#define LUA_TLIGHTUSERDATA 2 /* 轻量用户数据 - 就是 C 指针,不受 GC 管理 */
#define LUA_TNUMBER 3        /* number 类型 - 数值 (整数或浮点数) */
#define LUA_TSTRING 4        /* string 类型 - 字符串 */
#define LUA_TTABLE 5         /* table 类型 - 表 (关联数组) */
#define LUA_TFUNCTION 6      /* function 类型 - 函数 */
#define LUA_TUSERDATA 7      /* 完整用户数据 - 由 Lua 管理的 C 数据,可以有元表 */
#define LUA_TTHREAD 8        /* thread 类型 - 协程 */

#define LUA_NUMTYPES 9 /* 类型总数 (不包括 LUA_TNONE) */

/*
** C 函数可用的最小 Lua 栈空间
** 当 C 函数被调用时,Lua 保证至少有 20 个栈位可用
** 如果需要更多空间,需要调用 lua_checkstack() 检查并扩展
*/
/* minimum Lua stack available to a C function */
#define LUA_MINSTACK 20

/*
** ============================================================================
** 注册表中的预定义索引
** ============================================================================
** 注册表是一个特殊的表,只能被 C 代码访问
** 这些常量定义了注册表中的特殊位置
*/
/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */
/* 索引 1 保留给引用机制使用 (luaL_ref/luaL_unref) */
#define LUA_RIDX_GLOBALS 2    /* 全局环境表的位置 */
#define LUA_RIDX_MAINTHREAD 3 /* 主线程的位置 */
#define LUA_RIDX_LAST 3       /* 最后一个预定义索引 */

/*
** ============================================================================
** 数值类型定义
** ============================================================================
** 这些类型的实际定义在 luaconf.h 中
** 可以根据平台进行配置
*/

/*
** Lua 中数值的类型
** 通常定义为 double (双精度浮点数)
** 在 luaconf.h 中可以配置为其他类型
*/
/* type of numbers in Lua */
typedef LUA_NUMBER lua_Number;

/*
** Lua 整数类型
** 通常定义为 long long 或 __int64
** 用于表示整数值,避免浮点数的精度问题
*/
/* type for integer functions */
typedef LUA_INTEGER lua_Integer;

/*
** 无符号整数类型
** 用于位运算等需要无符号整数的场合
*/
/* unsigned integer type */
typedef LUA_UNSIGNED lua_Unsigned;

/*
** 延续函数的上下文类型
** 用于协程和异步操作中传递额外信息
** 通常是一个指针大小的整数类型
*/
/* type for continuation-function contexts */
typedef LUA_KCONTEXT lua_KContext;

/*
** ============================================================================
** 函数指针类型定义
** ============================================================================
**
** C 语言知识点 - 函数指针:
** typedef int (*lua_CFunction) (lua_State *L);
**
** 解析:
** - int: 返回类型
** - (*lua_CFunction): 指向函数的指针,类型名为 lua_CFunction
** - (lua_State *L): 参数列表
**
** 使用示例:
** int my_function(lua_State *L) { return 0; }
** lua_CFunction func = my_function;
*/

/*
** 注册到 Lua 的 C 函数类型
**
** 参数: lua_State *L - Lua 状态机指针
** 返回值: int - 返回值的个数 (不是返回值本身)
**
** 工作原理:
** 1. C 函数从 Lua 栈获取参数
** 2. 执行计算
** 3. 将结果压入栈
** 4. 返回结果的个数
**
** 示例:
** int add(lua_State *L) {
**     double a = lua_tonumber(L, 1);  // 获取第一个参数
**     double b = lua_tonumber(L, 2);  // 获取第二个参数
**     lua_pushnumber(L, a + b);       // 压入结果
**     return 1;                        // 返回 1 个结果
** }
*/
/*
** Type for C functions registered with Lua
*/
typedef int (*lua_CFunction)(lua_State *L);

/*
** 延续函数类型
**
** 用于协程和可恢复的函数调用
**
** 参数:
** - lua_State *L: Lua 状态机
** - int status: 状态码 (LUA_OK, LUA_YIELD 等)
** - lua_KContext ctx: 上下文信息,用于在挂起/恢复之间传递数据
**
** 返回值: 返回值的个数
**
** 应用场景: 需要跨越 yield 操作保存状态的函数
*/
/*
** Type for continuation functions
*/
typedef int (*lua_KFunction)(lua_State *L, int status, lua_KContext ctx);

/*
** ============================================================================
** 读写函数类型
** ============================================================================
** 用于 lua_load 和 lua_dump 的 I/O 操作
*/

/*
** 读取函数类型
**
** 用于 lua_load() 从自定义来源加载 Lua 代码
**
** 参数:
** - lua_State *L: Lua 状态机
** - void *ud: 用户数据,由调用者传入
** - size_t *sz: 输出参数,返回读取的字节数
**
** 返回值: 指向数据块的指针,返回 NULL 表示结束
**
** 工作流程:
** 1. lua_load 反复调用这个函数
** 2. 每次返回一块数据
** 3. 返回 NULL 表示没有更多数据
*/
/*
** Type for functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char *(*lua_Reader)(lua_State *L, void *ud, size_t *sz);

/*
** 写入函数类型
**
** 用于 lua_dump() 将编译后的代码写到自定义目标
**
** 参数:
** - lua_State *L: Lua 状态机
** - const void *p: 要写入的数据
** - size_t sz: 数据大小
** - void *ud: 用户数据
**
** 返回值: 0 表示成功,非 0 表示错误
**
** 应用: 将 Lua 函数序列化到文件、内存或网络
*/
typedef int (*lua_Writer)(lua_State *L, const void *p, size_t sz, void *ud);

/*
** 内存分配函数类型
**
** Lua 使用这个函数进行所有的内存操作
** 可以自定义内存分配器,用于:
** - 内存使用统计
** - 内存池管理
** - 调试内存泄漏
**
** 参数:
** - void *ud: 用户数据
** - void *ptr: 要重新分配的内存块 (NULL 表示新分配)
** - size_t osize: 原大小
** - size_t nsize: 新大小
**
** 返回值: 新内存块的指针
**
** 行为规范:
** - 当 nsize == 0: 释放内存,返回 NULL
** - 当 ptr == NULL: 分配新内存
** - 其他情况: 重新分配内存
**
** 等价关系:
** - malloc(nsize)   ≈ alloc(ud, NULL, 0, nsize)
** - free(ptr)       ≈ alloc(ud, ptr, osize, 0)
** - realloc(ptr,sz) ≈ alloc(ud, ptr, osize, nsize)
*/
/*
** Type for memory-allocation functions
*/
typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

/*
** 警告函数类型
**
** 用于接收 Lua 发出的警告消息
**
** 参数:
** - void *ud: 用户数据
** - const char *msg: 警告消息
** - int tocont: 是否还有后续消息 (1=是, 0=否)
**
** 用途: 自定义警告处理,例如写入日志文件
*/
/*
** Type for warning functions
*/
typedef void (*lua_WarnFunction)(void *ud, const char *msg, int tocont);

/*
** ============================================================================
** 调试相关类型
** ============================================================================
*/

/*
** 调试信息结构体 (前向声明)
** 实际定义在文件末尾
** 用于存储函数调用、源码位置等调试信息
*/
/*
** Type used by the debug API to collect debug information
*/
typedef struct lua_Debug lua_Debug;

/*
** 调试钩子函数类型
**
** 在特定事件发生时被调用 (函数调用、返回、执行行等)
**
** 参数:
** - lua_State *L: Lua 状态机
** - lua_Debug *ar: 调试信息结构
**
** 应用: 实现调试器、性能分析器等
*/
/*
** Functions to be called by the debugger in specific events
*/
typedef void (*lua_Hook)(lua_State *L, lua_Debug *ar);

/*
** 用户自定义头文件包含
** 如果定义了 LUA_USER_H,则包含该文件
** 用于添加自定义的声明和定义
**
** C 语言知识点 - 条件编译:
** #if defined(MACRO) 检查宏是否定义
** #include 可以包含宏展开的结果
*/
/*
** generic extra include file
*/
#if defined(LUA_USER_H)
#include LUA_USER_H
#endif

/*
** RCS 标识字符串
** RCS (Revision Control System) 是版本控制系统
** 这个字符串用于版本跟踪
**
** C 语言知识点:
** extern const char - 声明外部常量,定义在其他编译单元
*/
/*
** RCS ident string
*/
extern const char lua_ident[];

/*
** ============================================================================
** 状态机操作函数
** ============================================================================
** 这些函数用于创建、销毁和管理 Lua 状态机
**
** C 语言知识点 - LUA_API:
** LUA_API 是一个宏,通常定义为:
** - Windows: __declspec(dllexport) 或 __declspec(dllimport)
** - Unix/Linux: 可能为空或 __attribute__((visibility("default")))
** 用于控制符号的导出,支持动态链接库
*/
/*
** state manipulation
*/

/*
** 创建新的 Lua 状态机
**
** 参数:
** - lua_Alloc f: 内存分配函数
** - void *ud: 传递给分配函数的用户数据
** - unsigned seed: 随机数种子,用于表的哈希等
**
** 返回值: 新创建的 lua_State 指针,失败返回 NULL
**
** 说明:
** - 这是创建独立 Lua 虚拟机的底层函数
** - 通常使用更高级的 luaL_newstate() (在 lauxlib.h 中)
** - 每个状态机都是独立的,有自己的全局环境
*/
LUA_API lua_State *(lua_newstate)(lua_Alloc f, void *ud, unsigned seed);

/*
** 关闭 Lua 状态机
**
** 参数: lua_State *L - 要关闭的状态机
**
** 作用:
** - 释放所有内存
** - 调用所有 __gc 元方法
** - 关闭所有待关闭的值
**
** 注意: 调用后 L 指针无效,不能再使用
*/
LUA_API void(lua_close)(lua_State *L);

/*
** 创建新线程 (协程)
**
** 参数: lua_State *L - 父状态机
**
** 返回值: 新线程的 lua_State 指针
**
** 说明:
** - 新线程与 L 共享全局环境
** - 新线程被压入 L 的栈顶
** - 用于实现协程功能
*/
LUA_API lua_State *(lua_newthread)(lua_State * L);

/*
** 关闭线程
**
** 参数:
** - lua_State *L: 要关闭的线程
** - lua_State *from: 从哪个线程关闭 (用于错误传播)
**
** 返回值: 状态码
**
** 作用: 关闭待关闭的值,清理资源
*/
LUA_API int(lua_closethread)(lua_State *L, lua_State *from);

/*
** 设置 panic 函数
**
** 参数:
** - lua_State *L: Lua 状态机
** - lua_CFunction panicf: panic 函数
**
** 返回值: 旧的 panic 函数
**
** 说明:
** - panic 函数在无保护的错误发生时被调用
** - 默认行为是调用 exit()
** - 通常用于记录致命错误日志
*/
LUA_API lua_CFunction(lua_atpanic)(lua_State *L, lua_CFunction panicf);

/*
** 获取 Lua 版本号
**
** 参数: lua_State *L - 可以为 NULL
**
** 返回值: 版本号 (lua_Number 类型)
**
** 用途: 检查运行时和编译时的版本是否匹配
*/
LUA_API lua_Number(lua_version)(lua_State *L);

/*
** ============================================================================
** 基本栈操作
** ============================================================================
**
** Lua 栈是 C API 的核心:
** - 所有数据交换都通过栈进行
** - 栈索引从 1 开始 (栈底) 或从 -1 开始 (栈顶)
** - 正索引: 1, 2, 3, ... (从栈底向上)
** - 负索引: -1, -2, -3, ... (从栈顶向下)
**
** 例如,栈中有 3 个元素:
** 索引:  1    2    3     (正索引)
** 值:   [a]  [b]  [c]
** 索引: -3   -2   -1     (负索引)
*/
/*
** basic stack manipulation
*/

/*
** 将索引转换为绝对索引
**
** 参数:
** - lua_State *L: Lua 状态机
** - int idx: 可接受的索引 (正、负或伪索引)
**
** 返回值: 对应的绝对 (正) 索引
**
** 示例: 栈有 5 个元素
** - lua_absindex(L, -1) 返回 5
** - lua_absindex(L, 1) 返回 1
*/
LUA_API int(lua_absindex)(lua_State *L, int idx);

/*
** 获取栈顶索引 (即栈中元素个数)
**
** 返回值: 栈中元素个数
**
** 示例:
** - 空栈: 返回 0
** - 有 3 个元素: 返回 3
*/
LUA_API int(lua_gettop)(lua_State *L);

/*
** 设置栈顶
**
** 参数:
** - lua_State *L: Lua 状态机
** - int idx: 新的栈顶索引
**
** 作用:
** - 如果 idx > 当前栈顶: 用 nil 填充
** - 如果 idx < 当前栈顶: 删除多余元素
** - 如果 idx == 0: 清空栈
**
** 示例:
** - lua_settop(L, 0) 清空栈
** - lua_settop(L, -2) 删除栈顶元素
*/
LUA_API void(lua_settop)(lua_State *L, int idx);

/*
** 将指定索引的值的副本压入栈
**
** 参数: int idx - 要复制的元素索引
**
** 示例:
** 栈: [a] [b] [c]
** lua_pushvalue(L, 1)
** 结果: [a] [b] [c] [a]
*/
LUA_API void(lua_pushvalue)(lua_State *L, int idx);

/*
** 旋转栈元素
**
** 参数:
** - int idx: 旋转的起始位置
** - int n: 旋转步数 (正数向栈顶,负数向栈底)
**
** 说明:
** - 将 [idx, top] 区间的元素旋转 n 个位置
** - 正数: 底部元素移到顶部
** - 负数: 顶部元素移到底部
**
** 示例:
** 栈: [a] [b] [c] [d]
** lua_rotate(L, 1, 2)
** 结果: [c] [d] [a] [b]
*/
LUA_API void(lua_rotate)(lua_State *L, int idx, int n);

/*
** 复制元素
**
** 参数:
** - int fromidx: 源索引
** - int toidx: 目标索引
**
** 作用: 将 fromidx 的值复制到 toidx,不影响其他元素
**
** 示例:
** 栈: [a] [b] [c]
** lua_copy(L, 1, 3)
** 结果: [a] [b] [a]
*/
LUA_API void(lua_copy)(lua_State *L, int fromidx, int toidx);

/*
** 检查栈空间
**
** 参数: int n - 需要的额外栈位数
**
** 返回值: 1 表示成功,0 表示无法分配
**
** 说明:
** - Lua 保证至少有 LUA_MINSTACK (20) 个栈位
** - 如果需要更多,必须调用此函数
** - 失败可能是因为内存不足
*/
LUA_API int(lua_checkstack)(lua_State *L, int n);

/*
** 在两个状态机之间移动元素
**
** 参数:
** - lua_State *from: 源状态机
** - lua_State *to: 目标状态机
** - int n: 要移动的元素个数
**
** 说明:
** - 从 from 的栈顶弹出 n 个元素
** - 压入 to 的栈顶
** - from 和 to 必须属于同一个全局状态
*/
LUA_API void(lua_xmove)(lua_State *from, lua_State *to, int n);

/*
** ============================================================================
** 访问函数 (栈 -> C)
** ============================================================================
** 这些函数从 Lua 栈读取值到 C
*/
/*
** access functions (stack -> C)
*/

/*
** 检查是否为数值
**
** 参数: int idx - 栈索引
**
** 返回值: 1 表示是数值或可转换为数值的字符串,0 表示不是
*/
LUA_API int(lua_isnumber)(lua_State *L, int idx);

/*
** 检查是否为字符串或数值 (数值可以转为字符串)
*/
LUA_API int(lua_isstring)(lua_State *L, int idx);

/*
** 检查是否为 C 函数
*/
LUA_API int(lua_iscfunction)(lua_State *L, int idx);

/*
** 检查是否为整数
*/
LUA_API int(lua_isinteger)(lua_State *L, int idx);

/*
** 检查是否为用户数据 (包括完整和轻量)
*/
LUA_API int(lua_isuserdata)(lua_State *L, int idx);

/*
** 获取类型
**
** 返回值: LUA_TNIL, LUA_TNUMBER 等类型常量
*/
LUA_API int(lua_type)(lua_State *L, int idx);

/*
** 获取类型名称
**
** 参数: int tp - 类型常量
**
** 返回值: 类型名字符串 ("nil", "number", "string" 等)
*/
LUA_API const char *(lua_typename)(lua_State * L, int tp);

/*
** 转换为数值
**
** 参数:
** - int idx: 栈索引
** - int *isnum: 输出参数,如果非 NULL,设置是否成功转换
**
** 返回值: 转换后的数值,失败返回 0
**
** 说明: 可以转换数值和数值字符串
*/
LUA_API lua_Number(lua_tonumberx)(lua_State *L, int idx, int *isnum);

/*
** 转换为整数
**
** 类似 lua_tonumberx,但转换为整数类型
*/
LUA_API lua_Integer(lua_tointegerx)(lua_State *L, int idx, int *isnum);

/*
** 转换为布尔值
**
** 返回值: 1 (true) 或 0 (false)
**
** 规则:
** - nil 和 false 为 false
** - 其他所有值 (包括 0 和空字符串) 为 true
*/
LUA_API int(lua_toboolean)(lua_State *L, int idx);

/*
** 转换为字符串
**
** 参数:
** - int idx: 栈索引
** - size_t *len: 输出参数,字符串长度 (可为 NULL)
**
** 返回值: 字符串指针,失败返回 NULL
**
** 注意:
** - 返回的指针由 Lua 管理,不要 free
** - 字符串可能包含 '\0',使用 len 获取真实长度
** - 会修改栈上的值 (转换为字符串)
*/
LUA_API const char *(lua_tolstring)(lua_State * L, int idx, size_t *len);

/*
** 获取对象长度
**
** 返回值:
** - 字符串: 字节数
** - 表: 数组部分长度
** - 用户数据: 内存块大小
*/
LUA_API lua_Unsigned(lua_rawlen)(lua_State *L, int idx);

/*
** 转换为 C 函数
**
** 返回值: C 函数指针,如果不是 C 函数返回 NULL
*/
LUA_API lua_CFunction(lua_tocfunction)(lua_State *L, int idx);

/*
** 转换为用户数据
**
** 返回值: 用户数据的指针,失败返回 NULL
**
** 说明: 可以获取完整用户数据或轻量用户数据
*/
LUA_API void *(lua_touserdata)(lua_State * L, int idx);

/*
** 转换为线程
**
** 返回值: lua_State 指针,失败返回 NULL
*/
LUA_API lua_State *(lua_tothread)(lua_State * L, int idx);

/*
** 转换为指针
**
** 返回值: 表示对象的唯一指针
**
** 用途: 用于比较对象身份,不能解引用
*/
LUA_API const void *(lua_topointer)(lua_State * L, int idx);

/*
** ============================================================================
** 比较和算术函数
** ============================================================================
*/
/*
** Comparison and arithmetic functions
*/

/*
** 算术操作符常量
** ORDER TM, ORDER OP 是内部注释,表示这些常量的顺序很重要
*/
#define LUA_OPADD 0 /* 加法: + */ /* ORDER TM, ORDER OP */
#define LUA_OPSUB 1               /* 减法: - */
#define LUA_OPMUL 2               /* 乘法: * */
#define LUA_OPMOD 3               /* 取模: % */
#define LUA_OPPOW 4               /* 幂运算: ^ */
#define LUA_OPDIV 5               /* 除法: / */
#define LUA_OPIDIV 6              /* 整除: // */
#define LUA_OPBAND 7              /* 按位与: & */
#define LUA_OPBOR 8               /* 按位或: | */
#define LUA_OPBXOR 9              /* 按位异或: ~ */
#define LUA_OPSHL 10              /* 左移: << */
#define LUA_OPSHR 11              /* 右移: >> */
#define LUA_OPUNM 12              /* 取负: - (一元) */
#define LUA_OPBNOT 13             /* 按位非: ~ (一元) */

/*
** 执行算术运算
**
** 参数: int op - 操作符 (LUA_OPADD 等)
**
** 行为:
** - 二元操作: 弹出两个操作数,压入结果
** - 一元操作: 弹出一个操作数,压入结果
**
** 示例:
** 栈: [3] [4]
** lua_arith(L, LUA_OPADD)
** 结果: [7]
*/
LUA_API void(lua_arith)(lua_State *L, int op);

/*
** 比较操作符常量
*/
#define LUA_OPEQ 0 /* 等于: == */
#define LUA_OPLT 1 /* 小于: < */
#define LUA_OPLE 2 /* 小于等于: <= */

/*
** 原始相等比较
**
** 参数: int idx1, int idx2 - 要比较的两个索引
**
** 返回值: 1 表示相等,0 表示不等
**
** 说明:
** - 不调用元方法
** - 直接比较值
*/
LUA_API int(lua_rawequal)(lua_State *L, int idx1, int idx2);

/*
** 比较两个值
**
** 参数:
** - int idx1, int idx2: 要比较的索引
** - int op: 比较操作符 (LUA_OPEQ, LUA_OPLT, LUA_OPLE)
**
** 返回值: 1 表示真,0 表示假
**
** 说明: 会调用元方法 (__eq, __lt, __le)
*/
LUA_API int(lua_compare)(lua_State *L, int idx1, int idx2, int op);

/*
** ============================================================================
** 压栈函数 (C -> 栈)
** ============================================================================
** 这些函数将 C 值压入 Lua 栈
*/
/*
** push functions (C -> stack)
*/

/* 压入 nil */
LUA_API void(lua_pushnil)(lua_State *L);

/*
** 压入数值
** 参数: lua_Number n - 要压入的数值
*/
LUA_API void(lua_pushnumber)(lua_State *L, lua_Number n);

/*
** 压入整数
** 参数: lua_Integer n - 要压入的整数
*/
LUA_API void(lua_pushinteger)(lua_State *L, lua_Integer n);

/*
** 压入字符串
**
** 参数:
** - const char *s: 字符串指针
** - size_t len: 字符串长度
**
** 返回值: 内部字符串的指针
**
** 说明:
** - Lua 会复制字符串
** - 可以包含 '\0'
** - 会进行字符串内化 (相同字符串只存一份)
*/
LUA_API const char *(lua_pushlstring)(lua_State * L, const char *s, size_t len);

/*
** 压入外部字符串
**
** 参数:
** - const char *s: 字符串指针
** - size_t len: 长度
** - lua_Alloc falloc: 释放函数
** - void *ud: 用户数据
**
** 说明:
** - 用于避免复制大字符串
** - Lua 接管字符串所有权
** - 当不再需要时,调用 falloc 释放
*/
LUA_API const char *(lua_pushexternalstring)(lua_State * L,
                                             const char *s, size_t len, lua_Alloc falloc, void *ud);

/*
** 压入 C 字符串
**
** 参数: const char *s - 以 '\0' 结尾的 C 字符串
**
** 返回值: 内部字符串指针
**
** 说明: 等价于 lua_pushlstring(L, s, strlen(s))
*/
LUA_API const char *(lua_pushstring)(lua_State * L, const char *s);

/*
** 压入格式化字符串 (va_list 版本)
**
** 参数:
** - const char *fmt: 格式字符串 (类似 printf)
** - va_list argp: 参数列表
**
** 返回值: 格式化后的字符串指针
**
** C 语言知识点 - 可变参数:
** va_list 是 stdarg.h 定义的类型,用于访问可变参数
** 配合 va_start, va_arg, va_end 使用
*/
LUA_API const char *(lua_pushvfstring)(lua_State * L, const char *fmt,
                                       va_list argp);

/*
** 压入格式化字符串
**
** 参数:
** - const char *fmt: 格式字符串
** - ...: 可变参数
**
** 返回值: 格式化后的字符串指针
**
** 支持的格式:
** - %s: 字符串
** - %d: 整数
** - %f: 浮点数
** - %p: 指针
** - %%: 百分号
**
** 示例:
** lua_pushfstring(L, "x=%d, y=%d", 10, 20);
*/
LUA_API const char *(lua_pushfstring)(lua_State * L, const char *fmt, ...);

/*
** 压入 C 闭包
**
** 参数:
** - lua_CFunction fn: C 函数
** - int n: upvalue 个数
**
** 说明:
** - 从栈顶弹出 n 个值作为 upvalue
** - 创建闭包并压入栈
** - upvalue 是函数的私有数据
**
** 示例:
** lua_pushinteger(L, 42);  // upvalue
** lua_pushcclosure(L, my_func, 1);
** // my_func 可以通过 lua_upvalueindex(1) 访问 42
*/
LUA_API void(lua_pushcclosure)(lua_State *L, lua_CFunction fn, int n);

/*
** 压入布尔值
**
** 参数: int b - 0 表示 false,非 0 表示 true
*/
LUA_API void(lua_pushboolean)(lua_State *L, int b);

/*
** 压入轻量用户数据
**
** 参数: void *p - C 指针
**
** 说明:
** - 就是一个 C 指针,不受 GC 管理
** - 没有元表
** - 用于传递 C 数据结构的引用
*/
LUA_API void(lua_pushlightuserdata)(lua_State *L, void *p);

/*
** 压入当前线程
**
** 返回值: 1 表示这是主线程,0 表示不是
**
** 作用: 将线程自身压入栈
*/
LUA_API int(lua_pushthread)(lua_State *L);

/*
** ============================================================================
** 获取函数 (Lua -> 栈)
** ============================================================================
** 这些函数从 Lua 对象获取值并压入栈
*/
/*
** get functions (Lua -> stack)
*/

/*
** 获取全局变量
**
** 参数: const char *name - 变量名
**
** 返回值: 获取值的类型
**
** 行为:
** - 从全局表获取 name 对应的值
** - 将值压入栈顶
**
** 等价于 Lua 代码: push(_G[name])
*/
LUA_API int(lua_getglobal)(lua_State *L, const char *name);

/*
** 获取表字段
**
** 参数: int idx - 表的栈索引
**
** 返回值: 获取值的类型
**
** 行为:
** - 从栈顶弹出键
** - 从 idx 位置的表获取对应值
** - 将值压入栈顶
**
** 等价于 Lua 代码:
** key = pop()
** push(table[key])
**
** 注意: 会调用 __index 元方法
*/
LUA_API int(lua_gettable)(lua_State *L, int idx);

/*
** 获取表字段 (字符串键)
**
** 参数:
** - int idx: 表的索引
** - const char *k: 字段名
**
** 返回值: 获取值的类型
**
** 等价于: push(table[k])
*/
LUA_API int(lua_getfield)(lua_State *L, int idx, const char *k);

/*
** 获取表字段 (整数键)
**
** 参数:
** - int idx: 表的索引
** - lua_Integer n: 整数键
**
** 返回值: 获取值的类型
**
** 等价于: push(table[n])
**
** 用途: 访问数组元素
*/
LUA_API int(lua_geti)(lua_State *L, int idx, lua_Integer n);

/*
** 原始获取 (不调用元方法)
**
** 类似 lua_gettable,但不触发 __index
*/
LUA_API int(lua_rawget)(lua_State *L, int idx);

/*
** 原始获取 (整数键)
**
** 类似 lua_geti,但不触发 __index
*/
LUA_API int(lua_rawgeti)(lua_State *L, int idx, lua_Integer n);

/*
** 原始获取 (指针键)
**
** 参数: const void *p - 用作键的指针
**
** 说明: 使用轻量用户数据作为键
*/
LUA_API int(lua_rawgetp)(lua_State *L, int idx, const void *p);

/*
** 创建新表
**
** 参数:
** - int narr: 数组部分的预分配大小
** - int nrec: 哈希部分的预分配大小
**
** 说明:
** - 创建空表并压入栈
** - 预分配可以提高性能,但不是必需的
**
** 性能优化:
** 如果知道表的大小,预分配可以避免多次重新分配
*/
LUA_API void(lua_createtable)(lua_State *L, int narr, int nrec);

/*
** 创建新的完整用户数据
**
** 参数:
** - size_t sz: 内存块大小
** - int nuvalue: 关联用户值的个数
**
** 返回值: 用户数据的指针
**
** 说明:
** - 分配 sz 字节内存
** - 创建用户数据对象并压入栈
** - 可以设置元表和用户值
** - 内存由 Lua GC 管理
*/
LUA_API void *(lua_newuserdatauv)(lua_State * L, size_t sz, int nuvalue);

/*
** 获取元表
**
** 参数: int objindex - 对象的索引
**
** 返回值: 1 表示有元表,0 表示没有
**
** 行为:
** - 如果对象有元表,压入元表并返回 1
** - 否则不压入任何东西并返回 0
*/
LUA_API int(lua_getmetatable)(lua_State *L, int objindex);

/*
** 获取用户数据的用户值
**
** 参数:
** - int idx: 用户数据的索引
** - int n: 用户值的索引 (从 1 开始)
**
** 返回值: 用户值的类型
**
** 说明:
** - 用户值是与用户数据关联的 Lua 值
** - 可以用于存储额外的 Lua 对象
*/
LUA_API int(lua_getiuservalue)(lua_State *L, int idx, int n);

/*
** ============================================================================
** 设置函数 (栈 -> Lua)
** ============================================================================
** 这些函数将栈顶的值设置到 Lua 对象
*/
/*
** set functions (stack -> Lua)
*/

/*
** 设置全局变量
**
** 参数: const char *name - 变量名
**
** 行为:
** - 从栈顶弹出值
** - 设置到全局表
**
** 等价于 Lua 代码:
** _G[name] = pop()
*/
LUA_API void(lua_setglobal)(lua_State *L, const char *name);

/*
** 设置表字段
**
** 参数: int idx - 表的索引
**
** 行为:
** - 从栈顶弹出值 (value)
** - 从栈顶弹出键 (key)
** - table[key] = value
**
** 注意: 会调用 __newindex 元方法
*/
LUA_API void(lua_settable)(lua_State *L, int idx);

/*
** 设置表字段 (字符串键)
**
** 参数:
** - int idx: 表的索引
** - const char *k: 字段名
**
** 行为:
** - 从栈顶弹出值
** - table[k] = value
*/
LUA_API void(lua_setfield)(lua_State *L, int idx, const char *k);

/*
** 设置表字段 (整数键)
**
** 参数:
** - int idx: 表的索引
** - lua_Integer n: 整数键
**
** 行为:
** - 从栈顶弹出值
** - table[n] = value
*/
LUA_API void(lua_seti)(lua_State *L, int idx, lua_Integer n);

/*
** 原始设置 (不调用元方法)
**
** 类似 lua_settable,但不触发 __newindex
*/
LUA_API void(lua_rawset)(lua_State *L, int idx);

/*
** 原始设置 (整数键)
**
** 类似 lua_seti,但不触发 __newindex
*/
LUA_API void(lua_rawseti)(lua_State *L, int idx, lua_Integer n);

/*
** 原始设置 (指针键)
*/
LUA_API void(lua_rawsetp)(lua_State *L, int idx, const void *p);

/*
** 设置元表
**
** 参数: int objindex - 对象的索引
**
** 返回值: 1 表示成功,0 表示失败
**
** 行为:
** - 从栈顶弹出表
** - 设置为对象的元表
**
** 说明:
** - 只有表和完整用户数据可以有独立的元表
** - 其他类型共享每个类型的元表
*/
LUA_API int(lua_setmetatable)(lua_State *L, int objindex);

/*
** 设置用户数据的用户值
**
** 参数:
** - int idx: 用户数据的索引
** - int n: 用户值的索引
**
** 返回值: 1 表示成功,0 表示失败
*/
LUA_API int(lua_setiuservalue)(lua_State *L, int idx, int n);

/*
** ============================================================================
** 加载和调用函数
** ============================================================================
*/
/*
** 'load' and 'call' functions (load and run Lua code)
*/

/*
** 调用函数 (带延续)
**
** 参数:
** - int nargs: 参数个数
** - int nresults: 期望的返回值个数 (LUA_MULTRET 表示所有)
** - lua_KContext ctx: 延续上下文
** - lua_KFunction k: 延续函数
**
** 行为:
** - 从栈上弹出函数和参数
** - 调用函数
** - 将结果压入栈
**
** 示例:
** lua_getglobal(L, "print");  // 函数
** lua_pushstring(L, "hello"); // 参数
** lua_call(L, 1, 0);          // 1 个参数,0 个返回值
**
** 注意: 如果发生错误,会抛出异常,不返回
*/
LUA_API void(lua_callk)(lua_State *L, int nargs, int nresults,
                        lua_KContext ctx, lua_KFunction k);

/*
** 调用函数的宏版本
**
** 这是最常用的调用方式,不支持延续
*/
#define lua_call(L, n, r) lua_callk(L, (n), (r), 0, NULL)

/*
** 保护模式调用 (带延续)
**
** 参数:
** - int nargs: 参数个数
** - int nresults: 期望的返回值个数
** - int errfunc: 错误处理函数的索引 (0 表示无)
** - lua_KContext ctx: 延续上下文
** - lua_KFunction k: 延续函数
**
** 返回值:
** - LUA_OK: 成功
** - LUA_ERRRUN: 运行时错误
** - LUA_ERRMEM: 内存错误
** - LUA_ERRERR: 错误处理函数出错
**
** 说明:
** - 类似 lua_call,但捕获错误而不是抛出
** - 如果有 errfunc,发生错误时调用它
** - 错误对象在栈顶
*/
LUA_API int(lua_pcallk)(lua_State *L, int nargs, int nresults, int errfunc,
                        lua_KContext ctx, lua_KFunction k);

/*
** 保护模式调用的宏版本
**
** 这是最常用的保护调用方式
*/
#define lua_pcall(L, n, r, f) lua_pcallk(L, (n), (r), (f), 0, NULL)

/*
** 加载 Lua 代码块
**
** 参数:
** - lua_Reader reader: 读取函数
** - void *dt: 传递给 reader 的数据
** - const char *chunkname: 代码块名称 (用于错误消息)
** - const char *mode: "b" (二进制), "t" (文本), 或 "bt" (两者)
**
** 返回值:
** - LUA_OK: 成功,编译后的函数在栈顶
** - LUA_ERRSYNTAX: 语法错误
** - LUA_ERRMEM: 内存错误
**
** 说明:
** - 从 reader 读取源代码
** - 编译成函数
** - 不执行,只是压入栈
*/
LUA_API int(lua_load)(lua_State *L, lua_Reader reader, void *dt,
                      const char *chunkname, const char *mode);

/*
** 导出函数
**
** 参数:
** - lua_Writer writer: 写入函数
** - void *data: 传递给 writer 的数据
** - int strip: 是否去除调试信息
**
** 返回值: writer 的返回值
**
** 说明:
** - 将栈顶的函数导出为二进制格式
** - 通过 writer 写出
** - 可以用 lua_load 重新加载
*/
LUA_API int(lua_dump)(lua_State *L, lua_Writer writer, void *data, int strip);

/*
** ============================================================================
** 协程函数
** ============================================================================
*/
/*
** coroutine functions
*/

/*
** 让出协程 (带延续)
**
** 参数:
** - int nresults: 返回值个数
** - lua_KContext ctx: 延续上下文
** - lua_KFunction k: 延续函数
**
** 返回值: 从不返回到原调用者 (恢复时从 k 继续)
**
** 说明:
** - 挂起当前协程
** - 将 nresults 个值返回给 resume 的调用者
*/
LUA_API int(lua_yieldk)(lua_State *L, int nresults, lua_KContext ctx,
                        lua_KFunction k);

/*
** 恢复协程
**
** 参数:
** - lua_State *L: 要恢复的协程
** - lua_State *from: 从哪个协程恢复 (可为 NULL)
** - int narg: 传递的参数个数
** - int *nres: 输出参数,返回值个数
**
** 返回值:
** - LUA_OK: 协程正常结束
** - LUA_YIELD: 协程让出
** - 其他: 错误
**
** 说明:
** - 启动或继续执行协程
** - 从栈传递参数
** - 返回值在栈上
*/
LUA_API int(lua_resume)(lua_State *L, lua_State *from, int narg,
                        int *nres);

/*
** 获取协程状态
**
** 返回值:
** - LUA_OK: 正常 (未启动或已结束)
** - LUA_YIELD: 挂起
** - 其他: 错误状态
*/
LUA_API int(lua_status)(lua_State *L);

/*
** 检查是否可以让出
**
** 返回值: 1 表示可以 yield,0 表示不可以
**
** 说明:
** - 主线程不能 yield
** - C 函数中可能不能 yield (取决于调用方式)
*/
LUA_API int(lua_isyieldable)(lua_State *L);

/*
** 让出协程的宏版本
*/
#define lua_yield(L, n) lua_yieldk(L, (n), 0, NULL)

/*
** ============================================================================
** 警告相关函数
** ============================================================================
*/
/*
** Warning-related functions
*/

/*
** 设置警告函数
**
** 参数:
** - lua_WarnFunction f: 警告函数
** - void *ud: 用户数据
**
** 说明: 设置接收警告消息的函数
*/
LUA_API void(lua_setwarnf)(lua_State *L, lua_WarnFunction f, void *ud);

/*
** 发出警告
**
** 参数:
** - const char *msg: 警告消息
** - int tocont: 是否还有后续消息
**
** 说明:
** - 如果 tocont 为 1,消息会与下一条拼接
** - 用于多行警告消息
*/
LUA_API void(lua_warning)(lua_State *L, const char *msg, int tocont);

/*
** ============================================================================
** 垃圾回收选项
** ============================================================================
*/
/*
** garbage-collection options
*/

/* GC 操作常量 */
#define LUA_GCSTOP 0      /* 停止 GC */
#define LUA_GCRESTART 1   /* 重启 GC */
#define LUA_GCCOLLECT 2   /* 执行完整的 GC 循环 */
#define LUA_GCCOUNT 3     /* 获取内存使用量 (KB) */
#define LUA_GCCOUNTB 4    /* 获取内存使用量的余数 (字节) */
#define LUA_GCSTEP 5      /* 执行一步增量 GC */
#define LUA_GCISRUNNING 6 /* 检查 GC 是否运行 */
#define LUA_GCGEN 7       /* 切换到分代 GC */
#define LUA_GCINC 8       /* 切换到增量 GC */
#define LUA_GCPARAM 9     /* 获取/设置 GC 参数 */

/*
** ============================================================================
** 垃圾回收参数
** ============================================================================
*/
/*
** garbage-collection parameters
*/

/* 分代模式参数 */
/* parameters for generational mode */
#define LUA_GCPMINORMUL 0   /* 控制小回收 - minor collection 的频率 */
#define LUA_GCPMAJORMINOR 1 /* 控制从大回收切换到小回收 - major->minor 的阈值 */
#define LUA_GCPMINORMAJOR 2 /* 控制从小回收切换到大回收 - minor->major 的阈值 */

/* 增量模式参数 */
/* parameters for incremental mode */
#define LUA_GCPPAUSE 3    /* 连续 GC 之间的暂停时间 */
#define LUA_GCPSTEPMUL 4  /* GC "速度" - 每次分配多少内存触发 GC */
#define LUA_GCPSTEPSIZE 5 /* GC 粒度 - 每步回收多少内存 */

/* 参数总数 */
/* number of parameters */
#define LUA_GCPN 6

/*
** 垃圾回收控制
**
** 参数:
** - int what: 操作类型 (LUA_GCSTOP 等)
** - ...: 可变参数,取决于操作类型
**
** 返回值: 取决于操作类型
**
** 示例:
** - lua_gc(L, LUA_GCCOLLECT, 0): 执行完整 GC
** - lua_gc(L, LUA_GCCOUNT, 0): 获取内存使用 (KB)
** - lua_gc(L, LUA_GCSTOP, 0): 停止 GC
*/
LUA_API int(lua_gc)(lua_State *L, int what, ...);

/*
** ============================================================================
** 杂项函数
** ============================================================================
*/
/*
** miscellaneous functions
*/

/*
** 引发错误
**
** 行为:
** - 将栈顶的值作为错误对象
** - 抛出错误
** - 从不返回
**
** 说明:
** - 类似于 Lua 的 error() 函数
** - 会展开栈,调用 __close 元方法
**
** 使用场景:
** C 函数中检测到错误时调用
*/
LUA_API int(lua_error)(lua_State *L);

/*
** 遍历表
**
** 参数: int idx - 表的索引
**
** 返回值: 1 表示还有元素,0 表示遍历结束
**
** 使用方法:
** lua_pushnil(L);  // 初始键
** while (lua_next(L, t)) {
**     // 现在栈上有 'key' (在 -2) 和 'value' (在 -1)
**     printf("%s - %s\n",
**            lua_typename(L, lua_type(L, -2)),
**            lua_typename(L, lua_type(L, -1)));
**     lua_pop(L, 1);  // 移除 value,保留 key 供下次迭代
** }
**
** 注意:
** - 遍历时不要修改表的键
** - 每次迭代弹出 value,保留 key
*/
LUA_API int(lua_next)(lua_State *L, int idx);

/*
** 连接字符串
**
** 参数: int n - 要连接的值的个数
**
** 行为:
** - 从栈顶弹出 n 个值
** - 转换为字符串并连接
** - 将结果压入栈
**
** 说明:
** - 调用 __concat 元方法
** - 数值会自动转换为字符串
*/
LUA_API void(lua_concat)(lua_State *L, int n);

/*
** 获取长度
**
** 参数: int idx - 对象的索引
**
** 行为:
** - 获取对象的长度
** - 将结果 (数值) 压入栈
**
** 说明:
** - 等价于 Lua 的 # 操作符
** - 调用 __len 元方法
*/
LUA_API void(lua_len)(lua_State *L, int idx);

/*
** 数值转字符串缓冲区大小
** 用于 lua_numbertocstring 函数
*/
#define LUA_N2SBUFFSZ 64

/*
** 将数值转换为字符串
**
** 参数:
** - int idx: 栈索引
** - char *buff: 缓冲区 (至少 LUA_N2SBUFFSZ 字节)
**
** 返回值: 字符串长度
**
** 说明: 不压入栈,直接写入缓冲区
*/
LUA_API unsigned(lua_numbertocstring)(lua_State *L, int idx, char *buff);

/*
** 将字符串转换为数值
**
** 参数: const char *s - 字符串
**
** 返回值: 转换的字节数 (0 表示失败)
**
** 行为:
** - 尝试将字符串转换为数值
** - 成功则压入栈
*/
LUA_API size_t(lua_stringtonumber)(lua_State *L, const char *s);

/*
** 获取内存分配函数
**
** 参数: void **ud - 输出参数,用户数据指针
**
** 返回值: 当前的分配函数
*/
LUA_API lua_Alloc(lua_getallocf)(lua_State *L, void **ud);

/*
** 设置内存分配函数
**
** 参数:
** - lua_Alloc f: 新的分配函数
** - void *ud: 用户数据
*/
LUA_API void(lua_setallocf)(lua_State *L, lua_Alloc f, void *ud);

/*
** 标记为待关闭
**
** 参数: int idx - 栈索引
**
** 说明:
** - 标记该值在离开作用域时调用 __close 元方法
** - 类似于 Lua 的 <close> 变量
*/
LUA_API void(lua_toclose)(lua_State *L, int idx);

/*
** 关闭栈槽
**
** 参数: int idx - 栈索引
**
** 说明:
** - 立即调用从 idx 到栈顶的所有待关闭值的 __close 方法
** - 用于手动资源管理
*/
LUA_API void(lua_closeslot)(lua_State *L, int idx);

/*
** ============================================================================
** 实用宏定义
** ============================================================================
** 这些宏提供了常用操作的简写
*/
/*
** {==============================================================
** some useful macros
** ===============================================================
*/

/*
** 获取额外空间
**
** 说明:
** - lua_State 之前有 LUA_EXTRASPACE 字节的额外空间
** - 可以用于存储与状态机关联的 C 数据
** - 通过指针运算访问: (char *)(L) - LUA_EXTRASPACE
*/
#define lua_getextraspace(L) ((void *)((char *)(L) - LUA_EXTRASPACE))

/*
** 简化版 tonumber 和 tointeger
** 忽略 isnum 参数
*/
#define lua_tonumber(L, i) lua_tonumberx(L, (i), NULL)
#define lua_tointeger(L, i) lua_tointegerx(L, (i), NULL)

/*
** 弹出 n 个元素
**
** 实现: lua_settop(L, -(n)-1)
**
** 解释:
** - 假设栈有 5 个元素,要弹出 2 个
** - -(2)-1 = -3
** - lua_settop(L, -3) 将栈顶设置到倒数第 3 个位置
** - 相当于保留 3 个元素,弹出 2 个
*/
#define lua_pop(L, n) lua_settop(L, -(n) - 1)

/*
** 创建空表
** 等价于: lua_createtable(L, 0, 0)
*/
#define lua_newtable(L) lua_createtable(L, 0, 0)

/*
** 注册全局函数
**
** 参数:
** - n: 函数名
** - f: C 函数
**
** 实现:
** 1. lua_pushcfunction(L, f) - 压入函数
** 2. lua_setglobal(L, n) - 设置为全局变量
*/
#define lua_register(L, n, f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

/*
** 压入 C 函数 (无 upvalue)
** 等价于: lua_pushcclosure(L, f, 0)
*/
#define lua_pushcfunction(L, f) lua_pushcclosure(L, (f), 0)

/*
** 类型检查宏
** 通过比较 lua_type 的返回值实现
*/
#define lua_isfunction(L, n) (lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L, n) (lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L, n) (lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L, n) (lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L, n) (lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L, n) (lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L, n) (lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n) (lua_type(L, (n)) <= 0)

/*
** 压入字面字符串
**
** 使用 "" s 技巧确保参数是字面字符串
**
** C 语言知识点 - 字符串字面量拼接:
** C 编译器会自动连接相邻的字符串字面量
** "" s 会被编译为 s
** 如果 s 不是字面量,会产生编译错误
*/
#define lua_pushliteral(L, s) lua_pushstring(L, "" s)

/*
** 压入全局表
**
** 实现: lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS)
**
** 说明:
** - 从注册表获取全局表
** - LUA_RIDX_GLOBALS 是全局表在注册表中的索引
*/
#define lua_pushglobaltable(L) \
  ((void)lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS))

/*
** 简化版 tostring
** 忽略长度参数
*/
#define lua_tostring(L, i) lua_tolstring(L, (i), NULL)

/*
** 插入元素
**
** 将栈顶元素插入到 idx 位置
**
** 实现: 旋转 idx 到栈顶的元素,将最后一个移到 idx
*/
#define lua_insert(L, idx) lua_rotate(L, (idx), 1)

/*
** 移除元素
**
** 移除 idx 位置的元素
**
** 实现:
** 1. lua_rotate(L, idx, -1) - 将 idx 移到栈顶
** 2. lua_pop(L, 1) - 弹出栈顶
*/
#define lua_remove(L, idx) (lua_rotate(L, (idx), -1), lua_pop(L, 1))

/*
** 替换元素
**
** 用栈顶元素替换 idx 位置的元素
**
** 实现:
** 1. lua_copy(L, -1, idx) - 复制栈顶到 idx
** 2. lua_pop(L, 1) - 弹出栈顶
*/
#define lua_replace(L, idx) (lua_copy(L, -1, (idx)), lua_pop(L, 1))

/* }============================================================== */

/*
** ============================================================================
** 兼容性宏
** ============================================================================
** 这些宏提供向后兼容
*/
/*
** {==============================================================
** compatibility macros
** ===============================================================
*/

/*
** 旧版本的 newuserdata (只有一个用户值)
*/
#define lua_newuserdata(L, s) lua_newuserdatauv(L, s, 1)

/*
** 旧版本的用户值访问 (只访问第一个)
*/
#define lua_getuservalue(L, idx) lua_getiuservalue(L, idx, 1)
#define lua_setuservalue(L, idx) lua_setiuservalue(L, idx, 1)

/*
** 重置线程 (关闭所有待关闭的值)
*/
#define lua_resetthread(L) lua_closethread(L, NULL)

/* }============================================================== */

/*
** ============================================================================
** 调试 API
** ============================================================================
*/
/*
** {======================================================================
** Debug API
** =======================================================================
*/

/*
** 事件代码
** 定义了钩子函数可以捕获的事件类型
*/
/*
** Event codes
*/
#define LUA_HOOKCALL 0     /* 函数调用事件 */
#define LUA_HOOKRET 1      /* 函数返回事件 */
#define LUA_HOOKLINE 2     /* 执行新行事件 */
#define LUA_HOOKCOUNT 3    /* 指令计数事件 */
#define LUA_HOOKTAILCALL 4 /* 尾调用事件 */

/*
** 事件掩码
** 用于设置要捕获哪些事件
*/
/*
** Event masks
*/
#define LUA_MASKCALL (1 << LUA_HOOKCALL)   /* 捕获调用 */
#define LUA_MASKRET (1 << LUA_HOOKRET)     /* 捕获返回 */
#define LUA_MASKLINE (1 << LUA_HOOKLINE)   /* 捕获行 */
#define LUA_MASKCOUNT (1 << LUA_HOOKCOUNT) /* 捕获计数 */

/*
** 获取栈信息
**
** 参数:
** - int level: 栈层级 (0 是当前函数)
** - lua_Debug *ar: 输出参数,调试信息结构
**
** 返回值: 1 表示成功,0 表示 level 超出范围
*/
LUA_API int(lua_getstack)(lua_State *L, int level, lua_Debug *ar);

/*
** 获取详细信息
**
** 参数:
** - const char *what: 指定要获取的信息 ("n", "S", "l" 等)
** - lua_Debug *ar: 调试信息结构
**
** 返回值: 1 表示成功,0 表示失败
**
** what 参数:
** - 'n': 名称信息 (name, namewhat)
** - 'S': 源码信息 (source, linedefined 等)
** - 'l': 当前行号 (currentline)
** - 'u': upvalue 信息 (nups, nparams, isvararg)
** - 't': 是否尾调用 (istailcall)
*/
LUA_API int(lua_getinfo)(lua_State *L, const char *what, lua_Debug *ar);

/*
** 获取局部变量
**
** 参数:
** - const lua_Debug *ar: 调试信息
** - int n: 变量索引 (从 1 开始)
**
** 返回值: 变量名,NULL 表示没有更多变量
**
** 行为: 将变量值压入栈
*/
LUA_API const char *(lua_getlocal)(lua_State * L, const lua_Debug *ar, int n);

/*
** 设置局部变量
**
** 参数同 lua_getlocal
**
** 行为: 从栈顶弹出值并赋给变量
*/
LUA_API const char *(lua_setlocal)(lua_State * L, const lua_Debug *ar, int n);

/*
** 获取 upvalue
**
** 参数:
** - int funcindex: 函数的栈索引
** - int n: upvalue 索引 (从 1 开始)
**
** 返回值: upvalue 名称,NULL 表示没有
**
** 行为: 将 upvalue 值压入栈
*/
LUA_API const char *(lua_getupvalue)(lua_State * L, int funcindex, int n);

/*
** 设置 upvalue
**
** 参数同 lua_getupvalue
**
** 行为: 从栈顶弹出值并赋给 upvalue
*/
LUA_API const char *(lua_setupvalue)(lua_State * L, int funcindex, int n);

/*
** 获取 upvalue 的唯一标识
**
** 返回值: 指针,相同的 upvalue 返回相同的指针
**
** 用途: 检查两个闭包是否共享 upvalue
*/
LUA_API void *(lua_upvalueid)(lua_State * L, int fidx, int n);

/*
** 共享 upvalue
**
** 参数:
** - int fidx1, int n1: 第一个函数和 upvalue 索引
** - int fidx2, int n2: 第二个函数和 upvalue 索引
**
** 作用: 让两个闭包共享同一个 upvalue
*/
LUA_API void(lua_upvaluejoin)(lua_State *L, int fidx1, int n1,
                              int fidx2, int n2);

/*
** 设置钩子函数
**
** 参数:
** - lua_Hook func: 钩子函数
** - int mask: 事件掩码 (LUA_MASKCALL 等的组合)
** - int count: 指令计数间隔 (0 表示不计数)
**
** 示例:
** lua_sethook(L, my_hook, LUA_MASKCALL | LUA_MASKRET, 0);
** // 在每次函数调用和返回时调用 my_hook
*/
LUA_API void(lua_sethook)(lua_State *L, lua_Hook func, int mask, int count);

/*
** 获取当前钩子函数
*/
LUA_API lua_Hook(lua_gethook)(lua_State *L);

/*
** 获取钩子掩码
*/
LUA_API int(lua_gethookmask)(lua_State *L);

/*
** 获取钩子计数
*/
LUA_API int(lua_gethookcount)(lua_State *L);

/*
** lua_Debug 结构体定义
**
** 这个结构体包含了函数调用的详细调试信息
*/
struct lua_Debug
{
  int event; /* 触发的事件类型 (LUA_HOOKCALL 等) */

  const char *name;           /* (n) 函数名 */
  const char *namewhat;       /* (n) 名称类型: 'global', 'local', 'field', 'method' */
  const char *what;           /* (S) 函数类型: 'Lua' (Lua 函数), 'C' (C 函数), 'main' (主程序), 'tail' (尾调用) */
  const char *source;         /* (S) 源文件名或代码 */
  size_t srclen;              /* (S) source 的长度 */
  int currentline;            /* (l) 当前行号 */
  int linedefined;            /* (S) 函数定义的起始行 */
  int lastlinedefined;        /* (S) 函数定义的结束行 */
  unsigned char nups;         /* (u) upvalue 的个数 */
  unsigned char nparams;      /* (u) 参数个数 */
  char isvararg;              /* (u) 是否是可变参数函数 */
  unsigned char extraargs;    /* (t) 额外参数的个数 */
  char istailcall;            /* (t) 是否是尾调用 */
  int ftransfer;              /* (r) 第一个传输值的索引 */
  int ntransfer;              /* (r) 传输值的个数 */
  char short_src[LUA_IDSIZE]; /* (S) 简短的源文件名 (用于显示) */

  /* 私有部分 */
  /* private part */
  struct CallInfo *i_ci; /* 活动函数的调用信息 (内部使用) */
};

/* }====================================================================== */

/*
** 版本字符串宏
**
** C 语言知识点 - 字符串化:
** #x 将宏参数转换为字符串
** LUAI_TOSTR 是一个辅助宏,确保宏展开后再字符串化
*/
#define LUAI_TOSTRAUX(x) #x            /* 辅助宏: 将 x 转为字符串 */
#define LUAI_TOSTR(x) LUAI_TOSTRAUX(x) /* 确保 x 先展开再字符串化 */

/*
** 版本号的字符串形式
*/
#define LUA_VERSION_MAJOR LUAI_TOSTR(LUA_VERSION_MAJOR_N)     /* "5" */
#define LUA_VERSION_MINOR LUAI_TOSTR(LUA_VERSION_MINOR_N)     /* "5" */
#define LUA_VERSION_RELEASE LUAI_TOSTR(LUA_VERSION_RELEASE_N) /* "0" */

/*
** 完整版本字符串
**
** LUA_VERSION: "Lua 5.5"
** LUA_RELEASE: "Lua 5.5.0"
*/
#define LUA_VERSION "Lua " LUA_VERSION_MAJOR "." LUA_VERSION_MINOR
#define LUA_RELEASE LUA_VERSION "." LUA_VERSION_RELEASE

/*
** ============================================================================
** 版权声明
** ============================================================================
**
** Lua 使用 MIT 许可证,这是一个非常宽松的开源许可证:
**
** 主要权利:
** - 可以自由使用、复制、修改、合并、发布、分发、再许可和销售
** - 可以用于商业项目
** - 可以修改源代码
**
** 主要义务:
** - 必须在所有副本中包含版权声明和许可声明
**
** 免责声明:
** - 软件按"原样"提供,不提供任何形式的保证
** - 作者不对任何索赔、损害或其他责任负责
*/
/******************************************************************************
 * Copyright (C) 1994-2025 Lua.org, PUC-Rio.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************/

#endif /* 结束头文件保护 */

/*
** ============================================================================
** 文件总结
** ============================================================================
**
** 这个头文件是 Lua C API 的核心,定义了 C 程序与 Lua 交互的所有接口。
**
**
** 主要内容回顾:
**
** 1. 基本类型系统 (40-100 行)
**    - 定义了 Lua 的 9 种基本类型
**    - 提供了类型常量和检查宏
**    - 支持数值、字符串、表、函数等
**
** 2. 虚拟栈机制 (200-300 行)
**    - Lua 与 C 交互的核心机制
**    - 提供压栈、弹栈、索引等操作
**    - 支持正负索引和伪索引
**
** 3. 状态机管理 (120-150 行)
**    - lua_State 的创建和销毁
**    - 线程(协程)管理
**    - panic 函数设置
**
** 4. 数据访问函数 (300-400 行)
**    - 类型检查: lua_is* 系列
**    - 类型转换: lua_to* 系列
**    - 表操作: get/set 系列
**
** 5. 函数调用机制 (150-200 行)
**    - lua_call: 普通调用
**    - lua_pcall: 保护模式调用
**    - 支持可变参数和多返回值
**
** 6. 协程支持 (100-150 行)
**    - lua_yield: 让出执行
**    - lua_resume: 恢复执行
**    - 状态查询
**
** 7. 垃圾回收控制 (80-120 行)
**    - 两种 GC 模式: 增量和分代
**    - 详细的参数调优
**    - 手动触发和控制
**
** 8. 调试接口 (150-250 行)
**    - 钩子函数机制
**    - 栈帧遍历
**    - 变量和 upvalue 访问
**    - lua_Debug 结构
**
**
** 设计特点:
**
** 1. 基于栈的 API
**    - 所有数据交换通过栈进行
**    - 避免了复杂的内存管理
**    - 类型安全由 Lua 保证
**
** 2. 一致的命名规范
**    - lua_*: 核心 API
**    - LUA_*: 常量
**    - lua_is*: 类型检查
**    - lua_to*: 类型转换
**    - lua_push*: 压栈
**    - lua_get*|lua_set*: 访问/设置
**
** 3. 错误处理
**    - 保护模式调用捕获错误
**    - panic 函数处理致命错误
**    - 错误传播机制
**
** 4. 内存管理
**    - 自定义分配器支持
**    - 垃圾回收自动管理
**    - 资源生命周期明确
**
** 5. 扩展性
**    - 支持用户数据
**    - 元表机制
**    - 钩子和回调
**
**
** C 语言高级特性使用:
**
** 1. 函数指针
**    - lua_CFunction: C 函数注册
**    - lua_Alloc: 自定义内存分配
**    - lua_Hook: 调试钩子
**
** 2. 可变参数
**    - lua_pushfstring: 格式化输出
**    - lua_gc: 多态操作
**
** 3. 宏技巧
**    - 头文件保护
**    - 字符串化 (#)
**    - 类型检查宏
**    - 便利宏封装
**
** 4. 结构体
**    - lua_Debug: 调试信息
**    - lua_State: 前向声明
**
** 5. typedef
**    - 类型别名简化
**    - 跨平台兼容
**
**
** 使用建议:
**
** 1. 初学者
**    - 从简单的压栈、弹栈开始
**    - 理解栈索引机制
**    - 练习基本的函数调用
**
** 2. 进阶使用
**    - 掌握错误处理
**    - 理解元表和元方法
**    - 使用用户数据绑定 C 对象
**
** 3. 高级应用
**    - 自定义内存分配器
**    - 实现调试器
**    - 性能优化和 GC 调优
**
**
** 常见陷阱:
**
** 1. 栈溢出
**    - 忘记检查栈空间
**    - 使用 lua_checkstack
**
** 2. 索引错误
**    - 混淆正负索引
**    - 使用无效索引
**
** 3. 内存泄漏
**    - 忘记弹出值
**    - 循环引用
**
** 4. 生命周期
**    - 使用已释放的 lua_State
**    - 持有 Lua 字符串指针过久
**
**
** 学习资源:
**
** 1. 官方文档: www.lua.org/manual/5.5/
** 2. Programming in Lua (PIL) 书籍
** 3. Lua 源代码: 最好的学习材料
**
**
** 下一步学习:
**
** 1. lauxlib.h: 辅助库,提供更高级的封装
** 2. lualib.h: 标准库的 API
** 3. 实际项目: 尝试嵌入 Lua 或编写 Lua 扩展
**
** ============================================================================
*/