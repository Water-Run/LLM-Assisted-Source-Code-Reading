/*
** Summary: 本头文件定义了用于加载与保存Lua预编译块的接口。
** Summary: This header defines the interfaces responsible for loading and
** Summary: dumping Lua precompiled chunks. 这些接口在Lua虚拟机加载二进制代码
** Summary: 和生成可分发的字节码时非常核心。
*/

#ifndef lundump_h
#define lundump_h

/* Include C标准库中用于界限值的宏定义，便于后续对整数范围的处理 */
#include <limits.h>

/*
** llimits.h 包含Lua针对平台的数值限制、内存对齐、以及相关辅助宏定义。
** 这些宏保证在多种编译环境下保持类型大小与边界行为一致。
*/
#include "llimits.h"

/*
** lobject.h 定义Lua语言中最核心的数据结构，例如Proto、LClosure、TValue等。
** 这里需要Proto的定义以方便解释虚拟机代码块的结构。
*/
#include "lobject.h"

/*
** lzio.h 负责抽象Lua对于输入/输出的缓冲行为。
** 它封装了读取Lua chunks时可能用到的底层IO接口（ZIO结构）。
*/
#include "lzio.h"

/* data to catch conversion errors */
/* data to catch conversion errors. 数据串用于检测二进制块中数值转换是否出错。 */
#define LUAC_DATA "\x19\x93\r\n\x1a\n"

/* LUAC_INT 被设置为一个特殊的整型常量；用于验证二进制块的整数表示。 */
/* LUAC_INST 代表了一个用于确认指令大小端或对齐的魔数。 */
/* LUAC_NUM 通过 cast_num 提供了一个用于检查浮点数表示方式的魔数。 */
#define LUAC_INT -0x5678
#define LUAC_INST 0x12345678
#define LUAC_NUM cast_num(-370.5)

/*
** Encode major-minor version in one byte, one nibble for each
** Encode major-minor version in one byte, one nibble for each. 此宏将Lua的主版本号与次版本号
** 编码到同一个字节中，高4位表示主版本，低4位表示次版本，便于在二进制文件中快速验证版本兼容性。
*/
#define LUAC_VERSION (LUA_VERSION_MAJOR_N * 16 + LUA_VERSION_MINOR_N)

#define LUAC_FORMAT 0 /* this is the official format */
/* LUAC_FORMAT: 标示当前官方发布的chunk格式版本号，0代表默认*/

/*
** load one chunk; from lundump.c
** load one chunk; from lundump.c. 从 lundump.c 中导出的函数用于解析和装载一个Lua预编译块。
** luaU_undump 的参数说明：
**   - lua_State* L: 运行时状态机指针，为加载的函数原型分配环境；
**   - ZIO* Z: 封装的输入流，提供逐字节读取序列的接口；
**   - const char* name: 用于错误报告的chunk名称；
**   - int fixed: 指示用于生成的对象是一个固定（non-upvalue）还是可变上下文的闭包。
** 返回值：
**   - 返回一个LClosure，它封装了从chunk中创建的函数原型。
*/
LUAI_FUNC LClosure *luaU_undump(lua_State *L, ZIO *Z, const char *name,
                                int fixed);

/*
** dump one chunk; from ldump.c
** dump one chunk; from ldump.c. 该函数将给定的Proto序列化成可执行的预编译chunk。
** luaU_dump 参数说明：
**   - lua_State* L: 当前Lua状态，允许访问堆栈/字符串缓冲等资源；
**   - const Proto* f: 需要序列化的函数原型，包含指令、常量与Upvalue信息；
**   - lua_Writer w: 写入回调，Lua将通过它递出chunk数据；
**   - void* data: 传递给writer的用户数据；
**   - int strip: 表示是否剥离调试信息（1为剥离，0为保留）；
** 返回值：
**   - 成功返回0，失败返回错误代码。
*/
LUAI_FUNC int luaU_dump(lua_State *L, const Proto *f, lua_Writer w,
                        void *data, int strip);

#endif

/*
** 总结说明：
** 本文件仅提供Lua chunk的载入与导出接口的声明，实际实现分别位于 lundump.c 与 ldump.c。
** 宏定义部分为格式校验及版本识别提供静态信息，确保从二进制数据恢复Proto时能够
** 发现平台差异或数据损坏。文件尾部函数声明的注释详尽描述了参数与功能，便于迁移、调试以及
** 调用时查阅。总体来看，此头文件是Lua字节码读写流程中的关键桥梁，负责类型兼容性检测
** 以及与IO层（ZIO/lzio）协作实现chunk的序列化与反序列化。
*/