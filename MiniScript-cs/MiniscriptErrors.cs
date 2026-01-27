/*	MiniScriptErrors.cs

This file defines the exception hierarchy used by Miniscript.
The core of the tree is this:

	MiniscriptException
		LexerException -- any error while finding tokens from raw source
		CompilerException -- any error while compiling tokens into bytecode
		RuntimeException -- any error while actually executing code.

We have a number of fine-grained exception types within these,
but they will always derive from one of those three (and ultimately
from MiniscriptException).
*/
/* 本文件定义了Miniscript使用的异常层次结构。
   核心的异常树是这样的:

	MiniscriptException
		LexerException -- 从原始源代码查找标记(token)时的任何错误
		CompilerException -- 将标记编译为字节码时的任何错误
		RuntimeException -- 实际执行代码时的任何错误。

   我们在这些基础上有许多细粒度的异常类型,
   但它们总是派生自这三者之一(并最终派生自MiniscriptException)。
*/

using System;
// using System: 引入C#的System命名空间
// 说明: System命名空间包含基本类和基类,如Exception类
//      这是.NET框架的核心命名空间,提供了基础的类型系统

namespace Miniscript {
	// namespace: 命名空间,用于组织代码,避免类名冲突
	// 说明: Miniscript是本项目的根命名空间
	//      所有Miniscript相关的类都应该放在这个命名空间下
	
	/// <summary>
	/// 源代码位置类
	/// 功能: 记录代码在源文件中的位置信息,用于错误报告
	/// 用途: 当发生错误时,可以准确指出错误发生在哪个文件的哪一行
	/// </summary>
	public class SourceLoc {
		// public: 访问修饰符,表示这个类可以被任何其他代码访问
		// class: 定义一个类(引用类型)
		
		public string context;	// file name, etc. (optional)
		// 上下文信息,通常是文件名(可选)
		// string: C#的字符串类型,引用类型,可以为null
		// 说明: 存储错误发生的文件名或其他上下文信息
		//      如果为null或空字符串,则表示没有上下文信息
		
		public int lineNum;
		// 行号
		// int: C#的32位整数类型,值类型
		// 说明: 存储错误发生的行号,从1开始计数
		
		/// <summary>
		/// 构造函数: 创建一个新的源代码位置对象
		/// </summary>
		/// <param name="context">上下文信息(如文件名)</param>
		/// <param name="lineNum">行号</param>
		/// 说明: 构造函数用于初始化对象
		///      构造函数名必须与类名相同
		///      this关键字指代当前对象实例
		public SourceLoc(string context, int lineNum) {
			this.context = context;
			// this.context: 表示当前对象的context字段
			// context: 表示构造函数的参数
			// 当字段名与参数名相同时,需要用this来区分
			
			this.lineNum = lineNum;
			// 将参数lineNum的值赋给当前对象的lineNum字段
		}

		/// <summary>
		/// 重写ToString方法,生成位置信息的字符串表示
		/// </summary>
		/// <returns>格式化的位置字符串,如 "[文件名 line 10]"</returns>
		/// 说明: override关键字表示重写基类(Object)的ToString方法
		///      ToString是一个虚方法,所有类都从Object继承
		///      重写后可以自定义对象转换为字符串时的行为
		public override string ToString() {
			// string.Format: 字符串格式化方法,类似于C语言的printf
			// 第一个参数是格式字符串,{0}和{1}是占位符
			// 后续参数会按顺序替换占位符
			return string.Format("[{0}line {1}]",
				// 三元条件运算符: condition ? trueValue : falseValue
				// string.IsNullOrEmpty: 静态方法,检查字符串是否为null或空
				// 如果context为空,则不显示;否则显示"context "
				string.IsNullOrEmpty(context) ? "" : context + " ",
				lineNum);
			// 示例输出: "[myfile.ms line 42]" 或 "[line 42]"
		}
	}

	/// <summary>
	/// Miniscript基础异常类
	/// 功能: 作为所有Miniscript异常的基类
	/// 继承: System.Exception (C#的基础异常类)
	/// 说明: 这是异常层次结构的根,所有Miniscript的异常都继承自这个类
	/// </summary>
	public class MiniscriptException: Exception {
		// : Exception 表示继承自System.Exception类
		// 继承说明: 子类会继承父类的所有公有和受保护的成员
		//          可以添加新成员或重写虚成员
		
		public SourceLoc location;
		// 源代码位置信息
		// 说明: 存储异常发生的具体位置
		//      可以为null,表示位置未知或不适用

		/// <summary>
		/// 构造函数: 创建一个只包含错误消息的异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// 说明: : base(message) 调用父类Exception的构造函数
		///      base关键字用于访问基类的成员
		public MiniscriptException(string message) : base(message) {
			// 空函数体,因为所有初始化工作都由base(message)完成
			// location字段会被自动初始化为null(引用类型的默认值)
		}

		/// <summary>
		/// 构造函数: 创建一个包含错误消息和位置信息的异常
		/// </summary>
		/// <param name="context">上下文(如文件名)</param>
		/// <param name="lineNum">行号</param>
		/// <param name="message">错误消息</param>
		/// 说明: 这是一个重载的构造函数
		///      C#允许同一个类有多个构造函数,只要参数列表不同
		public MiniscriptException(string context, int lineNum, string message) : base(message) {
			// 创建一个新的SourceLoc对象并赋值给location字段
			location = new SourceLoc(context, lineNum);
			// new: 关键字,用于创建对象实例,会调用构造函数
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的异常(用于异常链)
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常(导致当前异常的原始异常)</param>
		/// 说明: 异常链允许保留原始异常信息
		///      当捕获一个异常后抛出新异常时,可以保留原始异常的堆栈跟踪
		public MiniscriptException(string message, Exception inner) : base(message, inner) {
			// inner参数会被传递给Exception基类,存储在InnerException属性中
		}

		/// <summary>
		/// Get a standard description of this error, including type and location.
		/// 获取此错误的标准描述,包括类型和位置。
		/// </summary>
		/// <returns>格式化的错误描述字符串</returns>
		/// 说明: 这个方法用于生成用户友好的错误消息
		///      会根据异常的具体类型添加相应的前缀
		public string Description() {
			string desc = "Error: ";
			// 初始化错误描述,默认前缀为 "Error: "
			
			// is关键字: 类型检查运算符,检查对象是否是指定类型或其派生类型
			// 说明: 这是运行时类型检查(RTTI - Runtime Type Information)
			//      返回bool值,true表示对象是该类型的实例
			if (this is LexerException) desc = "Lexer Error: ";
			// 如果当前对象是LexerException类型,修改前缀
			
			else if (this is CompilerException) desc = "Compiler Error: ";
			// else if: 否则如果,用于多条件判断
			
			else if (this is RuntimeException) desc = "Runtime Error: ";
			// 注意: 由于IndexException等都继承自RuntimeException
			//      如果是IndexException,这里也会返回true
			//      所以这种检查只能识别大类,不能区分细分类型
			
			desc += Message;
			// += : 字符串连接运算符,等同于 desc = desc + Message
			// Message: 继承自Exception基类的属性,存储错误消息
			
			if (location != null) desc += " " + location;
			// 如果有位置信息,追加到描述中
			// location是SourceLoc对象,会自动调用其ToString方法
			
			return desc;		
			// 返回完整的错误描述
			// 示例: "Compiler Error: Unexpected token [myfile.ms line 10]"
		}

	}

	/// <summary>
	/// 词法分析器异常
	/// 功能: 表示在词法分析阶段发生的错误
	/// 继承: MiniscriptException
	/// 说明: 词法分析是编译的第一阶段,将源代码文本分解为标记(token)
	///      如果源代码包含无法识别的字符或格式错误,会抛出此异常
	/// </summary>
	public class LexerException: MiniscriptException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的词法分析器错误
		/// </summary>
		public LexerException() : base("Lexer Error") {
			// 调用父类构造函数,传入默认错误消息
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的词法分析器异常
		/// </summary>
		/// <param name="message">自定义错误消息</param>
		public LexerException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的词法分析器异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public LexerException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 编译器异常
	/// 功能: 表示在编译阶段发生的错误
	/// 继承: MiniscriptException
	/// 说明: 编译是将标记(token)转换为字节码的过程
	///      如果代码有语法错误、语义错误等,会抛出此异常
	/// </summary>
	public class CompilerException: MiniscriptException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的语法错误
		/// </summary>
		public CompilerException() : base("Syntax Error") {
			// 默认消息为"Syntax Error"(语法错误)
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的编译器异常
		/// </summary>
		/// <param name="message">自定义错误消息</param>
		public CompilerException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含位置信息的编译器异常
		/// </summary>
		/// <param name="context">上下文(如文件名)</param>
		/// <param name="lineNum">行号</param>
		/// <param name="message">错误消息</param>
		/// 说明: 这个构造函数特别有用,因为编译错误通常需要指出具体位置
		public CompilerException(string context, int lineNum, string message) : base(context, lineNum, message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的编译器异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public CompilerException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 运行时异常
	/// 功能: 表示在代码执行阶段发生的错误
	/// 继承: MiniscriptException
	/// 说明: 这是一个基类,大多数具体的运行时错误都继承自这个类
	///      运行时错误是在程序实际执行时才会发生的错误
	/// </summary>
	public class RuntimeException: MiniscriptException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的运行时错误
		/// </summary>
		public RuntimeException() : base("Runtime Error") {
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的运行时异常
		/// </summary>
		/// <param name="message">自定义错误消息</param>
		public RuntimeException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的运行时异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public RuntimeException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 索引异常
	/// 功能: 表示数组、列表等访问越界的错误
	/// 继承: RuntimeException
	/// 说明: 当使用超出有效范围的索引访问集合时抛出
	///      例如: 访问list[10]但列表只有5个元素
	/// </summary>
	public class IndexException: RuntimeException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的索引错误
		/// </summary>
		public IndexException() : base("Index Error (index out of range)") {
			// "Index Error (index out of range)" - 索引错误(索引超出范围)
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的索引异常
		/// </summary>
		/// <param name="message">自定义错误消息</param>
		public IndexException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的索引异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public IndexException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 键异常
	/// 功能: 表示在映射(字典)中查找不存在的键时的错误
	/// 继承: RuntimeException
	/// 说明: 当尝试访问映射中不存在的键时抛出
	///      例如: myMap["nonexistent"]
	/// </summary>
	public class KeyException : RuntimeException {
		/// <summary>
		/// 构造函数: 创建一个键未找到的异常
		/// </summary>
		/// <param name="key">未找到的键</param>
		/// 说明: 这个构造函数会自动生成包含键名的错误消息
		///      字符串连接使用+运算符
		public KeyException(string key) : base("Key Not Found: '" + key + "' not found in map") {
			// 生成的消息格式: "Key Not Found: 'someKey' not found in map"
			// 说明: 明确指出哪个键没有找到
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的键异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public KeyException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 类型异常
	/// 功能: 表示类型不匹配或类型错误
	/// 继承: RuntimeException
	/// 说明: 当操作的数据类型与期望的类型不符时抛出
	///      例如: 尝试将字符串与数字相加,或调用非函数对象
	/// </summary>
	public class TypeException : RuntimeException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的类型错误
		/// </summary>
		public TypeException() : base("Type Error (wrong type for whatever you're doing)") {
			// "Type Error (wrong type for whatever you're doing)" 
			// - 类型错误(你正在做的事情使用了错误的类型)
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的类型异常
		/// </summary>
		/// <param name="message">自定义错误消息</param>
		public TypeException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的类型异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public TypeException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 参数过多异常
	/// 功能: 表示函数调用时提供的参数数量超过允许的最大值
	/// 继承: RuntimeException
	/// 说明: 当调用函数时传入的参数个数超过函数定义时抛出
	///      例如: 函数定义需要2个参数,但调用时传入了3个
	/// </summary>
	public class TooManyArgumentsException : RuntimeException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的参数过多错误
		/// </summary>
		public TooManyArgumentsException() : base("Too Many Arguments") {
			// "Too Many Arguments" - 参数过多
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的参数过多异常
		/// </summary>
		/// <param name="message">自定义错误消息</param>
		public TooManyArgumentsException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的参数过多异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public TooManyArgumentsException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 限制超出异常
	/// 功能: 表示运行时超出了某种资源或操作限制
	/// 继承: RuntimeException
	/// 说明: 当程序执行超出预设限制时抛出,如:
	///      - 执行时间过长
	///      - 递归深度过深
	///      - 内存使用超限
	///      这是一种安全机制,防止无限循环或资源耗尽
	/// </summary>
	public class LimitExceededException : RuntimeException {
		/// <summary>
		/// 默认构造函数: 创建一个默认的限制超出错误
		/// </summary>
		public LimitExceededException() : base("Runtime Limit Exceeded") {
			// "Runtime Limit Exceeded" - 运行时限制超出
		}

		/// <summary>
		/// 构造函数: 创建一个包含自定义错误消息的限制超出异常
		/// </summary>
		/// <param name="message">自定义错误消息,应说明具体超出了哪种限制</param>
		public LimitExceededException(string message) : base(message) {
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的限制超出异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public LimitExceededException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 未定义标识符异常
	/// 功能: 表示使用了未定义的变量或函数名
	/// 继承: RuntimeException
	/// 说明: 当尝试访问不存在的变量或函数时抛出
	///      例如: 使用了从未声明或赋值的变量名
	/// </summary>
	public class UndefinedIdentifierException : RuntimeException {
		/// <summary>
		/// 构造函数: 创建一个未定义标识符异常
		/// </summary>
		/// <param name="ident">未定义的标识符名称</param>
		/// 说明: 这个构造函数会自动生成包含标识符名称的错误消息
		public UndefinedIdentifierException(string ident) : base(
		"Undefined Identifier: '" + ident + "' is unknown in this context") {
			// 生成的消息格式: "Undefined Identifier: 'varName' is unknown in this context"
			// "未定义的标识符: 'varName' 在此上下文中未知"
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的未定义标识符异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public UndefinedIdentifierException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 未定义局部变量异常
	/// 功能: 表示使用了未定义的局部变量
	/// 继承: RuntimeException
	/// 说明: 与UndefinedIdentifierException类似,但特指局部作用域中的变量
	///      这种区分有助于更精确地诊断作用域相关的问题
	/// </summary>
	public class UndefinedLocalException : RuntimeException {
		/// <summary>
		/// 私有默认构造函数
		/// </summary>
		/// 说明: 这个构造函数被标记为private,外部代码不能调用
		///      这是一种设计模式,强制调用者必须提供标识符名称
		///      空实现表示不执行任何操作
		private UndefinedLocalException() {}		// don't call this version!
		// 不要调用这个版本!

		/// <summary>
		/// 构造函数: 创建一个未定义局部变量异常
		/// </summary>
		/// <param name="ident">未定义的局部变量标识符名称</param>
		/// 说明: 这是唯一可以从外部调用的构造函数
		///      强制要求提供变量名,生成有意义的错误消息
		public UndefinedLocalException(string ident) : base(
		"Undefined Local Identifier: '" + ident + "' is unknown in this context") {
			// 生成的消息格式: "Undefined Local Identifier: 'localVar' is unknown in this context"
			// "未定义的局部标识符: 'localVar' 在此上下文中未知"
		}

		/// <summary>
		/// 构造函数: 创建一个包含内部异常的未定义局部变量异常
		/// </summary>
		/// <param name="message">错误消息</param>
		/// <param name="inner">内部异常</param>
		public UndefinedLocalException(string message, Exception inner) : base(message, inner) {
		}
	}

	/// <summary>
	/// 检查工具类
	/// 功能: 提供常用的检查和验证静态方法
	/// 说明: static class - 静态类,不能被实例化,只能包含静态成员
	///      这是一个工具类模式,提供一组相关的辅助方法
	///      所有方法都是静态的,可以直接通过类名调用,无需创建对象
	/// C#高级特性:
	///      - 静态类在编译时会被标记为sealed和abstract
	///      - 不能包含实例构造函数
	///      - 不能被继承
	///      - 适合作为工具类或扩展方法的容器
	/// </summary>
	public static class Check {
		/// <summary>
		/// 范围检查方法
		/// 功能: 检查一个整数是否在指定范围内
		/// </summary>
		/// <param name="i">要检查的整数值</param>
		/// <param name="min">允许的最小值(包含)</param>
		/// <param name="max">允许的最大值(包含)</param>
		/// <param name="desc">描述性文本,用于错误消息(默认为"index")</param>
		/// <exception cref="IndexException">当i超出[min, max]范围时抛出</exception>
		/// 说明: static方法属于类本身,不属于任何对象实例
		///      可以通过 Check.Range(...) 直接调用
		/// C#特性: 参数默认值
		///      desc="index" 是参数的默认值
		///      调用时如果不提供这个参数,会自动使用"index"
		///      这是C# 4.0引入的特性,称为"可选参数"或"命名参数"
		public static void Range(int i, int min, int max, string desc="index") {
			// 检查i是否小于min或大于max
			if (i < min || i > max) {
				// || : 逻辑或运算符,两个条件任一为真则整体为真
				
				// throw: 关键字,用于抛出异常
				// 说明: 抛出异常会中断当前方法的执行
				//      异常会沿调用栈向上传播,直到被catch捕获
				throw new IndexException(string.Format("Index Error: {0} ({1}) out of range ({2} to {3})",
					desc, i, min, max));
				// string.Format的占位符说明:
				// {0} = desc (描述)
				// {1} = i (实际值)
				// {2} = min (最小值)
				// {3} = max (最大值)
				// 示例输出: "Index Error: index (10) out of range (0 to 5)"
			}
		}
		
		/// <summary>
		/// 类型检查方法
		/// 功能: 检查一个Value对象是否是指定的类型
		/// </summary>
		/// <param name="val">要检查的Value对象</param>
		/// <param name="requiredType">需要的类型(System.Type对象)</param>
		/// <param name="desc">描述性文本,用于错误消息(可选,默认为null)</param>
		/// <exception cref="TypeException">当val不是requiredType类型时抛出</exception>
		/// 说明: Value是MiniScript中的值类型(在其他文件中定义)
		/// C#高级特性:
		///      - System.Type: 表示类型信息的类,用于反射
		///      - IsInstanceOfType: Type类的方法,运行时类型检查
		///      - 这是使用反射(Reflection)进行类型检查
		/// 反射说明:
		///      反射是C#的高级特性,允许在运行时检查和操作类型信息
		///      System.Type对象包含了类型的元数据(metadata)
		///      IsInstanceOfType相当于增强版的is运算符,但接受Type对象作为参数
		public static void Type(Value val, System.Type requiredType, string desc=null) {
			// IsInstanceOfType: Type类的实例方法
			// 说明: 检查val是否是requiredType或其派生类型的实例
			//      这是运行时类型检查,比编译时的is运算符更灵活
			//      因为requiredType是一个变量,可以在运行时动态确定
			if (!requiredType.IsInstanceOfType(val)) {
				// ! : 逻辑非运算符,反转布尔值
				// 如果val不是requiredType类型,则执行if块
				
				// 三元条件运算符,确定typeStr的值
				string typeStr = val == null ? "null" : "a " + val.GetType();
				// 如果val为null,typeStr为"null"
				// 否则,使用GetType()获取val的实际类型,并加上"a "前缀
				// GetType(): Object类的方法,返回对象的运行时类型(System.Type)
				
				throw new TypeException(string.Format("got {0} where a {1} was required{2}",
					typeStr, requiredType, desc == null ? null : " (" + desc + ")"));
				// {0} = typeStr (实际得到的类型)
				// {1} = requiredType (需要的类型)
				// {2} = 描述(如果提供)
				// 示例输出: "got a String where a Number was required (function parameter)"
				//          或 "got null where a List was required"
			}
		}
	}
}