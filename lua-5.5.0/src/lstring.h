/*
** $Id: lstring.h $
** String table (keep all strings handled by Lua)
** 字符串表（保存所有由Lua处理的字符串）
** See Copyright Notice in lua.h
** 版权声明请参见 lua.h
*/

/*
** ============================================================================
** 文件概要 (File Overview)
** ============================================================================
**
** 文件名: lstring.h
** 所属项目: Lua 源码
** 文件类型: C 头文件 (Header File)
**
** 【功能描述】
** 本文件是 Lua 字符串子系统的头文件，定义了字符串表（String Table）相关的
** 宏定义和函数声明。字符串表是 Lua 内部用于管理所有字符串的核心数据结构。
**
** 【核心概念】
** 1. 短字符串 (Short String): 长度不超过 LUAI_MAXSHORTLEN（默认40）的字符串
**    - 会被"内化"(internalized)，即相同内容的短字符串在内存中只存一份
**    - 存储在全局字符串表(string table)中，使用哈希表管理
**    - 比较时只需比较指针，效率极高
**
** 2. 长字符串 (Long String): 长度超过 LUAI_MAXSHORTLEN 的字符串
**    - 不会被内化，每次创建都会分配新内存
**    - 哈希值采用延迟计算策略，只在需要时才计算
**
** 3. 字符串内化 (String Interning):
**    一种优化技术，确保相同内容的字符串在内存中只存在一份副本
**    这样做的好处是：节省内存、字符串比较变成指针比较（O(1)复杂度）
**
** 【依赖关系】
** - lgc.h: 垃圾回收相关定义
** - lobject.h: Lua 对象类型定义（包括 TString 结构）
** - lstate.h: Lua 状态机定义（包括 lua_State 和 global_State）
**
** 【本文件结构】
** 1. 头文件保护宏
** 2. 依赖头文件包含
** 3. 常量和宏定义
** 4. 函数声明
** 5. 头文件保护结束
**
** ============================================================================
*/

#ifndef lstring_h
#define lstring_h
/*
** 【C语言知识点: 头文件保护 (Include Guard)】
** #ifndef / #define / #endif 组合称为"头文件保护"或"包含卫士"
** 作用: 防止同一个头文件被多次包含导致的重复定义错误
** 工作原理:
**   - 第一次包含时, lstring_h 未定义, 条件为真, 执行 #define lstring_h
**   - 后续再次包含时, lstring_h 已定义, 条件为假, 跳过整个文件内容
*/

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
/*
** 【依赖头文件说明】
**
** lgc.h (Lua Garbage Collection):
**   - 提供垃圾回收相关的宏和函数声明
**   - 字符串作为可回收对象，需要与GC系统交互
**
** lobject.h (Lua Object):
**   - 定义了 TString 结构体（Lua字符串的内部表示）
**   - 定义了 Udata 结构体（用户数据类型）
**   - 定义了各种类型标签（如 LUA_VSHRSTR）
**
** lstate.h (Lua State):
**   - 定义了 lua_State（Lua线程/协程状态）
**   - 定义了 global_State（全局状态，包含字符串表）
**   - 定义了 lua_Alloc（内存分配器函数类型）
*/

/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
** 内存分配错误消息必须预先分配（因为在内存耗尽后无法再创建它）
*/
#define MEMERRMSG "not enough memory"
/*
** 【宏定义说明: MEMERRMSG】
**
** 用途: 定义内存不足时的错误提示信息
**
** 设计原因:
**   当 Lua 发生内存不足错误时，需要向用户报告这个错误
**   但此时已经没有内存可用来创建新的错误消息字符串了！
**   因此，这个错误消息必须在 Lua 初始化时就预先创建好
**   存放在字符串表中，以备后用
**
** 这是一个典型的"鸡生蛋"问题的解决方案:
**   需要报告"没有内存"的错误，但报告错误本身需要内存
**   解决方法就是预先分配好这个错误消息
*/

/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
**
** 短字符串的最大长度，即被内化的字符串的最大长度。
** （不能小于保留字或元方法标签的长度，因为这些字符串必须被内化；
** #("function") = 8, #("__newindex") = 10。）
*/
#if !defined(LUAI_MAXSHORTLEN)
#define LUAI_MAXSHORTLEN 40
#endif
/*
** 【宏定义说明: LUAI_MAXSHORTLEN】
**
** 用途: 定义短字符串的最大长度阈值
** 默认值: 40 个字符
**
** 【C语言知识点: 条件编译】
** #if !defined(X) 检查宏 X 是否未被定义
** 这种写法允许用户在编译时通过 -D 选项自定义该值
** 例如: gcc -DLUAI_MAXSHORTLEN=50 ...
**
** 【为什么是40?】
** - 必须 >= 10，因为最长的元方法名 "__newindex" 有10个字符
** - 必须 >= 8，因为最长的保留字 "function" 有8个字符
** - 40 是一个经验值，能覆盖大多数常用的短字符串
** - 太大会导致哈希表过大，太小会失去内化的优势
**
** 【#() 语法说明】
** 注释中的 #("function") 是 Lua 语法，表示求字符串长度
** 在 C 中等价于 strlen("function")
**
** 【短字符串 vs 长字符串的权衡】
** 短字符串: 内化存储，相同内容只存一份，比较快（指针比较）
** 长字符串: 独立存储，每个都是独立副本，比较慢（逐字符比较）
** 阈值选择需要平衡内存使用和性能
*/

/*
** Size of a short TString: Size of the header plus space for the string
** itself (including final '\0').
**
** 短 TString 的大小：头部大小加上字符串本身所需空间
** （包括末尾的 '\0' 终止符）。
*/
#define sizestrshr(l) \
    (offsetof(TString, contents) + ((l) + 1) * sizeof(char))
/*
** 【宏定义说明: sizestrshr(l)】
**
** 用途: 计算存储长度为 l 的短字符串所需的总内存大小
** 参数: l - 字符串的长度（不包括 '\0'）
**
** 【C语言知识点: offsetof 宏】
** offsetof(type, member) 是标准库宏（定义在 <stddef.h>）
** 返回结构体 type 中成员 member 的字节偏移量
**
** 例如，如果 TString 结构体是:
**   struct TString {
**     CommonHeader;  // 假设占 8 字节
**     lu_byte extra; // 1 字节
**     ...
**     char contents[]; // 柔性数组成员
**   };
** 那么 offsetof(TString, contents) 返回 contents 之前所有成员的总大小
**
** 【C语言知识点: 柔性数组成员 (Flexible Array Member)】
** contents[] 是 C99 引入的柔性数组成员
** 它必须是结构体的最后一个成员，且大小在编译时未知
** 分配内存时，可以为其分配任意大小的空间
**
** 【计算公式解析】
** sizestrshr(l) = offsetof(TString, contents) + (l + 1) * sizeof(char)
**              = TString头部大小 + (字符串长度 + 1) * 1字节
**              = 头部大小 + 字符串内容空间（含终止符）
**
** 【为什么要 +1】
** C 语言字符串以 '\0' 结尾，长度为 l 的字符串实际需要 l+1 个字节存储
**
** 【内存布局示意】
** +------------------+----------------------+
** |  TString 头部    |  字符串内容 + '\0'   |
** +------------------+----------------------+
** |<-- offsetof -->  |<--  (l+1) bytes  --> |
*/

#define luaS_newliteral(L, s) (luaS_newlstr(L, "" s, \
                                            (sizeof(s) / sizeof(char)) - 1))
/*
** 【宏定义说明: luaS_newliteral(L, s)】
**
** 用途: 从 C 字符串字面量创建 Lua 字符串的便捷宏
** 参数:
**   L - Lua 状态机指针
**   s - 字符串字面量（必须是编译时常量，如 "hello"）
**
** 展开结果: 调用 luaS_newlstr(L, "" s, 字符串长度)
**
** 【C语言知识点: 字符串字面量拼接】
** "" s 利用了 C 语言的字符串字面量自动拼接特性
** 相邻的字符串字面量会被编译器自动合并
** 例如: "" "hello" 等价于 "hello"
**
** 这里使用 "" s 的技巧作用:
**   1. 确保 s 是字符串字面量，而不是 char* 变量
**   2. 如果 s 不是字符串字面量，编译会报错
**   这是一种编译期类型检查
**
** 【C语言知识点: sizeof 与字符串】
** sizeof(s) 对于字符串字面量返回包含 '\0' 的总大小
** 例如: sizeof("hello") = 6 (5个字符 + 1个'\0')
**
** sizeof(s)/sizeof(char) 计算字符数（含'\0'）
** 由于 sizeof(char) 恒等于 1，这里除法其实可以省略
** 但写出来更加规范，表明意图是计算字符个数
**
** 减1是为了得到不含'\0'的实际字符串长度
**
** 【使用示例】
** TString *str = luaS_newliteral(L, "hello");
** // 展开为: luaS_newlstr(L, "" "hello", (sizeof("hello")/sizeof(char))-1)
** // 即:     luaS_newlstr(L, "hello", 5)
**
** 【优点】
** 1. 长度在编译时计算，运行时无需调用 strlen
** 2. 类型安全，只接受字符串字面量
*/

/*
** test whether a string is a reserved word
** 测试一个字符串是否是保留字
*/
#define isreserved(s) (strisshr(s) && (s)->extra > 0)
/*
** 【宏定义说明: isreserved(s)】
**
** 用途: 检查字符串 s 是否是 Lua 保留字（关键字）
** 参数: s - TString 指针
** 返回: 布尔值（非零表示是保留字）
**
** 【判断条件解析】
** 1. strisshr(s): 检查是否为短字符串
**    - 保留字都很短，所以必定是短字符串
**    - strisshr 宏定义在 lobject.h 中
**
** 2. (s)->extra > 0: 检查 extra 字段是否大于 0
**    - TString 结构体有一个 extra 字段
**    - 对于保留字，extra 存储该保留字的编号（1-based）
**    - 对于普通字符串，extra 为 0
**
** 【Lua 保留字列表】
** and, break, do, else, elseif, end, false, for, function, goto,
** if, in, local, nil, not, or, repeat, return, then, true, until, while
** 共 22 个保留字
**
** 【设计意图】
** 保留字是 Lua 语法的一部分，需要特殊处理
** 例如：不能用保留字作为变量名
** 通过 extra 字段快速判断，避免字符串比较
*/

/*
** equality for short strings, which are always internalized
** 短字符串的相等性判断，短字符串总是被内化的
*/
#define eqshrstr(a, b) check_exp((a)->tt == LUA_VSHRSTR, (a) == (b))
/*
** 【宏定义说明: eqshrstr(a, b)】
**
** 用途: 比较两个短字符串是否相等
** 参数: a, b - 两个 TString 指针（必须是短字符串）
** 返回: 布尔值（非零表示相等）
**
** 【C语言知识点: check_exp 宏】
** check_exp(cond, expr) 是 Lua 定义的调试宏
** 在调试模式下: 先检查 cond 是否为真，然后返回 expr
** 在发布模式下: 直接返回 expr，忽略 cond
** 定义在 llimits.h 中
**
** 【为什么短字符串比较只需比较指针?】
** 这是字符串内化(interning)带来的优化！
**
** 内化保证: 所有相同内容的短字符串在内存中只有一份副本
** 因此: 如果两个短字符串的指针相同，内容必定相同
**       如果两个短字符串的指针不同，内容必定不同
**
** 这将字符串比较从 O(n) 降低到 O(1)！
**
** 【LUA_VSHRSTR】
** 这是短字符串的类型标签，定义在 lobject.h 中
** tt 字段存储对象的类型信息
**
** 【使用场景】
** 当已知两个字符串都是短字符串时使用此宏
** 如果不确定类型，应使用 luaS_eqstr 函数
*/

/*
** ============================================================================
** 函数声明部分 (Function Declarations)
** ============================================================================
**
** 【C语言知识点: LUAI_FUNC 宏】
** LUAI_FUNC 是 Lua 定义的函数声明修饰宏
** 通常展开为 extern（外部链接）或特定的可见性属性
** 用于声明 Lua 内部函数，这些函数在多个源文件间共享
** 定义在 luaconf.h 中
**
** 【函数命名规范】
** luaS_ 前缀表示这是字符串子系统（String）的函数
** Lua 源码使用这种前缀来组织代码模块:
**   luaS_ - 字符串 (String)
**   luaT_ - 表 (Table)
**   luaM_ - 内存 (Memory)
**   luaG_ - 调试 (debuG)
**   luaC_ - 垃圾回收 (Collection)
**   等等...
*/

LUAI_FUNC unsigned luaS_hashlongstr(TString *ts);
/*
** 【函数声明: luaS_hashlongstr】
**
** 函数原型: unsigned luaS_hashlongstr(TString *ts)
**
** 功能: 计算长字符串的哈希值
**
** 参数:
**   ts - 指向长字符串的 TString 指针
**
** 返回值:
**   unsigned 类型的哈希值
**
** 【设计说明】
** 长字符串的哈希值采用"延迟计算"策略:
**   - 创建时不计算哈希值（节省创建时间）
**   - 第一次需要时才计算，并缓存结果
**   - 后续使用缓存的哈希值
**
** 这是因为很多长字符串可能永远不需要用作哈希表的键
** 延迟计算避免了不必要的开销
**
** 【与短字符串的区别】
** 短字符串在创建时就计算哈希值（因为要放入字符串表）
** 长字符串不进入字符串表，所以可以延迟计算
*/

LUAI_FUNC int luaS_eqstr(TString *a, TString *b);
/*
** 【函数声明: luaS_eqstr】
**
** 函数原型: int luaS_eqstr(TString *a, TString *b)
**
** 功能: 比较两个 Lua 字符串是否相等
**
** 参数:
**   a - 第一个字符串
**   b - 第二个字符串
**
** 返回值:
**   非零(真) - 两个字符串相等
**   零(假)   - 两个字符串不相等
**
** 【实现逻辑】
** 1. 如果两个都是短字符串: 直接比较指针（内化保证）
** 2. 如果类型不同: 不相等
** 3. 如果都是长字符串: 比较长度，然后逐字节比较内容
**
** 【为什么需要这个函数】
** eqshrstr 宏只能比较短字符串
** 这个函数可以比较任意两个字符串（短或长）
*/

LUAI_FUNC void luaS_resize(lua_State *L, int newsize);
/*
** 【函数声明: luaS_resize】
**
** 函数原型: void luaS_resize(lua_State *L, int newsize)
**
** 功能: 调整字符串表（哈希表）的大小
**
** 参数:
**   L       - Lua 状态机指针
**   newsize - 新的哈希表大小（桶的数量）
**
** 返回值: 无
**
** 【设计说明】
** 字符串表是一个哈希表，用于存储所有内化的短字符串
** 随着字符串数量增加/减少，需要动态调整哈希表大小
** 以保持良好的性能（避免过多的哈希冲突）
**
** 【调整时机】
** - 扩容: 当字符串数量超过当前大小时
** - 缩容: 在 GC 后，如果字符串数量明显减少
**
** 【哈希表基础知识】
** 哈希表通过哈希函数将键映射到"桶"(bucket)
** 理想情况下，每个桶只有一个元素
** 当多个键映射到同一个桶时，产生"冲突"
** 调整大小可以减少冲突，提高查找效率
*/

LUAI_FUNC void luaS_clearcache(global_State *g);
/*
** 【函数声明: luaS_clearcache】
**
** 函数原型: void luaS_clearcache(global_State *g)
**
** 功能: 清除字符串缓存
**
** 参数:
**   g - 全局状态指针
**
** 返回值: 无
**
** 【设计说明】
** Lua 维护了一个字符串缓存（strcache），用于加速字符串创建
** 缓存存储最近使用的字符串，避免重复的哈希表查找
**
** 在 GC 周期的某些阶段，需要清除这个缓存
** 因为缓存中的字符串可能已经被回收
**
** 【global_State vs lua_State】
** global_State: 全局状态，所有线程共享，包含字符串表、GC等
** lua_State: 线程/协程状态，每个线程一个，包含栈、调用链等
*/

LUAI_FUNC void luaS_init(lua_State *L);
/*
** 【函数声明: luaS_init】
**
** 函数原型: void luaS_init(lua_State *L)
**
** 功能: 初始化字符串子系统
**
** 参数:
**   L - Lua 状态机指针
**
** 返回值: 无
**
** 【初始化内容】
** 1. 创建字符串表（哈希表）
** 2. 预创建内存错误消息 (MEMERRMSG)
** 3. 初始化字符串缓存
** 4. 可能还会预创建一些常用字符串
**
** 【调用时机】
** 在 lua_newstate 创建新的 Lua 状态时调用
** 是 Lua 初始化流程的一部分
*/

LUAI_FUNC void luaS_remove(lua_State *L, TString *ts);
/*
** 【函数声明: luaS_remove】
**
** 函数原型: void luaS_remove(lua_State *L, TString *ts)
**
** 功能: 从字符串表中移除一个字符串
**
** 参数:
**   L  - Lua 状态机指针
**   ts - 要移除的字符串
**
** 返回值: 无
**
** 【使用场景】
** 当一个内化的短字符串被 GC 回收时，需要从字符串表中移除它
** 这样后续创建相同内容的字符串时，不会找到已失效的条目
**
** 【注意事项】
** - 只有短字符串才会被内化，才需要从字符串表移除
** - 长字符串不在字符串表中，不需要调用此函数
*/

LUAI_FUNC Udata *luaS_newudata(lua_State *L, size_t s,
                               unsigned short nuvalue);
/*
** 【函数声明: luaS_newudata】
**
** 函数原型: Udata *luaS_newudata(lua_State *L, size_t s, unsigned short nuvalue)
**
** 功能: 创建一个新的用户数据对象
**
** 参数:
**   L       - Lua 状态机指针
**   s       - 用户数据的大小（字节数）
**   nuvalue - 关联的用户值(user value)数量
**
** 返回值:
**   指向新创建的 Udata 对象的指针
**
** 【为什么在 lstring.h 中?】
** 虽然这个函数创建的是用户数据(userdata)而不是字符串
** 但它们的内存管理方式类似:
**   - 都是可变大小的对象
**   - 都需要特殊的内存分配处理
**
** 【Udata 是什么?】
** Userdata 是 Lua 中用于存储任意 C 数据的类型
** 可以在 Lua 中传递，但只能通过 C API 访问其内容
** 常用于封装 C 库的数据结构
**
** 【nuvalue 参数】
** 用户数据可以关联多个 Lua 值（称为 user values）
** 这些值可以是任意 Lua 类型，用于存储与该 userdata 相关的数据
*/

LUAI_FUNC TString *luaS_newlstr(lua_State *L, const char *str, size_t l);
/*
** 【函数声明: luaS_newlstr】
**
** 函数原型: TString *luaS_newlstr(lua_State *L, const char *str, size_t l)
**
** 功能: 从 C 字符串创建 Lua 字符串，指定长度
**
** 参数:
**   L   - Lua 状态机指针
**   str - C 字符串指针
**   l   - 字符串长度
**
** 返回值:
**   指向新创建（或已存在）的 TString 对象的指针
**
** 【核心函数】
** 这是创建 Lua 字符串的核心函数之一
**
** 【处理流程】
** 1. 如果 l <= LUAI_MAXSHORTLEN (短字符串):
**    a. 计算哈希值
**    b. 在字符串表中查找是否已存在相同内容的字符串
**    c. 如果存在，返回已有的字符串（内化）
**    d. 如果不存在，创建新字符串并加入字符串表
**
** 2. 如果 l > LUAI_MAXSHORTLEN (长字符串):
**    直接创建新的字符串对象，不进行内化
**
** 【参数 l 的作用】
** 显式指定长度允许:
**   - 处理包含 '\0' 的字符串（二进制数据）
**   - 处理非以 '\0' 结尾的字符串片段
*/

LUAI_FUNC TString *luaS_new(lua_State *L, const char *str);
/*
** 【函数声明: luaS_new】
**
** 函数原型: TString *luaS_new(lua_State *L, const char *str)
**
** 功能: 从以 '\0' 结尾的 C 字符串创建 Lua 字符串
**
** 参数:
**   L   - Lua 状态机指针
**   str - 以 '\0' 结尾的 C 字符串
**
** 返回值:
**   指向 TString 对象的指针
**
** 【与 luaS_newlstr 的关系】
** 这是 luaS_newlstr 的便捷封装
** 内部调用: luaS_newlstr(L, str, strlen(str))
**
** 【使用场景】
** 当你有一个标准的 C 字符串（以 '\0' 结尾）时使用
** 如果字符串可能包含嵌入的 '\0'，应使用 luaS_newlstr
*/

LUAI_FUNC TString *luaS_createlngstrobj(lua_State *L, size_t l);
/*
** 【函数声明: luaS_createlngstrobj】
**
** 函数原型: TString *luaS_createlngstrobj(lua_State *L, size_t l)
**
** 功能: 创建一个长字符串对象（只分配空间，不填充内容）
**
** 参数:
**   L - Lua 状态机指针
**   l - 字符串长度
**
** 返回值:
**   指向新创建的 TString 对象的指针
**   内容区域未初始化，调用者需要自行填充
**
** 【使用场景】
** 当需要分步构建长字符串时使用:
**   1. 先调用此函数分配空间
**   2. 然后直接向内容区域写入数据
**   3. 避免了额外的内存复制
**
** 例如: 读取大文件时，可以直接读入字符串对象的缓冲区
*/

LUAI_FUNC TString *luaS_newextlstr(lua_State *L,
                                   const char *s, size_t len, lua_Alloc falloc, void *ud);
/*
** 【函数声明: luaS_newextlstr】
**
** 函数原型: TString *luaS_newextlstr(lua_State *L, const char *s,
**                                     size_t len, lua_Alloc falloc, void *ud)
**
** 功能: 创建外部字符串（使用外部内存）
**
** 参数:
**   L      - Lua 状态机指针
**   s      - 外部字符串数据指针
**   len    - 字符串长度
**   falloc - 用于释放外部内存的分配器函数（可以为 NULL）
**   ud     - 传递给 falloc 的用户数据
**
** 返回值:
**   指向新创建的 TString 对象的指针
**
** 【C语言知识点: lua_Alloc 函数类型】
** lua_Alloc 是 Lua 定义的内存分配器函数类型:
**   typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);
**
** 这是一个函数指针类型，指向的函数:
**   - ud: 用户数据，每次调用都会传入
**   - ptr: 要重新分配的指针（NULL 表示新分配）
**   - osize: 原大小
**   - nsize: 新大小（0 表示释放）
**
** 【外部字符串的概念】
** 普通字符串: Lua 分配内存，复制数据，Lua 管理生命周期
** 外部字符串: 外部已有数据，Lua 只创建包装对象，不复制数据
**
** 【使用场景】
** - 大型只读数据（如内存映射文件）
** - 避免大数据的复制开销
** - 与外部系统共享内存
**
** 【注意事项】
** - 外部数据的生命周期必须至少与字符串对象一样长
** - 如果提供 falloc，在字符串被回收时会调用它释放外部内存
*/

LUAI_FUNC size_t luaS_sizelngstr(size_t len, int kind);
/*
** 【函数声明: luaS_sizelngstr】
**
** 函数原型: size_t luaS_sizelngstr(size_t len, int kind)
**
** 功能: 计算长字符串对象需要的内存大小
**
** 参数:
**   len  - 字符串长度
**   kind - 字符串类型/种类
**
** 返回值:
**   所需的总内存大小（字节）
**
** 【kind 参数】
** 长字符串可能有不同的存储方式:
**   - 普通长字符串: 数据存储在对象内部
**   - 外部长字符串: 数据存储在外部，对象只保存指针
**
** 不同类型的字符串需要不同的内存大小
**
** 【与 sizestrshr 的区别】
** sizestrshr 是宏，只用于短字符串，计算公式固定
** luaS_sizelngstr 是函数，用于长字符串，需要根据类型计算
*/

LUAI_FUNC TString *luaS_normstr(lua_State *L, TString *ts);
/*
** 【函数声明: luaS_normstr】
**
** 函数原型: TString *luaS_normstr(lua_State *L, TString *ts)
**
** 功能: 规范化字符串
**
** 参数:
**   L  - Lua 状态机指针
**   ts - 要规范化的字符串
**
** 返回值:
**   规范化后的字符串指针
**
** 【规范化的含义】
** 对于短字符串: 确保返回内化的版本
** 对于外部字符串: 可能转换为普通字符串
**
** 【使用场景】
** 当需要确保字符串处于"标准"状态时使用
** 例如: 作为表的键时，需要使用规范化的字符串
*/

#endif
/*
** 【头文件保护结束】
** 与文件开头的 #ifndef lstring_h 配对
** 标志头文件内容结束
*/

/*
** ============================================================================
** 总结 (Summary)
** ============================================================================
**
** 【文件核心要点回顾】
**
** 1. 字符串分类
**    ┌─────────────────────────────────────────────────────────────────┐
**    │  类型      │  长度        │  内化  │  哈希计算  │  比较方式    │
**    ├─────────────────────────────────────────────────────────────────┤
**    │  短字符串  │  ≤40 字符    │  是    │  创建时    │  指针比较    │
**    │  长字符串  │  >40 字符    │  否    │  延迟计算  │  内容比较    │
**    │  外部字符串│  任意        │  否    │  延迟计算  │  内容比较    │
**    └─────────────────────────────────────────────────────────────────┘
**
** 2. 关键数据结构
**    - TString: Lua 字符串的内部表示，包含头部信息和字符数据
**    - 字符串表 (String Table): 存储所有内化短字符串的哈希表
**    - 字符串缓存 (String Cache): 加速字符串查找的缓存
**
** 3. 宏定义总结
**    ┌─────────────────────────────────────────────────────────────────┐
**    │  宏名              │  用途                                      │
**    ├─────────────────────────────────────────────────────────────────┤
**    │  MEMERRMSG         │  内存不足错误消息                          │
**    │  LUAI_MAXSHORTLEN  │  短字符串最大长度阈值（默认40）            │
**    │  sizestrshr(l)     │  计算短字符串对象大小                      │
**    │  luaS_newliteral   │  从字面量创建字符串的便捷宏                │
**    │  isreserved(s)     │  检查是否为保留字                          │
**    │  eqshrstr(a,b)     │  短字符串相等比较（指针比较）              │
**    └─────────────────────────────────────────────────────────────────┘
**
** 4. 函数分类
**
**    【创建函数】
**    - luaS_new: 从 C 字符串创建（自动计算长度）
**    - luaS_newlstr: 从 C 字符串创建（指定长度）
**    - luaS_newliteral: 从字面量创建（宏）
**    - luaS_createlngstrobj: 创建长字符串对象
**    - luaS_newextlstr: 创建外部字符串
**    - luaS_newudata: 创建用户数据
**
**    【管理函数】
**    - luaS_init: 初始化字符串子系统
**    - luaS_resize: 调整字符串表大小
**    - luaS_remove: 从字符串表移除字符串
**    - luaS_clearcache: 清除字符串缓存
**
**    【工具函数】
**    - luaS_hashlongstr: 计算长字符串哈希
**    - luaS_eqstr: 字符串相等比较
**    - luaS_sizelngstr: 计算长字符串对象大小
**    - luaS_normstr: 规范化字符串
**
** 5. 字符串内化的优势
**    - 内存效率: 相同内容只存储一份
**    - 比较效率: O(1) 指针比较代替 O(n) 内容比较
**    - 哈希效率: 哈希值只计算一次并缓存
**
** 6. 设计模式与技巧
**    - 延迟计算: 长字符串哈希值按需计算
**    - 预分配: 错误消息预先创建
**    - 缓存: 字符串缓存加速查找
**    - 条件编译: 允许自定义配置（如 LUAI_MAXSHORTLEN）
**
** 【与其他模块的关系】
**
**                       ┌─────────────┐
**                       │   lstring   │
**                       │  (字符串)   │
**                       └──────┬──────┘
**                              │
**          ┌───────────────────┼───────────────────┐
**          │                   │                   │
**          ▼                   ▼                   ▼
**    ┌───────────┐      ┌───────────┐      ┌───────────┐
**    │   lobject │      │    lgc    │      │   lstate  │
**    │  (对象)   │      │   (GC)    │      │  (状态)   │
**    └───────────┘      └───────────┘      └───────────┘
**          │                   │                   │
**          │                   │                   │
**          └───────────────────┴───────────────────┘
**                              │
**                              ▼
**                     ┌───────────────┐
**                     │  TString 结构 │
**                     │  字符串表     │
**                     │  GC 对象      │
**                     └───────────────┘
**
** 【阅读建议】
**
** 1. 先阅读 lobject.h 了解 TString 结构体的定义
** 2. 再阅读 lstring.c 了解这些函数的具体实现
** 3. 关注字符串表的哈希表实现细节
** 4. 理解短字符串和长字符串的不同处理路径
**
** 【关键源文件】
** - lstring.h: 本文件，接口声明
** - lstring.c: 实现文件
** - lobject.h: TString 结构定义
** - lstate.h: 全局状态中的字符串表定义
**
** ============================================================================
*/