/*
** ============================================================================
** 文件概要
** ============================================================================
** 文件名: lctype.c
** 所属项目: Lua源码
** 主要功能:
**   这个文件实现了Lua自己的字符类型判断系统,类似于C标准库的<ctype.h>
**   但针对Lua的需求进行了定制化实现。
**
** 为什么需要自定义ctype:
**   1. 可移植性: 不依赖系统的locale设置,在所有平台上行为一致
**   2. 性能优化: 使用查表法,比函数调用更快
**   3. 定制化: 可以根据Lua语法的具体需求定义字符属性
**
** 核心数据结构:
**   - luai_ctype_: 一个256+2个元素的字节数组,存储每个字符的属性标志
**     索引0: 特殊的EOZ(End Of Z-string)标记
**     索引1-256: 对应ASCII码0-255的字符属性
**
** 字符属性标志位(通过位掩码组合):
**   0x01 (ALPHA): 字母字符
**   0x02 (DIGIT): 数字字符
**   0x04 (PUNCT): 标点符号
**   0x08 (SPACE): 空白字符
**   0x10 (XDIGIT): 十六进制数字
**
** 编译配置:
**   - LUA_USE_CTYPE: 如果定义,则使用系统ctype;否则使用本实现
**   - LUA_UCID: 如果定义,允许Unicode标识符(非ASCII字符视为字母)
** ============================================================================
*/

/*
** $Id: lctype.c $
** 'ctype' functions for Lua
** Lua的'ctype'(字符类型)函数
** See Copyright Notice in lua.h
** 参见lua.h中的版权声明
*/

/*
** 预处理器定义说明:
** - lctype_c: 标识当前编译单元为lctype.c,用于头文件的条件编译
** - LUA_CORE: 表明这是Lua核心代码的一部分,某些API只对核心代码开放
*/
#define lctype_c
#define LUA_CORE

/*
** lprefix.h: Lua的前缀头文件
** 作用: 包含平台相关的配置和一些必要的系统头文件
** 这个文件应该在所有Lua源文件中首先包含
*/
#include "lprefix.h"

/*
** lctype.h: 字符类型判断的头文件
** 包含:
**   - 字符属性标志的宏定义(ALPHA, DIGIT等)
**   - 字符判断宏(lisalpha, lisdigit等)
**   - luai_ctype_数组的声明
*/
#include "lctype.h"

/*
** 条件编译: 仅当不使用系统ctype时编译以下代码
** LUA_USE_CTYPE在luaconf.h中定义,用于选择是否使用系统的ctype.h
** 如果使用系统ctype,则不需要定义luai_ctype_数组
*/
#if !LUA_USE_CTYPE /* { */

/*
** limits.h: C标准库头文件
** 提供: UCHAR_MAX等整数类型的限制常量
** UCHAR_MAX: unsigned char的最大值,通常是255
** 用途: 确定luai_ctype_数组的大小
*/
#include <limits.h>

/*
** Unicode标识符支持的条件编译
** LUA_UCID: Lua UniCode IDentifiers的缩写
** 如果定义了这个宏,Lua将接受非ASCII字符作为标识符的一部分
*/
#if defined(LUA_UCID) /* accept UniCode IDentifiers? */
                      /* 接受Unicode标识符吗? */
/*
** NONA: NON-ASCII的缩写
** 值0x01对应ALPHA标志位
** 含义: 将所有非ASCII字符(码点>127)视为字母字符
** 这样Unicode字符就可以用在变量名等标识符中
*/
/* consider all non-ASCII codepoints to be alphabetic */
/* 将所有非ASCII码点视为字母 */
#define NONA 0x01
#else
/*
** 默认情况: 非ASCII字符没有特殊属性
** 值0x00表示这些字符不具有任何特殊属性
** 这意味着Lua标识符只能使用ASCII字母
*/
#define NONA 0x00 /* default */
/* 默认值 */
#endif

/*
** ============================================================================
** luai_ctype_ 数组详解
** ============================================================================
**
** 数组声明说明:
** - LUAI_DDEF: Lua内部数据定义宏,通常展开为 extern const 或 const
** - const lu_byte: 常量无符号字节类型(lu_byte通常typedef为unsigned char)
** - [UCHAR_MAX + 2]: 数组大小为258(通常是256+2)
**   索引0: EOZ(End Of Z-string),Lua用-1表示字符串结束
**   索引1-256: 对应字符0-255的属性
**
** 属性标志位组合说明(通过按位或运算):
**   0x00 = 无属性
**   0x01 = ALPHA (字母)
**   0x02 = DIGIT (数字)
**   0x04 = PUNCT (标点)
**   0x08 = SPACE (空白)
**   0x10 = XDIGIT (十六进制数字)
**
** 常见组合:
**   0x05 = 0x01|0x04 = ALPHA|PUNCT (字母+标点,用于字母字符)
**   0x15 = 0x01|0x04|0x10 = ALPHA|PUNCT|XDIGIT (字母+标点+十六进制)
**   0x16 = 0x02|0x04|0x10 = DIGIT|PUNCT|XDIGIT (数字+标点+十六进制)
**   0x0c = 0x04|0x08 = PUNCT|SPACE (标点+空白,用于空格等)
**
** 注意: 实际的ALPHA值可能是0x01,这里的组合是为了同时表示多个属性
** ============================================================================
*/
LUAI_DDEF const lu_byte luai_ctype_[UCHAR_MAX + 2] = {
    /*
    ** 索引0: EOZ (End Of Z-string)
    ** Lua用-1表示字符串结束,在数组中对应索引0
    ** 属性: 无特殊属性
    */
    0x00, /* EOZ */

    /*
    ** 范围0x00-0x0F (ASCII控制字符 0-15)
    ** 0x00-0x07: NULL, SOH, STX, ETX, EOT, ENQ, ACK, BEL
    ** 这些都是不可打印的控制字符,无特殊属性
    */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0. */

    /*
    ** 0x08: BS (退格)
    ** 0x09: HT (水平制表符 '\t')
    ** 0x0A: LF (换行符 '\n')
    ** 0x0B: VT (垂直制表符)
    ** 0x0C: FF (换页符)
    ** 0x0D: CR (回车符 '\r')
    ** 0x0E-0x0F: SO, SI
    ** 0x08标记为SPACE(0x08),0x09-0x0D标记为SPACE
    */
    0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,

    /*
    ** 范围0x10-0x1F (ASCII控制字符 16-31)
    ** DLE, DC1-DC4, NAK, SYN, ETB, CAN, EM, SUB, ESC, FS, GS, RS, US
    ** 这些都是控制字符,无特殊属性
    */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    /*
    ** 范围0x20-0x2F (ASCII可打印字符 32-47)
    ** 0x20: 空格 ' ' -> 0x0c (PUNCT|SPACE)
    ** 0x21-0x2F: !"#$%&'()*+,-./ -> 0x04 (PUNCT 标点符号)
    ** 这些是空格和各种标点符号
    */
    0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, /* 2. */
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,

    /*
    ** 范围0x30-0x3F (ASCII可打印字符 48-63)
    ** 0x30-0x39: '0'-'9' -> 0x16 (DIGIT|PUNCT|XDIGIT)
    **   数字字符,同时也是十六进制数字
    ** 0x3A-0x3F: :;<=>?@ -> 0x04 (PUNCT 标点符号)
    */
    0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, /* 3. */
    0x16, 0x16, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,

    /*
    ** 范围0x40-0x4F (ASCII可打印字符 64-79)
    ** 0x40: '@' -> 0x04 (PUNCT)
    ** 0x41-0x46: 'A'-'F' -> 0x15 (ALPHA|PUNCT|XDIGIT)
    **   大写字母A-F,既是字母又是十六进制数字
    ** 0x47-0x4F: 'G'-'O' -> 0x05 (ALPHA|PUNCT)
    **   大写字母G-O,仅是字母
    */
    0x04, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x05, /* 4. */
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,

    /*
    ** 范围0x50-0x5F (ASCII可打印字符 80-95)
    ** 0x50-0x5A: 'P'-'Z' -> 0x05 (ALPHA|PUNCT)
    **   大写字母P-Z,仅是字母
    ** 0x5B-0x5E: [\]^ -> 0x04 (PUNCT 标点符号)
    ** 0x5F: '_' -> 0x05 (ALPHA|PUNCT)
    **   下划线,在Lua中被视为字母(可用于标识符)
    */
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, /* 5. */
    0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 0x04, 0x05,

    /*
    ** 范围0x60-0x6F (ASCII可打印字符 96-111)
    ** 0x60: '`' -> 0x04 (PUNCT)
    ** 0x61-0x66: 'a'-'f' -> 0x15 (ALPHA|PUNCT|XDIGIT)
    **   小写字母a-f,既是字母又是十六进制数字
    ** 0x67-0x6F: 'g'-'o' -> 0x05 (ALPHA|PUNCT)
    **   小写字母g-o,仅是字母
    */
    0x04, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x05, /* 6. */
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,

    /*
    ** 范围0x70-0x7F (ASCII可打印字符 112-127)
    ** 0x70-0x7A: 'p'-'z' -> 0x05 (ALPHA|PUNCT)
    **   小写字母p-z,仅是字母
    ** 0x7B-0x7E: {|}~ -> 0x04 (PUNCT 标点符号)
    ** 0x7F: DEL (删除控制字符) -> 0x00 (无属性)
    */
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, /* 7. */
    0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 0x04, 0x00,

    /*
    ** 范围0x80-0x8F (扩展ASCII 128-143)
    ** NONA: 根据LUA_UCID编译选项确定
    **   如果支持Unicode标识符: NONA=0x01 (ALPHA)
    **   否则: NONA=0x00 (无属性)
    ** 这些是扩展ASCII或UTF-8多字节序列的一部分
    */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA, /* 8. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0x90-0x9F (扩展ASCII 144-159)
    ** 继续使用NONA标志
    */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA, /* 9. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0xA0-0xAF (扩展ASCII 160-175)
    ** 包括不间断空格等特殊字符
    ** 继续使用NONA标志
    */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA, /* a. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0xB0-0xBF (扩展ASCII 176-191)
    ** 继续使用NONA标志
    */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA, /* b. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0xC0-0xCF (扩展ASCII 192-207)
    ** 0xC0-0xC1: 0x00 (在UTF-8中是无效的起始字节)
    ** 0xC2-0xCF: NONA (有效的UTF-8双字节序列起始字节)
    */
    0x00, 0x00, NONA, NONA, NONA, NONA, NONA, NONA, /* c. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0xD0-0xDF (扩展ASCII 208-223)
    ** 全部使用NONA (有效的UTF-8双字节序列起始字节)
    */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA, /* d. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0xE0-0xEF (扩展ASCII 224-239)
    ** 全部使用NONA (有效的UTF-8三字节序列起始字节)
    */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA, /* e. */
    NONA, NONA, NONA, NONA, NONA, NONA, NONA, NONA,

    /*
    ** 范围0xF0-0xFF (扩展ASCII 240-255)
    ** 0xF0-0xF4: NONA (有效的UTF-8四字节序列起始字节)
    ** 0xF5-0xFF: 0x00 (在UTF-8中是无效的起始字节)
    ** UTF-8四字节序列最大到0xF4,之后的字节在标准UTF-8中无效
    */
    NONA, NONA, NONA, NONA, NONA, 0x00, 0x00, 0x00, /* f. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#endif /* } */
       /*
       ** 条件编译结束
       ** 如果LUA_USE_CTYPE被定义,上述代码将不会被编译
       */

/*
** ============================================================================
** 文件总结
** ============================================================================
**
** 一、整体架构
** ------------
** 本文件实现了Lua自定义的字符分类系统,核心是一个256+2元素的查找表。
** 相比使用标准C库的ctype.h函数,这种实现有三大优势:
**   1. 可移植性: 行为完全可控,不受系统locale影响
**   2. 性能: 数组查找比函数调用快
**   3. 定制化: 可以精确控制每个字符的属性
**
** 二、数据结构设计
** ----------------
** luai_ctype_数组的设计非常巧妙:
**   - 大小为UCHAR_MAX+2 (通常是258),覆盖所有可能的字节值
**   - 索引0对应EOZ(-1),是Lua中字符串结束的特殊标记
**   - 索引1-256对应字符0-255的实际属性
**   - 使用位标志而非枚举,可以同时表示多个属性(如'A'既是字母又是十六进制数字)
**
** 三、属性标志系统
** ----------------
** 每个字节存储该字符的属性位掩码:
**   Bit 0 (0x01): ALPHA  - 字母(包括下划线)
**   Bit 1 (0x02): DIGIT  - 十进制数字
**   Bit 2 (0x04): PUNCT  - 标点符号
**   Bit 3 (0x08): SPACE  - 空白字符
**   Bit 4 (0x10): XDIGIT - 十六进制数字
**
** 通过位运算可以快速判断字符属性,例如:
**   lisalpha(c) = luai_ctype_[c+1] & ALPHA
**   lisdigit(c) = luai_ctype_[c+1] & DIGIT
**
** 四、Unicode支持
** ---------------
** 通过LUA_UCID编译选项控制:
**   - 定义时: 非ASCII字符(128-255)标记为ALPHA,允许Unicode标识符
**   - 未定义: 非ASCII字符无特殊属性,仅支持ASCII标识符
**
** 注意0xC0-0xC1和0xF5-0xFF被标记为0x00,因为它们在UTF-8编码中无效。
**
** 五、特殊字符处理
** ----------------
** 1. 下划线'_' (0x5F): 被视为字母,可用于标识符的任何位置
** 2. 空白字符: 包括空格(0x20)和制表符、换行符等(0x09-0x0D)
** 3. 十六进制数字: '0'-'9','A'-'F','a'-'f'都被标记(用于解析0x开头的数字)
**
** 六、使用方式
** ------------
** 配合lctype.h中的宏使用:
**   ```c
**   // 判断是否为字母
**   if (lisalpha(c)) { ... }
**
**   // 判断是否为数字
**   if (lisdigit(c)) { ... }
**
**   // 判断是否为字母或数字(用于标识符的后续字符)
**   if (lisalnum(c)) { ... }
**   ```
**
** 七、与标准库的对比
** ------------------
** 相比C标准库ctype.h:
**   优点:
**     - 行为一致,不受locale影响
**     - 性能更好(查表 vs 函数调用)
**     - 可以自定义(如下划线被视为字母)
**   缺点:
**     - 只支持256个字符
**     - 需要维护额外的代码
**
** 八、编译选项影响
** ----------------
** 1. LUA_USE_CTYPE=1: 使用系统ctype.h,本文件代码不编译
** 2. LUA_USE_CTYPE=0: 使用本文件实现
** 3. LUA_UCID定义: 支持Unicode标识符
** 4. LUA_UCID未定义: 仅支持ASCII标识符
**
** 九、性能考虑
** ------------
** 这种实现方式的性能优势:
**   - 数组访问是O(1)操作
**   - 现代CPU的缓存友好(整个数组只有258字节)
**   - 避免了函数调用开销
**   - 位操作非常快速
**
** 十、安全性
** ----------
** 数组大小为UCHAR_MAX+2确保了:
**   - 即使传入-1(EOZ)也不会越界(映射到索引0)
**   - 所有有效的unsigned char值都有对应的索引
**   - 不需要额外的边界检查
**
** ============================================================================
*/