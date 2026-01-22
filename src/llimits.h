/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** 限制、基本类型以及一些其他"依赖于安装环境"的定义
** See Copyright Notice in lua.h
** 参见 lua.h 中的版权声明
*/

/*
** ============================================================================
** 文件概要说明
** ============================================================================
**
** 文件名: llimits.h
** 功能: Lua虚拟机的底层限制定义和类型抽象
**
** 主要内容:
** 1. 内存相关类型定义 (l_mem, lu_mem)
**    - 用于追踪Lua使用的总内存量
**    - 有符号和无符号版本,适配不同平台(16位/32位)
**
** 2. 基本类型定义
**    - lu_byte: 无符号字节类型,用于小的自然数
**    - ls_byte: 有符号字节类型
**    - TStatus: 线程状态/错误码类型
**    - l_uint32: 至少4字节的无符号整数
**
** 3. 类型转换宏
**    - cast系列: 统一的类型转换接口,使代码中的类型转换更显眼
**    - 指针转换、整数转换等各种特定用途的转换宏
**
** 4. 平台相关的宏定义
**    - 内联函数定义 (l_inline)
**    - 无返回函数标记 (l_noret)
**    - 导出符号控制 (LUAI_FUNC)
**
** 5. 数值运算抽象
**    - luai_num*系列宏: 抽象了基本数值运算
**    - 包括除法、模运算、幂运算等
**    - 允许在不同平台上定制数值运算行为
**
** 6. 调试和断言支持
**    - lua_assert: 内部断言宏
**    - check_exp: 带检查的表达式求值
**
** 7. 实用工具宏
**    - UNUSED: 避免未使用变量警告
**    - ispow2: 判断是否为2的幂
**    - LL: 计算字面字符串长度(不含结尾\0)
**
** 设计理念:
** - 通过宏和类型定义隔离平台差异
** - 为Lua虚拟机提供统一的底层接口
** - 便于移植到不同的硬件和编译器环境
** ============================================================================
*/

#ifndef llimits_h
#define llimits_h

#include <limits.h> /* 包含C标准库的限制定义,如INT_MAX, CHAR_BIT等 */
#include <stddef.h> /* 包含标准定义,如size_t, ptrdiff_t, NULL等 */

#include "lua.h" /* 包含Lua的公共API定义 */

/*
** l_numbits - 计算类型的比特数
** 参数: t - 任意类型
** 返回: 该类型占用的比特数
**
** 实现说明:
** - sizeof(t) 返回字节数
** - CHAR_BIT 是每字节的比特数(通常是8)
** - cast_int 将结果转换为int类型
**
** 使用场景: 用于需要知道类型位宽的位运算操作
*/
#define l_numbits(t) cast_int(sizeof(t) * CHAR_BIT)

/*
** 'l_mem' is a signed integer big enough to count the total memory
** used by Lua.  (It is signed due to the use of debt in several
** computations.) 'lu_mem' is a corresponding unsigned type.  Usually,
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.https://gemini.google.com/app/74cb7cf70647ac58
**
** 'l_mem' 是一个有符号整数,大到足以计数Lua使用的总内存。
** (它是有符号的,因为在几个计算中使用了负债/欠款的概念。)
** 'lu_mem' 是对应的无符号类型。
** 通常,'ptrdiff_t' 应该可以工作,但我们对16位机器使用 'long'。
**
** 类型选择逻辑:
** 1. 如果定义了LUAI_MEM,使用外部定义的类型
** 2. 如果是32位整数系统(LUAI_IS32INT),使用ptrdiff_t和size_t
**    - ptrdiff_t: 两个指针之间的差值类型,有符号
**    - size_t: sizeof运算符的返回类型,无符号
** 3. 否则(16位系统),使用long和unsigned long
**
** 为什么需要有符号类型?
** - Lua的垃圾回收器使用"债务"(debt)机制
** - 需要表示负数来跟踪内存分配的欠账
*/
#if defined(LUAI_MEM) /* { external definitions? */ /* { 外部定义? */
typedef LUAI_MEM l_mem;
typedef LUAI_UMEM lu_mem;
#elif LUAI_IS32INT /* }{ */      /* }{ 32位整数 */
typedef ptrdiff_t l_mem;
typedef size_t lu_mem;
#else /* 16-bit ints */ /* }{ */ /* }{ 16位整数 */
typedef long l_mem;
typedef unsigned long lu_mem;
#endif                           /* } */

/*
** MAX_LMEM - l_mem类型的最大值
**
** 计算方法详解:
** 1. cast(lu_mem, 1): 将1转换为无符号内存类型
** 2. << (l_numbits(l_mem) - 1): 左移到最高位
**    - 对于32位: 1 << 31 = 0x80000000
**    - 对于64位: 1 << 63 = 0x8000000000000000
** 3. - 1: 减1得到所有低位为1的数
**    - 对于32位: 0x7FFFFFFF (2147483647)
**    - 对于64位: 0x7FFFFFFFFFFFFFFF
** 4. cast(l_mem, ...): 转换为有符号类型
**
** 这个值等于有符号整数的最大值 (INT_MAX或LONG_MAX)
*/
#define MAX_LMEM \
  cast(l_mem, (cast(lu_mem, 1) << (l_numbits(l_mem) - 1)) - 1)

/*
** chars used as small naturals (so that 'char' is reserved for characters)
** 用作小自然数的字符类型(这样'char'就保留给字符使用)
**
** 设计意图:
** - lu_byte: 用于表示小的自然数(0-255),如枚举值、标志等
** - ls_byte: 用于表示小的有符号数(-128到127)
** - char: 保留给真正的字符数据使用
**
** 这种区分提高了代码的可读性和类型安全性
*/
typedef unsigned char lu_byte;
typedef signed char ls_byte;

/*
** Type for thread status/error codes
** 线程状态/错误码的类型
**
** 使用lu_byte(无符号字节)来表示状态码
** - 节省内存(只需1字节)
** - 状态码通常是小的非负整数
** - 定义在lua.h中,如LUA_OK(0), LUA_YIELD(1)等
*/
typedef lu_byte TStatus;

/*
** The C API still uses 'int' for status/error codes
** C API仍然使用'int'表示状态/错误码
**
** APIstatus - 将内部的TStatus(lu_byte)转换为C API的int
**
** 原因:
** - C API需要保持向后兼容性
** - 外部C代码期望int类型的状态码
** - 内部使用lu_byte节省内存,对外使用int保持兼容
*/
#define APIstatus(st) cast_int(st)

/*
** maximum value for size_t
** size_t的最大值
**
** 计算方法:
** - ~(size_t)0: 对0按位取反
**   - 所有位都是1
**   - 对于32位: 0xFFFFFFFF (4294967295)
**   - 对于64位: 0xFFFFFFFFFFFFFFFF
** - (size_t)(...): 确保类型正确
**
** 这是size_t能表示的最大值,也是理论上最大的内存大小
*/
#define MAX_SIZET ((size_t)(~(size_t)0))

/*
** Maximum size for strings and userdata visible for Lua; should be
** representable as a lua_Integer and as a size_t.
**
** Lua中字符串和用户数据可见的最大大小;应该能够
** 同时表示为lua_Integer和size_t。
**
** 限制说明:
** 1. Lua中的字符串长度需要能用lua_Integer表示(用于#运算符)
** 2. 实际内存分配需要能用size_t表示
** 3. MAX_SIZE取两者中较小的一个
**
** 判断逻辑:
** - 如果size_t小于lua_Integer(如32位size_t vs 64位lua_Integer)
**   使用MAX_SIZET
** - 否则使用LUA_MAXINTEGER并转换为size_t
**
** 这确保了字符串长度在两种类型系统中都能安全表示
*/
#define MAX_SIZE (sizeof(size_t) < sizeof(lua_Integer) ? MAX_SIZET \
                                                       : cast_sizet(LUA_MAXINTEGER))

/*
** test whether an unsigned value is a power of 2 (or zero)
** 测试一个无符号值是否是2的幂(或0)
**
** 原理:
** - 2的幂的二进制表示只有一个1: 如 8 = 1000
** - x-1会将这个1变为0,后面的0变为1: 7 = 0111
** - x & (x-1) 会得到0
**
** 示例:
** - ispow2(8): 8=1000, 7=0111, 1000&0111=0000, 结果为真
** - ispow2(6): 6=0110, 5=0101, 0110&0101=0100, 结果为假
** - ispow2(0): 0=0000, -1=1111, 0000&1111=0000, 结果为真(特殊情况)
**
** 用途: 常用于哈希表大小检查,哈希表通常要求大小为2的幂
*/
#define ispow2(x) (((x) & ((x) - 1)) == 0)

/*
** number of chars of a literal string without the ending \0
** 字面字符串的字符数量(不包括结尾的\0)
**
** 计算方法:
** - sizeof(x): 包含\0的总字节数
** - sizeof(char): 通常是1
** - -1: 减去结尾的\0
**
** 示例:
** - LL("hello"): sizeof("hello")=6, LL("hello")=5
**
** 用途: 在编译时计算字符串字面量的长度
*/
#define LL(x) (sizeof(x) / sizeof(char) - 1)

/*
** conversion of pointer to unsigned integer: this is for hashing only;
** there is no problem if the integer cannot hold the whole pointer
** value. (In strict ISO C this may cause undefined behavior, but no
** actual machine seems to bother.)
**
** 将指针转换为无符号整数:这仅用于哈希;
** 如果整数不能容纳整个指针值也没有问题。
** (在严格的ISO C中这可能导致未定义行为,但实际上
** 没有机器似乎在意这个问题。)
**
** L_P2I类型选择:
** 1. C99及以后,且定义了UINTPTR_MAX: 使用uintptr_t
**    - uintptr_t专门设计用来存储指针转换后的整数
**    - 但在C99中这是可选类型
** 2. C99但没有uintptr_t: 使用uintmax_t(最大的整数类型)
** 3. C89: 使用size_t
**
** 为什么这样设计?
** - 仅用于哈希,不需要可逆转换
** - 即使截断了高位,低位仍然足够分散
** - 实用主义:几乎所有现代机器都能正常工作
*/
#if !defined(LUA_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>                                              /* C99标准整数类型 */
#if defined(UINTPTR_MAX) /* even in C99 this type is optional */ /* 即使在C99中这个类型也是可选的 */
#define L_P2I uintptr_t
#else /* no 'intptr'? */                                        /* 没有'intptr'? */
#define L_P2I uintmax_t /* use the largest available integer */ /* 使用最大的可用整数 */
#endif
#else /* C89 option */ /* C89选项 */
#define L_P2I size_t
#endif

/*
** point2uint - 将指针转换为无符号整数
**
** 转换步骤:
** 1. (L_P2I)(p): 将指针p转换为L_P2I类型
** 2. & UINT_MAX: 只保留低32位(或更少,取决于UINT_MAX)
**    - UINT_MAX通常是0xFFFFFFFF(32位)
** 3. cast_uint(...): 转换为unsigned int
**
** 用途: 在哈希函数中将指针转换为可哈希的整数值
*/
#define point2uint(p) cast_uint((L_P2I)(p) & UINT_MAX)

/*
** types of 'usual argument conversions' for lua_Number and lua_Integer
** lua_Number和lua_Integer的"常规参数转换"类型
**
** 说明:
** - 在C中,当传递参数时会发生"常规算术转换"
** - float会提升到double
** - 小整数会提升到int
** - 这些类型定义了在Lua内部进行类似转换后的类型
** - LUAI_UACNUMBER和LUAI_UACINT在luaconf.h中定义
**
** 用途: 确保数值运算时使用正确的中间类型
*/
typedef LUAI_UACNUMBER l_uacNumber;
typedef LUAI_UACINT l_uacInt;

/*
** Internal assertions for in-house debugging
** 内部断言,用于内部调试
**
** 逻辑:
** - 如果定义了LUAI_ASSERT,则启用断言
** - #undef NDEBUG: 确保断言被启用(assert.h中的宏)
** - lua_assert(c): 使用标准库的assert
** - assert_code(c): 保留代码c(通常用于仅在调试时执行的代码)
*/
#if defined LUAI_ASSERT
#undef NDEBUG
#include <assert.h> /* 标准断言库 */
#define lua_assert(c) assert(c)
#define assert_code(c) c
#endif

/*
** 如果没有定义断言,则将其定义为空操作
** - lua_assert(c): (void)0 - 什么都不做
** - assert_code(c): (void)0 - 移除代码c
**
** 好处:
** - 发布版本中没有性能开销
** - 代码中的断言仍然存在,有助于理解代码意图
*/
#if defined(lua_assert)
#else
#define lua_assert(c) ((void)0)
#define assert_code(c) ((void)0)
#endif

/*
** check_exp - 检查表达式
** 参数: c - 条件, e - 表达式
** 返回: e的值
**
** 工作原理:
** - 逗号运算符: (a, b) 先求值a,再求值b,返回b
** - lua_assert(c): 检查条件c
** - 返回表达式e的值
**
** 用途:
** - 在求值表达式前检查前提条件
** - 调试版本中检查条件,发布版本中只求值表达式
**
** 示例: check_exp(x > 0, sqrt(x))
*/
#define check_exp(c, e) (lua_assert(c), (e))

/*
** to avoid problems with conditions too long
** 避免条件太长的问题
**
** lua_longassert - 长断言
**
** 实现:
** - assert_code确保只在启用断言时编译
** - 三元运算符: (c) ? (void)0 : lua_assert(0)
** - 如果c为真,什么都不做
** - 如果c为假,触发断言失败
**
** 为什么需要这个?
** - 某些复杂条件可能导致宏展开过长
** - 这种形式避免了将整个条件传递给assert
*/
#define lua_longassert(c) assert_code((c) ? (void)0 : lua_assert(0))

/*
** macro to avoid warnings about unused variables
** 避免未使用变量警告的宏
**
** UNUSED(x) - 标记变量x为有意未使用
**
** 工作原理:
** - (void)(x): 将x转换为void,表示有意忽略
** - 这会"使用"变量,避免编译器警告
**
** 使用场景:
** - 回调函数中不需要的参数
** - 条件编译中某些配置不使用的变量
**
** 示例: UNUSED(argc); // 在不需要参数个数的函数中
*/
#if !defined(UNUSED)
#define UNUSED(x) ((void)(x))
#endif

/*
** type casts (a macro highlights casts in the code)
** 类型转换(宏使代码中的转换更醒目)
**
** 设计理念:
** - 类型转换可能隐藏bug
** - 使用宏使转换在代码中更显眼
** - 便于搜索和审查所有类型转换
** - 提供语义化的转换名称
*/

/*
** cast - 通用类型转换宏
** 参数: t - 目标类型, exp - 表达式
**
** 这是基础的转换宏,其他所有转换都基于此
*/
#define cast(t, exp) ((t)(exp))

/* 各种特定用途的类型转换宏,提高代码可读性 */
#define cast_void(i) cast(void, (i))           /* 转换为void */
#define cast_voidp(i) cast(void *, (i))        /* 转换为void指针 */
#define cast_num(i) cast(lua_Number, (i))      /* 转换为Lua数值类型 */
#define cast_int(i) cast(int, (i))             /* 转换为int */
#define cast_short(i) cast(short, (i))         /* 转换为short */
#define cast_uint(i) cast(unsigned int, (i))   /* 转换为unsigned int */
#define cast_byte(i) cast(lu_byte, (i))        /* 转换为lu_byte */
#define cast_uchar(i) cast(unsigned char, (i)) /* 转换为unsigned char */
#define cast_char(i) cast(char, (i))           /* 转换为char */
#define cast_charp(i) cast(char *, (i))        /* 转换为char指针 */
#define cast_sizet(i) cast(size_t, (i))        /* 转换为size_t */
#define cast_Integer(i) cast(lua_Integer, (i)) /* 转换为Lua整数类型 */
#define cast_Inst(i) cast(Instruction, (i))    /* 转换为指令类型 */

/*
** cast a signed lua_Integer to lua_Unsigned
** 将有符号的lua_Integer转换为lua_Unsigned
**
** 如果未定义,默认实现为直接强制转换
**
** 注意:
** - 这种转换在C中是实现定义的(implementation-defined)
** - 现代补码(two's complement)系统中行为是可预测的
** - 负数会变成大的无符号数
**
** 示例: -1 (0xFFFFFFFF) -> 4294967295
*/
#if !defined(l_castS2U)
#define l_castS2U(i) ((lua_Unsigned)(i))
#endif

/*
** cast a lua_Unsigned to a signed lua_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
**
** 将lua_Unsigned转换为有符号的lua_Integer;这种转换
** 不是严格的ISO C,但补码架构应该工作正常。
**
** 注意:
** - 严格来说,转换超出范围的无符号数到有符号数是未定义行为
** - 但所有现代系统都使用补码表示,行为是确定的
** - 大的无符号数会变成负数
**
** 示例: 4294967295 (0xFFFFFFFF) -> -1
*/
#if !defined(l_castU2S)
#define l_castU2S(i) ((lua_Integer)(i))
#endif

/*
** cast a size_t to lua_Integer: These casts are always valid for
** sizes of Lua objects (see MAX_SIZE)
**
** 将size_t转换为lua_Integer:对于Lua对象的大小,
** 这些转换总是有效的(参见MAX_SIZE)
**
** 安全性:
** - MAX_SIZE确保了Lua对象大小既能用size_t表示,也能用lua_Integer表示
** - 因此这个转换不会溢出
**
** 用途: 在需要将对象大小作为整数返回给Lua时使用
*/
#define cast_st2S(sz) ((lua_Integer)(sz))

/*
** Cast a ptrdiff_t to size_t, when it is known that the minuend
** comes from the subtrahend (the base)
**
** 当已知被减数来自减数(基数)时,将ptrdiff_t转换为size_t
**
** 使用场景:
** - p1 - p2,其中p1和p2指向同一个数组
** - 结果保证非负(p1 >= p2)
** - 可以安全地转换为size_t
**
** 为什么安全?
** - ptrdiff_t是有符号的,表示指针差值
** - 当差值已知非负时,可以转换为无符号的size_t
** - 用于数组索引计算等场景
*/
#define ct_diff2sz(df) ((size_t)(df))

/*
** ptrdiff_t to lua_Integer
** ptrdiff_t转lua_Integer
**
** 先转size_t,再转lua_Integer
** 这确保了正确的符号扩展
*/
#define ct_diff2S(df) cast_st2S(ct_diff2sz(df))

/*
** Special type equivalent to '(void*)' for functions (to suppress some
** warnings when converting function pointers)
**
** 等价于'(void*)'的特殊类型,用于函数(在转换函数指针时抑制某些警告)
**
** 问题:
** - C标准不允许在函数指针和void*之间转换
** - 但POSIX和大多数实际系统都支持
** - 直接转换会产生编译器警告
**
** 解决方案:
** - 定义函数指针类型voidf
** - 作为函数指针转换的中介类型
*/
typedef void (*voidf)(void);

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
**
** 将void*指针转换为函数指针的宏。根据ISO C这种转换是未定义的,
** 但POSIX假定它可以工作。
** (gnu编译器中的'__extension__'只是为了避免警告。)
**
** GCC特殊处理:
** - __extension__: 告诉GCC这是有意的扩展用法,不要警告
**
** 使用场景:
** - dlsym等动态加载函数返回void*
** - 需要转换为函数指针使用
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__(voidf)(p))
#else
#define cast_func(p) ((voidf)(p))
#endif

/*
** non-return type
** 无返回类型
**
** l_noret - 标记函数不会返回
**
** 用途:
** - 编译器优化:知道函数不返回可以生成更好的代码
** - 静态分析:帮助检测控制流问题
** - 文档作用:明确函数不会返回
**
** 适用函数:
** - 错误处理函数(抛出错误)
** - 退出函数(exit, abort)
** - 长跳转函数(longjmp)
*/
#if !defined(l_noret)

#if defined(__GNUC__)
/* GCC特性:__attribute__((noreturn)) */
#define l_noret void __attribute__((noreturn))
#elif defined(_MSC_VER) && _MSC_VER >= 1200
/* MSVC特性:__declspec(noreturn) */
#define l_noret void __declspec(noreturn)
#else
/* 不支持的编译器:退化为普通void */
#define l_noret void
#endif

#endif

/*
** Inline functions
** 内联函数
**
** l_inline - 内联函数关键字
**
** 内联的好处:
** - 避免函数调用开销
** - 允许编译器进一步优化
** - 对于小函数很有用
**
** 平台适配:
** - C99及以后:使用标准的inline关键字
** - GCC在C89模式:使用__inline__扩展
** - 其他:定义为空,让编译器自行决定
*/
#if !defined(LUA_USE_C89)
#define l_inline inline
#elif defined(__GNUC__)
#define l_inline __inline__
#else
#define l_inline /* empty */ /* 空 */
#endif

/*
** l_sinline - 静态内联
**
** static inline的组合:
** - static: 限制在当前编译单元
** - inline: 建议编译器内联
**
** 用途:
** - 头文件中的小工具函数
** - 每个编译单元有自己的副本
** - 避免链接冲突
*/
#define l_sinline static l_inline

/*
** An unsigned with (at least) 4 bytes
** 一个(至少)4字节的无符号数
**
** l_uint32 - 至少32位的无符号整数
**
** 选择逻辑:
** - 如果是32位int系统:unsigned int肯定是4字节
** - 否则:使用unsigned long(在某些老系统上更安全)
**
** 用途:
** - 需要确保至少32位的场景
** - 位运算、哈希计算等
*/
#if LUAI_IS32INT
typedef unsigned int l_uint32;
#else
typedef unsigned long l_uint32;
#endif

/*
** The luai_num* macros define the primitive operations over numbers.
** luai_num*宏定义了对数值的原始操作。
**
** 设计目的:
** - 抽象数值运算,允许不同平台定制
** - 可以使用特殊的硬件指令
** - 可以调整浮点数行为
** - 统一处理特殊情况
*/

/*
** floor division (defined as 'floor(a/b)')
** 向下取整除法(定义为'floor(a/b)')
**
** luai_numidiv - 整数除法(向负无穷取整)
**
** 实现:
** - (void)L: 忽略L参数(在某些实现中可能需要)
** - l_floor: 向下取整函数
** - luai_numdiv(L,a,b): 浮点除法
**
** 示例:
** - 7 / 2 = 3.5, floor(3.5) = 3
** - -7 / 2 = -3.5, floor(-3.5) = -4
**
** 注意: 这与C的/运算符不同,C向0取整
*/
#if !defined(luai_numidiv)
#define luai_numidiv(L, a, b) ((void)L, l_floor(luai_numdiv(L, a, b)))
#endif

/*
** float division
** 浮点除法
**
** luai_numdiv - 浮点除法
**
** 默认实现:
** - 直接使用C的/运算符
**
** 可定制点:
** - 某些平台可能需要特殊处理
** - 如检查除零、处理特殊值等
*/
#if !defined(luai_numdiv)
#define luai_numdiv(L, a, b) ((a) / (b))
#endif

/*
** modulo: defined as 'a - floor(a/b)*b'; the direct computation
** using this definition has several problems with rounding errors,
** so it is better to use 'fmod'. 'fmod' gives the result of
** 'a - trunc(a/b)*b', and therefore must be corrected when
** 'trunc(a/b) ~= floor(a/b)'. That happens when the division has a
** non-integer negative result: non-integer result is equivalent to
** a non-zero remainder 'm'; negative result is equivalent to 'a' and
** 'b' with different signs, or 'm' and 'b' with different signs
** (as the result 'm' of 'fmod' has the same sign of 'a').
**
** 模运算:定义为'a - floor(a/b)*b';使用这个定义直接计算
** 会有几个舍入错误的问题,所以最好使用'fmod'。'fmod'给出
** 'a - trunc(a/b)*b'的结果,因此当'trunc(a/b) ~= floor(a/b)'时
** 必须进行修正。当除法有非整数负结果时会发生这种情况:
** 非整数结果等价于非零余数'm';负结果等价于'a'和'b'符号不同,
** 或'm'和'b'符号不同(因为'fmod'的结果'm'与'a'同号)。
**
** luai_nummod - 模运算
**
** 实现细节:
** 1. m = fmod(a, b): 使用C库的fmod
**    - fmod: 返回a - trunc(a/b)*b
**    - 结果符号与a相同
**
** 2. 需要修正的情况:
**    - Lua要求: a % b = a - floor(a/b)*b
**    - fmod给出: a - trunc(a/b)*b
**    - 当a/b为负且非整数时,trunc和floor不同
**
** 3. 修正条件: ((m > 0) ? (b) < 0 : ((m) < 0 && (b) > 0))
**    - m > 0 且 b < 0: m需要减去|b|,即m += b
**    - m < 0 且 b > 0: m需要加上|b|,即m += b
**
** 示例:
** - 7 % 3: fmod(7,3)=1, 无需修正, 结果=1
** - -7 % 3: fmod(-7,3)=-1, m<0且b>0, -1+3=2, 结果=2
** - 7 % -3: fmod(7,-3)=1, m>0且b<0, 1+(-3)=-2, 结果=-2
*/
#if !defined(luai_nummod)
#define luai_nummod(L, a, b, m)                     \
  {                                                 \
    (void)L;                                        \
    (m) = l_mathop(fmod)(a, b);                     \
    if (((m) > 0) ? (b) < 0 : ((m) < 0 && (b) > 0)) \
      (m) += (b);                                   \
  }
#endif

/*
** exponentiation
** 幂运算
**
** luai_numpow - 计算a的b次幂
**
** 优化:
** - 特殊处理b=2的情况: a*a 比 pow(a,2) 快得多
** - 其他情况使用标准库的pow函数
**
** l_mathop(pow):
** - 宏,可能展开为pow或powf(取决于lua_Number类型)
**
** 用途: 实现Lua的^运算符
*/
#if !defined(luai_numpow)
#define luai_numpow(L, a, b) \
  ((void)L, (b == 2) ? (a) * (a) : l_mathop(pow)(a, b))
#endif

/*
** the others are quite standard operations
** 其他是相当标准的操作
**
** 基本算术和比较运算:
** - 直接映射到C运算符
** - 允许在特殊平台上定制
** - 提供统一的接口
*/
#if !defined(luai_numadd)
#define luai_numadd(L, a, b) ((a) + (b))         /* 加法 */
#define luai_numsub(L, a, b) ((a) - (b))         /* 减法 */
#define luai_nummul(L, a, b) ((a) * (b))         /* 乘法 */
#define luai_numunm(L, a) (-(a))                 /* 取负 */
#define luai_numeq(a, b) ((a) == (b))            /* 相等 */
#define luai_numlt(a, b) ((a) < (b))             /* 小于 */
#define luai_numle(a, b) ((a) <= (b))            /* 小于等于 */
#define luai_numgt(a, b) ((a) > (b))             /* 大于 */
#define luai_numge(a, b) ((a) >= (b))            /* 大于等于 */
#define luai_numisnan(a) (!luai_numeq((a), (a))) /* 是否为NaN */
/* NaN检测原理: NaN不等于自身,这是IEEE 754的特性 */
#endif

/*
** lua_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if the float is not within the range of
** a lua_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
**
** lua_numbertointeger将具有整数值的浮点数转换为整数,
** 或者如果浮点数不在lua_Integer范围内则返回0。
** (范围比较很棘手,因为舍入的原因。这里的测试假定补码表示,
** 其中MININTEGER总是有精确的浮点表示;MAXINTEGER可能没有,
** 因此它到浮点的转换可能有未定义的值。)
**
** 参数:
** - n: 要转换的浮点数
** - p: 指向结果整数的指针
**
** 返回:
** - 1: 转换成功,结果存储在*p中
** - 0: 转换失败(超出范围或不是整数)
**
** 范围检查详解:
** 1. n >= (LUA_NUMBER)(LUA_MININTEGER)
**    - 检查下界
**    - MININTEGER可以精确表示为浮点数
**
** 2. n < -(LUA_NUMBER)(LUA_MININTEGER)
**    - 检查上界
**    - 使用-MININTEGER而不是MAXINTEGER
**    - 原因: 补码系统中,|MININTEGER| = MAXINTEGER + 1
**    - -MININTEGER可以精确表示,避免舍入问题
**
** 3. *(p) = (LUA_INTEGER)(n)
**    - 执行实际转换
**
** 4. , 1
**    - 逗号运算符,返回1表示成功
**
** 整体逻辑:
** - 所有条件都满足时,转换并返回1
** - 任一条件不满足,返回0
*/
#define lua_numbertointeger(n, p)         \
  ((n) >= (LUA_NUMBER)(LUA_MININTEGER) && \
   (n) < -(LUA_NUMBER)(LUA_MININTEGER) && \
   (*(p) = (LUA_INTEGER)(n), 1))

/*
** LUAI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
** LUAI_DDEF and LUAI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (LUAI_DDEF for
** definitions and LUAI_DDEC for declarations).
** Elf and MACH/gcc (versions 3.2 and later) mark them as "hidden" to
** optimize access when Lua is compiled as a shared library. Not all elf
** targets support this attribute. Unfortunately, gcc does not offer
** a way to check whether the target offers that support, and those
** without support give a warning about it. To avoid these warnings,
** change to the default definition.
**
** LUAI_FUNC是所有不导出到外部模块的外部函数的标记。
** LUAI_DDEF和LUAI_DDEC是所有外部(const)变量的标记,
** 都不导出到外部模块(LUAI_DDEF用于定义,LUAI_DDEC用于声明)。
** Elf和MACH/gcc(3.2及更高版本)将它们标记为"隐藏",
** 以在Lua编译为共享库时优化访问。并非所有elf目标都支持此属性。
** 不幸的是,gcc没有提供检查目标是否支持的方法,
** 不支持的目标会给出警告。为避免这些警告,可更改为默认定义。
**
** 符号可见性控制:
**
** 问题背景:
** - 共享库中,默认所有符号都是可见的(导出的)
** - 这会增加符号表大小,减慢加载速度
** - 可能导致符号冲突
**
** 解决方案:
** - 使用visibility("internal")隐藏内部符号
** - 只导出公共API
**
** 条件:
** - GCC 3.2+
** - ELF或MACH格式(Linux, macOS等)
*/
#if !defined(LUAI_FUNC)

#if defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 302) && \
    (defined(__ELF__) || defined(__MACH__))
/* GCC特性:设置符号为内部可见 */
#define LUAI_FUNC __attribute__((visibility("internal"))) extern
#else
/* 默认:普通extern */
#define LUAI_FUNC extern
#endif

/*
** LUAI_DDEC - 数据声明
** LUAI_DDEF - 数据定义
**
** 使用LUAI_FUNC标记:
** - 声明使用LUAI_FUNC extern
** - 定义为空(在定义处不需要extern)
*/
#define LUAI_DDEC(dec) LUAI_FUNC dec
#define LUAI_DDEF /* empty */ /* 空 */

#endif

/*
** Give these macros simpler names for internal use
** 为内部使用给这些宏更简单的名字
**
** l_likely/l_unlikely - 分支预测提示
**
** 用途:
** - 告诉编译器哪个分支更可能执行
** - 编译器可以据此优化代码布局
** - 提高CPU流水线效率
**
** luai_likely/luai_unlikely在luaconf.h中定义
** 通常映射到编译器内建函数,如__builtin_expect
**
** 示例:
** if (l_likely(p != NULL)) { ... } // p很可能非空
** if (l_unlikely(error)) { ... }   // 错误很少发生
*/
#define l_likely(x) luai_likely(x)
#define l_unlikely(x) luai_unlikely(x)

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** 消息和错误基本报告的"抽象层"
** ===================================================================
*/

/*
** print a string
** 打印字符串
**
** lua_writestring - 写字符串到输出
** 参数:
** - s: 字符串指针
** - l: 字符串长度
**
** 默认实现:
** - fwrite: C标准库函数
** - sizeof(char): 元素大小(通常是1)
** - stdout: 标准输出
**
** 可定制点:
** - 嵌入式系统可能需要不同的输出方式
** - 如写到串口、LCD等
*/
#if !defined(lua_writestring)
#define lua_writestring(s, l) fwrite((s), sizeof(char), (l), stdout)
#endif

/*
** print a newline and flush the output
** 打印换行符并刷新输出
**
** lua_writeline - 写换行并刷新
**
** 实现:
** 1. lua_writestring("\n", 1): 写入换行符
** 2. fflush(stdout): 刷新stdout缓冲区
**
** 为什么刷新?
** - 确保输出立即可见
** - 在交互式环境中很重要
** - 避免输出被缓冲延迟
*/
#if !defined(lua_writeline)
#define lua_writeline() (lua_writestring("\n", 1), fflush(stdout))
#endif

/*
** print an error message
** 打印错误消息
**
** lua_writestringerror - 写错误字符串
** 参数:
** - s: 格式字符串(printf风格)
** - p: 格式参数
**
** 默认实现:
** - fprintf(stderr, ...): 写到标准错误
** - fflush(stderr): 刷新错误流
**
** 使用stderr而不是stdout:
** - 错误消息应该到错误流
** - 可以独立重定向
** - 通常不缓冲或行缓冲,确保立即输出
**
** 格式化输出:
** - 支持printf风格的格式化
** - 如lua_writestringerror("Error: %s", msg)
*/
#if !defined(lua_writestringerror)
#define lua_writestringerror(s, p) \
  (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */

#endif

/*
** ============================================================================
** 文件总结说明
** ============================================================================
**
** llimits.h是Lua虚拟机的基础设施文件,它定义了:
**
** 一、核心设计思想
**
** 1. 平台抽象
**    - 隔离不同平台、编译器、架构的差异
**    - 通过条件编译适配各种环境
**    - 为上层代码提供统一接口
**
** 2. 类型安全
**    - 明确区分不同用途的类型(lu_byte vs char)
**    - 使用宏使类型转换更显眼
**    - 提供语义化的类型转换函数
**
** 3. 可配置性
**    - 关键操作通过宏定义,可以定制
**    - 支持外部配置(通过LUAI_*宏)
**    - 在性能和可移植性间取得平衡
**
** 二、关键技术点
**
** 1. 内存类型 (l_mem/lu_mem)
**    - 根据平台选择合适的整数类型
**    - 有符号版本支持"债务"机制
**    - 用于垃圾回收器的内存统计
**
** 2. 数值运算抽象 (luai_num*系列)
**    - 统一浮点运算接口
**    - 特别处理模运算和整数除法
**    - 支持平台特定优化
**
** 3. 指针和整数转换
**    - 仅用于哈希,不要求可逆
**    - 使用最合适的整数类型
**    - 实用主义:即使理论上未定义,实践中都能工作
**
** 4. 符号可见性控制
**    - 区分内部和外部符号
**    - 优化共享库性能
**    - 减少符号冲突
**
** 三、C语言高级特性使用
**
** 1. 条件编译
**    - #if defined(...): 检查宏是否定义
**    - __STDC_VERSION__: 检查C标准版本
**    - 平台检测: __GNUC__, _MSC_VER, __ELF__等
**
** 2. 编译器扩展
**    - __attribute__: GCC特性,如noreturn, visibility
**    - __declspec: MSVC特性
**    - __extension__: 抑制GCC警告
**
** 3. 位运算技巧
**    - ispow2: 检测2的幂
**    - ~(size_t)0: 生成全1位模式
**    - << (bits-1): 设置最高位
**
** 4. 宏编程技术
**    - 逗号运算符: (a, b) 顺序求值
**    - 三元运算符: 条件表达式
**    - do-while(0): 多语句宏(虽然本文件未用)
**
** 四、与其他模块的关系
**
** 1. lua.h
**    - 定义公共API类型(lua_Number, lua_Integer等)
**    - llimits.h在此基础上定义内部类型
**
** 2. luaconf.h
**    - 提供可配置的宏定义
**    - 如LUAI_UACNUMBER, luai_likely等
**
** 3. 垃圾回收器
**    - 使用l_mem/lu_mem跟踪内存
**    - 使用MAX_LMEM限制内存使用
**
** 4. 虚拟机核心
**    - 使用类型转换宏
**    - 使用数值运算抽象
**    - 使用断言进行调试
**
** 五、重要概念解释
**
** 1. 补码 (Two's Complement)
**    - 现代计算机表示有符号整数的标准方式
**    - 最高位是符号位
**    - 负数 = 正数按位取反加1
**    - 好处: 加减法统一,没有+0和-0
**
** 2. 类型转换的危险性
**    - 有符号/无符号转换可能改变值的解释
**    - 指针/整数转换在严格C中未定义
**    - 但实践中几乎总能正常工作
**
** 3. 内联和链接
**    - static inline: 每个编译单元独立副本
**    - extern: 跨编译单元共享
**    - visibility: 控制共享库导出
**
** 4. 浮点数特性
**    - NaN: 不等于任何值,包括自己
**    - floor vs trunc: 向下取整 vs 向0取整
**    - 精度限制: 大整数可能无法精确表示
**
** 六、阅读建议
**
** 1. 先理解基本类型定义(l_mem, lu_byte等)
** 2. 再看类型转换宏(cast系列)
** 3. 然后理解数值运算抽象
** 4. 最后关注平台相关的条件编译
**
** 5. 对于不熟悉的C特性:
**    - ptrdiff_t: 指针差值类型
**    - size_t: sizeof返回类型
**    - stdint.h: C99标准整数类型
**    - __attribute__: 编译器特定扩展
**
** 6. 关键学习点:
**    - 如何编写可移植的C代码
**    - 如何使用宏进行抽象
**    - 如何处理平台差异
**    - 类型安全和类型转换的权衡
**
** 七、实用价值
**
** 1. 学习系统编程
**    - 了解不同平台的差异
**    - 学习条件编译技巧
**    - 理解类型系统
**
** 2. 学习库设计
**    - 如何设计可配置的接口
**    - 如何隔离实现细节
**    - 如何平衡性能和可移植性
**
** 3. 调试和维护
**    - 理解断言的作用
**    - 学习如何使用编译器特性
**    - 了解符号可见性的重要性
**
** ============================================================================
*/