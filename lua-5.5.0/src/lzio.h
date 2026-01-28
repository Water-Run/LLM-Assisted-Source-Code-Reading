/*
** $Id: lzio.h $
** Buffered streams (缓冲流)
** See Copyright Notice in lua.h (参见lua.h中的版权声明)
*/

/*
================================================================================
文件概要: lzio.h - Lua 缓冲输入流模块
================================================================================

【文件基本信息】
- 文件名: lzio.h
- 功能: 定义Lua的缓冲输入流(ZIO)和可变缓冲区(Mbuffer)数据结构及相关操作
- 类型: 头文件(接口定义)
- 依赖: lua.h, lmem.h

【主要功能】
1. ZIO (缓冲输入流):
   - 为Lua提供统一的输入流抽象层
   - 支持从不同来源(文件、字符串、内存等)读取数据
   - 使用缓冲机制提高读取效率
   - 主要用于词法分析器(lexer)读取源代码

2. Mbuffer (可变缓冲区):
   - 动态可增长的字符缓冲区
   - 用于临时存储字符串数据
   - 自动管理内存分配和释放

【核心数据结构】
- ZIO: 输入流结构,包含缓冲区指针、读取器函数等
- Mbuffer: 可变缓冲区结构,用于动态字符串操作

【使用场景】
- 编译Lua代码时读取源文件
- 从字符串或内存块加载Lua代码
- 在词法分析阶段缓冲输入字符

================================================================================
*/

#ifndef lzio_h
#define lzio_h

#include "lua.h"  // Lua核心定义
#include "lmem.h" // Lua内存管理模块

/*
--------------------------------------------------------------------------------
常量定义部分
--------------------------------------------------------------------------------
*/

/* EOZ: End Of Z-stream (流结束标记)
 * 值为-1,表示输入流已经到达末尾
 * 当zgetc()无法再读取字符时返回此值
 */
#define EOZ (-1)

/*
--------------------------------------------------------------------------------
类型定义部分
--------------------------------------------------------------------------------
*/

/* ZIO结构体的类型别名
 * 使用typedef简化后续代码中的类型声明
 * ZIO = Z Input/Output (Z输入输出流)
 */
typedef struct Zio ZIO;

/*
--------------------------------------------------------------------------------
ZIO 输入流操作宏
--------------------------------------------------------------------------------
*/

/*
 * zgetc 宏 - 从输入流中读取一个字符
 *
 * 参数:
 *   z - ZIO结构体指针
 *
 * 返回值:
 *   成功: 返回读取的字符(0-255,作为unsigned char)
 *   失败: 返回EOZ(-1),表示流结束
 *
 * 工作原理:
 *   1. 先检查缓冲区中是否还有未读数据: (z)->n-- > 0
 *   2. 如果有:
 *      - 将n递减(表示未读字节数减1)
 *      - 从当前位置p读取一个字节: *(z)->p
 *      - 移动指针到下一位置: (z)->p++
 *      - 转换为unsigned char: cast_uchar()
 *   3. 如果缓冲区为空:
 *      - 调用luaZ_fill(z)填充缓冲区
 *      - luaZ_fill会调用reader函数读取新数据
 *
 * 性能优化:
 *   - 使用宏而非函数避免函数调用开销
 *   - 大部分情况直接从缓冲区读取,效率高
 *   - 只在缓冲区空时才调用填充函数
 *
 * C语言知识点:
 *   - 三元运算符: condition ? true_value : false_value
 *   - 后缀递减: n-- (先使用n的值,然后n减1)
 *   - 指针解引用和递增: *(p++) (先取p指向的值,然后p加1)
 *   - cast_uchar是Lua定义的宏,将char转为unsigned char(避免负值)
 */
#define zgetc(z) (((z)->n--) > 0 ? cast_uchar(*(z)->p++) : luaZ_fill(z))

/*
--------------------------------------------------------------------------------
Mbuffer 可变缓冲区结构定义
--------------------------------------------------------------------------------
*/

/*
 * Mbuffer - Memory Buffer (内存缓冲区)
 *
 * 用途:
 *   - 动态增长的字符缓冲区
 *   - 用于临时存储字符串、标识符、字面量等
 *   - 在词法分析时存储正在读取的token
 *
 * 字段说明:
 *   buffer   - 指向实际缓冲区内存的指针
 *              初始为NULL,使用时动态分配
 *   n        - 当前缓冲区中已使用的字节数
 *              表示有效数据的长度
 *   buffsize - 缓冲区的总容量(以字节为单位)
 *              可能大于n,差值是剩余可用空间
 *
 * 设计思想:
 *   - 类似C++的std::vector<char>
 *   - 容量不足时自动扩展,避免频繁重新分配
 *   - 通过n和buffsize分离"已用大小"和"总容量"
 */
typedef struct Mbuffer
{
  char *buffer;    // 缓冲区指针,指向动态分配的内存
  size_t n;        // 已使用的字节数(有效数据长度)
  size_t buffsize; // 缓冲区总容量(字节数)
} Mbuffer;

/*
--------------------------------------------------------------------------------
Mbuffer 缓冲区操作宏
--------------------------------------------------------------------------------
*/

/*
 * luaZ_initbuffer - 初始化缓冲区
 *
 * 参数:
 *   L    - lua_State指针(用于后续可能的内存分配)
 *   buff - Mbuffer结构体指针
 *
 * 功能:
 *   将缓冲区设置为空状态:
 *   - buffer = NULL (未分配内存)
 *   - buffsize = 0  (容量为0)
 *   - 注意: n的值未初始化,使用前应设为0
 *
 * C语言知识点:
 *   - 逗号表达式: (expr1, expr2) 依次执行expr1和expr2,返回expr2的值
 *   - 宏中使用逗号可以执行多条语句
 */
#define luaZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

/*
 * luaZ_buffer - 获取缓冲区指针
 *
 * 返回值: 指向缓冲区内存的char*指针
 * 用途: 访问缓冲区的实际数据
 */
#define luaZ_buffer(buff) ((buff)->buffer)

/*
 * luaZ_sizebuffer - 获取缓冲区总容量
 *
 * 返回值: 缓冲区总大小(字节数)
 * 注意: 这是容量,不是已使用大小
 */
#define luaZ_sizebuffer(buff) ((buff)->buffsize)

/*
 * luaZ_bufflen - 获取缓冲区已使用长度
 *
 * 返回值: 缓冲区中有效数据的长度
 * 这是实际包含数据的字节数,≤ buffsize
 */
#define luaZ_bufflen(buff) ((buff)->n)

/*
 * luaZ_buffremove - 从缓冲区末尾移除指定数量的字节
 *
 * 参数:
 *   buff - Mbuffer指针
 *   i    - 要移除的字节数
 *
 * 功能:
 *   将已使用长度n减去i
 *   相当于"退回"最后i个字节
 *   不释放内存,只是调整有效长度
 *
 * C语言知识点:
 *   - cast_sizet: 将i转换为size_t类型,确保类型匹配
 *   - 复合赋值: n -= i 等价于 n = n - i
 */
#define luaZ_buffremove(buff, i) ((buff)->n -= cast_sizet(i))

/*
 * luaZ_resetbuffer - 重置缓冲区
 *
 * 功能:
 *   将已使用长度设为0
 *   相当于"清空"缓冲区
 *   注意: 不释放内存,buffer和buffsize保持不变
 *   这样可以复用已分配的内存,避免频繁分配/释放
 */
#define luaZ_resetbuffer(buff) ((buff)->n = 0)

/*
 * luaZ_resizebuffer - 调整缓冲区大小
 *
 * 参数:
 *   L    - lua_State指针,用于内存管理
 *   buff - Mbuffer指针
 *   size - 新的缓冲区大小(字节数)
 *
 * 功能:
 *   1. 调用luaM_reallocvchar重新分配内存
 *   2. 更新buffer指针指向新内存
 *   3. 更新buffsize为新大小
 *
 * 内存管理:
 *   - 如果size > buffsize: 扩大缓冲区,可能移动内存位置
 *   - 如果size < buffsize: 缩小缓冲区,可能释放部分内存
 *   - 如果size = 0: 释放所有内存,相当于freebuffer
 *   - luaM_reallocvchar会保留原有数据(直到min(旧大小,新大小))
 *
 * C语言知识点:
 *   - 多行宏定义: 使用反斜杠\续行
 *   - 逗号表达式: 依次执行多个语句
 *   - luaM_reallocvchar: Lua的内存重分配函数,类似realloc
 *     - vchar表示"vector of char"(字符数组)
 */
#define luaZ_resizebuffer(L, buff, size)                       \
  ((buff)->buffer = luaM_reallocvchar(L, (buff)->buffer,       \
                                      (buff)->buffsize, size), \
   (buff)->buffsize = size)

/*
 * luaZ_freebuffer - 释放缓冲区内存
 *
 * 参数:
 *   L    - lua_State指针
 *   buff - Mbuffer指针
 *
 * 功能:
 *   调用luaZ_resizebuffer将大小设为0
 *   这会释放buffer指向的内存
 *   buffer变为NULL,buffsize变为0
 *
 * 实现方式:
 *   直接复用resizebuffer(size=0)实现释放
 *   体现了代码复用的设计思想
 */
#define luaZ_freebuffer(L, buff) luaZ_resizebuffer(L, buff, 0)

/*
--------------------------------------------------------------------------------
ZIO 公共函数声明
--------------------------------------------------------------------------------
*/

/*
 * luaZ_init - 初始化输入流
 *
 * 参数:
 *   L      - lua_State指针,Lua虚拟机状态
 *   z      - 要初始化的ZIO结构体指针
 *   reader - 读取函数指针,用于实际读取数据
 *   data   - 传递给reader的额外数据(如文件句柄等)
 *
 * 功能:
 *   设置ZIO结构体的各个字段,准备从指定数据源读取
 *
 * 使用场景:
 *   - 从文件读取: reader为文件读取函数,data为FILE*
 *   - 从字符串读取: reader为字符串读取函数,data为字符串指针
 *   - 从内存读取: reader为内存读取函数,data为内存块信息
 *
 * C语言知识点:
 *   - LUAI_FUNC: Lua内部函数声明宏,可能定义为static或extern
 *   - lua_Reader: 函数指针类型,定义在lua.h中
 *     typedef const char * (*lua_Reader) (lua_State *L, void *data, size_t *size);
 */
LUAI_FUNC void luaZ_init(lua_State *L, ZIO *z, lua_Reader reader,
                         void *data);

/*
 * luaZ_read - 从输入流读取指定数量的字节
 *
 * 参数:
 *   z - ZIO结构体指针
 *   b - 目标缓冲区指针,读取的数据将写入这里
 *   n - 要读取的字节数
 *
 * 返回值:
 *   实际读取的字节数
 *   可能小于n(如果到达流末尾)
 *
 * 功能:
 *   从输入流读取n个字节到缓冲区b
 *   如果当前缓冲区数据不足,会调用reader继续读取
 *
 * 与zgetc的区别:
 *   - zgetc: 一次读一个字符,宏实现,速度快
 *   - luaZ_read: 一次读多个字节,函数实现,适合批量读取
 */
LUAI_FUNC size_t luaZ_read(ZIO *z, void *b, size_t n);
/* read next n bytes (读取接下来的n个字节) */

/*
 * luaZ_getaddr - 获取输入流中指定长度数据的地址
 *
 * 参数:
 *   z - ZIO结构体指针
 *   n - 需要的字节数
 *
 * 返回值:
 *   指向数据的const void*指针
 *   如果缓冲区中没有足够数据,返回NULL
 *
 * 功能:
 *   返回当前缓冲区中n个字节的起始地址
 *   不复制数据,直接返回指针(零拷贝优化)
 *   如果缓冲区数据不足,返回NULL
 *
 * 使用场景:
 *   - 需要访问但不消耗数据时
 *   - 避免数据拷贝,提高效率
 *   - 主要用于预读(lookahead)操作
 *
 * 注意事项:
 *   返回的指针仅在下次读取操作前有效
 *   调用zgetc或luaZ_read后,指针可能失效
 */
LUAI_FUNC const void *luaZ_getaddr(ZIO *z, size_t n);

/*
--------------------------------------------------------------------------------
私有部分 - ZIO 结构体定义
--------------------------------------------------------------------------------
*/

/* --------- Private Part ------------------ */
/* --------- 私有部分 ------------------ */

/*
 * struct Zio - 输入流结构体
 *
 * 这是ZIO的完整定义,包含实现细节
 * 用户代码应通过上述公共接口访问,不直接操作这些字段
 *
 * 字段详解:
 *
 * n (size_t) - bytes still unread (缓冲区中尚未读取的字节数)
 *   - 表示当前缓冲区还有多少字节可读
 *   - 初始为0,调用luaZ_fill后被设置
 *   - zgetc每次调用会递减
 *   - 为0时需要重新填充缓冲区
 *
 * p (const char*) - current position in buffer (缓冲区中的当前位置)
 *   - 指向下一个要读取的字节
 *   - zgetc每次读取后会递增
 *   - 配合n使用: p指向开始,p+n指向结束
 *   - const保证不会通过p修改源数据
 *
 * reader (lua_Reader) - reader function (读取器函数)
 *   - 函数指针,用于实际的数据读取
 *   - 签名: const char* (*lua_Reader)(lua_State*, void*, size_t*)
 *   - 当缓冲区为空时,luaZ_fill调用此函数
 *   - 返回新数据的指针和大小
 *   - 不同数据源有不同的reader实现
 *
 * data (void*) - additional data (附加数据)
 *   - 传递给reader函数的上下文数据
 *   - 类型为void*,可以是任何类型的指针
 *   - 例如: FILE*用于文件读取,字符串指针用于字符串读取
 *   - 使用void*实现多态,增加灵活性
 *
 * L (lua_State*) - Lua state (for reader) (Lua状态机,供读取器使用)
 *   - 指向Lua虚拟机状态
 *   - reader函数可能需要访问Lua状态(如报告错误)
 *   - 也用于内存管理(通过L分配内存)
 *
 * 设计模式:
 *   - 策略模式: 通过reader函数指针实现不同的读取策略
 *   - 适配器模式: 将不同数据源适配为统一的ZIO接口
 *
 * 缓冲机制:
 *   1. reader一次性读取一块数据
 *   2. p指向这块数据,n记录大小
 *   3. zgetc逐字节消耗,n递减,p递增
 *   4. n为0时,调用luaZ_fill重新填充
 *   5. 减少reader调用次数,提高效率
 */
struct Zio
{
  size_t n;          /* bytes still unread (尚未读取的字节数) */
  const char *p;     /* current position in buffer (缓冲区中的当前位置) */
  lua_Reader reader; /* reader function (读取器函数) */
  void *data;        /* additional data (附加数据) */
  lua_State *L;      /* Lua state (for reader) (Lua状态,供读取器使用) */
};

/*
 * luaZ_fill - 填充输入流缓冲区
 *
 * 参数:
 *   z - ZIO结构体指针
 *
 * 返回值:
 *   成功: 返回读取的第一个字符(作为int)
 *   失败: 返回EOZ(-1),表示流结束
 *
 * 功能:
 *   1. 调用z->reader函数读取新数据
 *   2. 更新z->p指向新数据
 *   3. 更新z->n为新数据的大小
 *   4. 返回第一个字符(同时消耗它)
 *
 * 调用时机:
 *   - zgetc宏发现n=0时自动调用
 *   - 用户通常不直接调用此函数
 *
 * 实现位置:
 *   这是函数声明,实际实现在lzio.c中
 *
 * C语言知识点:
 *   - 返回int而非char是为了能够返回EOF(-1)
 *   - unsigned char范围是0-255,不能表示-1
 *   - int可以表示0-255以及-1
 */
LUAI_FUNC int luaZ_fill(ZIO *z);

#endif

/*
================================================================================
文件总结: lzio.h - Lua 缓冲输入流模块详解
================================================================================

【整体架构】
本文件实现了Lua的输入抽象层,主要包含两大组件:
1. ZIO (Z Input/Output): 缓冲输入流
2. Mbuffer (Memory Buffer): 可变内存缓冲区

【ZIO 设计思想】

1. 抽象与解耦
   - 将数据源(文件、字符串、内存等)抽象为统一的ZIO接口
   - 上层代码(词法分析器)只需使用zgetc等接口,无需关心数据来源
   - 通过lua_Reader函数指针实现多态,支持不同数据源

2. 缓冲机制
   - 批量读取: reader一次读取一块数据到内存
   - 逐字节消耗: zgetc从缓冲区逐字节读取
   - 自动填充: 缓冲区空时自动调用luaZ_fill
   - 优点: 减少系统调用(如fread)次数,提高I/O效率

3. 性能优化
   - zgetc使用宏实现,避免函数调用开销
   - 大部分情况下直接从缓冲区读取(快速路径)
   - 只在必要时才调用函数(慢速路径)
   - 零拷贝: luaZ_getaddr直接返回指针,不复制数据

【Mbuffer 设计思想】

1. 动态增长
   - 按需分配内存,初始为空
   - 容量不足时自动扩展(通常翻倍)
   - 避免固定大小限制,适应各种输入

2. 内存复用
   - resetbuffer只重置n,不释放内存
   - 下次使用可以复用已分配空间
   - 减少malloc/free调用,提高效率

3. 简洁API
   - 通过宏提供简洁的操作接口
   - 宏在编译时展开,零运行时开销

【C语言技术要点】

1. 宏的高级用法
   - 多语句宏: 使用逗号表达式
   - 多行宏: 使用反斜杠续行
   - 类型转换宏: cast_uchar, cast_sizet

2. 指针技巧
   - 指针算术: p++移动到下一字节
   - void*泛型指针: data字段支持任意类型
   - const正确性: const char*保护源数据

3. 函数指针
   - lua_Reader类型: 实现回调机制
   - 实现多态: 不同reader处理不同数据源
   - 策略模式的C语言实现

4. 内存管理
   - luaM_reallocvchar: Lua的内存分配器
   - 统一内存管理: 所有分配都通过Lua内存管理器
   - 便于内存追踪和垃圾回收集成

【使用流程示例】

典型的ZIO使用流程:
1. 准备数据源(如打开文件)
2. 调用luaZ_init初始化ZIO,传入reader和data
3. 词法分析器调用zgetc逐字符读取
4. zgetc从缓冲区读取,缓冲区空时自动调用luaZ_fill
5. luaZ_fill调用reader获取新数据
6. 读取完成后,资源由reader负责清理

Mbuffer使用流程:
1. 声明Mbuffer变量
2. 调用luaZ_initbuffer初始化
3. 使用时自动调用luaZ_resizebuffer扩展
4. 通过luaZ_buffer访问数据
5. 使用完后调用luaZ_freebuffer释放

【与Lua编译器的关系】

ZIO在Lua编译流程中的位置:
1. 源代码输入 -> ZIO(本模块)
2. ZIO -> 词法分析器(llex.c)
3. 词法分析器 -> Token流
4. Token流 -> 语法分析器(lparser.c)
5. 语法分析器 -> 字节码

ZIO屏蔽了输入来源的差异,让编译器其他部分代码统一。

【设计优点】

1. 模块化: ZIO独立封装,职责单一
2. 灵活性: 通过reader支持多种数据源
3. 高效性: 缓冲机制减少I/O次数
4. 易用性: 简洁的宏接口,类似标准库
5. 可移植性: 不依赖平台特定的I/O操作

【学习建议】

1. 理解缓冲I/O的原理和优势
2. 学习如何用C实现多态(函数指针)
3. 掌握宏的编写技巧和注意事项
4. 理解内存管理的最佳实践
5. 阅读lzio.c中的实际实现代码
6. 查看llex.c了解ZIO的实际使用

【相关文件】

- lzio.c: 本文件中声明的函数的实现
- llex.c: 词法分析器,ZIO的主要使用者
- lmem.h/lmem.c: Lua内存管理模块
- lua.h: Lua核心定义,包括lua_Reader等

================================================================================
*/