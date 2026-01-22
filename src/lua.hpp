/*******************************************************************************
 * 文件名: lua.hpp
 *
 * 【概要说明】
 * 这是 Lua 的 C++ 头文件包装器。Lua 核心库本身是用 C 语言编写的,可以同时
 * 作为 C 和 C++ 代码编译。但是当在 C++ 项目中使用 Lua 时,需要告诉 C++ 编译器
 * 这些函数使用的是 C 语言的链接约定(name mangling规则),而不是 C++ 的链接约定。
 *
 * 【为什么需要这个文件】
 * - C++ 编译器会对函数名进行"名称修饰"(name mangling),以支持函数重载等特性
 * - C 编译器不会进行名称修饰,函数名保持原样
 * - 如果直接在 C++ 中包含 Lua 的 C 头文件,链接器会找不到函数(因为名称不匹配)
 * - 使用 extern "C" 告诉 C++ 编译器:这些函数使用 C 链接约定,不要修饰名称
 *
 * 【使用方式】
 * 在 C++ 项目中,包含这个文件而不是直接包含 lua.h:
 *   #include "lua.hpp"  // 正确:在 C++ 中使用
 *   而不是:
 *   #include "lua.h"    // 错误:会导致链接错误
 *
 * 【文件结构】
 * 1. extern "C" 块 - 声明 C 语言链接约定
 * 2. 包含三个核心 Lua 头文件
 ******************************************************************************/

// Lua header files for C++
// Lua 的 C++ 头文件

// 'extern "C" not supplied automatically in lua.h and other headers
// because Lua also compiles as C++
// 'extern "C"' 不会在 lua.h 和其他头文件中自动提供,
// 因为 Lua 本身也可以作为 C++ 代码编译

/*
 * 【关键知识点:extern "C"】
 *
 * extern "C" 是 C++ 中的链接规范(linkage specification),用于:
 *
 * 1. 告诉 C++ 编译器使用 C 语言的链接约定
 * 2. 禁止 C++ 的名称修饰(name mangling)
 * 3. 使得 C++ 代码可以调用 C 语言编译的库
 *
 * 例如:
 * - C 函数: void foo(int x)  -> 链接符号: foo
 * - C++ 函数: void foo(int x) -> 链接符号: _Z3fooi (经过名称修饰)
 *
 * 如果没有 extern "C",C++ 会生成修饰后的名称,导致链接失败。
 */

extern "C"
{
/*
 * 【包含的头文件说明】
 *
 * 以下三个头文件是 Lua 的核心 API:
 */

/*
 * lua.h - Lua 核心 API
 *
 * 包含内容:
 * - Lua 虚拟机的核心数据结构 (lua_State)
 * - 基本 API 函数 (栈操作、函数调用、垃圾回收等)
 * - 数据类型定义 (LUA_TNUMBER, LUA_TSTRING 等)
 * - 基本常量和宏定义
 *
 * 主要函数类别:
 * - 状态管理: lua_newstate(), lua_close()
 * - 栈操作: lua_pushstring(), lua_tonumber(), lua_pop()
 * - 函数调用: lua_call(), lua_pcall()
 * - 表操作: lua_newtable(), lua_settable(), lua_gettable()
 * - 元表和元方法: lua_getmetatable(), lua_setmetatable()
 */
#include "lua.h"

/*
 * lualib.h - Lua 标准库
 *
 * 包含内容:
 * - 标准库的打开函数声明
 *
 * 主要函数:
 * - luaL_openlibs()  - 打开所有标准库
 * - luaopen_base()   - 基础库 (print, type, pairs 等)
 * - luaopen_package()- 包管理库 (require, module)
 * - luaopen_string() - 字符串库 (string.sub, string.find 等)
 * - luaopen_table()  - 表操作库 (table.insert, table.sort 等)
 * - luaopen_math()   - 数学库 (math.sin, math.random 等)
 * - luaopen_io()     - I/O 库 (io.open, io.read 等)
 * - luaopen_os()     - 操作系统库 (os.time, os.execute 等)
 * - luaopen_debug()  - 调试库 (debug.traceback 等)
 *
 * 使用示例:
 *   lua_State *L = luaL_newstate();
 *   luaL_openlibs(L);  // 打开所有标准库
 */
#include "lualib.h"

/*
 * lauxlib.h - Lua 辅助库
 *
 * 包含内容:
 * - 更高级、更便利的 API 函数
 * - 错误处理辅助函数
 * - 参数检查辅助函数
 * - 缓冲区操作
 *
 * 主要函数类别:
 * - 状态创建: luaL_newstate() (创建新的 Lua 状态机)
 * - 脚本执行: luaL_dofile(), luaL_dostring()
 * - 错误处理: luaL_error(), luaL_argerror()
 * - 参数检查: luaL_checkstring(), luaL_checknumber(), luaL_checktype()
 * - 注册函数: luaL_register(), luaL_newlib()
 * - 缓冲区: luaL_Buffer 及相关函数
 *
 * 这个库让 C/C++ 与 Lua 的交互更加简单和安全。
 * 例如:
 * - lua_tostring() 可能返回 NULL,需要手动检查
 * - luaL_checkstring() 会自动检查并在出错时抛出 Lua 错误
 */
#include "lauxlib.h"
}

/*
 * 【结束 extern "C" 块】
 *
 * 右大括号标志着 C 链接约定声明的结束。
 * 在这个块之后的代码将恢复使用 C++ 的链接约定。
 */

/*******************************************************************************
 * 【总结说明】
 *
 * 1. 【文件目的】
 *    这是一个简单但关键的包装文件,解决了 C++ 项目使用 Lua C API 的兼容性问题。
 *
 * 2. 【核心机制】
 *    - 使用 extern "C" 声明 C 链接约定
 *    - 包含三个核心 Lua 头文件
 *    - 确保 C++ 编译器正确链接到 Lua 的 C 库
 *
 * 3. 【为什么 Lua 不在自己的头文件中添加 extern "C"】
 *    如注释所说,Lua 本身也可以作为 C++ 代码编译。如果在 lua.h 中直接
 *    使用 extern "C",那么当 Lua 作为 C++ 编译时会出现问题。因此:
 *    - lua.h 等文件保持纯 C 兼容
 *    - 提供单独的 lua.hpp 给 C++ 用户使用
 *
 * 4. 【使用场景】
 *    当你在 C++ 项目中嵌入 Lua 时,例如:
 *
 *    // main.cpp (C++ 文件)
 *    #include "lua.hpp"  // 使用这个包装器
 *
 *    int main() {
 *        lua_State *L = luaL_newstate();  // 创建 Lua 状态机
 *        luaL_openlibs(L);                // 打开标准库
 *        luaL_dostring(L, "print('Hello from Lua!')");
 *        lua_close(L);                    // 关闭状态机
 *        return 0;
 *    }
 *
 * 5. 【相关知识点】
 *    - C/C++ 链接约定差异
 *    - 名称修饰(name mangling)
 *    - extern "C" 的作用和原理
 *    - Lua 的模块化设计(核心、标准库、辅助库分离)
 *
 * 6. 【文件大小】
 *    虽然这个文件只有几行代码,但它是 C++ 项目使用 Lua 的必要桥梁。
 *    这体现了良好的软件设计:职责单一,接口清晰。
 *
 ******************************************************************************/
