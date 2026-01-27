/*	MiniscriptInterpreter.cs

The only class in this file is Interpreter, which is your main interface 
to the MiniScript system.  You give Interpreter some MiniScript source 
code, and tell it where to send its output (via delegate functions called
TextOutputMethod).  Then you typically call RunUntilDone, which returns 
when either the script has stopped or the given timeout has passed.  

For details, see Chapters 1-3 of the MiniScript Integration Guide.

此文件中唯一的类是Interpreter,它是MiniScript系统的主要接口。
你需要给Interpreter提供MiniScript源代码,并告诉它将输出发送到哪里
(通过名为TextOutputMethod的委托函数)。然后通常调用RunUntilDone方法,
当脚本停止或给定的超时时间已过时,该方法返回。

详细信息请参阅MiniScript集成指南的第1-3章。
*/

using System;
using System.Collections.Generic;

namespace Miniscript {

	/// <summary>
	/// TextOutputMethod: a delegate used to return text from the script
	/// (e.g. normal output, errors, etc.) to your C# code.
	/// 
	/// TextOutputMethod: 一个委托类型,用于将文本从脚本返回到你的C#代码
	/// (例如:正常输出、错误信息等)
	/// 
	/// 【委托说明】:
	/// 委托(delegate)是C#中的一种类型,它表示对具有特定参数列表和返回类型的方法的引用。
	/// 可以理解为"函数指针"或"回调函数"。
	/// 使用委托的好处:可以将方法作为参数传递,实现回调机制,解耦代码。
	/// 
	/// 本委托定义了一个函数签名:
	/// - 参数1: string output - 要输出的文本内容
	/// - 参数2: bool addLineBreak - 是否添加换行符
	/// - 返回值: void - 无返回值
	/// </summary>
	/// <param name="output">要输出的文本内容</param>
	/// <param name="addLineBreak">是否在输出后添加换行符</param>
	public delegate void TextOutputMethod(string output, bool addLineBreak);

	/// <summary>
	/// Interpreter: an object that contains and runs one MiniScript script.
	/// 
	/// Interpreter: 包含并运行一个MiniScript脚本的对象
	/// 
	/// 【类的核心职责】:
	/// 1. 管理脚本源代码
	/// 2. 编译源代码为可执行的虚拟机指令
	/// 3. 控制虚拟机的执行(启动、停止、单步执行等)
	/// 4. 处理脚本的各种输出(标准输出、隐式输出、错误输出)
	/// 5. 提供REPL(交互式命令行)功能
	/// 6. 管理全局变量的读写
	/// </summary>
	public class Interpreter {
		
		/// <summary>
		/// standardOutput: receives the output of the "print" intrinsic.
		/// 
		/// standardOutput: 接收"print"内置函数的输出
		/// 
		/// 【属性说明】:
		/// 这是一个属性(Property),它封装了私有字段_standardOutput。
		/// 使用get/set访问器实现了读写控制。
		/// 
		/// 当设置standardOutput时,不仅更新本地字段,还会同步更新虚拟机(vm)中的对应设置。
		/// 这确保了虚拟机执行print语句时能正确输出到指定的位置。
		/// 
		/// 典型用法:
		/// interpreter.standardOutput = (text, addLineBreak) => Console.WriteLine(text);
		/// </summary>
		public TextOutputMethod standardOutput {
			get {
				return _standardOutput;  // 返回私有字段的值
			}
			set {
				_standardOutput = value;  // 设置私有字段
				// 如果虚拟机已创建,同步更新虚拟机的输出设置
				// null条件运算符(?.)是C# 6.0引入的特性,只有在vm不为null时才访问其属性
				if (vm != null) vm.standardOutput = value;
			}
		}
		
		/// <summary>
		/// implicitOutput: receives the value of expressions entered when
		/// in REPL mode.  If you're not using the REPL() method, you can
		/// safely ignore this.
		/// 
		/// implicitOutput: 在REPL模式下接收输入表达式的值。
		/// 如果你不使用REPL()方法,可以安全地忽略这个属性。
		/// 
		/// 【REPL模式说明】:
		/// REPL = Read-Eval-Print Loop (读取-求值-输出 循环)
		/// 这是一种交互式编程环境,常见于Python、JavaScript等语言的交互式shell。
		/// 
		/// 例如在REPL中输入: 2 + 3
		/// 不需要显式调用print,系统会自动输出: 5
		/// 这个自动输出就是通过implicitOutput委托实现的。
		/// 
		/// 与standardOutput的区别:
		/// - standardOutput: 处理显式的print语句输出
		/// - implicitOutput: 处理表达式的隐式结果输出(仅REPL模式)
		/// </summary>
		public TextOutputMethod implicitOutput;
		
		/// <summary>
		/// errorOutput: receives error messages from the runtime.  (This happens
		/// via the ReportError method, which is virtual; so if you want to catch
		/// the actual exceptions rather than get the error messages as strings,
		/// you can subclass Interpreter and override that method.)
		/// 
		/// errorOutput: 接收运行时的错误消息。(这通过ReportError方法实现,
		/// 该方法是虚方法;因此如果你想捕获实际的异常而不是获取错误消息字符串,
		/// 可以继承Interpreter类并重写该方法。)
		/// 
		/// 【虚方法说明】:
		/// virtual关键字标记的方法可以在派生类中被override(重写)。
		/// 这是面向对象编程中"多态"的实现方式之一。
		/// 
		/// 使用场景:
		/// - 默认行为: 将错误信息作为字符串输出
		/// - 自定义行为: 继承Interpreter,重写ReportError,实现自定义错误处理
		///   (例如:记录日志、显示GUI对话框、发送错误报告等)
		/// </summary>
		public TextOutputMethod errorOutput;
		
		/// <summary>
		/// hostData is just a convenient place for you to attach some arbitrary
		/// data to the interpreter.  It gets passed through to the context object,
		/// so you can access it inside your custom intrinsic functions.  Use it
		/// for whatever you like (or don't, if you don't feel the need).
		/// 
		/// hostData只是一个方便的地方,供你将任意数据附加到解释器上。
		/// 它会被传递到上下文对象,因此你可以在自定义的内置函数中访问它。
		/// 你可以随意使用它(或者如果不需要也可以不用)。
		/// 
		/// 【object类型说明】:
		/// object是C#中所有类型的基类。使用object类型意味着可以存储任何类型的数据。
		/// 
		/// 典型使用场景:
		/// - 游戏开发: 存储游戏对象引用
		/// - 应用程序: 存储应用程序上下文
		/// - 自定义功能: 在脚本和宿主程序之间传递数据
		/// 
		/// 示例:
		/// interpreter.hostData = myGameObject;
		/// // 在自定义内置函数中:
		/// var gameObj = context.interpreter.hostData as GameObject;
		/// </summary>
		public object hostData;
		
		/// <summary>
		/// done: returns true when we don't have a virtual machine, or we do have
		/// one and it is done (has reached the end of its code).
		/// 
		/// done: 当没有虚拟机,或有虚拟机且已完成(已到达代码末尾)时返回true。
		/// 
		/// 【只读属性说明】:
		/// 这个属性只有get访问器,没有set访问器,因此是只读的。
		/// 它的值是根据内部状态计算得出的,不能直接设置。
		/// 
		/// 【逻辑说明】:
		/// 返回true的情况:
		/// 1. vm == null: 虚拟机还未创建(脚本未编译)
		/// 2. vm.done == true: 虚拟机存在但已执行完毕
		/// 
		/// 使用场景:
		/// - 检查脚本是否执行完成
		/// - 决定是否需要继续调用RunUntilDone
		/// </summary>
		public bool done {
			// || 是逻辑或运算符,左侧为true时不再计算右侧(短路求值)
			get { return vm == null || vm.done; }	
		}
		
		/// <summary>
		/// vm: the virtual machine this interpreter is running.  Most applications will
		/// not need to use this, but it's provided for advanced users.
		/// 
		/// vm: 此解释器正在运行的虚拟机。大多数应用程序不需要使用它,
		/// 但为高级用户提供了访问。
		/// 
		/// 【虚拟机说明】:
		/// TAC.Machine是三地址码(Three-Address Code)虚拟机的实现。
		/// 它负责实际执行编译后的字节码指令。
		/// 
		/// 为什么大多数用户不需要直接访问:
		/// - Interpreter类已经封装了常用操作(运行、停止、单步等)
		/// - 直接操作vm需要了解底层实现细节
		/// 
		/// 高级用户可能的使用场景:
		/// - 调试: 检查虚拟机状态、堆栈等
		/// - 性能分析: 访问执行统计信息
		/// - 自定义控制: 实现特殊的执行流程
		/// </summary>
		public TAC.Machine vm;
		
		// 【私有字段说明】:
		// 以下是类的私有字段,只能在类内部访问,外部无法直接访问。
		// 这是封装(Encapsulation)原则的体现,隐藏内部实现细节。
		
		/// <summary>
		/// _standardOutput: 标准输出委托的私有存储字段
		/// 使用下划线前缀是C#的命名约定,表示私有字段
		/// 通过standardOutput属性对外暴露,实现封装
		/// </summary>
		TextOutputMethod _standardOutput;
		
		/// <summary>
		/// source: 存储MiniScript源代码的字符串
		/// 这是编译和执行的输入
		/// </summary>
		string source;
		
		/// <summary>
		/// parser: 解析器对象,负责将源代码转换为抽象语法树(AST)
		/// 然后进一步转换为虚拟机可执行的指令
		/// </summary>
		Parser parser;
		
		/// <summary>
		/// Constructor taking some MiniScript source code, and the output delegates.
		/// 
		/// 构造函数,接受MiniScript源代码和输出委托。
		/// 
		/// 【构造函数说明】:
		/// 构造函数用于创建类的实例时初始化对象。
		/// 它与类同名,没有返回类型。
		/// 
		/// 【可选参数说明】:
		/// C#支持可选参数(参数有默认值)。调用时可以省略这些参数。
		/// source=null 表示如果不提供源代码,默认为null
		/// standardOutput=null 和 errorOutput=null 同理
		/// 
		/// 【null合并运算符应用】:
		/// 如果传入的输出委托为null,使用lambda表达式创建默认实现。
		/// 
		/// 【Lambda表达式说明】:
		/// (s,eol) => Console.WriteLine(s) 是lambda表达式语法
		/// 等价于:
		/// void DefaultOutput(string s, bool eol) {
		///     Console.WriteLine(s);
		/// }
		/// 
		/// lambda表达式是创建匿名函数的简洁方式,常用于委托和LINQ。
		/// </summary>
		/// <param name="source">MiniScript源代码字符串,可选</param>
		/// <param name="standardOutput">标准输出委托,可选,默认输出到控制台</param>
		/// <param name="errorOutput">错误输出委托,可选,默认输出到控制台</param>
		public Interpreter(string source=null, TextOutputMethod standardOutput=null, TextOutputMethod errorOutput=null) {
			// 保存源代码
			this.source = source;
			
			// 如果没有提供standardOutput,创建默认实现(输出到控制台)
			// Console.WriteLine是.NET Framework提供的控制台输出方法
			if (standardOutput == null) standardOutput = (s,eol) => Console.WriteLine(s);
			
			// 如果没有提供errorOutput,同样默认输出到控制台
			if (errorOutput == null) errorOutput = (s,eol) => Console.WriteLine(s);
			
			// 使用属性设置,会触发set访问器的逻辑
			this.standardOutput = standardOutput;
			this.errorOutput = errorOutput;
		}
		
		/// <summary>
		/// Constructor taking source code in the form of a list of strings.
		/// 
		/// 构造函数,接受字符串列表形式的源代码。
		/// 
		/// 【构造函数链式调用说明】:
		/// : this(...) 语法表示调用同一类的另一个构造函数
		/// 这是构造函数链(constructor chaining),避免代码重复
		/// 
		/// 【string.Join说明】:
		/// string.Join是.NET提供的字符串连接方法
		/// 第一个参数是分隔符("\n"表示换行符)
		/// 第二个参数是要连接的字符串数组
		/// 
		/// 【ToArray()说明】:
		/// List<string>的ToArray()方法将列表转换为数组
		/// 这是必要的,因为string.Join期望接收数组参数
		/// 
		/// 执行流程:
		/// 1. 将List<string>转换为string[]数组
		/// 2. 用换行符连接所有字符串
		/// 3. 调用主构造函数,传入连接后的字符串
		/// </summary>
		/// <param name="source">源代码行的列表</param>
		public Interpreter(List<string> source) : this(string.Join("\n", source.ToArray())) {
		}
		
		/// <summary>
		/// Constructor taking source code in the form of a string array.
		/// 
		/// 构造函数,接受字符串数组形式的源代码。
		/// 
		/// 与上一个构造函数类似,但直接接受string[]数组,不需要ToArray()转换
		/// 
		/// 这三个构造函数提供了灵活性:
		/// - 单个字符串
		/// - 字符串列表
		/// - 字符串数组
		/// 
		/// 都会被转换为单个字符串存储在source字段中
		/// </summary>
		/// <param name="source">源代码行的数组</param>
		public Interpreter(string[] source) : this(string.Join("\n", source)) {
		}
		
		/// <summary>
		/// Stop the virtual machine, and jump to the end of the program code.
		/// Also reset the parser, in case it's stuck waiting for a block ender.
		/// 
		/// 停止虚拟机,并跳转到程序代码的末尾。
		/// 同时重置解析器,以防它卡在等待块结束符的状态。
		/// 
		/// 【方法功能】:
		/// 这是一个紧急停止方法,用于强制终止脚本执行。
		/// 
		/// 【使用场景】:
		/// - 用户请求终止脚本
		/// - 检测到死循环或无限递归
		/// - 错误恢复
		/// 
		/// 【实现细节】:
		/// 1. 如果虚拟机存在,调用其Stop()方法
		/// 2. 如果解析器存在,调用PartialReset()方法
		/// 
		/// PartialReset()的作用:
		/// 在REPL模式下,解析器可能处于"等待更多输入"状态
		/// (例如,用户输入了"if x then"但没有输入"end if")
		/// PartialReset清除这种状态,准备接收新的输入
		/// </summary>
		public void Stop() {
			// null条件运算符确保只在vm不为null时调用Stop()
			if (vm != null) vm.Stop();
			// 同样,只在parser不为null时调用PartialReset()
			if (parser != null) parser.PartialReset();
		}
		
		/// <summary>
		/// Reset the interpreter with the given source code.
		/// 
		/// 用给定的源代码重置解释器。
		/// 
		/// 【方法功能】:
		/// 清除所有内部状态,准备处理新的源代码。
		/// 
		/// 【与Stop()的区别】:
		/// - Stop(): 停止当前执行,但保留编译后的代码和虚拟机
		/// - Reset(): 完全清除一切,包括编译后的代码、虚拟机、解析器
		/// 
		/// 【使用场景】:
		/// - 加载新脚本
		/// - 清除旧脚本的所有痕迹
		/// - 释放内存(将vm和parser设为null,GC可以回收它们)
		/// 
		/// 【默认参数】:
		/// source=""表示如果不提供参数,默认清空源代码
		/// </summary>
		/// <param name="source">新的源代码,默认为空字符串</param>
		public void Reset(string source="") {
			this.source = source;  // 设置新的源代码
			parser = null;         // 清除解析器
			vm = null;             // 清除虚拟机
		}
		
		/// <summary>
		/// Compile our source code, if we haven't already done so, so that we are
		/// either ready to run, or generate compiler errors (reported via errorOutput).
		/// 
		/// 如果还没有编译,则编译我们的源代码,这样我们就准备好运行了,
		/// 或者生成编译器错误(通过errorOutput报告)。
		/// 
		/// 【方法功能】:
		/// 将MiniScript源代码编译为虚拟机可执行的指令。
		/// 
		/// 【幂等性】:
		/// 这个方法是幂等的(idempotent),多次调用不会重复编译。
		/// 如果vm已存在,直接返回,不做任何操作。
		/// 
		/// 【编译流程】:
		/// 1. 检查是否已编译(vm != null)
		/// 2. 创建或重用解析器
		/// 3. 解析源代码(parser.Parse)
		/// 4. 创建虚拟机(parser.CreateVM)
		/// 5. 设置虚拟机的解释器引用
		/// 
		/// 【异常处理】:
		/// 使用try-catch捕获MiniscriptException
		/// 如果编译失败,通过ReportError报告错误
		/// 
		/// 【WeakReference说明】:
		/// WeakReference是.NET提供的弱引用类型。
		/// 普通引用会阻止对象被垃圾回收,而弱引用不会。
		/// 
		/// 为什么使用弱引用:
		/// - 避免循环引用: vm引用interpreter,interpreter引用vm
		/// - 允许解释器在不再使用时被垃圾回收
		/// - vm仍可通过WeakReference访问interpreter(如果它还活着)
		/// 
		/// 【错误恢复】:
		/// 如果编译失败且vm为null,将parser也设为null
		/// 这确保下次调用Compile时会重新创建parser
		/// </summary>
		public void Compile() {
			// 如果已经编译过,直接返回
			if (vm != null) return;	// already compiled

			// 如果解析器不存在,创建新的解析器
			if (parser == null) parser = new Parser();
			
			try {
				// 解析源代码,构建抽象语法树(AST)和指令序列
				parser.Parse(source);
				
				// 从解析器创建虚拟机,传入标准输出委托
				// CreateVM方法会将AST转换为虚拟机指令
				vm = parser.CreateVM(standardOutput);
				
				// 设置虚拟机对解释器的弱引用
				// new WeakReference(this) 创建指向当前Interpreter实例的弱引用
				vm.interpreter = new WeakReference(this);
				
			} catch (MiniscriptException mse) {
				// 捕获MiniScript特定的异常(编译错误)
				
				// 报告错误(通过errorOutput委托)
				ReportError(mse);
				
				// 如果虚拟机创建失败,清除解析器
				// 这样下次调用Compile时会重新开始
				if (vm == null) parser = null;
			}
		}
		
		/// <summary>
		/// Reset the virtual machine to the beginning of the code.  Note that this
		/// does *not* reset global variables; it simply clears the stack and jumps
		/// to the beginning.  Useful in cases where you have a short script you
		/// want to run over and over, without recompiling every time.
		/// 
		/// 将虚拟机重置到代码的开头。注意,这*不会*重置全局变量;
		/// 它只是清除堆栈并跳转到开头。在你有一个想要反复运行的短脚本,
		/// 而不想每次都重新编译的情况下很有用。
		/// 
		/// 【方法功能】:
		/// 重新开始执行已编译的代码,但保留编译结果和全局状态。
		/// 
		/// 【与Reset()的区别】:
		/// - Reset(): 清除一切(源代码、编译结果、虚拟机、全局变量)
		/// - Restart(): 只重置执行位置,保留编译结果和全局变量
		/// 
		/// 【使用场景】:
		/// - 游戏循环: 每帧运行相同的脚本
		/// - 测试: 用不同的全局变量值运行相同的脚本
		/// - 性能优化: 避免重复编译的开销
		/// 
		/// 【保留的状态】:
		/// - 编译后的虚拟机指令
		/// - 全局变量的值
		/// - 函数定义
		/// 
		/// 【清除的状态】:
		/// - 执行堆栈
		/// - 程序计数器(跳回开头)
		/// - 局部变量
		/// </summary>
		public void Restart() {
			// 如果虚拟机存在,调用其Reset方法
			// vm.Reset()将程序计数器重置为0,清空调用堆栈等
			if (vm != null) vm.Reset();			
		}
		
		/// <summary>
		/// Run the compiled code until we either reach the end, or we reach the
		/// specified time limit.  In the latter case, you can then call RunUntilDone
		/// again to continue execution right from where it left off.
		/// 
		/// Or, if returnEarly is true, we will also return if we reach an intrinsic
		/// method that returns a partial result, indicating that it needs to wait
		/// for something.  Again, call RunUntilDone again later to continue.
		/// 
		/// Note that this method first compiles the source code if it wasn't compiled
		/// already, and in that case, may generate compiler errors.  And of course
		/// it may generate runtime errors while running.  In either case, these are
		/// reported via errorOutput.
		/// 
		/// 运行编译后的代码,直到到达末尾或达到指定的时间限制。
		/// 在后一种情况下,你可以再次调用RunUntilDone从中断的地方继续执行。
		/// 
		/// 或者,如果returnEarly为true,当到达返回部分结果的内置方法时也会返回,
		/// 表明它需要等待某些东西。同样,稍后再次调用RunUntilDone继续执行。
		/// 
		/// 注意,此方法会首先编译源代码(如果还没有编译),在这种情况下,
		/// 可能会生成编译器错误。当然,运行时也可能生成运行时错误。
		/// 无论哪种情况,这些错误都通过errorOutput报告。
		/// 
		/// 【方法功能】:
		/// 这是主要的执行方法,用于运行MiniScript代码。
		/// 
		/// 【时间分片执行】:
		/// timeLimit参数实现了时间分片(time-slicing)
		/// 允许脚本在长时间运行时不阻塞主程序
		/// 
		/// 【异步操作支持】:
		/// returnEarly和partialResult机制支持异步操作
		/// 例如:等待网络请求、用户输入等
		/// 
		/// 【执行流程】:
		/// 1. 记录起始状态(隐式结果计数)
		/// 2. 如果未编译,先编译
		/// 3. 记录起始时间
		/// 4. 循环执行虚拟机步骤,直到:
		///    - 代码执行完成(vm.done)
		///    - 达到时间限制
		///    - 脚本主动让出(vm.yielding)
		///    - 遇到需要等待的操作(partialResult)
		/// 5. 检查是否有新的隐式结果需要输出
		/// 
		/// 【部分结果机制】:
		/// partialResult用于处理需要等待的操作:
		/// - 内置函数返回partialResult表示"我需要等待"
		/// - RunUntilDone返回,但不标记为done
		/// - 外部代码处理等待的事件
		/// - 再次调用RunUntilDone继续执行
		/// 
		/// 【异常处理】:
		/// 捕获MiniscriptException并报告
		/// 然后调用Stop()终止执行,防止错误状态继续
		/// </summary>
		/// <param name="timeLimit">运行的最大时间(秒),默认60秒</param>
		/// <param name="returnEarly">如果为true,遇到返回部分结果的内置函数时提前返回</param>
		public void RunUntilDone(double timeLimit=60, bool returnEarly=true) {
			// 记录开始时的隐式结果计数器值
			// 用于后续检查是否产生了新的隐式结果
			int startImpResultCount = 0;
			
			try {
				// 如果虚拟机不存在,先编译
				if (vm == null) {
					Compile();
					// 编译后再次检查,如果还是null说明编译失败
					if (vm == null) return;	// (must have been some error)
				}
				
				// 记录开始时的隐式结果计数
				startImpResultCount = vm.globalContext.implicitResultCounter;
				
				// 记录开始时间,用于检查是否超时
				double startTime = vm.runTime;
				
				// 重置yielding标志
				// yielding表示脚本主动让出控制权(例如调用了yield函数)
				vm.yielding = false;
				
				// 主执行循环:当虚拟机未完成且未让出时继续执行
				while (!vm.done && !vm.yielding) {
					// ToDo注释说明了一个性能问题:
					// vm.runTime的调用占用了约14%的运行时间
					// 建议考虑使用Environment.TickCount替代
					// Environment.TickCount返回系统启动后的毫秒数
					// 注意:它每25天会溢出回到0,需要处理这种情况
					
					// 检查是否超时
					if (vm.runTime - startTime > timeLimit) return;	// time's up for now!
					
					// 执行一步虚拟机指令
					// Step()方法执行一条三地址码指令
					vm.Step();		// update the machine
					
					// 如果启用了提前返回,且当前上下文有部分结果
					// 部分结果表示正在等待某个异步操作完成
					if (returnEarly && vm.GetTopContext().partialResult != null) return;	// waiting for something
				}
				
			} catch (MiniscriptException mse) {
				// 捕获运行时错误
				ReportError(mse);  // 报告错误
				
				// 停止执行
				// 注释中的"was:"表示这里的实现被修改过
				// 原来是: vm.GetTopContext().JumpToEnd();
				// 现在改为: Stop();
				// JumpToEnd只是跳到代码末尾,而Stop()会做更彻底的清理
				Stop(); // was: vm.GetTopContext().JumpToEnd();
			}
			
			// 检查是否有新的隐式结果需要输出
			// 这用于REPL模式,自动显示表达式的值
			CheckImplicitResult(startImpResultCount);
		}
		
		/// <summary>
		/// Run one step of the virtual machine.  This method is not very useful
		/// except in special cases; usually you will use RunUntilDone (above) instead.
		/// 
		/// 运行虚拟机的一步。这个方法除了特殊情况外不是很有用;
		/// 通常你会使用RunUntilDone(上面)代替。
		/// 
		/// 【方法功能】:
		/// 执行单条虚拟机指令,然后立即返回。
		/// 
		/// 【使用场景】:
		/// - 调试: 单步执行以观察每条指令的效果
		/// - 精细控制: 需要在每条指令之间执行某些操作
		/// - 性能分析: 测量每条指令的执行时间
		/// - 自定义执行循环: 实现特殊的执行逻辑
		/// 
		/// 【与RunUntilDone的区别】:
		/// - Step(): 执行一条指令后立即返回
		/// - RunUntilDone(): 执行多条指令直到完成或超时
		/// 
		/// 【实现细节】:
		/// 1. 确保代码已编译(调用Compile)
		/// 2. 执行一步虚拟机
		/// 3. 捕获并报告任何异常
		/// 
		/// 注意:每次调用都会检查编译状态,这对于REPL模式很重要
		/// </summary>
		public void Step() {
			try {
				// 确保代码已编译
				// 如果已编译,Compile()会立即返回,开销很小
				Compile();
				
				// 执行一条虚拟机指令
				vm.Step();
				
			} catch (MiniscriptException mse) {
				// 捕获异常并报告
				ReportError(mse);
				
				// 发生错误时停止执行
				Stop(); // was: vm.GetTopContext().JumpToEnd();
			}
		}

		/// <summary>
		/// Read Eval Print Loop.  Run the given source until it either terminates,
		/// or hits the given time limit.  When it terminates, if we have new
		/// implicit output, print that to the implicitOutput stream.
		/// 
		/// 读取-求值-输出 循环。运行给定的源代码,直到它终止或达到给定的时间限制。
		/// 当它终止时,如果有新的隐式输出,将其打印到implicitOutput流。
		/// 
		/// 【REPL模式详解】:
		/// REPL是交互式编程环境的标准模式,常见于:
		/// - Python交互式shell
		/// - JavaScript浏览器控制台
		/// - 各种语言的命令行工具
		/// 
		/// 工作流程:
		/// 1. Read: 读取用户输入的一行代码
		/// 2. Eval: 求值/执行这行代码
		/// 3. Print: 打印结果(如果有)
		/// 4. Loop: 回到第1步,等待下一行输入
		/// 
		/// 【增量编译】:
		/// REPL模式支持增量编译,即:
		/// - 保留之前的代码和状态
		/// - 只编译新输入的代码
		/// - 累积构建程序
		/// 
		/// 【内存优化】:
		/// 当虚拟机和解析器都完成时,清除已编译的代码以节省内存
		/// 
		/// 【特殊命令】:
		/// #DUMP: 调试命令,输出当前执行上下文的详细信息
		/// 
		/// 【方法流程】:
		/// 1. 初始化解析器和虚拟机(如果需要)
		/// 2. 检查是否可以清理旧代码(内存优化)
		/// 3. 处理特殊命令(如#DUMP)
		/// 4. 记录起始状态
		/// 5. 解析新输入的代码
		/// 6. 执行代码(如果解析完成)
		/// 7. 输出隐式结果(如果有)
		/// 8. 捕获并报告错误
		/// </summary>
		/// <param name="sourceLine">用户输入的一行源代码</param>
		/// <param name="timeLimit">最大执行时间(秒),默认60秒</param>
		public void REPL(string sourceLine, double timeLimit=60) {
			// 如果解析器不存在,创建新的
			if (parser == null) parser = new Parser();
			
			// 如果虚拟机不存在,从解析器创建
			if (vm == null) {
				vm = parser.CreateVM(standardOutput);
				vm.interpreter = new WeakReference(this);
				
			} else if (vm.done && !parser.NeedMoreInput()) {
				// 内存优化:如果虚拟机和解析器都完成了
				// 清除之前编译的代码,释放内存
				// 这是安全的,因为我们不再需要旧代码了
				
				// Since the machine and parser are both done, we don't really need the
				// previously-compiled code.  So let's clear it out, as a memory optimization.
				// 由于机器和解析器都完成了,我们真的不需要之前编译的代码了。
				// 所以让我们清除它,作为内存优化。
				
				vm.GetTopContext().ClearCodeAndTemps();  // 清除代码和临时变量
				parser.PartialReset();  // 部分重置解析器
			}
			
			// 处理特殊调试命令
			if (sourceLine == "#DUMP") {
				// 输出当前执行上下文的详细信息
				// 包括:变量、堆栈、指令等
				vm.DumpTopContext();
				return;  // 处理完直接返回
			}
			
			// 记录开始时间,用于超时检查
			double startTime = vm.runTime;
			
			// 记录开始时的隐式结果计数
			int startImpResultCount = vm.globalContext.implicitResultCounter;
			
			// 设置是否存储隐式结果
			// 只有在设置了implicitOutput委托时才存储
			// 这样可以避免不必要的计算
			vm.storeImplicit = (implicitOutput != null);
			
			// 重置yielding标志
			vm.yielding = false;

			try {
				// 如果提供了源代码行,解析它
				// 第二个参数true表示这是REPL模式
				// REPL模式下,解析器会:
				// - 支持不完整的代码块(等待更多输入)
				// - 自动存储表达式结果
				if (sourceLine != null) parser.Parse(sourceLine, true);
				
				// 只有在解析器不需要更多输入时才执行
				// 例如,如果用户输入了"if x then",解析器会等待"end if"
				if (!parser.NeedMoreInput()) {
					// 执行循环,直到完成、让出或超时
					while (!vm.done && !vm.yielding) {
						// 检查超时
						if (vm.runTime - startTime > timeLimit) return;	// time's up for now!
						// 执行一步
						vm.Step();
					}
					
					// 检查并输出隐式结果
					CheckImplicitResult(startImpResultCount);
				}

			} catch (MiniscriptException mse) {
				// 捕获异常并报告
				ReportError(mse);
				
				// Attempt to recover from an error by jumping to the end of the code.
				// 尝试通过跳到代码末尾来从错误中恢复。
				
				// 停止执行,清理状态
				Stop(); // was: vm.GetTopContext().JumpToEnd();
			}
		}
		
		/// <summary>
		/// Report whether the virtual machine is still running, that is,
		/// whether it has not yet reached the end of the program code.
		/// 
		/// 报告虚拟机是否仍在运行,即它是否还没有到达程序代码的末尾。
		/// 
		/// 【方法功能】:
		/// 检查脚本是否正在执行中。
		/// 
		/// 【返回值】:
		/// true: 虚拟机存在且未完成(正在运行)
		/// false: 虚拟机不存在或已完成
		/// 
		/// 【使用场景】:
		/// - 判断是否需要继续调用RunUntilDone
		/// - UI更新:显示脚本运行状态
		/// - 资源管理:决定是否可以释放相关资源
		/// 
		/// 【与done属性的区别】:
		/// - done: 返回true表示未运行或已完成
		/// - Running(): 返回true表示正在运行
		/// - 它们的逻辑是相反的
		/// 
		/// 注意:这个方法的实现与done属性基本相同但逻辑相反
		/// done: vm == null || vm.done
		/// Running: vm != null && !vm.done
		/// </summary>
		/// <returns>如果虚拟机正在运行返回true,否则返回false</returns>
		public bool Running() {
			// 虚拟机存在且未完成时返回true
			return vm != null && !vm.done;
		}
		
		/// <summary>
		/// Return whether the parser needs more input, for example because we have
		/// run out of source code in the middle of an "if" block.  This is typically
		/// used with REPL for making an interactive console, so you can change the
		/// prompt when more input is expected.
		/// 
		/// 返回解析器是否需要更多输入,例如因为我们在"if"块中间用完了源代码。
		/// 这通常与REPL一起使用来制作交互式控制台,这样当期望更多输入时可以更改提示符。
		/// 
		/// 【方法功能】:
		/// 检查解析器是否处于"等待更多输入"状态。
		/// 
		/// 【使用场景】:
		/// REPL交互式控制台中改变提示符:
		/// - 正常提示符: "> "
		/// - 继续提示符: "... " (表示需要完成代码块)
		/// 
		/// 【需要更多输入的情况】:
		/// 例如用户输入:
		/// > if x > 5 then
		/// 
		/// 这是一个不完整的if语句,缺少:
		/// - if主体
		/// - end if结束符
		/// 
		/// 此时NeedMoreInput()返回true,提示符变为:
		/// ... 
		/// 
		/// 用户继续输入:
		/// ...   print "big"
		/// ... end if
		/// 
		/// 现在代码块完整了,NeedMoreInput()返回false
		/// 
		/// 【其他需要更多输入的情况】:
		/// - while循环未结束
		/// - for循环未结束
		/// - 函数定义未结束
		/// - 字符串字面量未闭合
		/// - 括号未匹配
		/// </summary>
		/// <returns>如果解析器需要更多输入返回true,否则返回false</returns>
		public bool NeedMoreInput() {
			// 短路求值:parser为null时直接返回false,不调用NeedMoreInput()
			// 这避免了空引用异常
			return parser != null && parser.NeedMoreInput();
		}
		
		/// <summary>
		/// Get a value from the global namespace of this interpreter.
		/// 
		/// 从此解释器的全局命名空间获取一个值。
		/// 
		/// 【方法功能】:
		/// 读取MiniScript脚本中的全局变量。
		/// 
		/// 【使用场景】:
		/// - 从C#代码读取脚本设置的变量
		/// - 获取脚本执行的结果
		/// - 实现C#和MiniScript之间的双向通信
		/// 
		/// 【命名空间说明】:
		/// MiniScript有两种变量作用域:
		/// - 全局作用域: 在整个脚本中可见,存储在globalContext中
		/// - 局部作用域: 只在函数内可见,存储在局部上下文中
		/// 
		/// 这个方法只访问全局作用域。
		/// 
		/// 【返回值】:
		/// - 如果变量存在:返回其Value对象
		/// - 如果变量不存在:返回null
		/// - 如果虚拟机未创建:返回null
		/// 
		/// 【异常处理】:
		/// GetVar可能抛出UndefinedIdentifierException(未定义标识符异常)
		/// 这里捕获该异常并返回null,而不是让异常传播
		/// 
		/// 【Value类型说明】:
		/// Value是MiniScript的值类型,可以表示:
		/// - 数字、字符串、布尔值等基本类型
		/// - 列表、字典等复杂类型
		/// - 函数引用
		/// 
		/// 在C#中使用时可能需要转换,例如:
		/// Value v = interpreter.GetGlobalValue("score");
		/// int score = v.IntValue();
		/// </summary>
		/// <param name="varName">要获取的全局变量名</param>
		/// <returns>变量的值,如果未找到则返回null</returns>
		public Value GetGlobalValue(string varName) {
			// 如果虚拟机未创建,返回null
			if (vm == null) return null;
			
			// 获取全局上下文
			// Context包含变量表、代码、堆栈等
			TAC.Context c = vm.globalContext;
			if (c == null) return null;
			
			try {
				// 尝试获取变量
				// GetVar从上下文的变量表中查找变量
				return c.GetVar(varName);
				
			} catch (UndefinedIdentifierException) {
				// 如果变量未定义,捕获异常并返回null
				// 这提供了更友好的API,调用者不需要处理异常
				return null;
			}
		}
		
		/// <summary>
		/// Set a value in the global namespace of this interpreter.
		/// 
		/// 在此解释器的全局命名空间中设置一个值。
		/// 
		/// 【方法功能】:
		/// 从C#代码向MiniScript脚本设置全局变量。
		/// 
		/// 【使用场景】:
		/// - 向脚本传递参数
		/// - 暴露C#对象给脚本使用
		/// - 设置配置选项
		/// - 实现C#和MiniScript之间的双向通信
		/// 
		/// 【与GetGlobalValue的对应关系】:
		/// GetGlobalValue: 从脚本读取到C#
		/// SetGlobalValue: 从C#写入到脚本
		/// 
		/// 【注意事项】:
		/// 1. 如果变量已存在,会覆盖其值
		/// 2. 如果变量不存在,会创建新变量
		/// 3. 只在虚拟机已创建时才有效
		/// 
		/// 【示例用法】:
		/// // 向脚本传递玩家分数
		/// interpreter.SetGlobalValue("playerScore", new ValNumber(100));
		/// 
		/// // 向脚本传递字符串
		/// interpreter.SetGlobalValue("playerName", new ValString("Alice"));
		/// 
		/// // 向脚本传递自定义对象
		/// interpreter.SetGlobalValue("gameData", myGameDataValue);
		/// 
		/// 【Value类型创建】:
		/// 需要根据要传递的数据类型创建相应的Value子类:
		/// - ValNumber: 数字
		/// - ValString: 字符串
		/// - ValList: 列表
		/// - ValMap: 字典/映射
		/// - 等等
		/// </summary>
		/// <param name="varName">要设置的全局变量名</param>
		/// <param name="value">要设置的值</param>
		public void SetGlobalValue(string varName, Value value) {
			// 只在虚拟机存在时设置变量
			// 如果vm为null,静默失败(不抛出异常)
			if (vm != null) vm.globalContext.SetVar(varName, value);
		}
		
		
		/// <summary>
		/// Helper method that checks whether we have a new implicit result, and if
		/// so, invokes the implicitOutput callback (if any).  This is how you can
		/// see the result of an expression in a Read-Eval-Print Loop (REPL).
		/// 
		/// 辅助方法,检查是否有新的隐式结果,如果有,
		/// 调用implicitOutput回调(如果有的话)。这是你可以在
		/// 读取-求值-输出循环(REPL)中看到表达式结果的方式。
		/// 
		/// 【方法功能】:
		/// 在REPL模式下自动输出表达式的值。
		/// 
		/// 【隐式结果机制】:
		/// 在REPL中,不使用print的表达式会自动显示结果:
		/// 
		/// > 2 + 3
		/// 5
		/// 
		/// > "Hello" + " World"
		/// Hello World
		/// 
		/// > [1, 2, 3]
		/// [1, 2, 3]
		/// 
		/// 【实现原理】:
		/// 1. globalContext维护一个implicitResultCounter计数器
		/// 2. 每次产生隐式结果时,计数器递增
		/// 3. 结果存储在特殊变量ValVar.implicitResult中
		/// 4. CheckImplicitResult比较计数器,发现新结果时输出
		/// 
		/// 【参数说明】:
		/// previousImpResultCount: 执行前的隐式结果计数器值
		/// 如果当前计数器 > previousImpResultCount,说明产生了新结果
		/// 
		/// 【protected访问修饰符说明】:
		/// protected表示这个方法:
		/// - 在类内部可以访问
		/// - 在派生类中可以访问
		/// - 在类外部不可访问
		/// 
		/// 这是一个内部辅助方法,不打算让外部直接调用
		/// 但允许子类访问,以便自定义行为
		/// </summary>
		/// <param name="previousImpResultCount">之前的隐式结果计数器值</param>
		protected void CheckImplicitResult(int previousImpResultCount) {
			// 检查是否需要输出隐式结果:
			// 1. implicitOutput不为null(有输出委托)
			// 2. 计数器增加了(有新的隐式结果)
			if (implicitOutput != null && vm.globalContext.implicitResultCounter > previousImpResultCount) {

				// 获取隐式结果的值
				// ValVar.implicitResult是一个特殊的预定义变量
				// 它的identifier属性是变量名(通常是"_")
				Value result = vm.globalContext.GetVar(ValVar.implicitResult.identifier);
				
				// 如果结果不为null,输出它
				if (result != null) {
					// ToString(vm)将Value转换为字符串表示
					// 传入vm参数是因为某些值的字符串化可能需要访问虚拟机状态
					// true参数表示输出后添加换行符
					implicitOutput.Invoke(result.ToString(vm), true);
				}
			}			
		}
		
		/// <summary>
		/// Report a MiniScript error to the user.  The default implementation 
		/// simply invokes errorOutput with the error description.  If you want
		/// to do something different, then make an Interpreter subclass, and
		/// override this method.
		/// 
		/// 向用户报告MiniScript错误。默认实现只是用错误描述调用errorOutput。
		/// 如果你想做不同的事情,那么创建一个Interpreter子类,并重写此方法。
		/// 
		/// 【方法功能】:
		/// 处理和报告MiniScript执行过程中的错误。
		/// 
		/// 【virtual关键字说明】:
		/// virtual标记的方法可以在派生类中被override(重写)
		/// 这是实现多态的关键机制
		/// 
		/// 【默认行为】:
		/// 将错误描述作为字符串输出到errorOutput委托
		/// 
		/// 【自定义错误处理】:
		/// 如果需要自定义错误处理,可以:
		/// 
		/// class MyInterpreter : Interpreter {
		///     protected override void ReportError(MiniscriptException mse) {
		///         // 自定义处理:
		///         // - 记录到日志文件
		///         // - 显示GUI错误对话框
		///         // - 发送错误报告到服务器
		///         // - 触发调试器
		///         LogToFile(mse);
		///         ShowErrorDialog(mse);
		///         
		///         // 也可以调用基类实现
		///         base.ReportError(mse);
		///     }
		/// }
		/// 
		/// 【MiniscriptException说明】:
		/// MiniscriptException是MiniScript特定的异常类型
		/// 包含详细的错误信息:
		/// - 错误类型(编译错误、运行时错误等)
		/// - 错误位置(行号、列号)
		/// - 错误描述
		/// - 堆栈跟踪
		/// 
		/// Description()方法生成人类可读的错误描述字符串
		/// 
		/// 【为什么使用virtual而不是直接用委托】:
		/// 使用virtual方法的优势:
		/// 1. 可以访问完整的异常对象,而不只是字符串
		/// 2. 可以完全自定义错误处理逻辑
		/// 3. 可以在报告前修改或过滤错误
		/// 4. 可以添加额外的上下文信息
		/// </summary>
		/// <param name="mse">被抛出的异常对象</param>
		protected virtual void ReportError(MiniscriptException mse) {
			// 调用errorOutput委托,传入错误描述字符串
			// mse.Description()生成格式化的错误消息
			// true参数表示输出后添加换行符
			errorOutput.Invoke(mse.Description(), true);
		}
	}
}