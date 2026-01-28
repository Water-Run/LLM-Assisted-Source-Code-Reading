/*
================================================================================
文件概要：lstate.h - Lua 全局状态管理
================================================================================

【文件基本信息】
- 文件名：lstate.h
- 作用：定义 Lua 虚拟机的全局状态结构和线程状态结构
- 版本：Lua 5.4+
- 版权：参见 lua.h 中的版权声明

【核心功能】
本文件是 Lua 虚拟机状态管理的核心头文件，定义了：
1. lua_State 结构：表示单个 Lua 线程（协程）的执行状态
2. global_State 结构：表示所有线程共享的全局状态
3. CallInfo 结构：表示函数调用栈信息
4. 垃圾回收相关的数据结构和宏定义
5. 字符串缓存机制
6. 各种状态标志位的定义

【重要概念】
1. Lua 的"线程"实际上是协程（coroutine），与操作系统线程不同
2. 每个 lua_State 代表一个独立的执行上下文，有自己的调用栈
3. 所有 lua_State 共享一个 global_State，包含垃圾回收器、字符串表等
4. 垃圾回收器支持增量式和分代式两种模式

【文件依赖】
- lua.h：Lua 的公共 API
- lobject.h：Lua 对象定义
- ltm.h：标签方法（元方法）定义
- lzio.h：输入流抽象

================================================================================
*/

/*
** $Id: lstate.h $
** Global State
** 全局状态
** See Copyright Notice in lua.h
** 参见 lua.h 中的版权声明
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

/*
** 这里包含的一些头文件需要这个定义
** 前向声明 CallInfo 结构体，因为下面的头文件可能需要引用它
** CallInfo 代表函数调用的信息，包含了函数执行的上下文
*/
/* Some header files included here need this definition */
typedef struct CallInfo CallInfo;

#include "lobject.h" /* Lua 对象的基本定义 */
#include "ltm.h"     /* 标签方法（元方法）的定义 */
#include "lzio.h"    /* 输入流抽象层 */

/*
================================================================================
垃圾回收对象的组织方式说明
================================================================================

【核心原则】
Lua 中的所有对象在被释放之前必须保持可访问，因此所有对象总是属于以下
某一个链表（且只属于一个），这些链表通过 'CommonHeader' 中的 'next' 字段连接。

【主要链表】
1. 'allgc'：所有未标记为需要终结化的对象
2. 'finobj'：所有标记为需要终结化的对象
3. 'tobefnz'：所有准备好被终结化的对象
4. 'fixedgc'：所有不应被回收的对象（当前只有小字符串，如保留字）

【分代回收的代际标记】
对于分代式垃圾回收器，某些链表有代际标记。每个标记指向该代的第一个元素，
该代一直延续到下一个标记。

allgc 链表的代际划分：
- 'allgc' -> 'survival'：新对象（新生代）
- 'survival' -> 'old'：存活了一次回收的对象
- 'old1' -> 'reallyold'：在上次回收中变老的对象
- 'reallyold' -> NULL：已经老化超过一个周期的对象

finobj 链表的代际划分（带终结器的对象）：
- 'finobj' -> 'finobjsur'：标记为需要终结化的新对象
- 'finobjsur' -> 'finobjold1'：存活的终结化对象
- 'finobjold1' -> 'finobjrold'：刚变老的终结化对象
- 'finobjrold' -> NULL：真正老的终结化对象

【特殊情况】
由于 'luaC_checkfinalizer' 和 'udata2finalize' 会在普通链表和"标记为终结化"
链表之间移动对象，所有链表都可能包含比其主要年龄更老的元素。此外，写屏障
可以将年轻链表中的年轻对象标记为 OLD0，然后变成 OLD1。但是，链表永远不会
包含比其主要年龄更年轻的元素。

【优化指针 firstold1】
分代回收器还使用一个指针 'firstold1'，它指向链表中第一个 OLD1 对象。
用于优化 'markold' 操作。注意它与 'old1' 的区别：
- 'firstold1'：此点之前没有 OLD1 对象；此点之后可以有所有年龄的对象
- 'old1'：此点之后没有比 OLD1 更年轻的对象

【说明】
- OLD0/OLD1/OLD2 是分代 GC 中的对象年龄标记
- "年轻"和"年老"是相对概念，表示对象经历 GC 周期的次数
- 写屏障（write barrier）是用于维护 GC 不变式的机制
*/

/*
** Some notes about garbage-collected objects: All objects in Lua must
** be kept somehow accessible until being freed, so all objects always
** belong to one (and only one) of these lists, using field 'next' of
** the 'CommonHeader' for the link:
**
** 'allgc': all objects not marked for finalization;
** 'finobj': all objects marked for finalization;
** 'tobefnz': all objects ready to be finalized;
** 'fixedgc': all objects that are not to be collected (currently
** only small strings, such as reserved words).
**
** For the generational collector, some of these lists have marks for
** generations. Each mark points to the first element in the list for
** that particular generation; that generation goes until the next mark.
**
** 'allgc' -> 'survival': new objects;
** 'survival' -> 'old': objects that survived one collection;
** 'old1' -> 'reallyold': objects that became old in last collection;
** 'reallyold' -> NULL: objects old for more than one cycle.
**
** 'finobj' -> 'finobjsur': new objects marked for finalization;
** 'finobjsur' -> 'finobjold1': survived   """";
** 'finobjold1' -> 'finobjrold': just old  """";
** 'finobjrold' -> NULL: really old       """".
**
** All lists can contain elements older than their main ages, due
** to 'luaC_checkfinalizer' and 'udata2finalize', which move
** objects between the normal lists and the "marked for finalization"
** lists. Moreover, barriers can age young objects in young lists as
** OLD0, which then become OLD1. However, a list never contains
** elements younger than their main ages.
**
** The generational collector also uses a pointer 'firstold1', which
** points to the first OLD1 object in the list. It is used to optimize
** 'markold'. (Potentially OLD1 objects can be anywhere between 'allgc'
** and 'reallyold', but often the list has no OLD1 objects or they are
** after 'old1'.) Note the difference between it and 'old1':
** 'firstold1': no OLD1 objects before this point; there can be all
**   ages after it.
** 'old1': no objects younger than OLD1 after this point.
*/

/*
================================================================================
灰色对象链表说明
================================================================================

【灰色对象的概念】
在三色标记算法中：
- 白色：未访问的对象（可能是垃圾）
- 灰色：已访问但其引用的对象尚未全部访问
- 黑色：已访问且其引用的对象也已全部访问

【灰色对象链表】
这些链表通过 'gclist' 字段连接（所有可能变灰的对象都有这个字段，
虽然在不同对象中位置可能不同，但字段名总是 'gclist'）。

任何灰色对象必须属于以下某个链表，且这些链表中的所有对象必须是灰色的
（有两个例外，见下文）：

1. 'gray'：常规的灰色对象，等待被访问
2. 'grayagain'：必须在原子阶段重新访问的对象，包括：
   - 触发写屏障的黑色对象
   - 传播阶段的各种弱表
   - 所有线程
3. 'weak'：具有弱值的表，需要被清理
4. 'ephemeron'：短生命周期表（弱键），有白色->白色的条目
5. 'allweak'：同时具有弱键和/或弱值的表，需要被清理

【"灰色规则"的两个例外】
1. 分代模式下的 TOUCHED2 对象：
   - 留在灰色链表中（因为在周期结束时必须再次访问）
   - 但被标记为黑色，因为对它们的赋值必须触发屏障（以将它们移回 TOUCHED1）

2. 开放的 upvalue：
   - 保持灰色以避免屏障
   - 但不在灰色链表中（它们甚至没有 'gclist' 字段）

【技术说明】
- 弱表（weak table）：键或值可以被 GC 回收的表
- 短生命周期表（ephemeron table）：特殊的弱表，键的可达性影响值的可达性
- 写屏障（write barrier）：当修改黑色对象使其引用白色对象时触发的机制
- 原子阶段（atomic phase）：GC 的最后阶段，不可被打断
*/

/*
** Moreover, there is another set of lists that control gray objects.
** These lists are linked by fields 'gclist'. (All objects that
** can become gray have such a field. The field is not the same
** in all objects, but it always has this name.)  Any gray object
** must belong to one of these lists, and all objects in these lists
** must be gray (with two exceptions explained below):
**
** 'gray': regular gray objects, still waiting to be visited.
** 'grayagain': objects that must be revisited at the atomic phase.
**   That includes
**   - black objects got in a write barrier;
**   - all kinds of weak tables during propagation phase;
**   - all threads.
** 'weak': tables with weak values to be cleared;
** 'ephemeron': ephemeron tables with white->white entries;
** 'allweak': tables with weak keys and/or weak values to be cleared.
**
** The exceptions to that "gray rule" are:
** - TOUCHED2 objects in generational mode stay in a gray list (because
** they must be visited again at the end of the cycle), but they are
** marked black because assignments to them must activate barriers (to
** move them back to TOUCHED1).
** - Open upvalues are kept gray to avoid barriers, but they stay out
** of gray lists. (They don't even have a 'gclist' field.)
*/

/*
================================================================================
关于 nCcalls 字段的说明
================================================================================

【字段结构】
nCcalls 是一个 32 位整数，分为两部分：
- 低 16 位：C 栈中递归调用的次数
- 高 16 位：栈中不可让步（non-yieldable）调用的次数

【设计原因】
将两个计数器放在一起，可以用一条指令同时修改和保存两者。
这在性能敏感的代码中很重要。

【不可让步调用】
某些 C 函数调用期间不允许协程让步（yield），例如：
- 元方法（metamethods）执行期间
- 某些内部 API 调用期间
这是为了保证某些操作的原子性和一致性。

【相关宏定义】
- yieldable(L)：检查线程是否可以让步
- getCcalls(L)：获取真实的 C 调用次数
- incnny(L)：增加不可让步调用计数
- decnny(L)：减少不可让步调用计数
- nyci：不可让步调用的增量值
*/

/*
** About 'nCcalls':  This count has two parts: the lower 16 bits counts
** the number of recursive invocations in the C stack; the higher
** 16 bits counts the number of non-yieldable calls in the stack.
** (They are together so that we can change and save both with one
** instruction.)
*/

/*
** 如果此线程在栈中没有不可让步的调用，则返回真
** 原理：高 16 位为 0 表示没有不可让步调用
** 0xffff0000 是高 16 位的掩码
*/
/* true if this thread does not have non-yieldable calls in the stack */
#define yieldable(L) (((L)->nCcalls & 0xffff0000) == 0)

/*
** 获取真实的 C 调用次数
** 原理：只取低 16 位
** 0xffff 是低 16 位的掩码
*/
/* real number of C calls */
#define getCcalls(L) ((L)->nCcalls & 0xffff)

/*
** 增加不可让步调用的数量
** 原理：给高 16 位加 1，即加 0x10000
** 这不影响低 16 位的 C 调用计数
*/
/* Increment the number of non-yieldable calls */
#define incnny(L) ((L)->nCcalls += 0x10000)

/*
** 减少不可让步调用的数量
** 原理：给高 16 位减 1
*/
/* Decrement the number of non-yieldable calls */
#define decnny(L) ((L)->nCcalls -= 0x10000)

/*
** 不可让步调用的增量
** 原理：0x10000 增加高 16 位（不可让步计数），| 1 增加低 16 位（C 调用计数）
** 用于同时增加两个计数器
*/
/* Non-yieldable call increment */
#define nyci (0x10000 | 1)

/*
** lua_longjmp 结构体的前向声明
** 在 ldo.c 中定义，用于错误处理中的非局部跳转
** longjmp 是 C 标准库函数，用于实现类似异常处理的机制
*/
struct lua_longjmp; /* defined in ldo.c */

/*
** 原子类型（相对于信号）以更好地确保 'lua_sethook' 是线程安全的
**
** 【技术说明】
** sig_atomic_t 是 C 标准库定义的类型（在 signal.h 中）
** 这个类型保证：
** 1. 读写操作是原子的（不可分割的）
** 2. 即使在信号处理函数中访问也是安全的
**
** Lua 使用它来存储 hook 相关的标志，因为这些标志可能在：
** - 主线程中被修改
** - 信号处理函数中被检查
** 使用原子类型可以避免数据竞争
*/
/*
** Atomic type (relative to signals) to better ensure that 'lua_sethook'
** is thread safe
*/
#if !defined(l_signalT)
#include <signal.h>
#define l_signalT sig_atomic_t
#endif

/*
** 额外的栈空间，用于处理标签方法（TM）调用和其他额外操作
**
** 【重要说明】
** - 这个空间不包含在 'stack_last' 中
** - 只用于避免栈检查，因为：
**   1. 元素会被迅速弹出，或
**   2. 压栈后很快就会进行栈检查
** - 函数栈帧从不使用这个额外空间，所以不需要保持干净
**
** 【为什么需要额外空间】
** 在某些操作中（如调用元方法），可能需要临时压入一些值到栈上。
** 如果每次都检查栈空间是否足够会影响性能。有了这个额外空间，
** 可以安全地进行少量压栈操作而不需要检查。
*/
/*
** Extra stack space to handle TM calls and some other extras. This
** space is not included in 'stack_last'. It is used only to avoid stack
** checks, either because the element will be promptly popped or because
** there will be a stack check soon after the push. Function frames
** never use this extra space, so it does not need to be kept clean.
*/
#define EXTRA_STACK 5

/*
** API 中字符串缓存的大小
**
** 【缓存结构】
** N：集合数量（最好是质数，减少哈希冲突）
** M：每个集合的大小
**
** 【说明】
** - M == 1 时，变成直接映射缓存（direct-mapped cache）
** - 这是一个 N-way set-associative cache
** - 用于缓存 C API 中频繁使用的字符串，提高性能
**
** 【技术背景】
** 在 Lua C API 中，经常需要创建字符串（如键名）。
** 通过缓存最近使用的字符串，可以避免重复的字符串创建和哈希计算。
*/
/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set.
** (M == 1 makes a direct cache.)
*/
#if !defined(STRCACHE_N)
#define STRCACHE_N 53
#define STRCACHE_M 2
#endif

/*
** 基本栈大小
** LUA_MINSTACK 是 lua.h 中定义的最小栈大小（通常是 20）
** 基本栈大小是最小栈的两倍，用于初始化新线程的栈
*/
#define BASIC_STACK_SIZE (2 * LUA_MINSTACK)

/*
** 计算栈大小（以栈槽为单位）
** stack_last.p 指向栈的末尾（最后一个元素+1）
** stack.p 指向栈的起始
** cast_int 将指针差值转换为整数
**
** 【C语言知识】
** 两个指针相减得到它们之间的元素个数（不是字节数）
** 这是 C 语言的指针运算规则
*/
#define stacksize(th) cast_int((th)->stack_last.p - (th)->stack.p)

/*
** 垃圾回收的类型
**
** 【三种模式】
** KGC_INC：增量式 GC（incremental GC）
**   - 将 GC 工作分散到多个步骤
**   - 避免长时间暂停
**   - 适合交互式应用
**
** KGC_GENMINOR：分代式 GC 的次要（常规）模式
**   - 只回收年轻对象
**   - 基于大多数对象"朝生夕死"的假设
**   - 速度快，但可能遗漏一些垃圾
**
** KGC_GENMAJOR：分代式 GC 的主要模式
**   - 回收所有代的对象
**   - 彻底但较慢
**   - 定期执行以清理老对象
*/
/* kinds of Garbage Collection */
#define KGC_INC 0      /* incremental gc */
#define KGC_GENMINOR 1 /* generational gc in minor (regular) mode */
#define KGC_GENMAJOR 2 /* generational in major mode */

/*
** 字符串表结构
**
** 【功能】
** Lua 的字符串内部化（interning）机制的核心数据结构
** 所有相同内容的字符串在内存中只保存一份
**
** 【字段说明】
** hash：桶数组，每个桶是一个 TString 链表（处理哈希冲突）
** nuse：当前表中的字符串数量
** size：桶的数量（哈希表的容量）
**
** 【技术细节】
** - 使用链地址法（chaining）处理哈希冲突
** - 当 nuse/size 比率过高时，会进行扩容（rehash）
** - 所有字符串对象的指针可以直接比较（因为内容相同的字符串是同一个对象）
*/
typedef struct stringtable
{
  TString **hash; /* array of buckets (linked lists of strings) */
  int nuse;       /* number of elements */
  int size;       /* number of buckets */
} stringtable;

/*
================================================================================
CallInfo 结构体 - 函数调用信息
================================================================================

【核心作用】
表示一个函数调用的上下文信息，Lua 通过 CallInfo 链表管理调用栈。

【关于联合体 'u'】
使用联合体节省内存，因为 Lua 函数和 C 函数的信息互斥：
- 字段 'l'：仅用于 Lua 函数
- 字段 'c'：仅用于 C 函数

【关于联合体 'u2'】
- funcidx：仅在 C 函数进行保护调用时使用（记录函数在栈中的索引）
- nyield：仅在函数"正在"让步时使用（从 yield 到下次 resume）
- nres：仅在返回时关闭 tbc 变量时使用（记录返回值数量）

【tbc 变量说明】
tbc = to-be-closed，是 Lua 5.4 引入的特性
用 <close> 或 <const> 标记的局部变量会在离开作用域时自动调用其 __close 元方法
*/
/*
** Information about a call.
** About union 'u':
** - field 'l' is used only for Lua functions;
** - field 'c' is used only for C functions.
** About union 'u2':
** - field 'funcidx' is used only by C functions while doing a
** protected call;
** - field 'nyield' is used only while a function is "doing" an
** yield (from the yield until the next resume);
** - field 'nres' is used only while closing tbc variables when
** returning from a function;
*/
struct CallInfo
{
  StkIdRel func;                    /* 栈中的函数索引（相对栈底的偏移） */
  StkIdRel top;                     /* 此函数的栈顶（该函数可用的栈空间上限） */
  struct CallInfo *previous, *next; /* 动态调用链表（双向链表） */
  union
  {
    struct
    {                             /* 仅用于 Lua 函数 */
      const Instruction *savedpc; /* 保存的程序计数器（指向下一条要执行的指令） */
      volatile l_signalT trap;    /* 函数是否在追踪行/计数（用于调试钩子） */
      int nextraargs;             /* 可变参数函数中的额外参数数量 */
    } l;
    struct
    {                        /* 仅用于 C 函数 */
      lua_KFunction k;       /* 让步时的延续函数（continuation function） */
      ptrdiff_t old_errfunc; /* 旧的错误处理函数（栈索引） */
      lua_KContext ctx;      /* 让步时的上下文信息 */
    } c;
  } u;
  union
  {
    int funcidx; /* 被调用函数的索引 */
    int nyield;  /* 让步的值的数量 */
    int nres;    /* 返回的值的数量 */
  } u2;
  l_uint32 callstatus; /* 调用状态标志（位域，见下面的定义） */
};

/*
** 函数可返回的最大期望结果数
** 必须能放入 CIST_NRESULTS 中（8 位，最大 255）
**
** 【说明】
** 这个值限制了一个函数调用时"期望"返回的最大值数量。
** 注意这不是实际返回值的限制（实际可以返回更多），
** 而是在调用时指定"我需要多少个返回值"的最大值。
*/
/*
** Maximum expected number of results from a function
** (must fit in CIST_NRESULTS).
*/
#define MAXRESULTS 250

/*
================================================================================
CallInfo 状态位定义
================================================================================

callstatus 是一个 32 位整数，不同的位表示不同的状态：

【位 0-7：CIST_NRESULTS】
期望的返回值数量 + 1（0-255）
- 0 表示期望 -1 个结果（即 LUA_MULTRET，要所有结果）
- 1 表示期望 0 个结果
- 2 表示期望 1 个结果，以此类推

【位 8-11：CIST_CCMT】
调用元方法的计数（包括额外参数）
用于跟踪嵌套的元方法调用深度，防止无限递归

【位 12-14：CIST_RECST】
恢复状态（recover status）
用于在协程中记录错误状态，以便在 __close 元方法让步后正确恢复

【位 15：CIST_C】
调用正在运行 C 函数

【位 16：CIST_FRESH】
调用在一个新的 "luaV_execute" 帧上
luaV_execute 是 Lua 虚拟机的主执行循环

【位 17：CIST_CLSRET】
函数正在关闭 tbc 变量

【位 18：CIST_TBC】
函数有 tbc 变量需要关闭

【位 19：CIST_OAH】
'allowhook' 的原始值
allowhook 控制是否允许调试钩子

【位 20：CIST_HOOKED】
调用正在运行调试钩子

【位 21：CIST_YPCALL】
正在进行可让步的保护调用

【位 22：CIST_TAIL】
调用是尾调用（tail call）
尾调用优化可以避免栈溢出

【位 23：CIST_HOOKYIELD】
最后一个钩子调用让步了

【位 24：CIST_FIN】
函数"调用"了一个终结器（finalizer）
*/
/*
** Bits in CallInfo status
*/
/* bits 0-7 are the expected number of results from this function + 1 */
#define CIST_NRESULTS 0xffu

/* bits 8-11 count call metamethods (and their extra arguments) */
#define CIST_CCMT 8 /* the offset, not the mask */
#define MAX_CCMT (0xfu << CIST_CCMT)

/* Bits 12-14 are used for CIST_RECST (see below) */
#define CIST_RECST 12 /* the offset, not the mask */

/* call is running a C function (still in first 16 bits) */
#define CIST_C (1u << (CIST_RECST + 3))
/* call is on a fresh "luaV_execute" frame */
#define CIST_FRESH (cast(l_uint32, CIST_C) << 1)
/* function is closing tbc variables */
#define CIST_CLSRET (CIST_FRESH << 1)
/* function has tbc variables to close */
#define CIST_TBC (CIST_CLSRET << 1)
/* original value of 'allowhook' */
#define CIST_OAH (CIST_TBC << 1)
/* call is running a debug hook */
#define CIST_HOOKED (CIST_OAH << 1)
/* doing a yieldable protected call */
#define CIST_YPCALL (CIST_HOOKED << 1)
/* call was tail called */
#define CIST_TAIL (CIST_YPCALL << 1)
/* last hook called yielded */
#define CIST_HOOKYIELD (CIST_TAIL << 1)
/* function "called" a finalizer */
#define CIST_FIN (CIST_HOOKYIELD << 1)

/*
** 从 callstatus 获取期望的返回值数量
** 减 1 是因为存储时加了 1
**
** 【为什么加 1 再减 1】
** 这样 0 可以表示 -1（LUA_MULTRET），1 表示 0 个结果，2 表示 1 个结果...
*/
#define get_nresults(cs) (cast_int((cs) & CIST_NRESULTS) - 1)

/*
** 字段 CIST_RECST 存储"恢复状态"
**
** 【用途】
** 在协程中关闭 tbc 变量时保持错误状态，使得 Lua 可以在因错误而调用的
** __close 元方法让步后正确恢复。
**
** 【技术说明】
** 三位足够存储错误状态（0-7）
** 通过位操作提取和设置这三位
*/
/*
** Field CIST_RECST stores the "recover status", used to keep the error
** status while closing to-be-closed variables in coroutines, so that
** Lua can correctly resume after an yield from a __close method called
** because of an error.  (Three bits are enough for error status.)
*/
#define getcistrecst(ci) (((ci)->callstatus >> CIST_RECST) & 7)
#define setcistrecst(ci, st)                                        \
  check_exp(((st) & 7) == (st), /* status must fit in three bits */ \
            ((ci)->callstatus = ((ci)->callstatus & ~(7u << CIST_RECST)) | (cast(l_uint32, st) << CIST_RECST)))

/*
** 检查活动函数是否是 Lua 函数
** 如果 CIST_C 位未设置，则是 Lua 函数
*/
/* active function is a Lua function */
#define isLua(ci) (!((ci)->callstatus & CIST_C))

/*
** 检查调用是否在运行 Lua 代码（不是钩子）
** 既不是 C 函数也不是钩子
*/
/* call is running Lua code (not a hook) */
#define isLuacode(ci) (!((ci)->callstatus & (CIST_C | CIST_HOOKED)))

/*
** 设置或清除 CIST_OAH 位（保存 allowhook 的原始值）
** v 为真时设置，为假时清除
*/
#define setoah(ci, v)                                    \
  ((ci)->callstatus = ((v) ? (ci)->callstatus | CIST_OAH \
                           : (ci)->callstatus & ~CIST_OAH))
/*
** 获取 CIST_OAH 位的值
** 返回 1 或 0
*/
#define getoah(ci) (((ci)->callstatus & CIST_OAH) ? 1 : 0)

/*
================================================================================
lua_State 结构体 - Lua 线程（协程）状态
================================================================================

【核心概念】
lua_State 代表一个 Lua 执行线程（协程）。每个 lua_State 都有：
- 独立的调用栈
- 独立的数据栈
- 独立的错误处理
但共享同一个 global_State（全局状态）

【重要说明】
Lua 的"线程"实际上是协程，不是操作系统级别的线程。
所有协程运行在同一个操作系统线程中，通过协作式多任务实现并发。

【字段详解】见下面的注释
*/
/*
** 'per thread' state
** 每个线程的状态
*/
struct lua_State
{
  CommonHeader; /* GC 相关的公共头部（next、tt、marked 等字段） */

  lu_byte allowhook; /* 是否允许调试钩子（hooks）执行 */

  TStatus status; /* 线程状态：正常、挂起、错误等 */

  StkIdRel top; /* 栈中第一个空闲槽位（栈顶） */

  struct global_State *l_G; /* 指向全局状态（所有线程共享） */

  CallInfo *ci; /* 当前函数的调用信息 */

  StkIdRel stack_last; /* 栈的末尾（最后一个元素 + 1） */

  StkIdRel stack; /* 栈基址 */

  UpVal *openupval; /* 此栈中的开放 upvalue 链表 */

  StkIdRel tbclist; /* 待关闭变量（to-be-closed）的链表 */

  GCObject *gclist; /* GC 相关的对象链表 */

  struct lua_State *twups; /* 有开放 upvalue 的线程链表 */

  struct lua_longjmp *errorJmp; /* 当前的错误恢复点（用于 longjmp） */

  CallInfo base_ci; /* 第一层的 CallInfo（C 宿主） */

  volatile lua_Hook hook; /* 调试钩子函数 */

  ptrdiff_t errfunc; /* 当前错误处理函数（栈索引） */

  l_uint32 nCcalls; /* 嵌套的不可让步或 C 调用的数量 */

  int oldpc; /* 最后追踪的 pc（程序计数器） */

  int nci; /* 'ci' 链表中的项数 */

  int basehookcount; /* 基本钩子计数 */

  int hookcount; /* 钩子计数（用于计数钩子） */

  volatile l_signalT hookmask; /* 钩子掩码（指定哪些事件触发钩子） */

  struct
  {                /* 关于传输值的信息（用于调用/返回钩子） */
    int ftransfer; /* 第一个传输值的偏移 */
    int ntransfer; /* 传输的值的数量 */
  } transferinfo;
};

/*
** 线程状态 + 额外空间
**
** 【说明】
** LX 结构体在 lua_State 前面预留了 LUA_EXTRASPACE 字节的额外空间
** 这个空间可以被嵌入 Lua 的应用程序用来存储自定义数据
**
** 【C 语言知识】
** 在 C 中，结构体成员按声明顺序在内存中排列
** extra_ 在前，l 在后，所以可以通过指向 l 的指针减去偏移来访问 extra_
*/
/*
** thread state + extra space
*/
typedef struct LX
{
  lu_byte extra_[LUA_EXTRASPACE]; /* 额外空间，供嵌入应用使用 */
  lua_State l;                    /* Lua 线程状态 */
} LX;

/*
================================================================================
global_State 结构体 - 全局状态
================================================================================

【核心概念】
global_State 包含所有 Lua 线程共享的数据，包括：
- 内存分配器
- 垃圾回收器状态
- 字符串表
- 全局注册表
- 元表
- 等等

【重要性】
一个 Lua 状态机（lua_State）中的所有协程共享同一个 global_State
这确保了资源的统一管理和数据的一致性

【字段详解】见下面的注释
*/
/*
** 'global state', shared by all threads of this state
** 全局状态，被此状态的所有线程共享
*/
typedef struct global_State
{
  lua_Alloc frealloc; /* 内存重分配函数（malloc/realloc/free 的封装） */

  void *ud; /* 'frealloc' 的辅助数据（用户数据指针） */

  l_mem GCtotalbytes; /* 当前分配的字节数 + 债务 */

  l_mem GCdebt; /* 已计数但尚未分配的字节（可以为负） */

  l_mem GCmarked; /* GC 周期中标记的对象数量 */

  l_mem GCmajorminor; /* 辅助计数器，用于控制主次回收切换 */

  stringtable strt; /* 字符串哈希表（字符串内部化） */

  TValue l_registry; /* 全局注册表 */

  TValue nilvalue; /* 一个 nil 值（用于优化，避免重复创建） */

  unsigned int seed; /* 哈希的随机种子（防止哈希攻击） */

  lu_byte gcparams[LUA_GCPN]; /* GC 参数数组 */

  lu_byte currentwhite; /* 当前的白色（GC 使用两种白色交替） */

  lu_byte gcstate; /* 垃圾回收器的状态 */

  lu_byte gckind; /* 正在运行的 GC 类型（增量式/分代式） */

  lu_byte gcstopem; /* 停止紧急回收 */

  lu_byte gcstp; /* 控制 GC 是否运行 */

  lu_byte gcemergency; /* 如果这是紧急回收则为真 */

  GCObject *allgc; /* 所有可回收对象的链表 */

  GCObject **sweepgc; /* 清扫在链表中的当前位置 */

  GCObject *finobj; /* 有终结器的可回收对象链表 */

  GCObject *gray; /* 灰色对象链表 */

  GCObject *grayagain; /* 需要原子阶段重新遍历的对象链表 */

  GCObject *weak; /* 弱值表链表 */

  GCObject *ephemeron; /* 短生命周期表链表（弱键） */

  GCObject *allweak; /* 全弱表链表 */

  GCObject *tobefnz; /* 待进行 GC 的用户数据链表 */

  GCObject *fixedgc; /* 不应被回收的对象链表 */

  /* 分代回收器的字段 */
  GCObject *survival; /* 存活了一次 GC 周期的对象起始位置 */

  GCObject *old1; /* old1 对象的起始位置 */

  GCObject *reallyold; /* 超过一个周期的老对象（"真正老的"） */

  GCObject *firstold1; /* 链表中第一个 OLD1 对象（如果有） */

  GCObject *finobjsur; /* 存活的带终结器的对象链表 */

  GCObject *finobjold1; /* old1 的带终结器的对象链表 */

  GCObject *finobjrold; /* 真正老的带终结器的对象链表 */

  struct lua_State *twups; /* 有开放 upvalue 的线程链表 */

  lua_CFunction panic; /* 在无保护错误中调用（致命错误处理） */

  TString *memerrmsg; /* 内存分配错误的消息 */

  TString *tmname[TM_N]; /* 标签方法名称数组（元方法名） */

  struct Table *mt[LUA_NUMTYPES]; /* 基本类型的元表 */

  TString *strcache[STRCACHE_N][STRCACHE_M]; /* API 中字符串的缓存 */

  lua_WarnFunction warnf; /* 警告函数 */

  void *ud_warn; /* 'warnf' 的辅助数据 */

  LX mainth; /* 此状态的主线程 */
} global_State;

/*
** 从 lua_State 获取 global_State
** L->l_G 是指向全局状态的指针
*/
#define G(L) (L->l_G)

/*
** 从 global_State 获取主线程
** G->mainth.l 是主线程的 lua_State
*/
#define mainthread(G) (&(G)->mainth.l)

/*
** 检查状态是否完全构建
**
** 【判断依据】
** 'g->nilvalue' 是一个 nil 值，说明状态已完全构建
** 在状态初始化时，最后才设置这个值
** 所以可以用它来判断状态是否初始化完成
*/
/*
** 'g->nilvalue' being a nil value flags that the state was completely
** build.
*/
#define completestate(g) ttisnil(&g->nilvalue)

/*
================================================================================
GCUnion - 所有可回收对象的联合体
================================================================================

【用途】
仅用于类型转换，不实际分配这个联合体的实例

【C 语言知识 - 联合体与类型转换】
根据 ISO C99 标准 6.5.2.3 p.5：
"如果一个联合体包含几个共享公共初始序列的结构体，
并且联合体对象当前包含这些结构体之一，
则允许在声明了联合体完整类型的任何地方检查它们中任何一个的公共初始部分。"

【说明】
所有 Lua 的可回收对象都以 CommonHeader 开头
通过这个联合体，可以安全地在不同对象类型之间转换指针
这是一种在 C 中实现多态的技巧
*/
/*
** Union of all collectable objects (only for conversions)
** ISO C99, 6.5.2.3 p.5:
** "if a union contains several structures that share a common initial
** sequence [...], and if the union object currently contains one
** of these structures, it is permitted to inspect the common initial
** part of any of them anywhere that a declaration of the complete type
** of the union is visible."
*/
union GCUnion
{
  GCObject gc;         /* 公共头部 */
  struct TString ts;   /* 字符串 */
  struct Udata u;      /* 用户数据 */
  union Closure cl;    /* 闭包（函数） */
  struct Table h;      /* 表 */
  struct Proto p;      /* 函数原型 */
  struct lua_State th; /* 线程（协程） */
  struct UpVal upv;    /* upvalue */
};

/*
================================================================================
类型转换宏
================================================================================

【原理】
根据 ISO C99 标准 6.7.2.1 p.14：
"指向联合体对象的指针，经过适当转换后，指向其每个成员，反之亦然。"

【用途】
这些宏用于在 GCObject 指针和具体对象类型指针之间转换
check_exp 用于在调试模式下进行类型检查

【安全性】
所有这些转换都基于对象的 tt（type tag）字段
确保只在正确的类型上进行转换
*/
/*
** ISO C99, 6.7.2.1 p.14:
** "A pointer to a union object, suitably converted, points to each of
** its members [...], and vice versa."
*/
#define cast_u(o) cast(union GCUnion *, (o))

/* 将 GCObject 转换为特定值的宏 */
/* macros to convert a GCObject into a specific value */

/* GCObject -> TString（字符串） */
#define gco2ts(o) \
  check_exp(novariant((o)->tt) == LUA_TSTRING, &((cast_u(o))->ts))

/* GCObject -> Udata（用户数据） */
#define gco2u(o) check_exp((o)->tt == LUA_VUSERDATA, &((cast_u(o))->u))

/* GCObject -> Lua Closure（Lua 闭包） */
#define gco2lcl(o) check_exp((o)->tt == LUA_VLCL, &((cast_u(o))->cl.l))

/* GCObject -> C Closure（C 闭包） */
#define gco2ccl(o) check_exp((o)->tt == LUA_VCCL, &((cast_u(o))->cl.c))

/* GCObject -> Closure（任意闭包） */
#define gco2cl(o) \
  check_exp(novariant((o)->tt) == LUA_TFUNCTION, &((cast_u(o))->cl))

/* GCObject -> Table（表） */
#define gco2t(o) check_exp((o)->tt == LUA_VTABLE, &((cast_u(o))->h))

/* GCObject -> Proto（函数原型） */
#define gco2p(o) check_exp((o)->tt == LUA_VPROTO, &((cast_u(o))->p))

/* GCObject -> lua_State（线程） */
#define gco2th(o) check_exp((o)->tt == LUA_VTHREAD, &((cast_u(o))->th))

/* GCObject -> UpVal（upvalue） */
#define gco2upv(o) check_exp((o)->tt == LUA_VUPVAL, &((cast_u(o))->upv))

/*
** 将 Lua 对象转换为 GCObject
**
** 【用途】
** 与上面的宏相反，这个宏将具体的 Lua 对象转换为 GCObject 指针
**
** 【条件检查】
** novariant((v)->tt) >= LUA_TSTRING
** 确保对象是可回收的类型（字符串、表、函数等）
** 不可回收的类型（如数字、布尔）不能使用这个宏
*/
/*
** macro to convert a Lua object into a GCObject
*/
#define obj2gco(v) \
  check_exp(novariant((v)->tt) >= LUA_TSTRING, &(cast_u(v)->gc))

/*
** 实际分配的总内存量
**
** 【计算方式】
** GCtotalbytes：总字节数（包括已分配和预计将分配的）
** GCdebt：债务（可以为负，表示有多余的"信用"）
**
** 【GC 债务机制】
** Lua 使用债务机制来控制 GC 的触发：
** - 每次分配内存时增加债务
** - 当债务超过阈值时触发 GC
** - GC 执行后减少债务
** 这种机制允许细粒度地控制 GC 频率
*/
/* actual number of total memory allocated */
#define gettotalbytes(g) ((g)->GCtotalbytes - (g)->GCdebt)

/*
================================================================================
函数声明
================================================================================

【LUAI_FUNC 宏】
这是一个平台相关的宏，用于声明 Lua 内部 API 函数
通常定义为 extern 或添加特定的函数属性（如 __declspec(dllexport)）

【函数分类】
这些函数主要涉及：
1. 内存管理（debt、thread）
2. 调用栈管理（CallInfo、C stack）
3. 错误处理（warning、error、reset）

【函数详解】见每个声明的注释
*/

/*
** 设置 GC 债务
** debt：新的债务值
**
** 【用途】
** 调整 GC 的触发阈值
** 债务越高，越快触发 GC
*/
LUAI_FUNC void luaE_setdebt(global_State *g, l_mem debt);

/*
** 释放一个线程
** L：当前线程
** L1：要释放的线程
**
** 【用途】
** 清理线程使用的资源（栈、CallInfo 等）
*/
LUAI_FUNC void luaE_freethread(lua_State *L, lua_State *L1);

/*
** 计算线程的大小（字节）
**
** 【返回值】
** lu_mem：无符号内存大小类型
**
** 【用途】
** GC 需要知道对象的大小来计算内存使用
*/
LUAI_FUNC lu_mem luaE_threadsize(lua_State *L);

/*
** 扩展 CallInfo 链表
**
** 【返回值】
** 新分配的 CallInfo 节点
**
** 【用途】
** 当调用栈满时，需要扩展 CallInfo 链表
*/
LUAI_FUNC CallInfo *luaE_extendCI(lua_State *L);

/*
** 收缩 CallInfo 链表
**
** 【用途】
** 当调用栈变短时，释放多余的 CallInfo 节点以节省内存
*/
LUAI_FUNC void luaE_shrinkCI(lua_State *L);

/*
** 检查 C 栈空间
**
** 【用途】
** 在可能进行深度递归的操作前检查 C 栈是否有足够空间
** 防止栈溢出导致程序崩溃
*/
LUAI_FUNC void luaE_checkcstack(lua_State *L);

/*
** 增加 C 栈计数
**
** 【用途】
** 在进入 C 函数时调用，跟踪 C 调用深度
** 防止 C 栈溢出
*/
LUAI_FUNC void luaE_incCstack(lua_State *L);

/*
** 发出警告
** msg：警告消息
** tocont：是否继续（1 表示这是多部分警告的一部分）
**
** 【用途】
** 通过 lua_WarnFunction 向用户发出警告
** 不会中断程序执行
*/
LUAI_FUNC void luaE_warning(lua_State *L, const char *msg, int tocont);

/*
** 发出警告错误
** where：错误发生的位置描述
**
** 【用途】
** 在特定位置发生错误时发出警告
*/
LUAI_FUNC void luaE_warnerror(lua_State *L, const char *where);

/*
** 重置线程状态
** status：新的状态
**
** 【返回值】
** TStatus：线程状态枚举
**
** 【用途】
** 在错误或协程操作后重置线程到可用状态
** 清理栈、关闭 upvalue 等
*/
LUAI_FUNC TStatus luaE_resetthread(lua_State *L, TStatus status);

#endif

/*
================================================================================
文件总结：lstate.h 核心要点
================================================================================

【一、文件职责】
lstate.h 是 Lua 虚拟机的"大脑"，定义了执行状态的核心数据结构：
1. lua_State：单个执行线程（协程）的状态
2. global_State：所有线程共享的全局状态
3. CallInfo：函数调用栈的节点

【二、关键设计思想】

1. 协程模型
   - Lua 的"线程"是协程，不是 OS 线程
   - 轻量级，完全由 Lua 控制
   - 通过 lua_State 实现多个独立执行上下文

2. 内存管理
   - 统一的内存分配器（lua_Alloc）
   - GC 债务机制控制回收频率
   - 支持增量式和分代式两种 GC 模式

3. 调用栈管理
   - CallInfo 双向链表表示调用栈
   - 区分 Lua 函数和 C 函数的上下文
   - 支持尾调用优化

4. 类型系统
   - 通过联合体和类型标签实现多态
   - 所有对象都有 CommonHeader
   - 类型安全的转换宏

【三、重要数据结构关系】

lua_State (线程)
  ├─ stack：数据栈
  ├─ ci：当前调用信息
  ├─ l_G：指向全局状态
  └─ openupval：开放的 upvalue

global_State (全局)
  ├─ allgc：所有可回收对象
  ├─ strt：字符串表
  ├─ mt[]：基本类型元表
  └─ mainth：主线程

CallInfo (调用)
  ├─ func：函数在栈中的位置
  ├─ top：函数的栈顶
  ├─ u：Lua/C 函数特定信息
  └─ callstatus：调用状态位

【四、GC 核心机制】

1. 对象链表组织
   - allgc：普通对象
   - finobj：带终结器的对象
   - fixedgc：不可回收对象
   - 分代 GC 的代际标记

2. 三色标记
   - gray：等待访问
   - grayagain：需要重新访问
   - 弱表的特殊处理

3. 债务机制
   - 通过债务值控制 GC 触发
   - 平衡内存使用和性能

【五、性能优化技巧】

1. 字符串内部化
   - 所有相同字符串共享一个实例
   - 字符串比较变成指针比较
   - stringtable 哈希表管理

2. 栈管理
   - 预留 EXTRA_STACK 避免频繁检查
   - 动态扩展和收缩

3. 位域操作
   - nCcalls 用一个 32 位整数存两个计数
   - callstatus 用位标志存多个状态

4. 缓存机制
   - strcache：API 字符串缓存
   - 减少重复创建

【六、C 语言高级技巧】

1. 联合体实现多态
   - GCUnion 统一所有对象类型
   - 通过公共头部安全转换

2. 宏的使用
   - 条件编译（#if !defined）
   - 内联函数（宏定义）
   - 类型检查（check_exp）

3. 内存布局控制
   - LX 结构体前置额外空间
   - 对齐和填充

4. 信号安全
   - sig_atomic_t 保证原子操作
   - volatile 防止编译器优化

【七、学习建议】

1. 理解结构关系
   - 先掌握三个核心结构
   - 理解它们的生命周期
   - 明确所有权和引用关系

2. 追踪执行流程
   - 从 lua_State 创建开始
   - 跟踪函数调用过程
   - 观察 GC 如何工作

3. 实践验证
   - 编写测试代码
   - 使用调试器查看内存布局
   - 分析性能特征

4. 阅读相关文件
   - ldo.c：执行和错误处理
   - lgc.c：垃圾回收实现
   - lvm.c：虚拟机执行循环

【八、常见问题】

Q: 为什么需要两种 GC 模式？
A: 增量式适合交互式应用（低延迟），分代式适合批处理（高吞吐量）

Q: upvalue 是什么？
A: 闭包捕获的外部局部变量，实现词法作用域

Q: 为什么有 stack 和 top？
A: stack 是栈底（固定），top 是栈顶（移动），定义了当前可用的栈空间

Q: CIST_* 标志有什么用？
A: 记录函数调用的各种状态，用于错误处理、调试、优化等

【九、关键概念速查】

- lua_State：线程状态（协程）
- global_State：全局共享状态
- CallInfo：函数调用信息
- GCObject：可回收对象基类
- upvalue：闭包的外部变量
- tbc：待关闭变量（Lua 5.4）
- 三色标记：白（未访问）、灰（部分访问）、黑（完全访问）
- 写屏障：维护 GC 不变式的机制
- 尾调用：优化的函数调用，不增加栈深度

【十、后续学习路径】

1. 阅读 ldo.c：理解执行流程
2. 阅读 lgc.c：深入 GC 实现
3. 阅读 lvm.c：学习指令执行
4. 阅读 lapi.c：了解 C API 实现
5. 调试实际代码：验证理论理解

================================================================================
*/
