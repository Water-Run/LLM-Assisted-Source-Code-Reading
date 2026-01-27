/*
** $Id: lctype.h $
** 'ctype' functions for Lua
** Lua的'ctype'函数
** See Copyright Notice in lua.h
** 参见lua.h中的版权声明
*/

/*
 * ============================================================================
 * 文件概要说明
 * ============================================================================
 *
 * 文件名: lctype.h
 * 功能: Lua字符类型判断和转换功能的头文件
 *
 * 主要作用:
 * 1. 提供字符分类功能(判断字母、数字、空格等)
 * 2. 提供字符转换功能(如大小写转换)
 * 3. 针对Lua的特殊需求优化,而非完全对应标准C库的ctype.h
 *
 * 设计思路:
 * - 在ASCII环境下使用自定义的查找表,速度更快且行为固定
 * - 在非ASCII环境下回退到标准C库的ctype函数
 * - 特别处理了Lua语言中的字母定义(包含下划线'_')
 *
 * 关键技术点:
 * 1. 条件编译: 根据字符集选择不同的实现方式
 * 2. 位掩码技术: 使用位运算高效判断字符属性
 * 3. 查找表优化: 预先计算字符属性,避免运行时判断
 *
 * ============================================================================
 */

#ifndef lctype_h
#define lctype_h

#include "lua.h"

/*
** WARNING: the functions defined here do not necessarily correspond
** to the similar functions in the standard C ctype.h. They are
** optimized for the specific needs of Lua.
*/
/*
 * 警告: 这里定义的函数不一定对应于标准C语言ctype.h中的相似函数。
 * 它们是为Lua的特定需求而优化的。
 *
 * 说明:
 * 标准C库的ctype.h提供isalpha、isdigit等函数,但这些函数:
 * 1. 可能受locale(区域设置)影响,行为不固定
 * 2. 性能可能不如直接查表
 * 3. 不符合Lua的特殊需求(如'_'被视为字母)
 * 因此Lua实现了自己的字符类型判断系统
 */

#if !defined(LUA_USE_CTYPE)
/*
 * 如果没有预定义LUA_USE_CTYPE,则根据字符集自动选择实现方式
 * LUA_USE_CTYPE的值决定使用哪种实现:
 * - 0: 使用Lua自定义的查找表实现(更快)
 * - 1: 使用标准C库的ctype函数
 */

#if 'A' == 65 && '0' == 48
/*
 * ASCII case: can use its own tables; faster and fixed
 * ASCII情况: 可以使用自己的表; 更快且固定
 *
 * 说明:
 * 'A' == 65: 检查字符'A'的ASCII码是否为65
 * '0' == 48: 检查字符'0'的ASCII码是否为48
 * 这两个条件用于判断当前系统是否使用ASCII字符集
 *
 * ASCII字符集的优势:
 * - 字符编码固定不变
 * - 可以预先计算所有字符的属性并存储在表中
 * - 查表比调用函数更快
 */
#define LUA_USE_CTYPE 0
#else
/*
 * must use standard C ctype
 * 必须使用标准C ctype
 *
 * 说明:
 * 如果不是ASCII字符集(如EBCDIC等其他字符集),
 * 则无法使用预定义的查找表,必须依赖标准C库
 */
#define LUA_USE_CTYPE 1
#endif

#endif

#if !LUA_USE_CTYPE /* { */
/*
 * ============================================================================
 * 自定义实现部分 (使用查找表的方式)
 * ============================================================================
 *
 * 这部分代码在LUA_USE_CTYPE为0时编译,即在ASCII环境下使用
 * 核心思想: 使用位掩码和查找表实现高效的字符属性判断
 */

#include <limits.h>
/*
 * limits.h: 标准C库头文件,定义了各种数据类型的限制
 * 这里主要使用UCHAR_MAX,它是unsigned char类型的最大值(通常是255)
 */

#include "llimits.h"
/*
 * llimits.h: Lua自定义的限制和类型定义头文件
 * 包含了Lua内部使用的各种宏定义和类型定义
 */

/*
 * 字符属性位定义
 * 使用位标志(bit flags)来表示字符的不同属性
 * 每个属性占用一个二进制位,可以通过位运算组合多个属性
 */
#define ALPHABIT 0  /* 字母位: 表示字符是否为字母(包括'_') */
#define DIGITBIT 1  /* 数字位: 表示字符是否为数字0-9 */
#define PRINTBIT 2  /* 可打印位: 表示字符是否可打印 */
#define SPACEBIT 3  /* 空格位: 表示字符是否为空白字符(空格、制表符等) */
#define XDIGITBIT 4 /* 十六进制数字位: 表示字符是否为十六进制数字0-9,A-F,a-f */

#define MASK(B) (1 << (B))
/*
 * MASK宏: 将位位置转换为位掩码
 *
 * 工作原理:
 * - 左移运算符<<将1左移B位
 * - 例如: MASK(0) = 1 << 0 = 0b00001 (二进制1)
 *         MASK(1) = 1 << 1 = 0b00010 (二进制2)
 *         MASK(2) = 1 << 2 = 0b00100 (二进制4)
 *
 * 用途:
 * 创建只有特定位为1的掩码,用于测试或设置字符属性
 * 例如: MASK(ALPHABIT) = 0b00001,用于测试字母属性
 */

/*
** add 1 to char to allow index -1 (EOZ)
** 给char加1以允许索引-1 (EOZ - End Of Z/文件结束标记)
*/
#define testprop(c, p) (luai_ctype_[(c) + 1] & (p))
/*
 * testprop宏: 测试字符c是否具有属性p
 *
 * 参数:
 * - c: 要测试的字符(可以是-1到255的值)
 * - p: 属性掩码(由MASK宏生成)
 *
 * 工作原理:
 * 1. (c)+1: 将字符值加1,这样索引-1变成0,字符0变成1,以此类推
 *    这使得可以处理特殊值EOZ(-1,表示文件结束)
 * 2. luai_ctype_[...]: 访问字符属性查找表
 * 3. & (p): 与运算,测试指定的属性位是否被设置
 *
 * 返回值:
 * - 非0: 字符具有该属性
 * - 0: 字符不具有该属性
 *
 * 示例:
 * 假设'A'的属性字节为0b00011(字母+可打印)
 * testprop('A', MASK(ALPHABIT)) = luai_ctype_['A'+1] & 0b00001
 *                                = 0b00011 & 0b00001 = 0b00001 (真)
 */

/*
** 'lalpha' (Lua alphabetic) and 'lalnum' (Lua alphanumeric) both include '_'
** 'lalpha'(Lua字母)和'lalnum'(Lua字母数字)都包含'_'
*/
#define lislalpha(c) testprop(c, MASK(ALPHABIT))
/*
 * lislalpha宏: 判断字符是否为Lua字母
 *
 * Lua字母的定义:
 * - 标准字母 A-Z, a-z
 * - 下划线 '_'
 *
 * 这与标准C的isalpha不同,标准isalpha不包含下划线
 * 在Lua中,下划线可以用于标识符,因此被视为字母
 *
 * 使用场景:
 * - 解析Lua标识符(变量名、函数名等)
 * - 标识符可以以字母或下划线开头
 */

#define lislalnum(c) testprop(c, (MASK(ALPHABIT) | MASK(DIGITBIT)))
/*
 * lislalnum宏: 判断字符是否为Lua字母或数字
 *
 * 工作原理:
 * - MASK(ALPHABIT) | MASK(DIGITBIT): 位或运算,组合两个掩码
 *   结果为 0b00001 | 0b00010 = 0b00011
 * - 测试字符是否同时满足字母或数字属性
 *
 * Lua字母数字的定义:
 * - 字母: A-Z, a-z, _
 * - 数字: 0-9
 *
 * 使用场景:
 * - 解析Lua标识符的后续字符
 * - 标识符的第2个字符开始可以是字母、数字或下划线
 */

#define lisdigit(c) testprop(c, MASK(DIGITBIT))
/*
 * lisdigit宏: 判断字符是否为数字
 *
 * 判断字符是否为0-9
 * 用于解析数字字面量
 */

#define lisspace(c) testprop(c, MASK(SPACEBIT))
/*
 * lisspace宏: 判断字符是否为空白字符
 *
 * 空白字符包括:
 * - 空格 ' '
 * - 制表符 '\t'
 * - 换行符 '\n'
 * - 回车符 '\r'
 * - 换页符 '\f'
 * - 垂直制表符 '\v'
 *
 * 用于跳过Lua源代码中的空白
 */

#define lisprint(c) testprop(c, MASK(PRINTBIT))
/*
 * lisprint宏: 判断字符是否为可打印字符
 *
 * 可打印字符: ASCII码32-126的字符
 * 包括字母、数字、标点符号等可见字符
 * 不包括控制字符(如'\0', '\n'等)
 */

#define lisxdigit(c) testprop(c, MASK(XDIGITBIT))
/*
 * lisxdigit宏: 判断字符是否为十六进制数字
 *
 * 十六进制数字包括:
 * - 0-9
 * - A-F
 * - a-f
 *
 * 用于解析十六进制数字字面量,如0xFF
 */

/*
** In ASCII, this 'ltolower' is correct for alphabetic characters and
** for '.'. That is enough for Lua needs. ('check_exp' ensures that
** the character either is an upper-case letter or is unchanged by
** the transformation, which holds for lower-case letters and '.'.)
*/
/*
 * 在ASCII中,这个'ltolower'对于字母字符和'.'是正确的。
 * 这对于Lua的需求来说已经足够了。
 * ('check_exp'确保字符要么是大写字母,要么经过转换后保持不变,
 * 这对小写字母和'.'都成立。)
 */
#define ltolower(c)                                                   \
  check_exp(('A' <= (c) && (c) <= 'Z') || (c) == ((c) | ('A' ^ 'a')), \
            (c) | ('A' ^ 'a'))
/*
 * ltolower宏: 将大写字母转换为小写字母
 *
 * 工作原理 - 巧妙的位运算技巧:
 * 1. 'A' ^ 'a': 计算大小写字母之间的位差异
 *    在ASCII中: 'A' = 65 = 0b01000001
 *               'a' = 97 = 0b01100001
 *               'A' ^ 'a' = 0b00100000 = 32
 *    这个值正好是大小写字母ASCII码的差值
 *
 * 2. (c) | ('A' ^ 'a'): 将第6位设置为1
 *    对于大写字母: 'A' | 32 = 0b01000001 | 0b00100000 = 0b01100001 = 'a'
 *    对于小写字母: 'a' | 32 = 0b01100001 | 0b00100000 = 0b01100001 = 'a'(不变)
 *    对于'.': '.' | 32 = 0b00101110 | 0b00100000 = 0b00101110 = '.'(不变)
 *
 * 3. check_exp: 这是一个调试宏(定义在llimits.h中)
 *    第一个参数是断言条件,确保:
 *    - 要么c是大写字母('A' <= c && c <= 'Z')
 *    - 要么转换后c保持不变((c) == ((c) | ('A' ^ 'a')))
 *    第二个参数是实际的转换表达式
 *
 * 优点:
 * - 无分支,效率高(不需要if-else判断)
 * - 对已经是小写的字母也能正确处理
 * - 对'.'字符也适用(Lua在某些场景需要处理'.')
 *
 * 为什么只需要处理'.'?
 * - Lua在解析浮点数时需要处理小数点
 * - 这个转换可能用于大小写不敏感的比较
 */

/*
 * one entry for each character and for -1 (EOZ)
 * 为每个字符和-1(EOZ)各提供一个条目
 */
LUAI_DDEC(const lu_byte luai_ctype_[UCHAR_MAX + 2];)
/*
 * luai_ctype_: 字符属性查找表的声明
 *
 * LUAI_DDEC宏: (定义在llimits.h中)用于声明外部数据
 * - 在头文件中,它展开为: extern const lu_byte luai_ctype_[...];
 * - 在实现文件中(lctype.c),实际定义这个数组
 *
 * 数组大小: UCHAR_MAX + 2
 * - UCHAR_MAX: unsigned char的最大值,通常是255
 * - +2: 因为需要额外的空间
 *   - +1: 为了能够访问索引0(对应字符-1,即EOZ)
 *   - +1: 为了能够访问索引UCHAR_MAX+1(对应字符UCHAR_MAX)
 *
 * 数组内容:
 * - 每个元素是一个字节(lu_byte)
 * - 每个字节的不同位表示字符的不同属性
 * - 例如: luai_ctype_['A'+1] 可能是 0b00101 (字母+可打印)
 *
 * const修饰符: 表示这是只读数据,不应被修改
 *
 * EOZ (End Of Z):
 * - 值为-1
 * - 表示输入流的结束
 * - Lua使用-1而不是0('\0')作为结束标记
 */

#else /* }{ */
/*
 * ============================================================================
 * 标准C库实现部分
 * ============================================================================
 *
 * 这部分代码在LUA_USE_CTYPE为1时编译,即在非ASCII环境下使用
 * 直接使用标准C库的ctype函数
 */

/*
** use standard C ctypes
** 使用标准C ctype函数
*/

#include <ctype.h>
/*
 * ctype.h: 标准C库头文件
 * 提供字符分类和转换函数:
 * - isalpha(): 判断是否为字母
 * - isdigit(): 判断是否为数字
 * - isalnum(): 判断是否为字母或数字
 * - isspace(): 判断是否为空白字符
 * - isprint(): 判断是否为可打印字符
 * - isxdigit(): 判断是否为十六进制数字
 * - tolower(): 转换为小写
 *
 * 注意:
 * - 这些函数的行为可能受locale影响
 * - 性能可能不如查表法
 * - 但在非ASCII环境下是必需的
 */

#define lislalpha(c) (isalpha(c) || (c) == '_')
/*
 * 使用标准isalpha(),但额外判断下划线
 * 因为标准isalpha()不包含'_',但Lua需要将'_'视为字母
 *
 * 逻辑或运算符||:
 * - 如果isalpha(c)为真,返回真
 * - 否则检查c是否为'_'
 */

#define lislalnum(c) (isalnum(c) || (c) == '_')
/*
 * 使用标准isalnum(),但额外判断下划线
 * isalnum(): 判断是否为字母或数字
 */

#define lisdigit(c) (isdigit(c))
/*
 * 直接使用标准isdigit()
 * 数字的定义在各种字符集中都是一致的
 */

#define lisspace(c) (isspace(c))
/*
 * 直接使用标准isspace()
 */

#define lisprint(c) (isprint(c))
/*
 * 直接使用标准isprint()
 */

#define lisxdigit(c) (isxdigit(c))
/*
 * 直接使用标准isxdigit()
 */

#define ltolower(c) (tolower(c))
/*
 * 直接使用标准tolower()
 *
 * tolower()函数:
 * - 如果c是大写字母,返回对应的小写字母
 * - 否则返回c本身
 * - 返回值类型是int而不是char,以便处理特殊值
 */

#endif /* } */

#endif

/*
 * ============================================================================
 * 文件总结说明
 * ============================================================================
 *
 * 一、整体架构
 *
 * 本文件采用条件编译的方式,提供两种实现:
 *
 * 1. 自定义实现(ASCII环境,LUA_USE_CTYPE=0):
 *    - 使用位掩码技术
 *    - 使用预计算的查找表
 *    - 性能优秀,行为固定
 *    - 支持EOZ(-1)特殊值
 *
 * 2. 标准库实现(非ASCII环境,LUA_USE_CTYPE=1):
 *    - 使用标准C库的ctype函数
 *    - 兼容性好,但性能稍差
 *    - 行为可能受locale影响
 *
 *
 * 二、核心技术详解
 *
 * 1. 位掩码技术:
 *    - 使用单个字节的不同位来表示字符的多个属性
 *    - 每个位代表一种属性(字母、数字、空格等)
 *    - 通过位运算(&, |)快速判断和组合属性
 *    - 优点: 节省内存,速度快,可以同时测试多个属性
 *
 *    示例:
 *    字符'A'的属性字节: 0b00101
 *    - 第0位(ALPHABIT): 1,表示是字母
 *    - 第1位(DIGITBIT): 0,表示不是数字
 *    - 第2位(PRINTBIT): 1,表示可打印
 *    - 第3位(SPACEBIT): 0,表示不是空格
 *    - 第4位(XDIGITBIT): 0,表示不是十六进制数字
 *
 * 2. 查找表优化:
 *    - luai_ctype_数组存储所有可能字符的属性
 *    - 数组索引即字符值(经过+1偏移)
 *    - O(1)时间复杂度,无需任何计算
 *    - 预计算所有结果,空间换时间
 *
 * 3. EOZ处理:
 *    - EOZ(End Of Z)值为-1,表示输入结束
 *    - 通过+1偏移,使-1映射到数组索引0
 *    - 允许统一处理正常字符和结束标记
 *
 * 4. 大小写转换的位运算技巧:
 *    - 利用ASCII编码的特点
 *    - 大小写字母只有第6位不同
 *    - 通过位或运算设置第6位实现转换
 *    - 无需条件判断,效率极高
 *
 *
 * 三、与标准C库的差异
 *
 * 1. 下划线处理:
 *    - 标准: isalpha('_') = 0, isalnum('_') = 0
 *    - Lua: lislalpha('_') = 1, lislalnum('_') = 1
 *    - 原因: Lua允许标识符包含下划线
 *
 * 2. 行为确定性:
 *    - 标准: 可能受locale影响,行为不确定
 *    - Lua: 在ASCII环境下行为完全固定
 *    - 原因: Lua需要一致的解析行为
 *
 * 3. 性能:
 *    - 标准: 函数调用开销
 *    - Lua: 直接查表,更快
 *
 *
 * 四、使用场景
 *
 * 1. 词法分析:
 *    - 解析标识符: lislalpha(第一个字符), lislalnum(后续字符)
 *    - 解析数字: lisdigit, lisxdigit
 *    - 跳过空白: lisspace
 *
 * 2. 字符串处理:
 *    - 大小写转换: ltolower
 *    - 验证可打印性: lisprint
 *
 * 3. 错误处理:
 *    - 检测文件结束: 通过EOZ(-1)
 *
 *
 * 五、设计优点
 *
 * 1. 高性能:
 *    - 查表比函数调用快
 *    - 位运算比多次比较快
 *    - 无分支预测开销
 *
 * 2. 可移植性:
 *    - 自动适配不同字符集
 *    - ASCII下优化,其他环境下兼容
 *
 * 3. 可维护性:
 *    - 宏定义统一接口
 *    - 实现细节隐藏
 *    - 易于理解和修改
 *
 * 4. 特殊需求:
 *    - 支持Lua语言的特殊定义(如'_'是字母)
 *    - 支持EOZ特殊值
 *    - 行为确定,不受环境影响
 *
 *
 * 六、学习要点
 *
 * 1. C语言技巧:
 *    - 条件编译(#if, #else, #endif)
 *    - 宏定义和宏函数
 *    - 位运算技巧
 *    - 外部变量声明
 *
 * 2. 优化技术:
 *    - 查找表
 *    - 位掩码
 *    - 空间换时间
 *    - 消除分支
 *
 * 3. 系统编程:
 *    - 字符集处理
 *    - 跨平台兼容性
 *    - 性能优先的设计
 *
 *
 * 七、相关文件
 *
 * - lctype.c: 本头文件对应的实现文件,定义luai_ctype_数组
 * - llimits.h: 包含LUAI_DDEC、check_exp等宏定义
 * - lua.h: Lua主头文件
 *
 *
 * 八、实际应用示例
 *
 * 在Lua解析器中解析标识符的伪代码:
 *
 * // 第一个字符必须是字母或下划线
 * if (lislalpha(c)) {
 *     // 后续字符可以是字母、数字或下划线
 *     while (lislalnum(c)) {
 *         // 读取标识符字符
 *         c = next_char();
 *     }
 * }
 *
 * 解析十六进制数字:
 *
 * if (c == '0' && (next == 'x' || next == 'X')) {
 *     // 十六进制数字
 *     while (lisxdigit(c)) {
 *         // 处理0-9, A-F, a-f
 *         c = next_char();
 *     }
 * }
 *
 * ============================================================================
 */
