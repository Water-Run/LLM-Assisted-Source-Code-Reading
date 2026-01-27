/*
** $Id: ltm.h $
** Tag methods (标签方法/元方法)
** See Copyright Notice in lua.h (参见lua.h中的版权声明)
*/

/*
================================================================================
【文件概要】ltm.h - Lua标签方法(Tag Methods)头文件

* 文件功能：
  这是Lua虚拟机中处理元方法(metamethods)的核心头文件。元方法是Lua实现面向对象
  和运算符重载的基础机制。

* 主要内容：
  1. TMS枚举：定义了所有元方法的类型(如__index, __add等)
  2. 元方法访问宏：提供快速访问元方法的宏定义
  3. 元方法调用函数：声明了各种元方法调用的函数接口

* 核心概念：
  - 元表(metatable)：附加在Lua值上的特殊表，包含元方法
  - 元方法(metamethod)：特殊的函数，用于定义值的特殊行为
  - 标签方法(tag method)：元方法的旧称，在Lua内部仍使用此术语

* 设计要点：
  - 使用位掩码优化元方法的存在性检查
  - 将常用元方法(TM_INDEX到TM_EQ)设计为快速访问
  - 提供多种元方法调用接口以适应不同场景

* 与其他模块的关系：
  - 依赖lobject.h：使用Lua对象的基本定义
  - 被lvm.c使用：虚拟机执行时调用元方法
  - 被lapi.c使用：API层面的元方法操作
================================================================================
*/

#ifndef ltm_h
#define ltm_h

#include "lobject.h" /* 包含Lua对象定义头文件，提供TValue, Table等类型 */

/*
** 警告：如果你改变了这个枚举的顺序，
** 请搜索 "ORDER TM" 和 "ORDER OP"
**
** 说明：这个枚举定义了Lua中所有支持的元方法类型。
** 枚举的顺序非常重要，因为：
** 1. 前6个(到TM_EQ)是"快速访问"元方法，在元表的flags字段中有对应的位
** 2. 后续的元方法按功能分组：算术运算、位运算、比较运算等
** 3. 某些Lua代码依赖这个顺序进行优化
*/
typedef enum
{
  TM_INDEX,    /* __index元方法：用于表的索引访问(table[key]读取) */
  TM_NEWINDEX, /* __newindex元方法：用于表的索引赋值(table[key]=value) */
  TM_GC,       /* __gc元方法：垃圾回收时调用的析构函数 */
  TM_MODE,     /* __mode元方法：定义弱表的模式("k"弱键, "v"弱值, "kv"都弱) */
  TM_LEN,      /* __len元方法：长度运算符#的行为 */
  TM_EQ,       /* __eq元方法：相等比较==的行为；这是最后一个快速访问的元方法 */

  /* 以下是算术运算元方法 */
  TM_ADD,  /* __add元方法：加法运算+ */
  TM_SUB,  /* __sub元方法：减法运算- */
  TM_MUL,  /* __mul元方法：乘法运算* */
  TM_MOD,  /* __mod元方法：取模运算% */
  TM_POW,  /* __pow元方法：幂运算^ */
  TM_DIV,  /* __div元方法：浮点除法/ */
  TM_IDIV, /* __idiv元方法：整数除法// */

  /* 以下是位运算元方法 */
  TM_BAND, /* __band元方法：按位与& */
  TM_BOR,  /* __bor元方法：按位或| */
  TM_BXOR, /* __bxor元方法：按位异或~ */
  TM_SHL,  /* __shl元方法：左移<< */
  TM_SHR,  /* __shr元方法：右移>> */

  /* 以下是一元运算元方法 */
  TM_UNM,  /* __unm元方法：一元取负运算- */
  TM_BNOT, /* __bnot元方法：按位取反~ */

  /* 以下是比较运算元方法 */
  TM_LT, /* __lt元方法：小于比较< */
  TM_LE, /* __le元方法：小于等于比较<= */

  /* 以下是其他特殊元方法 */
  TM_CONCAT, /* __concat元方法：连接运算.. */
  TM_CALL,   /* __call元方法：函数调用() */
  TM_CLOSE,  /* __close元方法：to-be-closed变量关闭时调用 */

  TM_N /* 枚举元素的总数，用于数组大小等 */
} TMS;

/*
** 所有快速访问方法的掩码(第0-5位为1)。
** 元表的flags字段中，如果某个位为1，表示该元表没有对应的元方法字段。
** (flags的第6位表示表正在使用dummy节点; 第7位用于'isrealasize'标志)
**
** 技术细节：
** - ~0u 创建一个所有位都是1的无符号整数
** - ~0u << (TM_EQ + 1) 将低(TM_EQ+1)位清零，高位保持为1
** - ~(~0u << (TM_EQ + 1)) 取反后，低(TM_EQ+1)位为1，高位为0
** - cast_byte 强制转换为字节类型
**
** 例如：如果TM_EQ=5，则maskflags = 0b00111111 (低6位为1)
** 这样flags中对应位为1就表示"没有这个元方法"，可以跳过查找
*/
#define maskflags cast_byte(~(~0u << (TM_EQ + 1)))

/*
** 测试是否没有元方法。
** (因为元方法使用原始访问，结果可能是一个"空"的nil值)
**
** 说明：ttisnil是定义在lobject.h中的宏，用于检查TValue是否为nil类型
** 这里检查元方法tm是否为nil，nil表示该元方法不存在
*/
#define notm(tm) ttisnil(tm)

/*
** 检查元表中是否没有特定元方法e
**
** 参数说明：
** - mt: 元表指针，可能为NULL
** - e: 元方法的枚举值(TMS类型)
**
** 工作原理：
** 1. 如果mt为NULL，肯定没有元方法，返回true
** 2. 否则检查mt->flags的第e位，如果为1表示没有该元方法
**
** 优化目的：避免在表中查找已知不存在的元方法，提升性能
*/
#define checknoTM(mt, e) ((mt) == NULL || (mt)->flags & (1u << (e)))

/*
** 从全局状态g中快速获取元方法
**
** 参数说明：
** - g: 全局状态指针(global_State)
** - mt: 元表指针
** - e: 元方法枚举值
**
** 工作流程：
** 1. 首先用checknoTM检查元方法是否存在
** 2. 如果不存在直接返回NULL，避免昂贵的表查找
** 3. 如果可能存在，调用luaT_gettm从元表中获取
**    - g->tmname[e] 是预先创建的元方法名称字符串(如"__index")
**
** 性能优化：通过flags快速路径，大多数情况避免表查找
*/
#define gfasttm(g, mt, e) \
  (checknoTM(mt, e) ? NULL : luaT_gettm(mt, e, (g)->tmname[e]))

/*
** 从Lua状态L中快速获取元方法
**
** 说明：这是gfasttm的便捷版本
** G(l)是一个宏，从lua_State中提取global_State
** 然后调用gfasttm完成实际工作
*/
#define fasttm(l, mt, e) gfasttm(G(l), mt, e)

/*
** 获取类型名称
**
** 说明：
** - luaT_typenames_是一个字符串数组，存储所有Lua类型的名称
** - 数组索引从-1开始(LUA_TNONE=-1)，所以需要+1调整
** - 返回如"nil", "number", "string", "table"等类型名称字符串
*/
#define ttypename(x) luaT_typenames_[(x) + 1]

/*
** LUAI_DDEC宏用于声明数据
** const char *const 表示：
** - 第一个const: 字符串内容不可修改
** - 第二个const: 指针本身不可修改
**
** 这是一个全局常量数组，包含所有Lua类型的名称
** LUA_TOTALTYPES是Lua中类型的总数
*/
LUAI_DDEC(const char *const luaT_typenames_[LUA_TOTALTYPES];)

/*
** 获取对象o的类型名称
**
** 参数：
** - L: Lua状态机
** - o: 要查询类型的值(TValue指针)
**
** 返回：表示类型名称的C字符串
**
** 说明：这个函数比ttypename更智能，对于userdata可能返回元表的__name字段
*/
LUAI_FUNC const char *luaT_objtypename(lua_State *L, const TValue *o);

/*
** 从元表events中获取特定的元方法
**
** 参数：
** - events: 元表(Table类型指针)
** - event: 要获取的元方法类型(TMS枚举)
** - ename: 元方法的名称(TString类型，如"__index")
**
** 返回：元方法对应的TValue指针，如果不存在返回nil值
**
** 说明：这是元方法查找的核心函数，在表中按名称查找元方法
*/
LUAI_FUNC const TValue *luaT_gettm(Table *events, TMS event, TString *ename);

/*
** 根据对象o获取其对应的元方法
**
** 参数：
** - L: Lua状态机
** - o: 对象值
** - event: 要获取的元方法类型
**
** 工作流程：
** 1. 根据o的类型找到对应的元表
** 2. 从元表中查找event对应的元方法
**
** 说明：这是更高层的接口，自动处理获取对象元表的逻辑
*/
LUAI_FUNC const TValue *luaT_gettmbyobj(lua_State *L, const TValue *o,
                                        TMS event);

/*
** 初始化标签方法系统
**
** 参数：
** - L: Lua状态机
**
** 说明：
** 这个函数在Lua虚拟机初始化时调用，主要工作：
** 1. 创建所有元方法名称的字符串(如"__index", "__add"等)
** 2. 将这些字符串存储在global_State的tmname数组中
** 3. 初始化类型名称数组luaT_typenames_
*/
LUAI_FUNC void luaT_init(lua_State *L);

/*
** 调用元方法(无返回值版本)
**
** 参数：
** - L: Lua状态机
** - f: 要调用的元方法函数
** - p1, p2, p3: 传递给元方法的参数
**
** 说明：
** 用于调用不需要返回值的元方法，如__gc, __close等
** 函数会设置调用栈，执行元方法，然后清理栈
*/
LUAI_FUNC void luaT_callTM(lua_State *L, const TValue *f, const TValue *p1,
                           const TValue *p2, const TValue *p3);

/*
** 调用元方法并返回结果
**
** 参数：
** - L: Lua状态机
** - f: 要调用的元方法函数
** - p1, p2: 传递给元方法的前两个参数
** - p3: 栈上的位置，用于存放结果
**
** 返回：lu_byte类型，可能表示调用的状态或结果类型
**
** 说明：用于需要返回值的元方法调用
*/
LUAI_FUNC lu_byte luaT_callTMres(lua_State *L, const TValue *f,
                                 const TValue *p1, const TValue *p2, StkId p3);

/*
** 尝试调用二元运算的元方法
**
** 参数：
** - L: Lua状态机
** - p1, p2: 运算的两个操作数
** - res: 栈上存放结果的位置(StkId是栈索引类型)
** - event: 要尝试的元方法(如TM_ADD, TM_SUB等)
**
** 工作流程：
** 1. 检查p1或p2是否有对应的元方法
** 2. 如果有，调用元方法并将结果放入res
** 3. 如果没有，触发错误
**
** 应用场景：处理加减乘除等二元运算符
*/
LUAI_FUNC void luaT_trybinTM(lua_State *L, const TValue *p1, const TValue *p2,
                             StkId res, TMS event);

/*
** 尝试调用连接运算的元方法
**
** 参数：
** - L: Lua状态机
**
** 说明：
** 处理字符串连接运算符".."
** 操作数从栈顶获取，这是为什么不需要显式参数
** 连接操作可能涉及多个值，所以需要特殊处理
*/
LUAI_FUNC void luaT_tryconcatTM(lua_State *L);

/*
** 尝试调用结合律二元运算的元方法
**
** 参数：
** - L: Lua状态机
** - p1, p2: 操作数
** - inv: 是否反转操作数顺序(inverse)
** - res: 结果存放位置
** - event: 元方法类型
**
** 说明：
** 用于支持交换律的运算(如a+b和b+a)
** 如果p1没有元方法但p2有，可以通过inv参数反转操作数
** 这样可以让b的元方法处理操作
*/
LUAI_FUNC void luaT_trybinassocTM(lua_State *L, const TValue *p1,
                                  const TValue *p2, int inv, StkId res, TMS event);

/*
** 尝试调用整数二元运算的元方法
**
** 参数：
** - L: Lua状态机
** - p1: 第一个操作数
** - i2: 第二个操作数(lua_Integer类型，整数)
** - inv: 是否反转操作数
** - res: 结果位置
** - event: 元方法类型
**
** 说明：
** 优化版本，当第二个操作数是整数时使用
** 避免将整数包装成TValue的开销
*/
LUAI_FUNC void luaT_trybiniTM(lua_State *L, const TValue *p1, lua_Integer i2,
                              int inv, StkId res, TMS event);

/*
** 调用比较运算的元方法
**
** 参数：
** - L: Lua状态机
** - p1, p2: 要比较的两个值
** - event: 比较类型(TM_LT或TM_LE)
**
** 返回：int类型，比较结果(0或1)
**
** 说明：
** 用于处理 < 和 <= 运算符
** 如果没有对应元方法，会尝试其他元方法或触发错误
** 例如：a<=b可以通过not(b<a)实现
*/
LUAI_FUNC int luaT_callorderTM(lua_State *L, const TValue *p1,
                               const TValue *p2, TMS event);

/*
** 调用与整数比较的元方法
**
** 参数：
** - L: Lua状态机
** - p1: 第一个操作数
** - v2: 第二个操作数(整数值)
** - inv: 是否反转比较
** - isfloat: v2是否应被视为浮点数
** - event: 比较类型
**
** 返回：int类型，比较结果
**
** 说明：优化版本，用于值与整数常量的比较
*/
LUAI_FUNC int luaT_callorderiTM(lua_State *L, const TValue *p1, int v2,
                                int inv, int isfloat, TMS event);

/*
** 调整可变参数
**
** 参数：
** - L: Lua状态机
** - ci: 调用信息(CallInfo结构，包含函数调用的上下文)
** - p: 函数原型(Proto结构，包含函数的字节码和元数据)
**
** 说明：
** 当函数使用可变参数(...)时调用
** 调整栈以正确设置可变参数区域
** 处理固定参数和可变参数之间的布局
*/
LUAI_FUNC void luaT_adjustvarargs(lua_State *L, struct CallInfo *ci,
                                  const Proto *p);

/*
** 获取单个可变参数
**
** 参数：
** - ci: 调用信息
** - ra: 目标栈位置
** - rc: 参数索引
**
** 说明：
** 用于select(n, ...)操作
** 从可变参数列表中提取特定索引的参数
*/
LUAI_FUNC void luaT_getvararg(CallInfo *ci, StkId ra, TValue *rc);

/*
** 获取多个可变参数
**
** 参数：
** - L: Lua状态机
** - ci: 调用信息
** - where: 栈上存放结果的起始位置
** - wanted: 期望获取的参数数量(-1表示所有参数)
** - vatab: 可变参数表(如果需要打包成表)
**
** 说明：
** 用于处理函数中的...表达式
** 将可变参数复制到指定的栈位置
** 如果wanted=-1，复制所有可变参数
*/
LUAI_FUNC void luaT_getvarargs(lua_State *L, struct CallInfo *ci, StkId where,
                               int wanted, int vatab);

#endif

/*
================================================================================
【文件总结】ltm.h - Lua元方法系统总结

## 一、核心功能
这个头文件定义了Lua元方法(metamethod)系统的所有接口。元方法是Lua实现动态行为
定制的关键机制，允许用户自定义表、userdata等类型的运算符行为。

## 二、关键数据结构

1. **TMS枚举**
   - 定义了25种元方法类型(TM_N=25)
   - 前6种(TM_INDEX到TM_EQ)是快速访问元方法
   - 包括：索引访问、算术运算、位运算、比较运算、特殊操作

2. **元方法分类**
   - 访问控制：__index, __newindex
   - 生命周期：__gc, __close
   - 运算符：算术(7种)、位运算(5种)、比较(2种)、一元(2种)
   - 特殊：__len, __concat, __call, __mode

## 三、优化机制

1. **快速路径优化**
   - 使用flags字段缓存元方法存在性
   - checknoTM宏实现O(1)的元方法检查
   - 避免不必要的表查找，提升90%以上的常见情况性能

2. **位掩码技术**
   - maskflags定义快速访问元方法的位掩码
   - 每个元方法占用flags中的一位
   - 空间高效：用8位表示6个元方法的状态

## 四、函数接口设计

1. **查询函数**
   - luaT_gettm: 底层元方法查找
   - luaT_gettmbyobj: 高层对象元方法查找
   - 提供两级接口满足不同需求

2. **调用函数**
   - luaT_callTM: 无返回值调用
   - luaT_callTMres: 有返回值调用
   - 适应不同元方法的语义需求

3. **运算辅助函数**
   - trybinTM: 标准二元运算
   - trybinassocTM: 支持交换律的二元运算
   - trybiniTM: 整数优化版本
   - 提供灵活的运算符重载支持

4. **可变参数处理**
   - adjustvarargs: 栈调整
   - getvararg/getvarargs: 参数提取
   - 完整支持Lua的可变参数特性

## 五、C语言高级特性说明

1. **宏定义技巧**
   - 使用\实现多行宏
   - 三元运算符?:实现条件逻辑
   - cast_byte进行类型安全转换

2. **位运算**
   - << 左移运算：~0u << n 生成高n位为0的掩码
   - & 按位与：检查特定位是否为1
   - | 按位或：设置特定位为1

3. **const修饰符**
   - const char *const: 双重常量保护
   - 第一个const: 保护数据不被修改
   - 第二个const: 保护指针不被修改

4. **函数指针与回调**
   - 元方法本质是存储在表中的函数
   - 通过TValue间接调用，实现动态分派

## 六、与Lua语言特性的对应

| C中的定义 | Lua中的使用 | 说明 |
|----------|-----------|------|
| TM_ADD | a + b | 加法运算符重载 |
| TM_INDEX | t[k] | 表索引读取 |
| TM_CALL | f(...) | 函数调用语法 |
| TM_LEN | #t | 长度运算符 |
| TM_GC | -- | 垃圾回收时自动调用 |

## 七、性能考虑

1. **快速检查**：flags位图避免哈希表查找
2. **缓存名称**：tmname数组预存元方法名字符串
3. **类型特化**：为整数运算提供优化路径
4. **内联宏**：fasttm等宏避免函数调用开销

## 八、设计哲学

- **最小化开销**：元方法不存在时几乎零开销
- **最大化灵活性**：25种元方法覆盖各种定制需求
- **类型安全**：通过TValue统一不同类型的元方法
- **向后兼容**：保持枚举顺序确保版本兼容性

这个文件虽然只有200行左右，但设计精巧，是Lua灵活性和高性能的关键基石。
================================================================================
*/