/*
** ============================================================================
** 文件概要说明
** ============================================================================
**
** 文件名: lualib.h
** 项目: Lua 标准库头文件
**
** 【文件功能】
** 这是 Lua 解释器的标准库声明头文件。它定义了 Lua 所有标准库的:
** 1. 库名称常量 (如 "math", "string" 等)
** 2. 库标识位掩码 (用于选择性加载库)
** 3. 库初始化函数声明 (如 luaopen_math, luaopen_string 等)
**
** 【标准库列表】
** Lua 提供以下标准库:
** - base: 基础库 (print, type, tonumber 等基本函数)
** - package: 模块加载库 (require, module 等)
** - coroutine: 协程库 (协程创建、调度)
** - debug: 调试库 (调试钩子、堆栈检查等)
** - io: 输入输出库 (文件操作)
** - math: 数学库 (数学函数)
** - os: 操作系统库 (日期时间、环境变量等)
** - string: 字符串库 (字符串操作)
** - table: 表操作库 (表排序、插入等)
** - utf8: UTF-8 编码支持库
**
** 【设计模式】
** 每个库都采用统一的设计模式:
** - XXX_LIBNAME: 定义库在 Lua 中的名称字符串
** - XXX_LIBK: 定义库的位标识(用于位掩码选择)
** - luaopen_xxx: 库的初始化函数
**
** 【使用场景】
** 当创建新的 Lua 状态机时,需要调用这些函数来加载标准库,
** 使 Lua 脚本可以使用这些库提供的功能。
** ============================================================================
*/

/*
** $Id: lualib.h $
** Lua standard libraries
** Lua 标准库
** See Copyright Notice in lua.h
** 版权声明见 lua.h 文件
*/

#ifndef lualib_h
#define lualib_h

#include "lua.h"

/*
** ============================================================================
** 版本后缀定义
** ============================================================================
*/

/* version suffix for environment variable names */
/* 用于环境变量名称的版本后缀 */
/*
** 【说明】这个宏用于生成带版本号的环境变量名
** 例如: 如果 Lua 版本是 5.4, 则生成 "_5_4"
** 用途: 允许不同版本的 Lua 使用不同的环境变量,避免版本冲突
**
** 【字符串拼接】使用了 C 语言的字符串字面量自动拼接特性
** 相邻的字符串字面量会自动拼接成一个字符串
** 例如: "a" "b" "c" 等价于 "abc"
*/
#define LUA_VERSUFFIX "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR

/*
** ============================================================================
** 基础库 (Base Library)
** ============================================================================
*/

/*
** 【库标识位】LUA_GLIBK = 1 (二进制: 0001)
** 基础库的位标识,是最基础的库,位值为 1
** 所有其他库的位标识都基于这个值进行左移操作
*/
#define LUA_GLIBK 1

/*
** 【函数声明】基础库初始化函数
** LUAMOD_API: 模块 API 修饰符宏,用于导出函数符号
**   - 在 Windows 上可能定义为 __declspec(dllexport) 或 __declspec(dllimport)
**   - 在 Linux 上通常为空或 __attribute__((visibility("default")))
**
** 参数: lua_State *L - Lua 虚拟机状态指针
** 返回: int - 通常返回 1,表示在栈上压入了一个值(基础库的全局表)
**
** 【功能】注册基础库中的所有函数,包括:
**   print, type, tonumber, tostring, error, pcall, pairs, ipairs 等
*/
LUAMOD_API int(luaopen_base)(lua_State *L);

/*
** ============================================================================
** 包管理库 (Package Library)
** ============================================================================
*/

/*
** 【库名称】在 Lua 中访问时使用的名称
** 例如: require "package" 或 package.path
*/
#define LUA_LOADLIBNAME "package"

/*
** 【库标识位】LUA_LOADLIBK = LUA_GLIBK << 1 = 2 (二进制: 0010)
** 使用位左移操作,确保每个库有唯一的位标识
** 这种设计允许使用位或(|)操作组合多个库
*/
#define LUA_LOADLIBK (LUA_GLIBK << 1)

/*
** 【函数声明】包管理库初始化函数
**
** 【功能】注册包管理相关函数,包括:
**   require: 加载模块
**   package.path: 模块搜索路径
**   package.cpath: C 模块搜索路径
**   package.loaded: 已加载模块缓存表
**   package.preload: 预加载函数表
**   package.searchers: 模块搜索器列表
*/
LUAMOD_API int(luaopen_package)(lua_State *L);

/*
** ============================================================================
** 协程库 (Coroutine Library)
** ============================================================================
*/

/*
** 【库名称】协程库的 Lua 访问名称
** 在 Lua 中使用: coroutine.create, coroutine.resume 等
*/
#define LUA_COLIBNAME "coroutine"

/*
** 【库标识位】LUA_COLIBK = LUA_LOADLIBK << 1 = 4 (二进制: 0100)
*/
#define LUA_COLIBK (LUA_LOADLIBK << 1)

/*
** 【函数声明】协程库初始化函数
**
** 【功能】注册协程相关函数,包括:
**   coroutine.create: 创建新协程
**   coroutine.resume: 恢复协程执行
**   coroutine.yield: 挂起当前协程
**   coroutine.status: 获取协程状态
**   coroutine.running: 获取当前运行的协程
**   coroutine.wrap: 创建包装的协程
**   coroutine.close: 关闭协程
**   coroutine.isyieldable: 检查是否可以 yield
*/
LUAMOD_API int(luaopen_coroutine)(lua_State *L);

/*
** ============================================================================
** 调试库 (Debug Library)
** ============================================================================
*/

/*
** 【库名称】调试库的 Lua 访问名称
*/
#define LUA_DBLIBNAME "debug"

/*
** 【库标识位】LUA_DBLIBK = LUA_COLIBK << 1 = 8 (二进制: 1000)
*/
#define LUA_DBLIBK (LUA_COLIBK << 1)

/*
** 【函数声明】调试库初始化函数
**
** 【功能】注册调试相关函数,包括:
**   debug.debug: 进入交互式调试模式
**   debug.getinfo: 获取函数信息
**   debug.gethook: 获取调试钩子
**   debug.getlocal: 获取局部变量
**   debug.getupvalue: 获取上值(闭包变量)
**   debug.sethook: 设置调试钩子
**   debug.setlocal: 设置局部变量
**   debug.setupvalue: 设置上值
**   debug.traceback: 生成调用栈跟踪
**   debug.getmetatable: 获取元表
**   debug.setmetatable: 设置元表
**   debug.upvalueid: 获取上值标识
**   debug.upvaluejoin: 连接上值
**   debug.getuservalue: 获取用户值
**   debug.setuservalue: 设置用户值
**
** 【安全提示】调试库功能强大,可以破坏沙箱,生产环境应谨慎使用
*/
LUAMOD_API int(luaopen_debug)(lua_State *L);

/*
** ============================================================================
** 输入输出库 (I/O Library)
** ============================================================================
*/

/*
** 【库名称】I/O 库的 Lua 访问名称
*/
#define LUA_IOLIBNAME "io"

/*
** 【库标识位】LUA_IOLIBK = LUA_DBLIBK << 1 = 16 (二进制: 10000)
*/
#define LUA_IOLIBK (LUA_DBLIBK << 1)

/*
** 【函数声明】I/O 库初始化函数
**
** 【功能】注册文件操作相关函数,包括:
**   io.open: 打开文件
**   io.close: 关闭文件
**   io.read: 从标准输入读取
**   io.write: 写入标准输出
**   io.input: 设置/获取默认输入文件
**   io.output: 设置/获取默认输出文件
**   io.flush: 刷新输出缓冲
**   io.lines: 迭代文件行
**   io.popen: 执行命令并获取管道 (平台相关)
**   io.tmpfile: 创建临时文件
**   io.type: 检查文件对象类型
**
** 【文件对象方法】
**   file:read, file:write, file:lines, file:seek,
**   file:setvbuf, file:flush, file:close
*/
LUAMOD_API int(luaopen_io)(lua_State *L);

/*
** ============================================================================
** 数学库 (Math Library)
** ============================================================================
*/

/*
** 【库名称】数学库的 Lua 访问名称
*/
#define LUA_MATHLIBNAME "math"

/*
** 【库标识位】LUA_MATHLIBK = LUA_IOLIBK << 1 = 32 (二进制: 100000)
*/
#define LUA_MATHLIBK (LUA_IOLIBK << 1)

/*
** 【函数声明】数学库初始化函数
**
** 【功能】注册数学计算相关函数,包括:
**   【三角函数】sin, cos, tan, asin, acos, atan
**   【双曲函数】sinh, cosh, tanh
**   【指数对数】exp, log, log10
**   【幂运算】pow (已弃用,使用 ^ 运算符)
**   【取整函数】floor, ceil, modf
**   【其他函数】abs, sqrt, max, min, random, randomseed
**   【常量】pi, huge, maxinteger, mininteger
**   【类型转换】tointeger, type (检查整数/浮点数)
**   【位运算】ult (无符号小于比较)
**
** 【底层实现】大多数函数直接调用 C 标准库 <math.h> 中的函数
*/
LUAMOD_API int(luaopen_math)(lua_State *L);

/*
** ============================================================================
** 操作系统库 (OS Library)
** ============================================================================
*/

/*
** 【库名称】操作系统库的 Lua 访问名称
*/
#define LUA_OSLIBNAME "os"

/*
** 【库标识位】LUA_OSLIBK = LUA_MATHLIBK << 1 = 64 (二进制: 1000000)
*/
#define LUA_OSLIBK (LUA_MATHLIBK << 1)

/*
** 【函数声明】操作系统库初始化函数
**
** 【功能】注册操作系统交互相关函数,包括:
**   【时间日期】
**     os.time: 获取时间戳
**     os.date: 格式化日期时间
**     os.clock: 获取 CPU 时间
**     os.difftime: 计算时间差
**   【环境变量】
**     os.getenv: 获取环境变量
**     os.setlocale: 设置本地化
**   【文件系统】
**     os.remove: 删除文件
**     os.rename: 重命名文件
**     os.tmpname: 生成临时文件名
**   【进程控制】
**     os.exit: 退出程序
**     os.execute: 执行系统命令
**
** 【平台差异】某些函数在不同操作系统上行为可能不同
*/
LUAMOD_API int(luaopen_os)(lua_State *L);

/*
** ============================================================================
** 字符串库 (String Library)
** ============================================================================
*/

/*
** 【库名称】字符串库的 Lua 访问名称
*/
#define LUA_STRLIBNAME "string"

/*
** 【库标识位】LUA_STRLIBK = LUA_OSLIBK << 1 = 128 (二进制: 10000000)
*/
#define LUA_STRLIBK (LUA_OSLIBK << 1)

/*
** 【函数声明】字符串库初始化函数
**
** 【功能】注册字符串处理相关函数,包括:
**   【基本操作】
**     string.len: 获取字符串长度
**     string.sub: 提取子串
**     string.rep: 重复字符串
**     string.reverse: 反转字符串
**   【大小写转换】
**     string.upper: 转大写
**     string.lower: 转小写
**   【字符操作】
**     string.byte: 字符转字节码
**     string.char: 字节码转字符
**   【格式化】
**     string.format: 格式化字符串 (类似 C 的 sprintf)
**   【模式匹配】
**     string.find: 查找模式
**     string.match: 匹配模式
**     string.gmatch: 全局匹配迭代器
**     string.gsub: 全局替换
**   【打包解包】
**     string.pack: 打包二进制数据
**     string.unpack: 解包二进制数据
**     string.packsize: 计算打包大小
**   【其他】
**     string.dump: 转储函数为二进制
**
** 【元表】字符串库设置了字符串类型的元表,
**         使得可以使用 str:method() 语法
*/
LUAMOD_API int(luaopen_string)(lua_State *L);

/*
** ============================================================================
** 表操作库 (Table Library)
** ============================================================================
*/

/*
** 【库名称】表操作库的 Lua 访问名称
*/
#define LUA_TABLIBNAME "table"

/*
** 【库标识位】LUA_TABLIBK = LUA_STRLIBK << 1 = 256 (二进制: 100000000)
*/
#define LUA_TABLIBK (LUA_STRLIBK << 1)

/*
** 【函数声明】表操作库初始化函数
**
** 【功能】注册表操作相关函数,包括:
**   table.concat: 连接数组元素为字符串
**   table.insert: 插入元素到数组
**   table.remove: 从数组移除元素
**   table.move: 移动数组元素
**   table.sort: 排序数组 (使用快速排序算法)
**   table.pack: 打包可变参数为表
**   table.unpack: 解包表为多个返回值
**
** 【数组约定】这些函数主要针对"数组部分"操作,
**             即键为连续整数 1, 2, 3, ... 的表元素
*/
LUAMOD_API int(luaopen_table)(lua_State *L);

/*
** ============================================================================
** UTF-8 编码支持库 (UTF-8 Library)
** ============================================================================
*/

/*
** 【库名称】UTF-8 库的 Lua 访问名称
*/
#define LUA_UTF8LIBNAME "utf8"

/*
** 【库标识位】LUA_UTF8LIBK = LUA_TABLIBK << 1 = 512 (二进制: 1000000000)
*/
#define LUA_UTF8LIBK (LUA_TABLIBK << 1)

/*
** 【函数声明】UTF-8 库初始化函数
**
** 【功能】注册 UTF-8 编码处理相关函数,包括:
**   utf8.char: UTF-8 码点转字符
**   utf8.codes: 迭代 UTF-8 字符
**   utf8.codepoint: 获取 UTF-8 码点
**   utf8.len: 计算 UTF-8 字符串长度
**   utf8.offset: 计算字节偏移量
**   utf8.charpattern: UTF-8 字符匹配模式
**
** 【重要性】Lua 5.3+ 加入此库,提供对 Unicode 的基本支持
*/
LUAMOD_API int(luaopen_utf8)(lua_State *L);

/*
** ============================================================================
** 库加载控制函数
** ============================================================================
*/

/* open selected libraries */
/* 打开选定的库 */
/*
** 【函数声明】选择性加载库
**
** 【参数说明】
**   lua_State *L: Lua 虚拟机状态指针
**   int load: 要加载的库的位掩码
**     - 使用位或(|)组合多个库标识
**     - 例如: LUA_MATHLIBK | LUA_STRLIBK 只加载数学和字符串库
**   int preload: 要预加载的库的位掩码
**     - 预加载是指将库注册到 package.preload 表中,
**       但不立即加载,等到 require 时才真正加载
**
** 【位掩码示例】
**   加载所有库: ~0 (所有位都为 1)
**   加载数学库: LUA_MATHLIBK
**   加载数学和字符串库: LUA_MATHLIBK | LUA_STRLIBK
**   不加载任何库: 0
**
** 【设计优势】这种设计允许:
**   1. 减少内存占用(不加载不需要的库)
**   2. 提高安全性(禁用危险的库如 io, os)
**   3. 延迟加载(使用 preload 机制)
*/
LUALIB_API void(luaL_openselectedlibs)(lua_State *L, int load, int preload);

/* open all libraries */
/* 打开所有库 */
/*
** 【宏定义】加载所有标准库的便捷宏
**
** 【实现说明】
**   ~0: 按位取反操作符,产生所有位都为 1 的值
**     - 这样可以匹配所有库的位标识
**     - 等价于十进制的 -1 或最大无符号整数值
**   0: preload 参数为 0,表示不使用预加载,直接加载所有库
**
** 【使用示例】
**   lua_State *L = luaL_newstate();  // 创建新的 Lua 状态机
**   luaL_openlibs(L);                // 加载所有标准库
**
** 【等价调用】
**   luaL_openlibs(L) 等价于:
**   luaL_openselectedlibs(L, ~0, 0)
**
**   也等价于手动调用所有初始化函数:
**   luaopen_base(L);
**   luaopen_package(L);
**   luaopen_coroutine(L);
**   // ... 其余库
**
** 【常见用法】这是最常用的初始化方式,
**             适合大多数应用场景
*/
#define luaL_openlibs(L) luaL_openselectedlibs(L, ~0, 0)

#endif

/*
** ============================================================================
** 文件总结说明
** ============================================================================
**
** 【整体架构】
** 本文件是 Lua 标准库的接口定义,采用了清晰的模块化设计:
**
** 1. 【命名规范】
**    - 库名称宏: LUA_XXXLIBNAME (字符串形式)
**    - 库标识宏: LUA_XXXLIBK (位掩码形式)
**    - 初始化函数: luaopen_xxx (函数指针形式)
**
** 2. 【位掩码技术】
**    使用位左移操作符 (<<) 为每个库分配唯一的位:
**    - LUA_GLIBK     = 1    = 0b0000000001
**    - LUA_LOADLIBK  = 2    = 0b0000000010
**    - LUA_COLIBK    = 4    = 0b0000000100
**    - LUA_DBLIBK    = 8    = 0b0000001000
**    - LUA_IOLIBK    = 16   = 0b0000010000
**    - LUA_MATHLIBK  = 32   = 0b0000100000
**    - LUA_OSLIBK    = 64   = 0b0001000000
**    - LUA_STRLIBK   = 128  = 0b0010000000
**    - LUA_TABLIBK   = 256  = 0b0100000000
**    - LUA_UTF8LIBK  = 512  = 0b1000000000
**
**    这种设计允许:
**    - 使用位或 (|) 组合多个库
**    - 使用位与 (&) 检查是否包含某个库
**    - 使用 ~0 表示所有库
**
** 3. 【API 修饰符】
**    - LUAMOD_API: 用于库初始化函数,标记为模块导出
**    - LUALIB_API: 用于库管理函数,标记为库 API 导出
**    这些宏处理跨平台的符号可见性和动态链接问题
**
** 4. 【初始化函数】
**    所有 luaopen_xxx 函数都遵循相同的接口:
**    - 参数: lua_State *L (虚拟机状态)
**    - 返回: int (栈上返回值的数量,通常为 1)
**    - 作用: 创建库表,注册库函数,设置元表等
**
** 【C 语言高级特性说明】
**
** 1. 【条件编译】
**    #ifndef, #define, #endif: 头文件保护
**    - 防止头文件被多次包含
**    - lualib_h 作为保护宏
**
** 2. 【宏定义】
**    #define: 定义编译时常量和宏函数
**    - 字符串拼接: 相邻字符串字面量自动拼接
**    - 宏展开: 预处理阶段进行文本替换
**
** 3. 【函数声明】
**    使用圆括号包裹函数名: int (luaopen_base) (lua_State *L)
**    - 这是为了兼容某些旧编译器
**    - 现代 C 也允许写成: int luaopen_base(lua_State *L)
**
** 4. 【位运算】
**    << (左移): a << b 等价于 a * 2^b
**    ~ (按位取反): ~0 产生所有位为 1 的值
**    | (按位或): 用于组合多个位标识
**    & (按位与): 用于检查是否包含某个标识
**
** 【使用示例】
**
** // 示例 1: 加载所有库
** lua_State *L = luaL_newstate();
** luaL_openlibs(L);
**
** // 示例 2: 只加载基础库和数学库
** lua_State *L = luaL_newstate();
** luaL_openselectedlibs(L, LUA_GLIBK | LUA_MATHLIBK, 0);
**
** // 示例 3: 只加载基础库,其他库预加载
** lua_State *L = luaL_newstate();
** luaL_openselectedlibs(L, LUA_GLIBK,
**     LUA_MATHLIBK | LUA_STRLIBK | LUA_TABLIBK);
**
** // 示例 4: 手动加载单个库
** lua_State *L = luaL_newstate();
** luaopen_base(L);      // 加载基础库
** lua_setglobal(L, "_G"); // 设置全局环境
** luaopen_math(L);      // 加载数学库
** lua_setglobal(L, "math"); // 注册为全局表
**
** 【设计哲学】
**
** 1. 【模块化】每个库独立,可以选择性加载
** 2. 【最小化】不强制加载所有库,节省资源
** 3. 【安全性】可以禁用危险的库(io, os, debug)用于沙箱环境
** 4. 【扩展性】可以方便地添加新的标准库
** 5. 【一致性】所有库遵循统一的命名和初始化模式
**
** 【注意事项】
**
** 1. lua_State 是 Lua 虚拟机的核心数据结构,包含了执行状态
** 2. 库初始化函数通常会在栈上留下一个表(库的命名空间)
** 3. luaL_openlibs 是最常用的初始化方式,适合大多数场景
** 4. 在沙箱环境中,应该使用 luaL_openselectedlibs 精确控制加载的库
** 5. 调试库功能强大但可能破坏安全性,生产环境需谨慎
**
** 【与其他文件的关系】
**
** - lua.h: 定义了 lua_State 和基本 API
** - luaconf.h: 定义了 LUAMOD_API, LUALIB_API 等配置宏
** - linit.c: 实现了 luaL_openselectedlibs 函数
** - lbaselib.c, lmathlib.c 等: 实现了各个库的 luaopen_xxx 函数
**
** ============================================================================
*/
