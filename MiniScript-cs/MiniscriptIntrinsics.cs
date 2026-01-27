/*	MiniscriptIntrinsics.cs
 * 
 * 此文件定义了Intrinsic类，该类表示MiniScript代码可用的内置函数。
 * 所有内置函数都保存在静态存储中，因此该类包含静态函数（如GetByName）
 * 用于查找已定义的内置函数。有关添加自定义内置函数的详细信息，
 * 请参见MiniScript集成指南第2章。
 *
 * 此文件还包含Intrinsics静态类，其中定义了所有标准内置函数。
 * 这会自动初始化，因此通常您无需担心它，不过它是了解如何编写
 * 内置函数的好地方。
 *
 * 注意：您应该将添加的任何内置函数放在单独的文件中；保持MiniScript
 * 源文件不变，以便在更新可用时轻松替换它们。
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Globalization;

namespace Miniscript {
	/// <summary>
	/// IntrinsicCode是一个委托，指向内置方法调用的实际C#代码。
	/// 
	/// C#高级特性说明：
	/// - 委托(Delegate): 类似于C/C++中的函数指针，但类型安全
	/// - 委托可以引用静态方法或实例方法
	/// - 委托支持多播（可以包含多个方法的调用列表）
	/// - 这里的委托用于将MiniScript函数调用路由到对应的C#实现
	/// </summary>
	/// <param name="context">调用内置函数时的TAC.Context上下文</param>
	/// <param name="partialResult">如果有的话，来自之前调用的部分结果（用于支持异步操作）</param>
	/// <returns>计算结果：是否完成、如果未完成则返回部分结果、如果完成则返回Value</returns>
	public delegate Intrinsic.Result IntrinsicCode(TAC.Context context, Intrinsic.Result partialResult);
	
	/// <summary>
	/// 关于托管MiniScript的应用程序的信息。在主程序中设置这些值。
	/// 这些信息通过`version`内置函数提供给用户。
	/// </summary>
	public static class HostInfo {
		public static string name;		// 宿主程序的名称
		public static string info;		// 关于宿主的URL或其他简短信息
		public static double version;	// 宿主程序版本号
	}
		
	/// <summary>
	/// Intrinsic: 表示MiniScript代码可用的内置函数。
	/// 
	/// 设计模式说明：
	/// - 工厂模式：通过Create静态方法创建实例
	/// - 单例模式：所有实例存储在静态列表中，确保每个内置函数只有一个实例
	/// - 注册表模式：通过nameMap提供快速按名称查找
	/// </summary>
	public class Intrinsic {
		// 此内置函数的名称（应该是有效的MiniScript标识符）
		public string name;
		
		// 内置函数调用的实际C#代码（委托类型，指向具体实现）
		public IntrinsicCode code;
		
		// 数字ID（内部使用 -- 无需担心这个）
		// C#属性(Property)说明：
		// - 属性是字段的封装，提供get/set访问器
		// - 这里只提供get访问器，使其成为只读属性
		// - 属性可以包含逻辑，不仅仅是简单返回字段值
		public int id { get { return numericID; } }

		// 从Value到短名称的静态映射，用于显示列表/映射时使用
		// 可以随意向其中添加您自己的内置函数提供的任何值（尤其是列表/映射）
		// 
		// C#泛型(Generics)说明：
		// - Dictionary<TKey, TValue>是泛型集合类
		// - <Value, string>指定键类型为Value，值类型为string
		// - 泛型提供编译时类型安全和运行时性能优势
		public static Dictionary<Value, string> shortNames = new Dictionary<Value, string>();

		private Function function;			// 内部Function对象，包含参数等信息
		private ValFunction valFunction;	// function的缓存包装器（用于脚本调用）
		int numericID;		// 数字ID，也是它在'all'列表中的索引

		// 所有内置函数的静态列表
		// 索引0为null，实际内置函数从索引1开始
		public static List<Intrinsic> all = new List<Intrinsic>() { null };
		
		// 名称到内置函数的映射，用于快速查找
		static Dictionary<string, Intrinsic> nameMap = new Dictionary<string, Intrinsic>();
				
		/// <summary>
		/// 工厂方法，用于创建新的Intrinsic，填充给定的名称和
		/// 其他所需的内部属性。您仍需要添加任何参数，
		/// 并定义它运行的代码。
		/// 
		/// 设计说明：
		/// - 使用工厂方法而不是公共构造函数，以确保所有实例都被正确注册
		/// - 自动分配唯一ID并添加到全局列表
		/// - 创建内部Function对象用于参数管理
		/// </summary>
		/// <param name="name">内置函数名称</param>
		/// <returns>新创建的（但空的）静态Intrinsic</returns>
		public static Intrinsic Create(string name) {
			Intrinsic result = new Intrinsic();
			result.name = name;
			result.numericID = all.Count;	// 使用当前列表大小作为ID
			result.function = new Function(null);	// 创建Function对象，参数为null表示内置函数
			result.valFunction = new ValFunction(result.function);	// 包装为ValFunction供脚本使用
			all.Add(result);				// 添加到全局列表
			nameMap[name] = result;			// 添加到名称映射
			return result;
		}
		
		/// <summary>
		/// 通过内部数字ID查找Intrinsic。
		/// 
		/// 使用场景：
		/// - 在编译后的代码中，使用数字ID比名称更高效
		/// - ID是列表索引，查找速度为O(1)
		/// </summary>
		public static Intrinsic GetByID(int id) {
			return all[id];
		}
		
		/// <summary>
		/// 通过名称查找Intrinsic。
		/// 
		/// 使用场景：
		/// - 编译时，将函数名解析为Intrinsic对象
		/// - 动态调用时查找函数
		/// </summary>
		public static Intrinsic GetByName(string name) {
			Intrinsics.InitIfNeeded();	// 确保所有标准内置函数已初始化
			Intrinsic result = null;
			if (nameMap.TryGetValue(name, out result)) return result;	// 使用TryGetValue避免异常
			return null;
		}
		
		/// <summary>
		/// 向此Intrinsic添加参数，可选地提供默认值
		/// （如果用户不提供参数值时使用）。必须按照
		/// 提供参数的顺序添加参数。
		/// 
		/// 参数处理说明：
		/// - 参数存储在function.parameters列表中
		/// - Function.Param包含参数名和默认值
		/// - 默认值为null表示必需参数
		/// </summary>
		/// <param name="name">参数名称</param>
		/// <param name="defaultValue">默认值（如果有）</param>
		public void AddParam(string name, Value defaultValue=null) {
			function.parameters.Add(new Function.Param(name, defaultValue));
		}
		
		/// <summary>
		/// 添加具有数字默认值的参数。
		/// （请参阅上面第一个版本的AddParam的注释。）
		/// 
		/// 优化说明：
		/// - 对于常见数值0和1，使用预定义的静态实例
		/// - 避免为相同值创建多个ValNumber对象
		/// - TAC.Num是工厂方法，内部也做了类似优化
		/// </summary>
		/// <param name="name">参数名称</param>
		/// <param name="defaultValue">此参数的默认值</param>
		public void AddParam(string name, double defaultValue) {
			Value defVal;
			if (defaultValue == 0) defVal = ValNumber.zero;		// 使用预定义的零
			else if (defaultValue == 1) defVal = ValNumber.one;	// 使用预定义的一
			else defVal = TAC.Num(defaultValue);				// 创建新的数字值
			function.parameters.Add(new Function.Param(name, defVal));
		}

		/// <summary>
		/// 添加具有字符串默认值的参数。
		/// （请参阅上面第一个版本的AddParam的注释。）
		/// 
		/// 优化说明：
		/// - 空字符串和特殊字符串使用预定义实例
		/// - "__isa"是特殊的魔术字符串，用于类型系统
		/// - "self"是方法调用的隐式第一个参数
		/// </summary>
		/// <param name="name">参数名称</param>
		/// <param name="defaultValue">此参数的默认值</param>
		public void AddParam(string name, string defaultValue) {
			Value defVal;
			if (string.IsNullOrEmpty(defaultValue)) defVal = ValString.empty;		// 空字符串
			else if (defaultValue == "__isa") defVal = ValString.magicIsA;			// 类型检查魔术字符串
			else if (defaultValue == "self") defVal = _self;						// self引用
			else defVal = new ValString(defaultValue);								// 新字符串
			function.parameters.Add(new Function.Param(name, defVal));
		}
		ValString _self = new ValString("self");	// 预定义的"self"字符串
		
		/// <summary>
		/// GetFunc由编译器内部使用，以获取进行内置调用的
		/// MiniScript函数。
		/// 
		/// 实现说明：
		/// - 内置函数包装在一个简单的MiniScript函数中
		/// - 该函数只包含一个操作码：CallIntrinsicA
		/// - 函数的主要作用是为参数提供局部变量上下文
		/// - 延迟创建（lazy initialization）：仅在首次调用时生成代码
		/// </summary>
		public ValFunction GetFunc() {
			if (function.code == null) {
				// 我们的小包装函数是单个操作码：CallIntrinsicA。
				// 它实际上只是为参数提供局部变量上下文。
				function.code = new List<TAC.Line>();
				// TAC.LTemp(0)：临时变量0用于存储返回值
				// TAC.Num(numericID)：传递内置函数的ID
				function.code.Add(new TAC.Line(TAC.LTemp(0), TAC.Line.Op.CallIntrinsicA, TAC.Num(numericID)));
			}
			return valFunction;
		}
		
		/// <summary>
		/// 内部使用的函数，根据给定的上下文和部分结果
		/// 执行内置函数（通过ID）。
		/// 
		/// 执行流程：
		/// 1. 通过ID查找Intrinsic对象
		/// 2. 调用其code委托
		/// 3. 返回Result对象（可能完成或未完成）
		/// </summary>
		public static Result Execute(int id, TAC.Context context, Result partialResult) {
			Intrinsic item = GetByID(id);
			return item.code(context, partialResult);	// 调用委托
		}
		
		/// <summary>
		/// Result表示内置调用的结果。内置函数要么
		/// 完成其工作，要么尚未完成（例如，因为它正在等待某些东西）。
		/// 如果完成了，设置done=true，并将结果Value存储在result中。
		/// 如果未完成，设置done=false，并将任何部分结果存储在result中
		/// （然后您的内置函数将以此Result作为partialResult再次调用）。
		/// 
		/// 异步执行机制说明：
		/// - 支持长时间运行的操作（如wait）不阻塞整个脚本
		/// - 未完成的结果会保存状态，下次tick时继续执行
		/// - 这是协程(coroutine)的简单实现
		/// </summary>
		public class Result {
			public bool done;		// 如果工作完成则为true；如果需要继续则为false
			public Value result;	// 如果完成则为最终结果；如果未完成则为进行中的数据
			
			/// <summary>
			/// Result构造函数，接受一个Value和可选的done标志。
			/// </summary>
			/// <param name="result">调用的结果或部分结果</param>
			/// <param name="done">我们的工作是否完成（可选，默认为true）</param>
			public Result(Value result, bool done=true) {
				this.done = done;
				this.result = result;
			}

			/// <summary>
			/// 简单数字结果的Result构造函数。
			/// 
			/// 便利构造函数，简化常见情况的代码
			/// </summary>
			public Result(double resultNum) {
				this.done = true;
				this.result = new ValNumber(resultNum);
			}

			/// <summary>
			/// 简单字符串结果的Result构造函数。
			/// 
			/// 优化：空字符串使用预定义实例
			/// </summary>
			public Result(string resultStr) {
				this.done = true;
				if (string.IsNullOrEmpty(resultStr)) this.result = ValString.empty;
				else this.result = new ValString(resultStr);
			}
			
			/// <summary>
			/// Result.Null: 表示null（无值）的静态Result。
			/// 
			/// 静态只读属性模式：
			/// - 使用属性而不是字段，以便可以延迟初始化
			/// - 私有静态字段_null存储实际实例
			/// - 避免每次返回null时创建新对象
			/// </summary>
			public static Result Null { get { return _null; } }
			static Result _null = new Result(null, true);
			
			/// <summary>
			/// Result.EmptyString: 表示""（空字符串）的静态Result。
			/// </summary>
			public static Result EmptyString { get { return _emptyString; } }
			static Result _emptyString = new Result(ValString.empty);
			
			/// <summary>
			/// Result.True: 表示true (1.0)的静态Result。
			/// 
			/// MiniScript中没有独立的布尔类型：
			/// - true表示为数字1.0
			/// - false表示为数字0.0
			/// </summary>
			public static Result True { get { return _true; } }
			static Result _true = new Result(ValNumber.one, true);
			
			/// <summary>
			/// Result.False: 表示false (0.0)的静态Result。
			/// </summary>
			public static Result False { get { return _false; } }
			static Result _false = new Result(ValNumber.zero, true);
			
			/// <summary>
			/// Result.Waiting: 表示需要等待的静态Result，
			/// 没有进行中的值。
			/// 
			/// 用于简单的等待情况，不需要保存状态
			/// </summary>
			public static Result Waiting { get { return _waiting; } }
			static Result _waiting = new Result(null, false);
		}
	}
	
	/// <summary>
	/// Intrinsics: 包含所有标准MiniScript内置函数的静态类。
	/// 您不应该修改这些，但可以随意浏览它们，
	/// 以获得大量如何编写自己的内置函数的示例。
	/// 
	/// 初始化模式：
	/// - 使用静态布尔标志确保只初始化一次
	/// - 延迟初始化：在首次需要时才初始化
	/// - 所有内置函数定义都在InitIfNeeded方法中
	/// </summary>
	public static class Intrinsics {

		static bool initialized;				// 初始化标志
		static ValMap intrinsicsMap = null;		// 用于"intrinsics"函数的映射

		/// <summary>
		/// KeyedValue结构体：用于排序操作。
		/// 
		/// 结构体(struct)说明：
		/// - 值类型，分配在栈上（对于小对象更高效）
		/// - 用于临时存储排序键和值的配对
		/// - 排序时避免修改原始数据结构
		/// </summary>
		private struct KeyedValue {
			public Value sortKey;	// 用于排序的键
			public Value value;		// 实际的值
			//public long valueIndex;	// （注释掉的）原始索引
		}
		
		/// <summary>
		/// 获取堆栈跟踪的辅助方法，作为ValString列表。
		/// 这是stackTrace内置函数的核心。
		/// 公共的，以便您可以从其他地方调用它（用于调试等）。
		/// 
		/// 堆栈跟踪机制：
		/// - 从虚拟机获取调用堆栈
		/// - 将每个SourceLoc转换为可读字符串
		/// - 包含文件名和行号信息
		/// </summary>
		public static ValList StackList(TAC.Machine vm) {
			ValList result = new ValList();
			if (vm == null) return result;
			// vm.GetStack()返回SourceLoc对象的枚举
			foreach (SourceLoc loc in vm.GetStack()) {
				if (loc == null) continue;
				string s = loc.context;
				if (string.IsNullOrEmpty(s)) s = "(current program)";
				s += " line " + loc.lineNum;
				result.values.Add(new ValString(s));
			}
			return result;
		}


		/// <summary>
		/// InitIfNeeded: 在脚本设置期间自动调用，以确保
		/// 定义了所有标准内置函数。注意我们如何使用
		/// 私有布尔标志来确保不会多次创建内置函数，
		/// 无论此方法被调用多少次。
		/// 
		/// 线程安全说明：
		/// - 这个实现不是线程安全的
		/// - 如果需要多线程支持，应使用lock或Lazy<T>
		/// - MiniScript设计为单线程使用
		/// </summary>
		public static void InitIfNeeded() {
			if (initialized) return;	// 已完成工作；退出
			initialized = true;
			Intrinsic f;	// 用于临时存储正在定义的内置函数

			// ========== abs ==========
			// 返回给定数字的绝对值。
			// x (number, default 0): 要取绝对值的数字
			// 示例: abs(-42)		返回 42
			f = Intrinsic.Create("abs");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				// context.GetLocalDouble: 从局部变量获取double值
				// 这是扩展方法，简化了参数获取
				return new Intrinsic.Result(Math.Abs(context.GetLocalDouble("x")));
			};

			// ========== acos ==========
			// 返回反余弦值，即余弦值为给定值的角度（以弧度为单位）。
			// x (number, default 0): 要查找角度的余弦值
			// 返回: 余弦为x的角度（以弧度为单位）
			// 示例: acos(0) 		返回 1.570796
			f = Intrinsic.Create("acos");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Acos(context.GetLocalDouble("x")));
			};

			// ========== asin ==========
			// 返回反正弦值，即正弦值为给定值的角度（以弧度为单位）。
			// x (number, default 0): 要查找角度的正弦值
			// 返回: 正弦为x的角度（以弧度为单位）
			// 示例: asin(1) 返回 1.570796
			f = Intrinsic.Create("asin");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Asin(context.GetLocalDouble("x")));
			};

			// ========== atan ==========
			// 返回值或比率的反正切值，即正切值为y/x的角度（以弧度为单位）。
			// 这将返回正确象限中的角度，同时考虑两个参数的符号。
			// 第二个参数是可选的，如果省略，此函数等效于传统的单参数atan函数。
			// 注意参数是y, x顺序。
			// y (number, default 0): 角度对边的高度
			// x (number, default 1): 角度邻边的长度
			// 返回: 正切值为y/x的角度（以弧度为单位）
			// 示例: atan(1, -1)		返回 2.356194
			f = Intrinsic.Create("atan");
			f.AddParam("y", 0);
			f.AddParam("x", 1);
			f.code = (context, partialResult) => {
				double y = context.GetLocalDouble("y");
				double x = context.GetLocalDouble("x");
				// 优化：如果x为1，使用更简单的Math.Atan
				if (x == 1.0) return new Intrinsic.Result(Math.Atan(y));
				// Math.Atan2处理象限正确性
				return new Intrinsic.Result(Math.Atan2(y, x));
			};

			/// <summary>
			/// 辅助函数：将double分解为符号和无符号数值
			/// 
			/// Func<T, TResult>委托说明：
			/// - 泛型委托，表示带参数和返回值的方法
			/// - Func<double, Tuple<bool, ulong>>：接受double，返回Tuple<bool, ulong>
			/// - Tuple用于返回多个值
			/// - lambda表达式语法：(参数) => 表达式
			/// </summary>
			Func<double, Tuple<bool, ulong>> doubleToUnsignedSplit = (val) => {
				// Item1: 是否为负数
				// Item2: 绝对值的无符号整数表示
				return new Tuple<bool, ulong>(Math.Sign(val) == -1, (ulong)Math.Abs(val));
			};

			// ========== bitAnd ==========
			// 将其参数视为整数，并计算按位"与"：
			// 结果中的每一位仅在两个参数中的相应位都设置时才设置。
			// i (number, default 0): 第一个整数参数
			// j (number, default 0): 第二个整数参数
			// 返回: i和j的按位"与"
			// 示例: bitAnd(14, 7)		返回 6
			// 另见: bitOr; bitXor
			f = Intrinsic.Create("bitAnd");
			f.AddParam("i", 0);
			f.AddParam("j", 0);
			f.code = (context, partialResult) => {
				// 分离符号和数值
				var i = doubleToUnsignedSplit(context.GetLocalDouble("i"));
				var j = doubleToUnsignedSplit(context.GetLocalDouble("j"));
				// 符号的按位与：两个都为负才为负
				var sign = i.Item1 & j.Item1;
				// 数值的按位与
				double val = i.Item2 & j.Item2;
				return new Intrinsic.Result(sign ? -val : val);
            };

			// ========== bitOr ==========
			// 将其参数视为整数，并计算按位"或"：
			// 结果中的每一位在任一（或两个）参数中的相应位设置时都会设置。
			// i (number, default 0): 第一个整数参数
			// j (number, default 0): 第二个整数参数
			// 返回: i和j的按位"或"
			// 示例: bitOr(14, 7)		返回 15
			// 另见: bitAnd; bitXor
			f = Intrinsic.Create("bitOr");
			f.AddParam("i", 0);
			f.AddParam("j", 0);
			f.code = (context, partialResult) => {
				var i = doubleToUnsignedSplit(context.GetLocalDouble("i"));
				var j = doubleToUnsignedSplit(context.GetLocalDouble("j"));
				// 符号的按位或：任一为负则为负
				var sign = i.Item1 | j.Item1;
				double val = i.Item2 | j.Item2;
                return new Intrinsic.Result(sign ? -val : val);
			};
			
			// ========== bitXor ==========
			// 将其参数视为整数，并计算按位"异或"：
			// 结果中的每一位仅在恰好一个（不是零个或两个）参数中的
			// 相应位设置时才设置。
			// i (number, default 0): 第一个整数参数
			// j (number, default 0): 第二个整数参数
			// 返回: i和j的按位"异或"
			// 示例: bitXor(14, 7)		返回 9
			// 另见: bitAnd; bitOr
			f = Intrinsic.Create("bitXor");
			f.AddParam("i", 0);
			f.AddParam("j", 0);
			f.code = (context, partialResult) => {
                var i = doubleToUnsignedSplit(context.GetLocalDouble("i"));
                var j = doubleToUnsignedSplit(context.GetLocalDouble("j"));
                // 符号的按位异或：一个为负则为负
                var sign = i.Item1 ^ j.Item1;
                double val = i.Item2 ^ j.Item2;
                return new Intrinsic.Result(sign ? -val : val);
            };
			
			// ========== char ==========
			// 从Unicode码位获取字符。
			// codePoint (number, default 65): 字符的Unicode码位
			// 返回: 包含指定字符的字符串
			// 示例: char(42)		返回 "*"
			// 另见: code
			f = Intrinsic.Create("char");
			f.AddParam("codePoint", 65);
			f.code = (context, partialResult) => {
				int codepoint = context.GetLocalInt("codePoint");
				// char.ConvertFromUtf32: 将Unicode码位转换为字符串
				// 处理基本多文种平面(BMP)和补充平面字符
				string s = char.ConvertFromUtf32(codepoint);
				return new Intrinsic.Result(s);
			};
			
			// ========== ceil ==========
			// 返回"天花板"，即大于或等于给定数字的最接近的整数。
			// x (number, default 0): 要获取天花板的数字
			// 返回: 不小于x的最接近整数
			// 示例: ceil(41.2)		返回 42
			// 另见: floor
			f = Intrinsic.Create("ceil");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Ceiling(context.GetLocalDouble("x")));
			};
			
			// ========== code ==========
			// 返回给定字符串第一个字符的Unicode码位。
			// 这是`char`的逆函数。
			// 可以使用函数语法或点语法调用。
			// self (string): 要获取码位的字符串
			// 返回: self第一个字符的Unicode码位
			// 示例: "*".code		返回 42
			// 示例: code("*")		返回 42
			f = Intrinsic.Create("code");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.self;
				int codepoint = 0;
				// char.ConvertToUtf32: 将字符串转换为Unicode码位
				// 第二个参数0表示从字符串的第一个字符开始
				if (self != null) codepoint = char.ConvertToUtf32(self.ToString(), 0);
				return new Intrinsic.Result(codepoint);
			};
						
			// ========== cos ==========
			// 返回给定角度（以弧度为单位）的余弦值。
			// radians (number): 要获取余弦值的角度（以弧度为单位）
			// 返回: 给定角度的余弦值
			// 示例: cos(0)		返回 1
			f = Intrinsic.Create("cos");
			f.AddParam("radians", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Cos(context.GetLocalDouble("radians")));
			};

			// ========== floor ==========
			// 返回"地板"，即小于或等于给定数字的最接近的整数。
			// x (number, default 0): 要获取地板的数字
			// 返回: 不大于x的最接近整数
			// 示例: floor(42.9)		返回 42
			// 另见: ceil
			f = Intrinsic.Create("floor");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Floor(context.GetLocalDouble("x")));
			};

			// ========== funcRef ==========
			// 返回表示MiniScript核心类型系统中函数引用的映射。
			// 这可以与`isa`一起使用，以检查变量是否引用函数
			// （但请务必使用@以避免调用函数并测试结果）。
			// 示例: @floor isa funcRef		返回 1
			// 另见: number, string, list, map
			f = Intrinsic.Create("funcRef");
			f.code = (context, partialResult) => {
				// 延迟创建类型映射
				// 从虚拟机的全局上下文进行评估副本
				// EvalCopy确保每个虚拟机实例有自己的类型副本
				if (context.vm.functionType == null) {
					context.vm.functionType = FunctionType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.functionType);
			};
			
			// ========== hash ==========
			// 返回对给定值"相对唯一"的整数。
			// 对于字符串，哈希区分大小写。对于列表或映射，
			// 哈希结合所有元素的哈希值。
			// 注意，返回的值取决于平台，并且可能在不同的
			// MiniScript实现中有所不同。
			// obj (any type): 要哈希的值
			// 返回: 给定值的整数哈希
			f = Intrinsic.Create("hash");
			f.AddParam("obj");
			f.code = (context, partialResult) => {
				Value val = context.GetLocal("obj");
				// val.Hash()调用值的哈希方法
				// 不同的Value子类有不同的哈希实现
				return new Intrinsic.Result(val.Hash());
			};

			// ========== hasIndex ==========
			// 返回给定索引对于此对象是否有效，即是否可以与
			// 方括号一起使用以从self获取某个值。当self是列表或字符串时，
			// 对于从-(字符串长度)到(字符串长度-1)的整数，结果为true。
			// 当self是映射时，对于映射中的任何键（索引），它为true。
			// 如果在数字上调用，此方法会抛出运行时异常。
			// self (string, list, or map): 要检查索引的对象
			// index (any): 要考虑作为可能索引的值
			// 返回: 如果self[index]有效则为1；否则为0
			// 示例: "foo".hasIndex(2)		返回 1
			// 示例: "foo".hasIndex(3)		返回 0
			// 另见: indexes
			f = Intrinsic.Create("hasIndex");
			f.AddParam("self");
			f.AddParam("index");
			f.code = (context, partialResult) => {
				Value self = context.self;
				Value index = context.GetLocal("index");
				// 列表：检查整数索引是否在有效范围内
				if (self is ValList) {
					if (index is ValNumber) {
						List<Value> list = ((ValList)self).values;
						int i = index.IntValue();
						// 支持负索引（从末尾倒数）
						return new Intrinsic.Result(ValNumber.Truth(i >= -list.Count && i < list.Count));
					}
					return Intrinsic.Result.False;
				// 字符串：检查整数索引是否在有效范围内
				} else if (self is ValString) {
					if (index is ValNumber) {
						string str = ((ValString)self).value;
						int i = index.IntValue();
						return new Intrinsic.Result(ValNumber.Truth(i >= -str.Length && i < str.Length));
					}
					return new Intrinsic.Result(ValNumber.zero);
				// 映射：检查键是否存在
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					return new Intrinsic.Result(ValNumber.Truth(map.ContainsKey(index)));
				}
				return Intrinsic.Result.Null;
			};
			
			// ========== indexes ==========
			// 返回字典的键，或字符串或列表的非负索引。
			// self (string, list, or map): 要获取索引的对象
			// 返回: self的有效索引列表
			// 示例: "foo".indexes		返回 [0, 1, 2]
			// 另见: hasIndex
			f = Intrinsic.Create("indexes");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.self;
				// 映射：返回所有键
				if (self is ValMap) {
					ValMap map = (ValMap)self;
					List<Value> keys = new List<Value>(map.map.Keys);
					// 将ValNull替换为C# null
					for (int i = 0; i < keys.Count; i++) if (keys[i] is ValNull) keys[i] = null;
					return new Intrinsic.Result(new ValList(keys));
				// 字符串：返回0到length-1的索引
				} else if (self is ValString) {
					string str = ((ValString)self).value;
					List<Value> indexes = new List<Value>(str.Length);
					for (int i = 0; i < str.Length; i++) {
						indexes.Add(TAC.Num(i));	// TAC.Num优化数字创建
					}
					return new Intrinsic.Result(new ValList(indexes));
				// 列表：返回0到count-1的索引
				} else if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					List<Value> indexes = new List<Value>(list.Count);
					for (int i = 0; i < list.Count; i++) {
						indexes.Add(TAC.Num(i));
					}
					return new Intrinsic.Result(new ValList(indexes));
				}
				return Intrinsic.Result.Null;
			};
			
			// ========== indexOf ==========
			// 返回给定值的索引或键，如果未找到，则返回null。
			// self (string, list, or map): 要搜索的对象
			// value (any): 要搜索的值
			// after (any, optional): 如果给定，在此索引之后开始搜索
			// 返回: 第一个索引（在`after`之后），使得self[index] == value，或null
			// 示例: "Hello World".indexOf("o")		返回 4
			// 示例: "Hello World".indexOf("o", 4)		返回 7
			// 示例: "Hello World".indexOf("o", 7)		返回 null			
			f = Intrinsic.Create("indexOf");
			f.AddParam("self");
			f.AddParam("value");
			f.AddParam("after");
			f.code = (context, partialResult) => {
				Value self = context.self;
				Value value = context.GetLocal("value");
				Value after = context.GetLocal("after");
				// 列表：使用FindIndex查找
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					int idx;
					if (after == null) {
						// LINQ lambda表达式：x => 条件
						// 检查元素是否等于要查找的值
						idx = list.FindIndex(x => 
							x == null ? value == null : x.Equality(value) == 1);
					} else {
						int afterIdx = after.IntValue();
						if (afterIdx < -1) afterIdx += list.Count;	// 处理负索引
						if (afterIdx < -1 || afterIdx >= list.Count-1) return Intrinsic.Result.Null;
						// FindIndex重载：从指定索引开始搜索
						idx = list.FindIndex(afterIdx + 1, x => 
							x == null ? value == null : x.Equality(value) == 1);
					}
					if (idx >= 0) return new Intrinsic.Result(idx);
				// 字符串：使用IndexOf查找子字符串
				} else if (self is ValString) {
					string str = ((ValString)self).value;
					if (value == null) return Intrinsic.Result.Null;
					string s = value.ToString();
					int idx;
					if (after == null) {
						// StringComparison.Ordinal：使用序数（二进制）排序规则
						// 比默认的文化相关比较更快
						idx = str.IndexOf(s, StringComparison.Ordinal);
					} else {
						int afterIdx = after.IntValue();
						if (afterIdx < -1) afterIdx += str.Length;
						if (afterIdx < -1 || afterIdx >= str.Length-1) return Intrinsic.Result.Null;
						idx = str.IndexOf(s, afterIdx + 1, StringComparison.Ordinal);
					}
					if (idx >= 0) return new Intrinsic.Result(idx);
				// 映射：迭代键值对查找
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					bool sawAfter = (after == null);
					// foreach迭代KeyValuePair<Value, Value>
					foreach (var kv in map.map) {
						if (!sawAfter) {
							if (kv.Key.Equality(after) == 1) sawAfter = true;
						} else {
							if (kv.Value == null ? value == null : kv.Value.Equality(value) == 1) return new Intrinsic.Result(kv.Key);
						}
					}
				}
				return Intrinsic.Result.Null;
			};

			// ========== insert ==========
			// 将新元素插入字符串或列表。对于列表，
			// 列表会就地修改并返回。字符串是不可变的，
			// 因此在这种情况下，原始字符串保持不变，但返回
			// 插入了值的新字符串。
			// self (string or list): 要插入的序列
			// index (number): 插入新项的位置
			// value (any): 在指定索引处插入的元素
			// 返回: 修改后的列表，新字符串
			// 示例: "Hello".insert(2, 42)		返回 "He42llo"
			// 另见: remove
			f = Intrinsic.Create("insert");
			f.AddParam("self");
			f.AddParam("index");
			f.AddParam("value");
			f.code = (context, partialResult) => {
				Value self = context.self;
				Value index = context.GetLocal("index");
				Value value = context.GetLocal("value");
				if (index == null) throw new RuntimeException("insert: index argument required");
				if (!(index is ValNumber)) throw new RuntimeException("insert: number required for index argument");
				int idx = index.IntValue();
				// 列表：就地插入
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					if (idx < 0) idx += list.Count + 1;	// +1因为插入时从末尾计数
					Check.Range(idx, 0, list.Count);	// 允许插入到末尾（count位置）
					list.Insert(idx, value);
					return new Intrinsic.Result(self);
				// 字符串：创建新字符串
				} else if (self is ValString) {
					string s = self.ToString();
					if (idx < 0) idx += s.Length + 1;
					Check.Range(idx, 0, s.Length);
					// 字符串拼接：前部分 + 新值 + 后部分
					s = s.Substring(0, idx) + value.ToString() + s.Substring(idx);
					return new Intrinsic.Result(s);
				} else {
					throw new RuntimeException("insert called on invalid type");
				}
			};

			// ========== intrinsics ==========
			// 返回所有命名内置函数的只读映射。
			f = Intrinsic.Create("intrinsics");
			f.code = (context, partialResult) => {
				if (intrinsicsMap != null) return new Intrinsic.Result(intrinsicsMap);
				intrinsicsMap = new ValMap();
				// assignOverride：自定义赋值行为
				// 阻止修改内置函数映射
				intrinsicsMap.assignOverride = (k,v) => {
					throw new RuntimeException("Assignment to protected map");
					return true;	// 实际上永远不会执行到这里
				};
		
				// 填充所有内置函数
				foreach (var intrinsic in Intrinsic.all) {
					if (intrinsic == null || string.IsNullOrEmpty(intrinsic.name)) continue;
					intrinsicsMap[intrinsic.name] = intrinsic.GetFunc();
				}
		
				return new Intrinsic.Result(intrinsicsMap);
			};

			// ========== join ==========
			// 将列表的元素连接在一起形成字符串。
			// self (list): 要连接的列表
			// delimiter (string, default " "): 在每对元素之间插入的字符串
			// 返回: 通过用delimiter连接self元素构建的字符串
			// 示例: [2,4,8].join("-")		返回 "2-4-8"
			// 另见: split
			f = Intrinsic.Create("join");
			f.AddParam("self");
			f.AddParam("delimiter", " ");
			f.code = (context, partialResult) => {
				Value val = context.self;
				string delim = context.GetLocalString("delimiter");
				if (!(val is ValList)) return new Intrinsic.Result(val);
				ValList src = (val as ValList);
				List<string> list = new List<string>(src.values.Count);
				// 将所有值转换为字符串
				for (int i=0; i<src.values.Count; i++) {
					if (src.values[i] == null) list.Add(null);
					else list.Add(src.values[i].ToString());
				}
				// string.Join: .NET内置方法，高效连接字符串
				// ToArray(): 将List<string>转换为string[]
				string result = string.Join(delim, list.ToArray());
				return new Intrinsic.Result(result);
			};
			
			// ========== len ==========
			// 返回字符串中的字符数、列表中的元素数或映射中的键/值对数。
			// 可以使用函数语法或点语法调用。
			// self (list, string, or map): 要获取长度的对象
			// 返回: self中的长度（元素数）
			// 示例: "hello".len		返回 5
			f = Intrinsic.Create("len");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.self;
				if (val is ValList) {
					List<Value> list = ((ValList)val).values;
					return new Intrinsic.Result(list.Count);
				} else if (val is ValString) {
					string str = ((ValString)val).value;
					return new Intrinsic.Result(str.Length);
				} else if (val is ValMap) {
					return new Intrinsic.Result(((ValMap)val).Count);
				}
				return Intrinsic.Result.Null;
			};
			
			// ========== list 类型 ==========
			// 返回表示MiniScript核心类型系统中list数据类型的映射。
			// 这可以与`isa`一起使用，以检查变量是否引用列表。
			// 您还可以在此处分配新方法，使其可用于所有列表。
			// 示例: [1, 2, 3] isa list		返回 1
			// 另见: number, string, map, funcRef
			f = Intrinsic.Create("list");
			f.code = (context, partialResult) => {
				if (context.vm.listType == null) {
					context.vm.listType = ListType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.listType);
			};
			
			// ========== log ==========
			// 返回给定数字的对数（以给定底数），
			// 即使base^y = x的数字y。
			// x (number): 要取对数的数字
			// base (number, default 10): 对数底数
			// 返回: 使base提升到它时产生x的数字
			// 示例: log(1000)		返回 3（因为10^3 == 1000）
			f = Intrinsic.Create("log");
			f.AddParam("x", 0);
			f.AddParam("base", 10);
			f.code = (context, partialResult) => {
				double x = context.GetLocalDouble("x");
				double b = context.GetLocalDouble("base");
				double result;
				// 特殊情况：自然对数（底数e）
				if (Math.Abs(b - 2.718282) < 0.000001) result = Math.Log(x);
				// 一般情况：换底公式 log_b(x) = ln(x) / ln(b)
				else result = Math.Log(x) / Math.Log(b);
				return new Intrinsic.Result(result);
			};
			
			// ========== lower ==========
			// 返回字符串的小写版本。
			// 可以使用函数语法或点语法调用。
			// self (string): 要小写的字符串
			// 返回: 所有大写字母转换为小写的字符串
			// 示例: "Mo Spam".lower		返回 "mo spam"
			// 另见: upper
			f = Intrinsic.Create("lower");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.self;
				if (val is ValString) {
					string str = ((ValString)val).value;
					// ToLower(): .NET字符串方法，使用当前文化
					return new Intrinsic.Result(str.ToLower());
				}
				return new Intrinsic.Result(val);
			};

			// ========== map 类型 ==========
			// 返回表示MiniScript核心类型系统中map数据类型的映射。
			// 这可以与`isa`一起使用，以检查变量是否引用映射。
			// 您还可以在此处分配新方法，使其可用于所有映射。
			// 示例: {1:"one"} isa map		返回 1
			// 另见: number, string, list, funcRef
			f = Intrinsic.Create("map");
			f.code = (context, partialResult) => {
				if (context.vm.mapType == null) {
					context.vm.mapType = MapType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.mapType);
			};
			
			// ========== number 类型 ==========
			// 返回表示MiniScript核心类型系统中number数据类型的映射。
			// 这可以与`isa`一起使用，以检查变量是否引用数字。
			// 您还可以在此处分配新方法，使其可用于所有映射
			// （尽管由于MiniScript解析器的限制，此类方法不适用于数字文字）。
			// 示例: 42 isa number		返回 1
			// 另见: string, list, map, funcRef
			f = Intrinsic.Create("number");
			f.code = (context, partialResult) => {
				if (context.vm.numberType == null) {
					context.vm.numberType = NumberType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.numberType);
			};
			
			// ========== pi ==========
			// 返回通用常数π，即圆的周长与其直径的比率。
			// 示例: pi		返回 3.141593
			f = Intrinsic.Create("pi");
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.PI);
			};

			// ========== print ==========
			// 在默认输出流上显示给定值。确切效果可能因环境而异。
			// 在大多数情况下，给定字符串后将跟随标准行分隔符
			// （除非使用第二个参数覆盖）。
			// s (any): 要打印的值（根据需要转换为字符串）
			// delimiter (string or null): 在s之后打印的字符串；如果为null，使用标准EOL
			// 返回: null
			// 示例: print 6*7
			f = Intrinsic.Create("print");
			f.AddParam("s", ValString.empty);
			f.AddParam("delimiter");
			f.code = (context, partialResult) => {
				Value sVal = context.GetLocal("s");
				string s = (sVal == null ? "null" : sVal.ToString());
				Value delimiter = context.GetLocal("delimiter");
				// context.vm.standardOutput: 虚拟机的输出函数委托
				// 第二个参数：是否添加换行符
				if (delimiter == null) context.vm.standardOutput(s, true);
				else context.vm.standardOutput(s + delimiter.ToString(), false);
				return Intrinsic.Result.Null;
			};
				
			// ========== pop ==========
			// 删除并返回列表中的最后一项，或映射的任意键。
			// 如果列表或映射为空（或在任何其他数据类型上调用），返回null。
			// 可以使用函数语法或点语法调用。
			// self (list or map): 要从末尾删除元素的对象
			// 返回: 删除的值，或null
			// 示例: [1, 2, 3].pop		返回（并删除）3
			// 另见: pull; push; remove
			f = Intrinsic.Create("pop");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.self;
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					if (list.Count < 1) return Intrinsic.Result.Null;
					Value result = list[list.Count-1];
					list.RemoveAt(list.Count-1);
					return new Intrinsic.Result(result);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					if (map.map.Count < 1) return Intrinsic.Result.Null;
					// LINQ First(): 获取第一个元素
					// 对于映射，"任意"实际上是迭代顺序的第一个
					Value result = map.map.Keys.First();
					map.map.Remove(result);
					return new Intrinsic.Result(result);
				}
				return Intrinsic.Result.Null;
			};

			// ========== pull ==========
			// 删除并返回列表中的第一项，或映射的任意键。
			// 如果列表或映射为空（或在任何其他数据类型上调用），返回null。
			// 可以使用函数语法或点语法调用。
			// self (list or map): 要从开头删除元素的对象
			// 返回: 删除的值，或null
			// 示例: [1, 2, 3].pull		返回（并删除）1
			// 另见: pop; push; remove
			f = Intrinsic.Create("pull");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.self;
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					if (list.Count < 1) return Intrinsic.Result.Null;
					Value result = list[0];
					list.RemoveAt(0);	// 删除第一个元素
					return new Intrinsic.Result(result);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					if (map.map.Count < 1) return Intrinsic.Result.Null;
					Value result = map.map.Keys.First();
					map.map.Remove(result);
					return new Intrinsic.Result(result);
				}
				return Intrinsic.Result.Null;
			};

			// ========== push ==========
			// 将项附加到列表末尾，或将其作为值为1的键插入映射。
			// 可以使用函数语法或点语法调用。
			// self (list or map): 要附加元素的对象
			// 返回: self
			// 另见: pop, pull, insert
			f = Intrinsic.Create("push");
			f.AddParam("self");
			f.AddParam("value");
			f.code = (context, partialResult) => {
				Value self = context.self;
				Value value = context.GetLocal("value");
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					list.Add(value);	// 添加到列表末尾
					return new Intrinsic.Result(self);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					// 将值作为键添加，值为1（true）
					map.map[value] = ValNumber.one;
					return new Intrinsic.Result(self);
				}
				return Intrinsic.Result.Null;
			};

			// ========== range ==========
			// 返回包含范围内一系列数字的列表。
			// from (number, default 0): 列表中包含的第一个数字
			// to (number, default 0): 停止向列表添加数字的点
			// step (number, optional): 每步添加到前一个数字的数量；
			//	如果to > from，默认为1，如果to < from，默认为-1
			// 示例: range(50, 5, -10)		返回 [50, 40, 30, 20, 10]
			f = Intrinsic.Create("range");
			f.AddParam("from", 0);
			f.AddParam("to", 0);
			f.AddParam("step");
			f.code = (context, partialResult) => {
				Value p0 = context.GetLocal("from");
				Value p1 = context.GetLocal("to");
				Value p2 = context.GetLocal("step");
				double fromVal = p0.DoubleValue();
				double toVal = p1.DoubleValue();
				// 自动确定步长方向
				double step = (toVal >= fromVal ? 1 : -1);
				if (p2 is ValNumber) step = (p2 as ValNumber).value;
				if (step == 0) throw new RuntimeException("range() error (step==0)");
				List<Value> values = new List<Value>();
				// 计算需要的元素数量
				int count = (int)((toVal - fromVal) / step) + 1;
				if (count > ValList.maxSize) throw new RuntimeException("list too large");
				try {
					// 预分配容量以提高性能
					values = new List<Value>(count);
					// 三元运算符：根据步长方向选择终止条件
					for (double v = fromVal; step > 0 ? (v <= toVal) : (v >= toVal); v += step) {
						values.Add(TAC.Num(v));
					}
				} catch (SystemException e) {
					// 捕获内存不足等系统异常
					values = null;
					throw(new LimitExceededException("range() error", e));
				}
				return new Intrinsic.Result(new ValList(values));
			};

			// ========== refEquals ==========
			// 测试两个值是否引用完全相同的对象（而不是
			// 仅仅表示相同的值）。对于数字，这与==相同，
			// 但对于字符串、列表和映射，它是引用相等性。
			f = Intrinsic.Create("refEquals");
			f.AddParam("a");
			f.AddParam("b");
			f.code = (context, partialResult) => {
				Value a = context.GetLocal("a");
				Value b = context.GetLocal("b");
				bool result = false;
				if (a == null) {
					result = (b == null);
				} else if (a is ValNumber) {
					// 数字：比较值
					result = (b is ValNumber && a.DoubleValue() == b.DoubleValue());
				} else if (a is ValString) {
					// 字符串：比较内部字符串对象的引用
					// ReferenceEquals: 检查两个引用是否指向同一对象
					result = (b is ValString && ReferenceEquals( ((ValString)a).value, ((ValString)b).value ));
				} else if (a is ValList) {
					// 列表：比较内部列表对象的引用
					result = (b is ValList && ReferenceEquals( ((ValList)a).values, ((ValList)b).values ));
				} else if (a is ValMap) {
					// 映射：比较内部字典对象的引用
					result = (b is ValMap && ReferenceEquals( ((ValMap)a).map, ((ValMap)b).map ));
				} else if (a is ValFunction) {
					// 函数：比较内部函数对象的引用
					result = (b is ValFunction && ReferenceEquals( ((ValFunction)a).function, ((ValFunction)b).function ));
				} else {
					// 其他类型：回退到值相等性
					result = (a.Equality(b) >= 1);
				}
				return new Intrinsic.Result(ValNumber.Truth(result));
			};

			// ========== remove ==========
			// 删除列表、映射或字符串的一部分。确切行为取决于
			// self的数据类型：
			// 		list: 通过索引删除一个元素；列表就地变异；
			//			返回null，如果给定索引超出范围则抛出错误
			//		map: 通过键删除一个键/值对；映射就地变异；
			//			如果找到键则返回1，否则返回0
			//		string:	返回删除第一个k出现的新字符串
			// 可以使用函数语法或点语法调用。
			// self (list, map, or string): 要删除某物的对象
			// k (any): 要删除的索引或子字符串
			// 返回: （见上文）
			// 示例: a=["a","b","c"]; a.remove 1		结果a == ["a", "c"]
			// 示例: d={"ichi":"one"}; d.remove "ni"		返回 0
			// 示例: "Spam".remove("S")		返回 "pam"
			// 另见: indexOf
			f = Intrinsic.Create("remove");
			f.AddParam("self");
			f.AddParam("k");
			f.code = (context, partialResult) => {
				Value self = context.self;
				Value k = context.GetLocal("k");
				if (self is ValMap) {
					ValMap selfMap = (ValMap)self;
					if (k == null) k = ValNull.instance;	// null键用ValNull.instance表示
					if (selfMap.map.ContainsKey(k)) {
						selfMap.map.Remove(k);
						return Intrinsic.Result.True;
					}
					return Intrinsic.Result.False;
				} else if (self is ValList) {
					if (k == null) throw new RuntimeException("argument to 'remove' must not be null");
					ValList selfList = (ValList)self;
					int idx = k.IntValue();
					if (idx < 0) idx += selfList.values.Count;	// 处理负索引
					Check.Range(idx, 0, selfList.values.Count-1);	// 验证索引范围
					selfList.values.RemoveAt(idx);
					return Intrinsic.Result.Null;
				} else if (self is ValString) {
					if (k == null) throw new RuntimeException("argument to 'remove' must not be null");
					ValString selfStr = (ValString)self;
					string substr = k.ToString();
					int foundPos = selfStr.value.IndexOf(substr, StringComparison.Ordinal);
					if (foundPos < 0) return new Intrinsic.Result(self);	// 未找到
					// Remove: 删除指定位置和长度的子字符串
					return new Intrinsic.Result(selfStr.value.Remove(foundPos, substr.Length));
				}
				throw new TypeException("Type Error: 'remove' requires map, list, or string");
			};

			// ========== replace ==========
			// 替换列表或映射的所有匹配元素，或字符串的子字符串，
			// 为新值。列表和映射就地变异，并返回自身。
			// 字符串是不可变的，因此原始字符串（当然）不变，但
			// 返回进行了替换的新字符串。注意，对于映射，搜索和
			// 替换的是值，而不是键。
			// self (list, map, or string): 要替换元素的对象
			// oldval (any): 要替换的值或子字符串
			// newval (any): 在找到oldval时替换的新值或子字符串
			// maxCount (number, optional): 如果给定，替换不超过这么多个
			// 返回: 修改后的列表或映射，或完成替换的新字符串
			// 示例: "Happy Pappy".replace("app", "ol")		返回 "Holy Poly"
			// 示例: [1,2,3,2,5].replace(2, 42)		返回（并变异为）[1, 42, 3, 42, 5]
			// 示例: d = {1: "one"}; d.replace("one", "ichi")		返回（并变异为）{1: "ichi"}
			f = Intrinsic.Create("replace");
			f.AddParam("self");
			f.AddParam("oldval");
			f.AddParam("newval");
			f.AddParam("maxCount");
			f.code = (context, partialResult) => {
				Value self = context.self;
				if (self == null) throw new RuntimeException("argument to 'replace' must not be null");
				Value oldval = context.GetLocal("oldval");
				Value newval = context.GetLocal("newval");
				Value maxCountVal = context.GetLocal("maxCount");
				int maxCount = -1;
				if (maxCountVal != null) {
					maxCount = maxCountVal.IntValue();
					if (maxCount < 1) return new Intrinsic.Result(self);
				}
				int count = 0;
				if (self is ValMap) {
					ValMap selfMap = (ValMap)self;
					// C#不允许在迭代键时更改值。
					// 因此先收集要更改的键，然后再更改它们。
					List<Value> keysToChange = null;
					foreach (Value k in selfMap.map.Keys) {
						if (selfMap.map[k].Equality(oldval) == 1) {
							if (keysToChange == null) keysToChange = new List<Value>();
							keysToChange.Add(k);
							count++;
							if (maxCount > 0 && count == maxCount) break;
						}
					}
					if (keysToChange != null) foreach (Value k in keysToChange) {
						selfMap.map[k] = newval;
					}
					return new Intrinsic.Result(self);
				} else if (self is ValList) {
					ValList selfList = (ValList)self;
					int idx = -1;
					while (true) {
						// FindIndex从idx+1开始查找
						idx = selfList.values.FindIndex(idx+1, x => x.Equality(oldval) == 1);
						if (idx < 0) break;
						selfList.values[idx] = newval;
						count++;
						if (maxCount > 0 && count == maxCount) break;
					}
					return new Intrinsic.Result(self);
				} else if (self is ValString) {
					string str = self.ToString();
					string oldstr = oldval == null ? "" : oldval.ToString();
					if (string.IsNullOrEmpty(oldstr)) throw new RuntimeException("replace: oldval argument is empty");
					string newstr = newval == null ? "" : newval.ToString();
					int idx = 0;
					while (true) {
						idx = str.IndexOf(oldstr, idx, StringComparison.Ordinal);
						if (idx < 0) break;
						// 字符串拼接：前部分 + 新值 + 后部分
						str = str.Substring(0, idx) + newstr + str.Substring(idx + oldstr.Length);
						idx += newstr.Length;	// 移动到替换后的位置
						count++;
						if (maxCount > 0 && count == maxCount) break;
					}
					return new Intrinsic.Result(str);
				}
				throw new TypeException("Type Error: 'replace' requires map, list, or string");
			};

			// ========== round ==========
			// 将数字四舍五入到指定的小数位数。如果给定
			// decimalPlaces为负数，则四舍五入到10的幂：
			// -1四舍五入到最接近的10，-2四舍五入到最接近的100，等等。
			// x (number): 要四舍五入的数字
			// decimalPlaces (number, defaults to 0): 小数点后四舍五入到多少位
			// 示例: round(pi, 2)		返回 3.14
			// 示例: round(12345, -3)		返回 12000
			f = Intrinsic.Create("round");
			f.AddParam("x", 0);
			f.AddParam("decimalPlaces", 0);
			f.code = (context, partialResult) => {
				double num = context.GetLocalDouble("x");
				int decimalPlaces = context.GetLocalInt("decimalPlaces");
				if (decimalPlaces >= 0) {
					if (decimalPlaces > 15) decimalPlaces = 15;	// 限制精度
					// Math.Round: .NET四舍五入方法
					num = Math.Round(num, decimalPlaces);
				} else {
					// 负数情况：四舍五入到10的幂
					double pow10 = Math.Pow(10, -decimalPlaces);
					num = Math.Round(num / pow10) * pow10;
				}
				return new Intrinsic.Result(num);
			};


			// ========== rnd ==========
			// 生成0到1之间的伪随机数（包括0但不包括1）。
			// 如果给定种子，则使用该种子值重置生成器，
			// 允许您创建可重复的随机数序列。如果您从不
			// 指定种子，则会自动初始化，在每次运行时生成唯一序列。
			// seed (number, optional): 如果给定，使用此值重置序列
			// 返回: 范围[0,1)内的伪随机数
			f = Intrinsic.Create("rnd");
			f.AddParam("seed");
			f.code = (context, partialResult) => {
				// 延迟创建Random实例
				if (random == null) random = new Random();
				Value seed = context.GetLocal("seed");
				if (seed != null) random = new Random(seed.IntValue());	// 用种子重置
				return new Intrinsic.Result(random.NextDouble());
			};

			// ========== sign ==========
			// 对于负数返回-1，对于正数返回1，对于零返回0。
			// x (number): 要获取符号的数字
			// 返回: 数字的符号
			// 示例: sign(-42.6)		返回 -1
			f = Intrinsic.Create("sign");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Sign(context.GetLocalDouble("x")));
			};

			// ========== sin ==========
			// 返回给定角度（以弧度为单位）的正弦值。
			// radians (number): 要获取正弦值的角度（以弧度为单位）
			// 返回: 给定角度的正弦值
			// 示例: sin(pi/2)		返回 1
			f = Intrinsic.Create("sin");
			f.AddParam("radians", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Sin(context.GetLocalDouble("radians")));
			};
				
			// ========== slice ==========
			// 返回字符串或列表的子集。这等效于使用
			// 方括号切片运算符seq[from:to]，但使用普通函数语法。
			// seq (string or list): 要获取子序列的序列
			// from (number, default 0): 要返回的第一个元素的从0开始的索引（如果为负，从末尾计数）
			// to (number, optional): 结果中*不*包含的第一个元素的从0开始的索引
			//		（如果为负，从末尾计数；如果省略，返回序列的其余部分）
			// 返回: 子字符串或子列表
			// 示例: slice("Hello", -2)		返回 "lo"
			// 示例: slice(["a","b","c","d"], 1, 3)		返回 ["b", "c"]
			f = Intrinsic.Create("slice");
			f.AddParam("seq");
			f.AddParam("from", 0);
			f.AddParam("to");
			f.code = (context, partialResult) => {
				Value seq = context.GetLocal("seq");
				int fromIdx = context.GetLocalInt("from");
				Value toVal = context.GetLocal("to");
				int toIdx = 0;
				if (toVal != null) toIdx = toVal.IntValue();
				if (seq is ValList) {
					List<Value> list = ((ValList)seq).values;
					if (fromIdx < 0) fromIdx += list.Count;		// 处理负索引
					if (fromIdx < 0) fromIdx = 0;
					if (toVal == null) toIdx = list.Count;		// 默认到末尾
					if (toIdx < 0) toIdx += list.Count;
					if (toIdx > list.Count) toIdx = list.Count;
					ValList slice = new ValList();
					if (fromIdx < list.Count && toIdx > fromIdx) {
						for (int i = fromIdx; i < toIdx; i++) {
							slice.values.Add(list[i]);
						}
					}
					return new Intrinsic.Result(slice);
				} else if (seq is ValString) {
					string str = ((ValString)seq).value;
					if (fromIdx < 0) fromIdx += str.Length;
					if (fromIdx < 0) fromIdx = 0;
					if (toVal == null) toIdx = str.Length;
					if (toIdx < 0) toIdx += str.Length;
					if (toIdx > str.Length) toIdx = str.Length;
					if (toIdx - fromIdx <= 0) return Intrinsic.Result.EmptyString;
					// Substring: 提取子字符串
					return new Intrinsic.Result(str.Substring(fromIdx, toIdx - fromIdx));
				}
				return Intrinsic.Result.Null;
			};
			
			// ========== sort ==========
			// 就地排序列表。使用null或无参数，这将按列表元素
			// 自己的值排序。使用byKey参数，每个元素都由该参数
			// 索引，元素按结果排序。（这仅在列表元素是映射时有效，
			// 或者它们是列表且byKey是整数索引时。）
			// self (list): 要排序的列表
			// byKey (optional): 如果给定，通过此键索引每个元素进行排序
			// ascending (optional, default true): 如果为false，按降序排序
			// 返回: self（已就地排序）
			// 示例: a = [5,3,4,1,2]; a.sort		结果a == [1, 2, 3, 4, 5]
			// 另见: shuffle
			f = Intrinsic.Create("sort");
			f.AddParam("self");
			f.AddParam("byKey");
			f.AddParam("ascending", ValNumber.one);
			f.code = (context, partialResult) => {
				Value self = context.self;
				ValList list = self as ValList;
				if (list == null || list.values.Count < 2) return new Intrinsic.Result(self);

				// IComparer<T>接口：定义比较逻辑
				// 选择升序或降序比较器
				IComparer<Value> sorter;
				if (context.GetVar("ascending").BoolValue()) sorter = ValueSorter.instance;
				else sorter = ValueReverseSorter.instance;

				Value byKey = context.GetLocal("byKey");
				if (byKey == null) {
					// 简单情况：按值本身排序
					// LINQ OrderBy: 返回排序后的IEnumerable
					// ToList: 转换回List
					list.values = list.values.OrderBy((arg) => arg, sorter).ToList();
				} else {
					// 较难情况：按键排序
					int count = list.values.Count;
					KeyedValue[] arr = new KeyedValue[count];
					for (int i=0; i<count; i++) {
						arr[i].value = list.values[i];
					}
					// 每个项的键将是项本身，除非它是映射，
					// 在这种情况下，它是由给定键索引的项。
					// （如果索引是整数，也适用于列表。）
					int byKeyInt = byKey.IntValue();
					for (int i=0; i<count; i++) {
						Value item = list.values[i];
						if (item is ValMap) arr[i].sortKey = ((ValMap)item).Lookup(byKey);
						else if (item is ValList) {
							ValList itemList = (ValList)item;
							if (byKeyInt > -itemList.values.Count && byKeyInt < itemList.values.Count) arr[i].sortKey = itemList.values[byKeyInt];
							else arr[i].sortKey = null;
						}
					}
					// 现在按键排序我们的键值列表
					var sortedArr = arr.OrderBy((arg) => arg.sortKey, sorter);
					// 最后，将其转换回我们的列表
					int idx=0;
					foreach (KeyedValue kv in sortedArr) {
						list.values[idx++] = kv.value;
					}
				}
				return new Intrinsic.Result(list);
			};

			// ========== split ==========
			// 通过某个分隔符将字符串拆分为列表。
			// 可以使用函数语法或点语法调用。
			// self (string): 要拆分的字符串
			// delimiter (string, default " "): 要拆分的子字符串
			// maxCount (number, default -1): 如果> 0，拆分为不超过这么多字符串
			// 返回: 通过在delimiter上拆分找到的子字符串列表
			// 示例: "foo bar baz".split		返回 ["foo", "bar", "baz"]
			// 示例: "foo bar baz".split("a", 2)		返回 ["foo b", "r baz"]
			// 另见: join
			f = Intrinsic.Create("split");
			f.AddParam("self");
			f.AddParam("delimiter", " ");
			f.AddParam("maxCount", -1);
			f.code = (context, partialResult) => {
				string self = context.self.ToString();
				string delim = context.GetLocalString("delimiter");
				int maxCount = context.GetLocalInt("maxCount");
				ValList result = new ValList();
				int pos = 0;
				while (pos < self.Length) {
					int nextPos;
					// 特殊情况：达到最大计数或空分隔符
					if (maxCount >= 0 && result.values.Count == maxCount - 1) nextPos = self.Length;
					else if (delim.Length == 0) nextPos = pos+1;	// 空分隔符：每个字符
					else nextPos = self.IndexOf(delim, pos, StringComparison.Ordinal);
					if (nextPos < 0) nextPos = self.Length;
					result.values.Add(new ValString(self.Substring(pos, nextPos - pos)));
					pos = nextPos + delim.Length;
					// 特殊情况：字符串末尾的分隔符产生空字符串
					if (pos == self.Length && delim.Length > 0) result.values.Add(ValString.empty);
				}
				return new Intrinsic.Result(result);
			};

			// ========== sqrt ==========
			// 返回数字的平方根。
			// x (number): 要获取平方根的数字
			// 返回: x的平方根
			// 示例: sqrt(1764)		返回 42
			f = Intrinsic.Create("sqrt");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Sqrt(context.GetLocalDouble("x")));
			};

			// ========== stackTrace ==========
			// 获取描述调用堆栈的列表。
			f = Intrinsic.Create("stackTrace");
			f.code = (context, partialResult) => {
				TAC.Machine vm = context.vm;
				var _stackAtBreak = new ValString("_stackAtBreak");
				// 检查是否有存储的堆栈（来自break或exit）
				if (vm.globalContext.variables.ContainsKey(_stackAtBreak)) {
					// 显示存储的堆栈
					// 宿主应用在开始'run'时清除它，以免干扰运行期间的最新堆栈
					return new Intrinsic.Result(vm.globalContext.variables.map[_stackAtBreak]);
				}
				// 否则，从虚拟机状态构建当前堆栈
				ValList result = StackList(vm);
				return new Intrinsic.Result(result);
			};

			// ========== str ==========
			// 将任何值转换为字符串。
			// x (any): 要转换的值
			// 返回: 给定值的字符串表示
			// 示例: str(42)		返回 "42"
			// 另见: val
			f = Intrinsic.Create("str");
			f.AddParam("x", ValString.empty);
			f.code = (context, partialResult) => {		
				var x = context.GetLocal("x");
				if (x == null) return new Intrinsic.Result(ValString.empty);
				return new Intrinsic.Result(x.ToString());
			};

			// ========== string 类型 ==========
			// 返回表示MiniScript核心类型系统中string数据类型的映射。
			// 这可以与`isa`一起使用，以检查变量是否引用字符串。
			// 您还可以在此处分配新方法，使其可用于所有字符串。
			// 示例: "Hello" isa string		返回 1
			// 另见: number, list, map, funcRef
			f = Intrinsic.Create("string");
			f.code = (context, partialResult) => {
				if (context.vm.stringType == null) {
					context.vm.stringType = StringType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.stringType);
			};

			// ========== shuffle ==========
			// 随机化列表中元素的顺序，或映射中从键到值的映射。
			// 这是就地完成的。
			// self (list or map): 要洗牌的对象
			// 返回: null
			f = Intrinsic.Create("shuffle");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.self;
				if (random == null) random = new Random();
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					// Fisher-Yates洗牌算法，即将每个元素
					// 与随机选择的元素交换。
					for (int i=list.Count-1; i >= 1; i--) {
						int j = random.Next(i+1);	// 0到i之间的随机索引
						Value temp = list[j];
						list[j] = list[i];
						list[i] = temp;
					}
				} else if (self is ValMap) {
					Dictionary<Value, Value> map = ((ValMap)self).map;
					// 再次使用Fisher-Yates，但这次交换的是
					// 与键关联的值，而不是键本身。
					List<Value> keys = System.Linq.Enumerable.ToList(map.Keys);
					for (int i=keys.Count-1; i >= 1; i--) {
						int j = random.Next(i+1);
						Value keyi = keys[i];
						Value keyj = keys[j];
						Value temp = map[keyj];
						map[keyj] = map[keyi];
						map[keyi] = temp;
					}
				}
				return Intrinsic.Result.Null;
			};

			// ========== sum ==========
			// 返回列表中所有元素或映射中所有值的总和。
			// self (list or map): 要求和的对象
			// 返回: 将self中所有值相加的结果
			// 示例: range(3).sum		返回 6 (3 + 2 + 1 + 0)
			f = Intrinsic.Create("sum");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.self;
				double sum = 0;
				if (val is ValList) {
					List<Value> list = ((ValList)val).values;
					foreach (Value v in list) {
						sum += v.DoubleValue();
					}
				} else if (val is ValMap) {
					Dictionary<Value, Value> map = ((ValMap)val).map;
					foreach (Value v in map.Values) {
						sum += v.DoubleValue();
					}
				}
				return new Intrinsic.Result(sum);
			};

			// ========== tan ==========
			// 返回给定角度（以弧度为单位）的正切值。
			// radians (number): 要获取正切值的角度（以弧度为单位）
			// 返回: 给定角度的正切值
			// 示例: tan(pi/4)		返回 1
			f = Intrinsic.Create("tan");
			f.AddParam("radians", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Tan(context.GetLocalDouble("radians")));
			};

			// ========== time ==========
			// 返回自脚本开始运行以来的秒数。
			f = Intrinsic.Create("time");
			f.code = (context, partialResult) => {
				// vm.runTime: 虚拟机跟踪的运行时间
				return new Intrinsic.Result(context.vm.runTime);
			};
			
			// ========== upper ==========
			// 返回字符串的大写（全部大写）版本。
			// 可以使用函数语法或点语法调用。
			// self (string): 要大写的字符串
			// 返回: 所有小写字母转换为大写的字符串
			// 示例: "Mo Spam".upper		返回 "MO SPAM"
			// 另见: lower
			f = Intrinsic.Create("upper");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.self;
				if (val is ValString) {
					string str = ((ValString)val).value;
					return new Intrinsic.Result(str.ToUpper());
				}
				return new Intrinsic.Result(val);
			};
			
			// ========== val ==========
			// 返回给定字符串的数值。（如果给定数字，
			// 原样返回；如果给定列表或映射，返回null。）
			// 可以使用函数语法或点语法调用。
			// self (string or number): 要获取值的字符串
			// 返回: 给定字符串的数值
			// 示例: "1234.56".val		返回 1234.56
			// 另见: str
			f = Intrinsic.Create("val");
			f.AddParam("self", 0);
			f.code = (context, partialResult) => {
				Value val = context.self;
				if (val is ValNumber) return new Intrinsic.Result(val);
				if (val is ValString) {
					double value = 0;
					// double.TryParse: 尝试解析字符串为double
					// NumberStyles.Any: 接受任何数字格式
					// CultureInfo.InvariantCulture: 使用不变文化（不依赖区域设置）
					double.TryParse(val.ToString(), NumberStyles.Any, CultureInfo.InvariantCulture, out value);
					return new Intrinsic.Result(value);
				}
				return Intrinsic.Result.Null;
			};

			// ========== values ==========
			// 返回字典的值或字符串的字符。
			// （将任何其他值原样返回。）
			// 可以使用函数语法或点语法调用。
			// self (any): 要获取值的对象
			// 示例: d={1:"one", 2:"two"}; d.values		返回 ["one", "two"]
			// 示例: "abc".values		返回 ["a", "b", "c"]
			// 另见: indexes
			f = Intrinsic.Create("values");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.self;
				if (self is ValMap) {
					ValMap map = (ValMap)self;
					// 从字典值集合创建列表
					List<Value> values = new List<Value>(map.map.Values);
					return new Intrinsic.Result(new ValList(values));
				} else if (self is ValString) {
					string str = ((ValString)self).value;
					List<Value> values = new List<Value>(str.Length);
					// 将每个字符转换为字符串值
					for (int i = 0; i < str.Length; i++) {
						values.Add(TAC.Str(str[i].ToString()));
					}
					return new Intrinsic.Result(new ValList(values));
				}
				return new Intrinsic.Result(self);
			};

			// ========== version ==========
			// 获取包含您当前运行的MiniScript版本和
			// 宿主环境信息的映射。这将至少包括以下键：
			//		miniscript: 字符串，如"1.5"
			//		buildDate: yyyy-mm-dd格式的日期，如"2020-05-28"
			//		host: 宿主主要和次要版本号，如0.9
			//		hostName: 宿主应用程序名称，例如"Mini Micro"
			//		hostInfo: 关于宿主应用的URL或其他简短信息
			f = Intrinsic.Create("version");
			f.code = (context, partialResult) => {
				if (context.vm.versionMap == null) {
					var d = new ValMap();
					d["miniscript"] = new ValString("1.6.2");
			
					// 在C#中获取构建日期异常困难。
					// 如果assembly.cs文件使用版本格式：1.0.*，这将起作用
					DateTime buildDate;
					// 反射(Reflection)说明：
					// - System.Reflection允许运行时检查程序集信息
					// - GetExecutingAssembly: 获取当前执行的程序集
					// - GetName: 获取程序集的AssemblyName
					// - Version: 包含构建和修订号
					System.Version version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
					buildDate = new DateTime(2000, 1, 1);
					buildDate = buildDate.AddDays(version.Build);	// Build是自2000-01-01以来的天数
					buildDate = buildDate.AddSeconds(version.Revision * 2);	// Revision是自午夜以来的2秒间隔数
					d["buildDate"] = new ValString(buildDate.ToString("yyyy-MM-dd"));

					d["host"] = new ValNumber(HostInfo.version);
					d["hostName"] = new ValString(HostInfo.name);
					d["hostInfo"] = new ValString(HostInfo.info);

					context.vm.versionMap = d;
				}
				return new Intrinsic.Result(context.vm.versionMap);
			};

			// ========== wait ==========
			// 暂停此脚本的执行一段时间。
			// seconds (default 1.0): 要等待多少秒
			// 示例: wait 2.5		暂停脚本2.5秒
			// 另见: time, yield
			// 
			// 异步执行说明：
			// - 首次调用：计算结束时间，返回未完成结果
			// - 后续调用：检查是否到达结束时间
			// - 这允许脚本在等待期间让出控制权
			f = Intrinsic.Create("wait");
			f.AddParam("seconds", 1);
			f.code = (context, partialResult) => {
				double now = context.vm.runTime;
				if (partialResult == null) {
					// 刚开始等待；计算结束时间并作为部分结果返回
					double interval = context.GetLocalDouble("seconds");
					return new Intrinsic.Result(new ValNumber(now + interval), false);
				} else {
					// 继续，直到当前时间超过部分结果中的时间
					if (now > partialResult.result.DoubleValue()) return Intrinsic.Result.Null;
					return partialResult;	// 仍在等待
				}
			};

			// ========== yield ==========
			// 暂停脚本的执行，直到宿主应用的下一个"tick"。
			// 例如在Mini Micro中，这将等待到下一个60Hz帧。
			// 确切含义可能有所不同，但通常如果您在紧密循环中
			// 执行某些操作，调用yield对宿主应用或其他脚本是有礼貌的。
			f = Intrinsic.Create("yield");
			f.code = (context, partialResult) => {
				// 设置yielding标志，让虚拟机知道要暂停执行
				context.vm.yielding = true;
				return Intrinsic.Result.Null;
			};

		}

		// Random实例，用于rnd和shuffle函数
		// TODO: 考虑将其存储在上下文中，而不是全局！
		static Random random;


		/// <summary>
		/// 辅助方法，用于编译对Slice的调用（当通过切片语法直接调用时）。
		/// 
		/// 编译器集成说明：
		/// - 此方法由编译器调用，将切片语法转换为函数调用
		/// - 例如：list[1:3] 被编译为 slice(list, 1, 3)
		/// - 生成TAC（三地址码）指令序列
		/// </summary>
		public static void CompileSlice(List<TAC.Line> code, Value list, Value fromIdx, Value toIdx, int resultTempNum) {
			// 推送参数到栈（逆序）
			code.Add(new TAC.Line(null, TAC.Line.Op.PushParam, list));
			code.Add(new TAC.Line(null, TAC.Line.Op.PushParam, fromIdx == null ? TAC.Num(0) : fromIdx));
			code.Add(new TAC.Line(null, TAC.Line.Op.PushParam, toIdx));
			// 获取slice函数并调用
			ValFunction func = Intrinsic.GetByName("slice").GetFunc();
			// CallFunctionA: 调用函数，参数数量为3
			// 结果存储在临时变量resultTempNum中
			code.Add(new TAC.Line(TAC.LTemp(resultTempNum), TAC.Line.Op.CallFunctionA, func, TAC.Num(3)));
		}
		
		/// <summary>
		/// FunctionType: 表示Function类型的静态映射。
		/// 
		/// 类型系统说明：
		/// - MiniScript的类型系统基于原型
		/// - 每种类型由一个映射表示
		/// - 可以向类型添加方法，使其可用于该类型的所有实例
		/// </summary>
		public static ValMap FunctionType() {
			if (_functionType == null) {
				_functionType = new ValMap();
			}
			return _functionType;
		}
		static ValMap _functionType = null;
		
		/// <summary>
		/// ListType: 表示List类型的静态映射，并提供
		/// 可以通过点语法在其上调用的内置方法。
		/// 
		/// 方法绑定说明：
		/// - 将内置函数绑定到类型映射
		/// - 脚本中可以使用 list.method() 语法调用
		/// - 例如：[1,2,3].len 会调用len内置函数
		/// </summary>
		public static ValMap ListType() {
			if (_listType == null) {
				_listType = new ValMap();
				// 绑定所有列表方法
				_listType["hasIndex"] = Intrinsic.GetByName("hasIndex").GetFunc();
				_listType["indexes"] = Intrinsic.GetByName("indexes").GetFunc();
				_listType["indexOf"] = Intrinsic.GetByName("indexOf").GetFunc();
				_listType["insert"] = Intrinsic.GetByName("insert").GetFunc();
				_listType["join"] = Intrinsic.GetByName("join").GetFunc();
				_listType["len"] = Intrinsic.GetByName("len").GetFunc();
				_listType["pop"] = Intrinsic.GetByName("pop").GetFunc();
				_listType["pull"] = Intrinsic.GetByName("pull").GetFunc();
				_listType["push"] = Intrinsic.GetByName("push").GetFunc();
				_listType["shuffle"] = Intrinsic.GetByName("shuffle").GetFunc();
				_listType["sort"] = Intrinsic.GetByName("sort").GetFunc();
				_listType["sum"] = Intrinsic.GetByName("sum").GetFunc();
				_listType["remove"] = Intrinsic.GetByName("remove").GetFunc();
				_listType["replace"] = Intrinsic.GetByName("replace").GetFunc();
				_listType["values"] = Intrinsic.GetByName("values").GetFunc();
			}
			return _listType;
		}
		static ValMap _listType = null;
		
		/// <summary>
		/// StringType: 表示String类型的静态映射，并提供
		/// 可以通过点语法在其上调用的内置方法。
		/// </summary>
		public static ValMap StringType() {
			if (_stringType == null) {
				_stringType = new ValMap();
				// 绑定所有字符串方法
				_stringType["hasIndex"] = Intrinsic.GetByName("hasIndex").GetFunc();
				_stringType["indexes"] = Intrinsic.GetByName("indexes").GetFunc();
				_stringType["indexOf"] = Intrinsic.GetByName("indexOf").GetFunc();
				_stringType["insert"] = Intrinsic.GetByName("insert").GetFunc();
				_stringType["code"] = Intrinsic.GetByName("code").GetFunc();
				_stringType["len"] = Intrinsic.GetByName("len").GetFunc();
				_stringType["lower"] = Intrinsic.GetByName("lower").GetFunc();
				_stringType["val"] = Intrinsic.GetByName("val").GetFunc();
				_stringType["remove"] = Intrinsic.GetByName("remove").GetFunc();
				_stringType["replace"] = Intrinsic.GetByName("replace").GetFunc();
				_stringType["split"] = Intrinsic.GetByName("split").GetFunc();
				_stringType["upper"] = Intrinsic.GetByName("upper").GetFunc();
				_stringType["values"] = Intrinsic.GetByName("values").GetFunc();
			}
			return _stringType;
		}
		static ValMap _stringType = null;
		
		/// <summary>
		/// MapType: 表示Map类型的静态映射，并提供
		/// 可以通过点语法在其上调用的内置方法。
		/// </summary>
		public static ValMap MapType() {
			if (_mapType == null) {
				_mapType = new ValMap();
				// 绑定所有映射方法
				_mapType["hasIndex"] = Intrinsic.GetByName("hasIndex").GetFunc();
				_mapType["indexes"] = Intrinsic.GetByName("indexes").GetFunc();
				_mapType["indexOf"] = Intrinsic.GetByName("indexOf").GetFunc();
				_mapType["len"] = Intrinsic.GetByName("len").GetFunc();
				_mapType["pop"] = Intrinsic.GetByName("pop").GetFunc();
				_mapType["push"] = Intrinsic.GetByName("push").GetFunc();
				_mapType["pull"] = Intrinsic.GetByName("pull").GetFunc();
				_mapType["shuffle"] = Intrinsic.GetByName("shuffle").GetFunc();
				_mapType["sum"] = Intrinsic.GetByName("sum").GetFunc();
				_mapType["remove"] = Intrinsic.GetByName("remove").GetFunc();
				_mapType["replace"] = Intrinsic.GetByName("replace").GetFunc();
				_mapType["values"] = Intrinsic.GetByName("values").GetFunc();
			}
			return _mapType;
		}
		static ValMap _mapType = null;
		
		/// <summary>
		/// NumberType: 表示Number类型的静态映射。
		/// 
		/// 注意：
		/// - 数字类型通常没有方法（由于解析器限制）
		/// - 但保留此映射以保持类型系统一致性
		/// </summary>
		public static ValMap NumberType() {
			if (_numberType == null) {
				_numberType = new ValMap();
			}
			return _numberType;
		}
		static ValMap _numberType = null;
		
		
	}
}
