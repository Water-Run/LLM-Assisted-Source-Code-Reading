/*
** $Id: ltable.h $
** Lua表（哈希）
** 请参阅 lua.h 中的版权声明
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"

/* 以下宏用于快速访问表的节点和值，是表内部操作的基础 */

/* 获取表的第i个节点的地址，用于哈希部分的操作 */
#define gnode(t, i) (&(t)->node[i])

/* 获取节点中的值地址，用于读写表项的值 */
#define gval(n) (&(n)->i_val)

/* 获取节点的下一个指针，用于解决哈希冲突的链表结构 */
#define gnext(n) ((n)->u.next)

/*
** 清除快速访问元方法的标志位，意味着表可能具有这些元方法。
** （第一次访问失败后会重新设置该标志位）
*/
#define invalidateTMcache(t) ((t)->flags &= cast_byte(~maskflags))

/*
** 标志位BITDUMMY设置在'flags'中，表示表正在使用虚拟节点（dummy node）作为哈希部分。
** 这是一种优化策略，用于处理空表或稀疏表。
*/
#define BITDUMMY (1 << 6)
#define NOTBITDUMMY cast_byte(~BITDUMMY)
#define isdummy(t) ((t)->flags & BITDUMMY)

#define setnodummy(t) ((t)->flags &= NOTBITDUMMY)
#define setdummy(t) ((t)->flags |= BITDUMMY)

/* 计算哈希节点的分配大小，如果表使用虚拟节点则返回0 */
#define allocsizenode(t) (isdummy(t) ? 0 : sizenode(t))

/* 根据表项的值返回对应的节点指针，用于反向查找 */
#define nodefromval(v) cast(Node *, (v))

/*
** 快速获取整数键的值的宏，优化了常见的整数索引操作。
** 先尝试在数组部分查找，如果失败则回退到完整查找。
** 参数：t-表指针，k-键，res-结果存储位置，tag-标签存储位置
*/
#define luaH_fastgeti(t, k, res, tag)   \
  {                                     \
    Table *h = t;                       \
    lua_Unsigned u = l_castS2U(k) - 1u; \
    if ((u < h->asize))                 \
    {                                   \
      tag = *getArrTag(h, u);           \
      if (!tagisempty(tag))             \
      {                                 \
        farr2val(h, u, tag, res);       \
      }                                 \
    }                                   \
    else                                \
    {                                   \
      tag = luaH_getint(h, (k), res);   \
    }                                   \
  }

/*
** 快速设置整数键的值的宏，处理元方法和边界情况。
** 如果满足条件（如无元方法或槽非空），则直接设置；否则返回错误编码。
*/
#define luaH_fastseti(t, k, val, hres)                               \
  {                                                                  \
    Table *h = t;                                                    \
    lua_Unsigned u = l_castS2U(k) - 1u;                              \
    if ((u < h->asize))                                              \
    {                                                                \
      lu_byte *tag = getArrTag(h, u);                                \
      if (checknoTM(h->metatable, TM_NEWINDEX) || !tagisempty(*tag)) \
      {                                                              \
        fval2arr(h, u, tag, val);                                    \
        hres = HOK;                                                  \
      }                                                              \
      else                                                           \
        hres = ~cast_int(u);                                         \
    }                                                                \
    else                                                             \
    {                                                                \
      hres = luaH_psetint(h, k, val);                                \
    }                                                                \
  }

/* pset操作的结果定义，用于表示不同的情况 */
#define HOK 0        /* 成功 */
#define HNOTFOUND 1  /* 键不存在 */
#define HNOTATABLE 2 /* 不是表（用于快速宏的错误处理） */
#define HFIRSTNODE 3 /* 哈希部分的起始索引 */

/*
** 'luaH_get*'操作设置'res'，除非值不存在，并返回结果的标签。
** 'luaH_pset*'（预设置）操作设置给定值并返回HOK，除非原值不存在。
** 如果键确实不存在，返回HNOTFOUND。
** 如果有该键的槽但无值，返回键位置的编码（通常称为'hres'）。
** （pset无法设置该值，因为可能涉及元方法。）
** 如果槽在哈希部分，编码为(HFIRSTNODE + 哈希索引)；如果槽在数组部分，编码为(~数组索引)，为负值。
** HNOTATABLE由快速宏用于表示被索引的值不是表。
** （数组部分的大小受限于能容纳在无符号整数中的最大2的幂，即INT_MAX+1。因此，C索引范围从0（编码为-1）到INT_MAX（编码为INT_MIN）。哈希部分的大小受限于能容纳在有符号整数中的最大2的幂，即(INT_MAX+1)/2。因此，在哈希部分安全地添加HFIRSTNODE到任何索引。）
*/

/*
** 表的数组部分表示为一个反转的值数组和一个标签数组，避免填充造成的空间浪费。
** 两者之间有一个无符号整数，稍后解释。
** 'array'指针指向两个数组之间，因此值用负索引，标签用非负索引。
**
**              值部分                              标签部分
**  --------------------------------------------------------
**  ...  |   值1     |   值0     |无符号整数|0|1|...
**  --------------------------------------------------------
**                                        ^ t->array
**
** 所有对't->array'的访问应通过宏'getArrTag'和'getArrVal'进行。
** 这种设计允许连续内存存储，提高缓存效率。
*/

/* 计算抽象C索引k对应的标签地址，用于访问数组部分的标签 */
#define getArrTag(t, k) (cast(lu_byte *, (t)->array) + sizeof(unsigned) + (k))

/* 计算抽象C索引k对应的值地址，用于访问数组部分的值 */
#define getArrVal(t, k) ((t)->array - 1 - (k))

/*
** 两个数组之间的无符号整数用作'#t'的提示；参见luaH_getn。
** 它被存储在那里以避免在没有数组部分的结构Table中浪费空间。
** 这是一种空间优化技巧。
*/
#define lenhint(t) cast(unsigned *, (t)->array)

/*
** 使用C索引在数组之间移动TValues
*/
#define arr2obj(h, k, val) \
  ((val)->tt_ = *getArrTag(h, (k)), (val)->value_ = *getArrVal(h, (k)))

#define obj2arr(h, k, val) \
  (*getArrTag(h, (k)) = (val)->tt_, *getArrVal(h, (k)) = (val)->value_)

/*
** 通常，在移动值之前需要检查标签。以下宏也在数组之间移动TValues，
** 但接收预计算的值或地址作为额外参数，优化了性能。
*/
#define farr2val(h, k, tag, res) \
  ((res)->tt_ = tag, (res)->value_ = *getArrVal(h, (k)))

#define fval2arr(h, k, tag, val) \
  (*tag = (val)->tt_, *getArrVal(h, (k)) = (val)->value_)

/* 以下函数声明提供了Lua表的核心操作接口 */

/* 通用表获取操作，根据键类型分派到特定实现 */
LUAI_FUNC lu_byte luaH_get(Table *t, const TValue *key, TValue *res);

/* 快速获取短字符串键的值，字符串在Lua中常用于表键 */
LUAI_FUNC lu_byte luaH_getshortstr(Table *t, TString *key, TValue *res);

/* 获取字符串键的值，支持任意字符串 */
LUAI_FUNC lu_byte luaH_getstr(Table *t, TString *key, TValue *res);

/* 获取整数键的值，整数索引是Lua表最常见的操作 */
LUAI_FUNC lu_byte luaH_getint(Table *t, lua_Integer key, TValue *res);

/* 用于元方法的特殊获取操作，避免重复查找 */
LUAI_FUNC const TValue *luaH_Hgetshortstr(Table *t, TString *key);

/* 预设置整数键的值，处理元方法和边界情况 */
LUAI_FUNC int luaH_psetint(Table *t, lua_Integer key, TValue *val);

/* 预设置短字符串键的值，优化字符串操作 */
LUAI_FUNC int luaH_psetshortstr(Table *t, TString *key, TValue *val);

/* 预设置字符串键的值 */
LUAI_FUNC int luaH_psetstr(Table *t, TString *key, TValue *val);

/* 通用预设置操作，根据键类型处理 */
LUAI_FUNC int luaH_pset(Table *t, const TValue *key, TValue *val);

/* 设置整数键的值，用于常见操作 */
LUAI_FUNC void luaH_setint(lua_State *L, Table *t, lua_Integer key,
                           TValue *value);

/* 通用设置操作，根据键类型分派 */
LUAI_FUNC void luaH_set(lua_State *L, Table *t, const TValue *key,
                        TValue *value);

/* 完成设置操作，处理元方法和错误情况 */
LUAI_FUNC void luaH_finishset(lua_State *L, Table *t, const TValue *key,
                              TValue *value, int hres);

/* 创建新表，分配初始内存 */
LUAI_FUNC Table *luaH_new(lua_State *L);

/* 调整表的大小，同时改变数组部分和哈希部分的大小 */
LUAI_FUNC void luaH_resize(lua_State *L, Table *t, unsigned nasize,
                           unsigned nhsize);

/* 仅调整数组部分的大小，哈希部分保持不变 */
LUAI_FUNC void luaH_resizearray(lua_State *L, Table *t, unsigned nasize);

/* 获取表的总大小（节点数），用于内存管理 */
LUAI_FUNC lu_mem luaH_size(Table *t);

/* 释放表的内存，防止内存泄漏 */
LUAI_FUNC void luaH_free(lua_State *L, Table *t);

/* 表的迭代操作，返回下一个键值对 */
LUAI_FUNC int luaH_next(lua_State *L, Table *t, StkId key);

/* 获取表的长度，用于'#'操作符的实现 */
LUAI_FUNC lua_Unsigned luaH_getn(lua_State *L, Table *t);

#if defined(LUA_DEBUG)
/* 调试函数，返回键的主位置，用于检测哈希冲突 */
LUAI_FUNC Node *luaH_mainposition(const Table *t, const TValue *key);
#endif

#endif

// 总结：
// 本文件（ltable.h）是Lua表实现的核心头文件，定义了表操作的宏和函数。Lua表是一个双部分结构，结合数组（用于连续整数索引）和哈希表（用于任意键），通过这些接口可以实现高效的数据访问。宏提供了底层操作，如节点访问和值移动，而函数则处理更复杂的逻辑，如调整大小和元方法处理。理解这些代码需要掌握Lua表的内部表示和优化策略。