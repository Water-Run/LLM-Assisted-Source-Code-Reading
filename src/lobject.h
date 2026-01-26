/*
** ============================================================================
** 文件概要说明
** ============================================================================
**
** 文件名: lobject.h
** 版本: $Id: lobject.h $
** 作用: Lua对象的类型定义
** 版权信息: 见lua.h
**
** 这个头文件是Lua虚拟机的核心文件之一,定义了Lua中所有基本数据类型的内部表示。
**
** 主要内容包括:
** 1. TValue结构 - Lua中所有值的统一表示(带标签的值)
** 2. 各种基本类型的定义和操作宏:
**    - nil (包括多种变体:标准nil、空槽、缺失键等)
**    - boolean (true/false)
**    - number (整数和浮点数)
**    - string (短字符串和长字符串)
**    - userdata (轻量和完全userdata)
**    - function (Lua闭包、C闭包、轻量C函数)
**    - table (哈希表)
**    - thread (协程)
** 3. 垃圾回收相关的结构和标记
** 4. 栈值(StackValue)的定义
** 5. 各种辅助宏用于类型检查、值设置和获取
**
** 核心设计思想:
** - 使用"带标签的值"(Tagged Value)设计,每个值包含实际数据和类型标签
** - 类型标签使用位运算编码类型信息和变体信息
** - 通过宏提供类型安全的访问接口
** ============================================================================
*/

#ifndef lobject_h
#define lobject_h

#include <stdarg.h> // 标准库头文件,提供可变参数列表支持(va_list, va_start等)

#include "llimits.h" // Lua的限制和平台相关定义(如整数类型、内存限制等)
#include "lua.h"     // Lua的公共API定义

/*
** ============================================================================
** 额外的可回收对象类型
** ============================================================================
**
** 这些是Lua内部使用的类型,不直接暴露给用户
** LUA_NUMTYPES是在lua.h中定义的基本类型数量
*/
/* upvalues - 上值,用于实现闭包的捕获变量 */
#define LUA_TUPVAL LUA_NUMTYPES
/* function prototypes - 函数原型,存储函数的字节码和元信息 */
#define LUA_TPROTO (LUA_NUMTYPES + 1)
/* removed keys in tables - 表中被删除的键,用于垃圾回收优化 */
#define LUA_TDEADKEY (LUA_NUMTYPES + 2)

/*
** ============================================================================
** 所有可能类型的总数(包括LUA_TNONE但不包括DEADKEY)
** ============================================================================
**
** LUA_TNONE通常值为-1,表示"无类型"
** DEADKEY是内部使用的特殊标记,不计入总数
*/
#define LUA_TOTALTYPES (LUA_TPROTO + 2)

/*
** ============================================================================
** 标签值(Tagged Values)的标签位使用说明
** ============================================================================
**
** 每个TValue都有一个tt_字段(类型标签),是一个字节(8位):
** - 位 0-3: 实际类型标签(LUA_T*常量,如LUA_TNIL, LUA_TNUMBER等)
** - 位 4-5: 变体位(用于区分同一类型的不同变体,如整数vs浮点数)
** - 位 6:   是否可回收标记(1表示需要垃圾回收)
** - 位 7:   保留
**
** 这种设计允许在一个字节内编码类型、变体和GC信息
*/

/*
** 为类型添加变体位
** 参数: t - 基础类型, v - 变体值(0-3)
** 返回: 带变体的类型标签
*/
#define makevariant(t, v) ((t) | ((v) << 4))

/*
** ============================================================================
** 所有Lua值的联合体
** ============================================================================
**
** Value联合体可以存储任何Lua值的实际数据
** 使用联合体可以节省内存,因为一个值同时只能是一种类型
*/
typedef union Value
{
  struct GCObject *gc; /* 可回收对象(字符串、表、函数等) */
  void *p;             /* 轻量用户数据(light userdata) - 不需要GC的C指针 */
  lua_CFunction f;     /* 轻量C函数 - 直接存储函数指针 */
  lua_Integer i;       /* 整数(通常是64位整数) */
  lua_Number n;        /* 浮点数(通常是double) */
  /* 未使用,但可避免未初始化值的警告 */
  lu_byte ub; /* 无符号字节 */
} Value;

/*
** ============================================================================
** 带标签的值(Tagged Values)
** ============================================================================
**
** 这是Lua中值的基本表示:一个实际值加上其类型标签
**
** TValue是整个Lua虚拟机最核心的数据结构:
** - value_: 存储实际数据的Value联合体
** - tt_: 类型标签字节,编码类型、变体和GC信息
**
** 这种设计的优点:
** 1. 统一表示所有类型
** 2. 类型检查快速(只需检查一个字节)
** 3. 内存效率高(联合体只占用最大成员的空间)
*/

/* TValue的字段定义宏 */
#define TValuefields \
  Value value_;      \
  lu_byte tt_

/* TValue结构体 */
typedef struct TValue
{
  TValuefields; /* 展开为: Value value_; lu_byte tt_; */
} TValue;

/* 获取TValue的value_字段 */
#define val_(o) ((o)->value_)
/* 获取原始值(无类型检查) */
#define valraw(o) (val_(o))

/* 获取TValue的原始类型标签(包含所有位信息) */
#define rawtt(o) ((o)->tt_)

/*
** 无变体的类型标签(仅保留位0-3)
** 用于获取基础类型,忽略变体信息
** 0x0F = 0b00001111,保留低4位
*/
#define novariant(t) ((t) & 0x0F)

/*
** 带变体的类型标签(位0-5)
** 0x3F = 0b00111111,保留低6位(类型+变体)
*/
#define withvariant(t) ((t) & 0x3F)
/* 获取TValue的完整类型标签(类型+变体) */
#define ttypetag(o) withvariant(rawtt(o))

/* 获取TValue的基础类型(无变体) */
#define ttype(o) (novariant(rawtt(o)))

/* ============================================================================
** 类型测试宏
** ============================================================================
*/
/* 检查标签是否完全匹配(包括变体) */
#define checktag(o, t) (rawtt(o) == (t))
/* 检查基础类型是否匹配(忽略变体) */
#define checktype(o, t) (ttype(o) == (t))

/* ============================================================================
** 内部测试宏
** ============================================================================
*/

/*
** 检查可回收对象的标签是否与原始值匹配
** 用于调试,确保GC对象的类型信息一致
*/
#define righttt(obj) (ttypetag(obj) == gcvalue(obj)->tt)

/*
** 检查值的"活性"(liveness)
**
** 任何被程序操作的值要么是:
** 1. 不可回收的(如数字、布尔值)
** 2. 可回收对象且标签正确且未死亡
**
** 'L == NULL'选项允许在L不可用时使用此宏
**
** 这个宏在调试版本中用于运行时检查,确保不会访问已被GC回收的对象
*/
#define checkliveness(L, obj)                     \
  ((void)L, lua_longassert(!iscollectable(obj) || \
                           (righttt(obj) && (L == NULL || !isdead(G(L), gcvalue(obj))))))

/* ============================================================================
** 值设置宏
** ============================================================================
*/

/* 设置值的类型标签 */
#define settt_(o, t) ((o)->tt_ = (t))

/*
** 主要的值复制宏(从obj2复制到obj1)
**
** 步骤:
** 1. 创建临时指针避免宏参数副作用
** 2. 复制value_字段(实际数据)
** 3. 复制tt_字段(类型标签)
** 4. 检查活性(调试版本)
** 5. 断言不是非标准nil
*/
#define setobj(L, obj1, obj2)         \
  {                                   \
    TValue *io1 = (obj1);             \
    const TValue *io2 = (obj2);       \
    io1->value_ = io2->value_;        \
    settt_(io1, io2->tt_);            \
    checkliveness(L, io1);            \
    lua_assert(!isnonstrictnil(io1)); \
  }

/*
** ============================================================================
** 不同类型的赋值操作
** ============================================================================
**
** 根据源和目标的不同,使用不同的赋值宏
** (目前它们大多相同,但将来可能不同,例如写屏障的不同处理)
*/

/* 从栈到栈 - s2v将StackValue转换为TValue */
#define setobjs2s(L, o1, o2) setobj(L, s2v(o1), s2v(o2))
/* 到栈(不是从同一个栈) */
#define setobj2s(L, o1, o2) setobj(L, s2v(o1), o2)
/* 从表到同一个表 */
#define setobjt2t setobj
/* 到新对象 */
#define setobj2n setobj
/* 到表 */
#define setobj2t setobj

/*
** ============================================================================
** Lua栈中的条目
** ============================================================================
**
** StackValue是栈槽的表示,它是一个联合体:
** 1. val: 普通的TValue
** 2. tbclist: 用于"to-be-closed"变量的链表节点
**
** "to-be-closed"变量是Lua 5.4引入的特性,允许在作用域结束时
** 自动调用清理函数(类似于其他语言的defer或RAII)
**
** delta字段用于链接到下一个tbc变量。由于delta是unsigned short,
** 如果距离太大,会使用虚拟条目(delta==0)
*/
typedef union StackValue
{
  TValue val; /* 栈上的普通值 */
  struct
  {
    TValuefields;         /* 和TValue相同的字段 */
    unsigned short delta; /* 到链表中下一个tbc变量的距离 */
  } tbclist;
} StackValue;

/*
** 栈元素索引
** StkId是指向栈槽的指针,用于索引和操作栈上的值
*/
typedef StackValue *StkId;

/*
** ============================================================================
** 栈重分配时的指针处理
** ============================================================================
**
** 当栈需要扩容时,所有指向栈的指针都会失效
** StkIdRel允许在重分配期间将指针转换为偏移量,重分配后再转回指针
**
** 这是一个联合体:
** - p: 实际指针(正常使用)
** - offset: 偏移量(重分配期间使用)
*/
typedef union
{
  StkId p;          /* 实际指针 */
  ptrdiff_t offset; /* 栈重分配时使用的偏移量 */
} StkIdRel;

/*
** 将StackValue转换为TValue
** 因为StackValue的第一个成员是val(TValue),所以取其地址即可
*/
#define s2v(o) (&(o)->val)

/*
** ============================================================================
** Nil类型
** ============================================================================
**
** Lua 5.4引入了多种nil变体,用于不同的内部目的:
** 1. LUA_VNIL: 标准的nil值
** 2. LUA_VEMPTY: 空槽(表中未使用的位置)
** 3. LUA_VABSTKEY: 表中不存在的键
** 4. LUA_VNOTABLE: 快速get访问非表时的信号
**
** 这些变体在语义上都等同于nil,但在内部实现中有不同用途
*/

/* 标准nil */
#define LUA_VNIL makevariant(LUA_TNIL, 0)

/* 空槽(可能与包含nil的槽不同) */
#define LUA_VEMPTY makevariant(LUA_TNIL, 1)

/* 表访问中键不存在时返回的值(缺失键) */
#define LUA_VABSTKEY makevariant(LUA_TNIL, 2)

/* 用于表示快速get正在访问非表对象的特殊变体 */
#define LUA_VNOTABLE makevariant(LUA_TNIL, 3)

/*
** 测试任何种类的nil
** checktype忽略变体位,所以匹配所有nil变体
*/
#define ttisnil(v) checktype((v), LUA_TNIL)

/*
** 测试表访问结果的宏
**
** 形式上,应该区分LUA_VEMPTY/LUA_VABSTKEY/LUA_VNOTABLE和其他标签
** 但由于当前nil等价于LUA_VEMPTY,简单测试是否为nil即可
*/
#define tagisempty(tag) (novariant(tag) == LUA_TNIL)

/* 测试标准nil */
#define ttisstrictnil(o) checktag((o), LUA_VNIL)

/* 将对象设置为nil */
#define setnilvalue(obj) settt_(obj, LUA_VNIL)

/* 测试是否为缺失键 */
#define isabstkey(v) checktag((v), LUA_VABSTKEY)

/*
** 测试非标准nil的宏(仅用于断言)
** 用于调试,检测意外的nil变体
*/
#define isnonstrictnil(v) (ttisnil(v) && !ttisstrictnil(v))

/*
** 默认情况下,任何种类的nil都被视为空
** (在任何定义中,与缺失键关联的值也必须被接受为空)
*/
#define isempty(v) ttisnil(v)

/*
** 缺失键的常量定义
** {NULL}, LUA_VABSTKEY 初始化一个TValue为缺失键
*/
#define ABSTKEYCONSTANT {NULL}, LUA_VABSTKEY

/* 将条目标记为空 */
#define setempty(v) settt_(v, LUA_VEMPTY)

/* ============================================================================
** 布尔值
** ============================================================================
**
** 布尔类型有两个变体:false和true
** 使用变体位区分,而不是在value_中存储0/1
*/

/* false变体 */
#define LUA_VFALSE makevariant(LUA_TBOOLEAN, 0)
/* true变体 */
#define LUA_VTRUE makevariant(LUA_TBOOLEAN, 1)

/* 测试是否为布尔类型 */
#define ttisboolean(o) checktype((o), LUA_TBOOLEAN)
/* 测试是否为false */
#define ttisfalse(o) checktag((o), LUA_VFALSE)
/* 测试是否为true */
#define ttistrue(o) checktag((o), LUA_VTRUE)

/*
** 测试值在条件语句中是否为假
** 在Lua中,只有false和nil为假
*/
#define l_isfalse(o) (ttisfalse(o) || ttisnil(o))
/*
** 测试标签是否表示假值
** 用于优化,直接从标签判断而不需要完整的TValue
*/
#define tagisfalse(t) ((t) == LUA_VFALSE || novariant(t) == LUA_TNIL)

/* 设置为false */
#define setbfvalue(obj) settt_(obj, LUA_VFALSE)
/* 设置为true */
#define setbtvalue(obj) settt_(obj, LUA_VTRUE)

/* ============================================================================
** 线程(协程)
** ============================================================================
**
** Lua的协程在内部称为"线程"(thread)
** 每个协程有自己的栈和执行状态
*/

/* 线程类型标签(可回收) */
#define LUA_VTHREAD makevariant(LUA_TTHREAD, 0)

/* 测试是否为线程,ctb添加可回收标记 */
#define ttisthread(o) checktag((o), ctb(LUA_VTHREAD))

/*
** 获取线程值
** check_exp进行类型检查,gco2th将GCObject转换为lua_State
*/
#define thvalue(o) check_exp(ttisthread(o), gco2th(val_(o).gc))

/*
** 设置线程值
** obj2gco将lua_State转换为GCObject
** ctb添加可回收标记
*/
#define setthvalue(L, obj, x)     \
  {                               \
    TValue *io = (obj);           \
    lua_State *x_ = (x);          \
    val_(io).gc = obj2gco(x_);    \
    settt_(io, ctb(LUA_VTHREAD)); \
    checkliveness(L, io);         \
  }

/* 设置栈上的线程值 */
#define setthvalue2s(L, o, t) setthvalue(L, s2v(o), t)

/* ============================================================================
** 可回收对象
** ============================================================================
**
** 所有需要垃圾回收的对象都有一个公共头部
** 这允许GC遍历所有对象而不需要知道具体类型
*/

/*
** 所有可回收对象的公共头部(宏形式,用于包含在其他对象中)
**
** next: 指向下一个GC对象的指针,用于形成GC链表
** tt: 类型标签(与TValue中的tt_相同)
** marked: GC标记字节,用于三色标记算法
*/
#define CommonHeader     \
  struct GCObject *next; \
  lu_byte tt;            \
  lu_byte marked

/*
** 所有可回收对象的公共类型
** 这是一个基类型,实际对象会在此基础上添加更多字段
*/
typedef struct GCObject
{
  CommonHeader;
} GCObject;

/*
** 可回收类型的位标记
** 第6位设为1表示此类型需要垃圾回收
*/
#define BIT_ISCOLLECTABLE (1 << 6)

/* 测试对象是否可回收 */
#define iscollectable(o) (rawtt(o) & BIT_ISCOLLECTABLE)

/*
** 标记类型为可回收
** ctb = "collectable tag bit"
*/
#define ctb(t) ((t) | BIT_ISCOLLECTABLE)

/* 获取GC对象指针(带类型检查) */
#define gcvalue(o) check_exp(iscollectable(o), val_(o).gc)

/* 获取原始GC对象指针(无类型检查) */
#define gcvalueraw(v) ((v).gc)

/*
** 设置GC对象值
** 自动从GCObject的tt字段获取类型并添加可回收标记
*/
#define setgcovalue(L, obj, x) \
  {                            \
    TValue *io = (obj);        \
    GCObject *i_g = (x);       \
    val_(io).gc = i_g;         \
    settt_(io, ctb(i_g->tt));  \
  }

/* ============================================================================
** 数字
** ============================================================================
**
** Lua 5.3+支持两种数字类型:
** 1. 整数(通常是64位有符号整数)
** 2. 浮点数(通常是double)
**
** 使用变体位区分这两种数字类型
*/

/* 数字类型的变体标签 */
#define LUA_VNUMINT makevariant(LUA_TNUMBER, 0) /* 整数 */
#define LUA_VNUMFLT makevariant(LUA_TNUMBER, 1) /* 浮点数 */

/* 测试是否为数字(整数或浮点数) */
#define ttisnumber(o) checktype((o), LUA_TNUMBER)
/* 测试是否为浮点数 */
#define ttisfloat(o) checktag((o), LUA_VNUMFLT)
/* 测试是否为整数 */
#define ttisinteger(o) checktag((o), LUA_VNUMINT)

/*
** 获取数字值(统一返回浮点数)
** 如果是整数,使用cast_num转换为浮点数
*/
#define nvalue(o) check_exp(ttisnumber(o), \
                            (ttisinteger(o) ? cast_num(ivalue(o)) : fltvalue(o)))
/* 获取浮点数值 */
#define fltvalue(o) check_exp(ttisfloat(o), val_(o).n)
/* 获取整数值 */
#define ivalue(o) check_exp(ttisinteger(o), val_(o).i)

/* 获取原始浮点数(无类型检查) */
#define fltvalueraw(v) ((v).n)
/* 获取原始整数(无类型检查) */
#define ivalueraw(v) ((v).i)

/* 设置浮点数值 */
#define setfltvalue(obj, x)  \
  {                          \
    TValue *io = (obj);      \
    val_(io).n = (x);        \
    settt_(io, LUA_VNUMFLT); \
  }

/*
** 改变浮点数值(假设已经是浮点数)
** 用于优化,避免重新设置类型标签
*/
#define chgfltvalue(obj, x)    \
  {                            \
    TValue *io = (obj);        \
    lua_assert(ttisfloat(io)); \
    val_(io).n = (x);          \
  }

/* 设置整数值 */
#define setivalue(obj, x)    \
  {                          \
    TValue *io = (obj);      \
    val_(io).i = (x);        \
    settt_(io, LUA_VNUMINT); \
  }

/* 改变整数值(假设已经是整数) */
#define chgivalue(obj, x)        \
  {                              \
    TValue *io = (obj);          \
    lua_assert(ttisinteger(io)); \
    val_(io).i = (x);            \
  }

/* ============================================================================
** 字符串
** ============================================================================
**
** Lua有两种字符串类型:
** 1. 短字符串(short strings): 长度较小,被内部化(interned),相同内容
**    的字符串只存储一份,通过哈希表管理
** 2. 长字符串(long strings): 长度较大,不内部化
**
** 短字符串可以通过指针比较相等性(因为相同字符串只有一个实例)
** 长字符串需要逐字节比较
*/

/* 字符串类型的变体标签 */
#define LUA_VSHRSTR makevariant(LUA_TSTRING, 0) /* 短字符串 */
#define LUA_VLNGSTR makevariant(LUA_TSTRING, 1) /* 长字符串 */

/* 测试是否为字符串 */
#define ttisstring(o) checktype((o), LUA_TSTRING)
/* 测试是否为短字符串 */
#define ttisshrstring(o) checktag((o), ctb(LUA_VSHRSTR))
/* 测试是否为长字符串 */
#define ttislngstring(o) checktag((o), ctb(LUA_VLNGSTR))

/* 获取原始TString指针,gco2ts将GCObject转换为TString */
#define tsvalueraw(v) (gco2ts((v).gc))

/* 获取TString指针(带类型检查) */
#define tsvalue(o) check_exp(ttisstring(o), gco2ts(val_(o).gc))

/*
** 设置字符串值
** 从TString的tt字段获取类型(短或长字符串)
*/
#define setsvalue(L, obj, x)   \
  {                            \
    TValue *io = (obj);        \
    TString *x_ = (x);         \
    val_(io).gc = obj2gco(x_); \
    settt_(io, ctb(x_->tt));   \
    checkliveness(L, io);      \
  }

/* 设置栈上的字符串 */
#define setsvalue2s(L, o, s) setsvalue(L, s2v(o), s)

/* 设置新对象的字符串 */
#define setsvalue2n setsvalue

/*
** 长字符串的种类(存储在shrlen字段中)
**
** shrlen是signed byte,对长字符串使用负值表示类型:
*/
#define LSTRREG -1 /* 常规长字符串(由Lua管理) */
#define LSTRFIX -2 /* 固定的外部长字符串(不会被GC) */
#define LSTRMEM -3 /* 外部长字符串,带释放函数 */

/*
** ============================================================================
** 字符串值的头部
** ============================================================================
**
** TString结构存储字符串的元信息和内容
*/
typedef struct TString
{
  CommonHeader;      /* GC头部 */
  lu_byte extra;     /* 短字符串:保留字标记; 长字符串:"有哈希值"标记 */
  ls_byte shrlen;    /* 短字符串:长度; 长字符串:负值表示类型 */
  unsigned int hash; /* 字符串的哈希值 */
  union
  {
    size_t lnglen;         /* 长字符串的长度 */
    struct TString *hnext; /* 哈希表链表(用于短字符串) */
  } u;
  char *contents;   /* 长字符串:指向内容的指针; 短字符串:未使用 */
  lua_Alloc falloc; /* 外部字符串的释放函数 */
  void *ud;         /* 外部字符串的用户数据 */
} TString;

/* 测试是否为短字符串(shrlen非负) */
#define strisshr(ts) ((ts)->shrlen >= 0)
/* 测试是否为外部字符串 */
#define isextstr(ts) (ttislngstring(ts) && tsvalue(ts)->shrlen != LSTRREG)

/*
** 从TString获取实际字符串(字节数组)
**
** 通用版本和短/长字符串的特化版本:
** - 短字符串:内容紧跟在TString结构后面
** - 长字符串:内容通过contents指针访问
*/
/* 获取短字符串内容(转换为char指针) */
#define rawgetshrstr(ts) (cast_charp(&(ts)->contents))
/* 获取短字符串(带检查) */
#define getshrstr(ts) check_exp(strisshr(ts), rawgetshrstr(ts))
/* 获取长字符串 */
#define getlngstr(ts) check_exp(!strisshr(ts), (ts)->contents)
/* 获取字符串(自动判断类型) */
#define getstr(ts) (strisshr(ts) ? rawgetshrstr(ts) : (ts)->contents)

/*
** 从TString获取字符串长度
** cast_sizet将signed/unsigned转换为size_t
*/
#define tsslen(ts) \
  (strisshr(ts) ? cast_sizet((ts)->shrlen) : (ts)->u.lnglen)

/*
** 获取字符串和长度
**
** 这个宏同时返回字符串指针和设置长度变量
** 使用cast_void使表达式没有值,只有副作用(设置len)
*/
#define getlstr(ts, len)                                                 \
  (strisshr(ts)                                                          \
       ? (cast_void((len) = cast_sizet((ts)->shrlen)), rawgetshrstr(ts)) \
       : (cast_void((len) = (ts)->u.lnglen), (ts)->contents))

/* ============================================================================
** 用户数据(Userdata)
** ============================================================================
**
** Lua支持两种用户数据:
** 1. 轻量用户数据(light userdata): 只是一个C指针,不需要GC
** 2. 完全用户数据(full userdata): 由Lua管理的内存块,可以有元表和用户值
*/

/*
** 轻量用户数据应该是用户数据的变体,但为了兼容性,它们是不同的类型
**
** 在Lua 5.0-5.2中,轻量用户数据是单独的类型
** 为了向后兼容,5.3+仍保持这种设计
*/
#define LUA_VLIGHTUSERDATA makevariant(LUA_TLIGHTUSERDATA, 0)

/* 完全用户数据 */
#define LUA_VUSERDATA makevariant(LUA_TUSERDATA, 0)

/* 测试是否为轻量用户数据 */
#define ttislightuserdata(o) checktag((o), LUA_VLIGHTUSERDATA)
/* 测试是否为完全用户数据 */
#define ttisfulluserdata(o) checktag((o), ctb(LUA_VUSERDATA))

/* 获取轻量用户数据的指针 */
#define pvalue(o) check_exp(ttislightuserdata(o), val_(o).p)
/* 获取完全用户数据,gco2u转换为Udata */
#define uvalue(o) check_exp(ttisfulluserdata(o), gco2u(val_(o).gc))

/* 获取原始指针 */
#define pvalueraw(v) ((v).p)

/* 设置轻量用户数据 */
#define setpvalue(obj, x)           \
  {                                 \
    TValue *io = (obj);             \
    val_(io).p = (x);               \
    settt_(io, LUA_VLIGHTUSERDATA); \
  }

/* 设置完全用户数据 */
#define setuvalue(L, obj, x)        \
  {                                 \
    TValue *io = (obj);             \
    Udata *x_ = (x);                \
    val_(io).gc = obj2gco(x_);      \
    settt_(io, ctb(LUA_VUSERDATA)); \
    checkliveness(L, io);           \
  }

/*
** 确保此类型后的地址总是完全对齐
**
** LUAI_MAXALIGN是平台相关的最大对齐类型(如double或long long)
** 这确保用户数据的字节数据有正确的对齐
*/
typedef union UValue
{
  TValue uv;     /* 用户值 */
  LUAI_MAXALIGN; /* 确保udata字节的最大对齐 */
} UValue;

/*
** ============================================================================
** 带用户值的用户数据头部
** ============================================================================
**
** 用户数据可以关联多个"用户值"(user values),这些是Lua值,
** 可以从用户数据访问,主要用于存储与用户数据相关的Lua对象
**
** 内存区域紧跟在此结构末尾
*/
typedef struct Udata
{
  CommonHeader;            /* GC头部 */
  unsigned short nuvalue;  /* 用户值的数量 */
  size_t len;              /* 字节数(用户数据块的大小) */
  struct Table *metatable; /* 元表 */
  GCObject *gclist;        /* GC链表 */
  UValue uv[1];            /* 用户值数组(柔性数组) */
} Udata;

/*
** ============================================================================
** 无用户值的用户数据头部
** ============================================================================
**
** 没有用户值的用户数据在GC期间不需要变灰,因此不需要gclist字段
**
** 为简化代码,总是使用Udata,但确保不访问无用户值时的gclist
** 这个结构仅用于计算此表示的正确大小
** (末尾的bindata字段确保后续二进制数据的正确对齐)
*/
typedef struct Udata0
{
  CommonHeader;
  unsigned short nuvalue; /* 用户值数量(总是0) */
  size_t len;             /* 字节数 */
  struct Table *metatable;
  union
  {
    LUAI_MAXALIGN;
  } bindata; /* 确保对齐 */
} Udata0;

/*
** 计算用户数据内存区域的偏移量
**
** nuv==0时使用Udata0布局(无gclist),否则使用Udata布局
** offsetof是C标准库宏,计算结构体成员的偏移量
*/
#define udatamemoffset(nuv)               \
  ((nuv) == 0 ? offsetof(Udata0, bindata) \
              : offsetof(Udata, uv) + (sizeof(UValue) * (nuv)))

/*
** 获取Udata内存块的地址
** cast_charp转换为char指针以进行字节级算术
*/
#define getudatamem(u) (cast_charp(u) + udatamemoffset((u)->nuvalue))

/* 计算用户数据的大小(头部+用户值+用户内存) */
#define sizeudata(nuv, nb) (udatamemoffset(nuv) + (nb))

/* ============================================================================
** 函数原型(Prototypes)
** ============================================================================
**
** Proto存储Lua函数的编译后信息:
** - 字节码指令
** - 常量
** - 调试信息
** - 嵌套函数
** - 上值描述
*/

/* 原型类型标签 */
#define LUA_VPROTO makevariant(LUA_TPROTO, 0)

/*
** 指令类型
** l_uint32通常是uint32_t,一条指令占4字节
*/
typedef l_uint32 Instruction;

/*
** ============================================================================
** 函数原型的上值描述
** ============================================================================
**
** 上值是闭包捕获的外部局部变量
*/
typedef struct Upvaldesc
{
  TString *name;   /* 上值名称(用于调试信息) */
  lu_byte instack; /* 是否在栈中(寄存器),否则在外部函数的上值列表中 */
  lu_byte idx;     /* 上值的索引(在栈中或在外部函数列表中) */
  lu_byte kind;    /* 对应变量的种类(常规、常量等) */
} Upvaldesc;

/*
** ============================================================================
** 函数原型的局部变量描述
** ============================================================================
**
** 用于调试信息,记录变量的作用域
*/
typedef struct LocVar
{
  TString *varname; /* 变量名 */
  int startpc;      /* 变量激活的第一个指令位置 */
  int endpc;        /* 变量失效的第一个指令位置 */
} LocVar;

/*
** ============================================================================
** 绝对行信息关联
** ============================================================================
**
** 为给定指令(pc)关联绝对源代码行号
**
** lineinfo数组为每条指令给出与前一条指令的行差
** 当差值太大无法放入一个字节时,Lua保存该指令的绝对行号
** (Lua也周期性保存绝对行号,以加速行号计算:可以在绝对行数组中
** 使用二分查找,但必须线性遍历lineinfo数组)
*/
typedef struct AbsLineInfo
{
  int pc;   /* 程序计数器(指令索引) */
  int line; /* 对应的绝对行号 */
} AbsLineInfo;

/*
** ============================================================================
** 原型中的标志
** ============================================================================
*/
#define PF_VAHID 1 /* 函数有隐藏的可变参数 */
#define PF_VATAB 2 /* 函数有可变参数表 */
#define PF_FIXED 4 /* 原型的部分在固定内存中 */

/* 可变参数函数要么有隐藏参数,要么有可变参数表 */
#define isvararg(p) ((p)->flag & (PF_VAHID | PF_VATAB))

/*
** 标记函数需要可变参数表
** (PF_VAHID标志稍后会被清除)
*/
#define needvatab(p) ((p)->flag |= PF_VATAB)

/*
** ============================================================================
** 函数原型结构
** ============================================================================
**
** 这是Lua函数编译后的表示,包含执行所需的所有信息
*/
typedef struct Proto
{
  CommonHeader;             /* GC头部 */
  lu_byte numparams;        /* 固定(命名)参数的数量 */
  lu_byte flag;             /* 标志(可变参数等) */
  lu_byte maxstacksize;     /* 此函数需要的寄存器数量 */
  int sizeupvalues;         /* upvalues数组的大小 */
  int sizek;                /* k数组的大小(常量数) */
  int sizecode;             /* code数组的大小(指令数) */
  int sizelineinfo;         /* lineinfo数组的大小 */
  int sizep;                /* p数组的大小(嵌套函数数) */
  int sizelocvars;          /* locvars数组的大小 */
  int sizeabslineinfo;      /* abslineinfo数组的大小 */
  int linedefined;          /* 函数定义开始的行号 */
  int lastlinedefined;      /* 函数定义结束的行号 */
  TValue *k;                /* 函数使用的常量数组 */
  Instruction *code;        /* 操作码数组(字节码) */
  struct Proto **p;         /* 函数内定义的函数数组 */
  Upvaldesc *upvalues;      /* 上值信息数组 */
  ls_byte *lineinfo;        /* 源代码行信息(调试) */
  AbsLineInfo *abslineinfo; /* 绝对行信息(调试) */
  LocVar *locvars;          /* 局部变量信息(调试) */
  TString *source;          /* 源文件名(调试) */
  GCObject *gclist;         /* GC链表 */
} Proto;

/* ============================================================================
** 函数
** ============================================================================
**
** Lua支持多种函数类型:
** 1. Lua闭包(LClosure): Lua函数,带上值
** 2. C闭包(CClosure): C函数,带上值
** 3. 轻量C函数(light C function): 直接的C函数指针,无上值
*/

/* 上值类型标签 */
#define LUA_VUPVAL makevariant(LUA_TUPVAL, 0)

/* 函数类型的变体标签 */
#define LUA_VLCL makevariant(LUA_TFUNCTION, 0) /* Lua闭包 */
#define LUA_VLCF makevariant(LUA_TFUNCTION, 1) /* 轻量C函数 */
#define LUA_VCCL makevariant(LUA_TFUNCTION, 2) /* C闭包 */

/* 测试是否为函数 */
#define ttisfunction(o) checktype(o, LUA_TFUNCTION)
/* 测试是否为Lua闭包 */
#define ttisLclosure(o) checktag((o), ctb(LUA_VLCL))
/* 测试是否为轻量C函数 */
#define ttislcf(o) checktag((o), LUA_VLCF)
/* 测试是否为C闭包 */
#define ttisCclosure(o) checktag((o), ctb(LUA_VCCL))
/* 测试是否为闭包(Lua或C) */
#define ttisclosure(o) (ttisLclosure(o) || ttisCclosure(o))

/* 测试是否为Lua函数 */
#define isLfunction(o) ttisLclosure(o)

/* 获取闭包(自动判断类型),gco2cl转换为Closure */
#define clvalue(o) check_exp(ttisclosure(o), gco2cl(val_(o).gc))
/* 获取Lua闭包,gco2lcl转换为LClosure */
#define clLvalue(o) check_exp(ttisLclosure(o), gco2lcl(val_(o).gc))
/* 获取轻量C函数指针 */
#define fvalue(o) check_exp(ttislcf(o), val_(o).f)
/* 获取C闭包,gco2ccl转换为CClosure */
#define clCvalue(o) check_exp(ttisCclosure(o), gco2ccl(val_(o).gc))

/* 获取原始函数指针 */
#define fvalueraw(v) ((v).f)

/* 设置Lua闭包 */
#define setclLvalue(L, obj, x) \
  {                            \
    TValue *io = (obj);        \
    LClosure *x_ = (x);        \
    val_(io).gc = obj2gco(x_); \
    settt_(io, ctb(LUA_VLCL)); \
    checkliveness(L, io);      \
  }

/* 设置栈上的Lua闭包 */
#define setclLvalue2s(L, o, cl) setclLvalue(L, s2v(o), cl)

/* 设置轻量C函数 */
#define setfvalue(obj, x) \
  {                       \
    TValue *io = (obj);   \
    val_(io).f = (x);     \
    settt_(io, LUA_VLCF); \
  }

/* 设置C闭包 */
#define setclCvalue(L, obj, x) \
  {                            \
    TValue *io = (obj);        \
    CClosure *x_ = (x);        \
    val_(io).gc = obj2gco(x_); \
    settt_(io, ctb(LUA_VCCL)); \
    checkliveness(L, io);      \
  }

/*
** ============================================================================
** Lua闭包的上值
** ============================================================================
**
** 上值(UpVal)用于实现闭包对外部局部变量的捕获
**
** 上值可以是"开放的"或"关闭的":
** - 开放的上值:指向仍在栈上的变量
** - 关闭的上值:变量已离开栈,值被复制到上值对象中
*/
typedef struct UpVal
{
  CommonHeader; /* GC头部 */
  union
  {
    TValue *p;        /* 指向栈或自己的value(开放/关闭) */
    ptrdiff_t offset; /* 栈重分配时使用的偏移量 */
  } v;
  union
  {
    struct
    {                          /* (当开放时) */
      struct UpVal *next;      /* 链表 */
      struct UpVal **previous; /* 双向链表 */
    } open;
    TValue value; /* 值(当关闭时) */
  } u;
} UpVal;

/*
** 闭包头部宏
** 所有闭包(Lua和C)共享的头部
*/
#define ClosureHeader \
  CommonHeader;       \
  lu_byte nupvalues;  \
  GCObject *gclist

/*
** C闭包
**
** C闭包是C函数加上上值
** 上值可以存储任意Lua值,用于在调用间保持状态
*/
typedef struct CClosure
{
  ClosureHeader;     /* 闭包头部 */
  lua_CFunction f;   /* C函数指针 */
  TValue upvalue[1]; /* 上值列表(柔性数组) */
} CClosure;

/*
** Lua闭包
**
** Lua闭包是函数原型加上上值
*/
typedef struct LClosure
{
  ClosureHeader;    /* 闭包头部 */
  struct Proto *p;  /* 函数原型 */
  UpVal *upvals[1]; /* 上值列表(柔性数组) */
} LClosure;

/*
** 闭包联合体
**
** 允许将Closure*转换为CClosure*或LClosure*
*/
typedef union Closure
{
  CClosure c; /* C闭包 */
  LClosure l; /* Lua闭包 */
} Closure;

/* 获取Lua闭包的原型 */
#define getproto(o) (clLvalue(o)->p)

/* ============================================================================
** 表(Tables)
** ============================================================================
**
** Lua的表是关联数组,同时支持:
** - 数组部分:用于整数键的快速访问
** - 哈希部分:用于其他类型的键
*/

/* 表类型标签 */
#define LUA_VTABLE makevariant(LUA_TTABLE, 0)

/* 测试是否为表 */
#define ttistable(o) checktag((o), ctb(LUA_VTABLE))

/* 获取表,gco2t转换为Table */
#define hvalue(o) check_exp(ttistable(o), gco2t(val_(o).gc))

/* 设置表值 */
#define sethvalue(L, obj, x)     \
  {                              \
    TValue *io = (obj);          \
    Table *x_ = (x);             \
    val_(io).gc = obj2gco(x_);   \
    settt_(io, ctb(LUA_VTABLE)); \
    checkliveness(L, io);        \
  }

/* 设置栈上的表 */
#define sethvalue2s(L, o, h) sethvalue(L, s2v(o), h)

/*
** ============================================================================
** 哈希表的节点
** ============================================================================
**
** 一对TValue(键-值对)加上一个next字段用于链接冲突条目
**
** 键的字段(key_tt和key_val)的分布不形成正规的TValue,
** 这允许Node在4字节和8字节对齐中都有更小的大小
*/
typedef union Node
{
  struct NodeKey
  {
    TValuefields;   /* 值的字段 */
    lu_byte key_tt; /* 键类型 */
    int next;       /* 链接冲突条目(索引,不是指针) */
    Value key_val;  /* 键值 */
  } u;
  TValue i_val; /* 直接访问节点值作为正规TValue */
} Node;

/*
** 将值复制到键中
** 注意:键的布局与TValue不同,需要分别复制字段
*/
#define setnodekey(node, obj)    \
  {                              \
    Node *n_ = (node);           \
    const TValue *io_ = (obj);   \
    n_->u.key_val = io_->value_; \
    n_->u.key_tt = io_->tt_;     \
  }

/*
** 从键复制值到TValue
** 重新组装分散的键字段为正规TValue
*/
#define getnodekey(L, obj, node) \
  {                              \
    TValue *io_ = (obj);         \
    const Node *n_ = (node);     \
    io_->value_ = n_->u.key_val; \
    io_->tt_ = n_->u.key_tt;     \
    checkliveness(L, io_);       \
  }

/*
** 表结构
**
** Lua的表使用混合存储策略:
** - array: 数组部分,用于整数键1..asize
** - node: 哈希部分,用于其他键
*/
typedef struct Table
{
  CommonHeader;            /* GC头部 */
  lu_byte flags;           /* 1<<p表示标签方法(p)不存在(优化) */
  lu_byte lsizenode;       /* node数组槽位数的log2 */
  unsigned int asize;      /* array数组的槽位数 */
  Value *array;            /* 数组部分 */
  Node *node;              /* 哈希部分 */
  struct Table *metatable; /* 元表 */
  GCObject *gclist;        /* GC链表 */
} Table;


/*
** ============================================================================
** 操作节点中键的宏
** ============================================================================
*/
/* 获取节点的键类型 */
#define keytt(node) ((node)->u.key_tt)
/* 获取节点的键值 */
#define keyval(node) ((node)->u.key_val)

/* 测试键是否为nil */
#define keyisnil(node) (keytt(node) == LUA_TNIL)
/* 测试键是否为整数 */
#define keyisinteger(node) (keytt(node) == LUA_VNUMINT)
/* 获取整数键的值 */
#define keyival(node) (keyval(node).i)
/* 测试键是否为短字符串 */
#define keyisshrstr(node) (keytt(node) == ctb(LUA_VSHRSTR))
/* 获取字符串键的值,gco2ts转换为TString */
#define keystrval(node) (gco2ts(keyval(node).gc))

/* 将键设为nil */
#define setnilkey(node) (keytt(node) = LUA_TNIL)

/* 测试键是否为可回收类型 */
#define keyiscollectable(n) (keytt(n) & BIT_ISCOLLECTABLE)

/* 获取键的GC对象 */
#define gckey(n) (keyval(n).gc)
/* 获取键的GC对象(如果可回收),否则NULL */
#define gckeyN(n) (keyiscollectable(n) ? gckey(n) : NULL)

/*
** 表中的死键
**
** 死键保持LUA_TDEADKEY标签但保留原始gcvalue
** 这将它们与常规键区分开,但允许在特殊搜索时找到它们
** (next需要这个来找到遍历期间从表中删除的键)
*/
#define setdeadkey(node) (keytt(node) = LUA_TDEADKEY)
#define keyisdead(node) (keytt(node) == LUA_TDEADKEY)

/* ============================================================================
** 哈希辅助宏
** ============================================================================
*/

/*
** 哈希的模运算(大小总是2的幂)
**
** 使用位运算优化取模:对于2的幂,s % size == s & (size-1)
** check_exp确保size确实是2的幂
*/
#define lmod(s, size) \
  (check_exp((size & (size - 1)) == 0, (cast_uint(s) & cast_uint((size) - 1))))

/* 2的x次幂 */
#define twoto(x) (1u << (x))
/* 获取哈希表节点数组的大小 */
#define sizenode(t) (twoto((t)->lsizenode))

/*
** luaO_utf8esc函数缓冲区的大小
** UTF-8编码最多需要6字节,加上终止符和一些余量
*/
#define UTF8BUFFSZ 8

/*
** 正确调用luaO_pushvfstring的宏
**
** 这个宏处理可变参数:
** 1. 使用va_start初始化参数列表
** 2. 调用luaO_pushvfstring
** 3. 使用va_end清理
** 4. 如果返回NULL(内存错误),抛出异常
*/
#define pushvfstring(L, argp, fmt, msg)               \
  {                                                   \
    va_start(argp, fmt);                              \
    msg = luaO_pushvfstring(L, fmt, argp);            \
    va_end(argp);                                     \
    if (msg == NULL)                                  \
      luaD_throw(L, LUA_ERRMEM); /* 只在va_end之后 */ \
  }

/* ============================================================================
** 函数声明
** ============================================================================
**
** LUAI_FUNC标记为Lua内部函数(通常是静态或有限可见性)
**
** 这些函数在lobject.c中实现,提供对象操作的核心功能
*/

/*
** UTF-8转义序列
** buff: 输出缓冲区, x: Unicode码点
** 返回: 写入的字节数
*/
LUAI_FUNC int luaO_utf8esc(char *buff, l_uint32 x);

/*
** 计算不小于x的最小的2的幂的指数
** 例如: x=5返回3 (因为2^3=8>=5)
*/
LUAI_FUNC lu_byte luaO_ceillog2(unsigned int x);

/*
** 编码参数p
** 用于压缩存储某些参数
*/
LUAI_FUNC lu_byte luaO_codeparam(unsigned int p);

/*
** 应用编码参数p到值x
** 解压缩编码的参数
*/
LUAI_FUNC l_mem luaO_applyparam(lu_byte p, l_mem x);

/*
** 原始算术运算(不触发元方法)
** L: Lua状态, op: 操作符, p1,p2: 操作数, res: 结果
** 返回: 1成功, 0失败
*/
LUAI_FUNC int luaO_rawarith(lua_State *L, int op, const TValue *p1,
                            const TValue *p2, TValue *res);

/*
** 算术运算(可能触发元方法)
** L: Lua状态, op: 操作符, p1,p2: 操作数, res: 结果栈位置
*/
LUAI_FUNC void luaO_arith(lua_State *L, int op, const TValue *p1,
                          const TValue *p2, StkId res);

/*
** 字符串转数字
** s: 字符串, o: 输出TValue
** 返回: 转换的字符数(0表示失败)
*/
LUAI_FUNC size_t luaO_str2num(const char *s, TValue *o);

/*
** 对象转字符串(到缓冲区)
** obj: 对象, buff: 缓冲区
** 返回: 字符串长度
*/
LUAI_FUNC unsigned luaO_tostringbuff(const TValue *obj, char *buff);

/*
** 十六进制字符的值
** c: 字符('0'-'9','a'-'f','A'-'F')
** 返回: 0-15的值
*/
LUAI_FUNC lu_byte luaO_hexavalue(int c);

/*
** 对象转字符串(就地转换TValue)
** L: Lua状态, obj: 对象
** 副作用: obj可能被转换为字符串
*/
LUAI_FUNC void luaO_tostring(lua_State *L, TValue *obj);

/*
** 格式化字符串(可变参数版本)
** L: Lua状态, fmt: 格式字符串, argp: 参数列表
** 返回: 结果字符串(压入栈),NULL表示内存错误
*/
LUAI_FUNC const char *luaO_pushvfstring(lua_State *L, const char *fmt,
                                        va_list argp);

/*
** 格式化字符串
** L: Lua状态, fmt: 格式字符串, ...: 可变参数
** 返回: 结果字符串(压入栈)
*/
LUAI_FUNC const char *luaO_pushfstring(lua_State *L, const char *fmt, ...);

/*
** 生成chunk标识符
** out: 输出缓冲区, source: 源字符串, srclen: 源长度
** 副作用: 在out中生成适合显示的chunk名称
*/
LUAI_FUNC void luaO_chunkid(char *out, const char *source, size_t srclen);

#endif

/*
** ============================================================================
** 文件总结
** ============================================================================
**
** lobject.h 是Lua虚拟机的核心头文件,定义了Lua中所有值的内部表示。
**
** 核心概念总结:
**
** 1. **带标签的值(Tagged Value - TValue)**
**    - Lua使用统一的TValue结构表示所有类型的值
**    - 包含Value联合体(存储实际数据)和tt_字节(类型标签)
**    - 类型标签编码:基础类型(位0-3)、变体(位4-5)、GC标记(位6)
**
** 2. **类型系统**
**    - 基本类型:nil, boolean, number, string, table, function, userdata, thread
**    - 每种类型可能有多个变体(如整数vs浮点数,短字符串vs长字符串)
**    - 使用位运算高效地编码和检查类型信息
**
** 3. **垃圾回收(GC)**
**    - 可回收对象都有CommonHeader(next, tt, marked)
**    - 使用可回收标记位(第6位)快速判断是否需要GC
**    - 支持三色标记算法(marked字段)
**
** 4. **内存优化**
**    - 使用联合体节省内存(一个值同时只是一种类型)
**    - 字符串内部化(短字符串)减少重复
**    - 表的混合存储(数组+哈希)优化访问效率
**
** 5. **高级特性支持**
**    - 闭包通过UpVal实现,支持开放和关闭状态
**    - 协程通过独立的栈和状态实现
**    - 元表机制允许自定义类型行为
**    - to-be-closed变量支持确定性资源清理
**
** 6. **宏设计模式**
**    - 大量使用宏提供类型安全的访问接口
**    - 宏在编译时展开,零运行时开销
**    - check_exp宏在调试版本提供运行时检查
**
** 7. **API设计原则**
**    - 提供多层次的访问:raw(无检查)、带检查、高级封装
**    - 使用命名约定区分不同用途(如setobj2s, setobj2t)
**    - 考虑未来扩展性(如不同的赋值宏可能有不同的写屏障)
**
** 阅读建议:
** - 首先理解TValue和类型标签系统
** - 然后逐个类型深入(从简单到复杂:nil -> boolean -> number -> ...)
** - 注意各种宏的命名模式和设计意图
** - 结合lobject.c的实现代码加深理解
**
** 这个文件体现了Lua实现的核心设计哲学:
** - 简洁高效的数据表示
** - 统一的类型系统
** - 精心设计的内存管理
** - 通过C宏实现零成本抽象
** ============================================================================
*/