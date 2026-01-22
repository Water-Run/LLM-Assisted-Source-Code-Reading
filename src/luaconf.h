/*
** $Id: luaconf.h $
** Configuration file for Lua (Lua的配置文件)
** See Copyright Notice in lua.h (版权声明见lua.h)
*/

/*
================================================================================
文件概要说明
================================================================================

【文件名称】luaconf.h
【文件作用】Lua解释器的核心配置文件,控制Lua的编译时行为和特性

【主要功能】
1. 系统平台适配配置(Windows/POSIX/macOS/iOS等)
2. 数值类型配置(整数和浮点数的类型选择)
3. 路径配置(Lua库和C库的搜索路径)
4. API导出标记(DLL导出/导入控制)
5. 兼容性设置(与旧版本Lua的兼容性)
6. 底层实现细节配置(格式化、数值转换等)

【关键概念】
- 这个文件通过宏定义(#define)来控制Lua的编译选项
- 某些定义可以通过编译器参数(-D选项)外部修改
- 某些定义必须在此文件中直接修改,因为它们影响Lua ABI(应用程序二进制接口)
- 搜索"@@"可以找到所有可配置的定义

【阅读提示】
- 本文件大量使用C预处理器指令(#define, #if, #ifdef等)
- 条件编译用于根据不同平台和需求选择不同的实现
- 理解本文件有助于了解Lua如何适配不同的平台和使用场景

================================================================================
*/

#ifndef luaconf_h
#define luaconf_h
/*
 * 头文件保护(Header Guard):
 * - 防止头文件被重复包含,这是C语言的标准做法
 * - #ifndef检查luaconf_h是否未定义
 * - 如果未定义,则定义它并包含下面的内容
 * - #endif结束条件编译块
 */

#include <limits.h>
/*
 * <limits.h> 标准C库头文件:
 * - 定义了各种整数类型的限制值
 * - 例如:INT_MAX(int的最大值), UINT_MAX(unsigned int的最大值)
 * - LONG_MAX, LLONG_MAX等
 * - 本文件中用于确定整数类型的大小和范围
 */

#include <stddef.h>
/*
 * <stddef.h> 标准C库头文件:
 * - 定义了一些标准类型和宏
 * - ptrdiff_t: 两个指针相减的结果类型
 * - size_t: sizeof运算符的结果类型
 * - NULL: 空指针常量
 * - 本文件中主要用到ptrdiff_t作为上下文类型
 */

/*
** ===================================================================
** General Configuration File for Lua (Lua通用配置文件)
**
** Some definitions here can be changed externally, through the compiler
** (某些定义可以通过编译器外部修改)
** (e.g., with '-D' options): They are commented out or protected
** (例如,使用'-D'选项):它们被注释掉或受到保护)
** by '#if !defined' guards. However, several other definitions
** (通过'#if !defined'保护。然而,其他几个定义)
** should be changed directly here, either because they affect the
** (应该直接在这里修改,要么因为它们影响)
** Lua ABI (by making the changes here, you ensure that all software
** (Lua ABI(通过在这里修改,你确保所有软件)
** connected to Lua, such as C libraries, will be compiled with the same
** (连接到Lua的软件,如C库,将使用相同的)
** configuration); or because they are seldom changed.
** (配置编译);要么因为它们很少改变。)
**
** Search for "@@" to find all configurable definitions.
** (搜索"@@"以找到所有可配置的定义。)
** ===================================================================
*/

/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Lua to some
** (系统配置:宏定义用于(如果需要)将Lua适配到某些)
** particular platform, for instance restricting it to C89.
** (特定平台,例如限制它为C89标准。)
** =====================================================================
*/

/*
@@ LUA_USE_C89 controls the use of non-ISO-C89 features.
** (LUA_USE_C89 控制非ISO-C89特性的使用。)
** Define it if you want Lua to avoid the use of a few C99 features
** (如果你想让Lua避免使用一些C99特性,请定义它)
** or Windows-specific features on Windows.
** (或Windows上的Windows特定特性。)
*/
/* #define LUA_USE_C89 */
/*
 * 说明:
 * - 这个宏被注释掉了,意味着默认不启用
 * - C89: 1989年的C标准(也称ANSI C),功能较少但兼容性最好
 * - C99: 1999年的C标准,增加了许多新特性(如long long类型)
 * - 如果定义此宏,Lua将只使用C89标准的特性,提高在老旧编译器上的兼容性
 * - 但这会限制某些功能,例如不能使用long long整数类型
 */

/*
** By default, Lua on Windows use (some) specific Windows features
** (默认情况下,Windows上的Lua使用(一些)特定的Windows特性)
*/
#if !defined(LUA_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
/*
 * 条件判断解析:
 * - !defined(LUA_USE_C89): 没有定义LUA_USE_C89
 * - defined(_WIN32): 在Windows平台上编译(_WIN32是编译器预定义的宏)
 * - !defined(_WIN32_WCE): 不是Windows CE(嵌入式Windows)
 * - 三个条件都满足时,启用Windows特性
 */
#define LUA_USE_WINDOWS /* enable goodies for regular Windows (为常规Windows启用好功能) */
/*
 * 说明:
 * - 在普通Windows平台上(非C89模式,非WinCE),定义LUA_USE_WINDOWS
 * - 这将启用Windows特定的功能,如DLL支持、特定的路径处理等
 */
#endif

#if defined(LUA_USE_WINDOWS)
/*
 * 如果定义了LUA_USE_WINDOWS(即在Windows平台上)
 */
#define LUA_DL_DLL /* enable support for DLL (启用DLL支持) */
/*
 * LUA_DL_DLL说明:
 * - DLL: Dynamic Link Library(动态链接库),Windows上的共享库格式
 * - 启用此选项后,Lua可以动态加载DLL格式的C模块
 * - 对应的加载函数会使用Windows API(LoadLibrary等)
 */
#define LUA_USE_C89 /* broadly, Windows is C89 (广义上,Windows是C89) */
/*
 * 说明:
 * - Windows编译器通常对C99支持不完整
 * - 因此在Windows上,Lua采用更保守的C89兼容模式
 * - 这确保在各种Windows编译器上都能正常编译
 */
#endif

/*
** When POSIX DLL ('LUA_USE_DLOPEN') is enabled, the Lua stand-alone
** (当启用POSIX DLL('LUA_USE_DLOPEN')时,Lua独立)
** application will try to dynamically link a 'readline' facility
** (应用程序将尝试动态链接'readline'功能)
** for its REPL.  In that case, LUA_READLINELIB is the name of the
** (用于其REPL。在这种情况下,LUA_READLINELIB是)
** library it will look for those facilities.  If lua.c cannot open
** (将查找这些功能的库名称。如果lua.c无法打开)
** the specified library, it will generate a warning and then run
** (指定的库,它将生成警告然后运行)
** without 'readline'.  If that macro is not defined, lua.c will not
** (不带'readline'。如果未定义该宏,lua.c将不会)
** use 'readline'.
** (使用'readline'。)
*/
#if defined(LUA_USE_LINUX)
/*
 * Linux平台配置
 */
#define LUA_USE_POSIX
/*
 * POSIX: Portable Operating System Interface(可移植操作系统接口)
 * - POSIX是Unix系统的标准API规范
 * - Linux实现了POSIX标准
 * - 定义此宏后,Lua将使用POSIX API
 */
#define LUA_USE_DLOPEN /* needs an extra library: -ldl (需要额外的库:-ldl) */
/*
 * dlopen相关说明:
 * - dlopen是POSIX标准的动态加载库的函数
 * - 用于在运行时加载共享库(.so文件)
 * - Linux上需要链接libdl库(编译时加 -ldl 参数)
 * - dlopen/dlsym/dlclose等函数在<dlfcn.h>中声明
 */
#define LUA_READLINELIB "libreadline.so"
/*
 * readline库说明:
 * - readline是一个提供命令行编辑功能的库
 * - 提供历史记录、自动补全、行编辑等功能
 * - 增强Lua交互式解释器(REPL)的用户体验
 * - "libreadline.so"是Linux上readline库的文件名
 */
#endif

#if defined(LUA_USE_MACOSX)
/*
 * macOS(苹果Mac操作系统)平台配置
 */
#define LUA_USE_POSIX
/*
 * macOS也是类Unix系统,实现了POSIX标准
 */
#define LUA_USE_DLOPEN /* macOS does not need -ldl (macOS不需要-ldl) */
/*
 * 说明:
 * - macOS也支持dlopen动态加载
 * - 但与Linux不同,macOS的dlopen函数在系统库中,不需要额外链接libdl
 * - 编译时不需要 -ldl 参数
 */
#define LUA_READLINELIB "libedit.dylib"
/*
 * libedit说明:
 * - macOS使用libedit而不是readline(许可证原因)
 * - libedit提供与readline兼容的接口
 * - .dylib是macOS的动态库扩展名(相当于Linux的.so)
 */
#endif

#if defined(LUA_USE_IOS)
/*
 * iOS(苹果移动操作系统)平台配置
 */
#define LUA_USE_POSIX
/*
 * iOS基于macOS,也实现了POSIX标准
 */
#define LUA_USE_DLOPEN
/*
 * iOS也支持动态加载,但受到更多限制(App Store应用不能使用)
 */
#endif

#if defined(LUA_USE_C89) && defined(LUA_USE_POSIX)
/*
 * 兼容性检查:C89和POSIX不兼容
 */
#error "POSIX is not compatible with C89"
/*
 * #error指令说明:
 * - 这是预处理器错误指令
 * - 如果代码执行到这里,编译会立即失败并显示错误消息
 * - POSIX使用了许多C99特性,与C89模式不兼容
 * - 例如:POSIX定义的某些类型和函数需要C99标准
 */
#endif

/*
@@ LUAI_IS32INT is true iff 'int' has (at least) 32 bits.
** (LUAI_IS32INT为真当且仅当'int'有(至少)32位。)
*/
#define LUAI_IS32INT ((UINT_MAX >> 30) >= 3)
/*
 * 这个宏的巧妙判断逻辑:
 * - UINT_MAX是unsigned int的最大值(来自<limits.h>)
 * - 如果int是32位,UINT_MAX = 0xFFFFFFFF(4,294,967,295)
 * - UINT_MAX >> 30 表示右移30位
 * - 如果是32位:0xFFFFFFFF >> 30 = 0b11 = 3
 * - 如果是16位:0xFFFF >> 30 = 0(因为只有16位)
 * - 所以 >= 3 就能判断至少是32位
 *
 * 用途:
 * - 后续代码中用于决定是否可以使用int作为Lua整数类型
 * - 在32位整数配置下,如果系统的int是32位,就可以直接使用
 */

/* }================================================================== */

/*
** {==================================================================
** Configuration for Number types. These options should not be
** (数值类型配置。这些选项不应该)
** set externally, because any other code connected to Lua must
** (外部设置,因为任何连接到Lua的其他代码必须)
** use the same configuration.
** (使用相同的配置。)
** ===================================================================
*/

/*
@@ LUA_INT_TYPE defines the type for Lua integers.
** (LUA_INT_TYPE定义Lua整数的类型。)
@@ LUA_FLOAT_TYPE defines the type for Lua floats.
** (LUA_FLOAT_TYPE定义Lua浮点数的类型。)
** Lua should work fine with any mix of these options supported
** (Lua应该可以很好地使用这些选项的任何组合)
** by your C compiler. The usual configurations are 64-bit integers
** (只要你的C编译器支持。通常的配置是64位整数)
** and 'double' (the default), 32-bit integers and 'float' (for
** (和'double'(默认),32位整数和'float'(对于)
** restricted platforms), and 'long'/'double' (for C compilers not
** (受限平台),以及'long'/'double'(对于不)
** compliant with C99, which may not have support for 'long long').
** (符合C99的C编译器,它们可能不支持'long long')。)
*/

/* predefined options for LUA_INT_TYPE (LUA_INT_TYPE的预定义选项) */
#define LUA_INT_INT 1
/*
 * 选项1: 使用int作为Lua整数类型
 * - 通常是32位(在大多数现代平台上)
 * - 适合内存受限的嵌入式系统
 */
#define LUA_INT_LONG 2
/*
 * 选项2: 使用long作为Lua整数类型
 * - 在32位系统上通常是32位
 * - 在64位Unix/Linux系统上通常是64位
 * - C89兼容的选择
 */
#define LUA_INT_LONGLONG 3
/*
 * 选项3: 使用long long作为Lua整数类型
 * - C99引入的类型,至少64位
 * - 现代Lua的默认选择,提供更大的整数范围
 * - 可以表示更大的整数而不会溢出
 */

/* predefined options for LUA_FLOAT_TYPE (LUA_FLOAT_TYPE的预定义选项) */
#define LUA_FLOAT_FLOAT 1
/*
 * 选项1: 使用float作为Lua浮点数类型
 * - 32位单精度浮点数
 * - 精度约7位十进制数字
 * - 节省内存,适合嵌入式系统
 */
#define LUA_FLOAT_DOUBLE 2
/*
 * 选项2: 使用double作为Lua浮点数类型
 * - 64位双精度浮点数
 * - 精度约15-17位十进制数字
 * - 现代Lua的默认选择,精度和范围更好
 */
#define LUA_FLOAT_LONGDOUBLE 3
/*
 * 选项3: 使用long double作为Lua浮点数类型
 * - 扩展精度浮点数(通常80位或128位,取决于平台)
 * - 最高精度,但不是所有平台都很好支持
 * - 性能可能较慢
 */

/* Default configuration ('long long' and 'double', for 64-bit Lua)
   (默认配置('long long'和'double',用于64位Lua)) */
#define LUA_INT_DEFAULT LUA_INT_LONGLONG
/*
 * 默认整数类型: long long
 * - 提供64位整数,范围约-9223372036854775808到9223372036854775807
 * - 这是现代Lua(5.3+)的标准配置
 * - 5.3之前的版本只有浮点数,没有独立的整数类型
 */
#define LUA_FLOAT_DEFAULT LUA_FLOAT_DOUBLE
/*
 * 默认浮点类型: double
 * - 提供良好的精度和范围
 * - 几乎所有现代平台都很好地支持double
 * - 是精度、性能和兼容性的最佳平衡
 */

/*
@@ LUA_32BITS enables Lua with 32-bit integers and 32-bit floats.
** (LUA_32BITS启用32位整数和32位浮点数的Lua。)
*/
/* #define LUA_32BITS */
/*
 * 32位Lua配置:
 * - 同时使用32位整数(int)和32位浮点数(float)
 * - 适合内存非常受限的嵌入式系统
 * - 精度和范围都会降低,但内存占用减半
 * - 默认不启用,因为现代系统通常有足够的资源
 */

/*
@@ LUA_C89_NUMBERS ensures that Lua uses the largest types available for
** (LUA_C89_NUMBERS确保Lua使用可用的最大类型)
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** (对于C89('long'和'double');Windows总是有'__int64',所以它)
** not need to use this case.
** (不需要使用这种情况。)
*/
#if defined(LUA_USE_C89) && !defined(LUA_USE_WINDOWS)
/*
 * 条件:使用C89模式且不在Windows上
 */
#define LUA_C89_NUMBERS 1
/*
 * C89数值配置:
 * - C89标准没有long long类型
 * - 所以只能使用long和double
 * - long的大小取决于平台(32位或64位)
 */
#else
#define LUA_C89_NUMBERS 0
/*
 * 不使用C89数值配置
 */
#endif

#if defined(LUA_32BITS) /* { */
/*
 * 如果定义了LUA_32BITS,配置32位Lua
 */
/*
** 32-bit integers and 'float'
** (32位整数和'float')
*/
#if LUAI_IS32INT /* use 'int' if big enough (如果足够大,使用'int') */
#define LUA_INT_TYPE LUA_INT_INT
/*
 * 如果系统的int是32位,直接使用int
 */
#else /* otherwise use 'long' (否则使用'long') */
#define LUA_INT_TYPE LUA_INT_LONG
/*
 * 如果int不是32位(例如16位),则使用long
 */
#endif
#define LUA_FLOAT_TYPE LUA_FLOAT_FLOAT
/*
 * 浮点类型使用float(32位)
 */

#elif LUA_C89_NUMBERS /* }{ */
/*
 * 如果使用C89数值配置
 */
/*
** largest types available for C89 ('long' and 'double')
** (C89可用的最大类型('long'和'double'))
*/
#define LUA_INT_TYPE LUA_INT_LONG
/*
 * C89模式下整数使用long
 */
#define LUA_FLOAT_TYPE LUA_FLOAT_DOUBLE
/*
 * C89模式下浮点使用double
 */

#else /* }{ */
/* use defaults (使用默认值) */

#define LUA_INT_TYPE LUA_INT_DEFAULT
/*
 * 默认情况:整数使用long long
 */
#define LUA_FLOAT_TYPE LUA_FLOAT_DEFAULT
/*
 * 默认情况:浮点使用double
 */

#endif /* } */

/* }================================================================== */

/*
** {==================================================================
** Configuration for Paths. (路径配置。)
** ===================================================================
*/

/*
** LUA_PATH_SEP is the character that separates templates in a path.
** (LUA_PATH_SEP是路径中分隔模板的字符。)
** LUA_PATH_MARK is the string that marks the substitution points in a
** (LUA_PATH_MARK是标记替换点的字符串)
** template.
** (在模板中。)
** LUA_EXEC_DIR in a Windows path is replaced by the executable's
** (Windows路径中的LUA_EXEC_DIR被可执行文件的)
** directory.
** (目录替换。)
*/
#define LUA_PATH_SEP ";"
/*
 * 路径分隔符:
 * - 在LUA_PATH中分隔多个搜索路径
 * - 类似于操作系统的PATH环境变量
 * - Windows和Unix都使用分号
 */
#define LUA_PATH_MARK "?"
/*
 * 路径标记:
 * - 在路径模板中标记模块名称的位置
 * - 例如:"./?.lua" 中的?会被替换为模块名
 * - 如果模块名是"mymod",则变为"./mymod.lua"
 */
#define LUA_EXEC_DIR "!"
/*
 * 可执行文件目录标记(仅Windows):
 * - 在Windows路径中,!会被替换为lua.exe所在的目录
 * - 允许相对于Lua可执行文件的位置查找模块
 * - 便于Lua的便携式部署
 */

/*
@@ LUA_PATH_DEFAULT is the default path that Lua uses to look for
** (LUA_PATH_DEFAULT是Lua用于查找的默认路径)
** Lua libraries.
** (Lua库。)
@@ LUA_CPATH_DEFAULT is the default path that Lua uses to look for
** (LUA_CPATH_DEFAULT是Lua用于查找的默认路径)
** C libraries.
** (C库。)
** CHANGE them if your machine has a non-conventional directory
** (如果你的机器有非常规的目录)
** hierarchy or if you want to install your libraries in
** (层次结构或如果你想安装你的库在)
** non-conventional directories.
** (非常规目录,请修改它们。)
*/

#define LUA_VDIR LUA_VERSION_MAJOR "." LUA_VERSION_MINOR
/*
 * 版本目录:
 * - 连接主版本号和次版本号
 * - 例如:Lua 5.4则为"5.4"
 * - 用于构建版本相关的路径
 * - LUA_VERSION_MAJOR和LUA_VERSION_MINOR在lua.h中定义
 */
#if defined(_WIN32) /* { */
/*
 * Windows平台的路径配置
 */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** (在Windows中,路径中的任何感叹号('!')被替换为)
** path of the directory of the executable file of the current process.
** (当前进程的可执行文件的目录路径。)
*/
#define LUA_LDIR "!\\lua\\"
/*
 * Lua库目录:
 * - ! = 可执行文件目录
 * - 完整路径示例:C:\Program Files\Lua\lua\
 * - \\是转义的反斜杠(Windows路径分隔符)
 */
#define LUA_CDIR "!\\"
/*
 * C库目录:
 * - 直接在可执行文件目录下
 */
#define LUA_SHRDIR "!\\..\\share\\lua\\" LUA_VDIR "\\"
/*
 * 共享目录:
 * - ..表示上级目录
 * - 完整路径示例:C:\Program Files\Lua\..\share\lua\5.4\
 * - 即:C:\Program Files\share\lua\5.4\
 */

#if !defined(LUA_PATH_DEFAULT)
/*
 * 如果没有外部定义LUA_PATH_DEFAULT
 */
#define LUA_PATH_DEFAULT                                                                                                              \
	LUA_LDIR "?.lua;" LUA_LDIR "?\\init.lua;" LUA_CDIR "?.lua;" LUA_CDIR "?\\init.lua;" LUA_SHRDIR "?.lua;" LUA_SHRDIR "?\\init.lua;" \
			 ".\\?.lua;"                                                                                                              \
			 ".\\?\\init.lua"
/*
 * Lua库搜索路径(Windows):
 * 按顺序搜索以下位置(假设require "mymod"):
 * 1. !\\lua\\mymod.lua
 * 2. !\\lua\\mymod\\init.lua
 * 3. !\\mymod.lua
 * 4. !\\mymod\\init.lua
 * 5. !\\..\\share\\lua\\5.4\\mymod.lua
 * 6. !\\..\\share\\lua\\5.4\\mymod\\init.lua
 * 7. .\\mymod.lua (当前目录)
 * 8. .\\mymod\\init.lua
 *
 * 解释:
 * - 反斜杠\是续行符,表示定义跨多行
 * - 分号;分隔不同的路径模板
 * - ?会被模块名替换
 * - init.lua是包(package)的约定入口文件
 */
#endif

#if !defined(LUA_CPATH_DEFAULT)
#define LUA_CPATH_DEFAULT                                                                   \
	LUA_CDIR "?.dll;" LUA_CDIR "..\\lib\\lua\\" LUA_VDIR "\\?.dll;" LUA_CDIR "loadall.dll;" \
			 ".\\?.dll"
/*
 * C库搜索路径(Windows):
 * 按顺序搜索以下位置(假设require "mymod"):
 * 1. !\\mymod.dll
 * 2. !\\..\\lib\\lua\\5.4\\mymod.dll
 * 3. !\\loadall.dll (特殊:加载所有符号)
 * 4. .\\mymod.dll (当前目录)
 *
 * 说明:
 * - .dll是Windows的动态链接库扩展名
 * - loadall.dll是一个特殊机制,可以包含多个模块
 */
#endif

#else /* }{ */
/*
 * Unix/Linux/macOS平台的路径配置
 */

#define LUA_ROOT "/usr/local/"
/*
 * Unix根目录:
 * - 标准Unix软件安装位置
 * - 系统级安装通常在/usr/local
 */
#define LUA_LDIR LUA_ROOT "share/lua/" LUA_VDIR "/"
/*
 * Lua库目录:
 * - 例如:/usr/local/share/lua/5.4/
 * - share目录存放架构无关的文件
 */
#define LUA_CDIR LUA_ROOT "lib/lua/" LUA_VDIR "/"
/*
 * C库目录:
 * - 例如:/usr/local/lib/lua/5.4/
 * - lib目录存放二进制库文件
 */

#if !defined(LUA_PATH_DEFAULT)
#define LUA_PATH_DEFAULT                                                              \
	LUA_LDIR "?.lua;" LUA_LDIR "?/init.lua;" LUA_CDIR "?.lua;" LUA_CDIR "?/init.lua;" \
			 "./?.lua;"                                                               \
			 "./?/init.lua"
/*
 * Lua库搜索路径(Unix):
 * 按顺序搜索以下位置(假设require "mymod"):
 * 1. /usr/local/share/lua/5.4/mymod.lua
 * 2. /usr/local/share/lua/5.4/mymod/init.lua
 * 3. /usr/local/lib/lua/5.4/mymod.lua
 * 4. /usr/local/lib/lua/5.4/mymod/init.lua
 * 5. ./mymod.lua
 * 6. ./mymod/init.lua
 *
 * 说明:
 * - Unix使用正斜杠/作为路径分隔符
 * - 相比Windows路径,结构更清晰简洁
 */
#endif

#if !defined(LUA_CPATH_DEFAULT)
#define LUA_CPATH_DEFAULT                   \
	LUA_CDIR "?.so;" LUA_CDIR "loadall.so;" \
			 "./?.so"
/*
 * C库搜索路径(Unix):
 * 按顺序搜索以下位置(假设require "mymod"):
 * 1. /usr/local/lib/lua/5.4/mymod.so
 * 2. /usr/local/lib/lua/5.4/loadall.so
 * 3. ./mymod.so
 *
 * 说明:
 * - .so是Unix/Linux的共享对象(共享库)扩展名
 * - macOS使用.dylib或.so都可以
 */
#endif

#endif /* } */

/*
@@ LUA_DIRSEP is the directory separator (for submodules).
** (LUA_DIRSEP是目录分隔符(用于子模块)。)
** CHANGE it if your machine does not use "/" as the directory separator
** (如果你的机器不使用"/"作为目录分隔符)
** and is not Windows. (On Windows Lua automatically uses "\".)
** (并且不是Windows,请修改它。(在Windows上Lua自动使用"\"。))
*/
#if !defined(LUA_DIRSEP)

#if defined(_WIN32)
#define LUA_DIRSEP "\\"
/*
 * Windows目录分隔符:反斜杠
 */
#else
#define LUA_DIRSEP "/"
/*
 * Unix/Linux/macOS目录分隔符:正斜杠
 */
#endif

#endif
/*
 * LUA_DIRSEP用于处理子模块:
 * - 例如:require "a.b.c"
 * - 在Unix上会查找文件:a/b/c.lua
 * - 在Windows上会查找文件:a\b\c.lua
 * - 点号.在模块名中被转换为目录分隔符
 */

/*
** LUA_IGMARK is a mark to ignore all after it when building the
** (LUA_IGMARK是一个标记,在构建时忽略其后的所有内容)
** module name (e.g., used to build the luaopen_ function name).
** (模块名称(例如,用于构建luaopen_函数名称)。)
** Typically, the suffix after the mark is the module version,
** (通常,标记后的后缀是模块版本,)
** as in "mod-v1.2.so".
** (如"mod-v1.2.so"。)
*/
#define LUA_IGMARK "-"
/*
 * 忽略标记:
 * - 用于从文件名中提取模块名
 * - 例如:加载"mymod-v1.2.so"时
 * - 模块名被识别为"mymod"(-v1.2被忽略)
 * - 对应的初始化函数名为luaopen_mymod
 * - 这允许同一模块有多个版本的库文件
 */

/* }================================================================== */

/*
** {==================================================================
** Marks for exported symbols in the C code
** (C代码中导出符号的标记)
** ===================================================================
*/

/*
@@ LUA_API is a mark for all core API functions.
** (LUA_API是所有核心API函数的标记。)
@@ LUALIB_API is a mark for all auxiliary library functions.
** (LUALIB_API是所有辅助库函数的标记。)
@@ LUAMOD_API is a mark for all standard library opening functions.
** (LUAMOD_API是所有标准库打开函数的标记。)
** CHANGE them if you need to define those functions in some special way.
** (如果你需要以某种特殊方式定义这些函数,请修改它们。)
** For instance, if you want to create one Windows DLL with the core and
** (例如,如果你想创建一个包含核心和)
** the libraries, you may want to use the following definition (define
** (库的Windows DLL,你可能想使用以下定义(定义)
** LUA_BUILD_AS_DLL to get it).
** (LUA_BUILD_AS_DLL来获得它)。)
*/
#if defined(LUA_BUILD_AS_DLL) /* { */
/*
 * 如果定义了LUA_BUILD_AS_DLL,表示将Lua编译为Windows DLL
 */

#if defined(LUA_CORE) || defined(LUA_LIB) /* { */
#define LUA_API __declspec(dllexport)
/*
 * __declspec(dllexport)说明:
 * - 这是Microsoft Visual C++的扩展语法
 * - 用于从DLL中导出函数或变量
 * - 当编译Lua核心或库时(定义了LUA_CORE或LUA_LIB)
 * - 函数被标记为导出,可以被其他模块使用
 * - 相当于告诉链接器:"这个符号要放到DLL的导出表中"
 */
#else /* }{ */
#define LUA_API __declspec(dllimport)
/*
 * __declspec(dllimport)说明:
 * - 用于从DLL中导入函数或变量
 * - 当使用Lua DLL时(没有定义LUA_CORE或LUA_LIB)
 * - 函数被标记为导入,链接到DLL中的实现
 * - 这样编译器生成更高效的代码
 */
#endif /* } */

#else /* }{ */

#define LUA_API extern
/*
 * extern关键字说明:
 * - C语言标准的外部链接声明
 * - 表示函数或变量在其他编译单元中定义
 * - 默认情况(非DLL构建)下使用
 * - 适用于静态链接的情况
 */

#endif /* } */

/*
** More often than not the libs go together with the core.
** (通常情况下库和核心一起。)
*/
#define LUALIB_API LUA_API
/*
 * 辅助库API使用与核心API相同的定义
 * - 辅助库(lauxlib)提供便利函数
 * - 通常与核心一起编译和链接
 */

#if defined(__cplusplus)
/* Lua uses the "C name" when calling open functions
   (Lua在调用打开函数时使用"C名称") */
#define LUAMOD_API extern "C"
/*
 * extern "C"说明:
 * - 用于C++环境
 * - 告诉C++编译器使用C链接约定,不进行名称修饰(name mangling)
 * - C++编译器会修改函数名以支持重载,如foo()变成_Z3foov
 * - extern "C"保持函数名为foo,这样C代码可以找到它
 * - Lua的模块加载需要找到luaopen_xxx函数,必须使用C名称
 */
#else
#define LUAMOD_API LUA_API
/*
 * 在C环境下,使用标准的LUA_API
 */
#endif

/* }================================================================== */

/*
** {==================================================================
** Compatibility with previous versions
** (与以前版本的兼容性)
** ===================================================================
*/

/*
@@ LUA_COMPAT_GLOBAL avoids 'global' being a reserved word
** (LUA_COMPAT_GLOBAL避免'global'成为保留字)
*/
#define LUA_COMPAT_GLOBAL
/*
 * 兼容性选项:
 * - 在某些旧版本中,'global'可能被用作标识符
 * - 定义此宏可以避免将其视为保留字
 * - 保持向后兼容性
 */

/*
@@ LUA_COMPAT_MATHLIB controls the presence of several deprecated
** (LUA_COMPAT_MATHLIB控制几个已弃用的存在)
** functions in the mathematical library.
** (数学库中的函数。)
** (These functions were already officially removed in 5.3;
** (这些函数已在5.3中正式删除;)
** nevertheless they are still available here.)
** (然而它们在这里仍然可用。))
*/
/* #define LUA_COMPAT_MATHLIB */
/*
 * 数学库兼容性:
 * - 默认不启用(被注释掉)
 * - 如果启用,会包含一些Lua 5.3中删除的数学函数
 * - 例如:math.log10, math.pow等
 * - 这些函数已有替代方案(如math.log(x, 10))
 */

/*
@@ The following macros supply trivial compatibility for some
** (以下宏为一些提供简单的兼容性)
** changes in the API. The macros themselves document how to
** (API的更改。宏本身记录了如何)
** change your code to avoid using them.
** (修改你的代码以避免使用它们。)
** (Once more, these macros were officially removed in 5.3, but they are
** (再次说明,这些宏在5.3中已正式删除,但它们)
** still available here.)
** (在这里仍然可用。))
*/
#define lua_strlen(L, i) lua_rawlen(L, (i))
/*
 * 兼容宏:字符串长度
 * - lua_strlen在Lua 5.2中被lua_rawlen替代
 * - lua_rawlen返回字符串、表或用户数据的原始长度
 * - 不会触发__len元方法
 * - 参数:(L)Lua状态机,(i)栈索引
 */

#define lua_objlen(L, i) lua_rawlen(L, (i))
/*
 * 兼容宏:对象长度
 * - lua_objlen也被lua_rawlen替代
 * - 功能相同,只是名称更改
 */

#define lua_equal(L, idx1, idx2) lua_compare(L, (idx1), (idx2), LUA_OPEQ)
/*
 * 兼容宏:相等比较
 * - lua_equal在Lua 5.2中被lua_compare替代
 * - lua_compare是更通用的比较函数
 * - LUA_OPEQ是相等操作符常量
 * - 等价于Lua中的 ==
 */
#define lua_lessthan(L, idx1, idx2) lua_compare(L, (idx1), (idx2), LUA_OPLT)
/*
 * 兼容宏:小于比较
 * - lua_lessthan也被lua_compare替代
 * - LUA_OPLT是小于操作符常量
 * - 等价于Lua中的 <
 */

/* }================================================================== */

/*
** {==================================================================
** Configuration for Numbers (low-level part).
** (数值配置(底层部分)。)
** Change these definitions if no predefined LUA_FLOAT_* / LUA_INT_*
** (如果没有预定义的LUA_FLOAT_* / LUA_INT_*)
** satisfy your needs.
** (满足你的需求,请修改这些定义。)
** ===================================================================
*/

/*
@@ LUAI_UACNUMBER is the result of a 'default argument promotion'
** (LUAI_UACNUMBER是'默认参数提升'的结果)
@@ over a floating number.
** (对浮点数。)
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** (l_floatatt(x)将浮点属性'x'纠正为正确的浮点类型)
** by prefixing it with one of FLT/DBL/LDBL.
** (通过为其添加FLT/DBL/LDBL之一的前缀。)
@@ LUA_NUMBER_FRMLEN is the length modifier for writing floats.
** (LUA_NUMBER_FRMLEN是写入浮点数的长度修饰符。)
@@ LUA_NUMBER_FMT is the format for writing floats with the maximum
** (LUA_NUMBER_FMT是写入浮点数的格式,使用最大)
** number of digits that respects tostring(tonumber(numeral)) == numeral.
** (尊重tostring(tonumber(numeral)) == numeral的数字位数。)
** (That would be floor(log10(2^n)), where n is the number of bits in
** (那将是floor(log10(2^n)),其中n是位数)
** the float mantissa.)
** (浮点尾数中。))
@@ LUA_NUMBER_FMT_N is the format for writing floats with the minimum
** (LUA_NUMBER_FMT_N是写入浮点数的格式,使用最小)
** number of digits that ensures tonumber(tostring(number)) == number.
** (确保tonumber(tostring(number)) == number的数字位数。)
** (That would be LUA_NUMBER_FMT+2.)
** (那将是LUA_NUMBER_FMT+2。)
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
** (l_mathop允许为所有数学运算添加'l'或'f'。)
@@ l_floor takes the floor of a float.
** (l_floor取浮点数的floor。)
@@ lua_str2number converts a decimal numeral to a number.
** (lua_str2number将十进制数字转换为数字。)
*/

/* The following definition is good for most cases here
   (以下定义对这里的大多数情况都适用) */

#define l_floor(x) (l_mathop(floor)(x))
/*
 * floor函数:
 * - 数学函数,返回不大于参数的最大整数
 * - 例如:floor(3.7) = 3, floor(-2.3) = -3
 * - l_mathop会根据浮点类型添加适当的后缀
 * - float用floorf, double用floor, long double用floorl
 */

/* now the variable definitions (现在是变量定义) */

#if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT /* { single float (单精度浮点) */

#define LUA_NUMBER float
/*
 * Lua数值类型定义为float
 */

#define l_floatatt(n) (FLT_##n)
/*
 * 浮点属性宏:
 * - ##是预处理器的连接操作符
 * - 例如:l_floatatt(MAX) 展开为 FLT_MAX
 * - FLT_MAX, FLT_MIN等定义在<float.h>中
 * - 这些是float类型的限制值
 */

#define LUAI_UACNUMBER double
/*
 * 默认参数提升:
 * - C语言中,float参数会自动提升为double
 * - 这是C的"默认参数提升"规则
 * - 在可变参数函数(如printf)中尤其重要
 */

#define LUA_NUMBER_FRMLEN ""
/*
 * 格式长度修饰符:
 * - float使用标准格式,无需修饰符
 * - 在printf格式字符串中使用,如"%f"
 */
#define LUA_NUMBER_FMT "%.7g"
/*
 * 浮点数格式化字符串:
 * - %.7g: 7位有效数字的通用格式
 * - g格式自动选择%e或%f,去除尾随零
 * - 7位足以保证float的精度往返转换
 * - 即:tostring(tonumber(s)) == s
 */
#define LUA_NUMBER_FMT_N "%.9g"
/*
 * 扩展精度格式:
 * - 9位有效数字,保证tonumber(tostring(n)) == n
 * - 多2位确保任何float都能精确表示
 */

#define l_mathop(op) op##f
/*
 * 数学运算宏:
 * - 为数学函数添加f后缀
 * - 例如:l_mathop(sin) 展开为 sinf
 * - sinf是float版本的sin函数
 * - 使用float版本可以提高性能,减少类型转换
 */

#define lua_str2number(s, p) strtof((s), (p))
/*
 * 字符串转数值:
 * - strtof: C99标准库函数,将字符串转换为float
 * - (s): 要转换的字符串
 * - (p): 指针,用于存储转换结束位置
 * - 返回转换后的float值
 */

#elif LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE /* }{ long double (长双精度浮点) */

#define LUA_NUMBER long double
/*
 * Lua数值类型定义为long double
 * - 扩展精度浮点数
 * - 通常是80位(Intel)或128位(某些RISC架构)
 * - 提供最高精度,但性能可能较慢
 */

#define l_floatatt(n) (LDBL_##n)
/*
 * 使用long double的限制值
 * - LDBL_MAX, LDBL_MIN等
 */

#define LUAI_UACNUMBER long double
/*
 * long double不会自动提升
 */

#define LUA_NUMBER_FRMLEN "L"
/*
 * 格式长度修饰符:
 * - long double使用L修饰符
 * - 在printf中使用,如"%Lf"
 */
#define LUA_NUMBER_FMT "%.19Lg"
/*
 * 19位有效数字:
 * - 足以表示long double的精度
 * - 具体位数取决于long double的实现
 */
#define LUA_NUMBER_FMT_N "%.21Lg"
/*
 * 21位有效数字:
 * - 保证往返转换的精度
 */

#define l_mathop(op) op##l
/*
 * 数学运算宏:
 * - 添加l后缀
 * - 例如:sinl, cosl等
 */

#define lua_str2number(s, p) strtold((s), (p))
/*
 * 字符串转long double:
 * - strtold: C99标准库函数
 */

#elif LUA_FLOAT_TYPE == LUA_FLOAT_DOUBLE /* }{ double (双精度浮点) */

#define LUA_NUMBER double
/*
 * Lua数值类型定义为double
 * - 这是默认和最常用的配置
 * - 64位双精度浮点数
 * - 良好的精度和性能平衡
 */

#define l_floatatt(n) (DBL_##n)
/*
 * 使用double的限制值
 * - DBL_MAX, DBL_MIN等
 */

#define LUAI_UACNUMBER double
/*
 * double在参数传递中保持不变
 */

#define LUA_NUMBER_FRMLEN ""
/*
 * double是标准格式,无需修饰符
 */
#define LUA_NUMBER_FMT "%.15g"
/*
 * 15位有效数字:
 * - double的标准精度
 * - 足以表示大多数double值
 */
#define LUA_NUMBER_FMT_N "%.17g"
/*
 * 17位有效数字:
 * - 保证double的完整精度往返转换
 * - IEEE 754双精度有约15-17位十进制精度
 */

#define l_mathop(op) op
/*
 * 数学运算宏:
 * - double是标准版本,无需后缀
 * - 直接使用sin, cos等
 */

#define lua_str2number(s, p) strtod((s), (p))
/*
 * 字符串转double:
 * - strtod: C89标准库函数
 * - 所有C编译器都支持
 */

#else /* }{ */

#error "numeric float type not defined"
/*
 * 错误检查:
 * - 如果LUA_FLOAT_TYPE不是上述三个值之一
 * - 编译会失败并显示此错误消息
 * - 确保必须选择一个有效的浮点类型
 */

#endif /* } */

/*
@@ LUA_UNSIGNED is the unsigned version of LUA_INTEGER.
** (LUA_UNSIGNED是LUA_INTEGER的无符号版本。)
@@ LUAI_UACINT is the result of a 'default argument promotion'
** (LUAI_UACINT是'默认参数提升'的结果)
@@ over a LUA_INTEGER.
** (对LUA_INTEGER。)
@@ LUA_INTEGER_FRMLEN is the length modifier for reading/writing integers.
** (LUA_INTEGER_FRMLEN是读/写整数的长度修饰符。)
@@ LUA_INTEGER_FMT is the format for writing integers.
** (LUA_INTEGER_FMT是写入整数的格式。)
@@ LUA_MAXINTEGER is the maximum value for a LUA_INTEGER.
** (LUA_MAXINTEGER是LUA_INTEGER的最大值。)
@@ LUA_MININTEGER is the minimum value for a LUA_INTEGER.
** (LUA_MININTEGER是LUA_INTEGER的最小值。)
@@ LUA_MAXUNSIGNED is the maximum value for a LUA_UNSIGNED.
** (LUA_MAXUNSIGNED是LUA_UNSIGNED的最大值。)
@@ lua_integer2str converts an integer to a string.
** (lua_integer2str将整数转换为字符串。)
*/

/* The following definitions are good for most cases here
   (以下定义对这里的大多数情况都适用) */

#define LUA_INTEGER_FMT "%" LUA_INTEGER_FRMLEN "d"
/*
 * 整数格式字符串构造:
 * - 使用字符串连接组合格式
 * - 例如:如果FRMLEN是"ll",则结果是"%lld"
 * - %d是有符号十进制整数格式
 */

#define LUAI_UACINT LUA_INTEGER
/*
 * 整数的默认参数提升:
 * - 现代平台上,整数通常不会提升
 * - 除非是小于int的类型(如short)
 */

#define lua_integer2str(s, sz, n) \
	l_sprintf((s), sz, LUA_INTEGER_FMT, (LUAI_UACINT)(n))
/*
 * 整数转字符串宏:
 * - (s): 目标缓冲区
 * - sz: 缓冲区大小
 * - (n): 要转换的整数
 * - (LUAI_UACINT)(n): 类型转换,应对参数提升
 * - 使用sprintf或snprintf格式化整数
 */

/*
** use LUAI_UACINT here to avoid problems with promotions (which
** (在这里使用LUAI_UACINT以避免提升问题(这)
** can turn a comparison between unsigneds into a signed comparison)
** (可能将无符号数之间的比较变成有符号比较))
*/
#define LUA_UNSIGNED unsigned LUAI_UACINT
/*
 * 无符号整数类型:
 * - 使用LUAI_UACINT的无符号版本
 * - 避免类型提升导致的比较问题
 * - 例如:如果LUAI_UACINT是long long,
 *   则LUA_UNSIGNED是unsigned long long
 */

/* now the variable definitions (现在是变量定义) */

#if LUA_INT_TYPE == LUA_INT_INT /* { int */

#define LUA_INTEGER int
/*
 * Lua整数类型定义为int
 * - 通常是32位
 * - 范围约-21亿到+21亿
 */
#define LUA_INTEGER_FRMLEN ""
/*
 * int的格式修饰符为空
 * - 直接使用%d
 */

#define LUA_MAXINTEGER INT_MAX
/*
 * int的最大值
 * - 来自<limits.h>
 * - 通常是2147483647(2^31 - 1)
 */
#define LUA_MININTEGER INT_MIN
/*
 * int的最小值
 * - 通常是-2147483648(-2^31)
 */

#define LUA_MAXUNSIGNED UINT_MAX
/*
 * unsigned int的最大值
 * - 通常是4294967295(2^32 - 1)
 */

#elif LUA_INT_TYPE == LUA_INT_LONG /* }{ long */

#define LUA_INTEGER long
/*
 * Lua整数类型定义为long
 * - 32位系统上通常是32位
 * - 64位Unix/Linux系统上通常是64位
 * - Windows上即使64位系统也是32位
 */
#define LUA_INTEGER_FRMLEN "l"
/*
 * long的格式修饰符
 * - 使用%ld
 */

#define LUA_MAXINTEGER LONG_MAX
/*
 * long的最大值
 * - 取决于平台
 */
#define LUA_MININTEGER LONG_MIN
/*
 * long的最小值
 */

#define LUA_MAXUNSIGNED ULONG_MAX
/*
 * unsigned long的最大值
 */

#elif LUA_INT_TYPE == LUA_INT_LONGLONG /* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance
   (使用宏LLONG_MAX的存在作为C99兼容性的代理) */
#if defined(LLONG_MAX)				   /* { */
/* use ISO C99 stuff (使用ISO C99特性) */

#define LUA_INTEGER long long
/*
 * Lua整数类型定义为long long
 * - C99引入的类型
 * - 至少64位
 * - 范围约-9223372036854775808到9223372036854775807
 * - 这是现代Lua的默认整数类型
 */
#define LUA_INTEGER_FRMLEN "ll"
/*
 * long long的格式修饰符
 * - 使用%lld
 */

#define LUA_MAXINTEGER LLONG_MAX
/*
 * long long的最大值
 * - 通常是9223372036854775807(2^63 - 1)
 */
#define LUA_MININTEGER LLONG_MIN
/*
 * long long的最小值
 * - 通常是-9223372036854775808(-2^63)
 */

#define LUA_MAXUNSIGNED ULLONG_MAX
/*
 * unsigned long long的最大值
 * - 通常是18446744073709551615(2^64 - 1)
 */

#elif defined(LUA_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types
   (在Windows中,可以使用特定的Windows类型) */

#define LUA_INTEGER __int64
/*
 * Windows特定的64位整数类型
 * - __int64是Microsoft的扩展
 * - 在C99之前的Windows编译器上可用
 * - 等价于long long
 */
#define LUA_INTEGER_FRMLEN "I64"
/*
 * Windows的64位整数格式修饰符
 * - 使用%I64d
 * - 这是Microsoft的扩展格式
 */

#define LUA_MAXINTEGER _I64_MAX
/*
 * __int64的最大值
 * - Windows定义的常量
 */
#define LUA_MININTEGER _I64_MIN
/*
 * __int64的最小值
 */

#define LUA_MAXUNSIGNED _UI64_MAX
/*
 * unsigned __int64的最大值
 */

#else /* }{ */

#error "Compiler does not support 'long long'. Use option '-DLUA_32BITS' \
  or '-DLUA_C89_NUMBERS' (see file 'luaconf.h' for details)"
/*
 * 错误消息:
 * - 如果编译器既不支持C99的long long
 * - 也不是Windows环境
 * - 则无法使用64位整数
 * - 提示用户使用32位配置或C89数值配置
 */

#endif /* } */

#else /* }{ */

#error "numeric integer type not defined"
/*
 * 错误检查:
 * - 确保LUA_INT_TYPE被正确设置
 */

#endif /* } */

/* }================================================================== */

/*
** {==================================================================
** Dependencies with C99 and other C details
** (与C99和其他C细节的依赖关系)
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (l_sprintf等价于C89中的'snprintf'或'sprintf'。)
** (All uses in Lua have only one format item.)
** (Lua中的所有使用都只有一个格式项。)
*/
#if !defined(LUA_USE_C89)
#define l_sprintf(s, sz, f, i) snprintf(s, sz, f, i)
/*
 * snprintf函数(C99):
 * - 安全的格式化字符串函数
 * - (s): 目标缓冲区
 * - sz: 缓冲区大小,防止缓冲区溢出
 * - (f): 格式字符串
 * - (i): 要格式化的值
 * - 返回写入的字符数(不包括'\0')
 * - 如果缓冲区太小,会截断但保证以'\0'结尾
 */
#else
#define l_sprintf(s, sz, f, i) ((void)(sz), sprintf(s, f, i))
/*
 * sprintf函数(C89):
 * - 不安全的格式化字符串函数
 * - 不检查缓冲区大小,可能导致溢出
 * - (void)(sz): 忽略sz参数,避免未使用参数警告
 * - C89模式下只能使用这个版本
 * - Lua确保所有使用都是安全的(缓冲区足够大)
 */
#endif

/*
@@ lua_strx2number converts a hexadecimal numeral to a number.
** (lua_strx2number将十六进制数字转换为数字。)
** In C99, 'strtod' does that conversion. Otherwise, you can
** (在C99中,'strtod'执行该转换。否则,你可以)
** leave 'lua_strx2number' undefined and Lua will provide its own
** (不定义'lua_strx2number',Lua将提供其自己的)
** implementation.
** (实现。)
*/
#if !defined(LUA_USE_C89)
#define lua_strx2number(s, p) lua_str2number(s, p)
/*
 * 十六进制字符串转数值:
 * - C99的strtod支持十六进制格式(如"0x1.8p+3")
 * - 可以直接使用lua_str2number(即strtod)
 * - 十六进制浮点格式:0x[整数部分].[小数部分]p[指数]
 * - 例如:0x1.8p+3 = 1.5 * 2^3 = 12.0
 */
#endif
/*
 * C89模式下:
 * - 不定义此宏
 * - Lua会在其他地方提供自己的实现
 * - 因为C89的strtod不支持十六进制
 */

/*
@@ lua_pointer2str converts a pointer to a readable string in a
** (lua_pointer2str将指针转换为可读字符串)
** non-specified way.
** (以非指定的方式。)
*/
#define lua_pointer2str(buff, sz, p) l_sprintf(buff, sz, "%p", p)
/*
 * 指针转字符串:
 * - %p是printf的指针格式
 * - 输出格式取决于实现(通常是十六进制地址)
 * - 例如:"0x7fff5fbff8a0"
 * - 用于调试和错误消息
 * - buff: 缓冲区
 * - sz: 缓冲区大小
 * - p: 要转换的指针
 */

/*
@@ lua_number2strx converts a float to a hexadecimal numeral.
** (lua_number2strx将浮点数转换为十六进制数字。)
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** (在C99中,'sprintf'(使用格式说明符'%a'/'%A')执行此操作。)
** Otherwise, you can leave 'lua_number2strx' undefined and Lua will
** (否则,你可以不定义'lua_number2strx',Lua将)
** provide its own implementation.
** (提供其自己的实现。)
*/
#if !defined(LUA_USE_C89)
#define lua_number2strx(L, b, sz, f, n) \
	((void)L, l_sprintf(b, sz, f, (LUAI_UACNUMBER)(n)))
/*
 * 数值转十六进制字符串:
 * - C99的%a格式输出十六进制浮点表示
 * - L: Lua状态(这里未使用,用(void)L忽略)
 * - b: 缓冲区
 * - sz: 缓冲区大小
 * - f: 格式字符串(通常是"%a"或"%A")
 * - (n): 要转换的数值
 * - (LUAI_UACNUMBER)(n): 类型转换
 * - 十六进制浮点便于精确表示浮点数
 */
#endif
/*
 * C89模式下:
 * - 不定义此宏
 * - Lua提供自己的实现
 */

/*
** 'strtof' and 'opf' variants for math functions are not valid in
** ('strtof'和数学函数的'opf'变体在)
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** (C89中无效。否则,宏'HUGE_VALF'是测试)
** availability of these variants. ('math.h' is already included in
** (这些变体可用性的良好代理。('math.h'已包含在)
** all files that use these macros.)
** (所有使用这些宏的文件中。))
*/
#if defined(LUA_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
/*
 * 条件:C89模式 或 有HUGE_VAL但没有HUGE_VALF
 * - HUGE_VAL是double的无穷大表示
 * - HUGE_VALF是float的无穷大表示(C99)
 * - 如果没有HUGE_VALF,说明不支持float变体函数
 */
#undef l_mathop /* variants not available (变体不可用) */
#undef lua_str2number
#define l_mathop(op) (lua_Number) op /* no variant (无变体) */
/*
 * 数学运算回退:
 * - 不使用float或long double变体
 * - 使用标准double版本,然后转换为lua_Number
 * - 例如:如果lua_Number是float,
 *   l_mathop(sin)展开为(float)sin
 *   实际调用double版本的sin,然后转换为float
 */
#define lua_str2number(s, p) ((lua_Number)strtod((s), (p)))
/*
 * 字符串转数值回退:
 * - 只使用strtod(C89标准)
 * - 然后转换为lua_Number
 */
#endif

/*
@@ LUA_KCONTEXT is the type of the context ('ctx') for continuation
** (LUA_KCONTEXT是延续的上下文('ctx')的类型)
** functions.  It must be a numerical type; Lua will use 'intptr_t' if
** (函数。它必须是数值类型;如果)
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** (可用,Lua将使用'intptr_t',否则它将使用'ptrdiff_t'(最接近)
** 'intptr_t' in C89)
** (C89中的'intptr_t'))
*/
#define LUA_KCONTEXT ptrdiff_t
/*
 * 延续上下文类型默认值:
 * - ptrdiff_t: 两个指针相减的结果类型(<stddef.h>)
 * - 足够大以存储指针差值
 * - C89标准的安全选择
 */

#if !defined(LUA_USE_C89) && defined(__STDC_VERSION__) && \
	__STDC_VERSION__ >= 199901L
/*
 * 条件检查C99支持:
 * - !defined(LUA_USE_C89): 不是C89模式
 * - defined(__STDC_VERSION__): 编译器定义了标准版本宏
 * - __STDC_VERSION__ >= 199901L: 版本号>=1999年1月(C99)
 */
#include <stdint.h>
/*
 * <stdint.h>是C99头文件:
 * - 定义了固定宽度整数类型
 * - int8_t, int16_t, int32_t, int64_t等
 * - intptr_t: 能够容纳指针值的整数类型
 */
#if defined(INTPTR_MAX) /* even in C99 this type is optional (即使在C99中这个类型也是可选的) */
#undef LUA_KCONTEXT
#define LUA_KCONTEXT intptr_t
/*
 * intptr_t类型:
 * - 专门设计用于存储指针值的整数类型
 * - 可以在指针和整数之间安全转换
 * - (void*)p == (void*)(intptr_t)p 总是成立
 * - 比ptrdiff_t更适合存储指针相关的上下文
 * - 即使在C99中也是可选的,所以检查INTPTR_MAX
 */
#endif
#endif

/*
@@ lua_getlocaledecpoint gets the locale "radix character" (decimal point).
** (lua_getlocaledecpoint获取区域设置的"基数字符"(小数点)。)
** Change that if you do not want to use C locales. (Code using this
** (如果你不想使用C区域设置,请修改它。(使用此)
** macro must include the header 'locale.h'.)
** (宏的代码必须包含头文件'locale.h'。))
*/
#if !defined(lua_getlocaledecpoint)
#define lua_getlocaledecpoint() (localeconv()->decimal_point[0])
/*
 * 获取本地化小数点:
 * - localeconv(): C标准库函数,返回本地化信息结构
 * - decimal_point: 小数点字符串(通常是"."或",")
 * - [0]: 取第一个字符
 * - 不同国家使用不同的小数点(如德国用逗号)
 * - 这确保Lua正确解析本地化的数字格式
 * - 需要#include <locale.h>
 */
#endif

/*
** macros to improve jump prediction, used mostly for error handling
** (宏用于改进跳转预测,主要用于错误处理)
** and debug facilities. (Some macros in the Lua API use these macros.
** (和调试功能。(Lua API中的某些宏使用这些宏。)
** Define LUA_NOBUILTIN if you do not want '__builtin_expect' in your
** (如果你不想在你的代码中使用'__builtin_expect',请定义LUA_NOBUILTIN)
** code.)
** (代码。))
*/
#if !defined(luai_likely)

#if defined(__GNUC__) && !defined(LUA_NOBUILTIN)
/*
 * GCC编译器优化:
 * - __GNUC__: GCC编译器预定义的宏
 */
#define luai_likely(x) (__builtin_expect(((x) != 0), 1))
#define luai_unlikely(x) (__builtin_expect(((x) != 0), 0))
/*
 * __builtin_expect说明:
 * - GCC内建函数,提供分支预测提示
 * - __builtin_expect(expr, expected_value)
 * - 告诉编译器expr的期望值
 * - luai_likely(x): 期望x为真(非零)
 * - luai_unlikely(x): 期望x为假(零)
 *
 * 工作原理:
 * - 现代CPU使用分支预测提高性能
 * - 预测正确时,流水线不会停顿
 * - 预测错误时,需要清空流水线,性能损失
 * - 编译器根据提示优化代码布局
 * - 常见路径放在一起,减少跳转
 *
 * 使用场景:
 * - luai_likely用于正常情况(如成功路径)
 * - luai_unlikely用于错误处理(如if (error))
 * - 例如:if (luai_unlikely(ptr == NULL)) { handle_error(); }
 */
#else
#define luai_likely(x) (x)
#define luai_unlikely(x) (x)
/*
 * 回退定义:
 * - 在不支持__builtin_expect的编译器上
 * - 宏简单地返回参数本身
 * - 不影响功能,只是没有优化提示
 */
#endif

#endif

/* }================================================================== */

/*
** {==================================================================
** Language Variations (语言变化)
** =====================================================================
*/

/*
@@ LUA_NOCVTN2S/LUA_NOCVTS2N control how Lua performs some
** (LUA_NOCVTN2S/LUA_NOCVTS2N控制Lua如何执行某些)
** coercions. Define LUA_NOCVTN2S to turn off automatic coercion from
** (强制转换。定义LUA_NOCVTN2S以关闭自动强制转换从)
** numbers to strings. Define LUA_NOCVTS2N to turn off automatic
** (数字到字符串。定义LUA_NOCVTS2N以关闭自动)
** coercion from strings to numbers.
** (强制转换从字符串到数字。)
*/
/* #define LUA_NOCVTN2S */
/* #define LUA_NOCVTS2N */
/*
 * 类型转换控制(默认不启用):
 * - LUA_NOCVTN2S: 禁止数字到字符串的自动转换
 *   例如:print(123)会报错,必须print(tostring(123))
 * - LUA_NOCVTS2N: 禁止字符串到数字的自动转换
 *   例如:"10" + 20会报错,必须tonumber("10") + 20
 *
 * 为何提供这些选项:
 * - 自动转换有时会隐藏错误
 * - 严格模式可以捕获更多bug
 * - 但默认不启用,保持Lua的便利性
 */

/*
@@ LUA_USE_APICHECK turns on several consistency checks on the C API.
** (LUA_USE_APICHECK打开C API上的几个一致性检查。)
** Define it as a help when debugging C code.
** (在调试C代码时定义它作为帮助。)
*/
/* #define LUA_USE_APICHECK */
/*
 * API检查(默认不启用):
 * - 启用后会在C API调用时进行额外的检查
 * - 检查栈索引是否有效
 * - 检查参数类型是否正确
 * - 检查栈空间是否足够
 * - 这些检查会降低性能
 * - 仅用于开发和调试阶段
 * - 发布版本应关闭以提高性能
 */

/* }================================================================== */

/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** (影响API并且必须稳定的宏(也就是说,必须是)
** same when you compile Lua and when you compile code that links to
** (当你编译Lua和当你编译链接到)
** Lua).
** (Lua的代码时相同)。)
** =====================================================================
*/

/*
@@ LUA_EXTRASPACE defines the size of a raw memory area associated with
** (LUA_EXTRASPACE定义与)
** a Lua state with very fast access.
** (Lua状态关联的原始内存区域的大小,具有非常快的访问。)
** CHANGE it if you need a different size.
** (如果你需要不同的大小,请修改它。)
*/
#define LUA_EXTRASPACE (sizeof(void *))
/*
 * 额外空间大小:
 * - 每个Lua状态(lua_State)前面预留的内存
 * - 默认是一个指针的大小(32位系统4字节,64位系统8字节)
 * - 用于存储与Lua状态关联的用户数据
 * - 访问非常快,因为位置固定在lua_State之前
 *
 * 使用方法:
 * - lua_getextraspace(L): 获取额外空间的指针
 * - 可以存储指向应用程序数据结构的指针
 * - 例如:存储应用程序上下文、回调函数等
 *
 * 为何使用sizeof(void *):
 * - 一个指针足够存储大多数需要的信息
 * - 可以指向更大的数据结构
 * - 保持内存对齐
 */

/*
@@ LUA_IDSIZE gives the maximum size for the description of the source
** (LUA_IDSIZE给出源代码描述的最大大小)
** of a function in debug information.
** (函数在调试信息中。)
** CHANGE it if you want a different size.
** (如果你想要不同的大小,请修改它。)
*/
#define LUA_IDSIZE 60
/*
 * 源代码ID大小:
 * - 用于存储函数来源的描述字符串
 * - 例如:"@filename.lua"或"=[C]"或"=(load)"
 * - 60字节足够存储大多数文件名
 * - 在调试信息和错误消息中使用
 * - 如果文件名太长会被截断
 *
 * 格式约定:
 * - "@filename": 从文件加载
 * - "=name": 特殊来源(如C代码)
 * - 其他: 直接是源代码文本
 */

/*
@@ LUAL_BUFFERSIZE is the initial buffer size used by the lauxlib
** (LUAL_BUFFERSIZE是lauxlib使用的初始缓冲区大小)
** buffer system.
** (缓冲区系统。)
*/
#define LUAL_BUFFERSIZE ((int)(16 * sizeof(void *) * sizeof(lua_Number)))
/*
 * 辅助库缓冲区大小:
 * - luaL_Buffer的初始大小
 * - 计算公式:16 * 指针大小 * lua_Number大小
 * - 在64位系统上,double作为lua_Number:
 *   16 * 8 * 8 = 1024字节
 * - 在32位系统上,double作为lua_Number:
 *   16 * 4 * 8 = 512字节
 *
 * 设计考虑:
 * - 足够大以避免频繁重新分配
 * - 不会太大浪费内存
 * - 随系统字长和数值大小自动调整
 * - 用于字符串构建等操作
 */

/*
@@ LUAI_MAXALIGN defines fields that, when used in a union, ensure
** (LUAI_MAXALIGN定义字段,当在联合中使用时,确保)
** maximum alignment for the other items in that union.
** (该联合中其他项的最大对齐。)
*/
#define LUAI_MAXALIGN \
	lua_Number n;     \
	double u;         \
	void *s;          \
	lua_Integer i;    \
	long l
/*
 * 最大对齐定义:
 * - 用于创建具有最大对齐要求的联合(union)
 * - 包含各种可能需要最大对齐的类型
 * - lua_Number n: Lua数值类型
 * - double u: 双精度浮点(通常需要8字节对齐)
 * - void *s: 指针(在64位系统需要8字节对齐)
 * - lua_Integer i: Lua整数类型
 * - long l: 长整数
 *
 * 内存对齐重要性:
 * - CPU访问对齐的数据更快
 * - 某些架构要求特定类型必须对齐,否则崩溃
 * - 例如:在某些ARM上,double必须8字节对齐
 * - 联合会使用最大对齐要求的成员的对齐
 *
 * 使用场景:
 * - Lua的内部数据结构使用这个定义
 * - 确保正确的内存对齐
 * - 提高性能和跨平台兼容性
 */

/* }================================================================== */

/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** (本地配置。你可以使用这个空间添加你的重新定义)
** without modifying the main part of the file.
** (而不修改文件的主要部分。)
*/

/*
 * 本地配置区域:
 * - 这是为用户保留的自定义区域
 * - 可以在这里添加特定项目的配置
 * - 不需要修改文件的主体部分
 * - 便于维护和升级Lua源码
 *
 * 使用建议:
 * - 在这里重新定义需要修改的宏
 * - 或者添加项目特定的配置
 * - 保持主要配置部分不变
 */

#endif
/*
 * 头文件保护结束
 * - 对应开头的#ifndef luaconf_h
 */

/*
================================================================================
文件总结
================================================================================

【核心功能回顾】
这个配置文件是Lua的核心配置中心,通过大量的宏定义控制Lua的编译行为。

【主要配置类别】

1. 平台适配 (行41-120)
   - Windows/Linux/macOS/iOS等平台的特定配置
   - C89/C99标准的兼容性控制
   - 动态库加载机制(DLL/dlopen)
   - readline库集成

2. 数值类型配置 (行127-457)
   - 整数类型选择:int/long/long long
   - 浮点类型选择:float/double/long double
   - 默认配置:64位整数(long long)和双精度浮点(double)
   - 特殊配置:32位Lua、C89兼容模式

3. 路径配置 (行464-579)
   - Lua库搜索路径(LUA_PATH)
   - C库搜索路径(LUA_CPATH)
   - 平台特定的目录结构
   - 模块查找规则

4. API导出控制 (行586-628)
   - DLL导出/导入机制(Windows)
   - extern声明(非Windows)
   - C++兼容性(extern "C")

5. 兼容性选项 (行635-664)
   - 与旧版本Lua的兼容性
   - 已弃用函数的支持
   - API变更的适配宏

6. 底层实现细节 (行671-866)
   - 数值格式化和转换
   - 字符串处理
   - 本地化支持
   - 编译器优化提示

7. 语言特性控制 (行873-916)
   - 类型自动转换控制
   - API一致性检查
   - 内存和对齐设置

【关键设计思想】

1. 灵活性
   - 通过宏定义提供大量可配置选项
   - 支持从嵌入式系统到服务器的各种环境

2. 兼容性
   - 同时支持C89和C99标准
   - 跨平台兼容(Windows/Unix/macOS)
   - 向后兼容旧版本Lua

3. 性能优化
   - 编译器特定优化(如GCC的__builtin_expect)
   - 内存对齐优化
   - 类型选择优化

4. 可维护性
   - 集中式配置管理
   - 清晰的注释和文档
   - 预留本地配置空间

【使用指南】

编译Lua时,可以通过以下方式配置:

1. 编译器参数:
   gcc -DLUA_32BITS -DLUA_USE_LINUX lua.c

2. 修改本文件:
   在文件末尾的本地配置区域添加自定义宏

3. 常见配置场景:
   - 嵌入式系统:定义LUA_32BITS节省内存
   - 老旧编译器:定义LUA_USE_C89保证兼容
   - Windows开发:定义LUA_BUILD_AS_DLL构建动态库
   - 调试阶段:定义LUA_USE_APICHECK进行API检查

【与其他文件的关系】

- lua.h: 定义版本号等常量,本文件引用这些常量
- luastate.c: 使用LUA_EXTRASPACE定义
- lmathlib.c: 使用数学函数相关的宏
- loadlib.c: 使用动态库加载相关的宏
- lobject.h: 使用LUA_NUMBER、LUA_INTEGER等类型定义

【学习建议】

1. 初学者:
   - 理解默认配置即可
   - 关注数值类型和路径配置
   - 了解平台差异

2. 进阶开发者:
   - 深入理解各种配置选项的影响
   - 学习如何针对特定需求定制Lua
   - 研究不同配置的性能差异

3. 系统开发者:
   - 掌握所有配置选项
   - 理解底层实现细节
   - 能够移植Lua到新平台

【总结】
luaconf.h是Lua的配置中枢,通过精心设计的宏定义系统,实现了Lua在不同平台、
不同需求下的灵活适配。理解这个文件有助于深入了解Lua的设计理念和实现细节,
也是定制和优化Lua的基础。

================================================================================
*/