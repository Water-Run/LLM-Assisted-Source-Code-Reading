/*
** ============================================================================
** 文件概要说明
** ============================================================================
** 文件名: lobject.c
** 版本: $Id: lobject.c $
**
** 功能概述:
** 这个文件实现了Lua对象系统的一些通用功能函数。主要包括:
** 1. 数学运算相关: 对数计算、浮点参数编码/解码、算术运算
** 2. 字符串到数字的转换: 支持十进制、十六进制、整数、浮点数
** 3. 数字到字符串的转换: 整数和浮点数的格式化输出
** 4. UTF-8编码处理
** 5. 格式化字符串输出(类似printf)
** 6. 源代码块标识符生成(用于错误信息)
**
** 版权说明见 lua.h
** ============================================================================
*/

/*
** Lua对象上的一些通用函数
** 参见lua.h中的版权声明
*/

/*
** 定义宏标识: 表明这是lobject.c模块的实现文件
** 这个宏用于条件编译,确保某些定义只在特定文件中生效
*/
#define lobject_c

/*
** 定义宏标识: 表明这是Lua核心代码的一部分
** 这会影响某些头文件的行为,启用核心API而非公共API
*/
#define LUA_CORE

/*
** 包含Lua的前缀配置头文件
** 这个文件通常包含平台特定的配置和宏定义
*/
#include "lprefix.h"

/*
** 标准C库头文件
** float.h - 浮点数类型的限制和特性(如FLT_MAX, DBL_DIG等)
** locale.h - 本地化设置(如小数点符号)
** math.h - 数学函数(如ldexp用于指数操作)
** stdarg.h - 可变参数列表处理(va_list, va_start, va_end)
** stdio.h - 标准输入输出(sprintf等)
** stdlib.h - 通用工具函数(如内存分配)
** string.h - 字符串处理函数(strcpy, strlen, memcpy等)
*/
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** Lua核心头文件
** lua.h - Lua的主要API定义
*/
#include "lua.h"

/*
** Lua内部头文件
** lctype.h - 字符类型判断(如lisdigit判断是否为数字)
** ldebug.h - 调试相关功能
** ldo.h - 函数调用和栈管理
** lmem.h - 内存管理
** lobject.h - 对象系统定义(本文件的头文件)
** lstate.h - Lua状态机定义
** lstring.h - 字符串处理
** lvm.h - 虚拟机指令执行
*/
#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"

/*
** ============================================================================
** 函数: luaO_ceillog2
** ============================================================================
** 功能: 计算ceil(log2(x)),即使得 x <= (1 << n) 成立的最小整数n
**
** 参数:
**   x - 无符号整数,需要计算其以2为底的对数上限
**
** 返回值:
**   lu_byte类型(无符号字节),表示计算结果
**
** 实现原理:
**   使用查找表优化。表log_2[i-1]存储ceil(log2(i))的值。
**   对于大于255的数,通过右移8位(除以256)来减小数值,
**   每次右移增加8到结果,直到x小于256可以查表。
**
** 使用场景:
**   常用于哈希表大小计算,需要找到大于等于某个值的2的幂次
*/
/*
** 计算 ceil(log2(x)),即使得 x <= (1 << n) 成立的最小整数 n
*/
lu_byte luaO_ceillog2(unsigned int x)
{
  /*
  ** 静态查找表: log_2[i - 1] = ceil(log2(i))
  ** 存储1到256的对数值,避免重复计算
  ** 例如: log_2[0]=ceil(log2(1))=0, log_2[1]=ceil(log2(2))=1
  **       log_2[2]=log_2[3]=ceil(log2(3))=ceil(log2(4))=2
  */
  static const lu_byte log_2[256] = {/* log_2[i - 1] = ceil(log2(i)) */
                                     0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                                     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                                     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
                                     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
  int l = 0; /* 累积的对数值 */
  x--;       /* 减1是因为要计算ceil而不是floor,例如log2(8)=3但我们要返回3而不是4 */
  /*
  ** 处理大数: 当x >= 256时,不能直接查表
  ** 每次右移8位(相当于除以256),对数值增加8
  ** 例如: x=1000时,循环一次后x=3(1000>>8=3),l=8
  */
  while (x >= 256)
  {
    l += 8;
    x >>= 8;
  }
  /*
  ** 最终查表: 此时x < 256,可以直接使用查找表
  ** cast_byte是类型转换宏,确保结果是lu_byte类型
  */
  return cast_byte(l + log_2[x]);
}

/*
** ============================================================================
** 函数: luaO_codeparam
** ============================================================================
** 功能: 将百分比参数p编码为浮点字节表示,格式为(eeeexxxx)
**
** 参数:
**   p - 无符号整数,表示百分比值
**
** 返回值:
**   lu_byte类型,编码后的浮点字节
**
** 编码格式说明:
**   采用类似IEEE 754的格式:
**   - 高4位(eeee): 指数,使用excess-7表示法(实际指数=eeee-7)
**   - 低4位(xxxx): 尾数
**   - 归一化数: 值=(1.xxxx) * 2^(eeee-7-1), 当eeee!=0时
**   - 非归一化数: 值=(0.xxxx) * 2^(-7), 当eeee=0时
**
** 实现原理:
**   1. 先将百分比转换为1/128的单位(乘128除100并向上取整)
**   2. 如果结果<0x10(16),作为非归一化数直接返回
**   3. 否则,提取高5位作为尾数,计算指数并编码
*/
/*
** 将 'p'% 编码为浮点字节,表示为 (eeeexxxx)
** 指数用excess-7表示。模仿IEEE 754,当可能时归一化数字,
** 假设尾数(xxxx)前有一个额外的1,并将指数(eeee)加1以表示这一点。
** 因此,真实值为 (1xxxx) * 2^(eeee - 7 - 1) 当 eeee != 0,
** 否则为 (xxxx) * 2^-7 (非归一化数)
*/
lu_byte luaO_codeparam(unsigned int p)
{
  /*
  ** 溢出检查: 计算最大可表示的百分比
  ** 0x1F是最大尾数(31), 0xF-7-1=7是最大指数
  ** 所以最大值是31 * 2^7 * 100 = 396800
  */
  if (p >= (cast(lu_mem, 0x1F) << (0xF - 7 - 1)) * 100u) /* overflow? 溢出? */
    return 0xFF;                                         /* return maximum value 返回最大值 */
  else
  {
    /*
    ** 转换百分比到内部表示:
    ** 乘128(左移7位相当于除以100/128约等于0.78125)
    ** 加99用于向上取整: (p*128+99)/100 = ceil(p*128/100)
    ** 例如: p=50时, (50*128+99)/100 = 6499/100 = 64
    */
    p = (cast(l_uint32, p) * 128 + 99) / 100; /* round up the division 向上取整除法 */
    if (p < 0x10)
    { /* subnormal number? 非归一化数? */
      /* exponent bits are already zero; nothing else to do 指数位已经是0;无需其他操作 */
      return cast_byte(p);
    }
    else
    { /* p >= 0x10 implies ceil(log2(p + 1)) >= 5 */
      /* preserve 5 bits in 'p' 保留p的5位 */
      /*
      ** 归一化处理:
      ** 1. 计算需要右移的位数log,使得p右移后剩余5位有效数字
      ** 2. (p >> log) - 0x10: 右移后减去隐含的1(因为归一化假设最高位是1)
      ** 3. (log + 1) << 4: 指数部分,加1是因为excess-7且归一化
      ** 例如: p=64(0x40)时, log=2, 结果=(64>>2-16)|(3<<4)=0x30
      */
      unsigned log = luaO_ceillog2(p + 1) - 5u;
      return cast_byte(((p >> log) - 0x10) | ((log + 1) << 4));
    }
  }
}

/*
** ============================================================================
** 函数: luaO_applyparam
** ============================================================================
** 功能: 计算 p * x,其中p是浮点字节编码的参数
**
** 参数:
**   p - 编码的浮点字节参数(由luaO_codeparam生成)
**   x - 要相乘的内存大小值
**
** 返回值:
**   l_mem类型,计算结果(可能被限制为MAX_LMEM)
**
** 实现原理:
**   1. 解码p得到尾数m和指数e
**   2. 根据指数正负选择不同的乘法顺序以避免溢出或保持精度
**   3. 正指数: 先乘后移,检查溢出
**   4. 负指数: 比较先乘后移和先移后乘,选择不溢出且精度高的方式
*/
/*
** 计算 'p' 乘以 'x',其中 'p' 是浮点字节。大致上,
** 我们需要将 'x' 乘以尾数,然后根据指数进行相应的移位。
** 如果指数是正的,乘法和移位都会增大 'x',所以只需关心溢出。
** 但对于负指数,在移位前乘法可以保留更多有效位,
** 只要乘法不溢出,所以我们检查哪种顺序更好。
*/
l_mem luaO_applyparam(lu_byte p, l_mem x)
{
  int m = p & 0xF;  /* mantissa 尾数:提取低4位 */
  int e = (p >> 4); /* exponent 指数:提取高4位 */
  if (e > 0)
  {            /* normalized? 归一化数? */
    e--;       /* correct exponent 修正指数(因为excess-7且归一化加了1) */
    m += 0x10; /* correct mantissa; maximum value is 0x1F 修正尾数;加上隐含的1,最大值为0x1F(31) */
  }
  e -= 7; /* correct excess-7 修正excess-7偏移 */
  /*
  ** 正指数情况: 需要左移,值会变大
  */
  if (e >= 0)
  {
    /*
    ** 溢出检查: x必须小于 (MAX_LMEM / 0x1F) >> e
    ** 这样 (x * m) << e 才不会溢出
    */
    if (x < (MAX_LMEM / 0x1F) >> e) /* no overflow? 无溢出? */
      return (x * m) << e;          /* order doesn't matter here 这里顺序无关紧要 */
    else                            /* real overflow 真正的溢出 */
      return MAX_LMEM;
  }
  else
  {         /* negative exponent 负指数 */
    e = -e; /* 转为正数便于右移操作 */
    /*
    ** 策略1: 先乘后移
    ** 优点: 保留更多精度
    ** 前提: 乘法不能溢出
    */
    if (x < MAX_LMEM / 0x1F) /* multiplication cannot overflow? 乘法不会溢出? */
      return (x * m) >> e;   /* multiplying first gives more precision 先乘提供更多精度 */
    /*
    ** 策略2: 先移后乘
    ** 当先乘会溢出但先移后乘不会溢出时使用
    */
    else if ((x >> e) < MAX_LMEM / 0x1F) /* cannot overflow after shift? 移位后不会溢出? */
      return (x >> e) * m;
    else /* real overflow 真正的溢出 */
      return MAX_LMEM;
  }
}

/*
** ============================================================================
** 函数: intarith
** ============================================================================
** 功能: 执行整数算术运算
**
** 参数:
**   L - Lua状态机指针
**   op - 运算符(LUA_OPADD等)
**   v1, v2 - 两个整数操作数
**
** 返回值:
**   运算结果(lua_Integer类型)
**
** 支持的运算:
**   加减乘、取模、整除、位运算(与或异或)、移位、取负、按位取反
**
** 注意:
**   intop是一个宏,用于包装运算符并处理可能的溢出
**   某些运算(如取模、整除、移位)调用专门的函数处理
*/
/*
** 整数算术运算
** 根据操作符op,对两个整数v1和v2执行相应的算术或位运算
*/
static lua_Integer intarith(lua_State *L, int op, lua_Integer v1,
                            lua_Integer v2)
{
  switch (op)
  {
  case LUA_OPADD:
    return intop(+, v1, v2); /* 加法: v1 + v2 */
  case LUA_OPSUB:
    return intop(-, v1, v2); /* 减法: v1 - v2 */
  case LUA_OPMUL:
    return intop(*, v1, v2); /* 乘法: v1 * v2 */
  /*
  ** 取模运算: 调用luaV_mod处理,因为C的%运算符对负数的行为与Lua定义不同
  ** Lua要求结果符号与除数相同
  */
  case LUA_OPMOD:
    return luaV_mod(L, v1, v2);
  /*
  ** 整除运算: 调用luaV_idiv处理,向负无穷取整而不是向零取整
  */
  case LUA_OPIDIV:
    return luaV_idiv(L, v1, v2);
  case LUA_OPBAND:
    return intop(&, v1, v2); /* 按位与: v1 & v2 */
  case LUA_OPBOR:
    return intop(|, v1, v2); /* 按位或: v1 | v2 */
  case LUA_OPBXOR:
    return intop(^, v1, v2); /* 按位异或: v1 ^ v2 */
  /*
  ** 左移运算: 调用luaV_shiftl处理,因为C的<<对负数和大移位量行为未定义
  */
  case LUA_OPSHL:
    return luaV_shiftl(v1, v2);
  /*
  ** 右移运算: 调用luaV_shiftr,使用逻辑右移(高位补0)
  */
  case LUA_OPSHR:
    return luaV_shiftr(v1, v2);
  case LUA_OPUNM:
    return intop(-, 0, v1); /* 取负: -v1 */
  /*
  ** 按位取反: 使用异或实现 ~v1 = v1 ^ 0xFFFFFFFF...
  ** l_castS2U将有符号数转为无符号数以避免符号扩展问题
  */
  case LUA_OPBNOT:
    return intop(^, ~l_castS2U(0), v1);
  default:
    lua_assert(0);
    return 0; /* 不应到达这里,断言失败 */
  }
}

/*
** ============================================================================
** 函数: numarith
** ============================================================================
** 功能: 执行浮点数算术运算
**
** 参数:
**   L - Lua状态机指针
**   op - 运算符
**   v1, v2 - 两个浮点数操作数
**
** 返回值:
**   运算结果(lua_Number类型)
**
** 支持的运算:
**   加减乘除、幂运算、整除、取模、取负
**
** 注意:
**   luai_num系列宏可能包含浮点异常检查等平台特定处理
*/
/*
** 浮点数算术运算
** 对两个浮点数v1和v2执行相应的算术运算
*/
static lua_Number numarith(lua_State *L, int op, lua_Number v1,
                           lua_Number v2)
{
  switch (op)
  {
  /*
  ** luai_numadd等宏可能包含特殊处理:
  ** - 浮点异常检查
  ** - 特殊值(NaN, Inf)处理
  ** - 平台特定的优化
  */
  case LUA_OPADD:
    return luai_numadd(L, v1, v2); /* 加法 */
  case LUA_OPSUB:
    return luai_numsub(L, v1, v2); /* 减法 */
  case LUA_OPMUL:
    return luai_nummul(L, v1, v2); /* 乘法 */
  case LUA_OPDIV:
    return luai_numdiv(L, v1, v2); /* 除法 */
  case LUA_OPPOW:
    return luai_numpow(L, v1, v2); /* 幂运算: v1^v2 */
  case LUA_OPIDIV:
    return luai_numidiv(L, v1, v2); /* 浮点整除 */
  case LUA_OPUNM:
    return luai_numunm(L, v1); /* 取负: -v1 */
  /*
  ** 浮点取模: 调用luaV_modf,实现 v1 - floor(v1/v2)*v2
  */
  case LUA_OPMOD:
    return luaV_modf(L, v1, v2);
  default:
    lua_assert(0);
    return 0;
  }
}

/*
** ============================================================================
** 函数: luaO_rawarith
** ============================================================================
** 功能: 执行原始算术运算(不触发元方法)
**
** 参数:
**   L - Lua状态机指针
**   op - 运算符
**   p1, p2 - 两个操作数(TValue类型)
**   res - 存储结果的位置
**
** 返回值:
**   1表示成功,0表示失败(类型不匹配等)
**
** 运算规则:
**   1. 位运算只对整数有效
**   2. 除法和幂运算只对浮点数有效
**   3. 其他运算: 两个整数用整数运算,否则转浮点数运算
*/
/*
** 原始算术运算(不使用元方法)
** 根据操作数类型和运算符选择合适的运算方式
*/
int luaO_rawarith(lua_State *L, int op, const TValue *p1, const TValue *p2,
                  TValue *res)
{
  switch (op)
  {
  /*
  ** 位运算组: 只对整数操作
  */
  case LUA_OPBAND:
  case LUA_OPBOR:
  case LUA_OPBXOR:
  case LUA_OPSHL:
  case LUA_OPSHR:
  case LUA_OPBNOT:
  { /* operate only on integers 仅对整数操作 */
    lua_Integer i1;
    lua_Integer i2;
    /*
    ** tointegerns尝试将TValue转换为整数
    ** 如果p1和p2都能转换为整数,执行整数运算
    */
    if (tointegerns(p1, &i1) && tointegerns(p2, &i2))
    {
      setivalue(res, intarith(L, op, i1, i2)); /* 设置结果为整数 */
      return 1;                                /* 成功 */
    }
    else
      return 0; /* fail 失败:操作数不是整数 */
  }
  /*
  ** 浮点运算组: 只对浮点数操作
  */
  case LUA_OPDIV:
  case LUA_OPPOW:
  { /* operate only on floats 仅对浮点数操作 */
    lua_Number n1;
    lua_Number n2;
    /*
    ** tonumberns尝试将TValue转换为浮点数
    ** 注意: 这是一个宏,所以n1和n2直接被赋值
    */
    if (tonumberns(p1, n1) && tonumberns(p2, n2))
    {
      setfltvalue(res, numarith(L, op, n1, n2)); /* 设置结果为浮点数 */
      return 1;
    }
    else
      return 0; /* fail 失败 */
  }
  /*
  ** 混合运算组: 可以对整数或浮点数操作
  */
  default:
  { /* other operations 其他运算 */
    lua_Number n1;
    lua_Number n2;
    /*
    ** 优先使用整数运算:
    ** 如果两个操作数都是整数,直接用整数运算,效率更高
    */
    if (ttisinteger(p1) && ttisinteger(p2))
    {
      setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
      return 1;
    }
    /*
    ** 否则尝试转换为浮点数运算
    ** tonumberns可以将整数或浮点数转换为浮点数
    */
    else if (tonumberns(p1, n1) && tonumberns(p2, n2))
    {
      setfltvalue(res, numarith(L, op, n1, n2));
      return 1;
    }
    else
      return 0; /* fail 失败:无法转换为数字 */
  }
  }
}

/*
** ============================================================================
** 函数: luaO_arith
** ============================================================================
** 功能: 执行算术运算(支持元方法)
**
** 参数:
**   L - Lua状态机指针
**   op - 运算符
**   p1, p2 - 两个操作数
**   res - 栈上结果位置
**
** 说明:
**   这是对外的算术运算接口。先尝试原始运算,失败则尝试元方法
*/
/*
** 算术运算(带元方法支持)
** 先尝试原始运算,失败则查找并调用元方法
*/
void luaO_arith(lua_State *L, int op, const TValue *p1, const TValue *p2,
                StkId res)
{
  /*
  ** s2v宏将栈索引转换为TValue指针
  */
  if (!luaO_rawarith(L, op, p1, p2, s2v(res)))
  {
    /* could not perform raw operation; try metamethod 无法执行原始运算;尝试元方法 */
    /*
    ** luaT_trybinTM查找并调用二元运算的元方法
    ** TMS是元方法类型枚举,从TM_ADD开始对应LUA_OPADD等
    ** cast(TMS, (op - LUA_OPADD) + TM_ADD)将运算符转换为对应的元方法类型
    */
    luaT_trybinTM(L, p1, p2, res, cast(TMS, (op - LUA_OPADD) + TM_ADD));
  }
}

/*
** ============================================================================
** 函数: luaO_hexavalue
** ============================================================================
** 功能: 将十六进制字符转换为对应的数值
**
** 参数:
**   c - 十六进制字符('0'-'9', 'a'-'f', 'A'-'F')
**
** 返回值:
**   对应的数值(0-15)
**
** 说明:
**   lisxdigit用于断言c是有效的十六进制字符
**   lisdigit判断是否为数字字符
**   ltolower将大写转为小写
*/
/*
** 将十六进制字符转换为数值
*/
lu_byte luaO_hexavalue(int c)
{
  lua_assert(lisxdigit(c)); /* 断言c是有效的十六进制字符 */
  /*
  ** 如果是数字字符'0'-'9',直接减去'0'得到0-9
  */
  if (lisdigit(c))
    return cast_byte(c - '0');
  /*
  ** 否则是字母'a'-'f'或'A'-'F'
  ** ltolower转为小写,然后减去'a'得到0-5,再加10得到10-15
  */
  else
    return cast_byte((ltolower(c) - 'a') + 10);
}

/*
** ============================================================================
** 函数: isneg
** ============================================================================
** 功能: 检查并跳过数字字符串开头的符号
**
** 参数:
**   s - 字符串指针的指针(用于修改指针位置)
**
** 返回值:
**   1表示负数(有'-'号),0表示非负数
**
** 副作用:
**   如果有符号('+' 或 '-'),*s会前进一个字符
*/
/*
** 检查数字符号
** 如果有'-'号返回1并跳过,如果有'+'号只跳过,否则返回0
*/
static int isneg(const char **s)
{
  if (**s == '-')
  {
    (*s)++;
    return 1;
  } /* 负数:跳过'-'并返回1 */
  else if (**s == '+')
    (*s)++; /* 正数:只跳过'+' */
  return 0; /* 没有符号 */
}

/*
** ============================================================================
** Lua的 'lua_strx2number' 实现
** ============================================================================
** 这一节实现了十六进制字符串到浮点数的转换
** 遵循C99标准中strtod的规范
*/
/*
** {==================================================================
** Lua对'lua_strx2number'的实现
** ===================================================================
*/

#if !defined(lua_strx2number)

/*
** 最大有效数字位数(避免即使是单精度浮点数也溢出)
** 限制为30位可以安全地处理大部分浮点数
*/
/* maximum number of significant digits to read (to avoid overflows
   even with single floats) */
#define MAXSIGDIG 30

/*
** ============================================================================
** 函数: lua_strx2number
** ============================================================================
** 功能: 将十六进制数字字符串转换为浮点数
**
** 参数:
**   s - 输入字符串
**   endptr - 输出参数,指向转换结束的位置
**
** 返回值:
**   转换得到的浮点数
**
** 格式:
**   [空格] [+/-] 0x 十六进制数字[.十六进制数字] [p[+/-]十进制指数]
**   例如: "0x1.8p3" 表示 1.5 * 2^3 = 12.0
**
** 实现细节:
**   1. 跳过前导空格和符号
**   2. 检查"0x"前缀
**   3. 读取十六进制数字,记录小数点位置
**   4. 读取可选的p指数部分
**   5. 使用ldexp进行指数运算
*/
/*
** 根据C99规范将十六进制数字字符串转换为数字
*/
static lua_Number lua_strx2number(const char *s, char **endptr)
{
  int dot = lua_getlocaledecpoint(); /* 获取当前locale的小数点字符(通常是'.') */
  lua_Number r = l_mathop(0.0);      /* result (accumulator) 结果(累加器),初始化为0.0 */
  int sigdig = 0;                    /* number of significant digits 有效数字个数 */
  int nosigdig = 0;                  /* number of non-significant digits 非有效数字个数(前导0) */
  int e = 0;                         /* exponent correction 指数修正值 */
  int neg;                           /* 1 if number is negative 如果数字是负数则为1 */
  int hasdot = 0;                    /* true after seen a dot 看到小数点后为true */
  *endptr = cast_charp(s);           /* nothing is valid yet 目前还没有有效内容 */
  /*
  ** 跳过前导空格
  ** lisspace检查字符是否为空白字符
  ** cast_uchar确保字符被当作无符号值处理(避免负数索引问题)
  */
  while (lisspace(cast_uchar(*s)))
    s++;           /* skip initial spaces 跳过开头的空格 */
  neg = isneg(&s); /* check sign 检查符号 */
  /*
  ** 检查"0x"或"0X"前缀
  ** 十六进制数必须以0x开头
  */
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X'))) /* check '0x' */
    return l_mathop(0.0);                                   /* invalid format (no '0x') 无效格式(没有'0x') */
  /*
  ** 主循环: 读取十六进制数字
  */
  for (s += 2;; s++)
  { /* skip '0x' and read numeral 跳过'0x'并读取数字 */
    /*
    ** 处理小数点
    */
    if (*s == dot)
    {
      if (hasdot)
        break; /* second dot? stop loop 第二个小数点?停止循环 */
      else
        hasdot = 1;
    }
    /*
    ** 处理十六进制数字
    */
    else if (lisxdigit(cast_uchar(*s)))
    {
      /*
      ** 前导0不算有效数字
      */
      if (sigdig == 0 && *s == '0') /* non-significant digit (zero)? 非有效数字(零)? */
        nosigdig++;
      /*
      ** 累加有效数字
      ** 限制最多读取MAXSIGDIG位以避免溢出
      */
      else if (++sigdig <= MAXSIGDIG)                  /* can read it without overflow? 可以无溢出地读取? */
        r = (r * l_mathop(16.0)) + luaO_hexavalue(*s); /* r = r*16 + 当前数字 */
      else
        e++; /* too many digits; ignore, but still count for exponent 数字太多;忽略,但仍计入指数 */
      /*
      ** 小数部分的每一位使指数减1
      ** 例如: 0x1.8中的8在小数点后1位,相当于8*16^(-1)
      */
      if (hasdot)
        e--; /* decimal digit? correct exponent 小数位?修正指数 */
    }
    else
      break; /* neither a dot nor a digit 既不是点也不是数字 */
  }
  /*
  ** 如果没有读到任何数字,返回0
  */
  if (nosigdig + sigdig == 0) /* no digits? 没有数字? */
    return l_mathop(0.0);     /* invalid format 无效格式 */
  *endptr = cast_charp(s);    /* valid up to here 到这里为止都是有效的 */
  /*
  ** 指数转换: 十六进制每位对应2^4
  ** 所以总指数需要乘以4
  */
  e *= 4; /* each digit multiplies/divides value by 2^4 每个数字将值乘/除以2^4 */
  /*
  ** 处理p指数部分(如果存在)
  ** 格式: p[+/-]十进制数
  ** 例如: 0x1.8p3表示1.5 * 2^3
  */
  if (*s == 'p' || *s == 'P')
  {                   /* exponent part? 指数部分? */
    int exp1 = 0;     /* exponent value 指数值 */
    int neg1;         /* exponent sign 指数符号 */
    s++;              /* skip 'p' 跳过'p' */
    neg1 = isneg(&s); /* sign 符号 */
    /*
    ** p后面必须至少有一个数字
    */
    if (!lisdigit(cast_uchar(*s)))
      return l_mathop(0.0); /* invalid; must have at least one digit 无效;必须至少有一个数字 */
    /*
    ** 读取十进制指数
    */
    while (lisdigit(cast_uchar(*s))) /* read exponent 读取指数 */
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1)
      exp1 = -exp1;          /* 应用指数符号 */
    e += exp1;               /* 合并到总指数 */
    *endptr = cast_charp(s); /* valid up to here 到这里为止都是有效的 */
  }
  if (neg)
    r = -r; /* 应用数字符号 */
  /*
  ** ldexp(x, exp)计算 x * 2^exp
  ** 这是标准C库函数,定义在math.h中
  ** l_mathop是一个宏,可能添加平台特定的处理
  */
  return l_mathop(ldexp)(r, e);
}

#endif
/* }====================================================== */

/*
** 数字字符串的最大长度
** 用于临时缓冲区大小,避免栈溢出
*/
/* maximum length of a numeral to be converted to a number */
#if !defined(L_MAXLENNUM)
#define L_MAXLENNUM 200
#endif

/*
** ============================================================================
** 函数: l_str2dloc
** ============================================================================
** 功能: 在指定模式下将字符串转换为浮点数
**
** 参数:
**   s - 输入字符串
**   result - 输出参数,存储转换结果
**   mode - 转换模式('x'表示十六进制,'d'或其他表示十进制)
**
** 返回值:
**   成功时返回指向结束'\0'的指针,失败返回NULL
*/
/*
** 将字符串's'转换为Lua数字(放入'result')。失败时返回NULL,
** 成功时返回结束'\0'的地址。('mode' == 'x')表示十六进制数。
*/
static const char *l_str2dloc(const char *s, lua_Number *result, int mode)
{
  char *endptr;
  /*
  ** 根据mode选择转换函数:
  ** - 'x': 使用lua_strx2number处理十六进制
  ** - 其他: 使用lua_str2number处理十进制(可能是系统的strtod)
  */
  *result = (mode == 'x') ? lua_strx2number(s, &endptr) /* try to convert 尝试转换 */
                          : lua_str2number(s, &endptr);
  if (endptr == s)
    return NULL; /* nothing recognized? 没有识别到任何内容? */
  /*
  ** 跳过尾部空格
  ** 有效的数字字符串在数字后可以有空格,但不能有其他字符
  */
  while (lisspace(cast_uchar(*endptr)))
    endptr++; /* skip trailing spaces 跳过尾部空格 */
  /*
  ** 检查是否到达字符串末尾
  ** 只有整个字符串都被转换且没有多余字符才算成功
  */
  return (*endptr == '\0') ? endptr : NULL; /* OK iff no trailing chars OK当且仅当没有尾部字符 */
}

/*
** ============================================================================
** 函数: l_str2d
** ============================================================================
** 功能: 将字符串转换为浮点数,处理locale问题
**
** 参数:
**   s - 输入字符串
**   result - 输出参数,存储转换结果
**
** 返回值:
**   成功时返回指向结束位置的指针,失败返回NULL
**
** 处理逻辑:
**   1. 检查特殊字符(点、x、n等)确定转换模式
**   2. 拒绝'inf'和'nan'
**   3. 尝试直接转换
**   4. 如果失败,可能是locale的小数点不同,尝试替换后再转换
*/
/*
** 将字符串's'转换为Lua数字(放入'result'),处理当前locale。
** 此函数接受当前locale或点作为小数点标记。如果转换失败,
** 可能意味着数字有点但locale接受其他字符。在这种情况下,
** 代码将's'复制到缓冲区(因为's'是只读的),将点改为当前locale
** 的小数点标记,然后再次尝试转换。
** 变量'mode'检查字符串中的特殊字符:
** - 'n' 表示 'inf' 或 'nan' (应被拒绝)
** - 'x' 表示十六进制数
** - '.' 只是优化对常见情况的搜索(没有特殊字符)
*/
static const char *l_str2d(const char *s, lua_Number *result)
{
  const char *endptr;
  /*
  ** strpbrk在s中查找第一个出现的".xXnN"字符
  ** 返回指向该字符的指针,或NULL(如果未找到)
  */
  const char *pmode = strpbrk(s, ".xXnN"); /* look for special chars 查找特殊字符 */
  /*
  ** 确定转换模式:
  ** - 如果找到特殊字符,将其转为小写作为模式
  ** - 否则mode为0(普通十进制)
  */
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  /*
  ** 拒绝'inf'和'nan'
  ** Lua不允许这些特殊浮点值从字符串转换而来
  */
  if (mode == 'n') /* reject 'inf' and 'nan' 拒绝'inf'和'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode); /* try to convert 尝试转换 */
  /*
  ** 如果转换失败,可能是locale问题
  */
  if (endptr == NULL)
  {                                    /* failed? may be a different locale 失败?可能是不同的locale */
    char buff[L_MAXLENNUM + 1];        /* 临时缓冲区 */
    const char *pdot = strchr(s, '.'); /* 查找点 */
    /*
    ** 检查是否有点且字符串不太长
    */
    if (pdot == NULL || strlen(s) > L_MAXLENNUM)
      return NULL;   /* string too long or no dot; fail 字符串太长或没有点;失败 */
    strcpy(buff, s); /* copy string to buffer 复制字符串到缓冲区 */
    /*
    ** 将点替换为locale的小数点字符
    ** lua_getlocaledecpoint获取当前locale的小数点(如','在某些欧洲locale中)
    */
    buff[pdot - s] = lua_getlocaledecpoint(); /* correct decimal point 修正小数点 */
    endptr = l_str2dloc(buff, result, mode);  /* try again 再次尝试 */
    /*
    ** 如果成功,需要调整endptr使其相对于原始字符串s
    */
    if (endptr != NULL)
      endptr = s + (endptr - buff); /* make relative to 's' 使其相对于's' */
  }
  return endptr;
}

/*
** 整数转换的常量
** MAXBY10: LUA_MAXINTEGER / 10,用于检查乘以10是否溢出
** MAXLASTD: LUA_MAXINTEGER % 10,最后一位数字的最大值
*/
#define MAXBY10 cast(lua_Unsigned, LUA_MAXINTEGER / 10)
#define MAXLASTD cast_int(LUA_MAXINTEGER % 10)

/*
** ============================================================================
** 函数: l_str2int
** ============================================================================
** 功能: 将字符串转换为整数
**
** 参数:
**   s - 输入字符串
**   result - 输出参数,存储转换结果
**
** 返回值:
**   成功时返回指向结束位置的指针,失败返回NULL
**
** 支持格式:
**   - 十进制: [空格][+/-]数字
**   - 十六进制: [空格][+/-]0x十六进制数字
**
** 溢出处理:
**   检测溢出并返回NULL(而不是返回错误的值)
*/
/*
** 将字符串转换为整数
*/
static const char *l_str2int(const char *s, lua_Integer *result)
{
  lua_Unsigned a = 0; /* 使用无符号数累加,避免有符号溢出的未定义行为 */
  int empty = 1;      /* 标记是否读到了数字 */
  int neg;
  while (lisspace(cast_uchar(*s)))
    s++;           /* skip initial spaces 跳过开头空格 */
  neg = isneg(&s); /* 检查并跳过符号 */
  /*
  ** 检查十六进制前缀"0x"或"0X"
  */
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X'))
  {         /* hex? 十六进制? */
    s += 2; /* skip '0x' 跳过'0x' */
    /*
    ** 读取十六进制数字
    */
    for (; lisxdigit(cast_uchar(*s)); s++)
    {
      a = a * 16 + luaO_hexavalue(*s); /* 累加: a = a*16 + 当前数字 */
      empty = 0;
    }
  }
  else
  { /* decimal 十进制 */
    /*
    ** 读取十进制数字
    */
    for (; lisdigit(cast_uchar(*s)); s++)
    {
      int d = *s - '0'; /* 当前数字 */
      /*
      ** 溢出检查:
      ** 1. a >= MAXBY10: 下一次乘10可能溢出
      ** 2. a > MAXBY10: 肯定溢出
      ** 3. a == MAXBY10 && d > MAXLASTD + neg:
      **    最后一位超出范围(neg用于处理-LUA_MAXINTEGER-1的情况)
      */
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg)) /* overflow? 溢出? */
        return NULL;                                           /* do not accept it (as integer) 不接受(作为整数) */
      a = a * 10 + cast_uint(d);                               /* 累加: a = a*10 + d */
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s)))
    s++; /* skip trailing spaces 跳过尾部空格 */
  /*
  ** 验证结果:
  ** - empty: 没有读到数字
  ** - *s != '\0': 有多余字符
  */
  if (empty || *s != '\0')
    return NULL; /* something wrong in the numeral 数字有问题 */
  else
  {
    /*
    ** 转换为有符号整数:
    ** - 负数: 0 - a(使用补码表示)
    ** - 正数: a
    ** l_castU2S将无符号数转为有符号数
    */
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}

/*
** ============================================================================
** 函数: luaO_str2num
** ============================================================================
** 功能: 将字符串转换为Lua数字(整数或浮点数)
**
** 参数:
**   s - 输入字符串
**   o - 输出参数,存储转换结果(TValue类型)
**
** 返回值:
**   成功时返回字符串长度+1,失败返回0
**
** 转换优先级:
**   1. 先尝试转换为整数
**   2. 失败则尝试转换为浮点数
**   3. 都失败返回0
*/
/*
** 将字符串转换为数字
** 先尝试整数,再尝试浮点数
*/
size_t luaO_str2num(const char *s, TValue *o)
{
  lua_Integer i;
  lua_Number n;
  const char *e;
  /*
  ** 优先尝试整数转换
  ** 整数转换更快且精确
  */
  if ((e = l_str2int(s, &i)) != NULL)
  {                  /* try as an integer 尝试作为整数 */
    setivalue(o, i); /* 设置TValue为整数类型 */
  }
  /*
  ** 整数转换失败,尝试浮点数
  */
  else if ((e = l_str2d(s, &n)) != NULL)
  {                    /* else try as a float 否则尝试作为浮点数 */
    setfltvalue(o, n); /* 设置TValue为浮点数类型 */
  }
  else
    return 0; /* conversion failed 转换失败 */
  /*
  ** 计算转换的字符数:
  ** e指向转换结束位置(通常是'\0')
  ** ct_diff2sz将指针差转换为size_t
  ** 加1包括结束符
  */
  return ct_diff2sz(e - s) + 1; /* success; return string size 成功;返回字符串大小 */
}

/*
** ============================================================================
** 函数: luaO_utf8esc
** ============================================================================
** 功能: 将Unicode码点编码为UTF-8字节序列
**
** 参数:
**   buff - 输出缓冲区(至少UTF8BUFFSZ字节)
**   x - Unicode码点(最大0x7FFFFFFF)
**
** 返回值:
**   编码的字节数
**
** UTF-8编码规则:
**   - 0x00-0x7F: 1字节,直接表示
**   - 0x80-0x7FF: 2字节
**   - 0x800-0xFFFF: 3字节
**   - 0x10000-0x10FFFF: 4字节
**   续字节格式: 10xxxxxx
**   首字节根据长度有不同的前缀
**
** 特殊之处:
**   编码从缓冲区末尾开始,向前填充
*/
/*
** UTF-8转义序列编码
*/
int luaO_utf8esc(char *buff, l_uint32 x)
{
  int n = 1;                    /* number of bytes put in buffer (backwards) 放入缓冲区的字节数(反向) */
  lua_assert(x <= 0x7FFFFFFFu); /* 确保码点在有效范围内 */
  /*
  ** ASCII范围: 单字节编码
  */
  if (x < 0x80) /* ASCII? */
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else
  {                          /* need continuation bytes 需要续字节 */
    unsigned int mfb = 0x3f; /* maximum that fits in first byte 首字节能容纳的最大值 */
    do
    { /* add continuation bytes 添加续字节 */
      /*
      ** 续字节格式: 10xxxxxx
      ** x & 0x3f 提取低6位
      ** 0x80 | ... 设置高2位为10
      ** 从缓冲区末尾向前填充
      */
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;   /* remove added bits 移除已添加的位 */
      mfb >>= 1; /* now there is one less bit available in first byte 首字节可用位减1 */
    } while (x > mfb); /* still needs continuation byte? 还需要续字节? */
    /*
    ** 首字节格式:
    ** - 2字节: 110xxxxx
    ** - 3字节: 1110xxxx
    ** - 4字节: 11110xxx
    ** ~mfb << 1 生成前缀(如110, 1110等)
    */
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x); /* add first byte 添加首字节 */
  }
  return n;
}

/*
** ============================================================================
** 数字到字符串转换的缓冲区大小检查
** ============================================================================
** LUA_N2SBUFFSZ必须足够容纳:
** - 整数格式(LUA_INTEGER_FMT): long long最多19位+符号+'\0' = 21字节
** - 浮点格式(LUA_NUMBER_FMT): 符号+小数点+指数字母+指数符号+4位指数+'\0'+有效数字
**   有效数字约为DIG属性指定的位数
*/
/*
** 数字转字符串的缓冲区大小
** 'LUA_N2SBUFFSZ'必须足够容纳LUA_INTEGER_FMT和LUA_NUMBER_FMT。
** 对于long long int,这是19位加一个符号和一个最终'\0',总计21。
** 对于long double,可以到一个符号、点、指数字母、指数符号、
** 4个指数数字、最终'\0',加上有效数字,约为*_DIG属性。
*/
#if LUA_N2SBUFFSZ < (20 + l_floatatt(DIG))
#error "invalid value for LUA_N2SBUFFSZ"
#endif

/*
** ============================================================================
** 函数: tostringbuffFloat
** ============================================================================
** 功能: 将浮点数转换为字符串并添加到缓冲区
**
** 参数:
**   n - 浮点数
**   buff - 输出缓冲区
**
** 返回值:
**   字符串长度
**
** 策略:
**   1. 先用标准精度转换
**   2. 读回检查是否有精度损失
**   3. 如果有损失,用高精度重新转换
**   4. 如果结果看起来像整数(没有小数点和指数),添加".0"
**
** 这样做的原因:
**   - 避免不必要的长数字(如1.1转成"1.1000000000000001")
**   - 确保足够精度(读回后值不变)
**   - 明确区分整数和浮点数(1.0而不是1)
*/
/*
** 将浮点数转换为字符串,添加到缓冲区。首先尝试不太大的数字位数,
** 以避免噪声(例如,1.1变成"1.1000000000000001")。如果这会损失精度,
** 导致读回的结果是不同的数字,那么用额外的精度再次转换。
** 此外,如果数字看起来像整数(没有小数点或指数),则在末尾添加".0"。
*/
static int tostringbuffFloat(lua_Number n, char *buff)
{
  /* first conversion 第一次转换 */
  /*
  ** l_sprintf是sprintf的包装宏
  ** LUA_NUMBER_FMT是格式字符串(如"%.14g")
  ** LUAI_UACNUMBER可能添加类型转换以避免参数传递问题
  */
  int len = l_sprintf(buff, LUA_N2SBUFFSZ, LUA_NUMBER_FMT,
                      (LUAI_UACNUMBER)n);
  /*
  ** 读回检查: 将字符串转回数字,看是否相等
  */
  lua_Number check = lua_str2number(buff, NULL); /* read it back 读回 */
  if (check != n)
  { /* not enough precision? 精度不够? */
    /* convert again with more precision 用更高精度再次转换 */
    /*
    ** LUA_NUMBER_FMT_N是高精度格式字符串(如"%.17g")
    */
    len = l_sprintf(buff, LUA_N2SBUFFSZ, LUA_NUMBER_FMT_N,
                    (LUAI_UACNUMBER)n);
  }
  /* looks like an integer? 看起来像整数? */
  /*
  ** strspn返回buff中只包含"-0123456789"字符的前缀长度
  ** 如果这个长度等于整个字符串长度,说明没有小数点和指数
  */
  if (buff[strspn(buff, "-0123456789")] == '\0')
  {
    buff[len++] = lua_getlocaledecpoint(); /* 添加小数点 */
    buff[len++] = '0';                     /* adds '.0' to result 添加'.0' */
  }
  return len;
}

/*
** ============================================================================
** 函数: luaO_tostringbuff
** ============================================================================
** 功能: 将数字对象转换为字符串并添加到缓冲区
**
** 参数:
**   obj - 数字对象(TValue类型)
**   buff - 输出缓冲区
**
** 返回值:
**   字符串长度
*/
/*
** 将数字对象转换为字符串,添加到缓冲区
*/
unsigned luaO_tostringbuff(const TValue *obj, char *buff)
{
  int len;
  lua_assert(ttisnumber(obj)); /* 断言obj是数字类型 */
  /*
  ** 整数: 使用整数格式化
  ** lua_integer2str是整数转字符串的函数(可能是sprintf的包装)
  */
  if (ttisinteger(obj))
    len = lua_integer2str(buff, LUA_N2SBUFFSZ, ivalue(obj));
  /*
  ** 浮点数: 使用tostringbuffFloat
  */
  else
    len = tostringbuffFloat(fltvalue(obj), buff);
  lua_assert(len < LUA_N2SBUFFSZ); /* 确保没有缓冲区溢出 */
  return cast_uint(len);
}

/*
** ============================================================================
** 函数: luaO_tostring
** ============================================================================
** 功能: 将数字对象转换为Lua字符串对象,替换原对象
**
** 参数:
**   L - Lua状态机
**   obj - 要转换的数字对象(会被字符串对象替换)
**
** 说明:
**   这个函数会修改obj,将其从数字类型变为字符串类型
**   新字符串会在Lua的字符串池中创建
*/
/*
** 将数字对象转换为Lua字符串,替换'obj'的值
*/
void luaO_tostring(lua_State *L, TValue *obj)
{
  char buff[LUA_N2SBUFFSZ];                    /* 临时缓冲区 */
  unsigned len = luaO_tostringbuff(obj, buff); /* 转换到缓冲区 */
  /*
  ** luaS_newlstr创建新字符串对象:
  ** - 如果字符串已存在于字符串池,返回已有的
  ** - 否则分配新字符串并加入字符串池
  ** setsvalue设置obj为字符串类型并指向新字符串
  */
  setsvalue(L, obj, luaS_newlstr(L, buff, len));
}

/*
** ============================================================================
** 'luaO_pushvfstring' 实现
** ============================================================================
** 这一节实现了类似printf的格式化字符串功能
** 支持的格式: %d, %c, %f, %p, %s, %%, %I(lua_Integer), %U(UTF-8)
*/
/*
** {==================================================================
** 'luaO_pushvfstring'
** ===================================================================
*/

/*
** 缓冲区大小:
** 需要容纳LUA_IDSIZE + LUA_N2SBUFFSZ(标识符和数字)
** 加上基本消息的最小空间,以便'luaG_addinfo'可以直接在静态缓冲区上工作
*/
/*
** 'luaO_pushvfstring'使用的缓冲区空间大小。应该是
** (LUA_IDSIZE + LUA_N2SBUFFSZ) 加上基本消息的最小空间,
** 以便'luaG_addinfo'可以直接在静态缓冲区上工作。
*/
#define BUFVFS cast_uint(LUA_IDSIZE + LUA_N2SBUFFSZ + 95)

/*
** ============================================================================
** 结构: BuffFS
** ============================================================================
** 功能: 用于'luaO_pushvfstring'的缓冲区
**
** 成员:
**   L - Lua状态机指针
**   b - 当前缓冲区指针(可能指向space或动态分配的内存)
**   buffsize - 缓冲区总大小
**   blen - 缓冲区中已使用的长度
**   err - 错误标志(0=无错误, 1=内存错误, 2=缓冲区溢出)
**   space - 初始静态缓冲区(避免小字符串的动态分配)
*/
/*
** 'luaO_pushvfstring'使用的缓冲区。'err'表示构建结果时的错误
** (内存错误[1]或缓冲区溢出[2])。
*/
typedef struct BuffFS
{
  lua_State *L;
  char *b;            /* 缓冲区指针 */
  size_t buffsize;    /* 缓冲区大小 */
  size_t blen;        /* length of string in 'buff' buff中字符串的长度 */
  int err;            /* 错误代码 */
  char space[BUFVFS]; /* initial buffer 初始缓冲区 */
} BuffFS;

/*
** ============================================================================
** 函数: initbuff
** ============================================================================
** 功能: 初始化BuffFS结构
*/
/*
** 初始化缓冲区
*/
static void initbuff(lua_State *L, BuffFS *buff)
{
  buff->L = L;
  buff->b = buff->space; /* 初始使用静态缓冲区 */
  buff->buffsize = sizeof(buff->space);
  buff->blen = 0;
  buff->err = 0;
}

/*
** ============================================================================
** 函数: pushbuff
** ============================================================================
** 功能: 将BuffFS的最终结果推入栈
**
** 说明:
**   这个函数在保护模式下调用,可能抛出错误
**   根据err的值决定:
**   - err==1: 内存错误,抛出LUA_ERRMEM
**   - err==2: 长度溢出,添加"..."并创建字符串
**   - err==0: 正常创建字符串
*/
/*
** 从'luaO_pushvfstring'推送最终结果。此函数可能显式引发错误
** 或通过内存错误引发,所以必须在保护模式下运行。
*/
static void pushbuff(lua_State *L, void *ud)
{
  BuffFS *buff = cast(BuffFS *, ud);
  switch (buff->err)
  {
  case 1:                      /* memory error 内存错误 */
    luaD_throw(L, LUA_ERRMEM); /* 抛出内存错误 */
    break;
  case 2: /* length overflow: Add "..." at the end of result 长度溢出:在结果末尾添加"..." */
    /*
    ** 如果剩余空间不足3字节,覆盖最后3个字节
    */
    if (buff->buffsize - buff->blen < 3)
      strcpy(buff->b + buff->blen - 3, "..."); /* 'blen' must be > 3 'blen'必须>3 */
    else
    { /* there is enough space left for the "..." 有足够的空间留给"..." */
      strcpy(buff->b + buff->blen, "...");
      buff->blen += 3;
    }
    /* FALLTHROUGH */
  default:
  { /* no errors, but it can raise one creating the new string 没有错误,但创建新字符串时可能引发一个 */
    /*
    ** 创建Lua字符串对象并推入栈
    ** luaS_newlstr会分配内存,可能触发GC或内存错误
    */
    TString *ts = luaS_newlstr(L, buff->b, buff->blen);
    /*
    ** setsvalue2s设置栈顶为字符串
    ** L->top.p是栈顶指针
    */
    setsvalue2s(L, L->top.p, ts);
    L->top.p++; /* 栈顶上移 */
  }
  }
}

/*
** ============================================================================
** 函数: clearbuff
** ============================================================================
** 功能: 清理缓冲区并返回结果字符串
**
** 返回值:
**   成功时返回字符串内容,失败返回NULL(错误消息在栈顶)
**
** 说明:
**   调用pushbuff将结果推入栈
**   如果使用了动态缓冲区,释放它
*/
/*
** 清理缓冲区
*/
static const char *clearbuff(BuffFS *buff)
{
  lua_State *L = buff->L;
  const char *res;
  /*
  ** luaD_rawrunprotected在保护模式下运行pushbuff
  ** 如果有错误(如内存错误),返回错误代码
  */
  if (luaD_rawrunprotected(L, pushbuff, buff) != LUA_OK) /* errors? 错误? */
    res = NULL;                                          /* error message is on the top of the stack 错误消息在栈顶 */
  else
    /*
    ** 成功: 从栈顶获取字符串内容
    ** s2v将栈索引转为TValue指针
    ** tsvalue获取字符串对象
    ** getstr获取字符串内容
    */
    res = getstr(tsvalue(s2v(L->top.p - 1)));
  /*
  ** 如果使用了动态缓冲区,释放它
  */
  if (buff->b != buff->space)                   /* using dynamic buffer? 使用动态缓冲区? */
    luaM_freearray(L, buff->b, buff->buffsize); /* free it 释放它 */
  return res;
}

/*
** ============================================================================
** 函数: addstr2buff
** ============================================================================
** 功能: 将字符串添加到缓冲区
**
** 参数:
**   buff - BuffFS结构
**   str - 要添加的字符串
**   slen - 字符串长度
**
** 说明:
**   如果空间不足,尝试扩展缓冲区
**   如果扩展失败或超出限制,设置错误标志
*/
/*
** 将字符串添加到缓冲区
*/
static void addstr2buff(BuffFS *buff, const char *str, size_t slen)
{
  size_t left = buff->buffsize - buff->blen; /* space left in the buffer 缓冲区剩余空间 */
  if (buff->err)                             /* do nothing else after an error 错误后什么都不做 */
    return;
  /*
  ** 检查是否需要扩展缓冲区
  */
  if (slen > left)
  { /* new string doesn't fit into current buffer? 新字符串放不进当前缓冲区? */
    /*
    ** 溢出检查: 避免超过MAX_SIZE/2
    */
    if (slen > ((MAX_SIZE / 2) - buff->blen))
    {                                          /* overflow? 溢出? */
      memcpy(buff->b + buff->blen, str, left); /* copy what it can 复制能复制的 */
      buff->blen = buff->buffsize;
      buff->err = 2; /* doesn't add anything else 不再添加任何东西 */
      return;
    }
    else
    {
      size_t newsize = buff->buffsize + slen; /* limited to MAX_SIZE/2 限制在MAX_SIZE/2 */
      char *newb =
          (buff->b == buff->space) /* still using static space? 仍在使用静态空间? */
              /*
              ** 从静态缓冲区切换到动态缓冲区:
              ** luaM_reallocvector分配新内存
              ** 参数: L, 旧指针(NULL表示新分配), 旧大小, 新大小, 类型
              */
              ? luaM_reallocvector(buff->L, NULL, 0, newsize, char)
              /*
              ** 扩展现有动态缓冲区
              */
              : luaM_reallocvector(buff->L, buff->b, buff->buffsize, newsize,
                                   char);
      if (newb == NULL)
      {                /* allocation error? 分配错误? */
        buff->err = 1; /* signal a memory error 标记内存错误 */
        return;
      }
      /*
      ** 如果是第一次分配,需要复制静态缓冲区的内容
      */
      if (buff->b == buff->space)          /* new buffer (not reallocated)? 新缓冲区(不是重新分配)? */
        memcpy(newb, buff->b, buff->blen); /* copy previous content 复制之前的内容 */
      buff->b = newb;                      /* set new (larger) buffer... 设置新的(更大的)缓冲区... */
      buff->buffsize = newsize;            /* ...and its new size ...及其新大小 */
    }
  }
  /*
  ** 复制新内容到缓冲区
  */
  memcpy(buff->b + buff->blen, str, slen); /* copy new content 复制新内容 */
  buff->blen += slen;
}

/*
** ============================================================================
** 函数: addnum2buff
** ============================================================================
** 功能: 将数字添加到缓冲区
**
** 说明:
**   先转换为字符串,再调用addstr2buff
*/
/*
** 将数字添加到缓冲区
*/
static void addnum2buff(BuffFS *buff, TValue *num)
{
  char numbuff[LUA_N2SBUFFSZ];                    /* 数字转字符串的缓冲区 */
  unsigned len = luaO_tostringbuff(num, numbuff); /* 转换数字 */
  addstr2buff(buff, numbuff, len);                /* 添加到主缓冲区 */
}

/*
** ============================================================================
** 函数: luaO_pushvfstring
** ============================================================================
** 功能: 格式化字符串并推入栈(可变参数版本)
**
** 参数:
**   L - Lua状态机
**   fmt - 格式字符串
**   argp - 可变参数列表
**
** 返回值:
**   成功时返回字符串内容,失败返回NULL
**
** 支持的格式:
**   %s - 字符串
**   %c - 字符
**   %d - int
**   %I - lua_Integer
**   %f - lua_Number
**   %p - 指针
**   %U - UTF-8字符
**   %% - 字面'%'
**
** 实现:
**   使用BuffFS管理缓冲区,支持动态扩展
**   处理过程中的错误(内存错误、溢出)
*/
/*
** 此函数只处理'%d'、'%c'、'%f'、'%p'、'%s'和'%%'常规格式,
** 加上Lua特定的'%I'和'%U'
*/
/*
** this function handles only '%d', '%c', '%f', '%p', '%s', and '%%'
   conventional formats, plus Lua-specific '%I' and '%U'
*/
const char *luaO_pushvfstring(lua_State *L, const char *fmt, va_list argp)
{
  BuffFS buff;        /* holds last part of the result 保存结果的最后部分 */
  const char *e;      /* points to next '%' 指向下一个'%' */
  initbuff(L, &buff); /* 初始化缓冲区 */
  /*
  ** 主循环: 查找并处理每个'%'
  */
  while ((e = strchr(fmt, '%')) != NULL)
  {
    addstr2buff(&buff, fmt, ct_diff2sz(e - fmt)); /* add 'fmt' up to '%' 添加'fmt'直到'%' */
    /*
    ** 根据转换说明符处理
    */
    switch (*(e + 1))
    { /* conversion specifier 转换说明符 */
    case 's':
    {                                       /* zero-terminated string 以零结尾的字符串 */
      const char *s = va_arg(argp, char *); /* 获取字符串参数 */
      if (s == NULL)
        s = "(null)"; /* NULL指针显示为"(null)" */
      addstr2buff(&buff, s, strlen(s));
      break;
    }
    case 'c':
    { /* an 'int' as a character 一个'int'作为字符 */
      /*
      ** va_arg获取int类型参数
      ** 根据可变参数提升规则,char被提升为int
      */
      char c = cast_char(va_arg(argp, int));
      addstr2buff(&buff, &c, sizeof(char));
      break;
    }
    case 'd':
    { /* an 'int' 一个'int' */
      TValue num;
      setivalue(&num, va_arg(argp, int)); /* 创建整数TValue */
      addnum2buff(&buff, &num);
      break;
    }
    case 'I':
    { /* a 'lua_Integer' 一个'lua_Integer' */
      TValue num;
      /*
      ** l_uacInt是lua_Integer的对齐版本
      ** 用于确保va_arg正确获取参数
      */
      setivalue(&num, cast_Integer(va_arg(argp, l_uacInt)));
      addnum2buff(&buff, &num);
      break;
    }
    case 'f':
    { /* a 'lua_Number' 一个'lua_Number' */
      TValue num;
      /*
      ** l_uacNumber是lua_Number的对齐版本
      ** cast_num确保类型正确
      */
      setfltvalue(&num, cast_num(va_arg(argp, l_uacNumber)));
      addnum2buff(&buff, &num);
      break;
    }
    case 'p':
    {                         /* a pointer 一个指针 */
      char bf[LUA_N2SBUFFSZ]; /* enough space for '%p' '%p'的足够空间 */
      void *p = va_arg(argp, void *);
      /*
      ** lua_pointer2str将指针转换为字符串
      ** 通常是十六进制格式如"0x12345678"
      */
      int len = lua_pointer2str(bf, LUA_N2SBUFFSZ, p);
      addstr2buff(&buff, bf, cast_uint(len));
      break;
    }
    case 'U':
    {                      /* an 'unsigned long' as a UTF-8 sequence 一个'unsigned long'作为UTF-8序列 */
      char bf[UTF8BUFFSZ]; /* UTF-8缓冲区 */
      unsigned long arg = va_arg(argp, unsigned long);
      /*
      ** 将Unicode码点编码为UTF-8
      ** 结果从缓冲区末尾开始,所以要计算起始位置
      */
      int len = luaO_utf8esc(bf, cast(l_uint32, arg));
      addstr2buff(&buff, bf + UTF8BUFFSZ - len, cast_uint(len));
      break;
    }
    case '%':
    {
      addstr2buff(&buff, "%", 1); /* 字面'%' */
      break;
    }
    default:
    {
      /*
      ** 未知格式说明符: 原样保留
      ** 例如"%q"会被保留为"%q"
      */
      addstr2buff(&buff, e, 2); /* keep unknown format in the result 在结果中保留未知格式 */
      break;
    }
    }
    fmt = e + 2; /* skip '%' and the specifier 跳过'%'和说明符 */
  }
  addstr2buff(&buff, fmt, strlen(fmt)); /* rest of 'fmt' 'fmt'的剩余部分 */
  return clearbuff(&buff);              /* empty buffer into a new string 将缓冲区清空到新字符串 */
}

/*
** ============================================================================
** 函数: luaO_pushfstring
** ============================================================================
** 功能: 格式化字符串并推入栈(普通参数版本)
**
** 说明:
**   这是luaO_pushvfstring的包装,使用...可变参数
**   如果发生内存错误,抛出异常
*/
/*
** 格式化字符串(普通参数版本)
*/
const char *luaO_pushfstring(lua_State *L, const char *fmt, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, fmt);                   /* 初始化可变参数列表 */
  msg = luaO_pushvfstring(L, fmt, argp); /* 调用可变参数版本 */
  va_end(argp);                          /* 清理可变参数列表 */
  if (msg == NULL)                       /* error? 错误? */
    luaD_throw(L, LUA_ERRMEM);           /* 抛出内存错误 */
  return msg;
}

/* }================================================================== */

/*
** 源代码标识符相关的常量定义
*/
#define RETS "..."       /* 表示省略的内容 */
#define PRE "[string \"" /* 前缀 */
#define POS "\"]"        /* 后缀 */

/*
** addstr宏: 复制字符串到目标并移动指针
*/
#define addstr(a, b, l) (memcpy(a, b, (l) * sizeof(char)), a += (l))

/*
** ============================================================================
** 函数: luaO_chunkid
** ============================================================================
** 功能: 生成源代码块的标识符(用于错误消息)
**
** 参数:
**   out - 输出缓冲区
**   source - 源字符串
**   srclen - 源字符串长度
**
** 输出格式:
**   - '='开头: 字面源(直接使用或截断)
**   - '@'开头: 文件名(完整或带"...")
**   - 其他: [string "内容"](单行或截断,去除换行)
**
** 说明:
**   这用于生成错误消息中的源位置信息
**   如: [string "local x = 1"] 或 @myfile.lua 或 =[C]
*/
/*
** 生成块标识符
*/
void luaO_chunkid(char *out, const char *source, size_t srclen)
{
  size_t bufflen = LUA_IDSIZE; /* free space in buffer 缓冲区可用空间 */
  /*
  ** '='开头: 字面源
  ** 例如: "=[C]" 表示C代码
  */
  if (*source == '=')
  {                                                   /* 'literal' source '字面'源 */
    if (srclen <= bufflen)                            /* small enough? 足够小? */
      memcpy(out, source + 1, srclen * sizeof(char)); /* 直接复制(跳过'=') */
    else
    {                                       /* truncate it 截断它 */
      addstr(out, source + 1, bufflen - 1); /* 复制前bufflen-1个字符 */
      *out = '\0';                          /* 添加结束符 */
    }
  }
  /*
  ** '@'开头: 文件名
  ** 例如: "@myscript.lua"
  */
  else if (*source == '@')
  {                                                   /* file name 文件名 */
    if (srclen <= bufflen)                            /* small enough? 足够小? */
      memcpy(out, source + 1, srclen * sizeof(char)); /* 直接复制 */
    else
    {                              /* add '...' before rest of name 在名字剩余部分前添加'...' */
      addstr(out, RETS, LL(RETS)); /* 添加"..." */
      bufflen -= LL(RETS);
      /*
      ** 复制文件名的后半部分
      ** source + 1 + srclen - bufflen 计算起始位置
      */
      memcpy(out, source + 1 + srclen - bufflen, bufflen * sizeof(char));
    }
  }
  /*
  ** 字符串源: 格式化为 [string "内容"]
  */
  else
  {                                        /* string; format as [string "source"] 字符串;格式化为[string "source"] */
    const char *nl = strchr(source, '\n'); /* find first new line (if any) 查找第一个换行符(如果有) */
    addstr(out, PRE, LL(PRE));             /* add prefix 添加前缀 */
    /*
    ** 为前缀、后缀和'\0'保留空间
    ** LL宏计算字符串字面量的长度
    */
    bufflen -= LL(PRE RETS POS) + 1; /* save space for prefix+suffix+'\0' 为前缀+后缀+'\0'保留空间 */
    /*
    ** 小型单行源: 完整复制
    */
    if (srclen < bufflen && nl == NULL)
    {                              /* small one-line source? 小型单行源? */
      addstr(out, source, srclen); /* keep it 保留它 */
    }
    else
    {
      /*
      ** 大型或多行源: 截断
      */
      if (nl != NULL)
        srclen = ct_diff2sz(nl - source); /* stop at first newline 在第一个换行符处停止 */
      if (srclen > bufflen)
        srclen = bufflen; /* 限制长度 */
      addstr(out, source, srclen);
      addstr(out, RETS, LL(RETS)); /* 添加"..." */
    }
    /*
    ** 添加后缀"]"
    ** LL(POS) + 1 包括结束符
    */
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}

/*
** ============================================================================
** 文件总结
** ============================================================================
**
** 本文件(lobject.c)是Lua对象系统的核心实现文件之一,提供了以下主要功能:
**
** 一、数学和编码相关函数
** ------------------------
** 1. luaO_ceillog2: 计算以2为底的对数上限,用于哈希表大小计算
**    - 使用查找表优化小数值的计算
**    - 大数值通过位移操作分解
**
** 2. luaO_codeparam / luaO_applyparam: 浮点参数编码/解码
**    - 将百分比编码为单字节浮点表示
**    - 采用类IEEE 754格式,支持归一化和非归一化数
**    - 用于GC参数等场景,节省内存
**
** 二、算术运算
** ------------
** 3. intarith: 整数算术运算
**    - 支持加减乘、取模、整除
**    - 支持位运算(与或异或、移位、取反)
**    - 处理溢出和边界情况
**
** 4. numarith: 浮点数算术运算
**    - 支持加减乘除、幂运算
**    - 处理特殊值(NaN、Inf)
**
** 5. luaO_rawarith: 原始算术运算(不使用元方法)
**    - 根据操作数类型选择整数或浮点运算
**    - 位运算只对整数有效
**    - 除法和幂运算只对浮点数有效
**
** 6. luaO_arith: 带元方法的算术运算
**    - 先尝试原始运算,失败则查找元方法
**
** 三、字符串到数字的转换
** ----------------------
** 7. lua_strx2number: 十六进制字符串到浮点数
**    - 遵循C99规范
**    - 支持小数点和p指数
**    - 例如: "0x1.8p3" = 1.5 * 2^3 = 12.0
**
** 8. l_str2d: 十进制字符串到浮点数
**    - 处理locale的小数点差异
**    - 拒绝'inf'和'nan'
**
** 9. l_str2int: 字符串到整数
**    - 支持十进制和十六进制
**    - 检测溢出
**
** 10. luaO_str2num: 统一的字符串到数字转换
**     - 优先尝试整数转换
**     - 失败则尝试浮点数转换
**
** 四、数字到字符串的转换
** ----------------------
** 11. tostringbuffFloat: 浮点数到字符串
**     - 先用标准精度转换
**     - 如果精度损失,用高精度重新转换
**     - 为整数形式的浮点数添加".0"
**
** 12. luaO_tostringbuff: 数字对象到字符串
**     - 根据类型选择整数或浮点转换
**
** 13. luaO_tostring: 将数字对象替换为字符串对象
**     - 在Lua字符串池中创建新字符串
**
** 五、UTF-8处理
** -------------
** 14. luaO_hexavalue: 十六进制字符到数值
**
** 15. luaO_utf8esc: Unicode码点到UTF-8编码
**     - 支持1-4字节编码
**     - 从缓冲区末尾向前填充
**
** 六、格式化字符串输出
** --------------------
** 16. luaO_pushvfstring / luaO_pushfstring: 格式化字符串
**     - 类似printf但支持Lua特定类型
**     - 支持: %d, %c, %f, %p, %s, %%, %I, %U
**     - 使用动态缓冲区,自动扩展
**     - 处理内存错误和溢出
**
** 七、源代码标识符
** ----------------
** 17. luaO_chunkid: 生成源代码块标识符
**     - 用于错误消息中的源位置信息
**     - 处理字面源、文件名、字符串源三种格式
**
** 高级C语言特性说明:
** ------------------
** 1. 可变参数(va_list, va_start, va_end):
**    - 用于实现类似printf的格式化函数
**    - va_arg按类型提取参数
**
** 2. 函数指针和回调:
**    - luaD_rawrunprotected使用函数指针实现保护调用
**
** 3. 内存管理:
**    - luaM_reallocvector动态分配和重新分配内存
**    - luaM_freearray释放内存
**
** 4. 类型转换和对齐:
**    - cast系列宏确保类型安全
**    - l_uacInt等类型确保可变参数对齐
**
** 5. 字符串操作:
**    - strchr, strpbrk, strspn等标准C库函数
**    - memcpy用于内存块复制
**
** 6. 浮点数操作:
**    - ldexp计算x * 2^exp
**    - 处理特殊值和精度问题
**
** 7. 静态局部变量:
**    - luaO_ceillog2中的log_2表,只初始化一次
**
** 8. 结构体和typedef:
**    - BuffFS结构封装缓冲区管理
**
** 设计思想:
** ----------
** - 性能优化: 查找表、内联函数、避免重复计算
** - 健壮性: 溢出检查、错误处理、断言
** - 可移植性: 处理locale差异、平台特定的类型大小
** - 精确性: 浮点数转换保持精度、正确的舍入
** - 内存效率: 静态缓冲区优先、动态扩展、及时释放
**
** ============================================================================
*/