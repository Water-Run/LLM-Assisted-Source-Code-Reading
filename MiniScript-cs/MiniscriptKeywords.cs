/*	MiniscriptKeywords.cs
 * 
 * ============================================================================
 * 文件概要:
 * ============================================================================
 * 
 * 【文件名称】MiniscriptKeywords.cs
 * 
 * 【文件功能】
 * 这个文件定义了一个小型的 Keywords 类,包含了所有 MiniScript 语言的保留字
 * (关键字),例如: break, for, if, while 等。
 * 
 * 【主要用途】
 * 1. 语法高亮/着色 - 在代码编辑器中为关键字添加不同颜色
 * 2. 标识符验证 - 确保用户输入的变量名或函数名不会与保留字冲突
 * 3. 词法分析 - 在解析 MiniScript 代码时识别关键字
 * 
 * 【设计模式】
 * 使用静态类(static class)设计,这意味着:
 * - 不能被实例化(不能用 new Keywords() 创建对象)
 * - 所有成员都必须是静态的
 * - 可以直接通过类名访问,如: Keywords.IsKeyword("if")
 * 
 * 【包含的关键字类型】
 * - 控制流程: break, continue, if, else, for, while, repeat, return
 * - 函数相关: function, end
 * - 逻辑运算: and, or, not
 * - 值类型: true, false, null
 * - 其他: in, isa, new, then
 * 
 * ============================================================================
 */

using System;  // 引入 System 命名空间,提供基础类型和 Array 类等基本功能

namespace Miniscript {  // 定义 Miniscript 命名空间,用于组织代码并避免命名冲突
	
	/// <summary>
	/// Keywords 静态类 - MiniScript 语言的关键字管理类
	/// 
	/// 【C# 高级特性说明 - static class(静态类)】
	/// static class 是 C# 的一个特殊类型:
	/// 1. 不能被实例化 - 不能使用 new 创建对象
	/// 2. 只能包含静态成员 - 所有字段、属性、方法都必须是 static
	/// 3. 自动密封(sealed) - 不能被继承
	/// 4. 用途: 提供工具方法或常量的集合,不需要维护状态
	/// 
	/// 【为什么这里使用静态类】
	/// 因为关键字列表是全局唯一的,不需要为每个使用者创建单独的实例
	/// </summary>
	public static class Keywords {
		
		/// <summary>
		/// all 字段 - 存储所有 MiniScript 关键字的字符串数组
		/// 
		/// 【字段说明】
		/// - public: 公开访问,外部可以直接读取这个数组
		/// - static: 静态成员,属于类本身而非实例
		/// - string[]: 字符串数组类型
		/// 
		/// 【数组初始化器】
		/// 使用 { } 语法直接初始化数组,这是 C# 的集合初始化器(Collection Initializer)
		/// 编译器会自动计算数组大小(这里是 20 个元素)
		/// 
		/// 【关键字分类说明】
		/// 
		/// 1. 循环控制关键字:
		///    - break: 跳出当前循环
		///    - continue: 跳过本次循环,继续下一次
		///    - for: for 循环语句
		///    - while: while 循环语句
		///    - repeat: repeat 循环语句(类似 do-while)
		///    - in: 用于 for 循环遍历集合
		/// 
		/// 2. 条件判断关键字:
		///    - if: 条件判断语句
		///    - else: if 的否定分支
		///    - then: if 语句的关键字(某些语言需要)
		/// 
		/// 3. 函数相关关键字:
		///    - function: 定义函数
		///    - return: 从函数返回值
		///    - end: 结束代码块(函数、循环、条件等)
		/// 
		/// 4. 逻辑运算关键字:
		///    - and: 逻辑与运算
		///    - or: 逻辑或运算
		///    - not: 逻辑非运算
		/// 
		/// 5. 对象/类型关键字:
		///    - new: 创建新对象
		///    - isa: 类型检查/继承检查
		/// 
		/// 6. 字面值关键字:
		///    - true: 布尔真值
		///    - false: 布尔假值
		///    - null: 空值/无值
		/// </summary>
		public static string[] all = {
			"break",      // 中断循环,跳出当前 for/while/repeat 循环
			"continue",   // 继续循环,跳过当前迭代,进入下一次循环
			"else",       // 条件语句的 else 分支
			"end",        // 结束代码块(函数/循环/条件语句的结束标记)
			"for",        // for 循环关键字
			"function",   // 函数定义关键字
			"if",         // 条件判断关键字
			"in",         // 用于 for 循环的遍历操作,如: for x in list
			"isa",        // 类型检查运算符,检查对象是否是某个类型的实例
			"new",        // 创建新对象实例
			"null",       // 空值字面量
			"then",       // if 语句的 then 关键字(用于单行 if 或某些语法风格)
			"repeat",     // repeat 循环关键字(类似其他语言的 do-while)
			"return",     // 函数返回关键字
			"while",      // while 循环关键字
			"and",        // 逻辑与运算符
			"or",         // 逻辑或运算符
			"not",        // 逻辑非运算符
			"true",       // 布尔真值字面量
			"false"       // 布尔假值字面量
		};

		/// <summary>
		/// IsKeyword 方法 - 检查给定的文本是否是 MiniScript 关键字
		/// 
		/// 【方法签名说明】
		/// - public: 公开方法,可以被外部调用
		/// - static: 静态方法,通过类名调用,如: Keywords.IsKeyword("if")
		/// - bool: 返回值类型,true 表示是关键字,false 表示不是
		/// - IsKeyword: 方法名,遵循 C# 命名规范(PascalCase - 帕斯卡命名法)
		/// - string text: 参数,要检查的文本字符串
		/// 
		/// 【实现原理】
		/// 使用 Array.IndexOf() 方法在 all 数组中查找指定文本
		///  
		/// 【C# 高级特性说明 - Array.IndexOf()】
		/// Array.IndexOf() 是 System.Array 类的静态方法
		/// 功能: 在一维数组中搜索指定对象,返回第一次出现的索引
		/// 
		/// 参数:
		/// - all: 要搜索的数组
		/// - text: 要查找的值
		/// 
		/// 返回值:
		/// - 如果找到: 返回元素在数组中的索引位置(0 到 数组长度-1)
		/// - 如果未找到: 返回 -1
		/// 
		/// 时间复杂度: O(n),需要线性搜索整个数组
		/// 
		/// 【表达式说明】
		/// >= 0: 判断返回值是否大于等于 0
		/// - 如果 >= 0: 说明找到了,text 是关键字,返回 true
		/// - 如果 < 0 (即 -1): 说明没找到,text 不是关键字,返回 false
		/// 
		/// 【使用示例】
		/// Keywords.IsKeyword("if")      // 返回 true
		/// Keywords.IsKeyword("myVar")   // 返回 false
		/// Keywords.IsKeyword("function") // 返回 true
		/// 
		/// 【性能考虑】
		/// 对于 20 个关键字的小数组,线性搜索的性能完全可以接受
		/// 如果关键字数量很大,可以考虑使用 HashSet<string> 来优化查找性能到 O(1)
		/// </summary>
		/// <param name="text">要检查的文本字符串</param>
		/// <returns>如果 text 是关键字返回 true,否则返回 false</returns>
		public static bool IsKeyword(string text) {
			// Array.IndexOf(all, text):
			// 在 all 数组中查找 text,返回索引位置
			// 找到: 返回索引(0-19)
			// 未找到: 返回 -1
			//
			// >= 0:
			// 判断返回值,如果 >= 0 说明找到了,返回 true
			// 否则返回 false
			return Array.IndexOf(all, text) >= 0;
		}
		
		// 【可能的扩展方法建议】
		// 如果需要更多功能,可以添加以下方法:
		//
		// 1. 获取关键字数量:
		// public static int Count => all.Length;
		//
		// 2. 判断是否是控制流关键字:
		// public static bool IsControlFlowKeyword(string text) {
		//     return text == "if" || text == "else" || text == "for" || 
		//            text == "while" || text == "break" || text == "continue";
		// }
		//
		// 3. 判断是否是逻辑运算符:
		// public static bool IsLogicalOperator(string text) {
		//     return text == "and" || text == "or" || text == "not";
		// }

	}
}

/*
 * ============================================================================
 * 详细总结说明
 * ============================================================================
 * 
 * 【文件整体架构】
 * 1. 命名空间: Miniscript - 组织代码,避免命名冲突
 * 2. 类: Keywords - 静态工具类,提供关键字相关功能
 * 3. 成员:
 *    - all 字段: 存储所有关键字的字符串数组
 *    - IsKeyword 方法: 检查文本是否为关键字
 * 
 * 
 * 【涉及的 C# 高级特性详解】
 * 
 * 1. 静态类 (static class)
 *    -------------------------
 *    定义: public static class Keywords
 *    
 *    特点:
 *    - 不能实例化(不能 new)
 *    - 只能包含静态成员
 *    - 自动密封,不能被继承
 *    - 编译时检查,确保所有成员都是静态的
 *    
 *    使用场景:
 *    - 工具类(如 Math, Console)
 *    - 扩展方法容器
 *    - 常量集合
 *    - 不需要维护状态的功能集合
 *    
 *    访问方式:
 *    Keywords.all           // 直接通过类名访问
 *    Keywords.IsKeyword()   // 直接通过类名调用
 * 
 * 
 * 2. 数组 (Array)
 *    -------------------------
 *    定义: public static string[] all = { ... };
 *    
 *    这是数组的集合初始化器语法:
 *    - 编译器自动计算数组大小
 *    - 简洁,可读性好
 *    - 等价于:
 *      string[] all = new string[20];
 *      all[0] = "break";
 *      all[1] = "continue";
 *      ...
 *    
 *    数组特点:
 *    - 固定大小,创建后不能改变长度
 *    - 连续内存存储,访问速度快 O(1)
 *    - 类型安全,只能存储 string 类型
 * 
 * 
 * 3. Array.IndexOf() 方法
 *    -------------------------
 *    完整签名: public static int IndexOf(Array array, object value)
 *    
 *    这是 System.Array 类的静态方法,用于在数组中查找元素
 *    
 *    参数:
 *    - array: 要搜索的一维数组
 *    - value: 要查找的对象
 *    
 *    返回值:
 *    - 找到: 返回第一次出现的索引(0 到 Length-1)
 *    - 未找到: 返回 -1
 *    
 *    内部实现:
 *    - 从索引 0 开始,逐个比较元素
 *    - 使用 Object.Equals() 进行比较
 *    - 对于字符串,进行值比较(不是引用比较)
 *    
 *    时间复杂度: O(n) - 线性搜索
 *    
 *    替代方案(如果性能关键):
 *    HashSet<string> keywords = new HashSet<string>(all);
 *    bool isKeyword = keywords.Contains(text);  // O(1) 查找
 * 
 * 
 * 【设计模式分析】
 * 
 * 1. 单例模式的变体
 *    - 虽然不是严格的单例模式,但静态类确保了全局唯一性
 *    - 关键字列表在整个应用程序中只有一份
 * 
 * 2. 工具类模式 (Utility Class Pattern)
 *    - 提供静态方法和常量
 *    - 不维护状态
 *    - 纯功能性,无副作用
 * 
 * 
 * 【性能分析】
 * 
 * 内存占用:
 * - string[] all: 约 20 个字符串引用 + 字符串实际内容
 * - 总计约几百字节,可忽略不计
 * 
 * 查找性能:
 * - IsKeyword() 使用线性搜索: O(n)
 * - 对于 20 个关键字,平均比较 10 次
 * - 实际性能: 微秒级,完全可以接受
 * 
 * 优化建议:
 * - 如果关键字数量增加到 100+ 个,考虑使用 HashSet
 * - 如果频繁调用,可以考虑缓存结果
 * 
 * 
 * 【使用场景示例】
 * 
 * 1. 词法分析器中:
 *    if (Keywords.IsKeyword(token)) {
 *        // 这是一个关键字,特殊处理
 *    }
 * 
 * 2. 语法高亮:
 *    foreach (var word in codeWords) {
 *        if (Keywords.IsKeyword(word)) {
 *            HighlightAsKeyword(word);
 *        }
 *    }
 * 
 * 3. 变量名验证:
 *    if (Keywords.IsKeyword(variableName)) {
 *        throw new Exception("不能使用关键字作为变量名");
 *    }
 * 
 * 4. 遍历所有关键字:
 *    foreach (string keyword in Keywords.all) {
 *        Console.WriteLine(keyword);
 *    }
 * 
 * 
 * 【潜在的改进方向】
 * 
 * 1. 使用 HashSet 提升查找性能:
 *    private static readonly HashSet<string> keywordSet = 
 *        new HashSet<string>(all);
 *    
 *    public static bool IsKeyword(string text) {
 *        return keywordSet.Contains(text);
 *    }
 * 
 * 2. 添加只读保护:
 *    public static readonly string[] all = { ... };
 *    // 防止外部修改数组内容
 * 
 * 3. 关键字分类:
 *    public static readonly string[] ControlFlowKeywords = 
 *        { "if", "else", "for", "while", ... };
 *    public static readonly string[] LogicalOperators = 
 *        { "and", "or", "not" };
 * 
 * 4. 支持大小写不敏感:
 *    public static bool IsKeyword(string text, 
 *        bool ignoreCase = false) {
 *        if (ignoreCase) {
 *            return Array.FindIndex(all, 
 *                k => k.Equals(text, 
 *                    StringComparison.OrdinalIgnoreCase)) >= 0;
 *        }
 *        return Array.IndexOf(all, text) >= 0;
 *    }
 * 
 * 
 * 【C# 语言特性总结】
 * 
 * 本文件虽然简单,但体现了多个 C# 核心概念:
 * 
 * 1. 命名空间 (namespace) - 代码组织
 * 2. 静态类 (static class) - 工具类设计
 * 3. 数组 (Array) - 数据存储
 * 4. 集合初始化器 - 简洁的数组初始化
 * 5. 静态方法 - 无需实例的功能调用
 * 6. Array 类的静态方法 - .NET 框架库的使用
 * 7. 访问修饰符 (public, static) - 封装和访问控制
 * 
 * 
 * 【学习要点】
 * 
 * 1. 理解静态类的使用场景和限制
 * 2. 掌握数组的基本操作
 * 3. 了解 Array.IndexOf() 的工作原理
 * 4. 认识简洁代码设计的价值(KISS 原则 - Keep It Simple, Stupid)
 * 5. 理解关键字在编程语言中的作用
 * 
 * 
 * 【与其他语言的对比】
 * 
 * Java:
 * - 使用 final class + private constructor 模拟静态类
 * - 使用 Arrays.asList().contains() 进行查找
 * 
 * Python:
 * - 使用模块级常量和函数
 * - keywords = ["break", "continue", ...]
 * - def is_keyword(text): return text in keywords
 * 
 * JavaScript:
 * - const keywords = ["break", "continue", ...];
 * - const isKeyword = text => keywords.includes(text);
 * 
 * 
 * 【最佳实践建议】
 * 
 * 1. 对于小型常量集合(< 50 个),数组 + IndexOf 足够
 * 2. 对于大型集合或频繁查询,使用 HashSet
 * 3. 考虑使用 readonly 保护数据不被修改
 * 4. 为关键字分类,便于特定场景使用
 * 5. 添加单元测试,确保关键字列表正确
 * 
 * ============================================================================
 */