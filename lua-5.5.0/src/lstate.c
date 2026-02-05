/*
** ========================================================================
** 文件概要
** ========================================================================
** 文件名: lstate.c
** 功能: Lua状态机(State)的管理
**
** 本文件是Lua虚拟机的核心组件之一，负责管理Lua状态（lua_State）的创建、
** 初始化、销毁以及线程管理。主要功能包括：
**
** 1. Lua状态(State)的创建与销毁
**    - lua_newstate: 创建新的Lua主状态
**    - lua_close: 关闭Lua状态
**
** 2. 线程(Thread)管理
**    - lua_newthread: 创建新的协程/线程
**    - luaE_freethread: 释放线程
**    - luaE_resetthread: 重置线程状态
**
** 3. 调用栈(CallInfo)管理
**    - luaE_extendCI: 扩展调用信息链表
**    - luaE_shrinkCI: 收缩调用信息链表
**
** 4. 内存管理相关
**    - 栈的初始化和释放
**    - GC债务(GCdebt)的设置
**
** 5. 注册表(Registry)的初始化
**    - 创建全局注册表
**    - 设置预定义值
** ========================================================================
*/

/*
** $Id: lstate.c $
** 全局状态
** 请查看lua.h中的版权声明
*/

#define lstate_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h> /* 提供 offsetof 宏和 NULL 定义 */
#include <string.h> /* 提供 memcpy 函数 */

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
 
/*
** 从lua_State指针获取对应的LX结构体指针
** LX结构体包含lua_State和额外空间(extra space)
** 通过指针运算，从lua_State的地址减去其在LX中的偏移量，得到LX的起始地址
*/
#define fromstate(L) (cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))

/*
** 这些宏允许用户在线程创建/删除时执行特定操作
** 如果用户没有定义这些宏，则提供默认的空实现
*/
#if !defined(luai_userstateopen)
#define luai_userstateopen(L) ((void)L) /* 状态打开时的用户钩子 */
#endif

#if !defined(luai_userstateclose)
#define luai_userstateclose(L) ((void)L) /* 状态关闭时的用户钩子 */
#endif

#if !defined(luai_userstatethread)
#define luai_userstatethread(L, L1) ((void)L) /* 线程创建时的用户钩子 */
#endif

#if !defined(luai_userstatefree)
#define luai_userstatefree(L, L1) ((void)L) /* 线程释放时的用户钩子 */
#endif

/*
** 设置GC债务为新值，保持已分配对象的真实数量（GCtotalobjs - GCdebt）不变
** 并避免'GCtotalbytes'溢出
**
** GC债务机制解释：
** - GCtotalbytes: 总字节数（包含债务）
** - GCdebt: 债务值，负值表示需要GC，正值表示有信用额度
** - 真实分配的内存 = GCtotalbytes - GCdebt
*/
void luaE_setdebt(global_State *g, l_mem debt)
{
  l_mem tb = gettotalbytes(g); /* 获取当前实际分配的字节数 */
  lua_assert(tb > 0);
  if (debt > MAX_LMEM - tb)
    debt = MAX_LMEM - tb; /* 防止溢出：将使GCtotalbytes == MAX_LMEM */
  g->GCtotalbytes = tb + debt;
  g->GCdebt = debt;
}

/*
** 扩展CallInfo链表
** CallInfo用于保存函数调用的上下文信息
** 当调用栈需要更多空间时，创建新的CallInfo节点
*/
CallInfo *luaE_extendCI(lua_State *L)
{
  CallInfo *ci;
  lua_assert(L->ci->next == NULL); /* 确保当前调用信息是链表末尾 */
  ci = luaM_new(L, CallInfo);      /* 分配新的CallInfo结构 */
  lua_assert(L->ci->next == NULL);
  /* 将新节点链接到链表 */
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  ci->u.l.trap = 0; /* 初始化Lua调用相关字段 */
  L->nci++;         /* 增加CallInfo计数 */
  return ci;
}

/*
** 释放线程未使用的所有CallInfo结构
** 保留当前正在使用的CallInfo及其之前的所有节点
*/
static void freeCI(lua_State *L)
{
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL; /* 断开当前节点与后续节点的连接 */
  /* 遍历并释放所有后续节点 */
  while ((ci = next) != NULL)
  {
    next = ci->next;
    luaM_free(L, ci);
    L->nci--;
  }
}

/*
** 释放线程未使用的一半CallInfo结构，保留第一个
** 这是一种内存优化策略，避免频繁分配/释放
** 通过保留一些空闲的CallInfo，减少将来的分配开销
*/
void luaE_shrinkCI(lua_State *L)
{
  CallInfo *ci = L->ci->next; /* 第一个空闲的CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return; /* 没有额外元素 */
  /* 每隔一个节点删除一个，实现"减半"效果 */
  while ((next = ci->next) != NULL)
  {                               /* 有两个额外元素吗？ */
    CallInfo *next2 = next->next; /* next的下一个 */
    ci->next = next2;             /* 从链表中移除next */
    L->nci--;
    luaM_free(L, next); /* 释放next */
    if (next2 == NULL)
      break; /* 没有更多元素 */
    else
    {
      next2->previous = ci;
      ci = next2; /* 继续 */
    }
  }
}

/*
** 当'getCcalls(L)'大于或等于LUAI_MAXCCALLS时调用
** 如果相等，抛出溢出错误。如果值大于LUAI_MAXCCALLS（表示正在处理溢出）
** 但不是太大，则不报告错误（允许溢出处理正常工作）
**
** C调用栈深度检查：防止C函数递归调用太深导致栈溢出
*/
void luaE_checkcstack(lua_State *L)
{
  if (getCcalls(L) == LUAI_MAXCCALLS)
    luaG_runerror(L, "C stack overflow"); /* C栈溢出错误 */
  else if (getCcalls(L) >= (LUAI_MAXCCALLS / 10 * 11))
    luaD_errerr(L); /* 处理栈错误时出错（错误的错误） */
}

/*
** 增加C调用栈计数并检查是否溢出
** LUAI_FUNC表示这是一个内部函数，可能被其他模块调用
*/
LUAI_FUNC void luaE_incCstack(lua_State *L)
{
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= LUAI_MAXCCALLS)) /* l_unlikely提示编译器这是小概率事件 */
    luaE_checkcstack(L);
}

/*
** 重置CallInfo到初始状态
** 用于线程初始化或错误恢复
*/
static void resetCI(lua_State *L)
{
  CallInfo *ci = L->ci = &L->base_ci;        /* 使用基础CallInfo */
  ci->func.p = L->stack.p;                   /* 函数位置指向栈底 */
  setnilvalue(s2v(ci->func.p));              /* 基础'ci'的'function'入口设为nil */
  ci->top.p = ci->func.p + 1 + LUA_MINSTACK; /* +1是为'function'入口预留 */
  ci->u.c.k = NULL;                          /* C调用相关：continuation函数为NULL */
  ci->callstatus = CIST_C;                   /* 标记为C调用 */
  L->status = LUA_OK;
  L->errfunc = 0; /* 栈展开可能会"丢弃"错误函数 */
}

/*
** 初始化Lua栈
** L1: 要初始化栈的线程
** L: 用于内存分配的线程（通常是主线程）
*/
static void stack_init(lua_State *L1, lua_State *L)
{
  int i;
  /* 初始化栈数组 */
  /* BASIC_STACK_SIZE: 基础栈大小
     EXTRA_STACK: 额外栈空间，用于错误处理等 */
  L1->stack.p = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  L1->tbclist.p = L1->stack.p; /* to-be-closed变量列表初始指向栈底 */
  /* 将新栈全部初始化为nil */
  for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack.p + i));               /* 清空新栈 */
  L1->stack_last.p = L1->stack.p + BASIC_STACK_SIZE; /* 栈的最后有效位置 */
  /* 初始化第一个ci */
  resetCI(L1);
  L1->top.p = L1->stack.p + 1; /* +1是为'function'入口预留 */
}

/*
** 释放栈内存
*/
static void freestack(lua_State *L)
{
  if (L->stack.p == NULL)
    return;            /* 栈还未完全构建 */
  L->ci = &L->base_ci; /* 释放整个'ci'链表 */
  freeCI(L);
  lua_assert(L->nci == 0);
  /* 释放栈 */
  luaM_freearray(L, L->stack.p, cast_sizet(stacksize(L) + EXTRA_STACK));
}

/*
** 创建注册表及其预定义值
** 注册表是一个全局可访问的表，用于存储重要的系统值
*/
static void init_registry(lua_State *L, global_State *g)
{
  /* 创建注册表 */
  TValue aux; /* 辅助变量，用于临时存储值 */
  Table *registry = luaH_new(L);
  sethvalue(L, &g->l_registry, registry);     /* 将注册表设置到全局状态 */
  luaH_resize(L, registry, LUA_RIDX_LAST, 0); /* 预分配空间 */

  /* registry[1] = false
     索引1通常用于存储一个哨兵值 */
  setbfvalue(&aux); /* 设置aux为false */
  luaH_setint(L, registry, 1, &aux);

  /* registry[LUA_RIDX_MAINTHREAD] = L
     存储主线程的引用 */
  setthvalue(L, &aux, L);
  luaH_setint(L, registry, LUA_RIDX_MAINTHREAD, &aux);

  /* registry[LUA_RIDX_GLOBALS] = new table (全局变量表)
     创建并存储全局环境表 */
  sethvalue(L, &aux, luaH_new(L));
  luaH_setint(L, registry, LUA_RIDX_GLOBALS, &aux);
}

/*
** 打开状态中可能导致内存分配错误的部分
** 这个函数在保护模式下运行，如果出错可以安全地清理
*/
static void f_luaopen(lua_State *L, void *ud)
{
  global_State *g = G(L);
  UNUSED(ud);                /* 未使用参数 */
  stack_init(L, L);          /* 初始化栈 */
  init_registry(L, g);       /* 初始化注册表 */
  luaS_init(L);              /* 初始化字符串子系统 */
  luaT_init(L);              /* 初始化标签方法(tag method)子系统 */
  luaX_init(L);              /* 初始化词法分析器 */
  g->gcstp = 0;              /* 允许gc（之前被设置为GCSTPGC阻止GC） */
  setnilvalue(&g->nilvalue); /* 现在状态已完整（将nilvalue从整数0改为真正的nil） */
  luai_userstateopen(L);     /* 调用用户定义的钩子 */
}

/*
** 预初始化线程，设置一致的值而不分配任何内存（避免错误）
** 这个函数不会失败，因为它不分配内存
*/
static void preinit_thread(lua_State *L, global_State *g)
{
  G(L) = g;                                     /* 设置全局状态指针 */
  L->stack.p = NULL;                            /* 栈尚未分配 */
  L->ci = NULL;                                 /* 调用信息链表尚未初始化 */
  L->nci = 0;                                   /* CallInfo计数 */
  L->twups = L;                                 /* 线程没有upvalue时指向自己 */
  L->nCcalls = 0;                               /* C调用深度 */
  L->errorJmp = NULL;                           /* 错误跳转点 */
  L->hook = NULL;                               /* 调试钩子 */
  L->hookmask = 0;                              /* 钩子掩码 */
  L->basehookcount = 0;                         /* 基础钩子计数 */
  L->allowhook = 1;                             /* 允许钩子 */
  resethookcount(L);                            /* 重置钩子计数 */
  L->openupval = NULL;                          /* 开放的upvalue链表 */
  L->status = LUA_OK;                           /* 状态码 */
  L->errfunc = 0;                               /* 错误处理函数 */
  L->oldpc = 0;                                 /* 旧的程序计数器（用于调试） */
  L->base_ci.previous = L->base_ci.next = NULL; /* 基础CallInfo的链接 */
}

/*
** 计算线程占用的内存大小
** 包括LX结构、CallInfo链表和栈
*/
lu_mem luaE_threadsize(lua_State *L)
{
  lu_mem sz = cast(lu_mem, sizeof(LX))                /* LX结构本身 */
              + cast_uint(L->nci) * sizeof(CallInfo); /* CallInfo链表 */
  if (L->stack.p != NULL)
    sz += cast_uint(stacksize(L) + EXTRA_STACK) * sizeof(StackValue); /* 栈空间 */
  return sz;
}

/*
** 关闭Lua状态
** 处理两种情况：
** 1. 部分构建的状态（构建过程中出错）
** 2. 完全构建的状态（正常关闭）
*/
static void close_state(lua_State *L)
{
  global_State *g = G(L);
  if (!completestate(g))    /* 关闭部分构建的状态？ */
    luaC_freeallobjects(L); /* 只收集其对象 */
  else
  {                                    /* 关闭完全构建的状态 */
    resetCI(L);                        /* 重置调用信息 */
    luaD_closeprotected(L, 1, LUA_OK); /* 关闭所有upvalue */
    L->top.p = L->stack.p + 1;         /* 清空栈以运行终结器 */
    luaC_freeallobjects(L);            /* 收集所有对象 */
    luai_userstateclose(L);            /* 用户关闭钩子 */
  }
  luaM_freearray(L, G(L)->strt.hash, cast_sizet(G(L)->strt.size)); /* 释放字符串表 */
  freestack(L);                                                    /* 释放栈 */
  lua_assert(gettotalbytes(g) == sizeof(global_State));            /* 确保只剩全局状态本身 */
  (*g->frealloc)(g->ud, g, sizeof(global_State), 0);               /* 释放主块 */
}

/*
** 创建新线程（协程）
** 新线程共享主线程的全局状态，但有独立的栈和调用信息
*/
LUA_API lua_State *lua_newthread(lua_State *L)
{
  global_State *g = G(L);
  GCObject *o;
  lua_State *L1;
  lua_lock(L);     /* 加锁（多线程保护） */
  luaC_checkGC(L); /* 检查是否需要GC */
  /* 创建新线程 */
  /* LUA_TTHREAD: 线程类型
     sizeof(LX): 分配LX结构的大小
     offsetof(LX, l): lua_State在LX中的偏移 */
  o = luaC_newobjdt(L, LUA_TTHREAD, sizeof(LX), offsetof(LX, l));
  L1 = gco2th(o); /* 将GC对象转换为线程 */
  /* 将新线程锚定在L的栈上（防止被GC） */
  setthvalue2s(L, L->top.p, L1);
  api_incr_top(L);
  preinit_thread(L1, g); /* 预初始化线程 */
  /* 继承钩子设置 */
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* 初始化L1的额外空间 */
  /* LUA_EXTRASPACE: 用户可以在lua_State前面添加的额外空间 */
  memcpy(lua_getextraspace(L1), lua_getextraspace(mainthread(g)),
         LUA_EXTRASPACE);
  luai_userstatethread(L, L1); /* 用户线程创建钩子 */
  stack_init(L1, L);           /* 初始化栈 */
  lua_unlock(L);
  return L1;
}

/*
** 释放线程
** 关闭所有upvalue并释放相关资源
*/
void luaE_freethread(lua_State *L, lua_State *L1)
{
  LX *l = fromstate(L1);             /* 获取LX结构 */
  luaF_closeupval(L1, L1->stack.p);  /* 关闭所有upvalue */
  lua_assert(L1->openupval == NULL); /* 确保所有upvalue已关闭 */
  luai_userstatefree(L, L1);         /* 用户释放钩子 */
  freestack(L1);                     /* 释放栈 */
  luaM_free(L, l);                   /* 释放LX结构 */
}

/*
** 重置线程状态
** 用于错误恢复或yield后的重置
** 返回最终状态
*/
TStatus luaE_resetthread(lua_State *L, TStatus status)
{
  resetCI(L); /* 重置调用信息 */
  if (status == LUA_YIELD)
    status = LUA_OK;                             /* yield状态转为OK */
  status = luaD_closeprotected(L, 1, status);    /* 关闭upvalue */
  if (status != LUA_OK)                          /* 有错误？ */
    luaD_seterrorobj(L, status, L->stack.p + 1); /* 设置错误对象 */
  else
    L->top.p = L->stack.p + 1;                                  /* 清空栈 */
  luaD_reallocstack(L, cast_int(L->ci->top.p - L->stack.p), 0); /* 调整栈大小 */
  return status;
}

/*
** 关闭线程（从另一个线程）
** 用于协程结束或错误处理
*/
LUA_API int lua_closethread(lua_State *L, lua_State *from)
{
  TStatus status;
  lua_lock(L);
  L->nCcalls = (from) ? getCcalls(from) : 0; /* 继承C调用深度 */
  status = luaE_resetthread(L, L->status);   /* 重置线程 */
  if (L == from)                             /* 关闭自己？ */
    luaD_throwbaselevel(L, status);          /* 跳转到基础层级 */
  lua_unlock(L);
  return APIstatus(status); /* 转换为API状态码 */
}

/*
** 创建新的Lua状态（主函数）
** f: 内存分配函数
** ud: 用户数据（传给分配函数）
** seed: 随机数种子
*/
LUA_API lua_State *lua_newstate(lua_Alloc f, void *ud, unsigned seed)
{
  int i;
  lua_State *L;
  /* 分配全局状态结构 */
  global_State *g = cast(global_State *,
                         (*f)(ud, NULL, LUA_TTHREAD, sizeof(global_State)));
  if (g == NULL)
    return NULL;
  L = &g->mainth.l;    /* 主线程是全局状态的一部分 */
  L->tt = LUA_VTHREAD; /* 设置类型标签 */

  /* GC相关初始化 */
  g->currentwhite = bitmask(WHITE0BIT); /* 当前白色（GC颜色） */
  L->marked = luaC_white(g);            /* 标记为白色 */

  preinit_thread(L, g);  /* 预初始化主线程 */
  g->allgc = obj2gco(L); /* 现在，唯一的对象是主线程 */
  L->next = NULL;
  incnny(L); /* 主线程始终不可yield */

  /* 设置内存分配器和用户数据 */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;   /* 警告函数 */
  g->ud_warn = NULL; /* 警告函数的用户数据 */
  g->seed = seed;    /* 随机数种子 */

  /* GC初始化 */
  g->gcstp = GCSTPGC; /* 构建状态时禁止GC */

  /* 字符串表初始化 */
  g->strt.size = g->strt.nuse = 0;
  g->strt.hash = NULL;

  /* 注册表初始化 */
  setnilvalue(&g->l_registry);

  /* panic函数（未处理错误的最后手段） */
  g->panic = NULL;

  /* GC状态初始化 */
  g->gcstate = GCSpause; /* GC暂停状态 */
  g->gckind = KGC_INC;   /* 增量式GC */
  g->gcstopem = 0;       /* 紧急GC停止标记 */
  g->gcemergency = 0;    /* 紧急GC标记 */

  /* GC链表初始化 - 各种对象链表 */
  g->finobj = g->tobefnz = g->fixedgc = NULL;                 /* 终结器相关 */
  g->firstold1 = g->survival = g->old1 = g->reallyold = NULL; /* 分代GC链表 */
  g->finobjsur = g->finobjold1 = g->finobjrold = NULL;        /* 带终结器的对象链表 */
  g->sweepgc = NULL;                                          /* 清扫链表 */
  g->gray = g->grayagain = NULL;                              /* 灰色对象链表 */
  g->weak = g->ephemeron = g->allweak = NULL;                 /* 弱引用相关 */
  g->twups = NULL;                                            /* 带upvalue的线程链表 */

  /* 内存统计 */
  g->GCtotalbytes = sizeof(global_State);
  g->GCmarked = 0; /* 标记的对象数 */
  g->GCdebt = 0;   /* GC债务 */

  /* 设置nilvalue为整数0，表示状态尚未完全构建 */
  setivalue(&g->nilvalue, 0); /* 标记状态尚未构建完成 */

  /* 设置GC参数（使用默认值） */
  setgcparam(g, PAUSE, LUAI_GCPAUSE);         /* GC暂停阈值 */
  setgcparam(g, STEPMUL, LUAI_GCMUL);         /* GC步进倍数 */
  setgcparam(g, STEPSIZE, LUAI_GCSTEPSIZE);   /* GC步进大小 */
  setgcparam(g, MINORMUL, LUAI_GENMINORMUL);  /* 分代GC minor倍数 */
  setgcparam(g, MINORMAJOR, LUAI_MINORMAJOR); /* minor到major的阈值 */
  setgcparam(g, MAJORMINOR, LUAI_MAJORMINOR); /* major到minor的比率 */

  /* 初始化元表数组 */
  for (i = 0; i < LUA_NUMTYPES; i++)
    g->mt[i] = NULL;

  /* 在保护模式下运行f_luaopen，进行可能失败的初始化 */
  if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK)
  {
    /* 内存分配错误：释放部分状态 */
    close_state(L);
    L = NULL;
  }
  return L;
}

/*
** 关闭Lua状态
** 只能关闭主线程
*/
LUA_API void lua_close(lua_State *L)
{
  lua_lock(L);
  L = mainthread(G(L)); /* 只有主线程可以被关闭 */
  close_state(L);
}

/*
** 发出警告
** msg: 警告消息
** tocont: 是否为续行（1表示消息未结束，0表示结束）
*/
void luaE_warning(lua_State *L, const char *msg, int tocont)
{
  lua_WarnFunction wf = G(L)->warnf; /* 获取警告函数 */
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont); /* 调用警告函数 */
}

/*
** 从错误消息生成警告
** where: 错误发生的位置描述
**
** 生成格式: "error in <where> (<error message>)"
*/
void luaE_warnerror(lua_State *L, const char *where)
{
  TValue *errobj = s2v(L->top.p - 1); /* 栈顶的错误对象 */
  const char *msg = (ttisstring(errobj))
                        ? getstr(tsvalue(errobj))         /* 如果是字符串，获取内容 */
                        : "error object is not a string"; /* 否则使用默认消息 */
  /* 生成警告 "error in %s (%s)" (where, msg) */
  luaE_warning(L, "error in ", 1); /* tocont=1，消息未结束 */
  luaE_warning(L, where, 1);
  luaE_warning(L, " (", 1);
  luaE_warning(L, msg, 1);
  luaE_warning(L, ")", 0); /* tocont=0，消息结束 */
}

/*
** ========================================================================
** 详细总结
** ========================================================================
**
** 本文件实现了Lua虚拟机的核心状态管理功能，主要包括：
**
** 1. 状态生命周期管理
**    - lua_newstate创建完整的Lua环境，包括全局状态、主线程、注册表等
**    - close_state负责清理所有资源，处理终结器
**    - 使用保护模式(protected mode)确保初始化失败时能安全清理
**
** 2. 线程（协程）管理
**    - 每个线程有独立的执行栈和调用信息链
**    - 线程共享全局状态（字符串池、注册表、GC等）
**    - 支持线程间的切换和状态重置
**
** 3. 调用栈管理
**    - CallInfo链表管理函数调用上下文
**    - 动态扩展和收缩策略优化内存使用
**    - C调用深度检查防止栈溢出
**
** 4. 内存管理集成
**    - 与GC系统紧密集成（GCdebt机制）
**    - 所有内存分配通过用户提供的分配器
**    - 精确的内存使用统计
**
** 5. 错误处理机制
**    - 多层错误处理（panic、错误函数、保护调用）
**    - 线程状态重置支持错误恢复
**    - 警告系统用于非致命问题
**
** 关键数据结构：
** - lua_State: 表示一个执行线程
** - global_State: 全局共享状态
** - CallInfo: 函数调用信息
** - LX: 包含lua_State和额外空间的完整结构
**
** 设计特点：
** - 模块化：各子系统独立初始化
** - 可扩展：用户钩子支持自定义行为
** - 健壮性：完善的错误处理和资源管理
** - 性能：CallInfo缓存、GC债务机制等优化
**
** 与其他模块的关系：
** - ldo.c: 函数调用和执行
** - lgc.c: 垃圾回收
** - lmem.c: 内存分配
** - ltable.c: 表操作（注册表）
** - lstring.c: 字符串管理
** ========================================================================
*/