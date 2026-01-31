/*
** 【文件概要】
** 文件名: lgc.h
** 所属模块: Lua 垃圾回收器（Garbage Collector, GC）
** 功能概述:
**   - 定义 GC 的状态机常量、颜色/年龄位布局、以及相关位运算宏。
**   - 提供 GC 运行控制宏、写屏障宏（barrier），以及默认参数。
**   - 声明 GC 相关的核心函数接口（由 .c 实现）。
**
** 关键阅读提示:
**   - 本文件是“接口 + 宏定义”层，逻辑主要通过宏表达。
**   - 真正的 GC 算法实现位于对应的 .c 文件（如 lgc.c）。
**   - 这里的宏会被 Lua 核心频繁使用，尤其是写屏障与颜色/年龄判断。
**
** 高级 C 用法提示:
**   - 使用大量宏进行条件内联与位操作，避免函数调用开销。
**   - 依赖 typedef/结构体与“对象头”字段（如 marked）进行位编码。
**   - 利用“条件宏”实现可选的测试逻辑（如 HARDMEMTESTS）。
*/

/*
** $Id: lgc.h $
** Garbage Collector
** See Copyright Notice in lua.h
**
** $Id: lgc.h $
** 垃圾回收器
** 版权说明见 lua.h
*/

#ifndef lgc_h
#define lgc_h

#include <stddef.h>

#include "lobject.h"
#include "lstate.h"

/*
** Collectable objects may have one of three colors: white, which means
** the object is not marked; gray, which means the object is marked, but
** its references may be not marked; and black, which means that the
** object and all its references are marked.  The main invariant of the
** garbage collector, while marking objects, is that a black object can
** never point to a white one. Moreover, any gray object must be in a
** "gray list" (gray, grayagain, weak, allweak, ephemeron) so that it
** can be visited again before finishing the collection cycle. (Open
** upvalues are an exception to this rule, as they are attached to
** a corresponding thread.)  These lists have no meaning when the
** invariant is not being enforced (e.g., sweep phase).
**
** 可回收对象可能具有三种颜色之一：白色表示对象未被标记；
** 灰色表示对象已被标记，但其引用可能未被标记；黑色表示对象及其
** 所有引用都已被标记。垃圾回收器在标记阶段的主要不变式是：
** 黑色对象不能指向白色对象。此外，任何灰色对象必须在某个“灰色列表”
** 中（gray, grayagain, weak, allweak, ephemeron），以便在结束本次回收
** 之前能够再次访问。（开放上值 open upvalues 是例外，因为它们附着于
** 对应的线程。）当该不变式不被强制（例如清扫阶段）时，这些列表没有意义。
**
** 说明注释:
** - 这是典型的“三色标记法”描述：白(未标)、灰(已标未遍历完)、黑(已标且子引用已遍历)。
** - “黑不指白”是增量/分代 GC 的核心安全条件，用于保证不会漏回收存活对象。
** - Lua 维护多个灰链表以处理弱表、后向屏障等特殊情况。
*/

/*
** Possible states of the Garbage Collector
**
** 垃圾回收器可能的状态
*/
#define GCSpropagate 0
#define GCSenteratomic 1
#define GCSatomic 2
#define GCSswpallgc 3
#define GCSswpfinobj 4
#define GCSswptobefnz 5
#define GCSswpend 6
#define GCScallfin 7
#define GCSpause 8

/*
** 说明注释:
** - 这些状态组成 GC 的状态机（增量/分代都会复用）。
** - 传播(Propagate)阶段：遍历灰对象并标记其引用。
** - 原子(Atomic)阶段：一次性完成关键标记并准备清扫。
** - 清扫(Sweep)阶段：分为不同子阶段清扫不同链表。
** - 调用终结器(Call Finalizers)阶段：执行 __gc。
** - 暂停(Pause)阶段：GC 空闲等待下一次触发。
*/

#define issweepphase(g) \
	(GCSswpallgc <= (g)->gcstate && (g)->gcstate <= GCSswpend)

/*
** 说明注释:
** - issweepphase 判断 GC 是否处于“清扫阶段”。
** - 通过比较状态范围实现，无需枚举多个状态。
*/

/*
** macro to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep phase may break
** the invariant, as objects turned white may point to still-black
** objects. The invariant is restored when sweep ends and all objects
** are white again.
**
** 用于判断何时必须维持主不变式（白对象不能指向黑对象）的宏。
** 在一次回收期间，清扫阶段可能打破该不变式，因为被刷白的对象
** 可能指向仍是黑色的对象。当清扫结束、所有对象再次变白时，
** 不变式被恢复。
**
** 说明注释:
** - 这解释了为何清扫阶段允许“黑指白”的暂时性违例。
** - GC 不变式只在标记/增量阶段严格保持。
*/

#define keepinvariant(g) ((g)->gcstate <= GCSatomic)

/*
** some useful bit tricks
**
** 一些有用的位操作技巧
**
** 说明注释:
** - 这里的宏用于对“标记字节 marked”进行位运算。
** - cast_byte、cast_void 等是 Lua 自定义的安全转换宏（见其他头文件）。
*/
#define resetbits(x, m) ((x) &= cast_byte(~(m)))
#define setbits(x, m) ((x) |= (m))
#define testbits(x, m) ((x) & (m))
#define bitmask(b) (1 << (b))
#define bit2mask(b1, b2) (bitmask(b1) | bitmask(b2))
#define l_setbit(x, b) setbits(x, bitmask(b))
#define resetbit(x, b) resetbits(x, bitmask(b))
#define testbit(x, b) testbits(x, bitmask(b))

/*
** Layout for bit use in 'marked' field. First three bits are
** used for object "age" in generational mode. Last bit is used
** by tests.
**
** 'marked' 字段中的位布局。前三位用于分代模式下的对象“年龄”。
** 最后一位用于测试。
**
** 说明注释:
** - Lua 的 GCObject 通常在对象头中包含 marked 字节。
** - 该字节既存颜色信息，也存分代年龄。
*/
#define WHITE0BIT 3	   /* object is white (type 0) */
#define WHITE1BIT 4	   /* object is white (type 1) */
#define BLACKBIT 5	   /* object is black */
#define FINALIZEDBIT 6 /* object has been marked for finalization */

#define TESTBIT 7

/*
** 说明注释:
** - WHITE0/WHITE1 双白位用于“翻转白色集合”，避免全局清零。
** - BLACKBIT 表示已完全标记。
** - FINALIZEDBIT 表示已进入待终结（__gc）流程。
** - TESTBIT 仅用于测试构建（例如断言/统计）。
*/

#define WHITEBITS bit2mask(WHITE0BIT, WHITE1BIT)

/*
** 说明注释:
** - WHITEBITS 同时包含 WHITE0 和 WHITE1，用于判断“是否为白”。
*/

#define iswhite(x) testbits((x)->marked, WHITEBITS)
#define isblack(x) testbit((x)->marked, BLACKBIT)
#define isgray(x) /* neither white nor black */ \
	(!testbits((x)->marked, WHITEBITS | bitmask(BLACKBIT)))

/*
** 说明注释:
** - isgray 判定：既不是白也不是黑，即灰。
** - 这里通过排除 WHITEBITS 与 BLACKBIT 实现。
*/

#define tofinalize(x) testbit((x)->marked, FINALIZEDBIT)

/*
** 说明注释:
** - tofinalize 表示对象已被标记为“需要终结”。
*/

#define otherwhite(g) ((g)->currentwhite ^ WHITEBITS)
#define isdeadm(ow, m) ((m) & (ow))
#define isdead(g, v) isdeadm(otherwhite(g), (v)->marked)

/*
** 说明注释:
** - currentwhite 保存“当前白色集合”位模式（WHITE0 或 WHITE1）。
** - otherwhite 得到“非当前白色集合”（上一个周期的白）。
** - 对象若仍带有“非当前白”位，通常可视为死亡候选。
*/

#define changewhite(x) ((x)->marked ^= WHITEBITS)
#define nw2black(x) \
	check_exp(!iswhite(x), l_setbit((x)->marked, BLACKBIT))

#define luaC_white(g) cast_byte((g)->currentwhite &WHITEBITS)

/*
** 说明注释:
** - changewhite 通过异或翻转 WHITE0/WHITE1，切换白色集合。
** - nw2black: “not white to black” 将非白对象设黑（带断言）。
**   check_exp 是 Lua 的断言宏，确保对象不是白色。
** - luaC_white 获取当前白色位掩码，用于新对象初始化。
*/

/* object age in generational mode */
/* 分代模式下的对象年龄 */
#define G_NEW 0		 /* created in current cycle */
#define G_SURVIVAL 1 /* created in previous cycle */
#define G_OLD0 2	 /* marked old by frw. barrier in this cycle */
#define G_OLD1 3	 /* first full cycle as old */
#define G_OLD 4		 /* really old object (not to be visited) */
#define G_TOUCHED1 5 /* old object touched this cycle */
#define G_TOUCHED2 6 /* old object touched in previous cycle */

#define AGEBITS 7 /* all age bits (111) */

/*
** 说明注释:
** - 年龄编码占用低 3 位（AGEBITS=0b111）。
** - G_NEW / G_SURVIVAL 表示“年轻对象”。
** - G_OLD0 / G_OLD1 / G_OLD 表示“老对象”的不同阶段。
** - G_TOUCHED1 / G_TOUCHED2 与后向屏障相关，表示老对象被“触碰”。
*/

#define getage(o) ((o)->marked & AGEBITS)
#define setage(o, a) ((o)->marked = cast_byte(((o)->marked & (~AGEBITS)) | a))
#define isold(o) (getage(o) > G_SURVIVAL)

/*
** 说明注释:
** - getage/setage 通过位掩码读取/写入年龄字段。
** - isold 判断对象是否进入“老代”（年龄 > SURVIVAL）。
*/

/*
** In generational mode, objects are created 'new'. After surviving one
** cycle, they become 'survival'. Both 'new' and 'survival' can point
** to any other object, as they are traversed at the end of the cycle.
** We call them both 'young' objects.
** If a survival object survives another cycle, it becomes 'old1'.
** 'old1' objects can still point to survival objects (but not to
** new objects), so they still must be traversed. After another cycle
** (that, being old, 'old1' objects will "survive" no matter what)
** finally the 'old1' object becomes really 'old', and then they
** are no more traversed.
**
** To keep its invariants, the generational mode uses the same barriers
** also used by the incremental mode. If a young object is caught in a
** forward barrier, it cannot become old immediately, because it can
** still point to other young objects. Instead, it becomes 'old0',
** which in the next cycle becomes 'old1'. So, 'old0' objects is
** old but can point to new and survival objects; 'old1' is old
** but cannot point to new objects; and 'old' cannot point to any
** young object.
**
** If any old object ('old0', 'old1', 'old') is caught in a back
** barrier, it becomes 'touched1' and goes into a gray list, to be
** visited at the end of the cycle.  There it evolves to 'touched2',
** which can point to survivals but not to new objects. In yet another
** cycle then it becomes 'old' again.
**
** The generational mode must also control the colors of objects,
** because of the barriers.  While the mutator is running, young objects
** are kept white. 'old', 'old1', and 'touched2' objects are kept black,
** as they cannot point to new objects; exceptions are threads and open
** upvalues, which age to 'old1' and 'old' but are kept gray. 'old0'
** objects may be gray or black, as in the incremental mode. 'touched1'
** objects are kept gray, as they must be visited again at the end of
** the cycle.
**
** 分代模式下，对象创建时为 'new'。经历一次回收仍存活后变为 'survival'。
** 'new' 与 'survival' 都可能指向任何对象，因为它们会在周期末被遍历；
** 我们称它们为“年轻对象”。如果 'survival' 再次存活，则变为 'old1'。
** 'old1' 仍可指向 'survival'（但不能指向 'new'），因此仍需遍历。
** 再经历一个周期（作为老对象，'old1' 无论如何都会“存活”）后，
** 最终变为真正的 'old'，不再被遍历。
**
** 为保持不变式，分代模式使用与增量模式相同的写屏障。
** 若年轻对象触发前向屏障，它不能立即变为老对象，因为仍可能指向
** 其他年轻对象。于是它变为 'old0'，下一个周期变为 'old1'。
** 因此：'old0' 是老对象但可指向新/存活对象；'old1' 是老对象但不能指向新对象；
** 'old' 不能指向任何年轻对象。
**
** 若任何老对象（'old0'/'old1'/'old'）触发后向屏障，它会变为 'touched1'，
** 并进入灰链表以便在周期末再次访问。之后演化为 'touched2'，
** 它可指向 'survival' 但不能指向 'new'。再过一个周期，它重新成为 'old'。
**
** 分代模式还必须控制对象颜色（因写屏障需要）。
** 在程序运行时，年轻对象保持白色。
** 'old'、'old1'、'touched2' 保持黑色（因为它们不能指向新对象）。
** 例外：线程与开放上值会老化到 'old1'/'old' 但保持灰色。
** 'old0' 可为灰或黑（类似增量模式）。
** 'touched1' 保持灰色，因为必须在周期末再次访问。
**
** 说明注释:
** - 这一大段注释是理解 Lua 分代 GC 的核心说明。
** - “前向屏障/后向屏障”在 Lua 中分别对应不同写屏障策略。
** - 颜色与年龄是两套概念，但在同一 marked 字节中编码。
*/

/*
** {======================================================
** Default Values for GC parameters
** =======================================================
**
** {======================================================
** GC 参数的默认值
** =======================================================
*/

/*
** Minor collections will shift to major ones after LUAI_MINORMAJOR%
** bytes become old.
**
** 当有 LUAI_MINORMAJOR% 的字节变为“老对象”后，小回收会转为大回收。
*/
#define LUAI_MINORMAJOR 70

/*
** Major collections will shift to minor ones after a collection
** collects at least LUAI_MAJORMINOR% of the new bytes.
**
** 大回收在一次回收中若至少回收 LUAI_MAJORMINOR% 的新字节，
** 则会转回小回收模式。
*/
#define LUAI_MAJORMINOR 50

/*
** A young (minor) collection will run after creating LUAI_GENMINORMUL%
** new bytes.
**
** 当创建了 LUAI_GENMINORMUL% 的新字节后，会触发一次年轻（小）回收。
*/
#define LUAI_GENMINORMUL 20

/* incremental */
/* 增量模式 */

/* Number of bytes must be LUAI_GCPAUSE% before starting new cycle */
/* 在开始新的回收周期前，内存字节数需达到 LUAI_GCPAUSE% */
#define LUAI_GCPAUSE 250

/*
** Step multiplier: The collector handles LUAI_GCMUL% work units for
** each new allocated word. (Each "work unit" corresponds roughly to
** sweeping one object or traversing one slot.)
**
** 步进倍率：对每个新分配的字，回收器处理 LUAI_GCMUL% 个“工作单元”。
** （每个“工作单元”大致相当于清扫一个对象或遍历一个槽位。）
*/
#define LUAI_GCMUL 200

/* How many bytes to allocate before next GC step */
/* 在下一次 GC 步进前可分配的字节数 */
#define LUAI_GCSTEPSIZE (200 * sizeof(Table))

#define setgcparam(g, p, v) (g->gcparams[LUA_GCP##p] = luaO_codeparam(v))
#define applygcparam(g, p, x) luaO_applyparam(g->gcparams[LUA_GCP##p], x)

/*
** 说明注释:
** - setgcparam 使用“记号拼接”##p 生成枚举名 LUA_GCPp。
** - luaO_codeparam/luaO_applyparam 是 Lua 内部对参数的编码/应用函数。
** - 这里涉及宏技巧，便于统一设置与读取 GC 参数。
*/

/* }====================================================== */

/*
** Control when GC is running:
**
** 控制 GC 何时运行:
*/
#define GCSTPUSR 1 /* bit true when GC stopped by user */
#define GCSTPGC 2  /* bit true when GC stopped by itself */
#define GCSTPCLS 4 /* bit true when closing Lua state */
#define gcrunning(g) ((g)->gcstp == 0)

/*
** 说明注释:
** - gcstp 是一个位字段，多个原因可同时“停止 GC”。
** - gcrunning 仅在无任何停止位时为真。
*/

/*
** Does one step of collection when debt becomes zero. 'pre'/'pos'
** allows some adjustments to be done only when needed. macro
** 'condchangemem' is used only for heavy tests (forcing a full
** GC cycle on every opportunity)
**
** 当债务（GCdebt）为零时执行一次回收步进。'pre'/'pos'
** 允许在需要时执行一些前后调整。宏 'condchangemem' 仅用于
** 重度测试（在每个机会强制一次完整 GC 周期）。
**
** 说明注释:
** - GCdebt 是 Lua 的“分配债务”概念，用于驱动增量回收。
** - 这里用宏包裹，以避免在热路径上产生函数调用开销。
*/

#if !defined(HARDMEMTESTS)
#define condchangemem(L, pre, pos, emg) ((void)0)
#else
#define condchangemem(L, pre, pos, emg) \
	{                                   \
		if (gcrunning(G(L)))            \
		{                               \
			pre;                        \
			luaC_fullgc(L, emg);        \
			pos;                        \
		}                               \
	}
#endif

/*
** 说明注释:
** - HARDMEMTESTS 是编译期开关，开启后会极端地触发 GC 测试。
** - condchangemem 在正常编译下为空操作。
*/

#define luaC_condGC(L, pre, pos)       \
	{                                  \
		if (G(L)->GCdebt <= 0)         \
		{                              \
			pre;                       \
			luaC_step(L);              \
			pos;                       \
		};                             \
		condchangemem(L, pre, pos, 0); \
	}

/*
** 说明注释:
** - 若 GCdebt <= 0，执行一次增量步骤。
** - pre/pos 用于在真正执行 GC 时才做额外工作（如栈修正）。
** - condchangemem 只在测试模式下触发完整 GC。
*/

/* more often than not, 'pre'/'pos' are empty */
/* 通常情况下，'pre'/'pos' 为空 */
#define luaC_checkGC(L) luaC_condGC(L, (void)0, (void)0)

#define luaC_objbarrier(L, p, o) ( \
	(isblack(p) && iswhite(o)) ? luaC_barrier_(L, obj2gco(p), obj2gco(o)) : cast_void(0))

#define luaC_barrier(L, p, v) ( \
	iscollectable(v) ? luaC_objbarrier(L, p, gcvalue(v)) : cast_void(0))

#define luaC_objbarrierback(L, p, o) ( \
	(isblack(p) && iswhite(o)) ? luaC_barrierback_(L, p) : cast_void(0))

#define luaC_barrierback(L, p, v) ( \
	iscollectable(v) ? luaC_objbarrierback(L, p, gcvalue(v)) : cast_void(0))

/*
** 说明注释:
** - 这些宏实现“写屏障”（barrier），用于维护三色不变式。
** - luaC_barrier: 处理“前向屏障”，当黑对象指向白对象时触发。
** - luaC_barrierback: 处理“后向屏障”，通常在老对象被写入新引用时触发。
** - obj2gco/gcvalue/iscollectable 是 Lua 内部对象系统的宏/函数。
** - cast_void(0) 用于在条件不满足时消除“未使用结果”警告。
*/

LUAI_FUNC void luaC_fix(lua_State *L, GCObject *o);
LUAI_FUNC void luaC_freeallobjects(lua_State *L);
LUAI_FUNC void luaC_step(lua_State *L);
LUAI_FUNC void luaC_runtilstate(lua_State *L, int state, int fast);
LUAI_FUNC void luaC_fullgc(lua_State *L, int isemergency);
LUAI_FUNC GCObject *luaC_newobj(lua_State *L, lu_byte tt, size_t sz);
LUAI_FUNC GCObject *luaC_newobjdt(lua_State *L, lu_byte tt, size_t sz,
								  size_t offset);
LUAI_FUNC void luaC_barrier_(lua_State *L, GCObject *o, GCObject *v);
LUAI_FUNC void luaC_barrierback_(lua_State *L, GCObject *o);
LUAI_FUNC void luaC_checkfinalizer(lua_State *L, GCObject *o, Table *mt);
LUAI_FUNC void luaC_changemode(lua_State *L, int newmode);

/*
** 说明注释:
** - 这些是 GC 的对外/跨模块接口声明，实际实现在 lgc.c 等文件中。
** - luaC_newobj/luaC_newobjdt: 创建新 GC 对象（含可变头部偏移版本）。
** - luaC_step/luaC_fullgc: 增量一步 / 完整一次 GC。
** - luaC_fix: 将对象固定（不被回收，常用于常量/全局对象）。
** - luaC_checkfinalizer: 检查元表中是否有 __gc 并登记。
** - luaC_changemode: 在增量/分代模式之间切换。
*/

#endif

/*
** 【文件总结】
** 1) 本文件定义了 Lua GC 的关键常量、位布局、与多种宏工具。
** 2) 通过颜色（白/灰/黑）与年龄（G_NEW~G_TOUCHED2）共同描述对象状态。
** 3) keepinvariant 与写屏障宏确保“黑不指白”的核心不变式。
** 4) 默认 GC 参数控制触发频率与回收力度，分别适配分代与增量模式。
** 5) 对外暴露的函数声明是 GC 子系统的入口，真正逻辑在实现文件中。
**
** 阅读建议:
** - 先掌握“三色标记法”与“写屏障”概念，再对照此文件的宏理解其实现意图。
** - 结合 lgc.c 阅读，可看到这些宏如何在实际逻辑中被调用。
*/
