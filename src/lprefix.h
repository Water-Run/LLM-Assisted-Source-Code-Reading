/*
** ============================================================================
** 文件概要说明
** ============================================================================
** 文件名: lprefix.h
** 作用: Lua前缀头文件 - 必须在任何其他头文件之前包含的定义
**
** 功能描述:
** 这是Lua源码中最基础的头文件之一,用于在包含任何其他头文件之前设置必要的
** 编译器宏定义。主要完成以下功能:
**
** 1. 启用POSIX/XSI标准功能
**    - 通过定义_XOPEN_SOURCE宏,使程序能够使用POSIX标准中的扩展功能
**    - POSIX (Portable Operating System Interface) 是类Unix系统的标准接口
**
** 2. 支持大文件操作(Large File Support)
**    - 在64位系统上启用大于2GB文件的支持
**    - 通过定义_FILE_OFFSET_BITS为64实现
**
** 3. Windows平台兼容性
**    - 禁用某些Windows特定的安全警告
**    - 使代码在Windows上编译时更加友好
**
** 技术要点:
** - 使用条件编译(#if, #ifdef等预处理指令)针对不同平台进行适配
** - 这些宏必须在包含标准库头文件(如stdio.h)之前定义才能生效
** - 这就是为什么这个文件叫"prefix"(前缀)的原因
**
** ============================================================================
*/

/*
** $Id: lprefix.h $
** Definitions for Lua code that must come before any other header file
** 必须在任何其他头文件之前的Lua代码定义
** See Copyright Notice in lua.h
** 参见lua.h中的版权声明
*/

/*
** 头文件保护(Header Guard)
**
** 作用: 防止头文件被重复包含
** 工作原理:
** - 第一次包含时,lprefix_h未定义,所以会定义它并处理文件内容
** - 第二次包含时,lprefix_h已定义,整个文件内容会被跳过
**
** 这是C语言中标准的头文件保护模式,防止重复定义导致编译错误
*/
#ifndef lprefix_h
#define lprefix_h

/*
** ============================================================================
** POSIX/XSI功能启用部分
** ============================================================================
*/

/*
** Allows POSIX/XSI stuff
** 允许使用POSIX/XSI标准的功能
**
** 说明:
** - POSIX: 可移植操作系统接口,定义了Unix系统的标准API
** - XSI: X/Open System Interface,POSIX的扩展标准
*/

/*
** 条件编译: 仅在非C89模式下执行
**
** LUA_USE_C89: 如果定义了此宏,表示要求Lua严格遵循C89标准
** !defined(LUA_USE_C89): 如果未定义此宏(即允许使用更新的标准)
** { 和 }: 用于标记条件编译块的开始和结束,方便代码折叠和阅读
*/
#if !defined(LUA_USE_C89) /* { */

/*
** _XOPEN_SOURCE 宏的处理
**
** _XOPEN_SOURCE: 这是一个特性测试宏(Feature Test Macro)
** 作用: 告诉系统头文件应该暴露哪些POSIX/XSI标准的功能
**
** 值的含义:
** - 500: Single Unix Specification version 1 (SUSv1)
** - 600: Single Unix Specification version 3 (SUSv3), 对应POSIX.1-2001
** - 700: Single Unix Specification version 4 (SUSv4), 对应POSIX.1-2008
*/
#if !defined(_XOPEN_SOURCE)
/*
** 如果_XOPEN_SOURCE未定义,则定义为600
**
** 600对应POSIX.1-2001标准,提供了:
** - 线程支持(pthread)
** - 实时扩展
** - 高级I/O功能
** - 更多的数学函数等
*/
#define _XOPEN_SOURCE 600

#elif _XOPEN_SOURCE == 0
/*
** 特殊处理: 如果_XOPEN_SOURCE被显式定义为0
**
** 用途: 允许用户通过编译选项 -D_XOPEN_SOURCE=0 来完全禁用XOPEN功能
** 这种情况下,取消该宏的定义,让系统使用默认行为
**
** 高级C用法说明:
** #undef: 预处理指令,用于取消之前定义的宏
*/
#undef _XOPEN_SOURCE /* use -D_XOPEN_SOURCE=0 to undefine it */
/* 使用 -D_XOPEN_SOURCE=0 来取消定义它 */
#endif

/*
** ============================================================================
** 大文件支持(Large File Support, LFS)
** ============================================================================
*/

/*
** Allows manipulation of large files in gcc and some other compilers
** 允许在gcc和其他一些编译器中操作大文件
**
** 背景知识:
** 在32位系统中,文件偏移量(offset)通常是32位的long类型,最大只能表示2GB
** 为了支持大于2GB的文件,需要使用64位的off_t类型
*/

/*
** 条件: 同时满足以下条件才启用大文件支持
** 1. !defined(LUA_32BITS): 不是32位Lua(即64位或允许64位文件操作)
** 2. !defined(_FILE_OFFSET_BITS): _FILE_OFFSET_BITS尚未定义
*/
#if !defined(LUA_32BITS) && !defined(_FILE_OFFSET_BITS)

/*
** _LARGEFILE_SOURCE: 启用大文件相关的函数
**
** 作用: 使系统头文件提供处理大文件的额外函数,例如:
** - fseeko64() / ftello64(): 64位版本的文件定位函数
** - 等等
*/
#define _LARGEFILE_SOURCE 1

/*
** _FILE_OFFSET_BITS: 设置文件偏移量的位数
**
** 作用: 定义为64时,会发生以下转换:
** - off_t 类型从32位变为64位
** - fopen/fseek等函数会自动使用64位版本
** - 无需修改代码,只需重新编译即可支持大文件
**
** 这是一种透明的大文件支持机制,称为LFS (Large File Summit)
**
** 技术细节:
** 当定义_FILE_OFFSET_BITS为64时,预处理器会将标准函数映射到64位版本:
** - fopen → fopen64
** - lseek → lseek64
** - stat → stat64
** 等等
*/
#define _FILE_OFFSET_BITS 64
#endif

#endif /* } */
/* 结束 !defined(LUA_USE_C89) 条件编译块 */

/*
** ============================================================================
** Windows平台特定设置
** ============================================================================
*/

/*
** Windows stuff
** Windows相关设置
**
** 说明:
** Windows使用的是不同的C运行时库(CRT - C Runtime Library)
** 需要特殊的宏定义来控制编译行为
*/

/*
** 条件: 如果在Windows平台编译
** _WIN32: Windows平台的标准预定义宏(包括32位和64位Windows)
** 注意: 即使是64位Windows,_WIN32也会被定义
*/
#if defined(_WIN32) /* { */

/*
** _CRT_SECURE_NO_WARNINGS 宏的处理
**
** 背景:
** Microsoft的C运行时库(CRT)会对某些标准C函数发出安全警告,例如:
** - strcpy → strcpy_s (安全版本)
** - sprintf → sprintf_s
** - fopen → fopen_s
** 等等
**
** 这些警告信息类似于:
** "warning C4996: 'strcpy': This function may be unsafe.
**  Consider using strcpy_s instead."
*/
#if !defined(_CRT_SECURE_NO_WARNINGS)
/*
** 定义此宏以禁用这些安全警告
**
** 原因:
** 1. Lua代码需要跨平台编译,使用标准ISO C函数
** 2. Microsoft特定的"安全"函数(如strcpy_s)不是标准C,会破坏可移植性
** 3. Lua有自己的安全检查机制
**
** 技术说明:
** _CRT_SECURE_NO_WARNINGS是Microsoft Visual C++特有的宏
** 其他编译器会忽略这个宏定义
*/
#define _CRT_SECURE_NO_WARNINGS /* avoid warnings about ISO C functions */
                                /* 避免关于ISO C函数的警告 */
#endif

#endif /* } */
/* 结束 defined(_WIN32) 条件编译块 */

#endif
/* 结束头文件保护 */

/*
** ============================================================================
** 详细总结说明
** ============================================================================
**
** 一、文件的整体作用
** ------------------
** lprefix.h是Lua源码的"先锋"头文件,它的职责是在编译开始前设置好环境,
** 确保后续的所有代码都能在正确的标准和特性下编译。
**
** 类比: 就像演出前的舞台准备工作,必须在演员登场前完成。
**
**
** 二、关键技术点详解
** ------------------
**
** 1. 条件编译 (Conditional Compilation)
**    - #if, #ifdef, #ifndef, #elif, #else, #endif
**    - 作用: 根据不同条件编译不同的代码
**    - 优势: 一套源码适配多个平台,无需维护多个版本
**
** 2. 预处理器宏 (Preprocessor Macros)
**    - #define: 定义宏
**    - #undef: 取消宏定义
**    - 预处理器在真正编译前处理这些指令,进行文本替换
**
** 3. 特性测试宏 (Feature Test Macros)
**    - _XOPEN_SOURCE, _POSIX_C_SOURCE等
**    - 作用: 控制系统头文件暴露哪些功能
**    - 必须在包含系统头文件前定义
**
** 4. 头文件保护 (Include Guards)
**    - 防止重复包含导致的重复定义错误
**    - 标准模式: #ifndef XXX_h, #define XXX_h, ..., #endif
**
**
** 三、不同平台的适配策略
** ----------------------
**
** 1. Unix/Linux系统:
**    - 启用POSIX.1-2001标准 (_XOPEN_SOURCE=600)
**    - 启用大文件支持 (64位文件偏移量)
**
** 2. Windows系统:
**    - 禁用Microsoft特有的安全警告
**    - 保持ISO C标准函数的使用
**
** 3. 严格C89模式:
**    - 如果定义了LUA_USE_C89,则不做任何扩展
**    - 严格遵循1989年的C标准
**
**
** 四、为什么这些设置很重要
** ------------------------
**
** 1. 可移植性:
**    - Lua需要在多种系统上编译:Linux, Windows, macOS, BSD等
**    - 不同系统有不同的默认行为和可用功能
**    - 通过统一设置,确保代码在各平台表现一致
**
** 2. 功能完整性:
**    - 不设置_XOPEN_SOURCE,某些POSIX函数可能不可用
**    - 不设置_FILE_OFFSET_BITS=64,无法处理大文件
**
** 3. 编译顺畅:
**    - 在Windows上避免大量烦人的警告信息
**    - 使编译过程更干净、更专业
**
**
** 五、学习要点
** ------------
**
** 1. C语言的编译过程:
**    源代码 → 预处理 → 编译 → 汇编 → 链接 → 可执行文件
**    本文件影响的是"预处理"阶段
**
** 2. 跨平台编程的基本思路:
**    - 使用条件编译适配不同平台
**    - 使用标准C函数保证可移植性
**    - 通过特性宏启用需要的系统功能
**
** 3. 头文件的重要性:
**    - 头文件的包含顺序很重要
**    - 某些宏必须在包含标准库前定义
**    - lprefix.h就是为了解决这个"先后顺序"问题
**
**
** 六、实际应用场景
** ----------------
**
** 当Lua需要:
** - 读写超过2GB的文件 → 需要_FILE_OFFSET_BITS=64
** - 使用多线程(pthread) → 需要_XOPEN_SOURCE=600
** - 在Windows上编译 → 需要禁用CRT安全警告
**
** 如果不正确设置这些宏:
** - 在32位系统可能无法处理大文件
** - 某些POSIX函数编译失败
** - Windows上出现大量警告影响开发
**
**
** 七、扩展阅读建议
** ----------------
**
** 1. POSIX标准: 了解类Unix系统的标准接口
** 2. C预处理器: 深入理解#define, #if等指令
** 3. 大文件支持(LFS): 理解32位系统的文件大小限制
** 4. 跨平台编程: 学习如何编写可移植的C代码
**
** ============================================================================
*/