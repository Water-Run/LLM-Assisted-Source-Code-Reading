/*
** $Id: lauxlib.h $
** Auxiliary functions for building Lua libraries
** 用于构建Lua库的辅助函数
** See Copyright Notice in lua.h
** 参见lua.h中的版权声明
*/

/*
** =======================================================================c=
** 文件概要说明
** ========================================================================
**
** 文件名: lauxlib.h
** 功能: Lua辅助库头文件
**
** 这个头文件定义了Lua辅助库(auxiliary library)的API接口。辅助库是对Lua C API
** 的高层封装,提供了更便捷的函数来简化C语言扩展Lua的开发工作。
**
** 主要功能模块:
**
** 1. 版本检查与兼容性
**    - 提供版本检查机制,确保C库与Lua核心版本兼容
**
** 2. 参数检查与类型验证
**    - 提供一系列函数检查Lua函数参数的类型和有效性
**    - 包括字符串、数字、整数等类型的检查和获取
**
** 3. 元表操作
**    - 简化元表的创建、设置和查询
**    - 提供userdata类型检查功能
**
** 4. 错误处理
**    - 提供格式化错误信息和错误报告功能
**    - 包含堆栈跟踪功能
**
** 5. 引用系统
**    - 提供在注册表中创建和管理对象引用的机制
**
** 6. 文件加载
**    - 提供加载Lua文件和字符串的函数
**
** 7. 缓冲区管理(luaL_Buffer)
**    - 提供高效的字符串构建机制
**    - 避免频繁的内存分配和字符串拼接
**
** 8. 文件句柄(luaL_Stream)
**    - 为IO库提供文件操作的标准结构
**
** 9. 库注册
**    - 提供便捷的方式将C函数注册为Lua库
**
** 设计理念:
** - 这个辅助库不是Lua核心的一部分,但几乎所有Lua C扩展都会使用它
** - 它封装了常见的模式和操作,减少样板代码
** - 提供了更好的错误处理和类型安全
**
** ========================================================================
*/

#ifndef lauxlib_h
#define lauxlib_h

/*
** ========================================================================
** 标准库包含
** ========================================================================
*/

#include <stddef.h> /* 包含stddef.h: 提供size_t类型定义和NULL宏 */
#include <stdio.h>  /* 包含stdio.h: 提供FILE类型,用于文件操作 */

#include "luaconf.h" /* 包含luaconf.h: Lua配置文件,定义了各种配置选项和平台相关设置 */
#include "lua.h"     /* 包含lua.h: Lua核心API头文件,定义了lua_State等基本类型 */

/*
** ========================================================================
** 全局表名称
** ========================================================================
*/

/* global table */
/* 全局表 */
#define LUA_GNAME "_G"
/*
** 说明: LUA_GNAME定义了Lua全局环境表的名称"_G"
** 在Lua中,_G是一个特殊的表,它包含了所有全局变量
** 实际上_G._G == _G,即全局表中有一个名为_G的字段指向自己
*/

/*
** ========================================================================
** 前置类型声明
** ========================================================================
*/

typedef struct luaL_Buffer luaL_Buffer;
/*
** 说明: 这是一个前置声明(forward declaration)
** luaL_Buffer是一个用于高效构建字符串的缓冲区结构
** 这里只是声明类型名称,完整定义在文件后面
** 使用前置声明可以在结构体完整定义之前就使用该类型名
*/

/*
** ========================================================================
** 错误代码定义
** ========================================================================
*/

/* extra error code for 'luaL_loadfilex' */
/* 'luaL_loadfilex'的额外错误代码 */
#define LUA_ERRFILE (LUA_ERRERR + 1)
/*
** 说明: 定义文件操作错误代码
** LUA_ERRERR是lua.h中定义的错误处理函数执行出错的代码
** LUA_ERRFILE用于表示文件相关的错误(如文件不存在、无法打开等)
** 通过在LUA_ERRERR基础上+1来避免与现有错误代码冲突
*/

/*
** ========================================================================
** 注册表中的特殊表键名
** ========================================================================
*/

/* key, in the registry, for table of loaded modules */
/* 注册表中,已加载模块表的键 */
#define LUA_LOADED_TABLE "_LOADED"
/*
** 说明: "_LOADED"表存储在Lua注册表中
** 该表记录了所有已经通过require加载的模块
** 键为模块名,值为模块返回的结果
** 这样可以避免重复加载同一个模块(require的缓存机制)
*/

/* key, in the registry, for table of preloaded loaders */
/* 注册表中,预加载加载器表的键 */
#define LUA_PRELOAD_TABLE "_PRELOAD"
/*
** 说明: "_PRELOAD"表存储在Lua注册表中
** 该表包含预加载的模块加载器函数
** 当require一个模块时,会首先在这个表中查找对应的加载器
** 这允许在Lua启动前就注册某些模块的加载方式
*/

/*
** ========================================================================
** 库注册结构体
** ========================================================================
*/

typedef struct luaL_Reg
{
  const char *name;   /* 函数名称 */
  lua_CFunction func; /* C函数指针 */
} luaL_Reg;
/*
** 说明: luaL_Reg用于注册C函数到Lua
**
** 结构体成员:
** - name: 函数在Lua中的名称(字符串),const表示这个字符串不可修改
** - func: 指向C函数的指针,类型为lua_CFunction(在lua.h中定义)
**
** 使用方式:
** 通常创建一个luaL_Reg数组,最后一个元素的name为NULL表示结束
** 例如:
** static const luaL_Reg mylib[] = {
**   {"add", l_add},
**   {"sub", l_sub},
**   {NULL, NULL}  // 哨兵元素,标记数组结束
** };
**
** 然后使用luaL_setfuncs()函数将这些函数注册到Lua表中
*/

/*
** ========================================================================
** 版本检查
** ========================================================================
*/

#define LUAL_NUMSIZES (sizeof(lua_Integer) * 16 + sizeof(lua_Number))
/*
** 说明: 计算数值类型大小的标识
**
** 这个宏用于版本兼容性检查,确保C库编译时使用的lua_Integer和lua_Number
** 类型大小与运行时Lua核心使用的大小一致
**
** 计算方法:
** - sizeof(lua_Integer)*16: lua_Integer的大小乘以16
** - sizeof(lua_Number): lua_Number的大小
** - 两者相加得到一个唯一标识
**
** 为什么乘以16?
** 因为sizeof通常返回较小的值(如4或8),乘以16后可以将两个大小
** 编码到不同的数值范围,避免碰撞。例如:
** - 如果Integer是8字节,Number是8字节: 8*16+8=136
** - 如果Integer是4字节,Number是8字节: 4*16+8=72
** 这样不同配置会产生不同的值
*/

LUALIB_API void(luaL_checkversion_)(lua_State *L, lua_Number ver, size_t sz);
/*
** 函数: luaL_checkversion_
** 功能: 检查Lua版本和配置兼容性(内部函数)
**
** 参数:
** - L: Lua状态机指针
** - ver: Lua版本号(来自LUA_VERSION_NUM)
** - sz: 数值类型大小(来自LUAL_NUMSIZES)
**
** 说明:
** - LUALIB_API是一个宏,定义函数的导出属性(在不同平台上可能是__declspec(dllexport)等)
** - 函数名后面的下划线表示这是一个内部实现函数,不应直接调用
** - 这个函数会比较传入的版本号和大小与当前Lua核心的值
** - 如果不匹配,会抛出错误,防止因版本不兼容导致的问题
*/

#define luaL_checkversion(L) \
  luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES)
/*
** 宏: luaL_checkversion
** 功能: 版本检查的便捷宏
**
** 使用方式:
** 在C库的初始化函数中调用 luaL_checkversion(L);
**
** 工作原理:
** - 自动传入当前编译时的LUA_VERSION_NUM和LUAL_NUMSIZES
** - 调用luaL_checkversion_进行实际检查
** - 这确保了C库与Lua核心的二进制兼容性
**
** 为什么需要版本检查?
** - 防止使用旧版本Lua编译的库在新版本上运行
** - 防止32位/64位混用
** - 防止整数/浮点数类型大小不匹配
*/

/*
** ========================================================================
** 元表操作函数
** ========================================================================
*/

LUALIB_API int(luaL_getmetafield)(lua_State *L, int obj, const char *e);
/*
** 函数: luaL_getmetafield
** 功能: 获取对象元表中的指定字段
**
** 参数:
** - L: Lua状态机
** - obj: 栈索引,指向要获取元表的对象
** - e: 字段名称
**
** 返回值:
** - 如果找到字段,返回字段的类型(LUA_TNIL, LUA_TNUMBER等),并将字段值压入栈
** - 如果对象没有元表或元表中没有该字段,返回LUA_TNIL,不压入任何值
**
** 使用场景:
** 检查对象是否定义了某个元方法,如__index、__add等
*/

LUALIB_API int(luaL_callmeta)(lua_State *L, int obj, const char *e);
/*
** 函数: luaL_callmeta
** 功能: 调用对象元表中的元方法
**
** 参数:
** - L: Lua状态机
** - obj: 栈索引,指向调用元方法的对象
** - e: 元方法名称(如"__tostring")
**
** 返回值:
** - 如果对象有该元方法,调用它并返回1,元方法的返回值留在栈上
** - 如果没有该元方法,返回0,不改变栈
**
** 工作流程:
** 1. 获取对象的元表
** 2. 在元表中查找字段e
** 3. 如果找到且是函数,将对象作为参数调用该函数
** 4. 否则返回0
**
** 典型用法:
** if (luaL_callmeta(L, 1, "__tostring")) {
**   // 对象有自定义的__tostring方法,结果已在栈上
** }
*/

/*
** ========================================================================
** 类型转换函数
** ========================================================================
*/

LUALIB_API const char *(luaL_tolstring)(lua_State * L, int idx, size_t *len);
/*
** 函数: luaL_tolstring
** 功能: 将栈上任意位置的值转换为字符串
**
** 参数:
** - L: Lua状态机
** - idx: 栈索引
** - len: 输出参数,如果非NULL,会存储字符串长度
**
** 返回值:
** - 返回字符串指针,同时将字符串压入栈顶
**
** 转换规则:
** - 如果值已经是字符串,直接返回
** - 如果值有__tostring元方法,调用该元方法
** - 否则使用默认格式(如数字转字符串,"table: 0x..."等)
**
** 注意:
** - 返回的字符串指针指向Lua内部管理的内存,不要修改或free
** - 字符串会被压入栈,所以在后续操作中要注意栈管理
*/

/*
** ========================================================================
** 参数错误处理函数
** ========================================================================
*/

LUALIB_API int(luaL_argerror)(lua_State *L, int arg, const char *extramsg);
/*
** 函数: luaL_argerror
** 功能: 抛出参数错误
**
** 参数:
** - L: Lua状态机
** - arg: 错误参数的位置(从1开始)
** - extramsg: 额外的错误信息
**
** 返回值:
** - 永不返回(通过longjmp跳转),但声明为int是为了可以写成 return luaL_argerror(...)
**
** 错误信息格式:
** "bad argument #arg to 'funcname' (extramsg)"
**
** 使用示例:
** if (lua_type(L, 1) != LUA_TNUMBER)
**   return luaL_argerror(L, 1, "number expected");
*/

LUALIB_API int(luaL_typeerror)(lua_State *L, int arg, const char *tname);
/*
** 函数: luaL_typeerror
** 功能: 抛出类型错误
**
** 参数:
** - L: Lua状态机
** - arg: 错误参数的位置
** - tname: 期望的类型名称
**
** 返回值:
** - 永不返回
**
** 错误信息格式:
** "bad argument #arg to 'funcname' (tname expected, got typename)"
**
** 使用示例:
** if (!lua_isnumber(L, 1))
**   return luaL_typeerror(L, 1, "number");
*/

/*
** ========================================================================
** 字符串参数检查函数
** ========================================================================
*/

LUALIB_API const char *(luaL_checklstring)(lua_State * L, int arg,
                                           size_t *l);
/*
** 函数: luaL_checklstring
** 功能: 检查并获取字符串参数
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
** - l: 输出参数,如果非NULL,存储字符串长度
**
** 返回值:
** - 如果参数是字符串或可转换为字符串,返回字符串指针
** - 否则抛出错误
**
** 说明:
** - 会尝试将数字转换为字符串
** - 返回的指针指向Lua管理的内存
** - "lstring"中的"l"表示length,即带长度的字符串
*/

LUALIB_API const char *(luaL_optlstring)(lua_State * L, int arg,
                                         const char *def, size_t *l);
/*
** 函数: luaL_optlstring
** 功能: 获取可选的字符串参数
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
** - def: 默认值(当参数不存在时)
** - l: 输出参数,存储字符串长度
**
** 返回值:
** - 如果参数存在且是字符串,返回该字符串
** - 如果参数不存在(nil或不存在),返回默认值def
** - 如果参数存在但不是字符串,抛出错误
**
** 使用场景:
** 处理可选参数,如 function foo(name, title)
** 其中title是可选的,可以有默认值
*/

/*
** ========================================================================
** 数值参数检查函数
** ========================================================================
*/

LUALIB_API lua_Number(luaL_checknumber)(lua_State *L, int arg);
/*
** 函数: luaL_checknumber
** 功能: 检查并获取浮点数参数
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
**
** 返回值:
** - 如果参数是数字,返回该数字(lua_Number类型,通常是double)
** - 如果参数可以转换为数字,返回转换后的值
** - 否则抛出错误
**
** 说明:
** - lua_Number在luaconf.h中定义,通常是double类型
** - 会尝试将字符串转换为数字
*/

LUALIB_API lua_Number(luaL_optnumber)(lua_State *L, int arg, lua_Number def);
/*
** 函数: luaL_optnumber
** 功能: 获取可选的浮点数参数
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
** - def: 默认值
**
** 返回值:
** - 参数存在且是数字时返回该数字
** - 参数不存在时返回默认值def
** - 参数存在但不是数字时抛出错误
*/

LUALIB_API lua_Integer(luaL_checkinteger)(lua_State *L, int arg);
/*
** 函数: luaL_checkinteger
** 功能: 检查并获取整数参数
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
**
** 返回值:
** - 返回整数值(lua_Integer类型,通常是long long)
** - 如果不是整数,抛出错误
**
** 说明:
** - lua_Integer在luaconf.h中定义,通常是long long(64位整数)
** - 浮点数会被转换为整数(截断小数部分)
*/

LUALIB_API lua_Integer(luaL_optinteger)(lua_State *L, int arg,
                                        lua_Integer def);
/*
** 函数: luaL_optinteger
** 功能: 获取可选的整数参数
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
** - def: 默认值
**
** 返回值:
** - 参数存在且是整数时返回该整数
** - 参数不存在时返回默认值def
*/

/*
** ========================================================================
** 栈和类型检查函数
** ========================================================================
*/

LUALIB_API void(luaL_checkstack)(lua_State *L, int sz, const char *msg);
/*
** 函数: luaL_checkstack
** 功能: 确保栈有足够空间
**
** 参数:
** - L: Lua状态机
** - sz: 需要的额外栈空间大小
** - msg: 错误信息(空间不足时使用)
**
** 说明:
** - Lua栈有大小限制,在压入大量值前应检查空间
** - 如果空间不足,这个函数会抛出错误
** - lua.h中有lua_checkstack(),但它只返回成功/失败,不抛出错误
*/

LUALIB_API void(luaL_checktype)(lua_State *L, int arg, int t);
/*
** 函数: luaL_checktype
** 功能: 检查参数类型
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
** - t: 期望的类型(LUA_TNUMBER, LUA_TSTRING等)
**
** 说明:
** - 如果类型不匹配,抛出错误
** - 不返回值,因为类型正确时不需要做什么,错误时会跳转
**
** 使用示例:
** luaL_checktype(L, 1, LUA_TTABLE);  // 确保第一个参数是表
*/

LUALIB_API void(luaL_checkany)(lua_State *L, int arg);
/*
** 函数: luaL_checkany
** 功能: 检查参数是否存在(不为nil)
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
**
** 说明:
** - 只检查参数存在,不关心类型
** - 主要用于确保必需参数已提供
** - 如果参数不存在或为nil,抛出错误
*/

/*
** ========================================================================
** 元表相关函数
** ========================================================================
*/

LUALIB_API int(luaL_newmetatable)(lua_State *L, const char *tname);
/*
** 函数: luaL_newmetatable
** 功能: 创建新的元表并注册到注册表
**
** 参数:
** - L: Lua状态机
** - tname: 元表的类型名称
**
** 返回值:
** - 如果是新创建的元表,返回1,元表被压入栈
** - 如果该名称的元表已存在,返回0,已存在的元表被压入栈
**
** 工作流程:
** 1. 在注册表中查找键为tname的元表
** 2. 如果不存在,创建新表,设置到注册表,返回1
** 3. 如果已存在,获取它,返回0
**
** 使用场景:
** 为userdata类型创建唯一的元表
**
** 示例:
** if (luaL_newmetatable(L, "MyType")) {
**   // 这是新创建的元表,设置元方法
**   lua_pushcfunction(L, my_gc);
**   lua_setfield(L, -2, "__gc");
** }
*/

LUALIB_API void(luaL_setmetatable)(lua_State *L, const char *tname);
/*
** 函数: luaL_setmetatable
** 功能: 设置栈顶对象的元表
**
** 参数:
** - L: Lua状态机
** - tname: 元表名称(注册表中的键)
**
** 说明:
** - 从注册表获取名为tname的元表
** - 将该元表设置为栈顶对象(通常是userdata)的元表
** - 栈顶对象保持在栈上
**
** 使用场景:
** 创建userdata后,为其设置类型对应的元表
*/

LUALIB_API void *(luaL_testudata)(lua_State * L, int ud, const char *tname);
/*
** 函数: luaL_testudata
** 功能: 测试值是否为指定类型的userdata
**
** 参数:
** - L: Lua状态机
** - ud: 栈索引
** - tname: 类型名称
**
** 返回值:
** - 如果是指定类型的userdata,返回userdata的数据指针
** - 否则返回NULL
**
** 检查方法:
** 1. 检查是否为userdata
** 2. 获取其元表
** 3. 与注册表中tname对应的元表比较
** 4. 相同则返回数据指针,不同则返回NULL
*/

LUALIB_API void *(luaL_checkudata)(lua_State * L, int ud, const char *tname);
/*
** 函数: luaL_checkudata
** 功能: 检查并获取指定类型的userdata
**
** 参数:
** - L: Lua状态机
** - ud: 栈索引
** - tname: 期望的类型名称
**
** 返回值:
** - 如果是指定类型的userdata,返回数据指针
** - 否则抛出类型错误
**
** 说明:
** - 这是luaL_testudata的检查版本
** - 类型不匹配时会抛出错误而不是返回NULL
*/

/*
** ========================================================================
** 错误信息和调试函数
** ========================================================================
*/

LUALIB_API void(luaL_where)(lua_State *L, int lvl);
/*
** 函数: luaL_where
** 功能: 获取调用位置信息
**
** 参数:
** - L: Lua状态机
** - lvl: 调用栈层级(1表示调用luaL_where的函数,2表示再上一层,等等)
**
** 说明:
** - 将位置信息字符串压入栈,格式如"file.lua:123: "
** - 如果无法获取位置信息,压入空字符串
** - 通常与luaL_error配合使用,在错误信息前添加位置
**
** 使用示例:
** luaL_where(L, 1);
** lua_pushfstring(L, "invalid value: %d", value);
** lua_concat(L, 2);  // 连接位置和错误信息
** lua_error(L);
*/

LUALIB_API int(luaL_error)(lua_State *L, const char *fmt, ...);
/*
** 函数: luaL_error
** 功能: 抛出格式化的错误
**
** 参数:
** - L: Lua状态机
** - fmt: 格式字符串(类似printf)
** - ...: 可变参数
**
** 返回值:
** - 永不返回,但声明为int便于写 return luaL_error(...)
**
** 说明:
** - 格式字符串支持printf风格的格式化,如%d, %s等
** - 自动添加调用位置信息
** - 通过longjmp跳转到错误处理代码
**
** 使用示例:
** if (x < 0)
**   return luaL_error(L, "invalid value: %d", x);
*/

/*
** ========================================================================
** 选项检查函数
** ========================================================================
*/

LUALIB_API int(luaL_checkoption)(lua_State *L, int arg, const char *def,
                                 const char *const lst[]);
/*
** 函数: luaL_checkoption
** 功能: 检查参数是否为预定义选项之一
**
** 参数:
** - L: Lua状态机
** - arg: 参数位置
** - def: 默认选项(当参数为nil时),可以为NULL
** - lst: 选项列表(字符串数组,以NULL结尾)
**
** 返回值:
** - 返回匹配选项在lst中的索引(从0开始)
** - 如果参数不匹配任何选项,抛出错误
**
** 使用示例:
** const char *modes[] = {"read", "write", "append", NULL};
** int mode = luaL_checkoption(L, 2, "read", modes);
** // mode为0,1,或2
*/

/*
** ========================================================================
** 文件和进程结果处理函数
** ========================================================================
*/

LUALIB_API int(luaL_fileresult)(lua_State *L, int stat, const char *fname);
/*
** 函数: luaL_fileresult
** 功能: 处理文件操作结果
**
** 参数:
** - L: Lua状态机
** - stat: 操作结果(通常是C库函数的返回值)
** - fname: 文件名(可选,用于错误信息)
**
** 返回值:
** - 成功时返回1,栈上是true
** - 失败时返回3,栈上是 nil, errmsg, errno
**
** 说明:
** - 这是一个辅助函数,统一处理文件操作的返回值
** - 遵循Lua中常见的错误处理模式:成功返回结果,失败返回nil+错误信息
** - 自动从errno获取系统错误信息
*/

LUALIB_API int(luaL_execresult)(lua_State *L, int stat);
/*
** 函数: luaL_execresult
** 功能: 处理进程执行结果
**
** 参数:
** - L: Lua状态机
** - stat: system()或其他进程函数的返回值
**
** 返回值:
** - 成功时返回1,栈上是true
** - 失败时返回3,栈上是 nil, "exit"/"signal", exitcode/signalno
**
** 说明:
** - 用于处理os.execute()等函数的返回值
** - 区分正常退出和被信号终止
*/

LUALIB_API void *luaL_alloc(void *ud, void *ptr, size_t osize,
                            size_t nsize);
/*
** 函数: luaL_alloc
** 功能: Lua的默认内存分配器
**
** 参数:
** - ud: 用户数据(未使用)
** - ptr: 原内存块指针
** - osize: 原大小
** - nsize: 新大小
**
** 返回值:
** - 新分配的内存指针,或NULL(释放内存时)
**
** 说明:
** - 这是lua_Alloc类型的函数,实现了Lua的内存分配接口
** - 内部使用标准C库的realloc()和free()
** - nsize==0时释放内存,ptr==NULL时分配新内存,否则重新分配
** - Lua的所有内存操作都通过这个接口,便于自定义内存管理
*/

/*
** ========================================================================
** 引用系统
** ========================================================================
*/

/* predefined references */
/* 预定义的引用 */
#define LUA_NOREF (-2)  /* 表示"无引用"的特殊值 */
#define LUA_REFNIL (-1) /* 对nil的引用 */
/*
** 说明: Lua引用系统的特殊值
**
** 引用系统允许在注册表中存储Lua值,并用整数来引用它们
** 这在C代码需要长期持有Lua对象时很有用
**
** - LUA_NOREF: 表示没有引用,用于初始化
** - LUA_REFNIL: 对nil值的引用(因为nil不能直接存储在表中)
**
** 正常的引用值都是非负整数
*/

LUALIB_API int(luaL_ref)(lua_State *L, int t);
/*
** 函数: luaL_ref
** 功能: 创建对栈顶值的引用
**
** 参数:
** - L: Lua状态机
** - t: 表的栈索引(通常是LUA_REGISTRYINDEX)
**
** 返回值:
** - 返回引用值(整数)
** - 栈顶值被弹出
**
** 工作原理:
** 1. 如果栈顶是nil,返回LUA_REFNIL,弹出nil
** 2. 否则,在表t中分配一个整数键,存储栈顶值
** 3. 返回这个整数键,弹出栈顶值
**
** 使用示例:
** lua_pushvalue(L, 1);  // 复制要引用的值
** int ref = luaL_ref(L, LUA_REGISTRYINDEX);  // 创建引用
** // 之后可以通过ref来访问这个值
*/

LUALIB_API void(luaL_unref)(lua_State *L, int t, int ref);
/*
** 函数: luaL_unref
** 功能: 释放引用
**
** 参数:
** - L: Lua状态机
** - t: 表的栈索引
** - ref: 要释放的引用值
**
** 说明:
** - 从表t中删除键为ref的条目
** - 之后ref值可能被重用
** - 如果ref是LUA_NOREF或LUA_REFNIL,不做任何操作
** - 不改变栈
**
** 使用示例:
** luaL_unref(L, LUA_REGISTRYINDEX, ref);  // 释放之前创建的引用
*/

/*
** ========================================================================
** 加载和执行Lua代码
** ========================================================================
*/

LUALIB_API int(luaL_loadfilex)(lua_State *L, const char *filename,
                               const char *mode);
/*
** 函数: luaL_loadfilex
** 功能: 加载Lua文件
**
** 参数:
** - L: Lua状态机
** - filename: 文件名,如果为NULL则从stdin读取
** - mode: 加载模式,"t"=仅文本,"b"=仅二进制,"bt"=两者都可,NULL=默认
**
** 返回值:
** - LUA_OK: 成功,编译后的函数在栈顶
** - LUA_ERRSYNTAX: 语法错误,错误信息在栈顶
** - LUA_ERRMEM: 内存分配错误
** - LUA_ERRFILE: 文件操作错误(无法打开等)
**
** 说明:
** - 只加载不执行,需要后续调用lua_pcall执行
** - 文本模式只接受Lua源码,二进制模式只接受编译后的字节码
*/

#define luaL_loadfile(L, f) luaL_loadfilex(L, f, NULL)
/*
** 宏: luaL_loadfile
** 功能: 加载文件(使用默认模式)
**
** 说明:
** - 等价于 luaL_loadfilex(L, f, NULL)
** - NULL模式表示接受文本或二进制
** - 这是最常用的文件加载方式
*/

LUALIB_API int(luaL_loadbufferx)(lua_State *L, const char *buff, size_t sz,
                                 const char *name, const char *mode);
/*
** 函数: luaL_loadbufferx
** 功能: 从内存缓冲区加载Lua代码
**
** 参数:
** - L: Lua状态机
** - buff: 包含Lua代码的缓冲区
** - sz: 缓冲区大小(字节)
** - name: 代码块名称(用于错误信息和调试)
** - mode: 加载模式(同luaL_loadfilex)
**
** 返回值:
** - 同luaL_loadfilex
**
** 说明:
** - buff可以是文本源码或编译后的字节码
** - name通常以'='或'@'开头表示特殊含义
*/

LUALIB_API int(luaL_loadstring)(lua_State *L, const char *s);
/*
** 函数: luaL_loadstring
** 功能: 加载字符串中的Lua代码
**
** 参数:
** - L: Lua状态机
** - s: 包含Lua代码的C字符串(以\0结尾)
**
** 返回值:
** - 同luaL_loadfilex
**
** 说明:
** - 是luaL_loadbufferx的简化版本
** - 使用字符串作为代码块名称
** - 只接受文本格式
*/

/*
** ========================================================================
** Lua状态创建和工具函数
** ========================================================================
*/

LUALIB_API lua_State *(luaL_newstate)(void);
/*
** 函数: luaL_newstate
** 功能: 创建新的Lua状态机
**
** 返回值:
** - 成功返回新的lua_State指针
** - 失败返回NULL(内存不足)
**
** 说明:
** - 这是创建Lua虚拟机的标准方法
** - 内部使用luaL_alloc作为内存分配器
** - 新状态机没有加载任何标准库,需要手动加载
** - 使用完毕后需要调用lua_close()释放
**
** 使用示例:
** lua_State *L = luaL_newstate();
** luaL_openlibs(L);  // 加载标准库
** // ... 使用L ...
** lua_close(L);
*/

LUALIB_API unsigned luaL_makeseed(lua_State *L);
/*
** 函数: luaL_makeseed
** 功能: 生成随机种子
**
** 参数:
** - L: Lua状态机
**
** 返回值:
** - 返回一个unsigned整数作为随机种子
**
** 说明:
** - 用于初始化Lua的随机数生成器
** - 内部可能使用时间、进程ID等生成种子
** - 不同的调用会尝试返回不同的值
*/

LUALIB_API lua_Integer(luaL_len)(lua_State *L, int idx);
/*
** 函数: luaL_len
** 功能: 获取值的长度
**
** 参数:
** - L: Lua状态机
** - idx: 栈索引
**
** 返回值:
** - 返回长度值
**
** 说明:
** - 对于字符串,返回字节数
** - 对于表,返回#操作符的结果(可能调用__len元方法)
** - 如果有__len元方法,调用它并返回结果
** - 不会在栈上留下任何值
*/

/*
** ========================================================================
** 字符串替换函数
** ========================================================================
*/

LUALIB_API void(luaL_addgsub)(luaL_Buffer *b, const char *s,
                              const char *p, const char *r);
/*
** 函数: luaL_addgsub
** 功能: 字符串全局替换并添加到缓冲区
**
** 参数:
** - b: 字符串缓冲区
** - s: 源字符串
** - p: 要查找的模式(普通字符串,非正则)
** - r: 替换字符串
**
** 说明:
** - 将s中所有的p替换为r,结果添加到缓冲区b
** - 不涉及模式匹配,只是简单的字符串查找替换
** - "gsub"代表"global substitution"(全局替换)
*/

LUALIB_API const char *(luaL_gsub)(lua_State * L, const char *s,
                                   const char *p, const char *r);
/*
** 函数: luaL_gsub
** 功能: 字符串全局替换
**
** 参数:
** - L: Lua状态机
** - s: 源字符串
** - p: 要查找的字符串
** - r: 替换字符串
**
** 返回值:
** - 返回替换后的字符串指针
** - 结果字符串被压入栈
**
** 说明:
** - 将s中所有的p替换为r
** - 内部使用luaL_Buffer实现
** - 返回的指针指向Lua管理的内存
*/

/*
** ========================================================================
** 库注册函数
** ========================================================================
*/

LUALIB_API void(luaL_setfuncs)(lua_State *L, const luaL_Reg *l, int nup);
/*
** 函数: luaL_setfuncs
** 功能: 注册函数列表到表中
**
** 参数:
** - L: Lua状态机
** - l: 函数注册数组(以{NULL,NULL}结尾)
** - nup: 上值(upvalue)数量
**
** 说明:
** - 将l中的所有函数注册到栈顶的表中
** - 如果nup>0,栈顶下方的nup个值会作为上值添加到每个函数
** - 上值是函数的私有数据,类似于闭包捕获的变量
**
** 栈变化:
** - 调用前: ... table [up1 up2 ... upn]
** - 调用后: ... table
**
** 使用示例:
** static const luaL_Reg mylib[] = {
**   {"func1", my_func1},
**   {"func2", my_func2},
**   {NULL, NULL}
** };
** lua_newtable(L);
** luaL_setfuncs(L, mylib, 0);
*/

LUALIB_API int(luaL_getsubtable)(lua_State *L, int idx, const char *fname);
/*
** 函数: luaL_getsubtable
** 功能: 获取或创建子表
**
** 参数:
** - L: Lua状态机
** - idx: 父表的栈索引
** - fname: 字段名
**
** 返回值:
** - 如果字段已存在且是表,返回1
** - 如果字段不存在,创建新表并返回0
**
** 说明:
** - 相当于 t[fname],但如果不存在则创建新表
** - 结果表被压入栈
** - 如果字段存在但不是表,抛出错误
**
** 典型用法:
** 获取或创建嵌套表结构,如package.loaded
*/

LUALIB_API void(luaL_traceback)(lua_State *L, lua_State *L1,
                                const char *msg, int level);
/*
** 函数: luaL_traceback
** 功能: 生成堆栈跟踪信息
**
** 参数:
** - L: 存放结果的Lua状态机
** - L1: 获取堆栈信息的Lua状态机(通常L==L1)
** - msg: 前缀消息(可以为NULL)
** - level: 起始层级(0=traceback本身,1=调用者,...)
**
** 说明:
** - 生成格式化的堆栈跟踪字符串,压入L的栈
** - 如果msg非NULL,它会作为第一行
** - 每一行显示函数名、文件名、行号等信息
** - 用于错误处理,提供详细的调用上下文
**
** 使用场景:
** 错误处理函数中,生成详细的错误报告
*/

LUALIB_API void(luaL_requiref)(lua_State *L, const char *modname,
                               lua_CFunction openf, int glb);
/*
** 函数: luaL_requiref
** 功能: 加载并注册模块
**
** 参数:
** - L: Lua状态机
** - modname: 模块名
** - openf: 模块的打开函数(返回模块表)
** - glb: 是否设置为全局变量(非0=是)
**
** 说明:
** - 调用openf加载模块
** - 将模块存储到package.loaded[modname]
** - 如果glb非0,将模块设置为全局变量
** - 将模块表压入栈
**
** 工作流程:
** 1. 检查package.loaded[modname]是否已加载
** 2. 如果未加载,调用openf(L)获取模块
** 3. 存储到package.loaded[modname]
** 4. 如果glb非0,设置_G[modname]=module
** 5. 将模块压入栈
**
** 使用示例:
** luaL_requiref(L, "math", luaopen_math, 1);  // 加载math库并设为全局
*/

/*
** ===============================================================
** some useful macros
** 一些有用的宏
** ===============================================================
*/

#define luaL_newlibtable(L, l) \
  lua_createtable(L, 0, sizeof(l) / sizeof((l)[0]) - 1)
/*
** 宏: luaL_newlibtable
** 功能: 创建预分配大小的表
**
** 参数:
** - L: Lua状态机
** - l: luaL_Reg数组
**
** 说明:
** - 计算l数组的大小(减1是因为最后有{NULL,NULL}哨兵)
** - 创建一个预分配足够空间的表
** - lua_createtable(L, narr, nrec)中narr是数组部分,nrec是哈希部分
** - 这里narr=0,nrec=函数数量,因为函数都以字符串为键
** - 预分配避免了后续添加元素时的rehash,提高性能
*/

#define luaL_newlib(L, l) \
  (luaL_checkversion(L), luaL_newlibtable(L, l), luaL_setfuncs(L, l, 0))
/*
** 宏: luaL_newlib
** 功能: 创建并注册库的一站式宏
**
** 参数:
** - L: Lua状态机
** - l: luaL_Reg数组
**
** 说明:
** - 组合了三个操作:版本检查、创建表、注册函数
** - 使用逗号运算符按顺序执行三个操作
** - 这是注册C库的标准方式
**
** 使用示例:
** static const luaL_Reg mylib[] = {
**   {"foo", l_foo},
**   {NULL, NULL}
** };
** int luaopen_mylib(lua_State *L) {
**   luaL_newlib(L, mylib);
**   return 1;  // 返回库表
** }
*/

#define luaL_argcheck(L, cond, arg, extramsg) \
  ((void)(luai_likely(cond) || luaL_argerror(L, (arg), (extramsg))))
/*
** 宏: luaL_argcheck
** 功能: 检查参数条件
**
** 参数:
** - L: Lua状态机
** - cond: 条件表达式
** - arg: 参数位置
** - extramsg: 错误信息
**
** 说明:
** - 如果cond为假,调用luaL_argerror抛出错误
** - luai_likely是一个编译器提示,表示cond通常为真
** - 使用||的短路特性:cond为真时不会调用luaL_argerror
** - (void)将结果转为void,避免编译器警告
**
** 使用示例:
** luaL_argcheck(L, x >= 0, 1, "value must be non-negative");
*/

#define luaL_argexpected(L, cond, arg, tname) \
  ((void)(luai_likely(cond) || luaL_typeerror(L, (arg), (tname))))
/*
** 宏: luaL_argexpected
** 功能: 检查参数类型期望
**
** 参数:
** - L: Lua状态机
** - cond: 条件表达式(通常是类型检查)
** - arg: 参数位置
** - tname: 期望的类型名
**
** 说明:
** - 类似luaL_argcheck,但专门用于类型检查
** - 条件不满足时抛出类型错误
**
** 使用示例:
** luaL_argexpected(L, lua_isnumber(L,1), 1, "number");
*/

#define luaL_checkstring(L, n) (luaL_checklstring(L, (n), NULL))
/*
** 宏: luaL_checkstring
** 功能: 检查字符串参数(不需要长度)
**
** 说明:
** - 调用luaL_checklstring,但长度参数传NULL
** - 当不需要知道字符串长度时使用
** - 返回const char*指针
*/

#define luaL_optstring(L, n, d) (luaL_optlstring(L, (n), (d), NULL))
/*
** 宏: luaL_optstring
** 功能: 获取可选字符串参数(不需要长度)
**
** 说明:
** - 调用luaL_optlstring,长度参数传NULL
** - d是默认值
*/

#define luaL_typename(L, i) lua_typename(L, lua_type(L, (i)))
/*
** 宏: luaL_typename
** 功能: 获取栈上值的类型名
**
** 参数:
** - L: Lua状态机
** - i: 栈索引
**
** 说明:
** - 先用lua_type获取类型代码(LUA_TNUMBER等)
** - 再用lua_typename将代码转为字符串("number"等)
** - 返回const char*指针
*/

#define luaL_dofile(L, fn) \
  (luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))
/*
** 宏: luaL_dofile
** 功能: 加载并执行文件
**
** 参数:
** - L: Lua状态机
** - fn: 文件名
**
** 返回值:
** - LUA_OK(0): 成功
** - 错误代码: 加载或执行失败,错误信息在栈上
**
** 说明:
** - 组合了加载和执行两步
** - 使用||的短路特性:加载失败时不会执行
** - lua_pcall参数: 0个参数,多个返回值,无错误处理函数
** - 如果成功,文件的返回值留在栈上
*/

#define luaL_dostring(L, s) \
  (luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))
/*
** 宏: luaL_dostring
** 功能: 加载并执行字符串
**
** 说明:
** - 与luaL_dofile类似,但针对字符串
** - 常用于执行简单的Lua代码片段
*/

#define luaL_getmetatable(L, n) (lua_getfield(L, LUA_REGISTRYINDEX, (n)))
/*
** 宏: luaL_getmetatable
** 功能: 从注册表获取元表
**
** 参数:
** - L: Lua状态机
** - n: 元表名称
**
** 说明:
** - 从注册表中获取键为n的元表
** - 将元表压入栈
** - 返回元表的类型
** - 这是获取已注册元表的标准方式
*/

#define luaL_opt(L, f, n, d) (lua_isnoneornil(L, (n)) ? (d) : f(L, (n)))
/*
** 宏: luaL_opt
** 功能: 获取可选参数的通用模式
**
** 参数:
** - L: Lua状态机
** - f: 获取函数(如luaL_checkinteger)
** - n: 参数位置
** - d: 默认值
**
** 说明:
** - 如果参数不存在或为nil,返回默认值d
** - 否则调用f(L,n)获取参数
** - lua_isnoneornil检查栈索引n是否超出范围或为nil
** - 这是实现各种luaL_opt*函数的基础
**
** 使用示例:
** int x = luaL_opt(L, luaL_checkinteger, 2, 10);  // 第2个参数,默认10
*/

#define luaL_loadbuffer(L, s, sz, n) luaL_loadbufferx(L, s, sz, n, NULL)
/*
** 宏: luaL_loadbuffer
** 功能: 加载缓冲区(默认模式)
**
** 说明:
** - 调用luaL_loadbufferx,模式参数为NULL
** - NULL模式表示自动检测文本/二进制
*/

/*
** Perform arithmetic operations on lua_Integer values with wrap-around
** semantics, as the Lua core does.
** 对lua_Integer值执行算术运算,使用环绕语义,就像Lua核心所做的那样
*/
#define luaL_intop(op, v1, v2) \
  ((lua_Integer)((lua_Unsigned)(v1)op(lua_Unsigned)(v2)))
/*
** 宏: luaL_intop
** 功能: 整数运算(带环绕语义)
**
** 参数:
** - op: 运算符(+, -, *, 等)
** - v1, v2: lua_Integer类型的操作数
**
** 说明:
** - Lua整数运算使用二进制补码表示,溢出时会环绕
** - 例如:在64位系统,INT64_MAX + 1 = INT64_MIN
** - 为了确保这种行为,先转为无符号类型运算,再转回有符号
** - 无符号运算的溢出行为是明确定义的(模2^N)
** - 有符号溢出在C语言中是未定义行为,所以要避免
**
** 为什么要这样?
** - 确保跨平台的一致性
** - 避免未定义行为
** - 匹配Lua语言规范的要求
*/

/* push the value used to represent failure/error */
/* 压入用于表示失败/错误的值 */
#if defined(LUA_FAILISFALSE)
#define luaL_pushfail(L) lua_pushboolean(L, 0)
#else
#define luaL_pushfail(L) lua_pushnil(L)
#endif
/*
** 宏: luaL_pushfail
** 功能: 压入表示失败的值
**
** 说明:
** - 在Lua中,失败通常用nil表示
** - 但某些配置可能使用false(如果定义了LUA_FAILISFALSE)
** - 这个宏根据配置选择合适的失败值
** - 用于统一的错误处理模式:成功返回结果,失败返回nil(或false)
**
** 使用场景:
** 文件操作、搜索操作等可能失败的函数
*/

/*
** {======================================================
** Generic Buffer manipulation
** 通用缓冲区操作
** =======================================================
*/

struct luaL_Buffer
{
  char *b;      /* buffer address 缓冲区地址 */
  size_t size;  /* buffer size 缓冲区大小 */
  size_t n;     /* number of characters in buffer 缓冲区中的字符数 */
  lua_State *L; /* Lua状态机指针 */
  union
  {
    LUAI_MAXALIGN;           /* ensure maximum alignment for buffer 确保缓冲区的最大对齐 */
    char b[LUAL_BUFFERSIZE]; /* initial buffer 初始缓冲区 */
  } init;
};
/*
** 结构体: luaL_Buffer
** 功能: 高效的字符串构建缓冲区
**
** 成员说明:
** - b: 指向当前缓冲区的指针(可能指向init.b或动态分配的内存)
** - size: 当前缓冲区的总大小
** - n: 已使用的字节数
** - L: 关联的Lua状态机
** - init: 联合体,包含初始缓冲区
**
** init联合体:
** - LUAI_MAXALIGN: 确保最大对齐(通常是lua_Number,保证正确对齐)
** - b[LUAL_BUFFERSIZE]: 初始缓冲区(栈上分配,避免小字符串的堆分配)
**
** 工作原理:
** 1. 初始时,b指向init.b,大小为LUAL_BUFFERSIZE
** 2. 如果需要更大空间,动态分配新缓冲区,b指向它
** 3. 最后通过luaL_pushresult将内容转为Lua字符串
**
** 优点:
** - 避免频繁的字符串拼接和内存分配
** - 小字符串使用栈缓冲,快速
** - 大字符串自动扩展
**
** 使用模式:
** luaL_Buffer b;
** luaL_buffinit(L, &b);
** luaL_addstring(&b, "hello");
** luaL_addstring(&b, " world");
** luaL_pushresult(&b);  // 结果字符串在栈上
*/

#define luaL_bufflen(bf) ((bf)->n)
/*
** 宏: luaL_bufflen
** 功能: 获取缓冲区当前长度
**
** 返回值: 缓冲区中已有的字节数
*/

#define luaL_buffaddr(bf) ((bf)->b)
/*
** 宏: luaL_buffaddr
** 功能: 获取缓冲区地址
**
** 返回值: 指向缓冲区数据的指针
**
** 注意: 添加数据后地址可能改变(扩展时重新分配)
*/

#define luaL_addchar(B, c)                                  \
  ((void)((B)->n < (B)->size || luaL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))
/*
** 宏: luaL_addchar
** 功能: 向缓冲区添加单个字符
**
** 参数:
** - B: luaL_Buffer指针
** - c: 要添加的字符
**
** 说明:
** - 使用逗号运算符分两步:
**   1. 确保有足够空间(如果没有,调用luaL_prepbuffsize扩展)
**   2. 将字符写入缓冲区并增加计数
** - ||的短路特性:有空间时不调用prepbuffsize
** - (void)丢弃第一步的结果
** - 这是一个高效的单字符添加操作
*/

#define luaL_addsize(B, s) ((B)->n += (s))
/*
** 宏: luaL_addsize
** 功能: 增加缓冲区已使用大小
**
** 参数:
** - B: luaL_Buffer指针
** - s: 增加的字节数
**
** 说明:
** - 通常在直接写入缓冲区后使用
** - 例如:先用luaL_prepbuffsize获取空间,然后用sprintf写入,
**   最后用luaL_addsize更新大小
*/

#define luaL_buffsub(B, s) ((B)->n -= (s))
/*
** 宏: luaL_buffsub
** 功能: 减少缓冲区已使用大小
**
** 参数:
** - B: luaL_Buffer指针
** - s: 减少的字节数
**
** 说明:
** - 用于"撤销"最后添加的内容
** - 不会真正删除数据,只是调整计数
*/

LUALIB_API void(luaL_buffinit)(lua_State *L, luaL_Buffer *B);
/*
** 函数: luaL_buffinit
** 功能: 初始化缓冲区
**
** 参数:
** - L: Lua状态机
** - B: 要初始化的缓冲区
**
** 说明:
** - 设置B的初始状态
** - b指向init.b
** - size设为LUAL_BUFFERSIZE
** - n设为0
** - 记录关联的Lua状态机
** - 必须在使用缓冲区前调用
*/

LUALIB_API char *(luaL_prepbuffsize)(luaL_Buffer * B, size_t sz);
/*
** 函数: luaL_prepbuffsize
** 功能: 准备指定大小的缓冲区空间
**
** 参数:
** - B: 缓冲区指针
** - sz: 需要的额外空间大小
**
** 返回值:
** - 返回可写入的缓冲区地址
**
** 说明:
** - 确保缓冲区至少还有sz字节可用空间
** - 如果空间不足,会扩展缓冲区(可能重新分配)
** - 返回的地址是从B->b + B->n开始的位置
** - 写入数据后需要调用luaL_addsize更新大小
**
** 使用示例:
** char *p = luaL_prepbuffsize(&b, 100);
** int written = sprintf(p, "%d", value);
** luaL_addsize(&b, written);
*/

LUALIB_API void(luaL_addlstring)(luaL_Buffer *B, const char *s, size_t l);
/*
** 函数: luaL_addlstring
** 功能: 向缓冲区添加指定长度的字符串
**
** 参数:
** - B: 缓冲区指针
** - s: 字符串指针
** - l: 字符串长度
**
** 说明:
** - 将s指向的l个字节复制到缓冲区
** - 字符串可以包含\0字符(因为指定了长度)
** - 内部处理空间分配
*/

LUALIB_API void(luaL_addstring)(luaL_Buffer *B, const char *s);
/*
** 函数: luaL_addstring
** 功能: 向缓冲区添加C字符串
**
** 参数:
** - B: 缓冲区指针
** - s: C字符串(以\0结尾)
**
** 说明:
** - 等价于 luaL_addlstring(B, s, strlen(s))
** - 用于添加C风格字符串
*/

LUALIB_API void(luaL_addvalue)(luaL_Buffer *B);
/*
** 函数: luaL_addvalue
** 功能: 将栈顶的值添加到缓冲区
**
** 参数:
** - B: 缓冲区指针
**
** 说明:
** - 栈顶值必须是字符串(或可转换为字符串)
** - 将字符串内容添加到缓冲区
** - 弹出栈顶值
** - 用于将Lua字符串添加到缓冲区
*/

LUALIB_API void(luaL_pushresult)(luaL_Buffer *B);
/*
** 函数: luaL_pushresult
** 功能: 完成缓冲区并将结果压入栈
**
** 参数:
** - B: 缓冲区指针
**
** 说明:
** - 将缓冲区内容转换为Lua字符串
** - 字符串被压入栈
** - 缓冲区变为无效状态,不应再使用
** - 如果使用了动态分配的内存,会被释放
** - 这是使用缓冲区的最后一步
*/

LUALIB_API void(luaL_pushresultsize)(luaL_Buffer *B, size_t sz);
/*
** 函数: luaL_pushresultsize
** 功能: 完成缓冲区(指定最后一段的大小)
**
** 参数:
** - B: 缓冲区指针
** - sz: 最后写入的字节数
**
** 说明:
** - 类似luaL_pushresult,但先调整大小
** - 等价于 luaL_addsize(B, sz); luaL_pushresult(B);
** - 用于在直接写入缓冲区后完成
*/

LUALIB_API char *(luaL_buffinitsize)(lua_State * L, luaL_Buffer *B, size_t sz);
/*
** 函数: luaL_buffinitsize
** 功能: 初始化缓冲区并准备指定大小
**
** 参数:
** - L: Lua状态机
** - B: 缓冲区指针
** - sz: 初始需要的大小
**
** 返回值:
** - 返回可写入的缓冲区地址
**
** 说明:
** - 等价于 luaL_buffinit(L, B); return luaL_prepbuffsize(B, sz);
** - 用于一次性初始化并准备空间
** - 适合已知所需大小的场景
*/

#define luaL_prepbuffer(B) luaL_prepbuffsize(B, LUAL_BUFFERSIZE)
/*
** 宏: luaL_prepbuffer
** 功能: 准备默认大小的缓冲区空间
**
** 说明:
** - 准备LUAL_BUFFERSIZE字节的空间
** - 这是一个常用的便捷宏
** - LUAL_BUFFERSIZE通常定义为较小的值(如512字节)
*/

/* }====================================================== */

/*
** {======================================================
** File handles for IO library
** IO库的文件句柄
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'LUA_FILEHANDLE' and
** initial structure 'luaL_Stream' (it may contain other fields
** after that initial structure).
** 文件句柄是一个userdata,其元表为'LUA_FILEHANDLE',
** 初始结构为'luaL_Stream'(在该初始结构之后可能包含其他字段)
*/

#define LUA_FILEHANDLE "FILE*"
/*
** 宏: LUA_FILEHANDLE
** 功能: 文件句柄元表的名称
**
** 说明:
** - 这是注册表中存储文件句柄元表的键
** - 所有Lua文件对象共享这个元表
** - 元表包含文件操作的元方法(如__gc用于关闭文件)
*/

typedef struct luaL_Stream
{
  FILE *f;              /* stream (NULL for incompletely created streams)
                           流(对于未完全创建的流为NULL) */
  lua_CFunction closef; /* to close stream (NULL for closed streams)
                           关闭流的函数(对于已关闭的流为NULL) */
} luaL_Stream;
/*
** 结构体: luaL_Stream
** 功能: 表示一个文件流
**
** 成员说明:
** - f: C标准库的FILE指针
**      NULL表示流未完全创建或创建失败
** - closef: 关闭流的函数指针
**      通常是一个调用fclose()的Lua C函数
**      NULL表示流已关闭
**
** 使用方式:
** 1. 创建userdata,大小至少为sizeof(luaL_Stream)
** 2. 设置元表为LUA_FILEHANDLE
** 3. 设置f和closef
** 4. 当对象被垃圾回收时,__gc元方法调用closef关闭文件
**
** 为什么需要closef?
** - 不同类型的流可能需要不同的关闭方式
** - stdin/stdout/stderr不应该被fclose()
** - 管道可能需要pclose()而不是fclose()
** - 通过函数指针提供灵活性
**
** 扩展:
** 可以在luaL_Stream后面添加额外字段,例如:
** struct MyStream {
**   luaL_Stream stream;  // 必须是第一个成员
**   int my_extra_data;
** };
*/

/* }====================================================== */

/*
** {============================================================
** Compatibility with deprecated conversions
** 与已弃用转换的兼容性
** =============================================================
*/
#if defined(LUA_COMPAT_APIINTCASTS)
/*
** 说明: 兼容性宏
**
** 如果定义了LUA_COMPAT_APIINTCASTS,提供旧版本的类型转换函数
** 这些函数在新版本中已不推荐使用,但为了向后兼容保留
*/

#define luaL_checkunsigned(L, a) ((lua_Unsigned)luaL_checkinteger(L, a))
/*
** 宏: luaL_checkunsigned (已弃用)
** 功能: 检查无符号整数参数
** 说明: 将有符号整数强制转换为无符号整数
*/

#define luaL_optunsigned(L, a, d) \
  ((lua_Unsigned)luaL_optinteger(L, a, (lua_Integer)(d)))
/*
** 宏: luaL_optunsigned (已弃用)
** 功能: 获取可选的无符号整数参数
*/

#define luaL_checkint(L, n) ((int)luaL_checkinteger(L, (n)))
/*
** 宏: luaL_checkint (已弃用)
** 功能: 检查int类型参数
** 说明: 将lua_Integer转换为int,可能截断
*/

#define luaL_optint(L, n, d) ((int)luaL_optinteger(L, (n), (d)))
/*
** 宏: luaL_optint (已弃用)
** 功能: 获取可选的int类型参数
*/

#define luaL_checklong(L, n) ((long)luaL_checkinteger(L, (n)))
/*
** 宏: luaL_checklong (已弃用)
** 功能: 检查long类型参数
** 说明: 将lua_Integer转换为long
*/

#define luaL_optlong(L, n, d) ((long)luaL_optinteger(L, (n), (d)))
/*
** 宏: luaL_optlong (已弃用)
** 功能: 获取可选的long类型参数
*/

#endif
/* }============================================================ */

#endif /* lauxlib_h */

/*
** ========================================================================
** 文件总结
** ========================================================================
**
** 本文件(lauxlib.h)是Lua辅助库的头文件,为C语言扩展Lua提供了丰富的工具函数。
**
** 核心设计思想:
**
** 1. 抽象层次
**    - lua.h提供低层次的C API,直接操作Lua虚拟机
**    - lauxlib.h提供高层次的辅助函数,封装常见模式
**    - 开发者通常使用辅助库,只在必要时使用底层API
**
** 2. 错误处理
**    - 统一的错误处理机制:检查函数失败时抛出错误
**    - 自动生成有用的错误信息(包含参数位置、期望类型等)
**    - 简化了C代码中的错误处理
**
** 3. 类型安全
**    - 提供类型检查函数,确保参数类型正确
**    - 支持可选参数和默认值
**    - 自动进行合理的类型转换(如字符串到数字)
**
** 4. 内存管理
**    - luaL_Buffer提供高效的字符串构建机制
**    - 引用系统允许C代码安全地持有Lua对象
**    - 所有内存由Lua管理,避免内存泄漏
**
** 5. 便利性
**    - 大量宏简化常见操作
**    - 统一的命名约定(luaL_前缀)
**    - 完整的文档和使用示例
**
** 主要功能分类总结:
**
** A. 参数处理(30%的API)
**    - luaL_check*系列: 检查必需参数
**    - luaL_opt*系列: 处理可选参数
**    - luaL_arg*系列: 参数错误处理
**
** B. 元表和类型(20%的API)
**    - luaL_newmetatable, luaL_setmetatable: 元表管理
**    - luaL_testudata, luaL_checkudata: userdata类型检查
**    - 支持自定义类型系统
**
** C. 缓冲区(15%的API)
**    - luaL_Buffer及相关函数: 高效字符串构建
**    - 避免频繁的字符串拼接
**    - 优化内存使用
**
** D. 代码加载(10%的API)
**    - luaL_loadfile, luaL_loadstring等: 加载Lua代码
**    - 支持文本和二进制格式
**    - 统一的错误处理
**
** E. 库注册(10%的API)
**    - luaL_Reg结构体和luaL_setfuncs: 批量注册函数
**    - luaL_newlib宏: 一站式库创建
**    - 简化库的开发流程
**
** F. 工具函数(15%的API)
**    - 引用系统(luaL_ref/luaL_unref)
**    - 字符串替换(luaL_gsub)
**    - 堆栈跟踪(luaL_traceback)
**    - 其他辅助功能
**
** 使用建议:
**
** 1. 总是在库初始化时调用luaL_checkversion()
**    - 确保二进制兼容性
**
** 2. 使用luaL_check*系列函数验证参数
**    - 提供清晰的错误信息
**    - 避免手动类型检查的样板代码
**
** 3. 使用luaL_Buffer构建字符串
**    - 比多次字符串拼接更高效
**    - 自动管理内存
**
** 4. 使用luaL_Reg和luaL_newlib注册函数
**    - 代码更清晰
**    - 减少错误
**
** 5. 为自定义类型创建唯一的元表
**    - 使用luaL_newmetatable注册
**    - 使用luaL_checkudata检查类型
**
** 典型的Lua C模块结构:
**
** ```c
** // 1. 定义函数
** static int l_myfunc(lua_State *L) {
**   double x = luaL_checknumber(L, 1);
**   lua_pushnumber(L, x * 2);
**   return 1;
** }
**
** // 2. 定义函数表
** static const luaL_Reg mylib[] = {
**   {"myfunc", l_myfunc},
**   {NULL, NULL}
** };
**
** // 3. 定义打开函数
** int luaopen_mylib(lua_State *L) {
**   luaL_newlib(L, mylib);
**   return 1;
** }
** ```
**
** 与lua.h的关系:
** - lua.h是核心API,必须包含
** - lauxlib.h基于lua.h构建
** - lauxlib.h不是必需的,但强烈推荐使用
** - 标准库自己也使用lauxlib.h
**
** 历史和演进:
** - 辅助库随Lua发展不断改进
** - 新版本添加了更多便利函数
** - 但保持了向后兼容性(通过LUA_COMPAT_*宏)
**
** 性能考虑:
** - 大多数辅助函数只是薄封装,性能开销很小
** - luaL_Buffer经过优化,比手动拼接字符串快
** - 参数检查的开销通常是值得的(提前发现错误)
**
** 扩展性:
** - 可以基于这个库创建更高层的抽象
** - 许多第三方库提供额外的辅助函数
** - 可以学习这个库的设计模式来编写自己的辅助函数
**
** 这个头文件是Lua C API的重要组成部分,掌握它对于:
** - 编写Lua C扩展
** - 嵌入Lua到C程序
** - 理解Lua的内部机制
** 都是非常重要的。
**
** ========================================================================
*/
