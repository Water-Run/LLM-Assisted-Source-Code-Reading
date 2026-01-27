/*
═══════════════════════════════════════════════════════════════════════════
【文件概要】Program.cs - MiniScript解释器的主程序入口
═══════════════════════════════════════════════════════════════════════════

【基本信息】
- 项目名称: MiniScript
- 编程语言: C#
- 文件作用: 程序的主入口点,提供REPL交互环境、文件执行、测试套件等功能

【主要功能】
1. REPL模式 (Read-Eval-Print Loop): 交互式命令行环境
2. 文件执行模式: 读取并执行.ms脚本文件
3. 测试模式: 运行单元测试和集成测试
4. TAC转储: 可选的中间代码(Three Address Code)输出功能

【核心组件】
- MainClass: 主类,包含程序入口点Main方法
- Print: 统一的输出接口
- ListErrors: 错误信息展示
- Test: 单个测试用例执行
- RunTestSuite: 批量测试套件执行
- RunFile: 脚本文件执行
- Main: 程序入口,解析命令行参数并分发到相应功能

【使用方式】
- 无参数: 启动REPL交互模式
- --test: 运行单元测试和快速测试
- --test --integration [文件路径]: 运行集成测试
- <文件路径>: 执行指定的MiniScript脚本文件

【依赖命名空间】
- System: 基础类型和控制台IO
- System.IO: 文件读写操作
- System.Collections.Generic: 泛型集合(List等)
- Miniscript: MiniScript解释器核心库

═══════════════════════════════════════════════════════════════════════════
*/

using System;
using Miniscript;
using System.IO;
using System.Collections.Generic;

class MainClass
{

    /*
	───────────────────────────────────────────────────────────────────────
	【方法】Print - 统一的控制台输出方法
	───────────────────────────────────────────────────────────────────────

	【功能说明】
	提供一个统一的输出接口,封装Console的WriteLine和Write方法。
	这样设计的好处是:
	1. 统一管理输出行为
	2. 便于将来重定向输出(比如输出到文件或GUI)
	3. 简化调用代码

	【参数说明】
	@param s - 要输出的字符串内容
	@param lineBreak - 是否在输出后换行,默认为true
	                   true: 使用Console.WriteLine(换行)
	                   false: 使用Console.Write(不换行)

	【C#知识点】
	- 默认参数: lineBreak=true 表示如果调用时不提供该参数,默认值为true
	- static方法: 不需要创建类实例就可以直接调用

	【使用示例】
	Print("Hello");        // 输出"Hello"并换行
	Print("Hello", false); // 输出"Hello"不换行
	*/
    static void Print(string s, bool lineBreak = true)
    {
        if (lineBreak) Console.WriteLine(s);
        else Console.Write(s);
    }

    /*
	───────────────────────────────────────────────────────────────────────
	【方法】ListErrors - 列出脚本编译或运行时的所有错误
	───────────────────────────────────────────────────────────────────────

	【功能说明】
	遍历Script对象中的错误列表,格式化输出每个错误的详细信息。
	用于调试和问题定位。

	【参数说明】
	@param script - Script对象,包含了errors错误列表

	【输出格式】
	如果有错误: "{错误类型} on line {行号}: {错误描述}"
	如果没有错误: "No errors."

	【C#知识点】
	- foreach循环: 遍历集合中的每个元素
	  语法: foreach (元素类型 变量名 in 集合) { }
	- string.Format: 字符串格式化方法,{0}{1}{2}是占位符
	  类似于C语言的printf或Python的字符串格式化

	【实际应用】
	当脚本编译失败或运行出错时,调用此方法可以看到:
	- 错误发生在哪一行 (err.lineNum)
	- 错误类型 (err.type) 如: 语法错误、运行时错误等
	- 错误的详细描述 (err.description)
	*/
    static void ListErrors(Script script)
    {
        // 检查错误列表是否为空
        if (script.errors == null)
        {
            Print("No errors.");  // 没有错误信息
            return;
        }
        // 遍历所有错误并输出
        foreach (Error err in script.errors)
        {
            Print(string.Format("{0} on line {1}: {2}",
                err.type, err.lineNum, err.description));
        }

    }

    /*
	───────────────────────────────────────────────────────────────────────
	【方法】Test - 执行单个测试用例
	───────────────────────────────────────────────────────────────────────

	【功能说明】
	这是测试框架的核心方法:
	1. 创建一个MiniScript解释器实例
	2. 加载测试源代码
	3. 执行代码并捕获输出
	4. 将实际输出与期望输出进行逐行比对
	5. 报告任何不匹配的地方

	【参数说明】
	@param sourceLines - 测试源代码行列表(MiniScript代码)
	@param sourceLineNum - 源代码在测试文件中的起始行号,用于错误定位
	@param expectedOutput - 期望的输出结果列表,每个元素是一行输出
	@param outputLineNum - 期望输出在测试文件中的起始行号,用于错误定位

	【测试流程】
	1. 创建解释器并加载源代码
	2. 重定向所有输出到actualOutput列表(包括标准输出、错误输出、隐式输出)
	3. 运行脚本(最多60秒,不启用部分运行模式)
	4. 逐行比较实际输出和期望输出
	5. 报告差异:不匹配、缺失输出、多余输出

	【C#知识点 - 委托(Delegate)】
	这是本方法的重点知识:

	委托是什么?
	- 委托是一种类型,可以引用方法
	- 类似于C语言的函数指针,但类型安全
	- 允许将方法作为参数传递

	本方法中的委托使用:
	```
	miniscript.standardOutput = (string s, bool eol) => actualOutput.Add(s);
	```

	详细解释:
	- standardOutput是一个委托类型的字段/属性
	- (string s, bool eol) => ... 是Lambda表达式语法
	  * (string s, bool eol) 是参数列表
	  * => 是Lambda操作符,读作"goes to"
	  * actualOutput.Add(s) 是方法体

	等价的传统写法:
	```
	void MyOutputHandler(string s, bool eol) {
	    actualOutput.Add(s);
	}
	miniscript.standardOutput = MyOutputHandler;
	```

	为什么这样设计?
	- 解释器不知道输出应该去哪里(控制台?文件?GUI?)
	- 通过委托,我们可以自定义输出行为
	- 这里我们将输出捕获到List中,用于测试验证

	【输出重定向说明】
	- standardOutput: 标准输出(print语句的输出)
	- errorOutput: 错误输出(错误信息的输出)
	- implicitOutput: 隐式输出(表达式的结果自动输出)
	这三个都被重定向到同一个处理器,统一收集到actualOutput列表

	【测试验证逻辑】
	1. 找出两个列表中较短的长度(minLen)
	2. 在此范围内逐行比较,发现不匹配立即报告
	3. 如果期望输出更长,报告"缺失输出"
	4. 如果实际输出更长,报告"多余输出"
	*/
    static void Test(List<string> sourceLines, int sourceLineNum,
                     List<string> expectedOutput, int outputLineNum)
    {
        // 如果期望输出为null,初始化为空列表
        if (expectedOutput == null) expectedOutput = new List<string>();

        // 以下是被注释掉的调试代码,可以在需要时取消注释来查看测试详情
        //		Console.WriteLine("TEST (LINE {0}):", sourceLineNum);
        //		Console.WriteLine(string.Join("\n", sourceLines));
        //		Console.WriteLine("EXPECTING (LINE {0}):", outputLineNum);
        //		Console.WriteLine(string.Join("\n", expectedOutput));

        // 创建MiniScript解释器实例,传入源代码
        Interpreter miniscript = new Interpreter(sourceLines);

        // 创建一个列表来收集实际的输出结果
        List<string> actualOutput = new List<string>();

        // 【委托使用】将所有输出重定向到actualOutput列表
        // Lambda表达式: (参数) => 表达式
        // 这里捕获standardOutput的输出,添加到actualOutput列表
        miniscript.standardOutput = (string s, bool eol) => actualOutput.Add(s);

        // 错误输出也重定向到同一个处理器
        miniscript.errorOutput = miniscript.standardOutput;

        // 隐式输出(表达式结果)也重定向到同一个处理器
        miniscript.implicitOutput = miniscript.standardOutput;

        // 运行脚本直到完成
        // 参数1: 60 - 最大运行时间60秒
        // 参数2: false - 不启用部分运行模式(一次性运行完)
        miniscript.RunUntilDone(60, false);

        // 以下是被注释掉的调试代码
        //		Console.WriteLine("ACTUAL OUTPUT:");
        //		Console.WriteLine(string.Join("\n", actualOutput));

        // 找出两个列表中较短的长度,用于逐行比较
        // 三元运算符: 条件 ? 真值 : 假值
        int minLen = expectedOutput.Count < actualOutput.Count ? expectedOutput.Count : actualOutput.Count;

        // 逐行比较期望输出和实际输出
        for (int i = 0; i < minLen; i++)
        {
            if (actualOutput[i] != expectedOutput[i])
            {
                // 发现不匹配,输出详细的错误信息
                Print(string.Format("TEST FAILED AT LINE {0}\n  EXPECTED: {1}\n    ACTUAL: {2}",
                    outputLineNum + i, expectedOutput[i], actualOutput[i]));
            }
        }

        // 检查是否有缺失的输出(期望输出更长)
        if (expectedOutput.Count > actualOutput.Count)
        {
            Print(string.Format("TEST FAILED: MISSING OUTPUT AT LINE {0}", outputLineNum + actualOutput.Count));
            // 列出所有缺失的输出行
            for (int i = actualOutput.Count; i < expectedOutput.Count; i++)
            {
                Print("  MISSING: " + expectedOutput[i]);
            }
        }
        // 检查是否有多余的输出(实际输出更长)
        else if (actualOutput.Count > expectedOutput.Count)
        {
            Print(string.Format("TEST FAILED: EXTRA OUTPUT AT LINE {0}", outputLineNum + expectedOutput.Count));
            // 列出所有多余的输出行
            for (int i = expectedOutput.Count; i < actualOutput.Count; i++)
            {
                Print("  EXTRA: " + actualOutput[i]);
            }
        }

    }

    /*
	───────────────────────────────────────────────────────────────────────
	【方法】RunTestSuite - 运行完整的测试套件
	───────────────────────────────────────────────────────────────────────

	【功能说明】
	读取并执行一个测试套件文件,文件格式如下:
	```
	源代码行1
	源代码行2
	----              <-- 分隔符,之后是期望输出
	期望输出行1
	期望输出行2
	====              <-- 测试用例结束标记
	下一个测试的源代码...
	```

	【测试文件格式规范】
	- "====" 标记一个测试用例的结束,同时开始新的测试用例
	- "----" 分隔源代码和期望输出
	- 每个测试用例包含: 源代码部分 + 期望输出部分(可选)

	【参数说明】
	@param path - 测试套件文件的路径

	【处理流程】
	1. 打开并读取测试文件
	2. 逐行解析,识别测试用例的边界
	3. 收集每个测试用例的源代码和期望输出
	4. 遇到"===="时,执行收集到的测试用例
	5. 文件结束时,执行最后一个测试用例

	【状态变量说明】
	- sourceLines: 当前测试用例的源代码行集合
	- expectedOutput: 当前测试用例的期望输出行集合
	- testLineNum: 当前测试用例源代码的起始行号
	- outputLineNum: 当前测试用例期望输出的起始行号
	- lineNum: 文件中的当前行号

	【C#知识点 - StreamReader】
	StreamReader是用于读取文本文件的类:
	- new StreamReader(path): 打开文件
	- ReadLine(): 读取一行,文件结束时返回null
	- 需要手动关闭或使用using语句自动管理

	【C#知识点 - String方法】
	- StartsWith(string): 检查字符串是否以指定前缀开始
	  返回bool值,true表示匹配

	【状态机逻辑】
	本方法使用状态机模式来解析测试文件:
	- sourceLines == null: 等待新测试用例开始
	- sourceLines != null && expectedOutput == null: 收集源代码
	- expectedOutput != null: 收集期望输出
	- 遇到"====": 执行测试并重置状态
	*/
    static void RunTestSuite(string path)
    {
        // 创建文件读取器,打开测试套件文件
        StreamReader file = new StreamReader(path);

        // 检查文件是否成功打开
        if (file == null)
        {
            Print("Unable to read: " + path);  // 无法读取文件
            return;
        }

        // 状态变量:当前测试用例的源代码行
        List<string> sourceLines = null;

        // 状态变量:当前测试用例的期望输出行
        List<string> expectedOutput = null;

        // 记录测试用例源代码的起始行号
        int testLineNum = 0;

        // 记录期望输出的起始行号
        int outputLineNum = 0;

        // 读取第一行
        string line = file.ReadLine();
        int lineNum = 1;  // 行号从1开始

        // 循环读取文件直到结束(ReadLine返回null)
        while (line != null)
        {
            // 检查是否是测试用例结束标记
            if (line.StartsWith("===="))
            {
                // 如果有已收集的测试用例,执行它
                if (sourceLines != null) Test(sourceLines, testLineNum, expectedOutput, outputLineNum);

                // 重置状态,准备收集下一个测试用例
                sourceLines = null;
                expectedOutput = null;
            }
            // 检查是否是源代码与期望输出的分隔符
            else if (line.StartsWith("----"))
            {
                // 开始收集期望输出
                expectedOutput = new List<string>();
                outputLineNum = lineNum + 1;  // 期望输出从下一行开始
            }
            // 如果当前在收集期望输出阶段
            else if (expectedOutput != null)
            {
                expectedOutput.Add(line);  // 添加到期望输出列表
            }
            // 否则,收集源代码
            else
            {
                // 如果这是新测试用例的第一行源代码
                if (sourceLines == null)
                {
                    sourceLines = new List<string>();
                    testLineNum = lineNum;  // 记录源代码起始行号
                }
                sourceLines.Add(line);  // 添加到源代码列表
            }

            // 读取下一行
            line = file.ReadLine();
            lineNum++;
        }

        // 文件读取完毕,如果还有未执行的测试用例,执行它
        if (sourceLines != null) Test(sourceLines, testLineNum, expectedOutput, outputLineNum);

        // 输出测试完成信息
        Print("\nIntegration tests complete.\n");  // 集成测试完成
    }

    /*
	───────────────────────────────────────────────────────────────────────
	【方法】RunFile - 执行MiniScript脚本文件
	───────────────────────────────────────────────────────────────────────

	【功能说明】
	读取并执行一个.ms脚本文件,这是运行MiniScript程序的标准方式。

	【参数说明】
	@param path - 脚本文件的路径
	@param dumpTAC - 是否转储TAC(Three Address Code,三地址码)
	                 TAC是编译器中的中间表示形式
	                 用于调试和理解代码的编译结果

	【执行流程】
	1. 打开并读取整个脚本文件到内存
	2. 创建解释器实例
	3. 设置输出处理器(使用Console输出)
	4. 编译脚本
	5. (可选)转储TAC中间代码
	6. 循环运行直到脚本执行完成

	【C#知识点 - 文件读取】
	- StreamReader: 用于读取文本文件
	- EndOfStream: bool属性,表示是否到达文件末尾
	- ReadLine(): 读取一行文本

	【编译与执行分离】
	注意这里的两阶段处理:
	1. Compile(): 编译阶段,将源代码转换为虚拟机指令
	2. RunUntilDone(): 执行阶段,运行编译后的指令

	这种设计允许:
	- 在执行前检查语法错误
	- 多次执行同一段编译好的代码
	- 在执行前查看中间代码(TAC)

	【TAC (Three Address Code)】
	三地址码是编译器的中间表示:
	- 每条指令最多有三个操作数
	- 接近汇编语言,但比源代码更规范
	- 便于优化和调试
	例如: a = b + c * d
	可能编译为:
	  t1 = c * d
	  t2 = b + t1
	  a = t2

	【循环执行的必要性】
	while (!miniscript.done) 循环的原因:
	- 脚本可能需要等待用户输入
	- 脚本可能有异步操作
	- RunUntilDone()可能因为时间限制而暂停
	- 需要持续运行直到脚本真正完成
	*/
    static void RunFile(string path, bool dumpTAC = false)
    {
        // 创建文件读取器
        StreamReader file = new StreamReader(path);

        // 检查文件是否成功打开
        if (file == null)
        {
            Print("Unable to read: " + path);  // 无法读取文件
            return;
        }

        // 创建列表存储所有源代码行
        List<string> sourceLines = new List<string>();

        // 循环读取文件的每一行,直到文件末尾
        while (!file.EndOfStream) sourceLines.Add(file.ReadLine());

        // 创建解释器实例,传入源代码
        Interpreter miniscript = new Interpreter(sourceLines);

        // 设置标准输出处理器
        // 这里使用Lambda表达式,将输出重定向到我们的Print方法
        // (string s, bool eol) => Print(s, eol)
        // 意思是:接收字符串s和布尔值eol,然后调用Print(s, eol)
        miniscript.standardOutput = (string s, bool eol) => Print(s, eol);

        // 隐式输出使用相同的处理器
        miniscript.implicitOutput = miniscript.standardOutput;

        // 编译脚本(将源代码转换为虚拟机指令)
        miniscript.Compile();

        // 如果需要转储TAC并且虚拟机已创建
        if (dumpTAC && miniscript.vm != null)
        {
            // 转储顶层上下文的TAC代码
            // DumpTopContext(): 输出编译后的中间代码,用于调试
            miniscript.vm.DumpTopContext();
        }

        // 循环运行脚本直到完成
        // done是Interpreter的属性,表示脚本是否已完成执行
        while (!miniscript.done)
        {
            // RunUntilDone(): 运行脚本直到完成或需要暂停
            // 可能因为以下原因返回:
            // - 脚本执行完成
            // - 等待用户输入
            // - 达到执行时间限制
            miniscript.RunUntilDone();
        }

    }

    /*
	───────────────────────────────────────────────────────────────────────
	【方法】Main - 程序入口点
	───────────────────────────────────────────────────────────────────────

	【功能说明】
	这是整个程序的入口方法,C#程序从这里开始执行。
	根据命令行参数决定运行模式:
	1. 测试模式 (--test)
	2. 集成测试模式 (--test --integration)
	3. 文件执行模式 (提供文件路径)
	4. REPL交互模式 (无参数)

	【参数说明】
	@param args - 命令行参数数组
	               args[0]: 第一个参数
	               args[1]: 第二个参数
	               依此类推...
	               args.Length: 参数个数

	【C#知识点 - Main方法】
	- Main是程序的入口点,必须是static
	- 签名可以是:
	  * static void Main()
	  * static void Main(string[] args)
	  * static int Main() - 返回退出代码
	  * static int Main(string[] args)

	【命令行参数处理】
	args数组包含所有命令行参数(不包括程序名本身)
	例如运行: Program.exe --test --integration test.txt
	则: args[0] = "--test"
	    args[1] = "--integration"
	    args[2] = "test.txt"
	    args.Length = 3

	【运行模式详解】

	模式1: 测试模式 (--test)
	- 运行单元测试
	- 运行快速测试文件(QuickTest.ms)
	- 可选择转储TAC代码

	模式2: 集成测试模式 (--test --integration [文件])
	- 运行完整的集成测试套件
	- 默认文件: ../../../TestSuite.txt
	- 可指定自定义测试文件

	模式3: 文件执行模式 (提供文件路径)
	- 直接执行指定的.ms脚本文件

	模式4: REPL交互模式 (无参数)
	- 启动交互式命令行
	- 用户可以逐行输入和执行MiniScript代码
	- 类似Python的交互模式

	【C#知识点 - const】
	const定义编译时常量:
	- 值在编译时确定,运行时不可更改
	- 必须在声明时初始化
	- 只能用于基本类型和字符串

	【C#知识点 - var关键字】
	var是隐式类型声明:
	- 编译器根据初始化值推断类型
	- 必须在声明时初始化
	- 编译后与显式类型声明完全相同
	例如: var file = "test.txt" 等价于 string file = "test.txt"

	【C#知识点 - Stopwatch类】
	System.Diagnostics.Stopwatch用于高精度计时:
	- Start(): 开始计时
	- Stop(): 停止计时
	- Elapsed: TimeSpan类型,表示经过的时间
	- Elapsed.TotalSeconds: 总秒数(double类型)

	【C#知识点 - 字符串插值】
	$"Run time: {stopwatch.Elapsed.TotalSeconds} sec"
	这是C# 6.0引入的字符串插值语法:
	- $ 前缀表示这是插值字符串
	- {} 内可以包含表达式
	- 比string.Format更简洁易读
	等价于: string.Format("Run time: {0} sec", stopwatch.Elapsed.TotalSeconds)

	【REPL模式详解】
	REPL = Read-Eval-Print Loop (读取-求值-输出-循环)
	1. Read: 读取用户输入
	2. Eval: 执行/求值输入的代码
	3. Print: 输出结果
	4. Loop: 循环重复以上过程

	NeedMoreInput()的作用:
	- 检查输入是否完整
	- 如果代码不完整(如多行代码块),返回true
	- 显示不同的提示符:
	  * "> " - 准备接收新语句
	  * ">>> " - 等待继续输入(多行输入)

	【无限循环与退出】
	while (true) 创建无限循环,但可以通过以下方式退出:
	- Console.ReadLine()返回null(用户按Ctrl+Z或Ctrl+D)
	- 检测到null后执行break跳出循环
	- 程序结束
	*/
    public static void Main(string[] args)
    {

        // ═══════════════════════════════════════════════════════════════
        // 模式检测: 测试模式 (--test)
        // ═══════════════════════════════════════════════════════════════
        if (args.Length > 0 && args[0] == "--test")
        {
            // 设置测试工具的名称信息
            Miniscript.HostInfo.name = "Test harness";  // 测试工具

            // ───────────────────────────────────────────────────────────
            // 子模式: 集成测试 (--test --integration)
            // ───────────────────────────────────────────────────────────
            if (args.Length > 1 && args[1] == "--integration")
            {
                // 确定测试文件路径
                // 如果提供了第3个参数且非空,使用它;否则使用默认路径
                // ?? 是空合并运算符: 如果左边为null,使用右边的值
                var file = args.Length < 3 || string.IsNullOrEmpty(args[2]) ? "../../../TestSuite.txt" : args[2];

                Print("Running test suite.\n");  // 运行测试套件
                RunTestSuite(file);  // 执行集成测试
                return;  // 测试完成,退出程序
            }

            // ───────────────────────────────────────────────────────────
            // 标准测试模式: 运行单元测试和快速测试
            // ───────────────────────────────────────────────────────────
            Print("Miniscript test harness.\n");  // MiniScript测试工具

            Print("Running unit tests.\n");  // 运行单元测试
            UnitTest.Run();  // 调用单元测试框架

            Print("\n");  // 空行分隔

            // 定义快速测试文件的路径(const编译时常量)
            const string quickTestFilePath = "../../../QuickTest.ms";

            // 检查快速测试文件是否存在
            if (File.Exists(quickTestFilePath))
            {
                Print("Running quick test.\n");  // 运行快速测试

                // 创建高精度计时器
                var stopwatch = new System.Diagnostics.Stopwatch();
                stopwatch.Start();  // 开始计时

                // 运行快速测试文件,并转储TAC代码(true参数)
                RunFile(quickTestFilePath, true);

                stopwatch.Stop();  // 停止计时

                // 使用字符串插值($"...")输出运行时间
                Print($"Run time: {stopwatch.Elapsed.TotalSeconds} sec");  // 运行时间: X 秒
            }
            else
            {
                Print("Quick test not found, skipping...\n");  // 未找到快速测试,跳过...
            }
            return;  // 测试完成,退出程序
        }

        // ═══════════════════════════════════════════════════════════════
        // 模式检测: 文件执行模式 (提供了文件路径参数)
        // ═══════════════════════════════════════════════════════════════
        if (args.Length > 0)
        {
            RunFile(args[0]);  // 执行指定的脚本文件
            return;  // 执行完成,退出程序
        }

        // ═══════════════════════════════════════════════════════════════
        // 默认模式: REPL交互模式 (没有提供任何参数)
        // ═══════════════════════════════════════════════════════════════

        // 创建REPL解释器实例(无参数构造函数,不加载任何源代码)
        Interpreter repl = new Interpreter();

        // 设置隐式输出,使表达式结果自动显示
        repl.implicitOutput = repl.standardOutput;

        // 进入无限循环,等待用户输入
        while (true)
        {
            // 显示提示符
            // NeedMoreInput(): 检查是否需要更多输入(多行代码块未完成)
            // 如果需要更多输入,显示 ">>> ",否则显示 "> "
            Console.Write(repl.NeedMoreInput() ? ">>> " : "> ");

            // 读取用户输入的一行
            string inp = Console.ReadLine();

            // 如果ReadLine返回null(用户按Ctrl+Z/Ctrl+D),退出循环
            if (inp == null) break;

            // 将用户输入传递给REPL解释器处理
            // REPL方法会:
            // 1. 累积多行输入(如果需要)
            // 2. 编译并执行完整的语句
            // 3. 输出结果
            // 4. 处理错误
            repl.REPL(inp);
        }
    }
}

/*
═══════════════════════════════════════════════════════════════════════════
【文件总结】Program.cs 完整分析
═══════════════════════════════════════════════════════════════════════════

【架构设计模式】
1. 命令模式 (Command Pattern)
   - 不同的命令行参数触发不同的操作
   - Main方法作为命令分发器

2. 策略模式 (Strategy Pattern)
   - 通过委托实现输出策略的灵活切换
   - 测试时捕获输出,运行时输出到控制台

3. 模板方法模式 (Template Method Pattern)
   - Test方法定义了测试执行的标准流程
   - RunTestSuite按照固定流程解析和执行测试

【核心技术要点】

1. 委托 (Delegates) 的深入理解
   委托是C#的一等公民,本文件展示了委托的重要应用:

   a) 输出重定向
   ```
   miniscript.standardOutput = (string s, bool eol) => actualOutput.Add(s);
   ```
   - 通过委托,解释器不依赖具体的输出实现
   - 测试时可以捕获输出进行验证
   - 生产环境可以输出到控制台
   - 这是依赖注入(Dependency Injection)的体现

   b) Lambda表达式语法
   - (参数列表) => 表达式/语句块
   - 简洁表达匿名方法
   - 编译器自动推断参数类型

   c) 委托链
   ```
   miniscript.errorOutput = miniscript.standardOutput;
   ```
   - 多个委托可以指向同一个处理器
   - 实现统一的输出处理

2. 文件I/O操作
   - StreamReader: 文本文件读取
   - 逐行读取 vs 一次性读取
   - 文件存在性检查: File.Exists()
   - 资源管理考虑(虽然未使用using,但应该注意)

3. 字符串处理技巧
   - StartsWith(): 前缀匹配
   - string.Format(): 格式化输出
   - 字符串插值: $"text {expression}"
   - string.Join(): 连接集合元素

4. 集合操作
   - List<T>: 泛型列表
   - Add(): 添加元素
   - Count: 元素数量
   - 索引访问: list[i]
   - foreach遍历

5. 条件与控制流
   - 多重if-else分支
   - 三元运算符: condition ? true_value : false_value
   - while循环
   - for循环
   - break退出循环

【测试框架设计分析】

测试文件格式设计精妙:
```
源代码
源代码
----
期望输出
期望输出
====
```

优点:
- 简单直观,易于编写
- 文本格式,易于版本控制
- 源码和期望结果紧密关联
- 支持多个测试用例

测试验证策略:
1. 逐行比较(精确匹配)
2. 区分三种失败情况:
   - 内容不匹配
   - 缺失输出
   - 多余输出
3. 详细的错误报告,包含行号

【REPL设计分析】

REPL是解释型语言的标准特性:
1. 即时反馈,适合学习和实验
2. 多行输入支持(NeedMoreInput)
3. 表达式自动求值输出(implicitOutput)
4. 错误处理(继续运行,不崩溃)

提示符设计:
- "> ": 准备接收新语句
- ">>> ": 等待多行输入继续

这是Python REPL的经典设计,用户体验友好。

【编译与执行分离】

本程序展示了两阶段执行模式:
1. Compile(): 编译阶段
   - 词法分析
   - 语法分析
   - 生成中间代码(TAC)

2. RunUntilDone(): 执行阶段
   - 虚拟机执行指令
   - 可以暂停和恢复
   - 支持异步操作

优点:
- 语法检查前置
- 可多次执行同一代码
- 便于调试(查看中间代码)
- 性能优化机会

【程序入口设计】

Main方法的分支清晰:
1. 测试分支 (--test)
   - 单元测试
   - 集成测试
   - 快速测试

2. 文件执行分支
   - 直接运行脚本

3. 交互分支 (默认)
   - REPL模式

这种设计支持多种使用场景:
- 开发者: 运行测试
- 用户: 执行脚本或交互使用
- CI/CD: 自动化测试

【C#高级特性总结】

本文件涉及的C#特性:
1. ★★★ 委托和Lambda表达式 - 核心知识
2. ★★☆ 泛型集合 (List<T>)
3. ★★☆ 字符串插值 ($"...")
4. ★☆☆ 默认参数
5. ★☆☆ var类型推断
6. ★☆☆ 三元运算符
7. ★☆☆ 空合并运算符 (??)

【性能考虑】

1. 文件读取: 逐行读取避免大文件内存问题
2. 字符串操作: 使用StringBuilder可能更优(未应用)
3. 测试输出: 使用List收集,内存效率高
4. 计时精度: Stopwatch提供高精度计时

【可改进之处】

1. 资源管理
   ```
   using (StreamReader file = new StreamReader(path)) {
       // 自动释放资源
   }
   ```

2. 异常处理
   - 当前缺少try-catch
   - 文件不存在、权限问题等未处理

3. 参数验证
   - 更严格的参数检查
   - 提供帮助信息 (--help)

4. 输出缓冲
   - 大量输出时考虑缓冲

5. 国际化
   - 硬编码的英文消息
   - 可以使用资源文件

【学习建议】

1. 理解委托的本质
   - 函数指针的类型安全版本
   - 实现回调和事件机制的基础
   - 练习: 自己实现一个简单的委托示例

2. 掌握Lambda表达式
   - 简化委托的语法糖
   - 闭包概念(捕获外部变量)
   - 练习: 使用LINQ with Lambda

3. 熟悉文件I/O
   - StreamReader/StreamWriter
   - using语句
   - 异步I/O (async/await)

4. 测试驱动开发(TDD)
   - 本文件的测试框架设计值得学习
   - 编写可测试的代码
   - 自动化测试的重要性

【实践练习建议】

1. 添加命令行帮助信息
2. 实现using语句管理文件资源
3. 添加异常处理
4. 扩展测试框架支持更多断言类型
5. 实现彩色输出(测试通过/失败)
6. 添加性能测试和基准测试
7. 实现测试结果的HTML报告生成

【与其他语言对比】

1. Python
   - Python也有REPL
   - lambda语法更简洁
   - 但C#的委托更强大(多播等)

2. JavaScript
   - 函数是一等公民,类似委托
   - 箭头函数类似Lambda
   - Node.js REPL类似

3. Java
   - Java 8+有Lambda表达式
   - 函数式接口类似委托
   - JShell REPL (Java 9+)

【总结陈词】

Program.cs是MiniScript项目的"指挥中心":
- 协调各个模块的工作
- 提供灵活的使用方式
- 展示了良好的软件设计
- 是学习C#委托、Lambda、文件I/O的绝佳示例

通过深入理解这个文件:
- 掌握了C#的核心特性
- 理解了解释器的基本架构
- 学习了测试框架的设计
- 体会了软件工程的最佳实践

继续深入学习MiniScript的其他模块,将会对脚本语言的
实现有更全面的认识!

═══════════════════════════════════════════════════════════════════════════
*/
