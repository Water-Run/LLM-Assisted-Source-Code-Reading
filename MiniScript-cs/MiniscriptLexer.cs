/*	MiniscriptLexer.cs

================================================================================
文件概要
================================================================================
文件名: MiniscriptLexer.cs
用途: MiniScript语言的词法分析器(Lexer)
功能: 将MiniScript源代码文本分解为一系列的标记(Token)

什么是词法分析器?
----------------
词法分析器是编译器/解释器的第一个阶段,负责将原始的源代码字符串分解成有意义的
最小单元(称为Token/标记)。例如,将 "x = 42 + y" 分解为:
  - 标识符: "x"
  - 赋值运算符: "="
  - 数字: "42"
  - 加号运算符: "+"
  - 标识符: "y"

这个文件包含两个主要的类:
1. Token类 - 表示单个词法标记
2. Lexer类 - 负责从源代码中提取Token

使用场景:
--------
除非您正在编写一个高级的MiniScript代码编辑器(带语法高亮、自动补全等功能),
否则通常不需要直接使用这个文件的内容。这是MiniScript解析器内部使用的组件。

*/

using System;                          // 引入基础类库,提供Exception等基本类型
using System.Collections.Generic;      // 引入泛型集合,本文件中使用Queue<Token>队列

namespace Miniscript {
	// ============================================================================
	// Token类 - 表示词法分析后的单个标记
	// ============================================================================
	// 这个类封装了从源代码中识别出的每一个最小语法单元
	// 例如: 数字、标识符、运算符、括号等
	public class Token {
		
		// ------------------------------------------------------------------------
		// Token.Type 枚举 - 定义所有可能的标记类型
		// ------------------------------------------------------------------------
		// 这是一个枚举(enum),C#中的枚举用于定义一组命名的整数常量
		// 每个枚举值默认从0开始递增: Unknown=0, Keyword=1, Number=2, ...
		public enum Type {
			Unknown,        // 未知类型(通常表示错误)
			Keyword,        // 关键字(如: if, then, else, while, for等)
			Number,         // 数字字面量(如: 42, 3.14, .5, 1e-10)
			String,         // 字符串字面量(如: "hello", "world")
			Identifier,     // 标识符(变量名、函数名等,如: myVar, calculate)
			
			// 运算符
			OpAssign,       // = 赋值运算符
			OpPlus,         // + 加法
			OpMinus,        // - 减法
			OpTimes,        // * 乘法
			OpDivide,       // / 除法
			OpMod,          // % 取模(求余数)
			OpPower,        // ^ 幂运算
			
			// 比较运算符
			OpEqual,        // == 等于
			OpNotEqual,     // != 不等于
			OpGreater,      // > 大于
			OpGreatEqual,   // >= 大于等于
			OpLesser,       // < 小于
			OpLessEqual,    // <= 小于等于
			
			// 复合赋值运算符(运算并赋值的简写形式)
			OpAssignPlus,   // += 加后赋值 (等价于 x = x + y)
			OpAssignMinus,  // -= 减后赋值
			OpAssignTimes,  // *= 乘后赋值
			OpAssignDivide, // /= 除后赋值
			OpAssignMod,    // %= 取模后赋值
			OpAssignPower,  // ^= 幂运算后赋值
			
			// 括号和分隔符
			LParen,         // ( 左小括号
			RParen,         // ) 右小括号
			LSquare,        // [ 左方括号(用于列表/数组)
			RSquare,        // ] 右方括号
			LCurly,         // { 左大括号(用于映射/字典)
			RCurly,         // } 右大括号
			
			// 其他符号
			AddressOf,      // @ 取地址符(用于获取函数引用)
			Comma,          // , 逗号(分隔列表项、函数参数等)
			Dot,            // . 点号(成员访问运算符)
			Colon,          // : 冒号(用于映射的键值对分隔)
			Comment,        // // 注释
			EOL             // End Of Line 行结束标记(\n, \r, \r\n, 或 ;)
		}
		
		// ------------------------------------------------------------------------
		// Token类的字段(成员变量)
		// ------------------------------------------------------------------------
		public Type type;           // 这个Token的类型(从上面的枚举中选择)
		public string text;         // Token的文本内容
		                           // 注意: 对于运算符等固定文本的Token,这个字段可能为null
		                           // 例如: '+' 运算符的type是OpPlus,text可以为null
		                           // 但标识符、数字、字符串的text字段必须包含实际内容
		
		public bool afterSpace;    // 标记这个Token前面是否有空白字符
		                           // 这个信息在某些语法分析场景中很有用
		                           // 例如区分 "x -y" (x减y) 和 "x - y" (同样是x减y但格式不同)
		
		// ------------------------------------------------------------------------
		// 构造函数 - 创建Token对象
		// ------------------------------------------------------------------------
		// 参数说明:
		//   type: Token类型,默认为Unknown
		//   text: Token文本,默认为null
		// C#特性说明: 这里使用了"默认参数值",调用时可以省略参数
		// 例如: new Token() 或 new Token(Token.Type.Number, "42")
		public Token(Type type=Type.Unknown, string text=null) {
			this.type = type;    // this关键字表示当前对象,用于区分参数和字段
			this.text = text;
		}

		// ------------------------------------------------------------------------
		// ToString方法 - 将Token转换为字符串表示(用于调试和输出)
		// ------------------------------------------------------------------------
		// override关键字: 重写父类(Object)的ToString方法
		// 所有C#类都隐式继承自Object类,Object.ToString()默认返回类型名
		// 我们重写它以提供更有用的信息
		public override string ToString() {
			if (text == null) return type.ToString();  // 如果没有文本,只返回类型名
			// string.Format是字符串格式化方法,类似于C的printf
			// {0}和{1}是占位符,分别被type和text替换
			return string.Format("{0}({1})", type, text);
		}

		// ------------------------------------------------------------------------
		// 静态EOL Token - 预定义的行结束标记
		// ------------------------------------------------------------------------
		// static关键字: 表示这是类级别的成员,不属于任何实例
		// 可以通过 Token.EOL 直接访问,无需创建Token对象
		// 对象初始化器语法: new Token() { type=Type.EOL }
		// 这是C#的语法糖,等价于:
		//   Token temp = new Token();
		//   temp.type = Type.EOL;
		public static Token EOL = new Token() { type=Type.EOL };
	}
	
	
	// ============================================================================
	// Lexer类 - 词法分析器的主要实现
	// ============================================================================
	// 这个类负责从源代码字符串中逐个提取Token
	// 工作流程:
	//   1. 接收源代码字符串
	//   2. 维护当前读取位置
	//   3. 根据字符特征识别不同类型的Token
	//   4. 提供Peek(查看下一个Token)和Dequeue(获取并移除下一个Token)方法
	public class Lexer {
		// ------------------------------------------------------------------------
		// 字段(成员变量)
		// ------------------------------------------------------------------------
		public int lineNum = 1;	    // 当前行号,从1开始(方便报告1-based的行号给用户)
		                            // 每遇到换行符(\n, \r, \r\n)时递增
		
		public int position;	    // 当前在输入字符串中的读取位置(字符索引)
		
		string input;               // 要分析的源代码字符串
		int inputLength;            // 源代码的长度(缓存,避免重复调用input.Length)

		Queue<Token> pending;       // Token队列,用于实现Peek功能
		                           // Queue是C#泛型集合类,先进先出(FIFO)
		                           // Queue<Token>表示这个队列只能存储Token类型的对象
		                           // 主要方法: Enqueue(入队), Dequeue(出队), Peek(查看队首)

		// ------------------------------------------------------------------------
		// AtEnd属性 - 判断是否已经读取完所有输入
		// ------------------------------------------------------------------------
		// C#属性(Property): 提供类似字段的访问语法,但实际是方法
		// get访问器: 定义读取属性时的行为
		// 使用方式: if (lexer.AtEnd) { ... }
		// 判断条件: 位置超出输入长度 且 待处理队列为空
		public bool AtEnd {
			get { return position >= inputLength && pending.Count == 0; }
		}

		// ------------------------------------------------------------------------
		// 构造函数 - 初始化Lexer
		// ------------------------------------------------------------------------
		// 参数: input - 要分析的源代码字符串
		public Lexer(string input) {
			this.input = input;
			inputLength = input.Length;  // 缓存长度,提高性能
			position = 0;                // 从第一个字符开始
			pending = new Queue<Token>(); // 创建空队列
			                              // new关键字用于创建对象实例
		}

		// ------------------------------------------------------------------------
		// Peek方法 - 查看下一个Token但不移除它
		// ------------------------------------------------------------------------
		// 应用场景: 解析器需要"向前看"一个Token来决定如何处理当前语句
		// 例如: 看到"else",需要peek下一个Token是否为"if"以区分"else"和"else if"
		// 
		// 工作原理:
		//   1. 如果队列为空,先Dequeue一个Token放入队列
		//   2. 返回队列头部的Token(不移除)
		//   3. 多次Peek返回同一个Token
		public Token Peek() {
			if (pending.Count == 0) {           // 如果队列为空
				if (AtEnd) return Token.EOL;    // 如果已到末尾,返回EOL标记
				pending.Enqueue(Dequeue());     // 否则读取一个Token并入队
			}
			return pending.Peek();              // 返回队首Token(不移除)
			                                   // Queue.Peek()方法: 查看但不移除队首元素
		}

		// ------------------------------------------------------------------------
		// Dequeue方法 - 获取下一个Token(主要的词法分析逻辑)
		// ------------------------------------------------------------------------
		// 这是Lexer类的核心方法,负责从源代码中识别并提取下一个Token
		// 返回: 下一个Token对象
		public Token Dequeue() {
			// 第一步: 如果队列中有待处理的Token,直接返回
			if (pending.Count > 0) return pending.Dequeue();  // Queue.Dequeue(): 移除并返回队首

			// 第二步: 跳过空白字符和注释
			int oldPos = position;          // 记录跳过空白前的位置
			SkipWhitespaceAndComment();     // 跳过空格、制表符和注释

			// 第三步: 检查是否到达输入末尾
			if (AtEnd) return Token.EOL;

			// 第四步: 创建新Token并记录是否前面有空格
			Token result = new Token();
			result.afterSpace = (position > oldPos);  // 如果位置移动了,说明前面有空白
			int startPos = position;                  // 记录Token的起始位置
			char c = input[position++];               // 读取当前字符并前进一位
			                                         // position++是后缀递增,先使用后递增

			// ====================================================================
			// 第五步: 识别双字符运算符
			// ====================================================================
			// 某些运算符由两个字符组成,需要先检查
			// 例如: ==, !=, >=, <=, +=, -=, *=, /=, %=, ^=
			if (!AtEnd) {                    // 确保还有下一个字符可读
				char c2 = input[position];   // 查看下一个字符(不移动position)
				
				// 检查各种双字符运算符
				if (c2 == '=') {             // 第二个字符是'='
					if (c == '=') result.type = Token.Type.OpEqual;          // ==
					else if (c == '+') result.type = Token.Type.OpAssignPlus;  // +=
					else if (c == '-') result.type = Token.Type.OpAssignMinus; // -=
					else if (c == '*') result.type = Token.Type.OpAssignTimes; // *=
					else if (c == '/') result.type = Token.Type.OpAssignDivide;// /=
					else if (c == '%') result.type = Token.Type.OpAssignMod;   // %=
					else if (c == '^') result.type = Token.Type.OpAssignPower; // ^=
				}
				if (c == '!' && c2 == '=') result.type = Token.Type.OpNotEqual;  // !=
				if (c == '>' && c2 == '=') result.type = Token.Type.OpGreatEqual;// >=
				if (c == '<' && c2 == '=') result.type = Token.Type.OpLessEqual; // <=

				// 如果识别出双字符运算符,前进position并返回
				if (result.type != Token.Type.Unknown) {
					position++;  // 跳过第二个字符
					return result;
				}
			}

			// ====================================================================
			// 第六步: 识别单字符运算符和符号
			// ====================================================================
			// 处理所有单字符的Token
			if (c == '+') result.type = Token.Type.OpPlus;        // + 加号
			else if (c == '-') result.type = Token.Type.OpMinus;  // - 减号
			else if (c == '*') result.type = Token.Type.OpTimes;  // * 乘号
			else if (c == '/') result.type = Token.Type.OpDivide; // / 除号
			else if (c == '%') result.type = Token.Type.OpMod;    // % 取模
			else if (c == '^') result.type = Token.Type.OpPower;  // ^ 幂运算
			else if (c == '(') result.type = Token.Type.LParen;   // ( 左括号
			else if (c == ')') result.type = Token.Type.RParen;   // ) 右括号
			else if (c == '[') result.type = Token.Type.LSquare;  // [ 左方括号
			else if (c == ']') result.type = Token.Type.RSquare;  // ] 右方括号
			else if (c == '{') result.type = Token.Type.LCurly;   // { 左大括号
			else if (c == '}') result.type = Token.Type.RCurly;   // } 右大括号
			else if (c == ',') result.type = Token.Type.Comma;    // , 逗号
			else if (c == ':') result.type = Token.Type.Colon;    // : 冒号
			else if (c == '=') result.type = Token.Type.OpAssign; // = 赋值
			else if (c == '<') result.type = Token.Type.OpLesser; // < 小于
			else if (c == '>') result.type = Token.Type.OpGreater;// > 大于
			else if (c == '@') result.type = Token.Type.AddressOf;// @ 取地址符
			
			// 处理行结束符(EOL可以是分号或换行)
			else if (c == ';' || c == '\n') {
				result.type = Token.Type.EOL;
				result.text = c == ';' ? ";" : "\n";  // 三元运算符: 条件 ? 真值 : 假值
				if (c != ';') lineNum++;              // 换行符增加行号
			}
			
			// 处理回车符(需要考虑Windows的\r\n)
			if (c == '\r') {
				// 小心处理: DOS/Windows使用\r\n,需要检查
				result.type = Token.Type.EOL;
				if (position < inputLength && input[position] == '\n') {
					position++;               // 跳过\n
					result.text = "\r\n";     // 记录为\r\n
				} else {
					result.text = "\r";       // 只有\r(Mac老格式)
				}
				lineNum++;                    // 行号递增
			}
			
			// 如果识别出单字符Token,直接返回
			if (result.type != Token.Type.Unknown) return result;

			// ====================================================================
			// 第七步: 处理更复杂的Token(数字、标识符、字符串)
			// ====================================================================

			// ********************************************************************
			// 7.1 处理点号'.'和数字
			// ********************************************************************
			// 点号有两种可能:
			//   1. 成员访问运算符: obj.property
			//   2. 小数点: .5 (等于0.5)
			if (c == '.') {
				// 如果点号后面不是数字,那它就是成员访问运算符
				if (position >= inputLength || !IsNumeric(input[position])) {
					result.type = Token.Type.Dot;
					return result;
				}
				// 否则,它是数字的一部分(如.5),继续往下处理
			}

			// ********************************************************************
			// 7.2 处理数字(整数和浮点数)
			// ********************************************************************
			// 数字格式支持:
			//   - 整数: 42, 0, 999
			//   - 小数: 3.14, .5, 0.1
			//   - 科学记数法: 1e10, 1E-5, 2.5e+3
			if (c == '.' || IsNumeric(c)) {
				result.type = Token.Type.Number;
				// 持续读取字符,直到不再是数字的一部分
				while (position < inputLength) {
					char lastc = c;            // 保存上一个字符
					c = input[position];       // 查看当前字符
					
					// 数字可以包含:
					//   - 数字0-9
					//   - 小数点.
					//   - 科学记数法标记E或e
					//   - 科学记数法的正负号(仅在E/e后面)
					if (IsNumeric(c) || c == '.' || c == 'E' || c == 'e' ||
					    ((c == '-' || c == '+') && (lastc == 'E' || lastc == 'e'))) {
						position++;            // 字符是数字的一部分,前进
					} else break;              // 否则结束数字读取
				}
			}
			
			// ********************************************************************
			// 7.3 处理标识符和关键字
			// ********************************************************************
			// 标识符: 变量名、函数名等
			// 规则: 可以包含字母、数字、下划线、Unicode字符(>0x9F)
			//       但不能以数字开头
			else if (IsIdentifier(c)) {
				// 持续读取标识符字符
				while (position < inputLength) {
					if (IsIdentifier(input[position])) position++;
					else break;
				}
				
				// 提取标识符文本
				result.text = input.Substring(startPos, position - startPos);
				// Substring方法: 从字符串中提取子串
				// 参数: (起始位置, 长度)
				
				// 检查是否为关键字(如if, then, else, while等)
				result.type = (Keywords.IsKeyword(result.text) ? 
				               Token.Type.Keyword : Token.Type.Identifier);
				// Keywords.IsKeyword: 调用Keywords类的静态方法检查是否为关键字
				
				// ************************************************************
				// 特殊处理: "end" 关键字
				// ************************************************************
				// MiniScript的end关键字必须跟另一个关键字
				// 例如: "end if", "end function", "end while"
				// 所以看到"end"时,需要读取下一个关键字并合并
				if (result.text == "end") {
					Token nextWord = Dequeue();  // 递归调用,获取下一个Token
					if (nextWord != null && nextWord.type == Token.Type.Keyword) {
						// 合并: "end" + " " + "if" = "end if"
						result.text = result.text + " " + nextWord.text;
					} else {
						// 如果"end"后面不是关键字,这是语法错误
						// throw: 抛出异常,中断程序执行
						throw new LexerException("'end' without following keyword ('if', 'function', etc.)");
					}
				}
				
				// ************************************************************
				// 特殊处理: "else" 关键字
				// ************************************************************
				// 类似地,需要检测"else if"并合并为一个Token
				else if (result.text == "else") {
					// 注意: 这里不能使用Peek/Dequeue,因为可能已经在Peek调用中
					// 所以直接检查输入字符串
					var p = position;
					// 跳过空白
					while (p < inputLength && (input[p]==' ' || input[p]=='\t')) p++;
					// 检查是否为"if"
					if (p+1 < inputLength && input.Substring(p,2) == "if" &&
							(p+2 >= inputLength || !IsIdentifier(input[p+2]))) {
						// 确保"if"后面不是标识符的一部分(避免匹配"ifx"等)
						result.text = "else if";
						position = p + 2;  // 更新位置
					}
				}
				return result;
			}
			
			// ********************************************************************
			// 7.4 处理字符串字面量
			// ********************************************************************
			// 字符串用双引号包围: "hello"
			// 特殊规则: 两个连续的双引号("")表示一个双引号字符
			//          例如: "He said ""Hi""" 表示字符串 He said "Hi"
			else if (c == '"') {
				result.type = Token.Type.String;
				bool haveDoubledQuotes = false;  // 是否包含转义的双引号
				startPos = position;             // 字符串内容的起始位置(引号之后)
				bool gotEndQuote = false;        // 是否找到结束引号
				
				// 读取字符串内容
				while (position < inputLength) {
					c = input[position++];
					
					if (c == '"') {
						// 检查是否为转义的双引号("")
						if (position < inputLength && input[position] == '"') {
							// 这是转义的双引号,继续读取
							haveDoubledQuotes = true;
							position++;
						} else {
							// 这是结束引号
							gotEndQuote = true;
							break;
						}
					} else if (c == '\n' || c == '\r') {
						// 字符串不应该包含换行(在MiniScript中)
						break;
					}
				}
				
				// 检查是否正确结束
				if (!gotEndQuote) throw new LexerException("missing closing quote (\")");
				
				// 提取字符串内容(不包括引号)
				result.text = input.Substring(startPos, position-startPos-1);
				
				// 如果有转义的双引号,需要还原
				// 将所有""替换为"
				if (haveDoubledQuotes) result.text = result.text.Replace("\"\"", "\"");
				// Replace方法: 字符串替换,返回新字符串(字符串不可变)
				
				return result;

			} else {
				// 无法识别的字符
				result.type = Token.Type.Unknown;
			}

			// 设置Token的文本内容(从起始位置到当前位置)
			result.text = input.Substring(startPos, position - startPos);
			return result;
		}

		// ------------------------------------------------------------------------
		// SkipWhitespaceAndComment - 跳过空白字符和注释
		// ------------------------------------------------------------------------
		// 空白字符: 空格和制表符(不包括换行,因为换行是语句分隔符)
		// 注释: 从//开始到行尾
		void SkipWhitespaceAndComment() {
			// 跳过所有连续的空白字符
			while (!AtEnd && IsWhitespace(input[position])) {
				position++;
			}

			// 检查是否为注释开始(//)
			if (position < input.Length - 1 && 
			    input[position] == '/' && 
			    input[position + 1] == '/') {
				// 这是注释,跳到行尾
				position += 2;  // 跳过//
				while (!AtEnd && input[position] != '\n') position++;
				// 注意: 不跳过\n,因为\n是EOL token
			}
		}
		
		// ------------------------------------------------------------------------
		// IsNumeric - 判断字符是否为数字
		// ------------------------------------------------------------------------
		// static方法: 不需要实例即可调用,如 Lexer.IsNumeric('5')
		public static bool IsNumeric(char c) {
			return c >= '0' && c <= '9';  // 检查字符是否在'0'到'9'之间
		}

		// ------------------------------------------------------------------------
		// IsIdentifier - 判断字符是否可以作为标识符的一部分
		// ------------------------------------------------------------------------
		// 标识符可以包含:
		//   - 下划线_
		//   - 小写字母a-z
		//   - 大写字母A-Z
		//   - 数字0-9(但不能作为开头)
		//   - Unicode字符(字符码>0x9F,支持非英语标识符)
		public static bool IsIdentifier(char c) {
			return c == '_'
				|| (c >= 'a' && c <= 'z')
				|| (c >= 'A' && c <= 'Z')
				|| (c >= '0' && c <= '9')
				|| c > '\u009F';  // \u009F是Unicode转义序列
		}

		// ------------------------------------------------------------------------
		// IsWhitespace - 判断字符是否为空白字符
		// ------------------------------------------------------------------------
		// 仅包括空格和制表符
		// 注意: 不包括换行,因为换行在MiniScript中有语法意义
		public static bool IsWhitespace(char c) {
			return c == ' ' || c == '\t';
		}
		
		// ------------------------------------------------------------------------
		// IsAtWhitespace - 判断当前位置是否在空白字符处
		// ------------------------------------------------------------------------
		// 注意: 这个方法忽略pending队列,只检查当前position
		public bool IsAtWhitespace() {
			return AtEnd || IsWhitespace(input[position]);
		}

		// ------------------------------------------------------------------------
		// IsInStringLiteral - 判断给定位置是否在字符串字面量内
		// ------------------------------------------------------------------------
		// 静态方法,用于辅助分析源代码
		// 参数:
		//   charPos: 要检查的字符位置
		//   source: 源代码字符串
		//   startPos: 开始检查的位置(默认为0)
		// 返回: true如果charPos在字符串内,false否则
		// 
		// 工作原理: 从startPos开始扫描到charPos,追踪引号的开闭状态
		public static bool IsInStringLiteral(int charPos, string source, int startPos=0) {
			bool inString = false;  // 当前是否在字符串内
			for (int i=startPos; i<charPos; i++) {
				if (source[i] == '"') inString = !inString;  // 遇到引号切换状态
				// !运算符: 逻辑非,true变false,false变true
			}
			return inString;
		}

		// ------------------------------------------------------------------------
		// CommentStartPos - 找到行中注释的起始位置
		// ------------------------------------------------------------------------
		// 参数:
		//   source: 源代码字符串
		//   startPos: 开始搜索的位置
		// 返回: 注释//的位置,如果没有注释返回-1
		// 
		// 注意: 必须确保//不在字符串内
		// 例如: print "url: http://example.com"  // 这里的//在字符串内,不是注释
		public static int CommentStartPos(string source, int startPos) {
			// IndexOf方法: 查找子字符串第一次出现的位置
			// 从startPos-2开始是为了在循环中+2后从startPos开始
			int commentStart = startPos-2;
			while (true) {
				// 查找下一个//
				commentStart = source.IndexOf("//", commentStart + 2);
				if (commentStart < 0) break;	// 没找到,退出
				// 检查这个//是否在字符串内
				if (!IsInStringLiteral(commentStart, source, startPos)) break;	// 找到有效注释
				// 否则继续查找下一个//
			}
			return commentStart;
		}
		
		// ------------------------------------------------------------------------
		// TrimComment - 移除源代码行末的注释
		// ------------------------------------------------------------------------
		// 参数: source - 源代码字符串
		// 返回: 去除注释后的字符串
		public static string TrimComment(string source) {
			// 找到最后一行的开始位置
			int startPos = source.LastIndexOf('\n') + 1;
			// LastIndexOf: 查找字符最后一次出现的位置,没找到返回-1
			// +1是为了跳过\n本身,指向新行的第一个字符
			
			// 找到注释的位置
			int commentStart = CommentStartPos(source, startPos);
			
			if (commentStart >= 0) {
				// 返回从行首到注释开始的部分
				return source.Substring(startPos, commentStart - startPos);
			}
			// 没有注释,返回整行
			return source;
		}

		// ------------------------------------------------------------------------
		// LastToken - 找到源代码中的最后一个Token
		// ------------------------------------------------------------------------
		// 静态方法,用于分析源代码的最后一个有效Token
		// 忽略行尾的空白和注释
		// 
		// 参数: source - 源代码字符串
		// 返回: 最后一个Token对象
		// 
		// 应用场景: 在交互式解释器中,判断用户输入是否完整
		// 例如: 如果最后一个token是逗号,说明列表未完成,需要继续输入
		public static Token LastToken(string source) {
			// 步骤1: 找到最后一行的起始位置
			int startPos = source.LastIndexOf('\n') + 1;
			
			// 步骤2: 找到注释的位置(如果有)
			int commentStart = CommentStartPos(source, startPos);
			
			// 步骤3: 从字符串末尾或注释开始处向前,跳过空白
			int endPos = (commentStart >= 0 ? commentStart-1 : source.Length - 1);
			while (endPos >= 0 && IsWhitespace(source[endPos])) endPos--;
			if (endPos < 0) return Token.EOL;  // 整行都是空白
			
			// 步骤4: 找到最后一个Token的起始位置
			int tokStart = endPos;
			char c = source[endPos];
			
			// 根据字符类型确定Token的边界
			if (IsIdentifier(c)) {
				// 标识符或关键字: 向前扫描直到非标识符字符
				while (tokStart > startPos && IsIdentifier(source[tokStart-1])) tokStart--;
			} else if (c == '"') {
				// 字符串: 向前找到开始引号
				bool inQuote = true;
				while (tokStart > startPos) {
					tokStart--;
					if (source[tokStart] == '"') {
						inQuote = !inQuote;  // 切换引号状态
						// 找到开始引号且前面不是另一个引号(即不是转义的"")
						if (!inQuote && tokStart > startPos && source[tokStart-1] != '"') break;
					}
				}
			} else if (c == '=' && tokStart > startPos) {
				// 可能是双字符运算符(==, !=, >=, <=等)
				char c2 = source[tokStart-1];
				if (c2 == '>' || c2 == '<' || c2 == '=' || c2 == '!') tokStart--;
			}
			// 其他单字符Token不需要特殊处理
			
			// 步骤5: 使用标准Lexer解析这个Token
			Lexer lex = new Lexer(source);
			lex.position = tokStart;  // 设置起始位置
			return lex.Dequeue();     // 提取Token
		}

		// ========================================================================
		// 单元测试辅助方法
		// ========================================================================
		// 以下方法用于测试Lexer的正确性
		// UnitTest是MiniScript项目中的测试框架类
		
		// ------------------------------------------------------------------------
		// Check - 验证Token的类型和文本
		// ------------------------------------------------------------------------
		public static void Check(Token tok, Token.Type type, string text=null, int lineNum=0) {
			UnitTest.ErrorIfNull(tok);  // 检查Token是否为null
			if (tok == null) return;
			
			// 检查Token类型是否匹配
			UnitTest.ErrorIf(tok.type != type, "Token type: expected "
						+ type + ", but got " + tok.type);

			// 如果提供了text参数,检查文本是否匹配
			UnitTest.ErrorIf(text != null && tok.text != text,
						"Token text: expected " + text + ", but got " + tok.text);
		}

		// ------------------------------------------------------------------------
		// CheckLineNum - 验证行号是否正确
		// ------------------------------------------------------------------------
		public static void CheckLineNum(int actual, int expected) {
			UnitTest.ErrorIf(actual != expected, "Lexer line number: expected "
				+ expected + ", but got " + actual);
		}

		// ------------------------------------------------------------------------
		// RunUnitTests - 运行所有单元测试
		// ------------------------------------------------------------------------
		// 这个方法包含多个测试用例,验证Lexer的各种功能
		public static void RunUnitTests() {
			// ================================================================
			// 测试1: 基本数字和运算符
			// ================================================================
			Lexer lex = new Lexer("42  * 3.14158");
			Check(lex.Dequeue(), Token.Type.Number, "42");
			CheckLineNum(lex.lineNum, 1);
			Check(lex.Dequeue(), Token.Type.OpTimes);
			Check(lex.Dequeue(), Token.Type.Number, "3.14158");
			UnitTest.ErrorIf(!lex.AtEnd, "AtEnd not set when it should be");
			CheckLineNum(lex.lineNum, 1);

			// ================================================================
			// 测试2: 括号、小数、标识符、关键字、注释
			// ================================================================
			lex = new Lexer("6*(.1-foo) end if // and a comment!");
			Check(lex.Dequeue(), Token.Type.Number, "6");
			CheckLineNum(lex.lineNum, 1);
			Check(lex.Dequeue(), Token.Type.OpTimes);
			Check(lex.Dequeue(), Token.Type.LParen);
			Check(lex.Dequeue(), Token.Type.Number, ".1");
			Check(lex.Dequeue(), Token.Type.OpMinus);
			Check(lex.Peek(), Token.Type.Identifier, "foo");    // 测试Peek
			Check(lex.Peek(), Token.Type.Identifier, "foo");    // 再次Peek应返回同一Token
			Check(lex.Dequeue(), Token.Type.Identifier, "foo"); // 然后Dequeue
			Check(lex.Dequeue(), Token.Type.RParen);
			Check(lex.Dequeue(), Token.Type.Keyword, "end if"); // "end if"合并为一个Token
			Check(lex.Dequeue(), Token.Type.EOL);
			UnitTest.ErrorIf(!lex.AtEnd, "AtEnd not set when it should be");
			CheckLineNum(lex.lineNum, 1);

			// ================================================================
			// 测试3: 字符串字面量和转义的双引号
			// ================================================================
			lex = new Lexer("\"foo\" \"isn't \"\"real\"\"\" \"now \"\"\"\" double!\"");
			Check(lex.Dequeue(), Token.Type.String, "foo");
			Check(lex.Dequeue(), Token.Type.String, "isn't \"real\"");  // ""转义为"
			Check(lex.Dequeue(), Token.Type.String, "now \"\" double!"); // """"转义为""
			UnitTest.ErrorIf(!lex.AtEnd, "AtEnd not set when it should be");

			// ================================================================
			// 测试4: 各种换行符(\n, \r, \r\n)
			// ================================================================
			lex = new Lexer("foo\nbar\rbaz\r\nbamf");
			Check(lex.Dequeue(), Token.Type.Identifier, "foo");
			CheckLineNum(lex.lineNum, 1);
			Check(lex.Dequeue(), Token.Type.EOL);
			Check(lex.Dequeue(), Token.Type.Identifier, "bar");
			CheckLineNum(lex.lineNum, 2);
			Check(lex.Dequeue(), Token.Type.EOL);
			Check(lex.Dequeue(), Token.Type.Identifier, "baz");
			CheckLineNum(lex.lineNum, 3);
			Check(lex.Dequeue(), Token.Type.EOL);
			Check(lex.Dequeue(), Token.Type.Identifier, "bamf");
			CheckLineNum(lex.lineNum, 4);
			Check(lex.Dequeue(), Token.Type.EOL);
			UnitTest.ErrorIf(!lex.AtEnd, "AtEnd not set when it should be");
			
			// ================================================================
			// 测试5: 复合赋值运算符
			// ================================================================
			lex = new Lexer("x += 42");
			Check(lex.Dequeue(), Token.Type.Identifier, "x");
			CheckLineNum(lex.lineNum, 1);
			Check(lex.Dequeue(), Token.Type.OpAssignPlus);  // +=
			Check(lex.Dequeue(), Token.Type.Number, "42");
			UnitTest.ErrorIf(!lex.AtEnd, "AtEnd not set when it should be");
			
			// ================================================================
			// 测试6: LastToken方法测试
			// ================================================================
			Check(LastToken("x=42 // foo"), Token.Type.Number, "42");
			Check(LastToken("x = [1, 2, // foo"), Token.Type.Comma);
			Check(LastToken("x = [1, 2 // foo"), Token.Type.Number, "2");
			Check(LastToken("x = [1, 2 // foo // and \"more\" foo"), Token.Type.Number, "2");
			Check(LastToken("x = [\"foo\", \"//bar\"]"), Token.Type.RSquare);
			Check(LastToken("print 1 // line 1\nprint 2"), Token.Type.Number, "2");			
			Check(LastToken("print \"Hi\"\"Quote\" // foo bar"), Token.Type.String, "Hi\"Quote");			
		}
	}
}

/*
================================================================================
文件总结
================================================================================

1. 核心功能概述
--------------
MiniscriptLexer.cs 实现了MiniScript语言的词法分析器,是编译器/解释器的第一阶段。
主要职责是将源代码字符串转换为Token(词法单元)序列,为后续的语法分析做准备。

2. 主要类和职责
--------------
(1) Token类
   - 表示单个词法单元
   - 包含类型(Type枚举)和文本内容
   - 支持30多种Token类型(关键字、运算符、字面量等)
   
(2) Lexer类
   - 执行实际的词法分析
   - 维护当前读取位置和行号
   - 提供Peek(预览)和Dequeue(提取)方法
   - 处理注释、空白、换行等
   - 支持字符串转义、科学记数法等复杂格式

3. 关键设计特点
--------------
(1) 状态追踪
   - lineNum: 追踪当前行号,用于错误报告
   - position: 当前读取位置
   - pending: Token队列,支持Peek操作

(2) 特殊处理
   - "end" 关键字自动合并(end if, end function等)
   - "else if" 识别为单个关键字
   - 字符串内的双引号转义("")
   - 注释检测(避免在字符串内误判)
   - 多种换行符支持(\n, \r, \r\n)

(3) 数字格式支持
   - 整数: 42
   - 小数: 3.14, .5
   - 科学记数法: 1e10, 2.5E-3

4. 重要的C#特性使用
------------------
(1) 枚举(enum): Token.Type定义所有Token类型
(2) 属性(Property): AtEnd提供类似字段的访问
(3) 泛型集合: Queue<Token>队列
(4) 静态方法: 无需实例即可调用的工具方法
(5) 方法重写(override): 重写ToString方法
(6) 默认参数: 构造函数和方法参数的默认值
(7) 字符串方法: Substring, IndexOf, LastIndexOf, Replace等
(8) 异常处理: throw抛出LexerException

5. 设计模式
----------
(1) 单一职责: 每个方法只做一件事
(2) 辅助方法: IsNumeric, IsIdentifier等提高代码可读性
(3) 静态工具方法: 提供独立于实例的功能
(4) 单元测试: 内置测试验证功能正确性

6. 使用流程
----------
典型使用方式:
```csharp
Lexer lexer = new Lexer(sourceCode);
while (!lexer.AtEnd) {
    Token token = lexer.Dequeue();
    // 处理token
}