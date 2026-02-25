/*	MiniscriptParser.cs

This file is responsible for parsing MiniScript source code, and converting
it into an internal format (a three-address byte code) that is considerably
faster to execute.

This is normally wrapped by the Interpreter class, so you probably don't
need to deal with Parser directly.

这个文件负责解析MiniScript源代码,并将其转换为内部格式(三地址字节码),
这种格式执行速度要快得多。

通常由Interpreter类封装,因此您可能不需要直接处理Parser。
*/
using System;
using System.Collections.Generic;
using System.Linq;  // LINQ: Language Integrated Query - C#的集合查询功能
using System.Globalization;  // 用于处理不同文化的数字格式

namespace Miniscript {
	public class Parser {

		// 错误上下文信息,用于错误报告(如文件名)
		public string errorContext;	// name of file, etc., used for error reporting
									// 文件名等,用于错误报告

		//public int lineNum;			// which line number we're currently parsing
										// 当前正在解析的行号

		// BackPatch: 表示代码中需要回填的位置
		// 当我们遇到需要跳转但还不知道跳转目标的情况时使用
		// 例如: if语句中,我们需要跳转到else或end if,但在解析if时还不知道具体位置
		// BackPatch: represents a place where we need to patch the code to fill
		// in a jump destination (once we figure out where that destination is).
		// 表示需要修补代码以填充跳转目标的位置(一旦我们确定了该目标的位置)。
		class BackPatch {
			public int lineNum;			// which code line to patch
										// 需要修补的代码行号
			public string waitingFor;	// what keyword we're waiting for (e.g., "end if")
										// 我们正在等待的关键字(例如,"end if")
		}

		// JumpPoint: 表示代码中稍后需要跳转到的位置
		// 通常是某种循环的顶部
		// 例如: while循环需要在循环体结束后跳回开始位置
		// JumpPoint: represents a place in the code we will need to jump to later
		// (typically, the top of a loop of some sort).
		// 表示代码中稍后需要跳转到的位置(通常是某种循环的顶部)。
		class JumpPoint {
			public int lineNum;			// line number to jump to
										// 要跳转到的行号
			public string keyword;		// jump type, by keyword: "while", "for", etc.
										// 通过关键字标识跳转类型:"while"、"for"等
		}

		/// <summary>
		/// ParseState: 解析状态类
		/// 封装了解析过程中的所有状态信息
		/// 当解析函数时,会创建新的ParseState并压入栈中
		/// </summary>
		class ParseState {
			// 生成的TAC(三地址码)指令列表
			public List<TAC.Line> code = new List<TAC.Line>();

			// 待回填的跳转列表(需要后续确定跳转目标的指令)
			public List<BackPatch> backpatches = new List<BackPatch>();

			// 跳转点列表(循环开始位置等)
			public List<JumpPoint> jumpPoints = new List<JumpPoint>();

			// 下一个临时变量编号(临时变量用于存储中间计算结果)
			public int nextTempNum = 0;

			// 仅在局部作用域查找的标识符
			public string localOnlyIdentifier;	// identifier to be looked up in local scope *only*
												// 仅在局部作用域中查找的标识符

			// 是否严格限制localOnlyIdentifier(严格模式会报错,非严格仅警告)
			public bool localOnlyStrict;		// whether localOnlyIdentifier applies strictly, or merely warns
												// localOnlyIdentifier是否严格应用,或仅发出警告

			/// <summary>
			/// 添加一行TAC代码到代码列表
			/// </summary>
			/// <param name="line">要添加的TAC指令行</param>
			public void Add(TAC.Line line) {
				code.Add(line);
			}

			/// <summary>
			/// 将最后一行代码添加为回填点
			/// 当遇到指定的关键字时,会回填这行代码的rhsA字段(跳转目标)
			///
			/// 使用场景示例:
			/// if condition then  <- 这里生成条件跳转,但还不知道跳到哪
			///   statements
			/// else               <- 这里才知道要跳到这个位置
			/// </summary>
			/// <param name="waitFor">等待的关键字</param>
			public void AddBackpatch(string waitFor) {
				backpatches.Add(new BackPatch() { lineNum=code.Count-1, waitingFor=waitFor });
			}

			/// <summary>
			/// 添加跳转点(通常是循环的开始位置)
			/// 循环结束时需要跳回这个位置
			/// </summary>
			/// <param name="jumpKeyword">跳转关键字(while/for等)</param>
			public void AddJumpPoint(string jumpKeyword) {
				jumpPoints.Add(new JumpPoint() { lineNum = code.Count, keyword = jumpKeyword });
			}

			/// <summary>
			/// 关闭跳转点(循环结束时调用)
			/// 返回跳转点信息并从列表中移除
			///
			/// 工作原理:
			/// 1. 检查最后一个跳转点是否匹配关键字
			/// 2. 如果不匹配,说明块结构不对称(如while没有end while)
			/// 3. 返回跳转点并从列表移除
			/// </summary>
			/// <param name="keyword">关键字(while/for等)</param>
			/// <returns>对应的跳转点</returns>
			public JumpPoint CloseJumpPoint(string keyword) {
				int idx = jumpPoints.Count - 1;
				if (idx < 0 || jumpPoints[idx].keyword != keyword) {
					throw new CompilerException(string.Format("'end {0}' without matching '{0}'", keyword));
				}
				JumpPoint result = jumpPoints[idx];
				jumpPoints.RemoveAt(idx);
				return result;
			}

			/// <summary>
			/// 检查给定行号是否是跳转目标
			/// 用于优化:如果某行是跳转目标,则不能进行某些优化
			///
			/// 检查两个来源:
			/// 1. 已生成的代码中的跳转指令
			/// 2. 待跳转的jumpPoints列表
			/// </summary>
			/// <param name="lineNum">要检查的行号</param>
			/// <returns>是否为跳转目标</returns>
			public bool IsJumpTarget(int lineNum) {
				// 遍历所有已生成的代码行
				for (int i=0; i < code.Count; i++) {
					var op = code[i].op;
					// 检查是否是跳转指令,且跳转目标是指定行号
					if ((op == TAC.Line.Op.GotoA || op == TAC.Line.Op.GotoAifB
					 || op == TAC.Line.Op.GotoAifNotB || op == TAC.Line.Op.GotoAifTrulyB)
					 && code[i].rhsA is ValNumber && code[i].rhsA.IntValue() == lineNum) return true;
				}
				// 检查是否在跳转点列表中
				for (int i=0; i<jumpPoints.Count(); i++) {
					if (jumpPoints[i].lineNum == lineNum) return true;
				}
				return false;
			}

			/// <summary>
			/// 当遇到'end'关键字时调用此方法
			/// 回填所有等待该关键字的跳转指令
			/// 将匹配的回填点(及其后的)修补到当前代码末尾
			///
			/// 工作流程:
			/// 1. 找到所有等待指定关键字的backpatch
			/// 2. 将它们的跳转目标设置为当前位置+reservingLines
			/// 3. 从backpatch列表中移除已处理的项
			/// </summary>
			/// <param name="keywordFound">找到的关键字</param>
			/// <param name="reservingLines">预留的额外行数(在当前位置之后)</param>
			public void Patch(string keywordFound, int reservingLines=0) {
				Patch(keywordFound, false, reservingLines);
			}

			/// <summary>
			/// Patch方法的完整版本
			/// 回填等待指定关键字的跳转,并可选择是否同时回填break语句
			///
			/// 参数说明:
			/// - alsoBreak: break语句可以跳出任何循环,所以可能需要特殊处理
			/// - reservingLines: 有时需要在当前位置后预留几行,跳转到预留位置之后
			/// </summary>
			/// <param name="keywordFound">找到的关键字</param>
			/// <param name="alsoBreak">是否同时修补"break";否则跳过它</param>
			/// <param name="reservingLines">预留的额外行数(在当前位置之后)</param>
			public void Patch(string keywordFound, bool alsoBreak, int reservingLines=0) {
				// 计算跳转目标:当前代码行数+预留行数
				Value target = TAC.Num(code.Count + reservingLines);
				bool done = false;

				// 从后向前遍历backpatch列表(栈式处理)
				for (int idx = backpatches.Count - 1; idx >= 0 && !done; idx--) {
					bool patchIt = false;
					if (backpatches[idx].waitingFor == keywordFound) {
						// 找到匹配的关键字,标记需要patch并设置done=true
						patchIt = done = true;
					} else if (backpatches[idx].waitingFor == "break") {
						// 不是期望的关键字,但是是"break"
						// break总是可以的,但是否patch取决于alsoBreak参数
						patchIt = alsoBreak;
					} else {
						// 既不是期望的关键字,也不是break;说明块结构不匹配
						throw new CompilerException("'" + keywordFound + "' skips expected '" + backpatches[idx].waitingFor + "'");
					}
					if (patchIt) {
						// 修补跳转目标
						code[backpatches[idx].lineNum].rhsA = target;
						backpatches.RemoveAt(idx);
					}
				}
				// 确保找到了至少一个匹配的backpatch
				if (!done) throw new CompilerException("'" + keywordFound + "' without matching block starter");
			}

			/// <summary>
			/// 修补单个if块的所有分支
			/// 包括最后的"else"块,以及一个或多个"end if"跳转
			///
			/// if块的结构比较复杂:
			/// if cond then
			///   ...         <- 条件为假时跳到else
			/// else          <- if部分结束时跳到end if
			///   ...
			/// end if        <- else结束跳到这里
			///
			/// 还要处理单行if的情况
			/// </summary>
			/// <param name="singleLineIf">是否是单行if</param>
			public void PatchIfBlock(bool singleLineIf) {
				Value target = TAC.Num(code.Count);

				int idx = backpatches.Count - 1;
				while (idx >= 0) {
					BackPatch bp = backpatches[idx];
					if (bp.waitingFor == "if:MARK") {
						// 找到特殊标记,表示if块的真正开始
						// 这个标记在ParseStatement中if语句处理时添加
						backpatches.RemoveAt(idx);
						return;
					} else if (bp.waitingFor == "end if" || bp.waitingFor == "else") {
						// 回填"end if"或"else"的跳转
						code[bp.lineNum].rhsA = target;
						backpatches.RemoveAt(idx);
					} else if (backpatches[idx].waitingFor == "break") {
						// break语句,总是允许的,不处理
					} else {
						// 其他情况:块结构不匹配
						string msg;
						if (singleLineIf) {
							// 单行if的特殊错误消息
							if (bp.waitingFor == "end for" || bp.waitingFor == "end while") {
								msg = "loop is invalid within single-line 'if'";
							} else {
								msg = "invalid control structure within single-line 'if'";
							}
						} else {
							msg = "'end if' without matching 'if'";
						}
						throw new CompilerException(msg);
					}
					idx--;
				}
				// 如果到这里,说明没找到if:MARK标记,这是错误
				throw new CompilerException("'end if' without matching 'if'");
			}
		}

		// 部分输入(当使用行继续符时的情况)
		// 例如: x = 1 +
		//          2  <- 这两行会被合并
		// Partial input, in the case where line continuation has been used.
		// 使用行继续符时的部分输入。
		string partialInput;

		// 正在处理的代码块列表(栈结构)
		// 当编译函数时,会将新的ParseState压入此栈
		// 到达函数结束时,将其弹出
		// 这是一个Stack<T>,C#的泛型集合类
		// List of open code blocks we're working on (while compiling a function,
		// we push a new one onto this stack, compile to that, and then pop it
		// off when we reach the end of the function).
		// 我们正在处理的打开代码块列表(在编译函数时,
		// 我们将一个新的压入此栈,编译到其中,然后在到达函数结束时将其弹出)。
		Stack<ParseState> outputStack;

		// 指向outputStack顶部的便捷引用
		// 总是指向当前正在编译的ParseState
		// Handy reference to the top of outputStack.
		// outputStack顶部的便捷引用。
		ParseState output;

		// 需要压入栈的新解析状态
		// 一旦完成当前行的处理就会压入
		// 用于函数定义:遇到function关键字时创建,当前语句结束后压入
		// A new parse state that needs to be pushed onto the stack, as soon as we
		// finish with the current line we're working on:
		// 需要被压入栈的新解析状态,一旦我们完成当前正在处理的行:
		ParseState pendingState = null;

		/// <summary>
		/// 构造函数
		/// 创建Parser实例并初始化
		/// </summary>
		public Parser() {
			Reset();
		}

		/// <summary>
		/// 完全清除并重置解析状态
		/// 丢弃所有代码和中间结果
		///
		/// 使用场景:开始解析新的源代码文件时
		/// </summary>
		public void Reset() {
			output = new ParseState();
			if (outputStack == null) outputStack = new Stack<ParseState>();
			else outputStack.Clear();
			outputStack.Push(output);
		}

		/// <summary>
		/// 部分重置,放弃回填点,但保留已编译的代码
		/// 在REPL(交互式解释器)中使用,当用户可能想要在错误的循环或函数后重置并继续时
		///
		/// 使用场景:用户在REPL中输入了未完成的循环或函数,想要取消并继续
		/// </summary>
		public void PartialReset() {
			if (outputStack == null) outputStack = new Stack<ParseState>();
			// 弹出所有函数级别的栈,只保留顶层
			while (outputStack.Count > 1) outputStack.Pop();
			output = outputStack.Peek();
			// 清除所有待处理的结构
			output.backpatches.Clear();
			output.jumpPoints.Clear();
			output.nextTempNum = 0;
			partialInput = null;
			pendingState = null;
		}

		/// <summary>
		/// 检查是否需要更多输入
		///
		/// 返回true的情况:
		/// 1. 有部分输入(行继续)
		/// 2. 有未完成的函数(栈深度>1)
		/// 3. 有未回填的跳转(未闭合的块结构)
		/// </summary>
		/// <returns>是否需要更多输入</returns>
		public bool NeedMoreInput() {
			if (!string.IsNullOrEmpty(partialInput)) return true;
			if (outputStack.Count > 1) return true;
			if (output.backpatches.Count > 0) return true;
			return false;
		}

		/// <summary>
		/// 检查给定的源代码是否以表示语句在下一行继续的标记结束
		/// 包括二元运算符、开括号或圆括号等
		///
		/// 实现原理:
		/// 1. 获取源代码的最后一个token
		/// 2. 检查该token类型是否暗示语句未完成
		///
		/// 例如:
		/// x = 1 +     <- 以+结尾,需要继续
		/// x = [1,     <- 以逗号结尾,需要继续
		/// x = func(   <- 以(结尾,需要继续
		/// </summary>
		/// <param name="sourceCode">要分析的源代码</param>
		/// <returns>如果需要行继续则返回true;否则返回false</returns>
		public static bool EndsWithLineContinuation(string sourceCode) {
 			try {
				// Lexer.LastToken: 静态方法,返回源代码的最后一个token
				Token lastTok = Lexer.LastToken(sourceCode);
				// 几乎任何token结尾都表示行继续,除了:
				switch (lastTok.type) {
				case Token.Type.EOL:          // 行尾
				case Token.Type.Identifier:   // 标识符
				case Token.Type.Number:       // 数字
				case Token.Type.RCurly:       // 右花括号 }
				case Token.Type.RParen:       // 右圆括号 )
				case Token.Type.RSquare:      // 右方括号 ]
				case Token.Type.String:       // 字符串
				case Token.Type.Unknown:      // 未知token
					return false;
				case Token.Type.Keyword:
					// 关键字中,只有这些会导致行继续:
					return lastTok.text == "and" || lastTok.text == "or" || lastTok.text == "isa"
							|| lastTok.text == "not" || lastTok.text == "new";
				default:
					// 其他所有情况(运算符、左括号等)都需要继续
					return true;
				}
			} catch (LexerException) {
				// 词法错误时假设不需要继续
				return false;
			}
		}

		/// <summary>
		/// 检查是否有未闭合的回填点
		/// 如果有,抛出相应的错误信息
		///
		/// 使用场景:文件结束但仍有未闭合的结构
		/// </summary>
		/// <param name="sourceLineNum">源代码行号</param>
		void CheckForOpenBackpatches(int sourceLineNum) {
			if (output.backpatches.Count == 0) return;
			BackPatch bp = output.backpatches[output.backpatches.Count - 1];
			string msg;
			// 根据等待的关键字生成相应的错误消息
			switch (bp.waitingFor) {
			case "end for":
				msg = "'for' without matching 'end for'";
				break;
			case "end if":
			case "else":
				msg = "'if' without matching 'end if'";
				break;
			case "end while":
				msg = "'while' without matching 'end while'";
				break;
			default:
				msg = "unmatched block opener";
				break;
			}
			throw new CompilerException(errorContext, sourceLineNum, msg);
		}

		/// <summary>
		/// 解析源代码的主入口点
		///
		/// 工作流程:
		/// 1. 处理行继续(如果在REPL模式)
		/// 2. 创建词法分析器(Lexer)
		/// 3. 调用ParseMultipleLines解析所有语句
		/// 4. 检查是否需要更多输入
		///
		/// REPL模式特点:
		/// - 检测行继续,累积部分输入
		/// - 允许交互式输入
		/// </summary>
		/// <param name="sourceCode">源代码字符串</param>
		/// <param name="replMode">是否为REPL模式</param>
		public void Parse(string sourceCode, bool replMode=false) {
			if (replMode) {
				// 通过查找最后一个(非注释)token来检查是否有不完整的最后一行
				bool isPartial = EndsWithLineContinuation(sourceCode);
				if (isPartial) {
					// 移除注释并累积到partialInput
					partialInput += Lexer.TrimComment(sourceCode);
					partialInput += " ";
					return;
				}
			}
			// 创建词法分析器,将源代码分解为token流
			Lexer tokens = new Lexer(partialInput + sourceCode);
			partialInput = null;
			// 解析多行语句
			ParseMultipleLines(tokens);

			if (!replMode && NeedMoreInput()) {
				// 非REPL模式下需要更多输入是错误
				tokens.lineNum++;	// 报告到最后一行之后,表明这是EOF问题
				if (outputStack.Count > 1) {
					throw new CompilerException(errorContext, tokens.lineNum,
						"'function' without matching 'end function'");
				}
				CheckForOpenBackpatches(tokens.lineNum);
			}
		}

		/// <summary>
		/// 创建加载了已解析代码的虚拟机
		///
		/// TAC.Context: 三地址码执行上下文
		/// TAC.Machine: 虚拟机,执行TAC指令
		/// </summary>
		/// <param name="standardOutput">标准输出方法</param>
		/// <returns>配置好的虚拟机</returns>
		public TAC.Machine CreateVM(TextOutputMethod standardOutput) {
			TAC.Context root = new TAC.Context(output.code);
			return new TAC.Machine(root, standardOutput);
		}

		/// <summary>
		/// 使用已解析的代码创建Function,用作import
		/// 这意味着,它运行所有代码,然后在末尾返回`locals`,
		/// 以便调用者可以获取其符号(导入的变量/函数)
		///
		/// 工作原理:
		/// 1. 添加一行代码:return locals
		/// 2. 将整个代码包装为Function
		/// 3. 导入时执行这个函数,获取所有定义的符号
		/// </summary>
		/// <returns>可用作导入的函数</returns>
		public Function CreateImport() {
			// 添加一行额外的代码以返回`locals`作为函数返回值
			ValVar locals = new ValVar("locals");
			output.Add(new TAC.Line(TAC.LTemp(0), TAC.Line.Op.ReturnA, locals));
			// 然后将整个内容包装在Function中
			var result = new Function(output.code);
			return result;
		}

		/// <summary>
		/// REPL(Read-Eval-Print Loop)入口
		/// 解析一行代码并立即执行
		///
		/// 步骤:
		/// 1. 解析输入
		/// 2. 创建虚拟机
		/// 3. 执行到完成
		/// </summary>
		/// <param name="line">输入的代码行</param>
		public void REPL(string line) {
			Parse(line);

			TAC.Machine vm = CreateVM(null);
			// Step(): 单步执行虚拟机
			while (!vm.done) vm.Step();
		}

		/// <summary>
		/// 允许换行
		/// 跳过所有EOL token,直到遇到非EOL token或到达末尾
		///
		/// 使用场景:
		/// - 二元运算符后
		/// - 括号内
		/// - 逗号后
		/// </summary>
		/// <param name="tokens">Token流</param>
		void AllowLineBreak(Lexer tokens) {
			while (tokens.Peek().type == Token.Type.EOL && !tokens.AtEnd) tokens.Dequeue();
		}

		// 委托(delegate): C#中的函数指针类型
		// 定义一个可以指向特定签名方法的类型
		// 这里定义的是表达式解析方法的签名
		// 用途:不同优先级的运算符解析使用不同的方法,但有相同的签名
		delegate Value ExpressionParsingMethod(Lexer tokens, bool asLval=false, bool statementStart=false);

		/// <summary>
		/// 解析多条语句,直到token用完或遇到'end function'
		///
		/// 核心流程:
		/// 1. 跳过空行
		/// 2. 处理'end function'(弹出解析栈)
		/// 3. 解析单条语句
		/// 4. 为生成的代码填充位置信息(用于错误报告)
		/// </summary>
		/// <param name="tokens">Token流</param>
		void ParseMultipleLines(Lexer tokens) {
			while (!tokens.AtEnd) {
				// 跳过任何空行
				if (tokens.Peek().type == Token.Type.EOL) {
					tokens.Dequeue();
					continue;
				}

				// 为错误报告准备源代码位置
				SourceLoc location = new SourceLoc(errorContext, tokens.lineNum);

				// 如果到达'end function',弹出上下文
				if (tokens.Peek().type == Token.Type.Keyword && tokens.Peek().text == "end function") {
					tokens.Dequeue();
					if (outputStack.Count > 1) {
						// 检查是否有未闭合的结构
						CheckForOpenBackpatches(tokens.lineNum);
						outputStack.Pop();
						output = outputStack.Peek();
					} else {
						// 没有对应的function,报错
						CompilerException e = new CompilerException("'end function' without matching block starter");
						e.location = location;
						throw e;
					}
					continue;
				}

				// 解析一行(语句)
				int outputStart = output.code.Count;
				try {
					ParseStatement(tokens);
				} catch (MiniscriptException mse) {
					// 为异常添加位置信息
					if (mse.location == null) mse.location = location;
					throw;
				}
				// 为刚生成的所有TAC行填充位置信息
				for (int i = outputStart; i < output.code.Count; i++) {
					output.code[i].location = location;
				}
			}
		}

		/// <summary>
		/// 解析单条语句
		///
		/// MiniScript的语句类型:
		/// 1. 关键字语句: if/while/for/return/break/continue
		/// 2. 赋值语句: x = expr
		/// 3. 复合赋值: x += expr, x -= expr等
		/// 4. 命令语句: print "hello" (无括号的函数调用)
		///
		/// allowExtra参数:
		/// - 用于单行if: if cond then statement
		/// - 允许语句后跟else而不是EOL
		/// </summary>
		/// <param name="tokens">Token流</param>
		/// <param name="allowExtra">是否允许额外的内容(不要求EOL)</param>
		void ParseStatement(Lexer tokens, bool allowExtra=false) {
			if (tokens.Peek().type == Token.Type.Keyword && tokens.Peek().text != "not"
				&& tokens.Peek().text != "true" && tokens.Peek().text != "false") {
				// 处理以关键字开头的语句
				string keyword = tokens.Dequeue().text;
				switch (keyword) {
				case "return":
					{
						Value returnValue = null;
						// 解析返回值(如果有)
						if (tokens.Peek().type != Token.Type.EOL && tokens.Peek().text != "else" && tokens.Peek().text != "else if") {
							returnValue = ParseExpr(tokens);
						}
						// 生成return指令,结果存到临时变量0(约定的返回值位置)
						output.Add(new TAC.Line(TAC.LTemp(0), TAC.Line.Op.ReturnA, returnValue));
					}
					break;
				case "if":
					{
						// 解析if语句
						Value condition = ParseExpr(tokens);
						RequireToken(tokens, Token.Type.Keyword, "then");

						// 生成条件跳转:如果条件为假,跳过then部分
						// 现在还不知道跳转目标,添加到backpatch列表
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoAifNotB, null, condition));

						// if块需要特殊标记,用于PatchIfBlock中识别if块的开始
						// 因为可能有多个需要回填的跳转(if、else if等)
						output.AddBackpatch("if:MARK");
						output.AddBackpatch("else");

						// 处理单行if的特殊情况
						// 单行if: if cond then statement [else statement]
						if (tokens.Peek().type != Token.Type.EOL) {
							ParseStatement(tokens, true);  // 解析then部分的单条语句
							if (tokens.Peek().type == Token.Type.Keyword && tokens.Peek().text == "else") {
								tokens.Dequeue();	// 跳过"else"
								StartElseClause();
								ParseStatement(tokens, true);		// 解析else部分的单条语句
							} else if (tokens.Peek().type == Token.Type.Keyword && tokens.Peek().text == "else if") {
								// 巧妙的技巧:将"else if"转换为常规"if"
								tokens.Peek().text = "if";		// 修改token为"if"
								StartElseClause();				// 开始else子句
								ParseStatement(tokens, true);	// 解析以"if"开头的单条语句
							} else {
								// 单行if后必须是else或EOL
								RequireEitherToken(tokens, Token.Type.Keyword, "else", Token.Type.EOL);
							}
							output.PatchIfBlock(true);	// 终止单行if
						} else {
							tokens.Dequeue();	// 跳过EOL
						}
					}
					return;
				case "else":
					StartElseClause();
					break;
				case "else if":
					{
						// else if处理
						StartElseClause();
						Value condition = ParseExpr(tokens);
						RequireToken(tokens, Token.Type.Keyword, "then");
						// 生成条件跳转,添加回填点
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoAifNotB, null, condition));
						output.AddBackpatch("else");
					}
					break;
				case "end if":
					// 回填if块的所有跳转
					// 这是复杂的,因为可能有多个跳转需要处理
					output.PatchIfBlock(false);
					break;
				case "while":
					{
						// while循环处理
						// 1. 记录循环开始位置(用于continue和循环结束时的跳转)
						output.AddJumpPoint(keyword);

						// 2. 解析循环条件
						Value condition = ParseExpr(tokens);

						// 3. 生成条件跳转:条件为假时跳出循环
						// 跳转目标稍后回填(在end while时)
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoAifNotB, null, condition));
						output.AddBackpatch("end while");
					}
					break;
				case "end while":
					{
						// while循环结束处理
						// 1. 获取循环开始位置
						JumpPoint jump = output.CloseJumpPoint("while");
						// 2. 无条件跳回循环开始
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoA, TAC.Num(jump.lineNum)));
						// 3. 回填while开头的条件跳转,跳到这里(循环后)
						// 同时回填所有break语句(alsoBreak=true)
						output.Patch(keyword, true);
					}
					break;
				case "for":
					{
						// for循环处理
						// MiniScript的for循环: for item in sequence

						// 1. 获取循环变量、"in"关键字和序列表达式
						Token loopVarTok = RequireToken(tokens, Token.Type.Identifier);
						ValVar loopVar = new ValVar(loopVarTok.text);
						RequireToken(tokens, Token.Type.Keyword, "in");
						Value stuff = ParseExpr(tokens);
						if (stuff == null) {
							throw new CompilerException(errorContext, tokens.lineNum,
								"sequence expression expected for 'for' loop");
						}

						// 2. 创建索引变量,初始化为-1
						// 索引变量名: __循环变量名_idx
						ValVar idxVar = new ValVar("__" + loopVarTok.text + "_idx");
						output.Add(new TAC.Line(idxVar, TAC.Line.Op.AssignA, TAC.Num(-1)));

						// 3. 记录循环开始位置
						output.AddJumpPoint(keyword);

						// 4. 索引递增
						output.Add(new TAC.Line(idxVar, TAC.Line.Op.APlusB, idxVar, TAC.Num(1)));

						// 5. 检查索引是否超出序列长度,如果是则跳出循环
						ValTemp sizeOfSeq = new ValTemp(output.nextTempNum++);
						output.Add(new TAC.Line(sizeOfSeq, TAC.Line.Op.LengthOfA, stuff));
						ValTemp isTooBig = new ValTemp(output.nextTempNum++);
						output.Add(new TAC.Line(isTooBig, TAC.Line.Op.AGreatOrEqualB, idxVar, sizeOfSeq));
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoAifB, null, isTooBig));
						output.AddBackpatch("end for");

						// 6. 从序列中获取当前元素到循环变量
						output.Add(new TAC.Line(loopVar, TAC.Line.Op.ElemBofIterA, stuff, idxVar));
					}
					break;
				case "end for":
					{
						// for循环结束处理
						// 1. 无条件跳回循环开始
						JumpPoint jump = output.CloseJumpPoint("for");
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoA, TAC.Num(jump.lineNum)));
						// 2. 回填循环开头的边界检查跳转,跳到这里
						// 同时回填所有break语句
						output.Patch(keyword, true);
					}
					break;
				case "break":
					{
						// break语句:跳出循环
						// 生成跳转指令,稍后回填(在end while/end for时)
						if (output.jumpPoints.Count == 0) {
							throw new CompilerException(errorContext, tokens.lineNum,
								"'break' without open loop block");
						}
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoA));
						output.AddBackpatch("break");
					}
					break;
				case "continue":
					{
						// continue语句:跳到循环开始
						// 无条件跳转到当前打开的跳转点
						if (output.jumpPoints.Count == 0) {
							throw new CompilerException(errorContext, tokens.lineNum,
								"'continue' without open loop block");
						}
						JumpPoint jump = output.jumpPoints.Last();  // Last(): LINQ方法,获取最后一个元素
						output.Add(new TAC.Line(null, TAC.Line.Op.GotoA, TAC.Num(jump.lineNum)));
					}
					break;
				default:
					throw new CompilerException(errorContext, tokens.lineNum,
						"unexpected keyword '" + keyword + "' at start of line");
				}
			} else {
				// 不是关键字开头,解析为赋值或命令语句
				ParseAssignment(tokens, allowExtra);
			}

			// 语句应该消耗到行尾的所有内容
			if (!allowExtra) RequireToken(tokens, Token.Type.EOL);

			// 最后,如果有待处理的状态(因为遇到了function())
			// 现在语句处理完成,将其压入栈
			if (pendingState != null) {
				output = pendingState;
				outputStack.Push(output);
				pendingState = null;
			}

		}

		/// <summary>
		/// 开始else子句
		///
		/// 工作流程:
		/// 1. 生成从if块末尾到end if的跳转(跳过else部分)
		/// 2. 回填之前的跳转(让它跳到这里,else的开始)
		/// 3. 为新生成的跳转添加回填点
		///
		/// 示例:
		/// if cond then
		///   ...         <- 条件为假时跳到这里 ↓
		///   goto END    <- 执行完then后跳过else
		/// else:         <- ↑ 之前的跳转到这里
		///   ...
		/// END:          <- ↑ goto END跳到这里
		/// </summary>
		void StartElseClause() {
			// 从当前位置(if块末尾)跳转到else块末尾
			// 跳转目标稍后回填
			output.Add(new TAC.Line(null, TAC.Line.Op.GotoA, null));
			// 回填之前打开的if块,让它跳到这里(刚才的goto之后)
			output.Patch("else");
			// 为刚才的goto添加回填点(将在end if时回填)
			output.AddBackpatch("end if");
		}

		/// <summary>
		/// 解析赋值语句或命令语句
		///
		/// MiniScript支持多种赋值形式:
		/// 1. 简单赋值: x = expr
		/// 2. 复合赋值: x += expr, x -= expr, x *= expr, x /= expr, x %= expr, x ^= expr
		/// 3. 命令语句: print "hello" (无括号函数调用)
		/// 4. 隐式结果: 单独的表达式,结果存到隐式变量
		///
		/// 优化:
		/// - 如果最后一条指令是对RHS临时变量的赋值,且没有跳转到下一行,
		///   则直接修改那条指令的LHS,避免额外的赋值操作
		/// </summary>
		/// <param name="tokens">Token流</param>
		/// <param name="allowExtra">是否允许额外内容</param>
		void ParseAssignment(Lexer tokens, bool allowExtra=false) {
			// 解析左侧表达式(可能是lvalue,如x或x[i])
			Value expr = ParseExpr(tokens, true, true);
			Value lhs, rhs;
			Token peek = tokens.Peek();

			if (peek.type == Token.Type.EOL ||
					(peek.type == Token.Type.Keyword && (peek.text == "else" || peek.text == "else if"))) {
				// 没有显式赋值;存储隐式结果
				// 例如: 2 + 2  (在REPL中会显示结果)
				rhs = FullyEvaluate(expr);
				output.Add(new TAC.Line(null, TAC.Line.Op.AssignImplicit, rhs));
				return;
			}

			if (peek.type == Token.Type.OpAssign) {
				// 简单赋值: x = expr
				tokens.Dequeue();	// 跳过'='
				lhs = expr;

				// 设置localOnly标识符
				// 如果赋值左侧是变量,右侧对该变量的引用应该在局部作用域查找
				output.localOnlyIdentifier = null;
				output.localOnlyStrict = false;
				if (lhs is ValVar vv) output.localOnlyIdentifier = vv.identifier;

				rhs = ParseExpr(tokens);
				output.localOnlyIdentifier = null;

			} else if (peek.type == Token.Type.OpAssignPlus || peek.type == Token.Type.OpAssignMinus
				    || peek.type == Token.Type.OpAssignTimes || peek.type == Token.Type.OpAssignDivide
				    || peek.type == Token.Type.OpAssignMod || peek.type == Token.Type.OpAssignPower) {
				// 复合赋值: x += expr 等
				// 等价于: x = x + expr

				// 确定操作类型
				var op = TAC.Line.Op.APlusB;
				switch (tokens.Dequeue().type) {
				case Token.Type.OpAssignMinus:		op = TAC.Line.Op.AMinusB;		break;
				case Token.Type.OpAssignTimes:		op = TAC.Line.Op.ATimesB;		break;
				case Token.Type.OpAssignDivide:		op = TAC.Line.Op.ADividedByB;	break;
				case Token.Type.OpAssignMod:		op = TAC.Line.Op.AModB;			break;
				case Token.Type.OpAssignPower:		op = TAC.Line.Op.APowB;			break;
				default: break;
				}

				lhs = expr;

				// 设置严格的localOnly模式
				output.localOnlyIdentifier = null;
				output.localOnlyStrict = true;
				if (lhs is ValVar vv) output.localOnlyIdentifier = vv.identifier;

				rhs = ParseExpr(tokens);

				// 生成: temp = lhs op rhs
				var opA = FullyEvaluate(lhs, ValVar.LocalOnlyMode.Strict);
				Value opB = FullyEvaluate(rhs);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), op, opA, opB));
				rhs = TAC.RTemp(tempNum);
				output.localOnlyIdentifier = null;

			} else {
				// 这看起来像命令语句
				// 解析行的其余部分作为函数调用的参数
				// 例如: print "hello", "world"
				Value funcRef = expr;
				int argCount = 0;
				while (true) {
					Value arg = ParseExpr(tokens);
					// PushParam: 将参数压入参数栈
					output.Add(new TAC.Line(null, TAC.Line.Op.PushParam, arg));
					argCount++;
					if (tokens.Peek().type == Token.Type.EOL) break;
					if (tokens.Peek().type == Token.Type.Keyword && (tokens.Peek().text == "else" || tokens.Peek().text == "else if")) break;
					if (tokens.Peek().type == Token.Type.Comma) {
						tokens.Dequeue();
						AllowLineBreak(tokens);
						continue;
					}
					if (RequireEitherToken(tokens, Token.Type.Comma, Token.Type.EOL).type == Token.Type.EOL) break;
				}
				// 调用函数
				ValTemp result = new ValTemp(output.nextTempNum++);
				output.Add(new TAC.Line(result, TAC.Line.Op.CallFunctionA, funcRef, TAC.Num(argCount)));
				output.Add(new TAC.Line(null, TAC.Line.Op.AssignImplicit, result));
				return;
			}

			// 现在需要将rhs的值赋给lhs

			// 检查lhs是否是临时变量;这表示它不是lvalue
			// (例如,它可能是列表切片)
			if (lhs is ValTemp) {
				throw new CompilerException(errorContext, tokens.lineNum, "invalid assignment (not an lvalue)");
			}

			// 优化:在很多情况下,最后一条TAC指令是对RHS临时变量的赋值
			// 这种情况下,作为简单但非常有用的优化,我们可以直接修改那条指令
			// 让它赋值给LHS。但是,如果有任何跳转到下一行,我们不能这样做,
			// 因为短路求值可能会发生(问题#6)。
			if (rhs is ValTemp && output.code.Count > 0 && !output.IsJumpTarget(output.code.Count)) {
				TAC.Line line = output.code[output.code.Count - 1];
				if (line.lhs.Equals(rhs)) {
					// 是的,就是这种情况。修补它。
					line.lhs = lhs;
					return;
				}
			}

            // 如果最后一行是创建并赋值函数,那么我们不添加第二个赋值操作
            // 而是用正确的LHS更新那一行
            if (rhs is ValFunction && output.code.Count > 0) {
                TAC.Line line = output.code[output.code.Count - 1];
                if (line.op == TAC.Line.Op.BindAssignA) {
                    line.lhs = lhs;
                    return;
                }
            }

			// 其他情况下,对LHS执行赋值语句
			output.Add(new TAC.Line(lhs, TAC.Line.Op.AssignA, rhs));
		}

		/// <summary>
		/// 解析表达式的入口点
		///
		/// 参数说明:
		/// - asLval: 是否作为左值(可以被赋值)
		/// - statementStart: 是否在语句开始(影响某些解析决策)
		///
		/// 表达式解析使用递归下降,按优先级从低到高:
		/// Function -> Or -> And -> Not -> IsA -> Comparisons -> AddSub ->
		/// MultDiv -> UnaryMinus -> New -> Power -> AddressOf -> Call ->
		/// Map -> List -> Quantity -> Atom
		/// </summary>
		Value ParseExpr(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseFunction;
			return nextLevel(tokens, asLval, statementStart);
		}

		/// <summary>
		/// 解析函数定义
		///
		/// 函数定义语法:
		/// function(param1, param2=default, ...)
		///   statements
		/// end function
		///
		/// 特殊处理:
		/// 1. 创建新的ParseState(新的代码上下文)
		/// 2. 暂存到pendingState(当前语句结束后才压栈)
		/// 3. 生成BindAssignA指令(绑定函数到变量)
		/// </summary>
		Value ParseFunction(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseOr;
			Token tok = tokens.Peek();
			if (tok.type != Token.Type.Keyword || tok.text != "function") return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();

			Function func = new Function(null);
			tok = tokens.Peek();
			if (tok.type != Token.Type.EOL) {
				// 解析参数列表
				var paren = RequireToken(tokens, Token.Type.LParen);
				while (tokens.Peek().type != Token.Type.RParen) {
					// 解析参数:逗号分隔的列表
					// identifier 或 identifier = constant
					Token id = tokens.Dequeue();
					if (id.type != Token.Type.Identifier) throw new CompilerException(errorContext, tokens.lineNum,
						"got " + id + " where an identifier is required");
					Value defaultValue = null;
					if (tokens.Peek().type == Token.Type.OpAssign) {
						tokens.Dequeue();	// 跳过'='
						defaultValue = ParseExpr(tokens);
						// 确保默认值是常量,不是表达式
						if (defaultValue is ValTemp) {
							throw new CompilerException(errorContext, tokens.lineNum,
								"parameter default value must be a literal value");
						}
					}
					func.parameters.Add(new Function.Param(id.text, defaultValue));
					if (tokens.Peek().type == Token.Type.RParen) break;
					RequireToken(tokens, Token.Type.Comma);
				}

				RequireToken(tokens, Token.Type.RParen);
			}

			// 现在,需要将函数体解析到它自己的解析上下文中
			// 但不要立即压栈 -- 我们正在解析当前上下文中的某个表达式或语句,
			// 需要先完成那个
			if (pendingState != null) throw new CompilerException(errorContext, tokens.lineNum,
				"can't start two functions in one statement");
			pendingState = new ParseState();
			pendingState.nextTempNum = 1;	// (因为0用于保存返回值)

			// 创建附加到新解析状态代码的函数对象
			func.code = pendingState.code;
			var valFunc = new ValFunction(func);
			// BindAssignA: 绑定函数(设置外部上下文引用)
			output.Add(new TAC.Line(null, TAC.Line.Op.BindAssignA, valFunc));
			return valFunc;
		}

		/// <summary>
		/// 解析'or'运算符
		///
		/// 短路求值:
		/// - 如果左侧为真(>=1),不计算右侧,直接返回1
		/// - 否则计算右侧,返回 left or right 的结果
		///
		/// 实现:
		/// 1. 计算左侧
		/// 2. 如果左侧为真,跳转到结果赋值(result=1)
		/// 3. 否则计算右侧,计算or结果
		/// 4. 跳过结果赋值
		/// 5. (跳转目标)result=1
		/// </summary>
		Value ParseOr(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseAnd;
			Value val = nextLevel(tokens, asLval, statementStart);
			List<TAC.Line> jumpLines = null;
			Token tok = tokens.Peek();
			while (tok.type == Token.Type.Keyword && tok.text == "or") {
				tokens.Dequeue();		// 丢弃"or"
				val = FullyEvaluate(val);

				AllowLineBreak(tokens); // 二元运算符后允许换行

				// 基于当前值设置短路跳转
				// 稍后填充跳转目标
				// 注意通常的GotoAifB操作码在这里不起作用,
				// 会破坏我们对中间真值的计算
				// 我们需要仅当真值>=1(绝对为真)时才跳转
				TAC.Line jump = new TAC.Line(null, TAC.Line.Op.GotoAifTrulyB, null, val);
				output.Add(jump);
				if (jumpLines == null) jumpLines = new List<TAC.Line>();
				jumpLines.Add(jump);

				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.AOrB, val, opB));
				val = TAC.RTemp(tempNum);

				tok = tokens.Peek();
			}

			// 现在,如果有任何短路跳转,
			// 它们需要将短路结果(总是1)复制到输出临时变量
			// 其他所有情况需要跳过那个赋值。所以:
			if (jumpLines != null) {
				output.Add(new TAC.Line(null, TAC.Line.Op.GotoA, TAC.Num(output.code.Count+2)));	// 跳过下一行:
				output.Add(new TAC.Line(val, TAC.Line.Op.AssignA, ValNumber.one));	// result = 1
				foreach (TAC.Line jump in jumpLines) {
					jump.rhsA = TAC.Num(output.code.Count-1);	// 短路到上面的result=1行
				}
			}

			return val;
		}

		/// <summary>
		/// 解析'and'运算符
		///
		/// 短路求值:
		/// - 如果左侧为假,不计算右侧,直接返回0
		/// - 否则计算右侧,返回 left and right 的结果
		///
		/// 实现类似ParseOr,但短路条件和结果相反
		/// </summary>
		Value ParseAnd(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseNot;
			Value val = nextLevel(tokens, asLval, statementStart);
			List<TAC.Line> jumpLines = null;
			Token tok = tokens.Peek();
			while (tok.type == Token.Type.Keyword && tok.text == "and") {
				tokens.Dequeue();		// 丢弃"and"
				val = FullyEvaluate(val);

				AllowLineBreak(tokens); // 二元运算符后允许换行

				// 基于当前值设置短路跳转
				// 稍后填充跳转目标
				TAC.Line jump = new TAC.Line(null, TAC.Line.Op.GotoAifNotB, null, val);
				output.Add(jump);
				if (jumpLines == null) jumpLines = new List<TAC.Line>();
				jumpLines.Add(jump);

				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.AAndB, val, opB));
				val = TAC.RTemp(tempNum);

				tok = tokens.Peek();
			}

			// 如果有短路跳转,需要将短路结果(总是0)复制到输出临时变量
			if (jumpLines != null) {
				output.Add(new TAC.Line(null, TAC.Line.Op.GotoA, TAC.Num(output.code.Count+2)));	// 跳过下一行:
				output.Add(new TAC.Line(val, TAC.Line.Op.AssignA, ValNumber.zero));	// result = 0
				foreach (TAC.Line jump in jumpLines) {
					jump.rhsA = TAC.Num(output.code.Count-1);	// 短路到上面的result=0行
				}
			}

			return val;
		}

		/// <summary>
		/// 解析'not'运算符(一元运算符)
		///
		/// 语法: not expression
		/// 生成: temp = not expression
		/// </summary>
		Value ParseNot(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseIsA;
			Token tok = tokens.Peek();
			Value val;
			if (tok.type == Token.Type.Keyword && tok.text == "not") {
				tokens.Dequeue();		// 丢弃"not"

				AllowLineBreak(tokens); // 一元运算符后允许换行

				val = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.NotA, val));
				val = TAC.RTemp(tempNum);
			} else {
				val = nextLevel(tokens, asLval, statementStart);
			}
			return val;
		}

		/// <summary>
		/// 解析'isa'运算符
		///
		/// 用于类型检查: value isa type
		/// 例如: x isa number, obj isa MyClass
		/// </summary>
		Value ParseIsA(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseComparisons;
			Value val = nextLevel(tokens, asLval, statementStart);
			if (tokens.Peek().type == Token.Type.Keyword && tokens.Peek().text == "isa") {
				tokens.Dequeue();		// 丢弃isa运算符
				AllowLineBreak(tokens); // 二元运算符后允许换行
				val = FullyEvaluate(val);
				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.AisaB, val, opB));
				val = TAC.RTemp(tempNum);
			}
			return val;
		}

		/// <summary>
		/// 解析比较运算符
		///
		/// 支持的比较: ==, !=, <, <=, >, >=
		///
		/// 链式比较:
		/// a < b < c  等价于  (a < b) and (b < c)
		/// 实现:将所有比较结果相乘(所有为真时结果为真)
		/// </summary>
		Value ParseComparisons(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseAddSub;
			Value val = nextLevel(tokens, asLval, statementStart);
			Value opA = val;
			TAC.Line.Op opcode = ComparisonOp(tokens.Peek().type);
			// 解析一串比较,全部相乘
			// (所以每个比较都必须为真,整个表达式才为真)
			bool firstComparison = true;
			while (opcode != TAC.Line.Op.Noop) {
				tokens.Dequeue();	// 丢弃运算符(我们有操作码)
				opA = FullyEvaluate(opA);

				AllowLineBreak(tokens); // 二元运算符后允许换行

				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), opcode,	opA, opB));
				if (firstComparison) {
					firstComparison = false;
				} else {
					// 不是第一个比较,将结果与之前的结果相乘
					tempNum = output.nextTempNum++;
					output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.ATimesB, val, TAC.RTemp(tempNum - 1)));
				}
				val = TAC.RTemp(tempNum);
				opA = opB;  // 下一个比较的左操作数是当前的右操作数
				opcode = ComparisonOp(tokens.Peek().type);
			}
			return val;
		}

		/// <summary>
		/// 查找与给定token类型对应的TAC比较操作符
		/// 如果不是比较操作符,返回TAC.Line.Op.Noop
		/// </summary>
		static TAC.Line.Op ComparisonOp(Token.Type tokenType) {
			switch (tokenType) {
			case Token.Type.OpEqual:		return TAC.Line.Op.AEqualB;
			case Token.Type.OpNotEqual:		return TAC.Line.Op.ANotEqualB;
			case Token.Type.OpGreater:		return TAC.Line.Op.AGreaterThanB;
			case Token.Type.OpGreatEqual:	return TAC.Line.Op.AGreatOrEqualB;
			case Token.Type.OpLesser:		return TAC.Line.Op.ALessThanB;
			case Token.Type.OpLessEqual:	return TAC.Line.Op.ALessOrEqualB;
			default: return TAC.Line.Op.Noop;
			}
		}

		/// <summary>
		/// 解析加法和减法运算符
		///
		/// 特殊处理减号:
		/// - 在语句开始且后面有空格时,可能是一元减号
		/// - 检查是否在空白处,避免歧义
		///
		/// 例如:
		/// x = -5     <- 一元减号
		/// x = 3-5    <- 二元减号
		/// -5         <- 语句开始的一元减号
		/// </summary>
		Value ParseAddSub(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseMultDiv;
			Value val = nextLevel(tokens, asLval, statementStart);
			Token tok = tokens.Peek();
			while (tok.type == Token.Type.OpPlus ||
					(tok.type == Token.Type.OpMinus
					&& (!statementStart || !tok.afterSpace  || tokens.IsAtWhitespace()))) {
				tokens.Dequeue();

				AllowLineBreak(tokens); // 二元运算符后允许换行

				val = FullyEvaluate(val);
				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum),
					tok.type == Token.Type.OpPlus ? TAC.Line.Op.APlusB : TAC.Line.Op.AMinusB,
					val, opB));
				val = TAC.RTemp(tempNum);

				tok = tokens.Peek();
			}
			return val;
		}

		/// <summary>
		/// 解析乘法、除法和取模运算符
		/// 优先级高于加减法
		/// </summary>
		Value ParseMultDiv(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseUnaryMinus;
			Value val = nextLevel(tokens, asLval, statementStart);
			Token tok = tokens.Peek();
			while (tok.type == Token.Type.OpTimes || tok.type == Token.Type.OpDivide || tok.type == Token.Type.OpMod) {
				tokens.Dequeue();

				AllowLineBreak(tokens); // 二元运算符后允许换行

				val = FullyEvaluate(val);
				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				switch (tok.type) {
				case Token.Type.OpTimes:
					output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.ATimesB, val, opB));
					break;
				case Token.Type.OpDivide:
					output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.ADividedByB, val, opB));
					break;
				case Token.Type.OpMod:
					output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.AModB, val, opB));
					break;
				}
				val = TAC.RTemp(tempNum);

				tok = tokens.Peek();
			}
			return val;
		}

		/// <summary>
		/// 解析一元减号
		///
		/// 优化:
		/// - 如果后面是数字字面量,直接取反
		/// - 否则生成 0 - value 的指令
		/// </summary>
		Value ParseUnaryMinus(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseNew;
			if (tokens.Peek().type != Token.Type.OpMinus) return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();		// 跳过'-'

			AllowLineBreak(tokens); // 一元运算符后允许换行

			Value val = nextLevel(tokens);
			if (val is ValNumber) {
				// 如果后面是数字字面量,直接取反并完成!
				ValNumber valnum = (ValNumber)val;
				valnum.value = -valnum.value;
				return valnum;
			}
			// 否则,从0减去它并返回新的临时变量
			int tempNum = output.nextTempNum++;
			output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.AMinusB, TAC.Num(0), val));

			return TAC.RTemp(tempNum);
		}

		/// <summary>
		/// 解析'new'运算符
		///
		/// 用于创建对象实例: new ClassName
		/// 生成NewA指令,创建继承自指定类的新对象
		/// </summary>
		Value ParseNew(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParsePower;
			if (tokens.Peek().type != Token.Type.Keyword || tokens.Peek().text != "new") return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();		// 跳过'new'

			AllowLineBreak(tokens); // 一元运算符后允许换行

			Value isa = nextLevel(tokens);
			Value result = new ValTemp(output.nextTempNum++);
			output.Add(new TAC.Line(result, TAC.Line.Op.NewA, isa));
			return result;
		}

		/// <summary>
		/// 解析幂运算符(^)
		/// MiniScript中优先级最高的二元运算符
		///
		/// 例如: 2^3 = 8
		/// </summary>
		Value ParsePower(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseAddressOf;
			Value val = nextLevel(tokens, asLval, statementStart);
			Token tok = tokens.Peek();
			while (tok.type == Token.Type.OpPower) {
				tokens.Dequeue();

				AllowLineBreak(tokens); // 二元运算符后允许换行

				val = FullyEvaluate(val);
				Value opB = nextLevel(tokens);
				int tempNum = output.nextTempNum++;
				output.Add(new TAC.Line(TAC.LTemp(tempNum), TAC.Line.Op.APowB, val, opB));
				val = TAC.RTemp(tempNum);

				tok = tokens.Peek();
			}
			return val;
		}

		/// <summary>
		/// 解析地址运算符(@)
		///
		/// 用于获取函数引用而不调用它
		/// 例如: @print  (返回print函数的引用,不调用它)
		///
		/// 实现:设置noInvoke标志,防止自动调用
		/// </summary>
		Value ParseAddressOf(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseCallExpr;
			if (tokens.Peek().type != Token.Type.AddressOf) return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();
			AllowLineBreak(tokens); // 一元运算符后允许换行
			Value val = nextLevel(tokens, true, statementStart);
			// 设置noInvoke标志
			if (val is ValVar) {
				((ValVar)val).noInvoke = true;
			} else if (val is ValSeqElem) {
				((ValSeqElem)val).noInvoke = true;
			}
			return val;
		}

		/// <summary>
		/// 完全求值一个值
		///
		/// 如果值是变量或序列元素,可能需要调用它(如果是函数)
		///
		/// 特殊处理:
		/// 1. 被@保护的值:不调用
		/// 2. super和self:保持原样,运行时特殊处理
		/// 3. 其他变量/序列元素:生成CallFunctionA(可能调用函数,也可能只是获取值)
		///
		/// localOnlyMode: 控制变量查找范围
		/// - Off: 正常查找
		/// - Warn: 仅局部查找,但只警告
		/// - Strict: 仅局部查找,严格模式
		/// </summary>
		Value FullyEvaluate(Value val, ValVar.LocalOnlyMode localOnlyMode = ValVar.LocalOnlyMode.Off) {
			if (val is ValVar) {
				ValVar var = (ValVar)val;
				// 如果var被@保护,原样返回;不尝试调用它
				if (var.noInvoke) return val;
				if (var.identifier == output.localOnlyIdentifier) var.localOnly = localOnlyMode;
				// 不调用super;保持原样以便在运行时进行特殊处理
				// 同样,作为优化,对"self"也是如此
				if (var.identifier == "super" || var.identifier == "self") return val;
				// 求值变量(可能是我们需要调用的函数)
				ValTemp temp = new ValTemp(output.nextTempNum++);
				output.Add(new TAC.Line(temp, TAC.Line.Op.CallFunctionA, val, ValNumber.zero));
				return temp;
			} else if (val is ValSeqElem) {
				ValSeqElem elem = ((ValSeqElem)val);
				// 如果序列元素被@保护,原样返回;不尝试调用它
				if (elem.noInvoke) return val;
				// 求值序列查找(可能是我们需要调用的函数)
				ValTemp temp = new ValTemp(output.nextTempNum++);
				output.Add(new TAC.Line(temp, TAC.Line.Op.CallFunctionA, val, ValNumber.zero));
				return temp;
			}
			return val;
		}

		/// <summary>
		/// 解析调用表达式和成员访问
		///
		/// 处理:
		/// 1. 点运算符: obj.member
		/// 2. 索引/切片: list[i], list[1:3], list[:5]
		/// 3. 函数调用: func(args)
		///
		/// 可以链式调用: obj.method().property[0]
		///
		/// 切片语法:
		/// - list[start:end]: 从start到end(不包括end)
		/// - list[:end]: 从开始到end
		/// - list[start:]: 从start到结束
		/// </summary>
		Value ParseCallExpr(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseMap;
			Value val = nextLevel(tokens, asLval, statementStart);
			while (true) {
				if (tokens.Peek().type == Token.Type.Dot) {
					// 点运算符:成员访问
					tokens.Dequeue();	// 丢弃'.'
					AllowLineBreak(tokens); // 二元运算符后允许换行
					Token nextIdent = RequireToken(tokens, Token.Type.Identifier);
					// 我们在这里链式序列;通过调用查找序列的前一部分,
					// 以便我们可以在其基础上构建
					val = FullyEvaluate(val);
					// 现在构建查找
					val = new ValSeqElem(val, new ValString(nextIdent.text));
					if (tokens.Peek().type == Token.Type.LParen && !tokens.Peek().afterSpace) {
						// 如果这个新元素后面跟着括号,我们需要立即将其解析为调用
						val = ParseCallArgs(val, tokens);
					}
				} else if (tokens.Peek().type == Token.Type.LSquare && !tokens.Peek().afterSpace) {
					// 方括号:索引或切片
					tokens.Dequeue();	// 丢弃'['
					AllowLineBreak(tokens); // 开括号后允许换行
					val = FullyEvaluate(val);

					if (tokens.Peek().type == Token.Type.Colon) {	// 例如,foo[:4]
						tokens.Dequeue();	// 丢弃':'
						AllowLineBreak(tokens); // 冒号后允许换行
						Value index2 = null;
						if (tokens.Peek().type != Token.Type.RSquare) index2 = ParseExpr(tokens);
						ValTemp temp = new ValTemp(output.nextTempNum++);
						// CompileSlice: 内置函数,编译切片操作
						Intrinsics.CompileSlice(output.code, val, null, index2, temp.tempNum);
						val = temp;
					} else {
						Value index = ParseExpr(tokens);
						if (tokens.Peek().type == Token.Type.Colon) {	// 例如,foo[2:4]或foo[2:]
							tokens.Dequeue();	// 丢弃':'
							AllowLineBreak(tokens); // 冒号后允许换行
							Value index2 = null;
							if (tokens.Peek().type != Token.Type.RSquare) index2 = ParseExpr(tokens);
							ValTemp temp = new ValTemp(output.nextTempNum++);
							Intrinsics.CompileSlice(output.code, val, index, index2, temp.tempNum);
							val = temp;
						} else {			// 例如,foo[3] (根本不是切片)
							if (statementStart) {
								// 在语句开始,我们不想编译最后的序列查找,
								// 因为我们可能需要将其转换为赋值
								// 但我们想编译之前的任何一个
								if (val is ValSeqElem) {
									ValSeqElem vsVal = (ValSeqElem)val;
									ValTemp temp = new ValTemp(output.nextTempNum++);
									output.Add(new TAC.Line(temp, TAC.Line.Op.ElemBofA, vsVal.sequence, vsVal.index));
									val = temp;
								}
								val = new ValSeqElem(val, index);
							} else {
								// 在表达式的其他任何地方,我们可以立即编译查找
								ValTemp temp = new ValTemp(output.nextTempNum++);
								output.Add(new TAC.Line(temp, TAC.Line.Op.ElemBofA, val, index));
								val = temp;
							}
						}
					}

					RequireToken(tokens, Token.Type.RSquare);
				} else if ((val is ValVar && !((ValVar)val).noInvoke)
					    || (val is ValSeqElem && !((ValSeqElem)val).noInvoke)) {
					// 得到一个变量...它可能指向一个函数!
					if (!asLval || (tokens.Peek().type == Token.Type.LParen && !tokens.Peek().afterSpace)) {
						// 如果后面跟着括号,肯定是函数调用,可能带参数!
						// 如果不是,好吧,除非我们需要lvalue,否则还是调用它
						val = ParseCallArgs(val, tokens);
					} else break;
				} else break;
			}

			return val;
		}

		/// <summary>
		/// 解析映射(字典)字面量
		///
		/// 语法: {key1: value1, key2: value2, ...}
		///
		/// 重要:
		/// - 使用CopyA操作,确保每次执行都创建新实例
		/// - 映射是可变对象,必须在运行时创建,而非解析时
		///
		/// 允许尾随逗号和多行格式
		/// </summary>
		Value ParseMap(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseList;
			if (tokens.Peek().type != Token.Type.LCurly) return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();
			// 注意:必须确保这个映射在运行时创建,而不是在解析时
			// 因为它是可变对象,我们需要在每次执行此代码时返回不同的实例
			// (在循环、函数等中)。所以,我们下面使用Op.CopyA!
			ValMap map = new ValMap();
			if (tokens.Peek().type == Token.Type.RCurly) {
				tokens.Dequeue();
			} else while (true) {
				AllowLineBreak(tokens); // 逗号或开花括号后允许换行

				// 允许映射以自己行上的}关闭
				if (tokens.Peek().type == Token.Type.RCurly) {
					tokens.Dequeue();
					break;
				}

				Value key = ParseExpr(tokens);
				RequireToken(tokens, Token.Type.Colon);
				AllowLineBreak(tokens); // 冒号后允许换行
				Value value = ParseExpr(tokens);
				// 如果key为null,使用ValNull.instance
				map.map[key ?? ValNull.instance] = value;

				if (RequireEitherToken(tokens, Token.Type.Comma, Token.Type.RCurly).type == Token.Type.RCurly) break;
			}
			Value result = new ValTemp(output.nextTempNum++);
			output.Add(new TAC.Line(result, TAC.Line.Op.CopyA, map));
			return result;
		}

		/// <summary>
		/// 解析列表字面量
		///
		/// 语法: [elem1, elem2, ...]
		///
		/// 重要:
		/// - 使用CopyA操作,确保每次执行都创建新实例
		/// - 列表是可变对象,必须在运行时创建,而非解析时
		///
		/// 允许尾随逗号和多行格式
		/// </summary>
		Value ParseList(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseQuantity;
			if (tokens.Peek().type != Token.Type.LSquare) return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();
			// 注意:必须确保这个列表在运行时创建,而不是在解析时
			// 因为它是可变对象,我们需要在每次执行此代码时返回不同的实例
			// (在循环、函数等中)。所以,我们下面使用Op.CopyA!
			ValList list = new ValList();
			if (tokens.Peek().type == Token.Type.RSquare) {
				tokens.Dequeue();
			} else while (true) {
				AllowLineBreak(tokens); // 逗号或开方括号后允许换行

				// 允许列表以自己行上的]关闭
				if (tokens.Peek().type == Token.Type.RSquare) {
					tokens.Dequeue();
					break;
				}

				Value elem = ParseExpr(tokens);
				list.values.Add(elem);
				if (RequireEitherToken(tokens, Token.Type.Comma, Token.Type.RSquare).type == Token.Type.RSquare) break;
			}
			Value result = new ValTemp(output.nextTempNum++);
			output.Add(new TAC.Line(result, TAC.Line.Op.CopyA, list));	// 在这个可变列表上使用COPY!
			return result;
		}

		/// <summary>
		/// 解析括号表达式
		///
		/// 语法: (expression)
		///
		/// 括号用于:
		/// 1. 改变运算优先级
		/// 2. 提高可读性
		/// </summary>
		Value ParseQuantity(Lexer tokens, bool asLval=false, bool statementStart=false) {
			ExpressionParsingMethod nextLevel = ParseAtom;
			if (tokens.Peek().type != Token.Type.LParen) return nextLevel(tokens, asLval, statementStart);
			tokens.Dequeue();
			AllowLineBreak(tokens); // 开括号后允许换行
			Value val = ParseExpr(tokens);
			RequireToken(tokens, Token.Type.RParen);
			return val;
		}

		/// <summary>
		/// 辅助方法:收集参数,为每个参数生成PushParam指令,
		/// 然后生成对给定函数的实际调用
		///
		/// 支持两种形式:
		/// 1. 括号形式: func(arg1, arg2)
		/// 2. 无括号形式: func (无参数)
		///
		/// 工作流程:
		/// 1. 解析所有参数,生成PushParam指令
		/// 2. 生成CallFunctionA指令,参数数量作为第二个操作数
		/// </summary>
		Value ParseCallArgs(Value funcRef, Lexer tokens) {
			int argCount = 0;
			if (tokens.Peek().type == Token.Type.LParen) {
				tokens.Dequeue();		// 移除'('
				if (tokens.Peek().type == Token.Type.RParen) {
					tokens.Dequeue();
				} else while (true) {
					AllowLineBreak(tokens); // 逗号或开括号后允许换行
					Value arg = ParseExpr(tokens);
					// PushParam: 将参数压入参数栈
					output.Add(new TAC.Line(null, TAC.Line.Op.PushParam, arg));
					argCount++;
					if (RequireEitherToken(tokens, Token.Type.Comma, Token.Type.RParen).type == Token.Type.RParen) break;
				}
			}
			ValTemp result = new ValTemp(output.nextTempNum++);
			// CallFunctionA: 调用函数,第二个操作数是参数数量
			output.Add(new TAC.Line(result, TAC.Line.Op.CallFunctionA, funcRef, TAC.Num(argCount)));
			return result;
		}

		/// <summary>
		/// 解析原子(最基本的值)
		///
		/// 原子包括:
		/// 1. 数字: 42, 3.14, 1e10
		/// 2. 字符串: "hello", 'world'
		/// 3. 标识符: x, myVar, self
		/// 4. 关键字常量: null, true, false
		///
		/// 数字解析:
		/// - 使用double.TryParse
		/// - CultureInfo.InvariantCulture: 确保使用不变文化(点号作为小数点)
		/// - NumberStyles: 允许数字格式(科学记数法等)
		/// </summary>
		Value ParseAtom(Lexer tokens, bool asLval=false, bool statementStart=false) {
			Token tok = !tokens.AtEnd ? tokens.Dequeue() : Token.EOL;
			if (tok.type == Token.Type.Number) {
				double d;
				// TryParse: 尝试解析字符串为double
				// NumberStyles: 枚举,指定允许的数字格式
				// CultureInfo.InvariantCulture: 不变文化,确保一致的格式(如点号作为小数点)
				if (double.TryParse(tok.text, NumberStyles.Number | NumberStyles.AllowExponent,
					CultureInfo.InvariantCulture, out d)) return new ValNumber(d);
				throw new CompilerException("invalid numeric literal: " + tok.text);
			} else if (tok.type == Token.Type.String) {
				return new ValString(tok.text);
			} else if (tok.type == Token.Type.Identifier) {
				if (tok.text == "self") return ValVar.self;  // self是特殊的全局变量
				ValVar result = new ValVar(tok.text);
				if (result.identifier == output.localOnlyIdentifier) {
					result.localOnly = (output.localOnlyStrict ? ValVar.LocalOnlyMode.Strict : ValVar.LocalOnlyMode.Warn);
				}
				return result;
			} else if (tok.type == Token.Type.Keyword) {
				switch (tok.text) {
				case "null":	return null;  // null在MiniScript中表示为C# null
				case "true":	return ValNumber.one;   // true = 1
				case "false":	return ValNumber.zero;  // false = 0
				}
			}
			throw new CompilerException(string.Format("got {0} where number, string, or identifier is required", tok));
		}

		/// <summary>
		/// 要求特定类型和文本的token
		/// 如果不匹配,抛出错误
		///
		/// 特殊错误检测:
		/// - 检测if条件中使用=而非==的常见错误
		/// </summary>
		Token RequireToken(Lexer tokens, Token.Type type, string text=null) {
			Token got = (tokens.AtEnd ? Token.EOL : tokens.Dequeue());
			if (got.type != type || (text != null && got.text != text)) {
				Token expected = new Token(type, text);
				// 为if条件中使用`=`而非`==`的常见错误提供特殊错误消息
				if (got.type == Token.Type.OpAssign && text == "then") {
					throw new CompilerException(errorContext, tokens.lineNum,
						"found = instead of == in if condition");
				}
				throw new CompilerException(errorContext, tokens.lineNum,
					string.Format("got {0} where {1} is required", got, expected));
			}
			return got;
		}

		/// <summary>
		/// 要求两个可能的token之一
		/// 用于需要两种选择之一的情况(如逗号或右括号)
		/// </summary>
		Token RequireEitherToken(Lexer tokens, Token.Type type1, string text1, Token.Type type2, string text2=null) {
			Token got = (tokens.AtEnd ? Token.EOL : tokens.Dequeue());
			if ((got.type != type1 && got.type != type2)
				|| ((text1 != null && got.text != text1) && (text2 != null && got.text != text2))) {
				Token expected1 = new Token(type1, text1);
				Token expected2 = new Token(type2, text2);
				throw new CompilerException(errorContext, tokens.lineNum,
					string.Format("got {0} where {1} or {2} is required", got, expected1, expected2));
			}
			return got;
		}

		/// <summary>
		/// RequireEitherToken的重载版本
		/// 当不需要指定第一个token的文本时使用
		/// </summary>
		Token RequireEitherToken(Lexer tokens, Token.Type type1, Token.Type type2, string text2=null) {
			return RequireEitherToken(tokens, type1, null, type2, text2);
		}

		/// <summary>
		/// 测试有效解析
		/// 单元测试辅助方法
		///
		/// 参数:
		/// - src: 要解析的源代码
		/// - dumpTac: 是否转储生成的TAC代码(用于调试)
		/// </summary>
		static void TestValidParse(string src, bool dumpTac=false) {
			Parser parser = new Parser();
			try {
				parser.Parse(src);
			} catch (System.Exception e) {
				Console.WriteLine(e.ToString() + " while parsing:");
				Console.WriteLine(src);
			}
			if (dumpTac && parser.output != null) TAC.Dump(parser.output.code, -1);
		}

		/// <summary>
		/// 运行单元测试
		/// 测试各种MiniScript语法构造
		///
		/// 覆盖:
		/// - 表达式
		/// - 函数定义
		/// - 控制流
		/// - 集合
		/// - 短路求值
		/// - 行继续
		/// </summary>
		public static void RunUnitTests() {
			TestValidParse("pi < 4");
			TestValidParse("(pi < 4)");
			TestValidParse("if true then 20 else 30");
			TestValidParse("f = function(x)\nreturn x*3\nend function\nf(14)");
			TestValidParse("foo=\"bar\"\nindexes(foo*2)\nfoo.indexes");
			TestValidParse("x=[]\nx.push(42)");
			TestValidParse("list1=[10, 20, 30, 40, 50]; range(0, list1.len)");
			TestValidParse("f = function(x); print(\"foo\"); end function; print(false and f)");
			TestValidParse("print 42");
			TestValidParse("print true");
			TestValidParse("f = function(x)\nprint x\nend function\nf 42");
			TestValidParse("myList = [1, null, 3]");
			TestValidParse("while true; if true then; break; else; print 1; end if; end while");
			TestValidParse("x = 0 or\n1");
			TestValidParse("x = [1, 2, \n 3]");
			TestValidParse("range 1,\n10, 2");
		}
	}
}
