/*
==============================================================================
文件: llex.h  (Lua 源码 - Lexical Analyzer / 词法分析器 接口头文件)
------------------------------------------------------------------------------

【你将从这里学到什么】
1) 词法分析器(lexer)在 Lua 编译流程中的位置：它把“字符流”切分为“Token 流”；
   解析器(parser)再消费 Token 流，构建语法结构并生成后续中间结果/字节码。
2) 本文件提供了 lexer 对外“可见”的：
   - Token 编码规则（保留字、运算符等）
   - Token 携带的语义信息类型（SemInfo）
   - Token 结构体（Token）
   - 贯穿 lexer + parser 的共享状态（LexState）
   - luaX_* 系列函数声明（初始化、设置输入、取 token、报错、token->字符串等）

【阅读方式建议】
- 本文件是“声明/接口”，真正实现通常在同名的 llex.c。
- 阅读时重点抓住三件事：
  A) Token 如何编码（FIRST_RESERVED、enum RESERVED、NUM_RESERVED）
  B) Token 的“值”怎么带出来（SemInfo / Token）
  C) LexState 里保存了哪些跨模块共享的状态（字符流、缓冲区、字符串驻留表等）

【涉及的稍“高级”C用法（会在文内注释解释）】
- 头文件 include guard（防重复包含）
- 宏与条件编译（#define / #if !defined）
- enum 的“序号稳定性”要求（“改顺序会出事”）
- union（共用体）在“同一块内存保存不同类型值”的用法
- 不完全类型/前向声明（struct FuncState 等指针成员，不需要完整定义）
- Lua 自己的宏：LUAI_FUNC、l_noret、cast_int 等（属于 Lua 内部可移植性封装）

------------------------------------------------------------------------------
==============================================================================
*/

/*
** $Id: llex.h $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/
/* 中文翻译：
** $Id: llex.h $
** 词法分析器（Lexical Analyzer）
** 版权声明请参见 lua.h
*/

#ifndef llex_h
#define llex_h
/* 中文说明：
** 这是典型的“头文件防重复包含”写法（include guard）。
** - 第一次包含本头文件时：llex_h 未定义 -> 进入 -> 定义 llex_h
** - 后续再次包含本头文件时：llex_h 已定义 -> 跳过整个文件内容
** 目的：避免重复定义/重复声明导致的编译错误。
*/

#include <limits.h>
/* 中文说明：
** <limits.h> 提供各类整数类型的极值宏。
** 这里最关键的是 UCHAR_MAX：unsigned char 的最大值（通常为 255）。
** 后面 FIRST_RESERVED 会用 UCHAR_MAX + 1 作为“非单字符 token”的起始编号。
*/

#include "lobject.h"
#include "lzio.h"
/* 中文说明：
** 这些是 Lua 内部头文件（不是标准 C 库）：
** - lobject.h：定义 Lua 的核心对象/类型（lua_Number、lua_Integer、TString、Table 等）
** - lzio.h：定义 ZIO（输入流抽象）、Mbuffer（可增长缓冲区）等
** 词法分析器需要读取输入流(ZIO)，并且要构造/引用 Lua 的字符串对象(TString)等。
*/

/*
** Single-char tokens (terminal symbols) are represented by their own
** numeric code. Other tokens start at the following value.
*/
/* 中文翻译：
** 单字符 token（终结符）直接使用该字符自身的数值编码来表示。
** 其他 token（例如保留字、复合运算符、数字/字符串字面量等）从下面这个值开始编码。
*/
/* 补充说明（结合实际代码）：
** Lua 的 lexer 读取字符流时，遇到像 '+'、'-'、'('、')' 这类单字符终结符，
** 直接把 token 值设为该字符的 ASCII/字节值（例如 '+' == 43）。
** 但像 '=='、'<='、'..'、'...'、关键字 'if'/'while' 等不是单字符，就必须用
** “大于所有单字符”的编号来避免冲突，于是 FIRST_RESERVED = UCHAR_MAX + 1。
*/
#define FIRST_RESERVED (UCHAR_MAX + 1)
/* 中文说明：
** UCHAR_MAX 覆盖 0..UCHAR_MAX 的所有可能“单字节字符值”。
** FIRST_RESERVED 从 UCHAR_MAX+1 开始，确保与任何单字符 token 不冲突。
*/

#if !defined(LUA_ENV)
#define LUA_ENV "_ENV"
#endif
/* 中文翻译与说明：
** 如果没有在编译时定义 LUA_ENV，则默认把 LUA_ENV 设为 "_ENV"。
** 在 Lua 中，_ENV 常用于表示当前环境表（环境变量名）。
** 这里用宏让源码在不同构建配置/版本里可以自定义环境变量的名字。
*/

/*
 * WARNING: if you change the order of this enumeration,
 * grep "ORDER RESERVED"
 */
/* 中文翻译：
** 警告：如果你改变了这个枚举的顺序，
** 请搜索（grep）"ORDER RESERVED" 相关代码并同步调整。
*/
/* 补充说明（为什么“不能随便改顺序”）：
** enum RESERVED 的顺序通常会和：
** - 保留字表（字符串 -> token 的映射表）
** - token -> 字符串的反向映射
** - NUM_RESERVED 的计算范围
** 等逻辑形成“隐式契约”。
** 顺序一变，这些映射就可能错位，导致关键字识别错误或报错信息错乱。
*/
enum RESERVED
{
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED,
  TK_BREAK,
  TK_DO,
  TK_ELSE,
  TK_ELSEIF,
  TK_END,
  TK_FALSE,
  TK_FOR,
  TK_FUNCTION,
  TK_GLOBAL,
  TK_GOTO,
  TK_IF,
  TK_IN,
  TK_LOCAL,
  TK_NIL,
  TK_NOT,
  TK_OR,
  TK_REPEAT,
  TK_RETURN,
  TK_THEN,
  TK_TRUE,
  TK_UNTIL,
  TK_WHILE,
  /* other terminal symbols */
  TK_IDIV,
  TK_CONCAT,
  TK_DOTS,
  TK_EQ,
  TK_GE,
  TK_LE,
  TK_NE,
  TK_SHL,
  TK_SHR,
  TK_DBCOLON,
  TK_EOS,
  TK_FLT,
  TK_INT,
  TK_NAME,
  TK_STRING
};
/* 中文翻译（逐段）：
** - “terminal symbols denoted by reserved words”：
**   由保留字表示的终结符（关键字 token），例如 and/break/do/.../while 等。
** - “other terminal symbols”：
**   其他终结符（通常是复合运算符、特殊符号、字面量分类等）：
**   TK_IDIV(整除)、TK_CONCAT(拼接)、TK_DOTS(...)、TK_EQ(==)、TK_GE(>=)、
**   TK_LE(<=)、TK_NE(~= 或 != 取决于语法)、TK_SHL/TK_SHR(位移)、
**   TK_DBCOLON(::)、TK_EOS(输入结束)、以及数值/名称/字符串类别 TK_FLT/TK_INT/TK_NAME/TK_STRING。
**
** 重要：这里只是“token 编码定义”，具体每个 token 对应的字符序列由 lexer 实现负责识别。
*/

/* number of reserved words */
#define NUM_RESERVED (cast_int(TK_WHILE - FIRST_RESERVED + 1))
/* 中文翻译：
** 保留字数量。
*/
/* 补充说明（结合公式理解）：
** - 保留字 token 从 TK_AND = FIRST_RESERVED 开始连续编号，
**   一直到 TK_WHILE（这是最后一个保留字 token）。
** - 因此保留字数量 = (TK_WHILE - FIRST_RESERVED + 1)
** - cast_int(...) 是 Lua 内部宏：把表达式安全地转换为 int（细节在 Lua 的基础宏里定义）。
*/

typedef union
{
  lua_Number r;
  lua_Integer i;
  TString *ts;
} SemInfo; /* semantics information */
/* 中文翻译：
** SemInfo：语义信息（semantics information）。
*/
/* 关键讲解：union（共用体）的意义与使用方式
** - union 的所有成员共享同一段内存：同一时刻“只应当使用其中一种解释”。
** - 这里 SemInfo 用来保存 token 的“附带值”：
**   r  : 浮点数（lua_Number）
**   i  : 整数（lua_Integer）
**   ts : 字符串对象指针（TString*，Lua 的内部字符串类型）
** - 为什么用 union：
**   Token 的语义值在不同 token 类型下只需要其中一种。
**   用 union 可以避免为每个 Token 同时存储三份空间（节省内存，并保持结构紧凑）。
** - 使用时的“契约”：
**   由 Token 的 token 字段决定 seminfo 的哪一项有效：
**   例如 TK_INT -> 使用 seminfo.i；TK_FLT -> 使用 seminfo.r；TK_NAME/TK_STRING -> 使用 seminfo.ts。
*/

typedef struct Token
{
  int token;
  SemInfo seminfo;
} Token;
/* 中文说明（Token 结构）：
** - token：token 类型/编码（可能是单字符的数值，也可能是 enum RESERVED 里的值）
** - seminfo：该 token 的语义值（如数字字面量的数值、标识符/字符串的 TString*）
** 词法分析器产出的最核心数据结构就是 Token。
*/

/* state of the scanner plus state of the parser when shared by all
   functions */
/* 中文翻译：
** 扫描器（词法分析器）的状态 + 解析器中与所有函数共享的状态。
*/
/* 补充说明（为什么 lexer 里会带 parser 的状态）：
** Lua 的编译流程中，lexer 和 parser 紧密配合。
** - lexer：负责把输入流切成 token
** - parser：按语法规则消费 token
** 但 parser 的某些信息（例如当前函数状态 FuncState、动态数据结构 Dyndata）
** 需要在 lexer 驱动 token 前进时被访问/更新，因此把它们挂在同一个 LexState 中。
** 这是一种工程上的“共享上下文”设计：减少参数传递和模块间循环依赖。
*/
typedef struct LexState
{
  /* 说明：下面每个字段右侧已有英文注释；你要求“原注释保留并翻译”，
  ** 所以我在每个字段后面追加对应的中文解释（用独立注释行，不改动原行）。
  */

  int current; /* current character (charint) */
  /* 中文：当前字符（charint）。
  ** 补充：通常用 int 保存“字符或特殊哨兵值”，因为 char 可能不足以表达 EOF/流结束等状态。
  */

  int linenumber; /* input line counter */
  /* 中文：输入的行号计数器（当前读到第几行）。
  ** 词法分析器在读到 '\n' 时通常会递增它，用于错误定位与调试信息。
  */

  int lastline; /* line of last token 'consumed' */
  /* 中文：上一个“已被消费”的 token 所在行号。
  ** 补充：parser 消费 token 时会更新 lastline，用于生成更准确的报错/调试位置信息。
  */

  Token t; /* current token */
  /* 中文：当前 token（已经由 lexer 识别出来，准备给 parser 使用的 token）。 */

  Token lookahead; /* look ahead token */
  /* 中文：向前看（前瞻）的 token。
  ** 补充：某些语法需要提前看一个 token 决定分支，这里缓存前瞻结果避免重复扫描输入。
  */

  struct FuncState *fs; /* current function (parser) */
  /* 中文：当前函数的编译状态（parser 侧）。
  ** C 技巧：这里用 struct FuncState * 指针，但没有包含 FuncState 的完整定义；
  ** 这叫“不完全类型/前向声明用法”。只要你不在此处访问其字段，声明指针是合法的。
  ** 这样能避免头文件互相 include 导致的循环依赖。
  */

  struct lua_State *L;
  /* 中文：Lua 虚拟机状态指针（lua_State）。
  ** lexer/parser 在创建对象、抛出错误、分配内存等时需要它。
  ** 同样是指针成员：这里也不需要在此头文件给出 lua_State 的完整定义。
  */

  ZIO *z; /* input stream */
  /* 中文：输入流（ZIO）。
  ** ZIO 是 Lua 的“流式输入”抽象：可以来自文件、字符串、内存块等。
  ** lexer 从 z 里逐字符读取，更新 current，并形成 token。
  */

  Mbuffer *buff; /* buffer for tokens */
  /* 中文：token 构造用的缓冲区（Mbuffer）。
  ** 典型用途：读取标识符/数字/字符串字面量时，需要一个可增长的缓冲区暂存字符序列。
  */

  Table *h; /* to avoid collection/reuse strings */
  /* 中文：一张 Table（哈希表）用于避免字符串被回收/并复用字符串。
  ** 补充：lexer 会把读到的标识符/字符串字面量转成 TString 对象。
  ** 为了：
  ** - 避免 GC 在编译过程中回收这些临时字符串
  ** - 或者实现字符串驻留/复用（interning）
  ** 会把它们暂时放进一张表 h 里做引用保持/去重。
  */

  struct Dyndata *dyd; /* dynamic structures used by the parser */
  /* 中文：parser 使用的动态结构（Dyndata）。
  ** 同样是前向声明指针，避免 include 依赖；具体内容在 parser 相关实现里定义。
  */

  TString *source; /* current source name */
  /* 中文：当前源的名字（source 名称），用于错误信息/调试信息显示，比如文件名或 chunk 名。 */

  TString *envn; /* environment variable name */
  /* 中文：环境变量名（通常是 "_ENV"，由 LUA_ENV 宏决定）。 */

  TString *brkn; /* "break" name (used as a label) */
  /* 中文："break" 的名字（用作 label）。
  ** 补充：Lua 编译器内部可能把 break 处理成 goto 到某个隐式 label，因此需要一个名字对象。
  */

  TString *glbn; /* "global" name (when not a reserved word) */
  /* 中文："global" 的名字（当它不是保留字时使用）。
  ** 结合本文件 enum RESERVED：这里也定义了 TK_GLOBAL，说明该源码树支持 global 相关语法/扩展。
  ** 该字段用于在不同构建/语法设置下处理 “global” 的识别策略。
  */
} LexState;

/*
** LUAI_FUNC / l_noret 等是什么？
** - LUAI_FUNC：Lua 内部用来统一处理导出符号、调用约定、可见性等的宏（跨平台封装）。
** - l_noret ：Lua 内部表示“该函数不会返回”（noreturn）的标记（通常用于语法错误抛出）。
** 你在阅读时可以把它们先当成“修饰符”，不改变函数的语义，只影响编译器/链接层面行为。
*/

/* ---------------------------------------------------------------------------
** luaX_init
** ---------------------------------------------------------------------------
** 功能：初始化词法分析器相关的全局/共享数据（通常是保留字表、token->字符串映射等）。
** 参数：
** - L：lua_State*，用于分配对象、注册字符串等。
** 使用时机：
** - 通常在 Lua 状态初始化或加载编译器部件时调用一次（具体由上层初始化流程决定）。
*/
LUAI_FUNC void luaX_init(lua_State *L);

/* ---------------------------------------------------------------------------
** luaX_setinput
** ---------------------------------------------------------------------------
** 功能：把输入流绑定到 LexState，设置初始字符、源名称等，为开始扫描做准备。
** 参数：
** - L：lua_State*（同上，提供内存/错误处理上下文）
** - ls：LexState*（将要被初始化/填充的词法状态）
** - z：ZIO*（输入流）
** - source：TString*（源名称：文件名、chunk 名等）
** - firstchar：int（首字符；常见场景：上层已经读了一个字符/做了预读，这里直接接管）
** 备注：
** - 你会在 llex.c 中看到它会设置 ls->current、ls->z、ls->linenumber 等。
*/
LUAI_FUNC void luaX_setinput(lua_State *L, LexState *ls, ZIO *z,
                             TString *source, int firstchar);

/* ---------------------------------------------------------------------------
** luaX_newstring
** ---------------------------------------------------------------------------
** 功能：把一段 C 字符串（str, l）转换/驻留为 Lua 的 TString 对象，并与 LexState 关联。
** 参数：
** - ls：LexState*（用来访问 ls->L、ls->h 等，确保字符串在编译期不会被 GC 回收，并可能去重）
** - str：const char*（原始字节序列，不一定以 '\0' 结尾，长度由 l 指定）
** - l：size_t（长度）
** 返回：
** - TString*：Lua 内部字符串对象指针。
** 典型用途：
** - 识别到标识符/字符串字面量后，把其内容转为 TString，存到 Token.seminfo.ts。
*/
LUAI_FUNC TString *luaX_newstring(LexState *ls, const char *str, size_t l);

/* ---------------------------------------------------------------------------
** luaX_next
** ---------------------------------------------------------------------------
** 功能：推进到“下一个 token”。
** 行为（典型语义）：
** - 消费当前 token（ls->t），并扫描输入流生成下一个 token 放入 ls->t。
** - 处理 lookahead：如果已有前瞻 token，可能直接把 lookahead 变成当前 token。
** 你在阅读 parser 时会频繁看到它：parser 通过反复调用 luaX_next 来驱动扫描。
*/
LUAI_FUNC void luaX_next(LexState *ls);

/* ---------------------------------------------------------------------------
** luaX_lookahead
** ---------------------------------------------------------------------------
** 功能：获取“前瞻 token”（不真正消费当前 token）。
** 返回：
** - int：前瞻 token 的编码（通常也会填充 ls->lookahead 的 seminfo）。
** 使用场景：
** - parser 需要根据下一个 token 决策语法分支，但又不能立刻消耗它。
*/
LUAI_FUNC int luaX_lookahead(LexState *ls);

/* ---------------------------------------------------------------------------
** luaX_syntaxerror
** ---------------------------------------------------------------------------
** 功能：报告语法错误，并终止当前解析流程（不返回）。
** 参数：
** - ls：LexState*（用于定位出错的行号、source 等）
** - s：const char*（错误描述文本）
** 返回特性：
** - l_noret：标记该函数不返回；它通常会抛出 Lua 错误（longjmp 或等价机制）。
** 注意：
** - 在 C 层面，“不返回”意味着调用后控制流不会继续执行；阅读代码时要把它当作终止点。
*/
LUAI_FUNC l_noret luaX_syntaxerror(LexState *ls, const char *s);

/* ---------------------------------------------------------------------------
** luaX_token2str
** ---------------------------------------------------------------------------
** 功能：把 token 编码转换为可读字符串（主要用于错误信息、调试输出）。
** 参数：
** - ls：LexState*（可能用于上下文：比如把保留字 token 映射回关键字字符串）
** - token：int（token 编码；可以是单字符，也可以是 enum RESERVED 中的值）
** 返回：
** - const char*：token 的文本表示（例如 "'+'"、"name"、"'=='"、"while" 等）
*/
LUAI_FUNC const char *luaX_token2str(LexState *ls, int token);

#endif

/*
==============================================================================
文件总结（用于回顾与建立“整体图景”）
------------------------------------------------------------------------------

1) Token 编码策略
- 单字符终结符：直接用字符自身的数值编码（0..UCHAR_MAX）。
- 复杂 token：从 FIRST_RESERVED (= UCHAR_MAX + 1) 开始编号，避免与单字符冲突。
- enum RESERVED：
  - 前半段是保留字 token（TK_AND ... TK_WHILE），顺序不能随意改动。
  - 后半段是其他复合终结符/类别（如 TK_EQ、TK_DOTS、TK_INT、TK_STRING 等）。
- NUM_RESERVED：
  - 通过 (TK_WHILE - FIRST_RESERVED + 1) 计算保留字数量，依赖枚举连续性与顺序稳定。

2) Token 的语义值（SemInfo / Token）
- Token.token：表示 token 类型/编码。
- Token.seminfo：用 union 存 token 的“值”：
  - 浮点：lua_Number r
  - 整数：lua_Integer i
  - 字符串：TString* ts
- 关键点：union 的“哪一项有效”由 token 类型决定，这是 lexer/parser 之间的约定。

3) LexState 的角色（lexer + parser 的共享上下文）
- lexer 侧必需：current、linenumber、z、buff
- parser 协作：fs、dyd、lastline、lookahead
- 内存/GC 相关：h 用来保持/复用编译期产生的字符串，避免回收与重复创建
- 源信息：source/envn/brkn/glbn 为错误信息与某些语法/扩展提供命名对象

4) luaX_* 接口的“典型调用关系”（帮助你建立调用链）
- luaX_init(L)
  初始化全局映射（保留字表、token字符串表等）。
- luaX_setinput(L, ls, z, source, firstchar)
  绑定输入流与词法状态，设置源信息与初始字符。
- 在 parser 运行过程中反复：
  - luaX_next(ls)         // 消费并产生下一个 token
  - luaX_lookahead(ls)    // 需要前瞻时使用
- 报错与展示：
  - luaX_syntaxerror(ls, msg)  // 语法错误终止点（不返回）
  - luaX_token2str(ls, token) // token 变可读字符串（用于报错/调试）

5) 下一步该看哪里（建议）
- 若你要“读通透实现细节”，请定位同目录的 llex.c：
  - 你会看到：如何读 ZIO、如何填 Token、如何识别保留字/数字/字符串、如何更新行号。
- 同时配合 parser 文件（通常是 lparser.c / lparser.h）看：
  - parser 如何调用 luaX_next/lookahead，以及如何使用 LexState.fs/dyd 等字段。

==============================================================================
*/
