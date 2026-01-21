/*
** ============================================================================
** 文件概要:
** ============================================================================
** 文件名: lapi.c
** 版本: $Id: lapi.c $
** 功能描述: Lua C API 实现
**
** 这是 Lua 虚拟机的 C API 层实现文件,提供了 C 程序与 Lua 虚拟机交互的接口。
** 主要功能包括:
**
** 1. 栈操作 (Stack Manipulation)
**    - 栈索引转换、栈顶管理、元素移动和复制等基本操作
**    - Lua 使用栈作为 C 和 Lua 之间的数据交换媒介
**
** 2. 数据访问 (Data Access)
**    - 从栈中读取各种类型的值(数字、字符串、表、函数等)
**    - 向栈中压入各种类型的值
**
** 3. 表操作 (Table Operations)
**    - 创建表、获取/设置表字段
**    - 元表(metatable)操作
**
** 4. 函数调用 (Function Calls)
**    - 普通调用和保护模式调用(pcall)
**    - 加载和执行 Lua 代码
**
** 5. 垃圾回收控制 (Garbage Collection)
**    - GC 参数设置、手动触发 GC 等
**
** 6. 其他辅助功能
**    - 错误处理、警告系统、用户数据创建等
**
** 版权信息: 见 lua.h
** ============================================================================
*/

#define lapi_c
#define LUA_CORE

#include "lprefix.h"

#include <limits.h> /* 提供整数类型的限制值,如 INT_MAX */
#include <stdarg.h> /* 提供可变参数列表支持,如 va_list, va_start, va_end */
#include <string.h> /* 提供字符串操作函数,如 memcmp, memcpy */

#include "lua.h" /* Lua 主头文件,定义了公共 API */

#include "lapi.h"    /* API 相关的内部声明 */
#include "ldebug.h"  /* 调试相关功能 */
#include "ldo.h"     /* 函数调用和错误处理 */
#include "lfunc.h"   /* 函数和闭包操作 */
#include "lgc.h"     /* 垃圾回收器 */
#include "lmem.h"    /* 内存管理 */
#include "lobject.h" /* Lua 对象定义 */
#include "lstate.h"  /* Lua 状态机 */
#include "lstring.h" /* 字符串操作 */
#include "ltable.h"  /* 表操作 */
#include "ltm.h"     /* 标签方法(Tag Methods,即元方法) */
#include "lundump.h" /* 二进制代码序列化 */
#include "lvm.h"     /* Lua 虚拟机 */

/*
** Lua 版本标识字符串
** 包含版权信息和作者信息,用于标识 Lua 版本
*/
const char lua_ident[] =
    "$LuaVersion: " LUA_COPYRIGHT " $"
    "$LuaAuthors: " LUA_AUTHORS " $";

/*
** ============================================================================
** 索引验证和转换宏
** ============================================================================
*/

/*
** 测试索引是否有效(不是 'nilvalue')
**
** 参数:
**   L - Lua 状态机
**   o - 要测试的 TValue 指针
**
** 返回: 如果 o 不是全局的 nilvalue,则为真
**
** 说明: nilvalue 是一个特殊的全局单例,用于表示无效索引
*/
#define isvalid(L, o) ((o) != &G(L)->nilvalue)

/*
** 测试是否为伪索引(pseudo index)
**
** 伪索引是特殊的负数索引,用于访问:
** - LUA_REGISTRYINDEX: 注册表
** - upvalue 索引: 闭包的上值
**
** 这些索引不对应栈上的实际位置
*/
#define ispseudo(i) ((i) <= LUA_REGISTRYINDEX)

/*
** 测试是否为 upvalue 索引
**
** upvalue 索引比 LUA_REGISTRYINDEX 更小(更负)
** 用于访问 C 闭包的上值
*/
#define isupvalue(i) ((i) < LUA_REGISTRYINDEX)

/*
** ============================================================================
** 索引到值的转换函数
** ============================================================================
*/

/*
** 将可接受的索引转换为指向其对应值的指针
**
** 参数:
**   L   - Lua 状态机
**   idx - 栈索引,可以是:
**         正数: 相对于当前函数基址的绝对索引(1 是第一个参数)
**         负数: 相对于栈顶的索引(-1 是栈顶元素)
**         伪索引: LUA_REGISTRYINDEX 或 upvalue 索引
**
** 返回: 指向 TValue 的指针,无效索引返回 nilvalue
**
** 详细说明:
** 1. 正索引: ci->func.p + idx 计算绝对位置
** 2. 负索引: L->top.p + idx 从栈顶向下计算
** 3. LUA_REGISTRYINDEX: 返回全局注册表
** 4. upvalue 索引: 访问 C 闭包的上值数组
*/
static TValue *index2value(lua_State *L, int idx)
{
  CallInfo *ci = L->ci; /* 获取当前调用信息 */

  if (idx > 0)
  {                             /* 正索引:绝对位置 */
    StkId o = ci->func.p + idx; /* 从函数基址开始偏移 */
    /* 检查索引不超过当前函数的栈顶 */
    api_check(L, idx <= ci->top.p - (ci->func.p + 1), "unacceptable index");
    /* 如果超出实际栈顶,返回 nil */
    if (o >= L->top.p)
      return &G(L)->nilvalue;
    else
      return s2v(o); /* s2v 将栈地址转换为 TValue 指针 */
  }
  else if (!ispseudo(idx))
  { /* 负索引:相对于栈顶 */
    /* 检查索引有效(不为0且在范围内) */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
              "invalid index");
    return s2v(L->top.p + idx); /* 从栈顶向下偏移(idx 是负数) */
  }
  else if (idx == LUA_REGISTRYINDEX) /* 注册表索引 */
    return &G(L)->l_registry;        /* 返回全局注册表 */
  else
  {                                /* upvalues 上值索引 */
    idx = LUA_REGISTRYINDEX - idx; /* 计算 upvalue 的实际索引(从1开始) */
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p)))
    {                                             /* C 闭包? */
      CClosure *func = clCvalue(s2v(ci->func.p)); /* 获取 C 闭包 */
      /* 检查 upvalue 索引是否在范围内 */
      return (idx <= func->nupvalues) ? &func->upvalue[idx - 1]
                                      : &G(L)->nilvalue;
    }
    else
    { /* 轻量 C 函数或 Lua 函数(通过钩子) */
      /* 轻量 C 函数没有 upvalue */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C function");
      return &G(L)->nilvalue; /* 没有 upvalue,返回 nil */
    }
  }
}

/*
** 将有效的实际索引(非伪索引)转换为其栈地址
**
** 参数:
**   L   - Lua 状态机
**   idx - 栈索引(必须是实际索引,不能是伪索引)
**
** 返回: StkId 栈地址
**
** 说明: 与 index2value 类似,但返回栈地址而不是 TValue 指针
**       用于需要栈地址的内部操作
*/
static StkId index2stack(lua_State *L, int idx)
{
  CallInfo *ci = L->ci;
  if (idx > 0)
  { /* 正索引 */
    StkId o = ci->func.p + idx;
    api_check(L, o < L->top.p, "invalid index"); /* 必须在栈顶以下 */
    return o;
  }
  else
  { /* 非正索引 */
    /* 检查索引有效且不是伪索引 */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
              "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top.p + idx; /* 从栈顶向下计算 */
  }
}

/*
** ============================================================================
** 栈操作函数
** ============================================================================
*/

/*
** 检查栈空间是否足够容纳 n 个额外元素
**
** 参数:
**   L - Lua 状态机
**   n - 需要的额外栈空间数量
**
** 返回: 如果成功分配空间返回 1,否则返回 0
**
** 说明:
** - 这是一个重要的安全检查函数,防止栈溢出
** - 如果当前栈空间不足,会尝试扩展栈
** - 同时会调整当前调用帧的 top,确保有足够空间
*/
LUA_API int lua_checkstack(lua_State *L, int n)
{
  int res;
  CallInfo *ci;
  lua_lock(L); /* lua_lock/lua_unlock 用于多线程保护,在单线程版本中可能是空宏 */
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'"); /* n 必须非负 */

  if (L->stack_last.p - L->top.p > n) /* 栈空间足够? */
    res = 1;                          /* 是的,检查通过 */
  else                                /* 需要增长栈 */
    res = luaD_growstack(L, n, 0);    /* 尝试扩展栈,0 表示不是紧急扩展 */

  /* 如果成功且调用帧的 top 不够高,则调整它 */
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n; /* 调整帧顶 */

  lua_unlock(L);
  return res;
}

/*
** 在两个 Lua 状态机之间移动 n 个值
**
** 参数:
**   from - 源状态机
**   to   - 目标状态机
**   n    - 要移动的元素数量
**
** 说明:
** - 两个状态机必须属于同一个全局状态(同一个 Lua 虚拟机实例)
** - 元素从 from 的栈顶移动到 to 的栈顶
** - 使用 setobjs2s 宏复制对象,这会正确处理值的类型标签
*/
LUA_API void lua_xmove(lua_State *from, lua_State *to, int n)
{
  int i;
  if (from == to)
    return; /* 同一个状态,无需移动 */
  lua_lock(to);
  api_checkpop(from, n); /* 检查源栈有足够的元素可弹出 */
  /* 检查两个状态属于同一个全局状态 */
  api_check(from, G(from) == G(to), "moving among independent states");
  /* 检查目标栈有足够空间 */
  api_check(from, to->ci->top.p - to->top.p >= n, "stack overflow");

  from->top.p -= n; /* 调整源栈顶 */
  /* 逐个复制元素 */
  for (i = 0; i < n; i++)
  {
    setobjs2s(to, to->top.p, from->top.p + i); /* 复制对象 */
    to->top.p++;                               /* 目标栈已经检查过,直接增加 */
  }
  lua_unlock(to);
}

/*
** 设置 panic 函数
**
** 参数:
**   L      - Lua 状态机
**   panicf - 新的 panic 函数
**
** 返回: 之前的 panic 函数
**
** 说明:
** - panic 函数在不受保护的错误发生时被调用(即无法通过 pcall 捕获的错误)
** - 通常在独立解释器中,panic 函数会打印错误并退出程序
** - 这允许宿主程序自定义致命错误的处理方式
*/
LUA_API lua_CFunction lua_atpanic(lua_State *L, lua_CFunction panicf)
{
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;    /* 保存旧的 panic 函数 */
  G(L)->panic = panicf; /* 设置新的 panic 函数 */
  lua_unlock(L);
  return old; /* 返回旧函数 */
}

/*
** 获取 Lua 版本号
**
** 返回: Lua 版本号(数值形式,如 504.0 表示 Lua 5.4)
**
** 说明: UNUSED(L) 宏表示参数 L 未使用,避免编译器警告
*/
LUA_API lua_Number lua_version(lua_State *L)
{
  UNUSED(L);
  return LUA_VERSION_NUM;
}

/*
** ============================================================================
** 基本栈操作
** ============================================================================
*/

/*
** 将可接受的栈索引转换为绝对索引
**
** 参数:
**   L   - Lua 状态机
**   idx - 栈索引
**
** 返回: 绝对索引(正数)
**
** 说明:
** - 正索引和伪索引保持不变
** - 负索引转换为相应的正索引
** - 例如: -1 转换为当前栈顶的绝对位置
*/
LUA_API int lua_absindex(lua_State *L, int idx)
{
  return (idx > 0 || ispseudo(idx))
             ? idx                                       /* 已经是绝对索引或伪索引 */
             : cast_int(L->top.p - L->ci->func.p) + idx; /* 转换负索引 */
}

/*
** 获取栈顶索引
**
** 返回: 栈中元素的数量(即栈顶元素的索引)
**
** 说明:
** - 返回值从 0 开始计数(空栈返回 0)
** - ci->func.p + 1 是第一个栈元素的位置
*/
LUA_API int lua_gettop(lua_State *L)
{
  return cast_int(L->top.p - (L->ci->func.p + 1));
}

/*
** 设置栈顶到指定索引
**
** 参数:
**   L   - Lua 状态机
**   idx - 新的栈顶索引
**
** 说明:
** - 如果 idx > 当前栈顶: 用 nil 填充新位置
** - 如果 idx < 当前栈顶: 弹出多余元素,可能触发 to-be-closed 变量的清理
** - idx 可以是 0,表示清空栈
**
** to-be-closed 变量: Lua 5.4 引入的特性,类似于 C++ 的 RAII
** 当变量离开作用域时自动调用其 __close 元方法
*/
LUA_API void lua_settop(lua_State *L, int idx)
{
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff; /* 新栈顶的差值,ptrdiff_t 是指针差值类型 */
  lua_lock(L);
  ci = L->ci;
  func = ci->func.p;

  if (idx >= 0)
  { /* 正索引: 设置绝对位置 */
    api_check(L, idx <= ci->top.p - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top.p; /* 计算需要增加的元素数 */
    /* 用 nil 填充新增的位置 */
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top.p++)); /* 设置 nil 并递增栈顶 */
  }
  else
  { /* 负索引: 相对于栈顶 */
    api_check(L, -(idx + 1) <= (L->top.p - (func + 1)), "invalid new top");
    diff = idx + 1; /* 将减去索引(因为它是负数) */
  }

  newtop = L->top.p + diff; /* 计算新栈顶 */

  /* 如果是缩减栈且有 to-be-closed 变量在将被弹出的范围内 */
  if (diff < 0 && L->tbclist.p >= newtop)
  {
    lua_assert(ci->callstatus & CIST_TBC);        /* 必须标记了有 TBC 变量 */
    newtop = luaF_close(L, newtop, CLOSEKTOP, 0); /* 关闭 to-be-closed 变量 */
  }

  L->top.p = newtop; /* 只有在关闭所有 upvalue 后才修正栈顶 */
  lua_unlock(L);
}

/*
** 关闭指定索引处的 to-be-closed 变量
**
** 参数:
**   L   - Lua 状态机
**   idx - 要关闭的变量的索引
**
** 说明:
** - 这是 Lua 5.4 新增的 API
** - 手动触发变量的 __close 元方法
** - 关闭后将该位置设为 nil
*/
LUA_API void lua_closeslot(lua_State *L, int idx)
{
  StkId level;
  lua_lock(L);
  level = index2stack(L, idx);
  /* 检查该位置确实有 to-be-closed 变量 */
  api_check(L, (L->ci->callstatus & CIST_TBC) && (L->tbclist.p == level),
            "no variable to close at given level");
  level = luaF_close(L, level, CLOSEKTOP, 0); /* 关闭变量 */
  setnilvalue(s2v(level));                    /* 设置为 nil */
  lua_unlock(L);
}

/*
** 反转栈片段
** (lua_rotate 的辅助函数)
**
** 参数:
**   L    - Lua 状态机
**   from - 起始位置
**   to   - 结束位置
**
** 说明:
** - 就地反转 from 到 to 之间的元素
** - 注意我们只移动(复制)栈内的值,不移动可能存在的额外字段
** - 使用临时变量 temp 进行三次赋值完成交换
*/
static void reverse(lua_State *L, StkId from, StkId to)
{
  for (; from < to; from++, to--)
  {
    TValue temp;
    setobj(L, &temp, s2v(from)); /* temp = *from */
    setobjs2s(L, from, to);      /* *from = *to */
    setobj2s(L, to, &temp);      /* *to = temp */
  }
}

/*
** 旋转栈元素
**
** 参数:
**   L   - Lua 状态机
**   idx - 起始索引
**   n   - 旋转位数
**
** 算法说明:
** 设 x = AB,其中 A 是长度为 n 的前缀
** 则 rotate x n == BA
** 但 BA == (A^r . B^r)^r (其中 ^r 表示反转)
**
** 示例: 栈为 [1,2,3,4,5],从索引 1 旋转 2 位
** A = [1,2], B = [3,4,5]
** A^r = [2,1], B^r = [5,4,3]
** (A^r . B^r) = [2,1,5,4,3]
** (A^r . B^r)^r = [3,4,5,1,2]
*/
LUA_API void lua_rotate(lua_State *L, int idx, int n)
{
  StkId p, t, m;
  lua_lock(L);
  t = L->top.p - 1;                                             /* 正在旋转的栈片段的末尾 */
  p = index2stack(L, idx);                                      /* 片段的开始 */
  api_check(L, L->tbclist.p < p, "moving a to-be-closed slot"); /* 不能移动 TBC 变量 */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1); /* 前缀的末尾 */
  reverse(L, p, m);                 /* 反转长度为 n 的前缀 */
  reverse(L, m + 1, t);             /* 反转后缀 */
  reverse(L, p, t);                 /* 反转整个片段 */
  lua_unlock(L);
}

/*
** 复制元素
**
** 参数:
**   L       - Lua 状态机
**   fromidx - 源索引
**   toidx   - 目标索引
**
** 说明:
** - 将 fromidx 处的值复制到 toidx 处
** - 如果目标是 upvalue,需要设置 GC 屏障(barrier)
** - GC 屏障: 确保垃圾回收器正确跟踪对象引用关系
*/
LUA_API void lua_copy(lua_State *L, int fromidx, int toidx)
{
  TValue *fr, *to;
  lua_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");       /* 目标必须有效 */
  setobj(L, to, fr);                                   /* 复制值 */
  if (isupvalue(toidx))                                /* 是函数 upvalue 吗? */
    luaC_barrier(L, clCvalue(s2v(L->ci->func.p)), fr); /* 设置 GC 屏障 */
  /* LUA_REGISTRYINDEX 不需要 GC 屏障
     (收集器在完成收集前会重新访问它) */
  lua_unlock(L);
}

/*
** 将索引处的值压入栈顶
**
** 参数:
**   L   - Lua 状态机
**   idx - 要复制的值的索引
**
** 说明:
** - 等价于获取 idx 处的值并压栈
** - 常用于复制栈中的值
*/
LUA_API void lua_pushvalue(lua_State *L, int idx)
{
  lua_lock(L);
  setobj2s(L, L->top.p, index2value(L, idx)); /* 复制到栈顶 */
  api_incr_top(L);                            /* 递增栈顶 */
  lua_unlock(L);
}

/*
** ============================================================================
** 访问函数 (栈 -> C)
** 从栈中读取值到 C 程序
** ============================================================================
*/

/*
** 获取值的类型
**
** 参数:
**   L   - Lua 状态机
**   idx - 索引
**
** 返回: Lua 类型常量 (LUA_TNIL, LUA_TNUMBER, LUA_TSTRING 等)
**        如果索引无效返回 LUA_TNONE
**
** 类型包括: nil, boolean, number, string, table, function, userdata, thread
*/
LUA_API int lua_type(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : LUA_TNONE); /* ttype 获取类型标签 */
}

/*
** 获取类型名称
**
** 参数:
**   L - Lua 状态机
**   t - 类型常量
**
** 返回: 类型名称字符串(如 "nil", "number", "string" 等)
**
** 说明: ttypename 是一个查表函数,将类型常量转换为字符串
*/
LUA_API const char *lua_typename(lua_State *L, int t)
{
  UNUSED(L);
  api_check(L, LUA_TNONE <= t && t < LUA_NUMTYPES, "invalid type");
  return ttypename(t);
}

/*
** 测试是否为 C 函数
**
** 返回: 如果是 C 函数(轻量或闭包)返回真
**
** 说明:
** - ttislcf: 测试是否为轻量 C 函数(light C function)
** - ttisCclosure: 测试是否为 C 闭包
*/
LUA_API int lua_iscfunction(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}

/*
** 测试是否为整数
*/
LUA_API int lua_isinteger(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return ttisinteger(o); /* 检查类型标签是否为整数 */
}

/*
** 测试是否为数字(或可转换为数字的字符串)
**
** 说明: tonumber 宏尝试将值转换为数字
**       如果成功,将结果存入 n 并返回真
*/
LUA_API int lua_isnumber(lua_State *L, int idx)
{
  lua_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}

/*
** 测试是否为字符串(或可转换为字符串)
**
** 说明: cvt2str 检查值是否可以转换为字符串(主要是数字)
*/
LUA_API int lua_isstring(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}

/*
** 测试是否为 userdata
**
** 说明: userdata 有两种:
** - 完整 userdata: 由 Lua 管理内存
** - 轻量 userdata: 只是一个 C 指针
*/
LUA_API int lua_isuserdata(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}

/*
** 原始相等比较(不触发元方法)
**
** 参数:
**   L      - Lua 状态机
**   index1 - 第一个值的索引
**   index2 - 第二个值的索引
**
** 返回: 如果两个值原始相等返回真
**
** 说明: luaV_rawequalobj 进行原始比较,不调用 __eq 元方法
*/
LUA_API int lua_rawequal(lua_State *L, int index1, int index2)
{
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? luaV_rawequalobj(o1, o2) : 0;
}

/*
** 执行算术运算
**
** 参数:
**   L  - Lua 状态机
**   op - 运算符 (LUA_OPADD, LUA_OPSUB, LUA_OPMUL 等)
**
** 说明:
** - 除一元运算外,其他运算需要两个操作数
** - 一元运算(LUA_OPUNM 取负, LUA_OPBNOT 按位取反)添加假的第二操作数
** - 结果存放在第一个操作数的位置
** - 可能触发元方法(__add, __sub 等)
*/
LUA_API void lua_arith(lua_State *L, int op)
{
  lua_lock(L);
  if (op != LUA_OPUNM && op != LUA_OPBNOT)
    api_checkpop(L, 2); /* 其他操作需要两个操作数 */
  else
  { /* 一元操作,添加假的第二操作数 */
    api_checkpop(L, 1);
    setobjs2s(L, L->top.p, L->top.p - 1); /* 复制第一个操作数 */
    api_incr_top(L);
  }
  /* 第一个操作数在 top - 2,第二个在 top - 1;结果放到 top - 2 */
  luaO_arith(L, op, s2v(L->top.p - 2), s2v(L->top.p - 1), L->top.p - 2);
  L->top.p--; /* 弹出第二个操作数 */
  lua_unlock(L);
}

/*
** 比较两个值
**
** 参数:
**   L      - Lua 状态机
**   index1 - 第一个值
**   index2 - 第二个值
**   op     - 比较操作 (LUA_OPEQ 等于, LUA_OPLT 小于, LUA_OPLE 小于等于)
**
** 返回: 比较结果(真或假)
**
** 说明: 可能触发元方法(__eq, __lt, __le)
*/
LUA_API int lua_compare(lua_State *L, int index1, int index2, int op)
{
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  lua_lock(L); /* 可能调用标签方法(元方法) */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2))
  { /* 两个索引都有效? */
    switch (op)
    {
    case LUA_OPEQ:
      i = luaV_equalobj(L, o1, o2);
      break; /* 相等 */
    case LUA_OPLT:
      i = luaV_lessthan(L, o1, o2);
      break; /* 小于 */
    case LUA_OPLE:
      i = luaV_lessequal(L, o1, o2);
      break; /* 小于等于 */
    default:
      api_check(L, 0, "invalid option");
    }
  }
  lua_unlock(L);
  return i;
}

/*
** 将数字转换为 C 字符串
**
** 参数:
**   L    - Lua 状态机
**   idx  - 数字的索引
**   buff - 输出缓冲区
**
** 返回: 字符串长度(包括结尾的 '\0'),如果不是数字返回 0
**
** 说明: luaO_tostringbuff 将数字格式化为字符串
*/
LUA_API unsigned(lua_numbertocstring)(lua_State *L, int idx, char *buff)
{
  const TValue *o = index2value(L, idx);
  if (ttisnumber(o))
  {                                            /* 是数字吗? */
    unsigned len = luaO_tostringbuff(o, buff); /* 转换为字符串 */
    buff[len++] = '\0';                        /* 添加结尾零 */
    return len;
  }
  else
    return 0; /* 不是数字 */
}

/*
** 将字符串转换为数字并压栈
**
** 参数:
**   L - Lua 状态机
**   s - 字符串
**
** 返回: 字符串长度(如果转换成功),否则返回 0
**
** 说明: luaO_str2num 尝试解析字符串为数字
**       如果成功,将数字压入栈顶
*/
LUA_API size_t lua_stringtonumber(lua_State *L, const char *s)
{
  size_t sz = luaO_str2num(s, s2v(L->top.p)); /* 尝试转换 */
  if (sz != 0)
    api_incr_top(L); /* 转换成功,压栈 */
  return sz;
}

/*
** 将值转换为 Lua 浮点数
**
** 参数:
**   L      - Lua 状态机
**   idx    - 索引
**   pisnum - 输出参数,指示是否成功转换(可为 NULL)
**
** 返回: 转换后的数字,失败返回 0
*/
LUA_API lua_Number lua_tonumberx(lua_State *L, int idx, int *pisnum)
{
  lua_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n); /* 尝试转换 */
  if (pisnum)
    *pisnum = isnum; /* 设置成功标志 */
  return n;
}

/*
** 将值转换为 Lua 整数
**
** 类似 lua_tonumberx,但转换为整数
*/
LUA_API lua_Integer lua_tointegerx(lua_State *L, int idx, int *pisnum)
{
  lua_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res); /* 尝试转换为整数 */
  if (pisnum)
    *pisnum = isnum;
  return res;
}

/*
** 将值转换为布尔值
**
** 说明: 在 Lua 中,只有 false 和 nil 为假,其他都为真
**       l_isfalse 宏检查值是否为假
*/
LUA_API int lua_toboolean(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o); /* 取反 l_isfalse 的结果 */
}

/*
** 将值转换为 C 字符串
**
** 参数:
**   L   - Lua 状态机
**   idx - 索引
**   len - 输出字符串长度(可为 NULL)
**
** 返回: C 字符串指针,失败返回 NULL
**
** 说明:
** - 如果值不是字符串但可以转换(如数字),会修改栈上的值
** - 转换后可能触发 GC,所以需要重新获取索引
*/
LUA_API const char *lua_tolstring(lua_State *L, int idx, size_t *len)
{
  TValue *o;
  lua_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o))
  { /* 不是字符串? */
    if (!cvt2str(o))
    { /* 不可转换? */
      if (len != NULL)
        *len = 0;
      lua_unlock(L);
      return NULL;
    }
    luaO_tostring(L, o);     /* 转换为字符串 */
    luaC_checkGC(L);         /* 转换可能创建新字符串,检查 GC */
    o = index2value(L, idx); /* 前面的调用可能重新分配栈 */
  }
  lua_unlock(L);
  if (len != NULL)
    return getlstr(tsvalue(o), *len); /* 获取字符串和长度 */
  else
    return getstr(tsvalue(o)); /* 只获取字符串 */
}

/*
** 获取值的原始长度(不触发元方法)
**
** 返回:
** - 字符串: 字符串长度
** - userdata: userdata 大小
** - table: 数组部分的长度
** - 其他: 0
**
** 说明:
** - tsvalue(o)->shrlen: 短字符串的长度
** - tsvalue(o)->u.lnglen: 长字符串的长度
** - luaH_getn: 获取表的长度(只计算数组部分)
*/
LUA_API lua_Unsigned lua_rawlen(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o))
  { /* ttypetag 获取详细类型标签 */
  case LUA_VSHRSTR:
    return cast(lua_Unsigned, tsvalue(o)->shrlen); /* 短字符串 */
  case LUA_VLNGSTR:
    return cast(lua_Unsigned, tsvalue(o)->u.lnglen); /* 长字符串 */
  case LUA_VUSERDATA:
    return cast(lua_Unsigned, uvalue(o)->len); /* userdata */
  case LUA_VTABLE:
  { /* 表 */
    lua_Unsigned res;
    lua_lock(L);
    res = luaH_getn(L, hvalue(o)); /* 获取表长度 */
    lua_unlock(L);
    return res;
  }
  default:
    return 0; /* 其他类型返回 0 */
  }
}

/*
** 将值转换为 C 函数指针
**
** 返回: C 函数指针,如果不是 C 函数返回 NULL
**
** 说明:
** - fvalue: 从轻量 C 函数中提取函数指针
** - clCvalue(o)->f: 从 C 闭包中提取函数指针
*/
LUA_API lua_CFunction lua_tocfunction(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  if (ttislcf(o))
    return fvalue(o); /* 轻量 C 函数 */
  else if (ttisCclosure(o))
    return clCvalue(o)->f; /* C 闭包 */
  else
    return NULL; /* 不是 C 函数 */
}

/*
** 辅助函数: 将值转换为 userdata 指针
**
** 说明:
** - getudatamem: 获取完整 userdata 的内存块
** - pvalue: 获取轻量 userdata 的指针值
** - l_sinline: 内联函数标记
*/
l_sinline void *touserdata(const TValue *o)
{
  switch (ttype(o))
  {
  case LUA_TUSERDATA:
    return getudatamem(uvalue(o)); /* 完整 userdata */
  case LUA_TLIGHTUSERDATA:
    return pvalue(o); /* 轻量 userdata */
  default:
    return NULL;
  }
}

/*
** 将值转换为 userdata 指针
*/
LUA_API void *lua_touserdata(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}

/*
** 将值转换为线程(协程)
**
** 返回: lua_State 指针,如果不是线程返回 NULL
*/
LUA_API lua_State *lua_tothread(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}

/*
** 获取对象的内部表示指针
**
** 返回: 对象内部表示的指针(仅供参考,不应解引用)
**
** 说明:
** - ISO C 不允许将函数指针转换为 void*
** - 这里通过 size_t 中转来实现(仅作为信息性质)
** - 对于可收集对象,返回 GCObject 指针
** - 对于不可收集对象,返回 NULL
*/
LUA_API const void *lua_topointer(lua_State *L, int idx)
{
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o))
  {
  case LUA_VLCF:
    return cast_voidp(cast_sizet(fvalue(o))); /* 轻量 C 函数 */
  case LUA_VUSERDATA:
  case LUA_VLIGHTUSERDATA:
    return touserdata(o); /* userdata */
  default:
  {
    if (iscollectable(o)) /* 可收集对象? */
      return gcvalue(o);  /* 返回 GCObject 指针 */
    else
      return NULL; /* 不可收集对象 */
  }
  }
}

/*
** ============================================================================
** 压栈函数 (C -> 栈)
** 将 C 值压入 Lua 栈
** ============================================================================
*/

/*
** 压入 nil 值
*/
LUA_API void lua_pushnil(lua_State *L)
{
  lua_lock(L);
  setnilvalue(s2v(L->top.p)); /* 设置栈顶为 nil */
  api_incr_top(L);            /* 递增栈顶 */
  lua_unlock(L);
}

/*
** 压入浮点数
*/
LUA_API void lua_pushnumber(lua_State *L, lua_Number n)
{
  lua_lock(L);
  setfltvalue(s2v(L->top.p), n); /* 设置为浮点数值 */
  api_incr_top(L);
  lua_unlock(L);
}

/*
** 压入整数
*/
LUA_API void lua_pushinteger(lua_State *L, lua_Integer n)
{
  lua_lock(L);
  setivalue(s2v(L->top.p), n); /* 设置为整数值 */
  api_incr_top(L);
  lua_unlock(L);
}

/*
** 压入指定长度的字符串
**
** 参数:
**   L   - Lua 状态机
**   s   - 字符串指针
**   len - 字符串长度
**
** 返回: 内部字符串的地址
**
** 说明:
** - 避免在 len == 0 时使用 s(因为 s 可能为 NULL)
** - luaS_newlstr 创建或查找字符串(Lua 字符串是内部化的)
** - 可能触发 GC
*/
LUA_API const char *lua_pushlstring(lua_State *L, const char *s, size_t len)
{
  TString *ts;
  lua_lock(L);
  ts = (len == 0) ? luaS_new(L, "") : luaS_newlstr(L, s, len); /* 创建字符串 */
  setsvalue2s(L, L->top.p, ts);                                /* 设置为字符串值 */
  api_incr_top(L);
  luaC_checkGC(L); /* 可能触发 GC */
  lua_unlock(L);
  return getstr(ts); /* 返回内部字符串地址 */
}

/*
** 压入外部字符串
**
** 参数:
**   L      - Lua 状态机
**   s      - 字符串指针
**   len    - 字符串长度
**   falloc - 释放函数
**   ud     - 用户数据
**
** 说明: 这是 Lua 5.4 新增功能,允许使用外部分配的字符串
**       避免额外的内存复制
*/
LUA_API const char *lua_pushexternalstring(lua_State *L,
                                           const char *s, size_t len, lua_Alloc falloc, void *ud)
{
  TString *ts;
  lua_lock(L);
  api_check(L, len <= MAX_SIZE, "string too large");           /* 检查长度 */
  api_check(L, s[len] == '\0', "string not ending with zero"); /* 必须以零结尾 */
  ts = luaS_newextlstr(L, s, len, falloc, ud);                 /* 创建外部字符串 */
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return getstr(ts);
}

/*
** 压入 C 字符串(以零结尾)
**
** 参数:
**   L - Lua 状态机
**   s - C 字符串(可为 NULL)
**
** 返回: 内部字符串地址
**
** 说明: 如果 s 为 NULL,压入 nil
*/
LUA_API const char *lua_pushstring(lua_State *L, const char *s)
{
  lua_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top.p)); /* NULL 字符串压入 nil */
  else
  {
    TString *ts;
    ts = luaS_new(L, s); /* 创建字符串 */
    setsvalue2s(L, L->top.p, ts);
    s = getstr(ts); /* 内部副本的地址 */
  }
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return s;
}

/*
** 压入格式化字符串(使用 va_list)
**
** 参数:
**   L    - Lua 状态机
**   fmt  - 格式字符串
**   argp - 可变参数列表
**
** 返回: 格式化后的字符串
**
** 说明: luaO_pushvfstring 实现类似 sprintf 的功能
**       但只支持有限的格式符
*/
LUA_API const char *lua_pushvfstring(lua_State *L, const char *fmt,
                                     va_list argp)
{
  const char *ret;
  lua_lock(L);
  ret = luaO_pushvfstring(L, fmt, argp); /* 格式化并压栈 */
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}

/*
** 压入格式化字符串(使用可变参数)
**
** 说明: pushvfstring 是一个宏,处理可变参数列表
*/
LUA_API const char *lua_pushfstring(lua_State *L, const char *fmt, ...)
{
  const char *ret;
  va_list argp;
  lua_lock(L);
  pushvfstring(L, argp, fmt, ret); /* 宏展开为 va_start/va_end 调用 */
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}

/*
** 压入 C 闭包
**
** 参数:
**   L  - Lua 状态机
**   fn - C 函数指针
**   n  - upvalue 数量
**
** 说明:
** - 如果 n == 0,压入轻量 C 函数(不创建闭包对象)
** - 否则创建 C 闭包,并从栈顶取 n 个值作为 upvalue
** - upvalue 允许 C 函数携带状态
*/
LUA_API void lua_pushcclosure(lua_State *L, lua_CFunction fn, int n)
{
  lua_lock(L);
  if (n == 0)
  {                               /* 没有 upvalue */
    setfvalue(s2v(L->top.p), fn); /* 压入轻量 C 函数 */
    api_incr_top(L);
  }
  else
  { /* 有 upvalue,创建 C 闭包 */
    int i;
    CClosure *cl;
    api_checkpop(L, n); /* 检查栈上有 n 个值 */
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = luaF_newCclosure(L, n); /* 创建 C 闭包 */
    cl->f = fn;                  /* 设置函数指针 */
    /* 从栈顶取 n 个值作为 upvalue */
    for (i = 0; i < n; i++)
    {
      setobj2n(L, &cl->upvalue[i], s2v(L->top.p - n + i));
      /* 不需要屏障,因为闭包是白色的(新创建的对象) */
      lua_assert(iswhite(cl));
    }
    L->top.p -= n;                     /* 弹出 upvalue */
    setclCvalue(L, s2v(L->top.p), cl); /* 压入闭包 */
    api_incr_top(L);
    luaC_checkGC(L);
  }
  lua_unlock(L);
}

/*
** 压入布尔值
**
** 说明:
** - setbtvalue: 设置为 true
** - setbfvalue: 设置为 false
*/
LUA_API void lua_pushboolean(lua_State *L, int b)
{
  lua_lock(L);
  if (b)
    setbtvalue(s2v(L->top.p)); /* true */
  else
    setbfvalue(s2v(L->top.p)); /* false */
  api_incr_top(L);
  lua_unlock(L);
}

/*
** 压入轻量 userdata
**
** 参数:
**   L - Lua 状态机
**   p - C 指针
**
** 说明: 轻量 userdata 只是一个 C 指针,不由 Lua 管理内存
*/
LUA_API void lua_pushlightuserdata(lua_State *L, void *p)
{
  lua_lock(L);
  setpvalue(s2v(L->top.p), p); /* 设置指针值 */
  api_incr_top(L);
  lua_unlock(L);
}

/*
** 压入线程(协程)
**
** 返回: 如果是主线程返回 1,否则返回 0
*/
LUA_API int lua_pushthread(lua_State *L)
{
  lua_lock(L);
  setthvalue(L, s2v(L->top.p), L); /* 压入线程自身 */
  api_incr_top(L);
  lua_unlock(L);
  return (mainthread(G(L)) == L); /* 是主线程吗? */
}

/*
** ============================================================================
** 获取函数 (Lua -> 栈)
** 从 Lua 对象(表、全局变量等)获取值并压栈
** ============================================================================
*/

/*
** 辅助函数: 从表中获取字符串键的值
**
** 参数:
**   L - Lua 状态机
**   t - 表(TValue)
**   k - 键(C 字符串)
**
** 返回: 值的类型
**
** 说明:
** - luaV_fastget: 快速路径,尝试直接从表中获取
** - luaV_finishget: 慢速路径,可能触发 __index 元方法
*/
static int auxgetstr(lua_State *L, const TValue *t, const char *k)
{
  lu_byte tag;
  TString *str = luaS_new(L, k); /* 创建字符串键 */
  /* 尝试快速获取,luaH_getstr 是快速表查找宏 */
  luaV_fastget(t, str, s2v(L->top.p), luaH_getstr, tag);
  if (!tagisempty(tag)) /* 快速路径成功? */
    api_incr_top(L);
  else
  {                                /* 需要慢速路径 */
    setsvalue2s(L, L->top.p, str); /* 压入键 */
    api_incr_top(L);
    tag = luaV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag); /* 完成获取 */
  }
  lua_unlock(L);
  return novariant(tag); /* 返回值类型(去除变体标记) */
}

/*
** 获取全局表
**
** 说明:
** - 全局表存储在注册表的 LUA_RIDX_GLOBALS 索引处
** - 这个函数假设注册表不能是弱表,所以在紧急 GC 期间不会被收集
*/
static void getGlobalTable(lua_State *L, TValue *gt)
{
  Table *registry = hvalue(&G(L)->l_registry);               /* 获取注册表 */
  lu_byte tag = luaH_getint(registry, LUA_RIDX_GLOBALS, gt); /* 获取全局表 */
  (void)tag;                                                 /* 避免未使用警告 */
  api_check(L, novariant(tag) == LUA_TTABLE, "global table must exist");
}

/*
** 获取全局变量
**
** 参数:
**   L    - Lua 状态机
**   name - 变量名
**
** 返回: 值的类型
*/
LUA_API int lua_getglobal(lua_State *L, const char *name)
{
  TValue gt;
  lua_lock(L);
  getGlobalTable(L, &gt);         /* 获取全局表 */
  return auxgetstr(L, &gt, name); /* 从全局表中获取变量 */
}

/*
** 获取表中的值(使用栈顶作为键)
**
** 参数:
**   L   - Lua 状态机
**   idx - 表的索引
**
** 返回: 值的类型
**
** 说明: 键在栈顶,获取后键被替换为值
*/
LUA_API int lua_gettable(lua_State *L, int idx)
{
  lu_byte tag;
  TValue *t;
  lua_lock(L);
  api_checkpop(L, 1); /* 需要一个键 */
  t = index2value(L, idx);
  /* 尝试快速获取,luaH_get 是通用表查找 */
  luaV_fastget(t, s2v(L->top.p - 1), s2v(L->top.p - 1), luaH_get, tag);
  if (tagisempty(tag)) /* 需要慢速路径? */
    tag = luaV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  lua_unlock(L);
  return novariant(tag);
}

/*
** 获取表字段(使用字符串键)
**
** 参数:
**   L   - Lua 状态机
**   idx - 表的索引
**   k   - 字段名
**
** 返回: 值的类型
*/
LUA_API int lua_getfield(lua_State *L, int idx, const char *k)
{
  lua_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}

/*
** 获取表中整数索引的值
**
** 参数:
**   L   - Lua 状态机
**   idx - 表的索引
**   n   - 整数键
**
** 返回: 值的类型
**
** 说明: luaV_fastgeti 是整数索引的快速路径
*/
LUA_API int lua_geti(lua_State *L, int idx, lua_Integer n)
{
  TValue *t;
  lu_byte tag;
  lua_lock(L);
  t = index2value(L, idx);
  luaV_fastgeti(t, n, s2v(L->top.p), tag); /* 尝试快速获取 */
  if (tagisempty(tag))
  { /* 需要慢速路径? */
    TValue key;
    setivalue(&key, n); /* 创建整数键 */
    tag = luaV_finishget(L, t, &key, L->top.p, tag);
  }
  api_incr_top(L);
  lua_unlock(L);
  return novariant(tag);
}

/*
** 完成原始获取操作
**
** 说明: tagisempty 检查是否获取到空值(nil 或不存在)
**       避免将空项复制到栈上,直接设为 nil
*/
static int finishrawget(lua_State *L, lu_byte tag)
{
  if (tagisempty(tag)) /* 避免将空项复制到栈上 */
    setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  lua_unlock(L);
  return novariant(tag);
}

/*
** 辅助函数: 获取表对象
**
** 说明: hvalue 从 TValue 中提取表指针
*/
l_sinline Table *gettable(lua_State *L, int idx)
{
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected"); /* 必须是表 */
  return hvalue(t);
}

/*
** 原始获取(不触发元方法)
**
** 说明: 使用栈顶的值作为键,直接从表中获取值
*/
LUA_API int lua_rawget(lua_State *L, int idx)
{
  Table *t;
  lu_byte tag;
  lua_lock(L);
  api_checkpop(L, 1); /* 需要一个键 */
  t = gettable(L, idx);
  tag = luaH_get(t, s2v(L->top.p - 1), s2v(L->top.p - 1)); /* 直接获取 */
  L->top.p--;                                              /* 弹出键 */
  return finishrawget(L, tag);
}

/*
** 原始获取整数索引
*/
LUA_API int lua_rawgeti(lua_State *L, int idx, lua_Integer n)
{
  Table *t;
  lu_byte tag;
  lua_lock(L);
  t = gettable(L, idx);
  luaH_fastgeti(t, n, s2v(L->top.p), tag); /* 快速整数获取 */
  return finishrawget(L, tag);
}

/*
** 原始获取指针键
**
** 说明: 使用轻量 userdata(指针)作为键
*/
LUA_API int lua_rawgetp(lua_State *L, int idx, const void *p)
{
  Table *t;
  TValue k;
  lua_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p)); /* 创建指针键 */
  return finishrawget(L, luaH_get(t, &k, s2v(L->top.p)));
}

/*
** 创建新表
**
** 参数:
**   L      - Lua 状态机
**   narray - 预分配的数组大小
**   nrec   - 预分配的哈希大小
**
** 说明:
** - Lua 的表同时是数组和哈希表
** - 预分配可以提高性能,避免多次重新分配
*/
LUA_API void lua_createtable(lua_State *L, int narray, int nrec)
{
  Table *t;
  lua_lock(L);
  t = luaH_new(L);             /* 创建新表 */
  sethvalue2s(L, L->top.p, t); /* 压栈 */
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    luaH_resize(L, t, cast_uint(narray), cast_uint(nrec)); /* 预分配 */
  luaC_checkGC(L);
  lua_unlock(L);
}

/*
** 获取元表
**
** 参数:
**   L        - Lua 状态机
**   objindex - 对象索引
**
** 返回: 如果有元表返回 1,否则返回 0
**
** 说明:
** - 表和 userdata 可以有独立的元表
** - 其他类型共享全局元表(存储在 G(L)->mt 数组中)
*/
LUA_API int lua_getmetatable(lua_State *L, int objindex)
{
  const TValue *obj;
  Table *mt;
  int res = 0;
  lua_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj))
  {
  case LUA_TTABLE:
    mt = hvalue(obj)->metatable; /* 表的元表 */
    break;
  case LUA_TUSERDATA:
    mt = uvalue(obj)->metatable; /* userdata 的元表 */
    break;
  default:
    mt = G(L)->mt[ttype(obj)]; /* 类型的全局元表 */
    break;
  }
  if (mt != NULL)
  {                               /* 有元表? */
    sethvalue2s(L, L->top.p, mt); /* 压入元表 */
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}

/*
** 获取 userdata 的用户值
**
** 参数:
**   L   - Lua 状态机
**   idx - userdata 索引
**   n   - 用户值索引(从 1 开始)
**
** 返回: 用户值的类型
**
** 说明: Lua 5.4 允许 userdata 关联多个用户值
**       用于存储与 userdata 相关的 Lua 对象
*/
LUA_API int lua_getiuservalue(lua_State *L, int idx, int n)
{
  TValue *o;
  int t;
  lua_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue)
  { /* 索引无效? */
    setnilvalue(s2v(L->top.p));
    t = LUA_TNONE;
  }
  else
  {
    setobj2s(L, L->top.p, &uvalue(o)->uv[n - 1].uv); /* 获取用户值 */
    t = ttype(s2v(L->top.p));
  }
  api_incr_top(L);
  lua_unlock(L);
  return t;
}

/*
** ============================================================================
** 设置函数 (栈 -> Lua)
** 将栈上的值设置到 Lua 对象中
** ============================================================================
*/

/*
** t[k] = 栈顶的值 (其中 'k' 是字符串)
**
** 说明:
** - luaV_fastset: 快速路径,尝试直接设置
** - luaV_finishfastset: 快速路径的后续处理(如 GC 屏障)
** - luaV_finishset: 慢速路径,可能触发 __newindex 元方法
** - hres: 快速设置的结果(HOK 表示成功)
*/
static void auxsetstr(lua_State *L, const TValue *t, const char *k)
{
  int hres;
  TString *str = luaS_new(L, k);
  api_checkpop(L, 1); /* 需要一个值 */
  /* 尝试快速设置,luaH_psetstr 是字符串键的快速设置 */
  luaV_fastset(t, str, s2v(L->top.p - 1), hres, luaH_psetstr);
  if (hres == HOK)
  {                                              /* 快速路径成功? */
    luaV_finishfastset(L, t, s2v(L->top.p - 1)); /* 完成快速设置(GC 屏障等) */
    L->top.p--;                                  /* 弹出值 */
  }
  else
  {                                /* 需要慢速路径 */
    setsvalue2s(L, L->top.p, str); /* 压入键(使其成为 TValue) */
    api_incr_top(L);
    luaV_finishset(L, t, s2v(L->top.p - 1), s2v(L->top.p - 2), hres);
    L->top.p -= 2; /* 弹出值和键 */
  }
  lua_unlock(L); /* 锁由调用者完成 */
}

/*
** 设置全局变量
**
** 参数:
**   L    - Lua 状态机
**   name - 变量名
**
** 说明: 将栈顶的值设置为全局变量
*/
LUA_API void lua_setglobal(lua_State *L, const char *name)
{
  TValue gt;
  lua_lock(L);             /* 解锁在 'auxsetstr' 中完成 */
  getGlobalTable(L, &gt);  /* 获取全局表 */
  auxsetstr(L, &gt, name); /* 设置字段 */
}

/*
** 设置表中的值(使用栈上的键和值)
**
** 说明: 栈顶是值,栈顶-1 是键
*/
LUA_API void lua_settable(lua_State *L, int idx)
{
  TValue *t;
  int hres;
  lua_lock(L);
  api_checkpop(L, 2); /* 需要键和值 */
  t = index2value(L, idx);
  /* 尝试快速设置,luaH_pset 是通用快速设置 */
  luaV_fastset(t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres, luaH_pset);
  if (hres == HOK)
    luaV_finishfastset(L, t, s2v(L->top.p - 1));
  else
    luaV_finishset(L, t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres);
  L->top.p -= 2; /* 弹出索引和值 */
  lua_unlock(L);
}

/*
** 设置表字段
**
** 参数:
**   L   - Lua 状态机
**   idx - 表的索引
**   k   - 字段名
*/
LUA_API void lua_setfield(lua_State *L, int idx, const char *k)
{
  lua_lock(L); /* 解锁在 'auxsetstr' 中完成 */
  auxsetstr(L, index2value(L, idx), k);
}

/*
** 设置表中整数索引的值
**
** 说明: luaV_fastseti 是整数索引的快速设置路径
*/
LUA_API void lua_seti(lua_State *L, int idx, lua_Integer n)
{
  TValue *t;
  int hres;
  lua_lock(L);
  api_checkpop(L, 1); /* 需要一个值 */
  t = index2value(L, idx);
  luaV_fastseti(t, n, s2v(L->top.p - 1), hres); /* 尝试快速设置 */
  if (hres == HOK)
    luaV_finishfastset(L, t, s2v(L->top.p - 1));
  else
  { /* 慢速路径 */
    TValue temp;
    setivalue(&temp, n); /* 创建整数键 */
    luaV_finishset(L, t, &temp, s2v(L->top.p - 1), hres);
  }
  L->top.p--; /* 弹出值 */
  lua_unlock(L);
}

/*
** 原始设置的辅助函数
**
** 参数:
**   L   - Lua 状态机
**   idx - 表的索引
**   key - 键
**   n   - 要弹出的元素数量
**
** 说明:
** - luaH_set: 直接设置表元素,不触发元方法
** - invalidateTMcache: 使元方法缓存失效(因为表可能有新的元方法)
** - luaC_barrierback: 后向 GC 屏障,确保新旧对象的引用正确
*/
static void aux_rawset(lua_State *L, int idx, TValue *key, int n)
{
  Table *t;
  lua_lock(L);
  api_checkpop(L, n);
  t = gettable(L, idx);
  luaH_set(L, t, key, s2v(L->top.p - 1));             /* 直接设置 */
  invalidateTMcache(t);                               /* 使元方法缓存失效 */
  luaC_barrierback(L, obj2gco(t), s2v(L->top.p - 1)); /* GC 屏障 */
  L->top.p -= n;
  lua_unlock(L);
}

/*
** 原始设置(不触发元方法)
**
** 说明: 使用栈顶-1 作为键,栈顶作为值
*/
LUA_API void lua_rawset(lua_State *L, int idx)
{
  aux_rawset(L, idx, s2v(L->top.p - 2), 2); /* 弹出 2 个元素(键和值) */
}

/*
** 原始设置指针键
*/
LUA_API void lua_rawsetp(lua_State *L, int idx, const void *p)
{
  TValue k;
  setpvalue(&k, cast_voidp(p)); /* 创建指针键 */
  aux_rawset(L, idx, &k, 1);    /* 弹出 1 个元素(值) */
}

/*
** 原始设置整数索引
*/
LUA_API void lua_rawseti(lua_State *L, int idx, lua_Integer n)
{
  Table *t;
  lua_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  luaH_setint(L, t, n, s2v(L->top.p - 1)); /* 直接设置整数索引 */
  luaC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p--;
  lua_unlock(L);
}

/*
** 设置元表
**
** 参数:
**   L        - Lua 状态机
**   objindex - 对象索引
**
** 返回: 始终返回 1
**
** 说明:
** - 栈顶的值是新元表(或 nil 表示移除元表)
** - 设置元表后需要检查是否有 __gc 元方法,如果有需要标记对象
*/
LUA_API int lua_setmetatable(lua_State *L, int objindex)
{
  TValue *obj;
  Table *mt;
  lua_lock(L);
  api_checkpop(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top.p - 1))) /* 栈顶是 nil? */
    mt = NULL;                    /* 移除元表 */
  else
  {
    api_check(L, ttistable(s2v(L->top.p - 1)), "table expected");
    mt = hvalue(s2v(L->top.p - 1)); /* 新元表 */
  }
  switch (ttype(obj))
  {
  case LUA_TTABLE:
  {
    hvalue(obj)->metatable = mt; /* 设置表的元表 */
    if (mt)
    {
      luaC_objbarrier(L, gcvalue(obj), mt);     /* GC 屏障 */
      luaC_checkfinalizer(L, gcvalue(obj), mt); /* 检查析构器 */
    }
    break;
  }
  case LUA_TUSERDATA:
  {
    uvalue(obj)->metatable = mt; /* 设置 userdata 的元表 */
    if (mt)
    {
      luaC_objbarrier(L, uvalue(obj), mt);
      luaC_checkfinalizer(L, gcvalue(obj), mt);
    }
    break;
  }
  default:
  {
    G(L)->mt[ttype(obj)] = mt; /* 设置类型的全局元表 */
    break;
  }
  }
  L->top.p--;
  lua_unlock(L);
  return 1;
}

/*
** 设置 userdata 的用户值
**
** 参数:
**   L   - Lua 状态机
**   idx - userdata 索引
**   n   - 用户值索引
**
** 返回: 如果成功返回 1,索引无效返回 0
*/
LUA_API int lua_setiuservalue(lua_State *L, int idx, int n)
{
  TValue *o;
  int res;
  lua_lock(L);
  api_checkpop(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0; /* n 不在 [1, uvalue(o)->nuvalue] 范围内 */
  else
  {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top.p - 1)); /* 设置用户值 */
    luaC_barrierback(L, gcvalue(o), s2v(L->top.p - 1));     /* GC 屏障 */
    res = 1;
  }
  L->top.p--;
  lua_unlock(L);
  return res;
}

/*
** ============================================================================
** 'load' 和 'call' 函数 (运行 Lua 代码)
** ============================================================================
*/

/*
** 检查结果数量是否合法
**
** 说明:
** - LUA_MULTRET: 表示接受任意数量的返回值
** - 否则检查栈空间是否足够容纳返回值
*/
#define checkresults(L, na, nr)                                                  \
  (api_check(L, (nr) == LUA_MULTRET || (L->ci->top.p - L->top.p >= (nr) - (na)), \
             "results from function overflow current stack size"),               \
   api_check(L, LUA_MULTRET <= (nr) && (nr) <= MAXRESULTS,                       \
             "invalid number of results"))

/*
** 调用函数(支持延续)
**
** 参数:
**   L        - Lua 状态机
**   nargs    - 参数数量
**   nresults - 期望的返回值数量(或 LUA_MULTRET)
**   ctx      - 延续上下文
**   k        - 延续函数
**
** 说明:
** - 延续(continuation): 当函数 yield 时,k 函数会在恢复时被调用
** - isLua: 检查是否在 Lua 函数中(Lua 函数中不能使用延续)
** - yieldable: 检查当前协程是否可以 yield
*/
LUA_API void lua_callk(lua_State *L, int nargs, int nresults,
                       lua_KContext ctx, lua_KFunction k)
{
  StkId func;
  lua_lock(L);
  api_check(L, k == NULL || !isLua(L->ci),
            "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1); /* 参数 + 函数 */
  api_check(L, L->status == LUA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top.p - (nargs + 1); /* 函数位置 */
  if (k != NULL && yieldable(L))
  {                               /* 需要准备延续? */
    L->ci->u.c.k = k;             /* 保存延续函数 */
    L->ci->u.c.ctx = ctx;         /* 保存上下文 */
    luaD_call(L, func, nresults); /* 执行调用 */
  }
  else                                   /* 无延续或不可 yield */
    luaD_callnoyield(L, func, nresults); /* 直接调用 */
  adjustresults(L, nresults);            /* 调整返回值数量 */
  lua_unlock(L);
}

/*
** 执行保护模式调用
*/
struct CallS
{               /* f_call 的数据 */
  StkId func;   /* 要调用的函数 */
  int nresults; /* 期望的返回值数量 */
};

/*
** 辅助函数: 执行实际调用
**
** 说明: 这个函数在 luaD_pcall 中被调用,在保护模式下运行
*/
static void f_call(lua_State *L, void *ud)
{
  struct CallS *c = cast(struct CallS *, ud);
  luaD_callnoyield(L, c->func, c->nresults);
}

/*
** 保护模式调用(pcall)
**
** 参数:
**   L        - Lua 状态机
**   nargs    - 参数数量
**   nresults - 期望的返回值数量
**   errfunc  - 错误处理函数的索引(0 表示无)
**   ctx      - 延续上下文
**   k        - 延续函数
**
** 返回: 状态码(LUA_OK, LUA_ERRRUN 等)
**
** 说明:
** - pcall 捕获错误,不会触发 panic
** - 如果有错误处理函数,会在错误时调用它
** - 错误处理函数可以修改错误对象
*/
LUA_API int lua_pcallk(lua_State *L, int nargs, int nresults, int errfunc,
                       lua_KContext ctx, lua_KFunction k)
{
  struct CallS c;
  TStatus status;
  ptrdiff_t func;
  lua_lock(L);
  api_check(L, k == NULL || !isLua(L->ci),
            "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == LUA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0; /* 无错误处理函数 */
  else
  {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o); /* 保存错误处理函数的位置 */
  }
  c.func = L->top.p - (nargs + 1); /* 要调用的函数 */
  if (k == NULL || !yieldable(L))
  {                        /* 无延续或不可 yield? */
    c.nresults = nresults; /* 执行常规保护调用 */
    status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else
  { /* 准备延续(调用已由 'resume' 保护) */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;     /* 保存延续 */
    ci->u.c.ctx = ctx; /* 保存上下文 */
    /* 保存错误恢复信息 */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci, L->allowhook);       /* 保存 'allowhook' 的值 */
    ci->callstatus |= CIST_YPCALL;  /* 函数可以进行错误恢复 */
    luaD_call(L, c.func, nresults); /* 执行调用 */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = LUA_OK; /* 如果到这里,没有错误 */
  }
  adjustresults(L, nresults);
  lua_unlock(L);
  return APIstatus(status); /* 转换内部状态码为 API 状态码 */
}

/*
** 加载 Lua 代码块
**
** 参数:
**   L         - Lua 状态机
**   reader    - 读取函数
**   data      - reader 的用户数据
**   chunkname - 代码块名称(用于错误信息)
**   mode      - 模式字符串("b" 二进制, "t" 文本, "bt" 两者)
**
** 返回: 状态码
**
** 说明:
** - ZIO: Lua 的输入流抽象
** - luaD_protectedparser: 在保护模式下解析代码
** - 如果成功,会将编译后的函数压栈
** - 函数的第一个 upvalue 设置为全局表(_ENV)
*/
LUA_API int lua_load(lua_State *L, lua_Reader reader, void *data,
                     const char *chunkname, const char *mode)
{
  ZIO z;
  TStatus status;
  lua_lock(L);
  if (!chunkname)
    chunkname = "?";
  luaZ_init(L, &z, reader, data);                        /* 初始化输入流 */
  status = luaD_protectedparser(L, &z, chunkname, mode); /* 解析 */
  if (status == LUA_OK)
  {                                            /* 没有错误? */
    LClosure *f = clLvalue(s2v(L->top.p - 1)); /* 获取新函数 */
    if (f->nupvalues >= 1)
    { /* 有 upvalue? */
      /* 从注册表获取全局表 */
      TValue gt;
      getGlobalTable(L, &gt);
      /* 将全局表设置为 f 的第一个 upvalue(可能是 LUA_ENV) */
      setobj(L, f->upvals[0]->v.p, &gt);
      luaC_barrier(L, f->upvals[0], &gt); /* GC 屏障 */
    }
  }
  lua_unlock(L);
  return APIstatus(status);
}

/*
** 转储 Lua 函数
**
** 参数:
**   L      - Lua 状态机
**   writer - 写入函数
**   data   - writer 的用户数据
**   strip  - 是否去除调试信息
**
** 返回: 状态码
**
** 说明:
** - 调用 'writer' 写入函数的各个部分
** - 确保栈返回时大小不变
** - luaU_dump: 序列化函数为字节码
*/
LUA_API int lua_dump(lua_State *L, lua_Writer writer, void *data, int strip)
{
  int status;
  ptrdiff_t otop = savestack(L, L->top.p); /* 原始栈顶 */
  TValue *f = s2v(L->top.p - 1);           /* 要转储的函数 */
  lua_lock(L);
  api_checkpop(L, 1);
  api_check(L, isLfunction(f), "Lua function expected");
  status = luaU_dump(L, clLvalue(f)->p, writer, data, strip);
  L->top.p = restorestack(L, otop); /* 恢复栈顶 */
  lua_unlock(L);
  return status;
}

/*
** 获取协程状态
**
** 返回: 协程状态(LUA_OK, LUA_YIELD 等)
*/
LUA_API int lua_status(lua_State *L)
{
  return APIstatus(L->status);
}

/*
** ============================================================================
** 垃圾回收函数
** ============================================================================
*/

/*
** 垃圾回收控制
**
** 参数:
**   L    - Lua 状态机
**   what - 操作类型
**   ...  - 可变参数(取决于操作类型)
**
** 返回: 取决于操作类型
**
** 说明:
** - LUA_GCSTOP: 停止 GC
** - LUA_GCRESTART: 重启 GC
** - LUA_GCCOLLECT: 执行完整 GC
** - LUA_GCCOUNT: 获取内存使用量(KB)
** - LUA_GCCOUNTB: 获取内存使用量的余数(字节)
** - LUA_GCSTEP: 执行一步 GC
** - LUA_GCISRUNNING: GC 是否在运行
** - LUA_GCGEN: 切换到分代 GC
** - LUA_GCINC: 切换到增量 GC
** - LUA_GCPARAM: 设置/获取 GC 参数
*/
LUA_API int lua_gc(lua_State *L, int what, ...)
{
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & (GCSTPGC | GCSTPCLS)) /* 内部停止? */
    return -1;                         /* 停止时所有选项都无效 */
  lua_lock(L);
  va_start(argp, what);
  switch (what)
  {
  case LUA_GCSTOP:
  {
    g->gcstp = GCSTPUSR; /* 由用户停止 */
    break;
  }
  case LUA_GCRESTART:
  {
    luaE_setdebt(g, 0); /* 重置 GC 债务 */
    g->gcstp = 0;       /* 其他位必须在这里为零 */
    break;
  }
  case LUA_GCCOLLECT:
  {
    luaC_fullgc(L, 0); /* 执行完整 GC */
    break;
  }
  case LUA_GCCOUNT:
  {
    /* GC 值以 KB 表示: #bytes/2^10 */
    res = cast_int(gettotalbytes(g) >> 10);
    break;
  }
  case LUA_GCCOUNTB:
  {
    res = cast_int(gettotalbytes(g) & 0x3ff); /* 余数部分 */
    break;
  }
  case LUA_GCSTEP:
  {
    lu_byte oldstp = g->gcstp;
    l_mem n = cast(l_mem, va_arg(argp, size_t));
    int work = 0; /* GC 是否做了工作 */
    g->gcstp = 0; /* 允许 GC 运行 */
    if (n <= 0)
      n = g->GCdebt; /* 强制运行一个基本步骤 */
    luaE_setdebt(g, g->GCdebt - n);
    luaC_condGC(L, (void)0, work = 1);  /* 条件性运行 GC */
    if (work && g->gcstate == GCSpause) /* 周期结束? */
      res = 1;                          /* 发出信号 */
    g->gcstp = oldstp;                  /* 恢复之前的状态 */
    break;
  }
  case LUA_GCISRUNNING:
  {
    res = gcrunning(g); /* GC 是否在运行 */
    break;
  }
  case LUA_GCGEN:
  {
    res = (g->gckind == KGC_INC) ? LUA_GCINC : LUA_GCGEN;
    luaC_changemode(L, KGC_GENMINOR); /* 切换到分代模式 */
    break;
  }
  case LUA_GCINC:
  {
    res = (g->gckind == KGC_INC) ? LUA_GCINC : LUA_GCGEN;
    luaC_changemode(L, KGC_INC); /* 切换到增量模式 */
    break;
  }
  case LUA_GCPARAM:
  {
    int param = va_arg(argp, int);
    int value = va_arg(argp, int);
    api_check(L, 0 <= param && param < LUA_GCPN, "invalid parameter");
    res = cast_int(luaO_applyparam(g->gcparams[param], 100)); /* 获取当前值 */
    if (value >= 0)
      g->gcparams[param] = luaO_codeparam(cast_uint(value)); /* 设置新值 */
    break;
  }
  default:
    res = -1; /* 无效选项 */
  }
  va_end(argp);
  lua_unlock(L);
  return res;
}

/*
** ============================================================================
** 杂项函数
** ============================================================================
*/

/*
** 抛出错误
**
** 说明:
** - 错误对象在栈顶
** - 如果错误对象是内存错误消息,触发内存错误
** - 否则触发常规错误
** - 此函数不会返回(通过 longjmp 跳出)
*/
LUA_API int lua_error(lua_State *L)
{
  TValue *errobj;
  lua_lock(L);
  errobj = s2v(L->top.p - 1);
  api_checkpop(L, 1);
  /* 错误对象是内存错误消息吗? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    luaM_error(L); /* 触发内存错误 */
  else
    luaG_errormsg(L); /* 触发常规错误 */
  /* 代码不可达;当控制实际离开内核时会解锁 */
  return 0; /* 避免警告 */
}

/*
** 表迭代
**
** 参数:
**   L   - Lua 状态机
**   idx - 表的索引
**
** 返回: 如果还有下一个元素返回 1,否则返回 0
**
** 说明:
** - 栈顶是当前键,会被替换为下一个键和值
** - 如果返回 0,会弹出键
** - 通常在循环中使用
*/
LUA_API int lua_next(lua_State *L, int idx)
{
  Table *t;
  int more;
  lua_lock(L);
  api_checkpop(L, 1); /* 需要一个键 */
  t = gettable(L, idx);
  more = luaH_next(L, t, L->top.p - 1); /* 获取下一个键值对 */
  if (more)
    api_incr_top(L); /* 压入值(键已在栈上) */
  else               /* 没有更多元素 */
    L->top.p--;      /* 弹出键 */
  lua_unlock(L);
  return more;
}

/*
** 标记变量为 to-be-closed
**
** 参数:
**   L   - Lua 状态机
**   idx - 变量的索引
**
** 说明:
** - Lua 5.4 新增功能
** - 标记的变量离开作用域时会调用其 __close 元方法
** - 类似于 C++ 的 RAII 或 Python 的 with 语句
*/
LUA_API void lua_toclose(lua_State *L, int idx)
{
  StkId o;
  lua_lock(L);
  o = index2stack(L, idx);
  api_check(L, L->tbclist.p < o, "given index below or equal a marked one");
  luaF_newtbcupval(L, o);        /* 创建新的 to-be-closed upvalue */
  L->ci->callstatus |= CIST_TBC; /* 标记函数有 TBC 槽 */
  lua_unlock(L);
}

/*
** 连接栈顶的 n 个值
**
** 参数:
**   L - Lua 状态机
**   n - 要连接的值的数量
**
** 说明:
** - 使用 Lua 的连接操作符(..)
** - 可能触发 __concat 元方法
** - 如果 n == 0,压入空字符串
*/
LUA_API void lua_concat(lua_State *L, int n)
{
  lua_lock(L);
  api_checknelems(L, n); /* 检查栈上有 n 个元素 */
  if (n > 0)
  {
    luaV_concat(L, n); /* 执行连接 */
    luaC_checkGC(L);
  }
  else
  {                                                   /* 没有要连接的 */
    setsvalue2s(L, L->top.p, luaS_newlstr(L, "", 0)); /* 压入空字符串 */
    api_incr_top(L);
  }
  lua_unlock(L);
}

/*
** 获取长度
**
** 参数:
**   L   - Lua 状态机
**   idx - 对象索引
**
** 说明:
** - 对字符串,返回字符数
** - 对表,返回长度(可能触发 __len 元方法)
** - 结果压入栈顶
*/
LUA_API void lua_len(lua_State *L, int idx)
{
  TValue *t;
  lua_lock(L);
  t = index2value(L, idx);
  luaV_objlen(L, L->top.p, t); /* 获取长度 */
  api_incr_top(L);
  lua_unlock(L);
}

/*
** 获取内存分配器
**
** 参数:
**   L  - Lua 状态机
**   ud - 输出用户数据指针(可为 NULL)
**
** 返回: 内存分配函数
**
** 说明: 允许宿主程序查询当前使用的分配器
*/
LUA_API lua_Alloc lua_getallocf(lua_State *L, void **ud)
{
  lua_Alloc f;
  lua_lock(L);
  if (ud)
    *ud = G(L)->ud;   /* 获取用户数据 */
  f = G(L)->frealloc; /* 获取分配函数 */
  lua_unlock(L);
  return f;
}

/*
** 设置内存分配器
**
** 参数:
**   L  - Lua 状态机
**   f  - 新的分配函数
**   ud - 用户数据
**
** 说明: 允许宿主程序自定义内存管理
*/
LUA_API void lua_setallocf(lua_State *L, lua_Alloc f, void *ud)
{
  lua_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  lua_unlock(L);
}

/*
** 设置警告函数
**
** 参数:
**   L  - Lua 状态机
**   f  - 警告函数
**   ud - 用户数据
**
** 说明: Lua 5.4 新增,允许自定义警告处理
*/
void lua_setwarnf(lua_State *L, lua_WarnFunction f, void *ud)
{
  lua_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  lua_unlock(L);
}

/*
** 发出警告
**
** 参数:
**   L      - Lua 状态机
**   msg    - 警告消息
**   tocont - 是否继续(与下一条消息连接)
*/
void lua_warning(lua_State *L, const char *msg, int tocont)
{
  lua_lock(L);
  luaE_warning(L, msg, tocont);
  lua_unlock(L);
}

/*
** 创建新的 userdata
**
** 参数:
**   L       - Lua 状态机
**   size    - userdata 大小
**   nuvalue - 用户值数量
**
** 返回: userdata 的内存块指针
**
** 说明:
** - userdata 是 Lua 中存储 C 数据的机制
** - nuvalue 个用户值可以关联 Lua 对象
** - SHRT_MAX: short 类型的最大值
*/
LUA_API void *lua_newuserdatauv(lua_State *L, size_t size, int nuvalue)
{
  Udata *u;
  lua_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < SHRT_MAX, "invalid value");
  u = luaS_newudata(L, size, cast(unsigned short, nuvalue)); /* 创建 userdata */
  setuvalue(L, s2v(L->top.p), u);                            /* 压栈 */
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return getudatamem(u); /* 返回内存块 */
}

/*
** ============================================================================
** Upvalue 操作
** ============================================================================
*/

/*
** 辅助函数: 获取 upvalue
**
** 参数:
**   fi    - 函数(TValue)
**   n     - upvalue 索引(从 1 开始)
**   val   - 输出 upvalue 的值指针
**   owner - 输出拥有者(GCObject)
**
** 返回: upvalue 名称,如果无效返回 NULL
**
** 说明:
** - C 闭包的 upvalue 直接存储在闭包中
** - Lua 闭包的 upvalue 是独立的 UpVal 对象
*/
static const char *aux_upvalue(TValue *fi, int n, TValue **val,
                               GCObject **owner)
{
  switch (ttypetag(fi))
  {
  case LUA_VCCL:
  { /* C 闭包 */
    CClosure *f = clCvalue(fi);
    if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
      return NULL; /* n 不在 [1, f->nupvalues] 范围内 */
    *val = &f->upvalue[n - 1];
    if (owner)
      *owner = obj2gco(f);
    return ""; /* C 闭包没有 upvalue 名称 */
  }
  case LUA_VLCL:
  { /* Lua 闭包 */
    LClosure *f = clLvalue(fi);
    TString *name;
    Proto *p = f->p;
    if (!(cast_uint(n) - 1u < cast_uint(p->sizeupvalues)))
      return NULL;                /* n 不在 [1, p->sizeupvalues] 范围内 */
    *val = f->upvals[n - 1]->v.p; /* 获取 upvalue 值 */
    if (owner)
      *owner = obj2gco(f->upvals[n - 1]);
    name = p->upvalues[n - 1].name; /* 获取 upvalue 名称 */
    return (name == NULL) ? "(no name)" : getstr(name);
  }
  default:
    return NULL; /* 不是闭包 */
  }
}

/*
** 获取 upvalue 并压栈
**
** 参数:
**   L         - Lua 状态机
**   funcindex - 函数索引
**   n         - upvalue 索引
**
** 返回: upvalue 名称,如果无效返回 NULL
*/
LUA_API const char *lua_getupvalue(lua_State *L, int funcindex, int n)
{
  const char *name;
  TValue *val = NULL; /* 避免警告 */
  lua_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name)
  {
    setobj2s(L, L->top.p, val); /* 压入 upvalue */
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}

/*
** 设置 upvalue
**
** 参数:
**   L         - Lua 状态机
**   funcindex - 函数索引
**   n         - upvalue 索引
**
** 返回: upvalue 名称,如果无效返回 NULL
**
** 说明: 使用栈顶的值设置 upvalue
*/
LUA_API const char *lua_setupvalue(lua_State *L, int funcindex, int n)
{
  const char *name;
  TValue *val = NULL;     /* 避免警告 */
  GCObject *owner = NULL; /* 避免警告 */
  TValue *fi;
  lua_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1); /* 需要一个值 */
  name = aux_upvalue(fi, n, &val, &owner);
  if (name)
  {
    L->top.p--;
    setobj(L, val, s2v(L->top.p)); /* 设置 upvalue */
    luaC_barrier(L, owner, val);   /* GC 屏障 */
  }
  lua_unlock(L);
  return name;
}

/*
** 获取 upvalue 引用
**
** 说明: 用于 lua_upvalueid 和 lua_upvaluejoin
*/
static UpVal **getupvalref(lua_State *L, int fidx, int n, LClosure **pf)
{
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Lua function expected");
  f = clLvalue(fi);
  if (pf)
    *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1]; /* 获取 upvalue 指针 */
  else
    return (UpVal **)&nullup; /* 无效索引 */
}

/*
** 获取 upvalue 的唯一 ID
**
** 参数:
**   L    - Lua 状态机
**   fidx - 函数索引
**   n    - upvalue 索引
**
** 返回: upvalue 的内部地址(用作 ID)
**
** 说明: 可用于判断两个闭包是否共享同一个 upvalue
*/
LUA_API void *lua_upvalueid(lua_State *L, int fidx, int n)
{
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi))
  {
  case LUA_VLCL:
  { /* Lua 闭包 */
    return *getupvalref(L, fidx, n, NULL);
  }
  case LUA_VCCL:
  { /* C 闭包 */
    CClosure *f = clCvalue(fi);
    if (1 <= n && n <= f->nupvalues)
      return &f->upvalue[n - 1];
    /* else */
  } /* FALLTHROUGH */
  case LUA_VLCF:
    return NULL; /* 轻量 C 函数没有 upvalue */
  default:
  {
    api_check(L, 0, "function expected");
    return NULL;
  }
  }
}

/*
** 连接两个 upvalue
**
** 参数:
**   L     - Lua 状态机
**   fidx1 - 第一个函数索引
**   n1    - 第一个 upvalue 索引
**   fidx2 - 第二个函数索引
**   n2    - 第二个 upvalue 索引
**
** 说明: 使 fidx1 的 n1 upvalue 指向 fidx2 的 n2 upvalue
**       之后两个闭包共享同一个 upvalue
*/
LUA_API void lua_upvaluejoin(lua_State *L, int fidx1, int n1,
                             int fidx2, int n2)
{
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;                  /* 共享 upvalue */
  luaC_objbarrier(L, f1, *up1); /* GC 屏障 */
}

/*
** ============================================================================
** 文件总结
** ============================================================================
**
** 本文件实现了 Lua 的完整 C API,是 C 程序与 Lua 交互的桥梁。
**
** 核心设计思想:
**
** 1. 栈式接口
**    - 所有数据交换都通过虚拟栈进行
**    - 栈索引可以是正数(绝对位置)、负数(相对栈顶)或伪索引
**    - 这种设计简化了内存管理,避免了复杂的指针操作
**
** 2. 类型安全
**    - 使用 api_check 宏进行运行时类型检查
**    - 提供各种类型测试和转换函数
**    - 确保 C 代码不会破坏 Lua 内部状态
**
** 3. 垃圾回收集成
**    - 几乎所有创建对象的操作都会检查是否需要 GC
**    - 使用 GC 屏障(barrier)确保引用关系正确
**    - 支持 to-be-closed 变量(类似 RAII)
**
** 4. 错误处理
**    - 提供保护模式调用(pcall)捕获错误
**    - 支持错误处理函数和延续(continuation)
**    - panic 函数处理不可恢复的错误
**
** 5. 元机制
**    - 支持元表和元方法
**    - 快速路径优化常见操作
**    - 慢速路径处理复杂情况和元方法调用
**
** 重要概念解释:
**
** - TValue: Lua 值的内部表示,包含类型和值
** - StkId: 栈位置(指针)
** - CallInfo: 调用帧,存储函数调用信息
** - UpVal: 上值,闭包捕获的外部变量
** - GCObject: 可被垃圾回收的对象
**
** 高级 C 用法:
**
** - va_list/va_start/va_end: 可变参数处理
** - ptrdiff_t: 指针差值类型,保证在不同平台上正确
** - cast 宏: 类型转换,提高代码可读性
** - inline 函数: 小函数内联以提高性能
** - const: 常量限定符,防止意外修改
**
** 性能优化技巧:
**
** - 快速路径/慢速路径模式
** - 字符串内部化(interning)
** - 预分配表大小
** - 内联小函数
** - 宏展开减少函数调用开销
**
** 使用建议:
**
** - 始终检查栈空间(lua_checkstack)
** - 注意索引的正负和有效性
** - 理解何时会触发 GC
** - 使用 pcall 而不是直接 call 来处理错误
** - 合理使用 upvalue 传递状态
**
** ============================================================================
*/