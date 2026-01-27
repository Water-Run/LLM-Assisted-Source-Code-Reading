/*	MiniscriptTAC.cs

This file defines the three-address code (TAC) which represents compiled
MiniScript code.  TAC is sort of a pseudo-assembly language, composed of
simple instructions containing an opcode and up to three variable/value 
references.

This is all internal MiniScript virtual machine code.  You don't need to
deal with it directly (see MiniscriptInterpreter instead).

这个文件定义了三地址码（TAC），它表示编译后的 MiniScript 代码。
TAC 类似于伪汇编语言，由简单的指令组成，每条指令包含一个操作码和最多三个变量/值引用。

这些都是 MiniScript 虚拟机的内部代码。你不需要直接处理它（请参见 MiniscriptInterpreter）。
*/

using System;
using System.Collections;  // 提供 IEnumerable 等基础集合接口
using System.Collections.Generic;  // 提供泛型集合类如 List<T>, Dictionary<TKey, TValue>

namespace Miniscript {

	// 静态类：TAC 是静态类，意味着它只包含静态成员，不能被实例化
	// 静态类通常用于组织相关的功能和类型定义
	public static class TAC {

		/// <summary>
		/// Line 类：表示一条 TAC 指令
		/// 这是虚拟机执行的最小单元
		/// 每条指令包含：操作码（op）、左操作数（lhs）、右操作数A（rhsA）、右操作数B（rhsB）
		/// </summary>
		public class Line {
			/// <summary>
			/// Op 枚举：定义所有可能的操作码
			/// 枚举类型是一组命名常量，用于提高代码可读性
			/// 默认情况下，第一个枚举值为 0，后续值递增
			/// </summary>
			public enum Op {
				Noop = 0,              // 空操作：什么都不做
				AssignA,               // 赋值：lhs = rhsA
				AssignImplicit,        // 隐式赋值：用于 REPL 模式，将表达式结果存储到 _ 变量
				APlusB,                // 加法：lhs = rhsA + rhsB
				AMinusB,               // 减法：lhs = rhsA - rhsB
				ATimesB,               // 乘法：lhs = rhsA * rhsB
				ADividedByB,           // 除法：lhs = rhsA / rhsB
				AModB,                 // 取模：lhs = rhsA % rhsB
				APowB,                 // 乘方：lhs = rhsA ^ rhsB
				AEqualB,               // 相等比较：lhs = (rhsA == rhsB)
				ANotEqualB,            // 不等比较：lhs = (rhsA != rhsB)
				AGreaterThanB,         // 大于比较：lhs = (rhsA > rhsB)
				AGreatOrEqualB,        // 大于等于比较：lhs = (rhsA >= rhsB)
				ALessThanB,            // 小于比较：lhs = (rhsA < rhsB)
				ALessOrEqualB,         // 小于等于比较：lhs = (rhsA <= rhsB)
				AisaB,                 // 类型检查：lhs = (rhsA isa rhsB)，检查 rhsA 是否是 rhsB 类型
				AAndB,                 // 逻辑与：lhs = rhsA and rhsB
				AOrB,                  // 逻辑或：lhs = rhsA or rhsB
				BindAssignA,           // 绑定赋值：用于闭包，将外部变量绑定到函数
				CopyA,                 // 复制：lhs = copy of rhsA，深拷贝用于字面量
				NewA,                  // 创建新对象：lhs = new rhsA，创建 rhsA 的实例
				NotA,                  // 逻辑非：lhs = not rhsA
				GotoA,                 // 无条件跳转：跳转到指令 rhsA
				GotoAifB,              // 条件跳转：如果 rhsB 为真，跳转到 rhsA
				GotoAifTrulyB,         // 严格条件跳转：只有当 rhsB 的整数值非零时才跳转（用于短路求值）
				GotoAifNotB,           // 条件跳转：如果 rhsB 为假，跳转到 rhsA
				PushParam,             // 压栈参数：将 rhsA 压入参数栈，准备函数调用
				CallFunctionA,         // 调用函数：调用 rhsA 函数，使用 rhsB 个参数
				CallIntrinsicA,        // 调用内置函数：调用 ID 为 rhsA 的内置函数
				ReturnA,               // 返回：lhs = rhsA; return
				ElemBofA,              // 索引访问：lhs = rhsA[rhsB]，获取序列元素
				ElemBofIterA,          // 迭代器索引：lhs = rhsA iter rhsB，用于 for 循环
				LengthOfA              // 长度：lhs = len(rhsA)
			}

			// 指令的组成部分
			public Value lhs;          // 左操作数（Left-Hand Side）：结果存储位置
			public Op op;              // 操作码：指定要执行的操作
			public Value rhsA;         // 右操作数A（Right-Hand Side A）：第一个操作数
			public Value rhsB;         // 右操作数B（Right-Hand Side B）：第二个操作数
//			public string comment;     // 注释（已注释掉）：用于调试
			public SourceLoc location; // 源码位置：记录此指令对应的源代码位置，用于错误报告

			/// <summary>
			/// 构造函数：创建一条新的 TAC 指令
			/// 使用默认参数简化指令创建
			/// </summary>
			/// <param name="lhs">结果存储位置</param>
			/// <param name="op">操作码</param>
			/// <param name="rhsA">第一个操作数（可选）</param>
			/// <param name="rhsB">第二个操作数（可选）</param>
			public Line(Value lhs, Op op, Value rhsA=null, Value rhsB=null) {
				this.lhs = lhs;
				this.op = op;
				this.rhsA = rhsA;
				this.rhsB = rhsB;
			}
			
			/// <summary>
			/// GetHashCode：计算对象的哈希码
			/// 重写此方法以支持将 Line 对象用作字典键或哈希集合成员
			/// 使用 XOR (^) 运算符组合各个字段的哈希码
			/// </summary>
			public override int GetHashCode() {
				return lhs.GetHashCode() ^ op.GetHashCode() ^ rhsA.GetHashCode() ^ rhsB.GetHashCode() ^ location.GetHashCode();
			}
			
			/// <summary>
			/// Equals：比较两个 Line 对象是否相等
			/// 重写此方法以正确比较对象内容而非引用
			/// 这是一个比较高级的 C# 概念：重写 Object 基类方法
			/// </summary>
			/// <param name="obj">要比较的对象</param>
			/// <returns>如果对象相等返回 true，否则返回 false</returns>
			public override bool Equals(object obj) {
				// 使用 is 运算符检查类型：这是 C# 的类型检查运算符
				if (!(obj is Line)) return false;
				// 类型转换：将 object 转换为 Line 类型
				Line b = (Line)obj;
				// 比较所有字段是否相等
				return op == b.op && lhs == b.lhs && rhsA == b.rhsA && rhsB == b.rhsB && location == b.location;
			}
			
			/// <summary>
			/// ToString：将指令转换为可读的字符串表示
			/// 重写此方法以便调试时能看到有意义的输出
			/// 使用 string.Format 进行字符串格式化
			/// </summary>
			public override string ToString() {
				string text;
				// switch 语句：根据操作码类型生成不同的字符串
				switch (op) {
				case Op.AssignA:
					// 格式化字符串：{0} 表示第一个参数，{1} 表示第二个参数
					text = string.Format("{0} := {1}", lhs, rhsA);
					break;
				case Op.AssignImplicit:
					text = string.Format("_ := {0}", rhsA);
					break;
				case Op.APlusB:
					text = string.Format("{0} := {1} + {2}", lhs, rhsA, rhsB);
					break;
				case Op.AMinusB:
					text = string.Format("{0} := {1} - {2}", lhs, rhsA, rhsB);
					break;
				case Op.ATimesB:
					text = string.Format("{0} := {1} * {2}", lhs, rhsA, rhsB);
					break;
				case Op.ADividedByB:
					text = string.Format("{0} := {1} / {2}", lhs, rhsA, rhsB);
					break;
				case Op.AModB:
					text = string.Format("{0} := {1} % {2}", lhs, rhsA, rhsB);
					break;
				case Op.APowB:
					text = string.Format("{0} := {1} ^ {2}", lhs, rhsA, rhsB);
					break;
				case Op.AEqualB:
					text = string.Format("{0} := {1} == {2}", lhs, rhsA, rhsB);
					break;
				case Op.ANotEqualB:
					text = string.Format("{0} := {1} != {2}", lhs, rhsA, rhsB);
					break;
				case Op.AGreaterThanB:
					text = string.Format("{0} := {1} > {2}", lhs, rhsA, rhsB);
					break;
				case Op.AGreatOrEqualB:
					text = string.Format("{0} := {1} >= {2}", lhs, rhsA, rhsB);
					break;
				case Op.ALessThanB:
					text = string.Format("{0} := {1} < {2}", lhs, rhsA, rhsB);
					break;
				case Op.ALessOrEqualB:
					text = string.Format("{0} := {1} <= {2}", lhs, rhsA, rhsB);
					break;
				case Op.AAndB:
					text = string.Format("{0} := {1} and {2}", lhs, rhsA, rhsB);
					break;
				case Op.AOrB:
					text = string.Format("{0} := {1} or {2}", lhs, rhsA, rhsB);
					break;
				case Op.AisaB:
					text = string.Format("{0} := {1} isa {2}", lhs, rhsA, rhsB);
					break;
				case Op.BindAssignA:
					text = string.Format("{0} := {1}; {0}.outerVars=", rhsA, rhsB);
					break;
				case Op.CopyA:
					text = string.Format("{0} := copy of {1}", lhs, rhsA);
					break;
				case Op.NewA:
					text = string.Format("{0} := new {1}", lhs, rhsA);
					break;
				case Op.NotA:
					text = string.Format("{0} := not {1}", lhs, rhsA);
					break;
				case Op.GotoA:
					text = string.Format("goto {0}", rhsA);
					break;
				case Op.GotoAifB:
					text = string.Format("goto {0} if {1}", rhsA, rhsB);
					break;
				case Op.GotoAifTrulyB:
					text = string.Format("goto {0} if truly {1}", rhsA, rhsB);
					break;
				case Op.GotoAifNotB:
					text = string.Format("goto {0} if not {1}", rhsA, rhsB);
					break;
				case Op.PushParam:
					text = string.Format("push param {0}", rhsA);
					break;
				case Op.CallFunctionA:
					text = string.Format("{0} := call {1} with {2} args", lhs, rhsA, rhsB);
					break;
				case Op.CallIntrinsicA:
					// 调用静态方法：Intrinsic.GetByID 根据 ID 获取内置函数
					text = string.Format("intrinsic {0}", Intrinsic.GetByID(rhsA.IntValue()));
					break;
				case Op.ReturnA:
					text = string.Format("{0} := {1}; return", lhs, rhsA);
					break;
				case Op.ElemBofA:
					text = string.Format("{0} = {1}[{2}]", lhs, rhsA, rhsB);
					break;
				case Op.ElemBofIterA:
					text = string.Format("{0} = {1} iter {2}", lhs, rhsA, rhsB);
					break;
				case Op.LengthOfA:
					text = string.Format("{0} = len({1})", lhs, rhsA);
					break;
				default:
					// 抛出异常：RuntimeException 是自定义异常类
					throw new RuntimeException("unknown opcode: " + op);
					
				}
				//if (comment != null) text = text + "\t// " + comment;
				// 附加源码位置信息用于调试
				if (location != null) text = text + "\t// " + location;
				return text;
			}

			/// <summary>
			/// Evaluate：执行这条指令并返回应该存储到 lhs 的值
			/// 这是虚拟机的核心执行方法
			/// </summary>
			/// <param name="context">当前执行上下文，包含变量、临时值等</param>
			/// <returns>指令执行的结果值</returns>
			public Value Evaluate(Context context) {
				// 处理赋值操作：这是最常见的操作，需要特别优化
				if (op == Op.AssignA || op == Op.ReturnA || op == Op.AssignImplicit) {
					// Assignment is a bit of a special case.  It's EXTREMELY common
					// in TAC, so needs to be efficient, but we have to watch out for
					// the case of a RHS that is a list or map.  This means it was a
					// literal in the source, and may contain references that need to
					// be evaluated now.
					// 赋值是一个特殊情况。它在 TAC 中极其常见，所以需要高效，
					// 但我们必须注意 RHS 是列表或映射的情况。
					// 这意味着它是源代码中的字面量，可能包含需要立即求值的引用。
					
					if (rhsA is ValList || rhsA is ValMap) {
						// FullEval：完全求值，递归求值所有嵌套引用
						return rhsA.FullEval(context);
					} else if (rhsA == null) {
						return null;
					} else {
						// Val：获取值，如果是变量则解析为实际值
						return rhsA.Val(context);
					}
				}
				
				// 处理复制操作：用于字面量赋值
				if (op == Op.CopyA) {
					// This opcode is used for assigning a literal.  We actually have
					// to copy the literal, in the case of a mutable object like a
					// list or map, to ensure that if the same code executes again,
					// we get a new, unique object.
					// 此操作码用于赋值字面量。对于可变对象如列表或映射，
					// 我们实际上必须复制字面量，以确保如果相同代码再次执行，
					// 我们得到一个新的、唯一的对象。
					
					if (rhsA is ValList) {
						// EvalCopy：求值并复制
						return ((ValList)rhsA).EvalCopy(context);
					} else if (rhsA is ValMap) {
						return ((ValMap)rhsA).EvalCopy(context);
					} else if (rhsA == null) {
						return null;
					} else {
						return rhsA.Val(context);
					}
				}

				// 求值操作数：将操作数转换为实际值
				// 三元运算符：condition ? trueValue : falseValue
				Value opA = rhsA!=null ? rhsA.Val(context) : null;
				Value opB = rhsB!=null ? rhsB.Val(context) : null;

				// 处理类型检查（isa）操作
				if (op == Op.AisaB) {
					if (opA == null) return ValNumber.Truth(opB == null);
					// IsA：检查 opA 是否是 opB 类型或其子类型
					return ValNumber.Truth(opA.IsA(opB, context.vm));
				}

				// 处理对象创建（new）操作
				if (op == Op.NewA) {
					// Create a new map, and set __isa on it to operand A (after 
					// verifying that this is a valid map to subclass).
					// 创建一个新映射，并在验证这是一个有效的可子类化映射后，
					// 将其 __isa 设置为操作数 A。
					
					if (!(opA is ValMap)) {
						throw new RuntimeException("argument to 'new' must be a map");
					} else if (opA == context.vm.stringType) {
						throw new RuntimeException("invalid use of 'new'; to create a string, use quotes, e.g. \"foo\"");
					} else if (opA == context.vm.listType) {
						throw new RuntimeException("invalid use of 'new'; to create a list, use square brackets, e.g. [1,2]");
					} else if (opA == context.vm.numberType) {
						throw new RuntimeException("invalid use of 'new'; to create a number, use a numeric literal, e.g. 42");
					} else if (opA == context.vm.functionType) {
						throw new RuntimeException("invalid use of 'new'; to create a function, use the 'function' keyword");
					}
					ValMap newMap = new ValMap();
					// SetElem：设置映射元素
					// magicIsA：特殊键名 "__isa"，用于原型继承
					newMap.SetElem(ValString.magicIsA, opA);
					return newMap;
				}

				// 处理索引访问操作（当索引是字符串时）
				if (op == Op.ElemBofA && opB is ValString) {
					// You can now look for a string in almost anything...
					// and we have a convenient (and relatively fast) method for it:
					// 你现在可以在几乎任何东西中查找字符串...
					// 我们有一个方便（且相对快速）的方法：
					
					ValMap ignored;
					// out 参数：允许方法返回多个值
					// Resolve：解析字符串索引，支持原型链查找
					return ValSeqElem.Resolve(opA, ((ValString)opB).value, context, out ignored);
				}

				// check for special cases of comparison to null (works with any type)
				// 检查与 null 比较的特殊情况（适用于任何类型）
				if (op == Op.AEqualB && (opA == null || opB == null)) {
					return ValNumber.Truth(opA == opB);
				}
				if (op == Op.ANotEqualB && (opA == null || opB == null)) {
					return ValNumber.Truth(opA != opB);
				}
				
				// check for implicit coersion of other types to string; this happens
				// when either side is a string and the operator is addition.
				// 检查其他类型到字符串的隐式强制转换；
				// 当任一侧是字符串且操作符是加法时会发生这种情况。
				if ((opA is ValString || opB is ValString) && op == Op.APlusB) {
					if (opA == null) return opB;
					if (opB == null) return opA;
					// ToString：将值转换为字符串
					string sA = opA.ToString(context.vm);
					string sB = opB.ToString(context.vm);
					// 检查字符串长度限制
					if (sA.Length + sB.Length > ValString.maxSize) throw new LimitExceededException("string too large");
					return new ValString(sA + sB);
				}

				// 处理数值操作
				if (opA is ValNumber) {
					double fA = ((ValNumber)opA).value;
					switch (op) {
					case Op.GotoA:
						// 直接修改指令指针
						context.lineNum = (int)fA;
						return null;
					case Op.GotoAifB:
						// BoolValue：将值转换为布尔值
						if (opB != null && opB.BoolValue()) context.lineNum = (int)fA;
						return null;
					case Op.GotoAifTrulyB:
						{
							// Unlike GotoAifB, which branches if B has any nonzero
							// value (including 0.5 or 0.001), this branches only if
							// B is TRULY true, i.e., its integer value is nonzero.
							// (Used for short-circuit evaluation of "or".)
							// 与 GotoAifB 不同（它在 B 有任何非零值时分支，包括 0.5 或 0.001），
							// 这只在 B 真正为真时分支，即其整数值非零。
							// （用于 "or" 的短路求值。）
							
							int i = 0;
							// IntValue：将值转换为整数
							if (opB != null) i = opB.IntValue();
							if (i != 0) context.lineNum = (int)fA;
							return null;
						}
					case Op.GotoAifNotB:
						if (opB == null || !opB.BoolValue()) context.lineNum = (int)fA;
						return null;
					case Op.CallIntrinsicA:
						// NOTE: intrinsics do not go through NextFunctionContext.  Instead
						// they execute directly in the current context.  (But usually, the
						// current context is a wrapper function that was invoked via
						// Op.CallFunction, so it got a parameter context at that time.)
						// 注意：内置函数不通过 NextFunctionContext。
						// 相反，它们直接在当前上下文中执行。
						// （但通常，当前上下文是通过 Op.CallFunction 调用的包装函数，
						// 所以它在那时获得了参数上下文。）
						
						// Execute：执行内置函数
						Intrinsic.Result result = Intrinsic.Execute((int)fA, context, context.partialResult);
						if (result.done) {
							// 内置函数执行完成
							context.partialResult = null;
							return result.result;
						}
						// OK, this intrinsic function is not yet done with its work.
						// We need to stay on this same line and call it again with 
						// the partial result, until it reports that its job is complete.
						// 好的，这个内置函数还没有完成工作。
						// 我们需要停留在同一行，并用部分结果再次调用它，
						// 直到它报告工作完成。
						context.partialResult = result;
						context.lineNum--;
						return null;
					case Op.NotA:
						// AbsClamp01：将值限制在 [0, 1] 范围内
						return new ValNumber(1.0 - AbsClamp01(fA));
					}
					
					// 处理双操作数的数值运算
					if (opB is ValNumber || opB == null) {
						double fB = opB != null ? ((ValNumber)opB).value : 0;
						switch (op) {
						case Op.APlusB:
							return new ValNumber(fA + fB);
						case Op.AMinusB:
							return new ValNumber(fA - fB);
						case Op.ATimesB:
							return new ValNumber(fA * fB);
						case Op.ADividedByB:
							return new ValNumber(fA / fB);
						case Op.AModB:
							return new ValNumber(fA % fB);
						case Op.APowB:
							// Math.Pow：.NET 库函数，计算幂
							return new ValNumber(Math.Pow(fA, fB));
						case Op.AEqualB:
							return ValNumber.Truth(fA == fB);
						case Op.ANotEqualB:
							return ValNumber.Truth(fA != fB);
						case Op.AGreaterThanB:
							return ValNumber.Truth(fA > fB);
						case Op.AGreatOrEqualB:
							return ValNumber.Truth(fA >= fB);
						case Op.ALessThanB:
							return ValNumber.Truth(fA < fB);
						case Op.ALessOrEqualB:
							return ValNumber.Truth(fA <= fB);
						case Op.AAndB:
							if (!(opB is ValNumber)) fB = opB != null && opB.BoolValue() ? 1 : 0;
							return new ValNumber(AbsClamp01(fA * fB));
						case Op.AOrB:
							if (!(opB is ValNumber)) fB = opB != null && opB.BoolValue() ? 1 : 0;
							return new ValNumber(AbsClamp01(fA + fB - fA * fB));
						default:
							break;
						}
					}
					// Handle equality testing between a number (opA) and a non-number (opB).
					// These are always considered unequal.
					// 处理数字（opA）和非数字（opB）之间的相等测试。
					// 这些总是被认为不相等。
					if (op == Op.AEqualB) return ValNumber.zero;
					if (op == Op.ANotEqualB) return ValNumber.one;

				} else if (opA is ValString) {
					// 处理字符串操作
					string sA = ((ValString)opA).value;
					
					// 字符串乘法和除法（字符串重复）
					if (op == Op.ATimesB || op == Op.ADividedByB) {
						double factor = 0;
						if (op == Op.ATimesB) {
							// Check.Type：类型检查辅助方法
							Check.Type(opB, typeof(ValNumber), "string replication");
							factor = ((ValNumber)opB).value;
						} else {
							Check.Type(opB, typeof(ValNumber), "string division");
							factor = 1.0 / ((ValNumber)opB).value;								
						}
						// double.IsNaN 和 double.IsInfinity：检查特殊浮点值
						if (double.IsNaN(factor) || double.IsInfinity(factor)) return null;
						if (factor <= 0) return ValString.empty;
						int repeats = (int)factor;
						if (repeats * sA.Length > ValString.maxSize) throw new LimitExceededException("string too large");
						// StringBuilder：高效的字符串构建类
						var result = new System.Text.StringBuilder();
						for (int i = 0; i < repeats; i++) result.Append(sA);
						int extraChars = (int)(sA.Length * (factor - repeats));
						// Substring：字符串截取
						if (extraChars > 0) result.Append(sA.Substring(0, extraChars));
						return new ValString(result.ToString());						
					}
					
					// 字符串索引访问
					if (op == Op.ElemBofA || op == Op.ElemBofIterA) {
						return ((ValString)opA).GetElem(opB);
					}
					
					// 字符串与字符串或 null 的操作
					if (opB == null || opB is ValString) {
						string sB = (opB == null ? null : opB.ToString(context.vm));
						switch (op) {
							case Op.AMinusB: {
									// 字符串减法：移除后缀
									if (opB == null) return opA;
									// EndsWith：检查字符串是否以指定字符串结尾
									if (sA.EndsWith(sB)) sA = sA.Substring(0, sA.Length - sB.Length);
									return new ValString(sA);
								}
							case Op.NotA:
								// string.IsNullOrEmpty：检查字符串是否为 null 或空
								return ValNumber.Truth(string.IsNullOrEmpty(sA));
							case Op.AEqualB:
								// string.Equals：字符串比较
								return ValNumber.Truth(string.Equals(sA, sB));
							case Op.ANotEqualB:
								return ValNumber.Truth(!string.Equals(sA, sB));
							case Op.AGreaterThanB:
								// string.Compare：字符串比较，返回 -1、0 或 1
								// StringComparison.Ordinal：按字符值比较
								return ValNumber.Truth(string.Compare(sA, sB, StringComparison.Ordinal) > 0);
							case Op.AGreatOrEqualB:
								return ValNumber.Truth(string.Compare(sA, sB, StringComparison.Ordinal) >= 0);
							case Op.ALessThanB:
								int foo = string.Compare(sA, sB, StringComparison.Ordinal);
								return ValNumber.Truth(foo < 0);
							case Op.ALessOrEqualB:
								return ValNumber.Truth(string.Compare(sA, sB, StringComparison.Ordinal) <= 0);
							case Op.LengthOfA:
								return new ValNumber(sA.Length);
							default:
								break;
						}
					} else {
						// RHS is neither null nor a string.
						// We no longer automatically coerce in all these cases; about
						// all we can do is equal or unequal testing.
						// (Note that addition was handled way above here.)
						// RHS 既不是 null 也不是字符串。
						// 我们不再在所有这些情况下自动强制转换；
						// 我们能做的只是相等或不相等测试。
						// （注意加法在上面已经处理过了。）
						if (op == Op.AEqualB) return ValNumber.zero;
						if (op == Op.ANotEqualB) return ValNumber.one;						
					}
				} else if (opA is ValList) {
					// 处理列表操作
					// List<Value>：泛型列表，存储 Value 类型的元素
					List<Value> list = ((ValList)opA).values;
					
					if (op == Op.ElemBofA || op == Op.ElemBofIterA) {
						// list indexing
						// 列表索引
						return ((ValList)opA).GetElem(opB);
					} else if (op == Op.LengthOfA) {
						return new ValNumber(list.Count);
					} else if (op == Op.AEqualB) {
						// Equality：自定义相等比较方法
						return ValNumber.Truth(((ValList)opA).Equality(opB));
					} else if (op == Op.ANotEqualB) {
						return ValNumber.Truth(1.0 - ((ValList)opA).Equality(opB));
					} else if (op == Op.APlusB) {
						// list concatenation
						// 列表连接
						Check.Type(opB, typeof(ValList), "list concatenation");
						List<Value> list2 = ((ValList)opB).values;
						if (list.Count + list2.Count > ValList.maxSize) throw new LimitExceededException("list too large");
						// 创建新列表，指定初始容量
						List<Value> result = new List<Value>(list.Count + list2.Count);
						// foreach：遍历集合
						foreach (Value v in list) result.Add(context.ValueInContext(v));
						foreach (Value v in list2) result.Add(context.ValueInContext(v));
						return new ValList(result);
					} else if (op == Op.ATimesB || op == Op.ADividedByB) {
						// list replication (or division)
						// 列表复制（或除法）
						double factor = 0;
						if (op == Op.ATimesB) {
							Check.Type(opB, typeof(ValNumber), "list replication");
							factor = ((ValNumber)opB).value;
						} else {
							Check.Type(opB, typeof(ValNumber), "list division");
							factor = 1.0 / ((ValNumber)opB).value;								
						}
						if (double.IsNaN(factor) || double.IsInfinity(factor)) return null;
						if (factor <= 0) return new ValList();
						int finalCount = (int)(list.Count * factor);
						if (finalCount > ValList.maxSize) throw new LimitExceededException("list too large");
						List<Value> result = new List<Value>(finalCount);
						for (int i = 0; i < finalCount; i++) {
							// % 运算符：取模，实现循环索引
							result.Add(context.ValueInContext(list[i % list.Count]));
						}
						return new ValList(result);
					} else if (op == Op.NotA) {
						return ValNumber.Truth(!opA.BoolValue());
					}
				} else if (opA is ValMap) {
					// 处理映射（字典）操作
					if (op == Op.ElemBofA) {
						// map lookup
						// 映射查找
						// (note, cases where opB is a string are handled above, along with
						// all the other types; so we'll only get here for non-string cases)
						// （注意，opB 是字符串的情况在上面已经处理过了，
						// 和所有其他类型一起；所以我们只会在非字符串情况下到达这里）
						ValSeqElem se = new ValSeqElem(opA, opB);
						return se.Val(context);
						// (This ensures we walk the "__isa" chain in the standard way.)
						// （这确保我们以标准方式遍历 "__isa" 链。）
					} else if (op == Op.ElemBofIterA) {
						// With a map, ElemBofIterA is different from ElemBofA.  This one
						// returns a mini-map containing a key/value pair.
						// 对于映射，ElemBofIterA 与 ElemBofA 不同。
						// 这个返回一个包含键/值对的小映射。
						return ((ValMap)opA).GetKeyValuePair(opB.IntValue());
					} else if (op == Op.LengthOfA) {
						// Count：字典的元素数量
						return new ValNumber(((ValMap)opA).Count);
					} else if (op == Op.AEqualB) {
						return ValNumber.Truth(((ValMap)opA).Equality(opB));
					} else if (op == Op.ANotEqualB) {
						return ValNumber.Truth(1.0 - ((ValMap)opA).Equality(opB));
					} else if (op == Op.APlusB) {
						// map combination
						// 映射组合
						// Dictionary<Value, Value>：泛型字典
						Dictionary<Value, Value> map = ((ValMap)opA).map;
						Check.Type(opB, typeof(ValMap), "map combination");
						Dictionary<Value, Value> map2 = ((ValMap)opB).map;
						ValMap result = new ValMap();
						// KeyValuePair：键值对结构
						foreach (KeyValuePair<Value, Value> kv in map) result.map[kv.Key] = context.ValueInContext(kv.Value);
						foreach (KeyValuePair<Value, Value> kv in map2) result.map[kv.Key] = context.ValueInContext(kv.Value);
						return result;
					} else if (op == Op.NotA) {
						return ValNumber.Truth(!opA.BoolValue());
					}
				} else if (opA is ValFunction && opB is ValFunction) {
					// 处理函数操作
					Function fA = ((ValFunction)opA).function;
					Function fB = ((ValFunction)opB).function;
					switch (op) {
					case Op.AEqualB:
						return ValNumber.Truth(fA == fB);
					case Op.ANotEqualB:
						return ValNumber.Truth(fA != fB);
					}
				} else {
					// opA is something else... perhaps null
					// opA 是其他东西...可能是 null
					switch (op) {
					case Op.BindAssignA:
						{
							// 闭包绑定：将外部变量绑定到函数
							if (context.variables == null) context.variables = new ValMap();
							ValFunction valFunc = (ValFunction)opA;
							// BindAndCopy：绑定外部变量并复制函数
                            return valFunc.BindAndCopy(context.variables);
						}
					case Op.NotA:
						return opA != null && opA.BoolValue() ? ValNumber.zero : ValNumber.one;
					case Op.ElemBofA:
						if (opA is null) {
							throw new TypeException("Null Reference Exception: can't index into null");
						} else {
							throw new TypeException("Type Exception: can't index into this type");
						}
					}
				}
				

				// 处理逻辑运算（适用于非数值类型）
				if (op == Op.AAndB || op == Op.AOrB) {
					// We already handled the case where opA was a number above;
					// this code handles the case where opA is something else.
					// 我们已经在上面处理了 opA 是数字的情况；
					// 这段代码处理 opA 是其他东西的情况。
					double fA = opA != null && opA.BoolValue() ? 1 : 0;
					double fB;
					if (opB is ValNumber) fB = ((ValNumber)opB).value;
					else fB = opB != null && opB.BoolValue() ? 1 : 0;
					double result;
					if (op == Op.AAndB) {
						result = AbsClamp01(fA * fB);
					} else {
						result = AbsClamp01(fA + fB - fA * fB);
					}
					return new ValNumber(result);
				}
				return null;
			}

			/// <summary>
			/// AbsClamp01：将值限制在 [0, 1] 范围内并取绝对值
			/// 用于布尔运算中的模糊逻辑
			/// static 方法：属于类而非实例，可以直接通过类名调用
			/// </summary>
			static double AbsClamp01(double d) {
				if (d < 0) d = -d;  // 取绝对值
				if (d > 1) return 1;  // 限制上界
				return d;
			}

		}
		
		/// <summary>
		/// TAC.Context：运行时上下文类
		/// 跟踪运行时环境，包括局部变量。
		/// Context 对象通过 "parent" 引用形成链表，
		/// 每次函数调用都会形成一个新的上下文（这被称为调用栈）。
		/// </summary>
		public class Context {
			public List<Line> code;			// TAC lines we're executing | 我们正在执行的 TAC 行
			public int lineNum;				// next line to be executed | 下一行要执行的代码
			public ValMap variables;		// local variables for this call frame | 此调用帧的局部变量
			public ValMap outerVars;        // variables of the context where this function was defined | 定义此函数的上下文的变量（模块变量）
			public Value self;				// value of self in this context | 此上下文中 self 的值
			public Stack<Value> args;		// pushed arguments for upcoming calls | 即将调用的已压栈参数
			public Context parent;			// parent (calling) context | 父（调用）上下文
			public Value resultStorage;		// where to store the return value (in the calling context) | 存储返回值的位置（在调用上下文中）
			public Machine vm;				// virtual machine | 虚拟机
			public Intrinsic.Result partialResult;	// work-in-progress of our current intrinsic | 当前内置函数的进行中工作
			public int implicitResultCounter;	// how many times we have stored an implicit result | 我们存储隐式结果的次数

			/// <summary>
			/// done 属性：检查是否执行完所有代码
			/// 属性（Property）是 C# 的特性，提供类似字段的访问方式但实际是方法
			/// get：只读属性，没有 set
			/// </summary>
			public bool done {
				get { return lineNum >= code.Count; }
			}

			/// <summary>
			/// root 属性：获取调用栈的根上下文（全局上下文）
			/// 通过遍历 parent 链找到最顶层
			/// </summary>
			public Context root {
				get {
					Context c = this;
					while (c.parent != null) c = c.parent;
					return c;
				}
			}

			/// <summary>
			/// interpreter 属性：获取托管此虚拟机的解释器
			/// WeakReference：弱引用，不阻止垃圾回收
			/// Target：获取弱引用的目标对象
			/// as：安全类型转换，失败返回 null
			/// </summary>
			public Interpreter interpreter {
				get {
					if (vm == null || vm.interpreter == null) return null;
					return vm.interpreter.Target as Interpreter;
				}
			}

			List<Value> temps;			// values of temporaries; temps[0] is always return value | 临时值；temps[0] 总是返回值

			/// <summary>
			/// 构造函数：创建新的上下文
			/// </summary>
			public Context(List<Line> code) {
				this.code = code;
			}

			/// <summary>
			/// ClearCodeAndTemps：清除代码和临时变量
			/// 用于重置上下文
			/// </summary>
			public void ClearCodeAndTemps() {
		 		code.Clear();  // Clear：清空列表
				lineNum = 0;
				if (temps != null) temps.Clear();
			}

			/// <summary>
			/// Reset：将此上下文重置到第一行代码，
			/// 清除任何临时变量，并可选地清除所有变量。
			/// </summary>
			/// <param name="clearVariables">如果为 true，清除我们的局部变量</param>
			public void Reset(bool clearVariables=true) {
				lineNum = 0;
				temps = null;
				if (clearVariables) variables = new ValMap();
			}

			/// <summary>
			/// JumpToEnd：跳转到代码末尾
			/// 用于提前终止执行
			/// </summary>
			public void JumpToEnd() {
				lineNum = code.Count;
			}

			/// <summary>
			/// SetTemp：设置临时变量的值
			/// 临时变量用于存储中间计算结果
			/// </summary>
			/// <param name="tempNum">临时变量编号</param>
			/// <param name="value">要设置的值</param>
			public void SetTemp(int tempNum, Value value) {
				// OFI: let each context record how many temps it will need, so we
				// can pre-allocate this list with that many and avoid having to
				// grow it later.  Also OFI: do lifetime analysis on these temps
				// and reuse ones we don't need anymore.
				// 优化机会：让每个上下文记录它需要多少临时变量，
				// 这样我们可以预分配这个列表，避免以后增长。
				// 另一个优化机会：对这些临时变量做生命周期分析，
				// 并重用我们不再需要的临时变量。
				
				if (temps == null) temps = new List<Value>();
				// 确保列表足够大
				while (temps.Count <= tempNum) temps.Add(null);
				temps[tempNum] = value;
			}

			/// <summary>
			/// GetTemp：获取临时变量的值
			/// </summary>
			public Value GetTemp(int tempNum) {
				return temps == null ? null : temps[tempNum];
			}

			/// <summary>
			/// GetTemp：获取临时变量的值，如果不存在则返回默认值
			/// 方法重载：同名方法，不同参数
			/// </summary>
			public Value GetTemp(int tempNum, Value defaultValue) {
				if (temps != null && tempNum < temps.Count) return temps[tempNum];
				return defaultValue;
			}

			/// <summary>
			/// SetVar：设置变量值
			/// 处理特殊变量如 "self"、"globals"、"locals"
			/// </summary>
			public void SetVar(string identifier, Value value) {
				// 禁止赋值给特殊变量
				if (identifier == "globals" || identifier == "locals") {
					throw new RuntimeException("can't assign to " + identifier);
				}
				// self 是特殊处理的
				if (identifier == "self") self = value;
				if (variables == null) variables = new ValMap();
				// assignOverride：委托（Delegate），允许自定义赋值行为
				// 这是 C# 的高级特性：委托是类型安全的函数指针
				// 如果 assignOverride 不为 null，调用它；如果它返回 false，执行默认赋值
				if (variables.assignOverride == null || !variables.assignOverride(new ValString(identifier), value)) {
					variables[identifier] = value;
				}
			}
			
			/// <summary>
			/// Get the value of a local variable ONLY -- does not check any other
			/// scopes, nor check for special built-in identifiers like "globals".
			/// Used mainly by host apps to easily look up an argument to an
			/// intrinsic function call by the parameter name.
			/// 仅获取局部变量的值 -- 不检查任何其他作用域，
			/// 也不检查像 "globals" 这样的特殊内置标识符。
			/// 主要由宿主应用使用，通过参数名轻松查找内置函数调用的参数。
			/// </summary>
			public Value GetLocal(string identifier, Value defaultValue=null) {
				Value result;
				// TryGetValue：安全的字典查找，不抛出异常
				// out 参数：用于返回查找到的值
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					return result;
				}
				return defaultValue;
			}
			
			/// <summary>
			/// GetLocalInt：获取局部整数变量
			/// 便捷方法，自动进行类型转换
			/// </summary>
			public int GetLocalInt(string identifier, int defaultValue = 0) {
				Value result;
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					if (result == null) return 0;	// variable found, but its value was null! | 找到变量，但其值为 null！
					return result.IntValue();
				}
				return defaultValue;
			}

			/// <summary>
			/// GetLocalBool：获取局部布尔变量
			/// </summary>
			public bool GetLocalBool(string identifier, bool defaultValue = false) {
				Value result;
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					if (result == null) return false;	// variable found, but its value was null! | 找到变量，但其值为 null！
					return result.BoolValue();
				}
				return defaultValue;
			}

			/// <summary>
			/// GetLocalFloat：获取局部浮点变量（单精度）
			/// </summary>
			public float GetLocalFloat(string identifier, float defaultValue = 0) {
				Value result;
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					if (result == null) return 0;	// variable found, but its value was null! | 找到变量，但其值为 null！
					return result.FloatValue();
				}
				return defaultValue;
			}

			/// <summary>
			/// GetLocalDouble：获取局部浮点变量（双精度）
			/// </summary>
			public double GetLocalDouble(string identifier, double defaultValue = 0) {
				Value result;
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					if (result == null) return 0;	// variable found, but its value was null! | 找到变量，但其值为 null！
					return result.DoubleValue();
				}
				return defaultValue;
			}

			/// <summary>
			/// GetLocalString：获取局部字符串变量
			/// </summary>
			public string GetLocalString(string identifier, string defaultValue = null) {
				Value result;
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					if (result == null) return null;	// variable found, but its value was null! | 找到变量，但其值为 null！
					return result.ToString();
				}
				return defaultValue;
			}

			/// <summary>
			/// GetSourceLoc：获取当前执行位置的源码位置
			/// 用于错误报告
			/// </summary>
			public SourceLoc GetSourceLoc() {
				if (lineNum < 0 || lineNum >= code.Count) return null;
				return code[lineNum].location;
			}
			
			/// <summary>
			/// Get the value of a variable available in this context (including
			/// locals, globals, and intrinsics).  Raise an exception if no such
			/// identifier can be found.
			/// 获取此上下文中可用的变量值（包括局部变量、全局变量和内置函数）。
			/// 如果找不到这样的标识符，则引发异常。
			/// </summary>
			/// <param name="identifier">要查找的标识符名称</param>
			/// <param name="localOnly">如果为 true，仅在局部作用域中查找</param>
			/// <returns>该标识符的值</returns>
			public Value GetVar(string identifier, ValVar.LocalOnlyMode localOnly=ValVar.LocalOnlyMode.Off) {
				// check for special built-in identifiers 'locals', 'globals', etc.
				// 检查特殊的内置标识符 'locals'、'globals' 等
				
				// 优化：使用 switch 按长度分组，减少字符串比较次数
				switch (identifier.Length)
				{
				case 4:
					if (identifier == "self") return self;
					break;
				case 5:
					if (identifier == "outer") {
						// return module variables, if we have them; else globals
						// 返回模块变量（如果有）；否则返回全局变量
						if (outerVars != null) return outerVars;
						if (root.variables == null) root.variables = new ValMap();
						return root.variables;
					}
					break;
				case 6:
					if (identifier == "locals") {
						if (variables == null) variables = new ValMap();
						return variables;
					}
					break;
				case 7:
					if (identifier == "globals") {
						if (root.variables == null) root.variables = new ValMap();
						return root.variables;
					}
					break;
				}
				
				// check for a local variable
				// 检查局部变量
				Value result;
				if (variables != null && variables.TryGetValue(identifier, out result)) {
					return result;
				}
				
				// 处理仅限局部模式
				if (localOnly != ValVar.LocalOnlyMode.Off) {
					if (localOnly == ValVar.LocalOnlyMode.Strict) throw new UndefinedLocalException(identifier);
					else vm.standardOutput("Warning: assignment of unqualified local '" + identifier 
					 + "' based on nonlocal is deprecated " + code[lineNum].location, true);
				}

				// check for a module variable
				// 检查模块变量
				if (outerVars != null && outerVars.TryGetValue(identifier, out result)) {
					return result;
				}

				// OK, we don't have a local or module variable with that name.
				// Check the global scope (if that's not us already).
				// 好的，我们没有该名称的局部或模块变量。
				// 检查全局作用域（如果还不是我们自己）。
				if (parent != null) {
					Context globals = root;
					if (globals.variables != null && globals.variables.TryGetValue(identifier, out result)) {
						return result;
					}
				}

				// Finally, check intrinsics.
				// 最后，检查内置函数。
				Intrinsic intrinsic = Intrinsic.GetByName(identifier);
				if (intrinsic != null) return intrinsic.GetFunc();

				// No luck there either?  Undefined identifier.
				// 还是没找到？未定义的标识符。
				throw new UndefinedIdentifierException(identifier);
			}

			/// <summary>
			/// StoreValue：将值存储到 lhs 指定的位置
			/// lhs 可以是临时变量、变量或序列元素
			/// </summary>
			public void StoreValue(Value lhs, Value value) {
				if (lhs is ValTemp) {
					SetTemp(((ValTemp)lhs).tempNum, value);
				} else if (lhs is ValVar) {
					SetVar(((ValVar)lhs).identifier, value);
				} else if (lhs is ValSeqElem) {
					// 序列元素赋值（如 list[0] = 42）
					ValSeqElem seqElem = (ValSeqElem)lhs;
					Value seq = seqElem.sequence.Val(this);
					if (seq == null) throw new RuntimeException("can't set indexed element of null");
					if (!seq.CanSetElem()) {
						throw new RuntimeException("can't set an indexed element in this type");
					}
					Value index = seqElem.index;
					// 解析索引值
					if (index is ValVar || index is ValSeqElem || 
						index is ValTemp) index = index.Val(this);
					seq.SetElem(index, value);
				} else {
					if (lhs != null) throw new RuntimeException("not an lvalue");
				}
			}
			
			/// <summary>
			/// ValueInContext：在当前上下文中求值
			/// 将可能的引用转换为实际值
			/// </summary>
			public Value ValueInContext(Value value) {
				if (value == null) return null;
				return value.Val(this);
			}

			/// <summary>
			/// Store a parameter argument in preparation for an upcoming call
			/// (which should be executed in the context returned by NextCallContext).
			/// 存储参数参数，为即将到来的调用做准备
			/// （应该在 NextCallContext 返回的上下文中执行）。
			/// </summary>
			/// <param name="arg">参数</param>
			public void PushParamArgument(Value arg) {
				if (args == null) args = new Stack<Value>();
				// Stack：后进先出（LIFO）数据结构
				if (args.Count > 255) throw new RuntimeException("Argument limit exceeded");
				args.Push(arg);  // Push：压栈				
			}

			/// <summary>
			/// Get a context for the next call, which includes any parameter arguments
			/// that have been set.
			/// 获取下一次调用的上下文，包括已设置的任何参数参数。
			/// </summary>
			/// <returns>调用上下文</returns>
			/// <param name="func">要调用的函数</param>
			/// <param name="argCount">要从栈中弹出多少参数</param>
			/// <param name="gotSelf">此方法是否使用点语法调用</param> 
			/// <param name="resultStorage">完成时存储结果的值</param>
			public Context NextCallContext(Function func, int argCount, bool gotSelf, Value resultStorage) {
				Context result = new Context(func.code);

				result.code = func.code;
				result.resultStorage = resultStorage;
				result.parent = this;
				result.vm = vm;

				// Stuff arguments, stored in our 'args' stack,
				// into local variables corrersponding to parameter names.
				// As a special case, skip over the first parameter if it is named 'self'
				// and we were invoked with dot syntax.
				// 将存储在我们的 'args' 栈中的参数，
				// 填充到对应参数名称的局部变量中。
				// 作为特殊情况，如果第一个参数名为 'self' 且我们使用点语法调用，
				// 则跳过第一个参数。
				
				int selfParam = (gotSelf && func.parameters.Count > 0 && func.parameters[0].name == "self" ? 1 : 0);
				for (int i = 0; i < argCount; i++) {
					// Careful -- when we pop them off, they're in reverse order.
					// 小心 -- 当我们弹出它们时，它们是反向顺序的。
					Value argument = args.Pop();  // Pop：出栈
					int paramNum = argCount - 1 - i + selfParam;
					if (paramNum >= func.parameters.Count) {
						throw new TooManyArgumentsException();
					}
					string param = func.parameters[paramNum].name;
					if (param == "self") result.self = argument;
					else result.SetVar(param, argument);
				}
				// And fill in the rest with default values
				// 用默认值填充其余参数
				for (int paramNum = argCount+selfParam; paramNum < func.parameters.Count; paramNum++) {
					result.SetVar(func.parameters[paramNum].name, func.parameters[paramNum].defaultValue);
				}

				return result;
			}

			/// <summary>
			/// This function prints the three-address code to the console, for debugging purposes.
			/// 此函数将三地址码打印到控制台，用于调试目的。
			/// </summary>
			public void Dump() {
				Console.WriteLine("CODE:");
				TAC.Dump(code, lineNum);

				Console.WriteLine("\nVARS:");
				if (variables == null) {
					Console.WriteLine(" NONE");
				} else {
					// foreach 遍历字典的键
					foreach (Value v in variables.Keys) {
						string id = v.ToString(vm);
						// string.Format：格式化字符串
						Console.WriteLine(string.Format("{0}: {1}", id, variables[id].ToString(vm)));
					}
				}

				Console.WriteLine("\nTEMPS:");
				if (temps == null) {
					Console.WriteLine(" NONE");
				} else {
					for (int i = 0; i < temps.Count; i++) {
						Console.WriteLine(string.Format("_{0}: {1}", i, temps[i]));
					}
				}
			}

			/// <summary>
			/// ToString：返回上下文的字符串表示
			/// </summary>
			public override string ToString() {
				return string.Format("Context[{0}/{1}]", lineNum, code.Count);
			}
		}
		
		/// <summary>
		/// TAC.Machine implements a complete MiniScript virtual machine.  It 
		/// keeps the context stack, keeps track of run time, and provides 
		/// methods to step, stop, or reset the program.
		/// TAC.Machine 实现了一个完整的 MiniScript 虚拟机。
		/// 它维护上下文栈，跟踪运行时间，并提供步进、停止或重置程序的方法。
		/// </summary>
		public class Machine {
			public WeakReference interpreter;		// interpreter hosting this machine | 托管此虚拟机的解释器（弱引用）
			public TextOutputMethod standardOutput;	// where print() results should go | print() 结果应该去哪里（委托）
			public bool storeImplicit = false;		// whether to store implicit values (e.g. for REPL) | 是否存储隐式值（例如用于 REPL）
			public bool yielding = false;			// set to true by yield intrinsic | 由 yield 内置函数设置为 true
			public ValMap functionType;				// 函数类型的原型映射
			public ValMap listType;					// 列表类型的原型映射
			public ValMap mapType;					// 映射类型的原型映射
			public ValMap numberType;				// 数字类型的原型映射
			public ValMap stringType;				// 字符串类型的原型映射
			public ValMap versionMap;				// 版本信息映射
			
			/// <summary>
			/// globalContext 属性：全局上下文
			/// 包含全局变量
			/// </summary>
			public Context globalContext {			// contains global variables | 包含全局变量
				get { return _globalContext; }
			}

			/// <summary>
			/// done 属性：检查程序是否执行完毕
			/// 当只剩全局上下文且已执行完所有代码时返回 true
			/// </summary>
			public bool done {
				get { return (stack.Count <= 1 && stack.Peek().done); }
			}

			/// <summary>
			/// runTime 属性：获取程序运行时间（秒）
			/// 使用 Stopwatch 类测量时间
			/// </summary>
			public double runTime {
				get { return stopwatch == null ? 0 : stopwatch.Elapsed.TotalSeconds; }
			}

			Context _globalContext;					// 私有字段，存储全局上下文
			Stack<Context> stack;					// 上下文栈，实现调用栈
			System.Diagnostics.Stopwatch stopwatch;	// 计时器，跟踪运行时间

			/// <summary>
			/// 构造函数：创建新的虚拟机
			/// </summary>
			/// <param name="globalContext">全局上下文</param>
			/// <param name="standardOutput">标准输出方法（委托）</param>
			public Machine(Context globalContext, TextOutputMethod standardOutput) {
				_globalContext = globalContext;
				_globalContext.vm = this;
				// 如果没有提供标准输出方法，使用默认的控制台输出
				if (standardOutput == null) {
					// Lambda 表达式：(参数) => 表达式
					// 这是 C# 的匿名函数语法
					this.standardOutput = (s,eol) => Console.WriteLine(s);
				} else {
					this.standardOutput = standardOutput;
				}
				stack = new Stack<Context>();
				stack.Push(_globalContext);
			}

			/// <summary>
			/// Stop：停止虚拟机执行
			/// 清空调用栈，只保留全局上下文
			/// </summary>
			public void Stop() {
				while (stack.Count > 1) stack.Pop();
				// Peek：查看栈顶元素但不移除
				stack.Peek().JumpToEnd();
			}
			
			/// <summary>
			/// Reset：重置虚拟机
			/// 清空调用栈，重置全局上下文
			/// </summary>
			public void Reset() {
				while (stack.Count > 1) stack.Pop();
				stack.Peek().Reset(false);
			}

			/// <summary>
			/// Step：执行一步（一条指令）
			/// 这是虚拟机的核心执行循环
			/// </summary>
			public void Step() {
				if (stack.Count == 0) return;		// not even a global context | 连全局上下文都没有
				// 启动计时器
				if (stopwatch == null) {
					// Stopwatch：.NET 提供的高精度计时器
					stopwatch = new System.Diagnostics.Stopwatch();
					stopwatch.Start();
				}
				Context context = stack.Peek();
				// 跳过已完成的上下文
				while (context.done) {
					if (stack.Count == 1) return;	// all done (can't pop the global context) | 全部完成（不能弹出全局上下文）
					PopContext();
					context = stack.Peek();
				}

				// 获取当前要执行的指令
				Line line = context.code[context.lineNum++];
				try {
					DoOneLine(line, context);
				} catch (MiniscriptException mse) {
					// 捕获 MiniScript 异常并添加位置信息
					// try-catch：异常处理
					if (mse.location == null) mse.location = line.location;
					if (mse.location == null) {
						// 如果还没有位置信息，尝试从调用栈中获取
						foreach (Context c in stack) {
							if (c.lineNum >= c.code.Count) continue;
							mse.location = c.code[c.lineNum].location;
							if (mse.location != null) break;
						}
					}
					throw;  // 重新抛出异常
				}
			}
			
			/// <summary>
			/// Directly invoke a ValFunction by manually pushing it onto the call stack.
			/// This might be useful, for example, in invoking handlers that have somehow
			/// been registered with the host app via intrinsics.
			/// 通过手动将 ValFunction 压入调用栈来直接调用它。
			/// 例如，在调用通过内置函数注册到宿主应用的处理程序时，这可能很有用。
			/// </summary>
			/// <param name="func">要调用的 Miniscript 函数</param>
			/// <param name="resultStorage">存储调用结果的位置（在调用上下文中）</param>
			/// <param name="arguments">要压入的参数列表（可选）</param>
			public void ManuallyPushCall(ValFunction func, Value resultStorage=null, List<Value> arguments=null) {
				var context = stack.Peek();
				int argCount = func.function.parameters.Count;
				for (int i=0; i<argCount; i++) {
					if (arguments != null && i < arguments.Count) {
						Value val = context.ValueInContext(arguments[i]);
						context.PushParamArgument(val);						
					} else {
						context.PushParamArgument(null);
					}
				}
				Value self = null;	// "self" is always null for a manually pushed call | 手动压入的调用中 "self" 总是 null
				
				Context nextContext = context.NextCallContext(func.function, argCount, self != null, null);
				if (self != null) nextContext.self = self;
				nextContext.resultStorage = resultStorage;
				stack.Push(nextContext);				
			}
			
			/// <summary>
			/// DoOneLine：执行一行代码
			/// 根据操作码执行相应的操作
			/// </summary>
			void DoOneLine(Line line, Context context) {
//				Console.WriteLine("EXECUTING line " + (context.lineNum-1) + ": " + line);
				
				// 处理参数压栈
				if (line.op == Line.Op.PushParam) {
					Value val = context.ValueInContext(line.rhsA);
					context.PushParamArgument(val);
				} else if (line.op == Line.Op.CallFunctionA) {
					// 函数调用是最复杂的操作之一
					// Resolve rhsA.  If it's a function, invoke it; otherwise,
					// just store it directly (but pop the call context).
					// 解析 rhsA。如果它是函数，调用它；否则，直接存储它（但弹出调用上下文）。
					
					ValMap valueFoundIn;
					// 解析整个点链（如果有）
					Value funcVal = line.rhsA.Val(context, out valueFoundIn);	// resolves the whole dot chain, if any | 解析整个点链（如果有）
					if (funcVal is ValFunction) {
						Value self = null;
						// bind "super" to the parent of the map the function was found in
						// 将 "super" 绑定到找到函数的映射的父映射
						Value super = valueFoundIn == null ? null : valueFoundIn.Lookup(ValString.magicIsA);
						if (line.rhsA is ValSeqElem) {
							// bind "self" to the object used to invoke the call, except
							// when invoking via "super"
							// 将 "self" 绑定到用于调用的对象，除非通过 "super" 调用
							Value seq = ((ValSeqElem)(line.rhsA)).sequence;
							if (seq is ValVar && ((ValVar)seq).identifier == "super") self = context.self;
							else self = context.ValueInContext(seq);
						}
						ValFunction func = (ValFunction)funcVal;
						int argCount = line.rhsB.IntValue();
						Context nextContext = context.NextCallContext(func.function, argCount, self != null, line.lhs);
						nextContext.outerVars = func.outerVars;
						if (valueFoundIn != null) nextContext.SetVar("super", super);
						if (self != null) nextContext.self = self;	// (set only if bound above) | （仅在上面绑定时设置）
						stack.Push(nextContext);
					} else {
						// The user is attempting to call something that's not a function.
						// We'll allow that, but any number of parameters is too many.  [#35]
						// (No need to pop them, as the exception will pop the whole call stack anyway.)
						// 用户试图调用非函数的东西。
						// 我们允许这样做，但任何数量的参数都太多了。[#35]
						// （不需要弹出它们，因为异常无论如何都会弹出整个调用栈。）
						int argCount = line.rhsB.IntValue();
						if (argCount > 0) throw new TooManyArgumentsException();
						context.StoreValue(line.lhs, funcVal);
					}
				} else if (line.op == Line.Op.ReturnA) {
					// 处理返回
					Value val = line.Evaluate(context);
					context.StoreValue(line.lhs, val);
					PopContext();
				} else if (line.op == Line.Op.AssignImplicit) {
					// 隐式赋值（用于 REPL）
					Value val = line.Evaluate(context);
					if (storeImplicit) {
						context.StoreValue(ValVar.implicitResult, val);
						context.implicitResultCounter++;
					}
				} else {
					// 其他所有操作
					Value val = line.Evaluate(context);
					context.StoreValue(line.lhs, val);
				}
			}

			/// <summary>
			/// PopContext：弹出上下文
			/// 将返回值从弹出的上下文复制到调用上下文
			/// </summary>
			void PopContext() {
				// Our top context is done; pop it off, and copy the return value in temp 0.
				// 我们的顶部上下文完成了；弹出它，并复制临时变量 0 中的返回值。
				if (stack.Count == 1) return;	// down to just the global stack (which we keep) | 只剩下全局栈（我们保留它）
				Context context = stack.Pop();
				Value result = context.GetTemp(0, null);
				Value storage = context.resultStorage;
				context = stack.Peek();
				context.StoreValue(storage, result);
			}

			/// <summary>
			/// GetTopContext：获取栈顶上下文
			/// </summary>
			public Context GetTopContext() {
				return stack.Peek();
			}

			/// <summary>
			/// DumpTopContext：打印栈顶上下文（用于调试）
			/// </summary>
			public void DumpTopContext() {
				stack.Peek().Dump();
			}
			
			/// <summary>
			/// FindShortName：查找值的短名称
			/// 用于调试输出，查找全局变量或内置函数的名称
			/// </summary>
			public string FindShortName(Value val) {
				if (globalContext == null || globalContext.variables == null) return null;
				// 在全局变量中查找
				foreach (var kv in globalContext.variables.map) {
					if (kv.Value == val && kv.Key != val) return kv.Key.ToString(this);
				}
				string result = null;
				// 在内置函数中查找
				Intrinsic.shortNames.TryGetValue(val, out result);
				return result;
			}
			
			/// <summary>
			/// GetStack：获取调用栈
			/// 返回源码位置列表
			/// </summary>
			public List<SourceLoc> GetStack() {
				var result = new List<SourceLoc>();
				// NOTE: C# iteration over a Stack goes in reverse order.
				// This will return the newest call context first, and the
				// oldest (global) call context last.
				// 注意：C# 对 Stack 的迭代是反向的。
				// 这将首先返回最新的调用上下文，最后返回最旧的（全局）调用上下文。
				foreach (var context in stack) {
					result.Add(context.GetSourceLoc());
				}
				return result;
			}
		}

		/// <summary>
		/// Dump：打印 TAC 代码（用于调试）
		/// </summary>
		/// <param name="lines">要打印的代码行</param>
		/// <param name="lineNumToHighlight">要高亮的行号</param>
		/// <param name="indent">缩进级别</param>
		public static void Dump(List<Line> lines, int lineNumToHighlight, int indent=0) {
			int lineNum = 0;
			foreach (Line line in lines) {
				// 高亮当前执行的行
				string s = (lineNum == lineNumToHighlight ? "> " : "  ") + (lineNum++) + ". ";
				Console.WriteLine(s + line);
				// 递归打印函数内的代码
				if (line.op == Line.Op.BindAssignA) {
					ValFunction func = (ValFunction)line.rhsA;
					Dump(func.function.code, -1, indent+1);
				}
			}
		}

		/// <summary>
		/// 以下是一些便捷的静态方法，用于创建 TAC 值
		/// 这些方法简化了 TAC 代码的生成
		/// </summary>
		
		/// <summary>
		/// LTemp：创建左值临时变量
		/// </summary>
		public static ValTemp LTemp(int tempNum) {
			return new ValTemp(tempNum);
		}
		
		/// <summary>
		/// LVar：创建左值变量
		/// </summary>
		public static ValVar LVar(string identifier) {
			// 优化：缓存常用的 "self" 变量
			if (identifier == "self") return ValVar.self;
			return new ValVar(identifier);
		}
		
		/// <summary>
		/// RTemp：创建右值临时变量
		/// </summary>
		public static ValTemp RTemp(int tempNum) {
			return new ValTemp(tempNum);
		}
		
		/// <summary>
		/// Num：创建数值
		/// </summary>
		public static ValNumber Num(double value) {
			return new ValNumber(value);
		}
		
		/// <summary>
		/// Str：创建字符串值
		/// </summary>
		public static ValString Str(string value) {
			return new ValString(value);
		}
		
		/// <summary>
		/// IntrinsicByName：通过名称获取内置函数
		/// </summary>
		public static ValNumber IntrinsicByName(string name) {
			return new ValNumber(Intrinsic.GetByName(name).id);
		}
		
	}
}