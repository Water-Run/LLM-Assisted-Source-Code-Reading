/*
** $Id: ldo.h $
** Stack and Call structure of Lua
** 【文件标识】Lua的栈和调用结构
** See Copyright Notice in lua.h
** 【版权声明】参见lua.h中的版权声明
*/

/*
================================================================================
【文件概要】ldo.h - Lua执行引擎的核心头文件

【基本信息】
- 文件名: ldo.h
- 作用: 定义Lua虚拟机的执行控制、栈管理、函数调用等核心机制
- 地位: 这是Lua解释器最核心的头文件之一,"do"代表"do/execute"(执行)

【主要功能】
1. 栈管理: 定义栈空间检查、增长、缩减等宏和函数
2. 函数调用: 提供函数调用的准备、执行、返回等接口
3. 错误处理: 定义保护模式执行和异常抛出机制
4. Hook机制: 支持调试钩子的触发

【核心概念】
- Stack(栈): Lua使用栈来存储局部变量、函数参数、返回值等
- CallInfo: 调用信息结构,记录每次函数调用的上下文
- Protected Mode: 保护模式,允许在C层面捕获Lua错误
- StkId: 栈标识符,指向栈中某个元素的指针类型

【依赖关系】
- llimits.h: Lua的基本限制和类型定义
- lobject.h: Lua对象的表示和操作
- lstate.h: Lua状态机的定义
- lzio.h: Lua的缓冲输入接口
================================================================================
*/

#ifndef ldo_h
#define ldo_h

#include "llimits.h" // 【包含】基本限制定义(如最大值、最小值等)
#include "lobject.h" // 【包含】Lua对象类型定义(TValue, Table等)
#include "lstate.h"  // 【包含】Lua状态机定义(lua_State结构)
#include "lzio.h"    // 【包含】缓冲输入流定义(用于解析)

/*
================================================================================
【栈检查和增长机制】

Lua使用一个动态数组作为运行栈,当栈空间不足时需要自动扩展。
以下宏提供了高效的栈空间检查和扩展机制。
================================================================================
*/

/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/
/*
** 【宏说明】检查栈大小并在需要时增长栈
**
** 参数'pre'/'pos'允许宏在重新分配栈时保留指向栈的指针,
** 仅在需要时执行操作。它还允许在栈重新分配时运行一次GC步骤。
**
** 'condmovestack'在压力测试中用于强制在每次检查时重新分配栈。
**
** 【设计思想】
** - pre: 在栈可能重新分配前执行的代码(通常用于保存指针)
** - pos: 在栈重新分配后执行的代码(通常用于恢复指针)
** - 这种设计允许调用者在栈地址变化时保持指针的有效性
**
** 【为什么需要pre/pos】
** 栈重新分配时,整个栈会移动到新的内存位置,所有指向栈的指针都会失效。
** pre/pos机制允许在重新分配前保存指针的偏移量,之后根据新栈地址恢复。
*/

#if !defined(HARDSTACKTESTS)
/*
** 【正常模式】不进行额外的栈移动测试
** condmovestack被定义为空操作,不做任何事
** (void)0 是C语言中表示"什么都不做"的惯用法
*/
#define condmovestack(L, pre, pos) ((void)0)
#else
/* realloc stack keeping its size */
/* 【测试模式】重新分配栈但保持其大小 */
/*
** 【说明】这个模式用于压力测试,强制每次都重新分配栈
** 即使栈大小不变,也会触发内存重新分配,用于测试指针保存/恢复逻辑
**
** stacksize(L): 获取当前栈大小
** luaD_reallocstack: 重新分配栈内存
** 第三个参数0表示不需要额外空间,只是重新分配
*/
#define condmovestack(L, pre, pos)  \
  {                                 \
    int sz_ = stacksize(L);         \
    pre;                            \
    luaD_reallocstack((L), sz_, 0); \
    pos;                            \
  }
#endif

/*
** 【核心宏】luaD_checkstackaux - 检查栈空间并在需要时扩展
**
** 参数说明:
** - L: lua_State指针,表示Lua虚拟机状态
** - n: 需要的额外栈空间大小(元素个数)
** - pre: 在可能的栈重新分配前执行的代码
** - pos: 在栈重新分配后执行的代码
**
** 【工作流程】
** 1. 检查剩余栈空间: L->stack_last.p - L->top.p 计算栈顶到栈末的距离
**    - L->top.p: 当前栈顶指针
**    - L->stack_last.p: 栈的最后可用位置
** 2. 如果剩余空间 <= n,说明空间不足:
**    - 执行pre代码(保存需要的指针)
**    - 调用luaD_growstack扩展栈
**    - 执行pos代码(恢复指针)
** 3. 否则,空间充足,只执行condmovestack(在测试模式下会强制重新分配)
**
** 【l_unlikely宏】
** 这是一个编译器提示,告诉编译器这个条件"不太可能"为真。
** 编译器可以据此优化分支预测,因为大多数时候栈空间是足够的。
*/
#define luaD_checkstackaux(L, n, pre, pos)           \
  if (l_unlikely(L->stack_last.p - L->top.p <= (n))) \
  {                                                  \
    pre;                                             \
    luaD_growstack(L, n, 1);                         \
    pos;                                             \
  }                                                  \
  else                                               \
  {                                                  \
    condmovestack(L, pre, pos);                      \
  }

/* In general, 'pre'/'pos' are empty (nothing to save) */
/* 【简化版本】一般情况下,'pre'/'pos'是空的(没有需要保存的东西) */
/*
** 【最常用的栈检查宏】
** 当不需要保存任何指针时使用这个简化版本
** (void)0 表示pre和pos都是空操作
*/
#define luaD_checkstack(L, n) luaD_checkstackaux(L, n, (void)0, (void)0)

/*
================================================================================
【栈位置保存和恢复】

由于栈可能重新分配到不同的内存地址,直接保存指针是不安全的。
这里提供了基于偏移量的保存/恢复机制。
================================================================================
*/

/*
** 【宏】savestack - 将栈指针转换为偏移量保存
**
** 参数:
** - L: lua_State指针
** - pt: 指向栈中某个位置的指针(StkId类型)
**
** 返回: ptrdiff_t类型的偏移量(相对于栈底的字节偏移)
**
** 【工作原理】
** 1. cast_charp(pt): 将pt转换为char*类型
** 2. cast_charp(L->stack.p): 将栈底指针转换为char*类型
** 3. 两者相减得到字节偏移量
**
** 【为什么用char*】
** char*的指针运算是以字节为单位的,这样可以精确计算字节偏移
*/
#define savestack(L, pt) (cast_charp(pt) - cast_charp(L->stack.p))

/*
** 【宏】restorestack - 将偏移量恢复为栈指针
**
** 参数:
** - L: lua_State指针
** - n: 之前保存的偏移量(ptrdiff_t类型)
**
** 返回: StkId类型的指针,指向栈中对应位置
**
** 【工作原理】
** 1. cast_charp(L->stack.p): 将栈底指针转换为char*
** 2. 加上偏移量n,得到目标位置的char*指针
** 3. cast(StkId, ...): 将结果转换回StkId类型
**
** 【配合使用示例】
** ptrdiff_t saved = savestack(L, p);  // 保存指针p的位置
** // ... 栈可能重新分配 ...
** p = restorestack(L, saved);  // 恢复指针p
*/
#define restorestack(L, n) cast(StkId, cast_charp(L->stack.p) + (n))

/* macro to check stack size, preserving 'p' */
/* 【宏】检查栈大小,同时保留指针'p' */
/*
** 【高级宏】checkstackp - 检查栈并保留一个指针
**
** 参数:
** - L: lua_State指针
** - n: 需要的额外栈空间
** - p: 需要保留的栈指针
**
** 【工作原理】
** 使用luaD_checkstackaux,在pre部分保存p的偏移量,在pos部分恢复p:
** 1. pre部分: ptrdiff_t t__ = savestack(L, p) - 保存p的偏移量到临时变量t__
** 2. 如果需要,luaD_growstack会重新分配栈
** 3. pos部分: p = restorestack(L, t__) - 根据偏移量恢复p
**
** 【使用场景】
** 当你持有一个指向栈的指针,但需要确保有足够的栈空间时使用。
** 例如:你正在操作栈上的某个值,但接下来的操作可能需要更多栈空间。
*/
#define checkstackp(L, n, p)                                         \
  luaD_checkstackaux(L, n,                                           \
                     ptrdiff_t t__ = savestack(L, p), /* save 'p' */ \
                     p = restorestack(L, t__))        /* 'pos' part: restore 'p' */

/*
================================================================================
【C调用深度限制】

为了防止栈溢出和无限递归,Lua限制了C函数的嵌套调用深度。
================================================================================
*/

/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
/*
** 【常量】LUAI_MAXCCALLS - C调用的最大嵌套深度
**
** 用于限制:
** - 嵌套的C函数调用
** - 语法上嵌套的非终结符
** - 其他通过C递归实现的特性
**
** 【限制说明】
** - 值必须能装入16位无符号整数(即 <= 65535)
** - 必须与C栈的大小兼容
**
** 【为什么需要这个限制】
** 1. 防止C栈溢出:每次C函数调用都会消耗C栈空间
** 2. 防止无限递归:检测可能的递归错误
** 3. 提供可预测的行为:用户知道调用深度的限制
**
** 【默认值200的考虑】
** - 对于正常程序足够大,不会限制合理的递归
** - 对于大多数平台,200次调用不会耗尽C栈
** - 可以通过编译选项修改这个值
*/
#if !defined(LUAI_MAXCCALLS)
#define LUAI_MAXCCALLS 200
#endif

/*
================================================================================
【保护模式函数类型】

Lua使用setjmp/longjmp机制实现异常处理,保护函数类型用于在保护模式下执行。
================================================================================
*/

/* type of protected functions, to be ran by 'runprotected' */
/* 【类型定义】保护函数的类型,将被'runprotected'运行 */
/*
** 【函数指针类型】Pfunc - 保护模式下执行的函数类型
**
** 函数签名:
** - 参数1: lua_State *L - Lua虚拟机状态
** - 参数2: void *ud - 用户数据(user data),可以是任何类型的指针
** - 返回值: void(无返回值)
**
** 【用途】
** 这种函数在保护模式下执行,如果发生错误,不会直接崩溃,
** 而是通过longjmp跳转到错误处理代码。
**
** 【典型用法】
** void my_protected_func(lua_State *L, void *ud) {
**     // 执行可能出错的操作
**     // 如果出错,会通过luaD_throw跳转,不会执行到这里的后续代码
** }
**
** TStatus status = luaD_pcall(L, my_protected_func, userdata, ...);
** // status会告诉你函数是正常结束还是出错了
*/
typedef void (*Pfunc)(lua_State *L, void *ud);

/*
================================================================================
【函数声明部分】

以下是ldo.c中实现的所有函数的声明。这些函数构成了Lua执行引擎的核心。
LUAI_FUNC宏用于声明Lua内部函数,通常会处理函数的可见性等问题。
================================================================================
*/

/*
** 【函数】luaD_errerr - 处理"错误处理函数本身出错"的情况
**
** 参数:
** - L: lua_State指针
**
** 返回: l_noret表示这个函数不会返回(会抛出异常)
**
** 【说明】
** 当Lua试图调用错误处理函数,但错误处理函数本身又出错时,
** 会调用这个函数。这是一个紧急情况,因为连错误处理都失败了。
** 这个函数会抛出一个"error in error handling"错误。
**
** l_noret: 这是一个编译器属性,表示函数不会正常返回
** (通常是因为会调用longjmp或exit)
*/
LUAI_FUNC l_noret luaD_errerr(lua_State *L);

/*
** 【函数】luaD_seterrorobj - 设置错误对象到栈上
**
** 参数:
** - L: lua_State指针
** - errcode: 错误状态码(TStatus类型)
** - oldtop: 旧的栈顶位置
**
** 【说明】
** 当发生错误时,需要在栈上放置一个错误对象。
** 这个函数根据错误码创建合适的错误对象并放到栈上。
** oldtop参数用于确定错误对象应该放在哪里。
**
** 【错误对象】
** - 可能是错误消息字符串
** - 可能是用户自定义的错误对象
** - 对于内存错误,可能是预定义的错误消息
*/
LUAI_FUNC void luaD_seterrorobj(lua_State *L, TStatus errcode, StkId oldtop);

/*
** 【函数】luaD_protectedparser - 在保护模式下解析Lua代码
**
** 参数:
** - L: lua_State指针
** - z: ZIO输入流(包含要解析的代码)
** - name: 代码块的名称(通常是文件名或"=stdin"等)
** - mode: 解析模式("t"表示文本,"b"表示二进制,NULL表示两者都可以)
**
** 返回: TStatus - 解析状态(成功或错误码)
**
** 【说明】
** 这个函数在保护模式下解析Lua代码。如果解析过程中出错,
** 不会崩溃,而是返回错误状态。
**
** 【ZIO】
** ZIO(Zio Input/Output)是Lua的缓冲输入机制,提供统一的接口
** 来读取不同来源的数据(文件、内存、字符串等)
*/
LUAI_FUNC TStatus luaD_protectedparser(lua_State *L, ZIO *z,
                                       const char *name,
                                       const char *mode);

/*
** 【函数】luaD_hook - 触发调试钩子
**
** 参数:
** - L: lua_State指针
** - event: 钩子事件类型(如调用、返回、行、计数等)
** - line: 当前行号(-1表示无行号信息)
** - fTransfer: 是否是尾调用转移
** - nTransfer: 转移的参数数量
**
** 【说明】
** Lua支持调试钩子,可以在特定事件发生时调用用户定义的函数。
** 这对于调试器、性能分析器等工具非常有用。
**
** 【钩子事件类型】
** - LUA_HOOKCALL: 函数调用
** - LUA_HOOKRET: 函数返回
** - LUA_HOOKLINE: 执行到新的一行
** - LUA_HOOKCOUNT: 执行了指定数量的指令
** - LUA_HOOKTAILCALL: 尾调用
*/
LUAI_FUNC void luaD_hook(lua_State *L, int event, int line,
                         int fTransfer, int nTransfer);

/*
** 【函数】luaD_hookcall - 在函数调用时触发钩子
**
** 参数:
** - L: lua_State指针
** - ci: CallInfo指针,包含调用信息
**
** 【说明】
** 这是luaD_hook的特化版本,专门用于函数调用事件。
** 它会从CallInfo中提取必要的信息并调用luaD_hook。
*/
LUAI_FUNC void luaD_hookcall(lua_State *L, CallInfo *ci);

/*
** 【函数】luaD_pretailcall - 准备尾调用
**
** 参数:
** - L: lua_State指针
** - ci: 当前的CallInfo
** - func: 要调用的函数
** - narg1: 参数数量加1
** - delta: 栈调整量
**
** 返回: int - 如果可以进行尾调用优化返回非0,否则返回0
**
** 【尾调用优化】
** 当一个函数的最后一个动作是调用另一个函数并返回其结果时,
** 可以重用当前的栈帧,而不是创建新的。这样可以避免栈溢出,
** 实现无限深度的尾递归。
**
** 【示例】
** function foo(n)
**     if n > 0 then
**         return foo(n-1)  -- 这是尾调用
**     end
**     return n
** end
*/
LUAI_FUNC int luaD_pretailcall(lua_State *L, CallInfo *ci, StkId func,
                               int narg1, int delta);

/*
** 【函数】luaD_precall - 准备函数调用
**
** 参数:
** - L: lua_State指针
** - func: 要调用的函数(在栈上的位置)
** - nResults: 期望的返回值数量(LUA_MULTRET表示所有返回值)
**
** 返回: CallInfo* - 新的调用信息,如果函数已经是C函数并执行完毕则返回NULL
**
** 【说明】
** 这个函数为即将进行的函数调用做准备:
** 1. 检查是否是C函数还是Lua函数
** 2. 如果是C函数,直接调用并返回NULL
** 3. 如果是Lua函数,创建新的CallInfo并返回
** 4. 检查栈空间是否足够
** 5. 设置调用钩子
*/
LUAI_FUNC CallInfo *luaD_precall(lua_State *L, StkId func, int nResults);

/*
** 【函数】luaD_call - 调用一个函数
**
** 参数:
** - L: lua_State指针
** - func: 要调用的函数在栈上的位置
** - nResults: 期望的返回值数量
**
** 【说明】
** 这是Lua中调用函数的标准方式。它会:
** 1. 调用luaD_precall做准备
** 2. 如果是Lua函数,进入虚拟机执行
** 3. 调用luaD_poscall处理返回
** 4. 允许在执行过程中yield(挂起)
**
** 【与luaD_callnoyield的区别】
** luaD_call允许函数执行过程中挂起(yield),
** 而luaD_callnoyield不允许。
*/
LUAI_FUNC void luaD_call(lua_State *L, StkId func, int nResults);

/*
** 【函数】luaD_callnoyield - 调用函数(不允许yield)
**
** 参数:
** - L: lua_State指针
** - func: 要调用的函数在栈上的位置
** - nResults: 期望的返回值数量
**
** 【说明】
** 与luaD_call类似,但不允许在执行过程中挂起。
** 这用于某些不能被中断的操作,例如:
** - 元方法调用
** - 错误处理函数调用
** - 垃圾回收器回调
**
** 如果被调用的函数试图yield,会产生错误。
*/
LUAI_FUNC void luaD_callnoyield(lua_State *L, StkId func, int nResults);

/*
** 【函数】luaD_closeprotected - 在保护模式下关闭to-be-closed变量
**
** 参数:
** - L: lua_State指针
** - level: 栈级别(要关闭到哪个位置)
** - status: 当前状态码
**
** 返回: TStatus - 操作后的状态码
**
** 【说明】
** Lua 5.4引入了to-be-closed变量(使用<close>或<const>声明)。
** 这些变量在离开作用域时会自动调用其__close元方法。
** 这个函数在保护模式下执行关闭操作,确保即使__close出错也能正确处理。
**
** 【to-be-closed变量示例】
** local file <close> = io.open("test.txt")
** -- file会在离开作用域时自动关闭
*/
LUAI_FUNC TStatus luaD_closeprotected(lua_State *L, ptrdiff_t level,
                                      TStatus status);

/*
** 【函数】luaD_pcall - 在保护模式下调用函数
**
** 参数:
** - L: lua_State指针
** - func: 要执行的保护函数(Pfunc类型)
** - u: 传递给func的用户数据
** - oldtop: 调用前的栈顶位置
** - ef: 错误处理函数的位置(0表示没有)
**
** 返回: TStatus - 执行状态(LUA_OK表示成功)
**
** 【说明】
** 这是Lua的核心错误处理机制。它使用setjmp/longjmp实现:
** 1. 设置错误处理点(setjmp)
** 2. 调用func
** 3. 如果func中调用了luaD_throw,会跳回到这里(longjmp)
** 4. 根据错误码进行相应处理
**
** 【保护模式的好处】
** - 错误不会导致程序崩溃
** - 可以在C代码中捕获Lua错误
** - 保证即使出错也能正确清理资源
*/
LUAI_FUNC TStatus luaD_pcall(lua_State *L, Pfunc func, void *u,
                             ptrdiff_t oldtop, ptrdiff_t ef);

/*
** 【函数】luaD_poscall - 函数调用后的处理
**
** 参数:
** - L: lua_State指针
** - ci: 完成调用的CallInfo
** - nres: 实际返回值数量
**
** 【说明】
** 在函数执行完毕后调用,负责:
** 1. 将返回值移动到正确的位置
** 2. 调整栈顶指针
** 3. 触发返回钩子
** 4. 释放CallInfo
** 5. 恢复调用者的状态
*/
LUAI_FUNC void luaD_poscall(lua_State *L, CallInfo *ci, int nres);

/*
** 【函数】luaD_reallocstack - 重新分配栈空间
**
** 参数:
** - L: lua_State指针
** - newsize: 新的栈大小
** - raiseerror: 如果失败是否抛出错误(1表示是,0表示否)
**
** 返回: int - 成功返回1,失败返回0(仅当raiseerror为0时)
**
** 【说明】
** 改变栈的大小。这个函数:
** 1. 分配新的内存
** 2. 复制旧栈的内容
** 3. 更新所有指向栈的指针
** 4. 释放旧的栈内存
**
** 【注意】
** 调用这个函数后,所有指向栈的指针都会失效!
** 必须使用savestack/restorestack来保持指针有效性。
*/
LUAI_FUNC int luaD_reallocstack(lua_State *L, int newsize, int raiseerror);

/*
** 【函数】luaD_growstack - 增长栈空间
**
** 参数:
** - L: lua_State指针
** - n: 需要的额外空间
** - raiseerror: 如果失败是否抛出错误
**
** 返回: int - 成功返回1,失败返回0
**
** 【说明】
** 确保栈至少有n个额外的空闲槽位。如果空间不足:
** 1. 计算新的栈大小(通常是当前大小的两倍)
** 2. 调用luaD_reallocstack重新分配
** 3. 可能触发一次垃圾回收
**
** 【与luaD_reallocstack的区别】
** - luaD_growstack只增长栈,不会缩小
** - luaD_growstack会考虑栈的增长策略(如倍增)
** - luaD_reallocstack直接设置到指定大小
*/
LUAI_FUNC int luaD_growstack(lua_State *L, int n, int raiseerror);

/*
** 【函数】luaD_shrinkstack - 缩小栈空间
**
** 参数:
** - L: lua_State指针
**
** 【说明】
** 当栈空间使用率很低时,缩小栈以节省内存。
** 通常在垃圾回收时调用。这个函数会:
** 1. 检查当前栈的使用情况
** 2. 如果使用率低于某个阈值,缩小栈
** 3. 但会保证栈大小不小于某个最小值
**
** 【时机】
** 主要在以下情况下调用:
** - 垃圾回收期间
** - 协程结束后
** - 长时间闲置的栈
*/
LUAI_FUNC void luaD_shrinkstack(lua_State *L);

/*
** 【函数】luaD_inctop - 增加栈顶
**
** 参数:
** - L: lua_State指针
**
** 【说明】
** 将栈顶指针L->top.p向上移动一个位置。
** 这个函数会先检查栈空间是否足够,如果不够会自动扩展。
** 这是一个安全的方式来向栈上添加元素。
**
** 【使用场景】
** 当你要向栈上push一个新值时,先调用luaD_inctop确保有空间,
** 然后再设置值。
*/
LUAI_FUNC void luaD_inctop(lua_State *L);

/*
** 【函数】luaD_checkminstack - 检查并确保最小栈空间
**
** 参数:
** - L: lua_State指针
**
** 返回: int - 成功返回1,失败返回0
**
** 【说明】
** 确保栈至少有LUA_MINSTACK个空闲槽位。
** LUA_MINSTACK是Lua API调用所需的最小栈空间。
**
** 【为什么需要】
** Lua C API的许多函数需要一定的栈空间来工作。
** 这个函数确保调用C API前有足够的空间。
*/
LUAI_FUNC int luaD_checkminstack(lua_State *L);

/*
** 【函数】luaD_throw - 抛出Lua异常
**
** 参数:
** - L: lua_State指针
** - errcode: 错误码(TStatus类型)
**
** 返回: l_noret - 不会返回
**
** 【说明】
** 抛出Lua异常。这个函数:
** 1. 如果有错误处理器,跳转到最近的luaD_pcall(通过longjmp)
** 2. 如果没有错误处理器,终止程序或返回到主机程序
**
** 【实现机制】
** 使用C标准库的longjmp函数实现非本地跳转。
** 这类似于其他语言中的throw/raise异常。
**
** 【错误码】
** - LUA_OK: 成功(不会用于throw)
** - LUA_ERRRUN: 运行时错误
** - LUA_ERRMEM: 内存分配错误
** - LUA_ERRERR: 错误处理函数出错
** - LUA_ERRSYNTAX: 语法错误
** - LUA_YIELD: 协程挂起(不是真正的错误)
*/
LUAI_FUNC l_noret luaD_throw(lua_State *L, TStatus errcode);

/*
** 【函数】luaD_throwbaselevel - 在基础层抛出异常
**
** 参数:
** - L: lua_State指针
** - errcode: 错误码
**
** 返回: l_noret - 不会返回
**
** 【说明】
** 类似luaD_throw,但用于没有错误处理器的情况。
** "基础层"指的是Lua状态机的最外层,即主线程的底部。
** 在这一层抛出异常意味着无法被捕获,会导致程序退出或
** 返回到主机程序(如果是嵌入式使用)。
**
** 【使用场景】
** - 严重错误,如内存耗尽
** - 虚拟机初始化失败
** - 无法恢复的内部错误
*/
LUAI_FUNC l_noret luaD_throwbaselevel(lua_State *L, TStatus errcode);

/*
** 【函数】luaD_rawrunprotected - 原始保护模式运行
**
** 参数:
** - L: lua_State指针
** - f: 要执行的函数
** - ud: 用户数据
**
** 返回: TStatus - 执行状态
**
** 【说明】
** 这是最底层的保护模式执行函数。它直接使用setjmp/longjmp,
** 不进行任何额外的处理(如错误对象设置等)。
**
** 【与luaD_pcall的区别】
** - luaD_rawrunprotected: 最原始的保护,只捕获异常
** - luaD_pcall: 在luaD_rawrunprotected基础上添加了错误对象处理、
**   栈清理等高级功能
**
** 【使用场景】
** 主要在Lua内部使用,例如:
** - 虚拟机初始化
** - 最底层的错误处理
** - 不能依赖其他机制的关键操作
*/
LUAI_FUNC TStatus luaD_rawrunprotected(lua_State *L, Pfunc f, void *ud);

#endif

/*
================================================================================
【文件总结】ldo.h - Lua执行引擎核心头文件

【整体架构】
这个头文件定义了Lua虚拟机执行的核心机制,可以分为几个主要部分:

1. 【栈管理】(约占30%)
   - 栈检查: luaD_checkstack, luaD_checkstackaux, checkstackp
   - 栈增长: luaD_growstack, luaD_reallocstack, luaD_inctop
   - 栈缩减: luaD_shrinkstack
   - 指针保存: savestack, restorestack

   【设计思想】
   Lua的栈是动态增长的,这些机制确保栈空间总是充足的,同时
   通过保存/恢复机制处理栈重新分配时指针失效的问题。

2. 【函数调用机制】(约占30%)
   - 准备阶段: luaD_precall, luaD_pretailcall
   - 执行阶段: luaD_call, luaD_callnoyield
   - 返回阶段: luaD_poscall

   【设计思想】
   函数调用被分解为多个阶段,每个阶段处理特定的任务。
   支持尾调用优化和协程挂起等高级特性。

3. 【错误处理】(约占25%)
   - 异常抛出: luaD_throw, luaD_throwbaselevel
   - 保护执行: luaD_pcall, luaD_rawrunprotected
   - 错误对象: luaD_seterrorobj, luaD_errerr
   - 资源清理: luaD_closeprotected

   【设计思想】
   使用setjmp/longjmp实现异常处理,提供多层次的错误捕获和处理。
   即使在错误情况下也能正确清理资源。

4. 【调试支持】(约占10%)
   - 钩子机制: luaD_hook, luaD_hookcall

   【设计思想】
   在关键执行点插入钩子,允许外部监控和调试Lua代码的执行。

5. 【解析器集成】(约占5%)
   - 保护解析: luaD_protectedparser

   【设计思想】
   将代码解析也纳入错误处理机制,语法错误不会导致崩溃。

【关键C语言技术】

1. 【宏定义技巧】
   - 多语句宏使用do-while(0)包装(虽然这里用的是大括号)
   - 使用##和#进行字符串拼接和字符串化
   - 条件编译(#if, #ifdef)用于不同配置

2. 【函数指针】
   - typedef void (*Pfunc)(lua_State *L, void *ud)
   - 允许将函数作为参数传递,实现回调机制

3. 【类型转换】
   - cast, cast_charp等宏用于类型转换
   - 通过char*进行指针运算计算字节偏移

4. 【编译器属性】
   - l_noret: 告诉编译器函数不会返回(如__attribute__((noreturn)))
   - l_unlikely: 分支预测提示

5. 【longjmp/setjmp】
   - 虽然没有直接出现在头文件中,但多个l_noret函数都会使用
   - 这是C语言中实现异常处理的经典方法

【性能优化考虑】

1. 【栈检查优化】
   - 使用l_unlikely提示分支预测,因为大多数时候栈是够用的
   - 只在必要时才重新分配栈,减少内存操作

2. 【内联和宏】
   - 频繁使用的简单操作定义为宏,避免函数调用开销
   - 如luaD_checkstack, savestack, restorestack

3. 【尾调用优化】
   - luaD_pretailcall专门处理尾调用,避免栈增长

【安全性设计】

1. 【栈溢出保护】
   - LUAI_MAXCCALLS限制递归深度
   - 自动栈增长机制

2. 【错误隔离】
   - 保护模式确保错误不会跨越C/Lua边界导致崩溃
   - 多层次的错误处理(rawrunprotected -> pcall -> call)

3. 【资源清理】
   - luaD_closeprotected确保即使出错也能清理资源
   - 错误处理函数本身的错误也被处理(luaD_errerr)

【与其他模块的关系】

- lstate.h: 提供lua_State结构定义
- lobject.h: 提供Lua对象类型定义
- lvm.h: 虚拟机会调用这里定义的函数来管理执行流程
- lapi.h: Lua C API会使用这些函数来实现调用、错误处理等
- lgc.h: 垃圾回收器会调用栈缩减等函数

【学习建议】

1. 【从简单到复杂】
   - 先理解栈的基本概念和savestack/restorestack
   - 再学习栈检查和增长机制
   - 最后理解函数调用和错误处理的完整流程

2. 【结合实现阅读】
   - 这个头文件只有声明,要真正理解需要阅读ldo.c的实现
   - 特别要理解setjmp/longjmp的使用

3. 【实验验证】
   - 可以通过添加打印语句观察栈的增长
   - 通过故意制造错误观察错误处理流程
   - 通过递归观察调用深度限制

4. 【关注细节】
   - 为什么有些函数返回int,有些返回TStatus
   - 为什么需要pre/pos参数
   - 为什么有callnoyield和call两个版本

【常见陷阱】

1. 【栈指针失效】
   - 任何可能重新分配栈的操作后,旧的栈指针都会失效
   - 必须使用savestack/restorestack保持有效性

2. 【错误处理】
   - 不是所有函数都能在所有上下文中调用
   - 有些操作不能被中断(yield),要使用callnoyield

3. 【递归限制】
   - C函数递归深度受LUAI_MAXCCALLS限制
   - Lua函数递归深度受栈大小限制

================================================================================
*/
