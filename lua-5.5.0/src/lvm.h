/*
** 文件概要：
** 这个文件是Lua虚拟机的头文件 lvm.h。
** 它定义了Lua VM（虚拟机）中用于值操作、类型转换、比较、数学运算等的核心宏和函数原型。
** 该文件是Lua解释器的一部分，主要处理Lua值的各种操作，如类型转换（例如数字到字符串、浮点到整数）、对象比较、表（table）的快速访问和设置、算术运算（如除法、取模、移位）以及VM执行的核心函数。
** 文件中包含条件编译宏，用于控制某些行为的自定义（如数字到字符串的转换、浮点到整数的舍入模式）。
** 它依赖于其他头文件：ldo.h（用于调用栈管理）、lobject.h（Lua对象定义）、ltm.h（元方法定义）。
** 这个头文件不包含函数实现（那些在对应的.c文件中），而是提供声明和宏，帮助Lua VM高效处理指令和值。
** 阅读起点：从条件宏（如cvt2str、cvt2num）开始，了解类型转换机制；然后看转换函数如tonumber；接着是快速表操作宏；最后是函数原型，涵盖VM的核心操作。
*/

/*
** $Id: lvm.h $
** Lua virtual machine
** See Copyright Notice in lua.h
*/
// 原注释翻译：$Id: lvm.h $
// Lua虚拟机
// 请参阅lua.h中的版权声明

#ifndef lvm_h
#define lvm_h

#include "ldo.h"
#include "lobject.h"
#include "ltm.h"

#if !defined(LUA_NOCVTN2S)
#define cvt2str(o) ttisnumber(o)
#else
#define cvt2str(o) 0 /* no conversion from numbers to strings */
#endif

/*
** 添加的说明注释：
** 这个宏cvt2str用于检查一个Lua对象（TValue *o）是否可以转换为字符串。
** 如果未定义LUA_NOCVTN2S（默认情况），则如果o是数字类型（ttisnumber(o)返回true），则允许转换。
** ttisnumber是lobject.h中定义的宏，检查对象的类型标签是否为数字（浮点或整数）。
** 否则，如果定义了LUA_NOCVTN2S，则禁用从数字到字符串的转换，返回0。
** 这是一个条件编译宏，允许用户自定义Lua的行为，例如在某些嵌入式环境中禁用转换以节省空间。
** 涉及的C用法：条件编译#if !defined用于配置选项；ttisnumber是内联类型检查，效率高。
** 结合实际代码：这个宏在Lua中用于隐式转换，例如在字符串连接时，如果一个操作数是数字，它可以被转换为字符串。
*/

#if !defined(LUA_NOCVTS2N)
#define cvt2num(o) ttisstring(o)
#else
#define cvt2num(o) 0 /* no conversion from strings to numbers */
#endif

/*
** 添加的说明注释：
** 这个宏cvt2num用于检查一个Lua对象（TValue *o）是否可以转换为数字。
** 如果未定义LUA_NOCVTS2N（默认情况），则如果o是字符串类型（ttisstring(o)返回true），则允许转换。
** ttisstring是lobject.h中定义的宏，检查对象的类型标签是否为字符串。
** 否则，如果定义了LUA_NOCVTS2N，则禁用从字符串到数字的转换，返回0。
** 类似于cvt2str，这是为了自定义转换行为。
** 结合实际代码：用于Lua的隐式转换，例如在算术运算中，如果一个操作数是字符串，它可能被解析为数字。
** 涉及的C用法：类似条件编译，ttisstring是类型标签检查。
*/

/*
** You can define LUA_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
// 原注释翻译：
// 如果你想通过向下取整（flooring）将浮点数转换为整数（而不是在它们不是整数值时引发错误），你可以定义LUA_FLOORN2I
#if !defined(LUA_FLOORN2I)
#define LUA_FLOORN2I F2Ieq
#endif

/*
** 添加的说明注释：
** 这个宏LUA_FLOORN2I定义了浮点数到整数转换的默认模式。
** 如果未定义，它默认为F2Ieq模式（只接受精确整数值，否则错误）。
** 用户可以定义它为F2Ifloor来使用向下取整。
** F2Ieq、F2Ifloor等是下面定义的枚举值。
** 结合实际代码：这个宏在tointeger等函数中使用，控制转换行为。例如，在Lua脚本中将浮点数用作表索引时，会应用这个转换。
** 涉及的C用法：宏定义用于配置，允许在编译时自定义行为，而无需修改代码。
*/

/*
** Rounding modes for float->integer coercion
*/
// 原注释翻译：浮点数到整数强制转换的舍入模式
typedef enum
{
  F2Ieq,    /* no rounding; accepts only integral values */
  F2Ifloor, /* takes the floor of the number */
  F2Iceil   /* takes the ceiling of the number */
} F2Imod;

/*
** 添加的说明注释：
** 这个枚举F2Imod定义了浮点数转换为整数时的三种舍入模式：
** - F2Ieq：不进行舍入，只接受已经是整数的值（例如3.0可以，但3.1不行，会引发错误）。
** - F2Ifloor：向下取整（floor），例如3.9变成3。
** - F2Iceil：向上取整（ceil），例如3.1变成4。
** 这些模式用于luaV_flttointeger等函数中。
** 结合实际代码：枚举用于tointeger函数的参数mode，允许灵活控制转换。例如，在Lua的整数运算中，如果输入是浮点，会根据mode决定是否取整。
** 涉及的C用法：typedef enum是C的标准枚举类型，用于定义一组相关常量，提高代码可读性。
*/

/* convert an object to a float (including string coercion) */
// 原注释翻译：将对象转换为浮点数（包括字符串强制转换）
#define tonumber(o, n) \
  (ttisfloat(o) ? (*(n) = fltvalue(o), 1) : luaV_tonumber_(o, n))

/*
** 添加的说明注释：
** 这个宏tonumber尝试将Lua对象o（TValue*）转换为lua_Number（双精度浮点），结果存入n（lua_Number*）。
** 首先检查如果o已经是浮点类型（ttisfloat(o)），则直接赋值fltvalue(o)到*n，并返回1（成功）。
** fltvalue是lobject.h中的宏，提取浮点值。
** 如果不是，则调用luaV_tonumber_函数进行转换（可能包括字符串解析）。
** 返回1表示成功，0表示失败。
** 结合实际代码：这个宏在Lua的算术运算中使用，例如加法时确保操作数是数字。如果o是字符串如"3.14"，luaV_tonumber_会尝试解析它。
** 涉及的C用法：条件表达式用于短路评估；宏中使用逗号运算符来执行赋值并返回1。
*/

/* convert an object to a float (without string coercion) */
// 原注释翻译：将对象转换为浮点数（不包括字符串强制转换）
#define tonumberns(o, n) \
  (ttisfloat(o) ? ((n) = fltvalue(o), 1) : (ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))

/*
** 添加的说明注释：
** 这个宏tonumberns类似于tonumber，但不尝试字符串到数字的转换。
** 首先检查如果是浮点（ttisfloat），直接赋值。
** 然后检查如果是整数（ttisinteger），则将整数值ivalue(o)转换为lua_Number（使用cast_num，可能是个宏或函数进行类型转换），并返回1。
** 否则返回0（失败）。
** 结合实际代码：用于需要严格数字类型而不允许字符串隐式转换的场景，例如某些内部检查。
** 涉及的C用法：嵌套条件表达式；cast_num可能是lua_Number的强制转换，处理整数到浮点的转换。
*/

/* convert an object to an integer (including string coercion) */
// 原注释翻译：将对象转换为整数（包括字符串强制转换）
#define tointeger(o, i)                             \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                            : luaV_tointeger(o, i, LUA_FLOORN2I))

/*
** 添加的说明注释：
** 这个宏tointeger尝试将对象o转换为lua_Integer（有符号64位整数），结果存入i（lua_Integer*）。
** 使用l_likely（一个优化宏，提示编译器这个分支更可能发生）检查如果是整数类型，直接赋值ivalue(o)到*i，返回1。
** 否则调用luaV_tointeger函数，可能涉及浮点取整或字符串解析，使用LUA_FLOORN2I作为默认模式。
** 结合实际代码：常用于Lua的位运算或表索引，其中需要整数。l_likely是Lua的优化，用于分支预测，提高性能。
** 涉及的C用法：l_likely可能是__builtin_expect的包装，用于GCC优化；宏中使用条件表达式。
*/

/* convert an object to an integer (without string coercion) */
// 原注释翻译：将对象转换为整数（不包括字符串强制转换）
#define tointegerns(o, i)                           \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                            : luaV_tointegerns(o, i, LUA_FLOORN2I))

/*
** 添加的说明注释：
** 类似于tointeger，但不尝试字符串转换。
** 如果是整数，直接赋值；否则调用luaV_tointegerns，可能只处理浮点到整数的转换。
** 结合实际代码：用于内部严格整数检查，避免字符串解析开销。
** 涉及的C用法：类似tointeger，使用l_likely优化。
*/

#define intop(op, v1, v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

/*
** 添加的说明注释：
** 这个宏intop用于对两个lua_Integer值v1和v2执行操作op（例如+、-等），处理符号问题。
** l_castS2U和l_castU2S可能是Lua的宏，用于将有符号整数转换为无符号，再操作，然后转回有符号。
** 这用于避免C中整数溢出的未定义行为，例如在移位或算术中。
** 结合实际代码：用于像加法、减法这样的整数操作，确保安全溢出处理。
** 涉及的C用法：宏用于类型转换，l_castS2U/l_castU2S可能是static_cast或类似，用于无符号运算以定义行为。
*/

#define luaV_rawequalobj(t1, t2) luaV_equalobj(NULL, t1, t2)

/*
** 添加的说明注释：
** 这个宏luaV_rawequalobj是luaV_equalobj的包装，将L设为NULL，表示“raw”相等检查（不调用元方法）。
** 用于直接比较两个TValue对象t1和t2，而不触发__eq元方法。
** 结合实际代码：在VM中用于快速相等检查，例如在if语句中。
** 涉及的C用法：宏简化函数调用。
*/

/*
** fast track for 'gettable'
*/
// 原注释翻译：'gettable'的快速路径
#define luaV_fastget(t, k, res, f, tag) \
  (tag = (!ttistable(t) ? LUA_VNOTABLE : f(hvalue(t), k, res)))

/*
** 添加的说明注释：
** 这个宏luaV_fastget用于表的快速get操作。
** 检查t是否是表（ttistable(t)），如果不是，设置tag为LUA_VNOTABLE（可能是枚举值表示不是表）。
** 如果是表，则调用函数f（通常是luaH_get等），传入表的hash值hvalue(t)、键k、结果res，并设置tag为f的返回值。
** 这是一个优化宏，避免不必要的检查。
** 结合实际代码：用于Lua的OP_GETTABLE指令，快速从表中获取值。如果不是表，会fallback到慢路径。
** 涉及的C用法：宏中使用条件表达式赋值；hvalue是lobject.h中的宏，提取表的指针。
*/

/*
** Special case of 'luaV_fastget' for integers, inlining the fast case
** of 'luaH_getint'.
*/
// 原注释翻译：'luaV_fastget'的特殊情况，用于整数，内联'luaH_getint'的快速情况。
#define luaV_fastgeti(t, k, res, tag)      \
  if (!ttistable(t))                       \
    tag = LUA_VNOTABLE;                    \
  else                                     \
  {                                        \
    luaH_fastgeti(hvalue(t), k, res, tag); \
  }

/*
** 添加的说明注释：
** 这个宏是luaV_fastget的变体，专门用于整数键k。
** 如果t不是表，设置tag为LUA_VNOTABLE。
** 否则，调用luaH_fastgeti（可能是luaH_getint的内联版本），传入表、k、res，并设置tag。
** 这进一步优化了整数索引的表访问，因为Lua表常用于数组。
** 结合实际代码：用于OP_GETTABUP等指令，当键是整数时。
** 涉及的C用法：宏中使用块语句{}来包含调用；luaH_fastgeti可能是内联函数，提高性能。
*/

#define luaV_fastset(t, k, val, hres, f) \
  (hres = (!ttistable(t) ? HNOTATABLE : f(hvalue(t), k, val)))

/*
** 添加的说明注释：
** 这个宏luaV_fastset用于表的快速set操作。
** 检查t是否是表，如果不是，设置hres为HNOTATABLE（可能是常量表示不是表）。
** 如果是，调用f（通常luaH_set等），传入表、k、val，并设置hres为f的返回值。
** 类似于fastget，用于优化表赋值。
** 结合实际代码：用于OP_SETTABLE指令的快速路径。
** 涉及的C用法：类似fastget的条件表达式。
*/

#define luaV_fastseti(t, k, val, hres)      \
  if (!ttistable(t))                        \
    hres = HNOTATABLE;                      \
  else                                      \
  {                                         \
    luaH_fastseti(hvalue(t), k, val, hres); \
  }

/*
** 添加的说明注释：
** 这个宏是luaV_fastset的整数键变体。
** 如果不是表，设置hres为HNOTATABLE。
** 否则调用luaH_fastseti，优化整数键的设置。
** 结合实际代码：用于整数索引的表赋值优化。
** 涉及的C用法：类似fastgeti的块语句。
*/

/*
** Finish a fast set operation (when fast set succeeds).
*/
// 原注释翻译：完成快速设置操作（当快速设置成功时）。
#define luaV_finishfastset(L, t, v) luaC_barrierback(L, gcvalue(t), v)

/*
** 添加的说明注释：
** 这个宏luaV_finishfastset在快速设置成功后调用，用于垃圾回收屏障。
** 调用luaC_barrierback（ldo.h或gc相关），传入Lua状态L、表t的GC值gcvalue(t)、设置的值v。
** 这确保如果v是新对象，更新GC信息，防止在表中引用年轻对象时被错误回收。
** 结合实际代码：在fastset成功后调用，以维护GC一致性。
** 涉及的C用法：luaC_barrierback是库调用，处理Lua的增量GC；gcvalue是宏，提取对象的GC头。
*/

/*
** Shift right is the same as shift left with a negative 'y'
*/
// 原注释翻译：右移与左移相同，但'y'为负数
#define luaV_shiftr(x, y) luaV_shiftl(x, intop(-, 0, y))

/*
** 添加的说明注释：
** 这个宏luaV_shiftr实现整数右移，通过调用luaV_shiftl，但将y取负（使用intop(-, 0, y)计算- y）。
** intop是前面定义的宏，用于安全整数运算。
** 这统一了左右移的实现。
** 结合实际代码：用于Lua的位运算OP_SHRI，x右移y位。
** 涉及的C用法：宏嵌套调用，复用shiftl函数。
*/

LUAI_FUNC int luaV_equalobj(lua_State *L, const TValue *t1, const TValue *t2);
/*
** 添加的说明注释：
** 函数luaV_equalobj：比较两个TValue对象t1和t2是否相等。
** 如果需要，可能调用元方法__eq（通过L状态）。
** 返回1如果相等，0否则。
** 结合实际代码：用于Lua的==运算符。如果类型相同，直接比较值；否则检查元方法。
** 涉及的C用法：LUAI_FUNC是Lua的API宏，可能添加extern或 visibility 属性。
*/

LUAI_FUNC int luaV_lessthan(lua_State *L, const TValue *l, const TValue *r);
/*
** 添加的说明注释：
** 函数luaV_lessthan：比较l < r。
** 可能调用__lt元方法。
** 返回1如果l < r，0否则。
** 结合实际代码：用于<运算符，支持数字、字符串等，并处理元方法。
*/

LUAI_FUNC int luaV_lessequal(lua_State *L, const TValue *l, const TValue *r);
/*
** 添加的说明注释：
** 函数luaV_lessequal：比较l <= r。
** 可能调用__le元方法。
** 返回1如果l <= r，0否则。
** 结合实际代码：用于<=运算符，类似lessthan。
*/

LUAI_FUNC int luaV_tonumber_(const TValue *obj, lua_Number *n);
/*
** 添加的说明注释：
** 函数luaV_tonumber_：将obj转换为lua_Number，存入n。
** 处理字符串解析等。
** 返回1成功，0失败。
** 结合实际代码：tonumber宏的fallback，用于非浮点对象。
*/

LUAI_FUNC int luaV_tointeger(const TValue *obj, lua_Integer *p, F2Imod mode);
/*
** 添加的说明注释：
** 函数luaV_tointeger：将obj转换为lua_Integer，根据mode取整。
** 处理浮点和字符串。
** 返回1成功，0失败。
** 结合实际代码：tointeger宏的fallback。
*/

LUAI_FUNC int luaV_tointegerns(const TValue *obj, lua_Integer *p,
                               F2Imod mode);
/*
** 添加的说明注释：
** 函数luaV_tointegerns：类似tointeger，但无字符串转换。
** 只处理数字类型。
** 结合实际代码：tointegerns宏的fallback。
*/

LUAI_FUNC int luaV_flttointeger(lua_Number n, lua_Integer *p, F2Imod mode);
/*
** 添加的说明注释：
** 函数luaV_flttointeger：将浮点n转换为整数p，根据mode。
** 处理取整逻辑。
** 返回1如果成功（在整数范围内），0否则。
** 结合实际代码：用于浮点到整数的低级转换，可能检查溢出。
*/

LUAI_FUNC lu_byte luaV_finishget(lua_State *L, const TValue *t, TValue *key,
                                 StkId val, lu_byte tag);
/*
** 添加的说明注释：
** 函数luaV_finishget：完成表的get操作。
** 如果fastget失败（tag != LUA_VNOTABLE），处理慢路径，如元方法__index。
** 将结果放入val（栈位置）。
** 返回tag或结果类型。
** 结合实际代码：fastget的fallback，用于非表或哈希未命中。
** 涉及的C用法：lu_byte是无符号char，用于小整数。
*/

LUAI_FUNC void luaV_finishset(lua_State *L, const TValue *t, TValue *key,
                              TValue *val, int aux);
/*
** 添加的说明注释：
** 函数luaV_finishset：完成表的set操作。
** 如果fastset失败，处理慢路径，如__newindex元方法。
** aux可能是辅助参数，如哈希结果。
** 结合实际代码：fastset的fallback。
*/

LUAI_FUNC void luaV_finishOp(lua_State *L);
/*
** 添加的说明注释：
** 函数luaV_finishOp：完成当前VM操作。
** 可能处理元方法调用后的清理或结果设置。
** 结合实际代码：在VM循环中，用于操作结束。
*/

LUAI_FUNC void luaV_execute(lua_State *L, CallInfo *ci);
/*
** 添加的说明注释：
** 函数luaV_execute：Lua VM的主执行函数。
** 执行当前调用信息ci中的字节码。
** 处理指令分发、栈操作等。
** 结合实际代码：这是Lua解释器的核心循环，解释Proto中的指令。
** 涉及的C用法：CallInfo是ldo.h中的结构，管理调用栈。
*/

LUAI_FUNC void luaV_concat(lua_State *L, int total);
/*
** 添加的说明注释：
** 函数luaV_concat：连接total个值（在栈上）。
** 处理字符串连接，可能调用__concat元方法。
** 结果推入栈。
** 结合实际代码：用于..运算符，支持多个操作数。
*/

LUAI_FUNC lua_Integer luaV_idiv(lua_State *L, lua_Integer x, lua_Integer y);
/*
** 添加的说明注释：
** 函数luaV_idiv：整数除法x // y。
** 处理除零等错误，通过L抛出异常。
** 返回商。
** 结合实际代码：用于//运算符，确保向零取整或Lua的语义。
*/

LUAI_FUNC lua_Integer luaV_mod(lua_State *L, lua_Integer x, lua_Integer y);
/*
** 添加的说明注释：
** 函数luaV_mod：整数取模x % y。
** 处理负数和除零。
** 返回余数。
** 结合实际代码：用于%运算符，Lua的模运算定义为floor除法。
*/

LUAI_FUNC lua_Number luaV_modf(lua_State *L, lua_Number x, lua_Number y);
/*
** 添加的说明注释：
** 函数luaV_modf：浮点取模x % y。
** 类似mod，但用于浮点。
** 结合实际代码：浮点版本的%运算符。
*/

LUAI_FUNC lua_Integer luaV_shiftl(lua_Integer x, lua_Integer y);
/*
** 添加的说明注释：
** 函数luaV_shiftl：左移x << y。
** 如果y负，则右移。
** 处理移位边界。
** 结合实际代码：用于<<运算符，支持负移位作为右移。
*/

LUAI_FUNC void luaV_objlen(lua_State *L, StkId ra, const TValue *rb);
/*
** 添加的说明注释：
** 函数luaV_objlen：计算对象rb的长度（#运算符）。
** 将结果放入ra（栈位置）。
** 处理表、字符串等，可能调用__len元方法。
** 结合实际代码：用于len操作，支持不同类型。
*/

#endif

/*
** 详细的总结说明部分：
** 这个文件lvm.h是Lua虚拟机的基础头文件，提供了值操作的核心工具。
** 主要内容分为几部分：
** 1. 类型转换宏（如cvt2str, tonumber, tointeger）：处理Lua值的隐式和显式转换，支持自定义行为通过条件编译。
** 2. 舍入模式枚举F2Imod和相关宏：控制浮点到整数的转换策略，允许用户选择错误处理或取整。
** 3. 快速表访问宏（如luaV_fastget, luaV_fastset）：优化表的get/set操作，特别是整数键，减少开销。
** 4. 运算宏（如intop, luaV_shiftr）：安全处理整数运算和移位，避免C未定义行为。
** 5. 函数原型：覆盖比较（equalobj, lessthan）、转换（tonumber_, tointeger）、VM执行（execute, finishOp）、数学运算（idiv, mod, shiftl）、连接（concat）和长度（objlen）。
** 这些函数和宏是Lua VM高效运行的基础，确保类型安全、性能优化和元方法支持。
** 在实际使用中，这个文件与lvm.c结合，实现Lua字节码的解释。
** 如果你不熟悉C，注意宏的广泛使用（用于内联和优化），以及LUAI_FUNC这样的API宏（用于跨平台可见性）。
** 建议结合Lua源代码的lvm.c文件阅读，以查看这些函数的实现细节。
** 如果有特定部分需要更深入解释，请提供反馈。
*/
