/*	MiniscriptTypes.cs

Classes in this file represent the MiniScript type system.  Value is the 
abstract base class for all of them (i.e., represents ANY value in MiniScript),
from which more specific types are derived.

此文件中的类表示MiniScript类型系统。Value是所有类型的抽象基类
(即表示MiniScript中的任意值),所有具体类型都派生自它。

*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Globalization;

namespace Miniscript {
	
	/// <summary>
	/// Value: abstract base class for the MiniScript type hierarchy.
	/// Defines a number of handy methods that you can call on ANY
	/// value (though some of these do nothing for some types).
	/// 
	/// Value: MiniScript类型层次结构的抽象基类。
	/// 定义了一系列便捷方法,可以在任何值上调用
	/// (尽管某些方法对某些类型不做任何操作)。
	/// 
	/// 设计说明:
	/// - abstract关键字表示这是抽象类,不能直接实例化,只能被继承
	/// - 提供了类型系统的统一接口
	/// - 所有子类必须实现ToString、Hash、Equality等核心方法
	/// </summary>
	public abstract class Value {
		/// <summary>
		/// Get the current value of this Value in the given context.  Basic types
		/// evaluate to themselves, but some types (e.g. variable references) may
		/// evaluate to something else.
		/// 
		/// 在给定上下文中获取此Value的当前值。基础类型求值为自身,
		/// 但某些类型(例如变量引用)可能求值为其他内容。
		/// 
		/// 参数说明:
		/// @param context - TAC上下文,提供变量查找和执行环境
		/// @return 此值的求值结果(可能与this相同)
		/// 
		/// virtual关键字说明:
		/// - virtual表示虚方法,可以在派生类中被override重写
		/// - 提供了默认实现(返回自身),子类可以选择性覆盖
		/// </summary>
		public virtual Value Val(TAC.Context context) {
			return this;		// most types evaluate to themselves
			                    // 大多数类型求值为自身
		}
		
		/// <summary>
		/// 重写Object基类的ToString方法,调用带虚拟机参数的版本
		/// 
		/// override关键字说明:
		/// - override表示重写父类(Object)的方法
		/// - ToString是C#所有对象都有的基础方法
		/// </summary>
		public override string ToString() {
			return ToString(null);
		}
		
		/// <summary>
		/// 抽象方法:将此值转换为字符串表示
		/// 
		/// abstract关键字说明:
		/// - abstract方法没有实现体,必须在派生类中实现
		/// - 强制所有具体类型提供自己的字符串表示逻辑
		/// 
		/// @param vm - 虚拟机实例,用于查找值的简短名称等
		/// </summary>
		public abstract string ToString(TAC.Machine vm);
		
		/// <summary>
		/// This version of Val is like the one above, but also returns
		/// (via the output parameter) the ValMap the value was found in,
		/// which could be several steps up the __isa chain.
		/// 
		/// 此版本的Val类似上面的版本,但还通过输出参数返回找到该值的ValMap,
		/// 该map可能位于__isa链的上游几步。
		/// 
		/// out参数说明:
		/// - out是C#的输出参数,方法必须为其赋值
		/// - 允许方法返回多个值(一个通过return,一个通过out)
		/// - 调用者必须声明变量来接收out参数,但不需要初始化
		/// 
		/// @param context - TAC执行上下文
		/// @param valueFoundIn - 输出参数,返回找到值的映射对象
		/// @return 求值后的值
		/// </summary>
		public virtual Value Val(TAC.Context context, out ValMap valueFoundIn) {
			valueFoundIn = null;
			return this;
		}
		
		/// <summary>
		/// Similar to Val, but recurses into the sub-values contained by this
		/// value (if it happens to be a container, such as a list or map).
		/// 
		/// 类似于Val,但会递归到此值包含的子值中
		/// (如果它恰好是容器,如列表或映射)。
		/// 
		/// 功能说明:
		/// - 完全求值,包括嵌套的容器内容
		/// - 用于彻底解析所有变量引用和临时值
		/// 
		/// @param context - 求值上下文
		/// @return 完全求值后的值
		/// </summary>
		public virtual Value FullEval(TAC.Context context) {
			return this;
		}
		
		/// <summary>
		/// Get the numeric value of this Value as an integer.
		/// 获取此Value的整数数值。
		/// 
		/// 实现说明:
		/// - 默认通过DoubleValue()转换后再转为int
		/// - 会发生截断(舍去小数部分)
		/// 
		/// @return 此值转为有符号整数
		/// </summary>
		public virtual int IntValue() {
			return (int)DoubleValue();
		}
		
		/// <summary>
		/// Get the numeric value of this Value as an unsigned integer.
		/// 获取此Value的无符号整数数值。
		/// 
		/// @return 此值转为无符号整数
		/// </summary>
		public virtual uint UIntValue() {
			return (uint)DoubleValue();
		}

		/// <summary>
		/// Get the numeric value of this Value as a single-precision float.
		/// 获取此Value的单精度浮点数数值。
		/// 
		/// @return 此值转为float
		/// </summary>
		public virtual float FloatValue() {
			return (float)DoubleValue();
		}
		
		/// <summary>
		/// Get the numeric value of this Value as a double-precision floating-point number.
		/// 获取此Value的双精度浮点数数值。
		/// 
		/// 默认实现:
		/// - 返回0,因为大多数类型没有数值
		/// - ValNumber会覆盖此方法返回实际数值
		/// 
		/// @return 此值转为double
		/// </summary>
		public virtual double DoubleValue() {
			return 0;				// most types don't have a numeric value
			                        // 大多数类型没有数值
		}
		
		/// <summary>
		/// Get the boolean (truth) value of this Value.  By default, we consider
		/// any numeric value other than zero to be true.  (But subclasses override
		/// this with different criteria for strings, lists, and maps.)
		/// 
		/// 获取此Value的布尔(真值)值。默认情况下,我们认为
		/// 任何非零数值为真。(但子类会用不同的标准覆盖此方法,
		/// 如字符串、列表和映射)。
		/// 
		/// 真值判定规则:
		/// - 数字: 非零为真
		/// - 字符串: 非空为真
		/// - 列表/映射: 非空为真
		/// - null: 假
		/// - 函数: 总是真
		/// 
		/// @return 此值转为bool
		/// </summary>
		public virtual bool BoolValue() {
			return IntValue() != 0;
		}
		
		/// <summary>
		/// Get this value in the form of a MiniScript literal.
		/// 获取此值的MiniScript字面量形式。
		/// 
		/// 功能说明:
		/// - 生成可以直接粘贴到MiniScript代码中的表示
		/// - 例如: 字符串会加引号,列表用方括号等
		/// 
		/// @param vm - 虚拟机实例
		/// @param recursionLimit - 递归深度限制,-1表示无限制
		/// @return MiniScript代码形式的字符串
		/// </summary>
		public virtual string CodeForm(TAC.Machine vm, int recursionLimit=-1) {
			return ToString(vm);
		}
		
		/// <summary>
		/// Get a hash value for this Value.  Two values that are considered
		/// equal will return the same hash value.
		/// 
		/// 获取此Value的哈希值。两个被认为相等的值将返回相同的哈希值。
		/// 
		/// 哈希值用途:
		/// - 用于字典的键
		/// - 提高查找性能
		/// - 必须与Equality方法保持一致
		/// 
		/// @return 哈希值
		/// </summary>
		public abstract int Hash();
		
		/// <summary>
		/// Check whether this Value is equal to another Value.
		/// 检查此Value是否等于另一个Value。
		/// 
		/// 返回值说明:
		/// - 1表示相等
		/// - 0表示不相等
		/// - 0.5表示不确定(模糊逻辑,MiniScript特性)
		/// 
		/// @param rhs - 要比较的另一个值(right-hand side右侧值)
		/// @return 相等程度(0到1之间)
		/// </summary>
		public abstract double Equality(Value rhs);
		
		/// <summary>
		/// Can we set elements within this value?  (I.e., is it a list or map?)
		/// 我们能否设置此值中的元素?(即,它是列表还是映射?)
		/// 
		/// @return 如果SetElem可以工作返回true;如果它什么都不做返回false
		/// </summary>
		public virtual bool CanSetElem() { return false; }
		
		/// <summary>
		/// Set an element associated with the given index within this Value.
		/// 设置此Value中与给定索引关联的元素。
		/// 
		/// 功能说明:
		/// - 默认实现为空(什么都不做)
		/// - 只有容器类型(列表、映射)会实现此方法
		/// 
		/// @param index - 要设置的值的索引/键
		/// @param value - 要设置的值
		/// </summary>
		public virtual void SetElem(Value index, Value value) {}

		/// <summary>
		/// Return whether this value is the given type (or some subclass thereof)
		/// in the context of the given virtual machine.
		/// 
		/// 返回此值在给定虚拟机的上下文中是否为给定类型(或其某个子类)。
		/// 
		/// 类型检查说明:
		/// - 支持MiniScript的动态类型系统
		/// - 通过__isa链检查继承关系
		/// 
		/// @param type - 要检查的类型
		/// @param vm - 虚拟机实例
		/// @return 是否为该类型
		/// </summary>
		public virtual bool IsA(Value type, TAC.Machine vm) {
			return false;
		}

		/// <summary>
		/// Compare two Values for sorting purposes.
		/// 比较两个Value以用于排序。
		/// 
		/// static方法说明:
		/// - static表示静态方法,属于类而非实例
		/// - 可以直接通过类名调用: Value.Compare(x, y)
		/// - 不能访问实例成员(this)
		/// 
		/// 排序规则:
		/// 1. null总是排在最后
		/// 2. 如果任一参数是字符串,进行字符串比较
		/// 3. 如果两个都是数字,进行数值比较
		/// 4. 否则,认为所有值相等(排序目的)
		/// 
		/// @param x - 第一个值
		/// @param y - 第二个值
		/// @return 负数(x<y), 0(x==y), 正数(x>y)
		/// </summary>
		public static int Compare(Value x, Value y) {
			// Always sort null to the end of the list.
			// 总是将null排序到列表末尾。
			if (x == null) {
				if (y == null) return 0;
				return 1;
            }
			if (y == null) return -1;
			
			// If either argument is a string, do a string comparison
			// 如果任一参数是字符串,进行字符串比较
			// is关键字说明: 用于类型检查,相当于 x.GetType() == typeof(ValString)
			if (x is ValString || y is ValString) {
				var sx = x.ToString();
				var sy = y.ToString();
				return sx.CompareTo(sy);  // CompareTo是C#字符串的标准比较方法
			}
			
			// If both arguments are numbers, compare numerically
			// 如果两个参数都是数字,进行数值比较
			if (x is ValNumber && y is ValNumber) {
				double fx = ((ValNumber)x).value;  // 强制类型转换
				double fy = ((ValNumber)y).value;
				if (fx < fy) return -1;
				if (fx > fy) return 1;
				return 0;
			}
			
			// Otherwise, consider all values equal, for sorting purposes.
			// 否则,出于排序目的,认为所有值相等。
			return 0;
		}

		/// <summary>
		/// 旋转整数的位,用于哈希计算
		/// 
		/// 位运算说明:
		/// - >> 右移运算符: 将所有位向右移动
		/// - << 左移运算符: 将所有位向左移动
		/// - | 按位或运算符: 合并两个位模式
		/// - sizeof(int) * 8: 获取int的位数(通常是32位)
		/// 
		/// 功能说明:
		/// - 将最右边的位移到最左边,其余位右移一位
		/// - 用于在哈希计算中混合位,增加哈希质量
		/// 
		/// @param n - 要旋转的整数
		/// @return 旋转后的整数
		/// </summary>
		private int RotateBits(int n) {
			return (n >> 1) | (n << (sizeof(int) * 8 - 1));
		}

		/// <summary>
		/// Compare lhs and rhs for equality, in a way that traverses down
		/// the tree when it finds a list or map.  For any other type, this
		/// just calls through to the regular Equality method.
		///
		/// Note that this works correctly for loops (maintaining a visited
		/// list to avoid recursing indefinitely).
		/// 
		/// 比较左侧值和右侧值的相等性,当发现列表或映射时遍历树结构。
		/// 对于其他类型,这只是调用常规的Equality方法。
		/// 
		/// 注意:这对循环正确工作(维护已访问列表以避免无限递归)。
		/// 
		/// 算法说明:
		/// - 使用栈实现非递归深度优先遍历
		/// - 使用HashSet跟踪已访问的值对,避免循环引用导致的无限循环
		/// - 支持嵌套的列表和映射比较
		/// 
		/// Stack<T>说明:
		/// - 后进先出(LIFO)的集合
		/// - Push(): 压入元素
		/// - Pop(): 弹出并返回顶部元素
		/// - Count: 获取元素数量
		/// 
		/// HashSet<T>说明:
		/// - 不允许重复元素的集合
		/// - Contains(): 检查是否包含元素
		/// - Add(): 添加元素
		/// - 提供O(1)的查找性能
		/// 
		/// @param rhs - 要比较的右侧值
		/// @return 是否相等
		/// </summary>
		protected bool RecursiveEqual(Value rhs) { 
			// 创建工作栈和已访问集合
			var toDo = new Stack<ValuePair>();
			var visited = new HashSet<ValuePair>();
			toDo.Push(new ValuePair() { a = this, b = rhs });  // 对象初始化器语法
			
			// 循环处理栈中的所有值对
			while (toDo.Count > 0) {
				var pair = toDo.Pop();

				visited.Add(pair);
				
				// 如果左侧是列表
				if (pair.a is ValList listA) {  // is关键字+模式匹配,声明并转换
					var listB = pair.b as ValList;  // as关键字:安全类型转换,失败返回null
					if (listB == null) return false;  // 类型不匹配
					
					// Object.ReferenceEquals: 检查是否为同一对象引用
					if (Object.ReferenceEquals(listA, listB)) continue;  // 同一个列表,跳过
					
					int aCount = listA.values.Count;
					if (aCount != listB.values.Count) return false;  // 长度不同
					
					// 比较每个元素
					for (int i = 0; i < aCount; i++) {
						var newPair = new ValuePair() {  a = listA.values[i], b = listB.values[i] };
						if (!visited.Contains(newPair)) toDo.Push(newPair);
					}
					
				// 如果左侧是映射
				} else if (pair.a is ValMap mapA) {
					var mapB = pair.b as ValMap;
					if (mapB == null) return false;
					if (Object.ReferenceEquals(mapA, mapB)) continue;
					if (mapA.map.Count != mapB.map.Count) return false;
					
					// foreach说明: 遍历集合的简化语法
					// KeyValuePair<TKey, TValue>: 字典的键值对结构
					foreach (KeyValuePair<Value, Value> kv in mapA.map) {
						Value valFromB;
						// TryGetValue: 尝试获取值,成功返回true并输出值
						if (!mapB.TryGetValue(kv.Key, out valFromB)) return false;
						Value valFromA = mapA.map[kv.Key];
						var newPair = new ValuePair() {  a = valFromA, b = valFromB };
						if (!visited.Contains(newPair)) toDo.Push(newPair);
					}
					
				// 处理null情况
				} else if (pair.a == null || pair.b == null) {
					if (pair.a != null || pair.b != null) return false;
					
				// 其他类型直接比较
				} else {
					// No other types can recurse, so we can safely do:
					// 其他类型不能递归,所以我们可以安全地这样做:
					if (pair.a.Equality(pair.b) == 0) return false;
				}
			}
			
			// If we clear out our toDo list without finding anything unequal,
			// then the values as a whole must be equal.
			// 如果我们清空了待办列表而没有发现任何不相等的内容,
			// 那么整体上这些值必定相等。
			return true;
		}

		/// <summary>
		/// Hash function that works correctly with nested lists and maps.
		/// 对嵌套列表和映射正确工作的哈希函数。
		/// 
		/// 算法说明:
		/// - 类似RecursiveEqual,使用栈进行非递归遍历
		/// - 使用位旋转混合哈希值
		/// - 避免循环引用导致的无限循环
		/// 
		/// @return 哈希值
		/// </summary>
		protected int RecursiveHash()
		{
			int result = 0;
			var toDo = new Stack<Value>();
			var visited = new HashSet<Value>();
			toDo.Push(this);
			
			while (toDo.Count > 0) {
				Value item = toDo.Pop();
				visited.Add(item);
				
				if (item is ValList list) {
					// 混合列表长度的哈希值
					result = RotateBits(result) ^ list.values.Count.GetHashCode();
					
					// 倒序遍历以保持栈的正确顺序
					for (int i=list.values.Count-1; i>=0; i--) {
						Value child = list.values[i];
						// 只处理未访问的容器类型,避免循环
						if (!(child is ValList || child is ValMap) || !visited.Contains(child)) {
							toDo.Push(child);
						}
					}
					
				} else  if (item is ValMap map) {
					result = RotateBits(result) ^ map.map.Count.GetHashCode();
					foreach (KeyValuePair<Value, Value> kv in map.map) {
						// 同时处理键和值
						if (!(kv.Key is ValList || kv.Key is ValMap) || !visited.Contains(kv.Key)) {
							toDo.Push(kv.Key);
						}
						if (!(kv.Value is ValList || kv.Value is ValMap) || !visited.Contains(kv.Value)) {
							toDo.Push(kv.Value);
						}
					}
					
				} else {
					// Anything else, we can safely use the standard hash method
					// 其他任何类型,我们可以安全地使用标准哈希方法
					// ^是异或运算符,用于混合哈希值
					result = RotateBits(result) ^ (item == null ? 0 : item.Hash());
				}
			}
			return result;
		}
	}

	/// <summary>
	/// ValuePair: used internally when working out whether two maps
	/// or lists are equal.
	/// 
	/// ValuePair: 在判断两个映射或列表是否相等时内部使用。
	/// 
	/// struct说明:
	/// - struct是值类型,存储在栈上(而class是引用类型,存储在堆上)
	/// - 更轻量级,适合小型数据结构
	/// - 按值传递,而不是按引用传递
	/// 
	/// 用途:
	/// - 作为HashSet的元素,跟踪已比较的值对
	/// - 避免在相等性比较中出现无限循环
	/// </summary>
	struct ValuePair {
		public Value a;
		public Value b;
		
		/// <summary>
		/// 重写Equals方法以支持HashSet的正确工作
		/// 
		/// @param obj - 要比较的对象
		/// @return 是否相等
		/// </summary>
		public override bool Equals(object obj) {
			if (obj is ValuePair other) {  // 模式匹配
				// ReferenceEquals: 比较引用是否相同,而不是值是否相等
				return ReferenceEquals(a, other.a) && ReferenceEquals(b, other.b);
			}
			return false;
		}

		/// <summary>
		/// 重写GetHashCode以支持HashSet的正确工作
		/// 
		/// unchecked说明:
		/// - 禁用整数溢出检查
		/// - 在哈希计算中溢出是可接受的(甚至有用的)
		/// 
		/// @return 哈希码
		/// </summary>
		public override int GetHashCode() {
			unchecked {
				// 397是一个质数,有助于减少哈希冲突
				return ((a != null ? a.GetHashCode() : 0) * 397) ^ (b != null ? b.GetHashCode() : 0);
			}
		}
	}

	/// <summary>
	/// ValueSorter: 实现IComparer接口的值排序器
	/// 
	/// IComparer<T>接口说明:
	/// - 定义类型特定的比较方法
	/// - 用于自定义集合的排序行为
	/// - 必须实现Compare方法
	/// 
	/// 用途:
	/// - 用于列表的sort方法
	/// - 提供MiniScript值的标准排序逻辑
	/// </summary>
	public class ValueSorter : IComparer<Value>
	{
		// 单例模式: 提供一个全局共享的实例
		public static ValueSorter instance = new ValueSorter();
		
		/// <summary>
		/// 实现IComparer接口的Compare方法
		/// 
		/// @param x - 第一个值
		/// @param y - 第二个值
		/// @return 比较结果
		/// </summary>
		public int Compare(Value x, Value y)
		{
			return Value.Compare(x, y);
		}
	}

	/// <summary>
	/// ValueReverseSorter: 反向排序器
	/// 
	/// 功能说明:
	/// - 与ValueSorter相同,但颠倒比较顺序
	/// - 用于降序排序
	/// </summary>
	public class ValueReverseSorter : IComparer<Value>
	{
		public static ValueReverseSorter instance = new ValueReverseSorter();
		
		public int Compare(Value x, Value y)
		{
			// 交换参数顺序实现反向排序
			return Value.Compare(y, x);
		}
	}

	/// <summary>
	/// ValNull is an object to represent null in places where we can't use
	/// an actual null (such as a dictionary key or value).
	/// 
	/// ValNull是一个对象,用于在不能使用实际null的地方表示null
	/// (例如作为字典的键或值)。
	/// 
	/// 设计说明:
	/// - C#中null不能作为字典的键,所以需要一个对象表示
	/// - 使用单例模式,全局只有一个实例
	/// - 所有方法都返回null相关的值
	/// </summary>
	public class ValNull : Value {
		// private构造函数: 防止外部创建实例,强制使用单例
		private ValNull() {}
		
		/// <summary>
		/// 返回字符串表示"null"
		/// </summary>
		public override string ToString(TAC.Machine machine) {
			return "null";
		}
		
		/// <summary>
		/// 类型检查: null只能是null类型
		/// </summary>
		public override bool IsA(Value type, TAC.Machine vm) {
			return type == null;
		}

		/// <summary>
		/// null的哈希值固定为-1
		/// </summary>
		public override int Hash() {
			return -1;
		}

		/// <summary>
		/// 求值返回实际的null(C#的null)
		/// </summary>
		public override Value Val(TAC.Context context) {
			return null;
		}

		/// <summary>
		/// 带输出参数的求值版本
		/// </summary>
		public override Value Val(TAC.Context context, out ValMap valueFoundIn) {
			valueFoundIn = null;
			return null;
		}
		
		/// <summary>
		/// 完全求值返回null
		/// </summary>
		public override Value FullEval(TAC.Context context) {
			return null;
		}
		
		/// <summary>
		/// null的整数值为0
		/// </summary>
		public override int IntValue() {
			return 0;
		}

		/// <summary>
		/// null的浮点值为0.0
		/// </summary>
		public override double DoubleValue() {
			return 0.0;
		}
		
		/// <summary>
		/// null的布尔值为false
		/// </summary>
		public override bool BoolValue() {
			return false;
		}

		/// <summary>
		/// 相等性检查: 只与null或ValNull相等
		/// </summary>
		public override double Equality(Value rhs) {
			return (rhs == null || rhs is ValNull ? 1 : 0);
		}

		// 静态只读字段: 全局唯一的ValNull实例
		static readonly ValNull _inst = new ValNull();
		
		/// <summary>
		/// Handy accessor to a shared "instance".
		/// 方便访问共享"实例"的访问器。
		/// 
		/// 属性(Property)说明:
		/// - get关键字定义获取器
		/// - 提供类似字段的访问语法,但实际是方法调用
		/// - 只读属性(没有set)
		/// </summary>
		public static ValNull instance { get { return _inst; } }
		
	}
	
	/// <summary>
	/// ValNumber represents a numeric (double-precision floating point) value in MiniScript.
	/// Since we also use numbers to represent boolean values, ValNumber does that job too.
	/// 
	/// ValNumber表示MiniScript中的数值(双精度浮点数)值。
	/// 由于我们也使用数字来表示布尔值,所以ValNumber也承担该工作。
	/// 
	/// 设计说明:
	/// - 使用double作为底层存储,精度足够且性能好
	/// - 布尔值表示为0(假)和1(真)
	/// - 提供到各种数值类型的转换
	/// </summary>
	public class ValNumber : Value {
		public double value;  // 存储实际的数值

		/// <summary>
		/// 构造函数: 创建一个新的数字值
		/// 
		/// @param value - 要存储的数值
		/// </summary>
		public ValNumber(double value) {
			this.value = value;  // this关键字: 引用当前对象
		}

		/// <summary>
		/// 转换为字符串,使用MiniScript的标准格式
		/// 
		/// 格式化规则:
		/// 1. 整数: 显示为整数(如 "42")
		/// 2. 非常大/小的数: 科学计数法(如 "1.234567E10")
		/// 3. 其他: 小数形式,1-6位小数(如 "3.14159")
		/// 
		/// CultureInfo.InvariantCulture说明:
		/// - 确保数字格式不受系统区域设置影响
		/// - 总是使用点作为小数分隔符(而不是逗号)
		/// - 保证跨平台一致性
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			// Convert to a string in the standard MiniScript way.
			// 按照标准MiniScript方式转换为字符串。
			
			if (value % 1.0 == 0.0) {
				// integer values as integers
				// 整数值显示为整数
				string result = value.ToString("0", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";  // 避免显示"-0"
				return result;
				
			} else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
				// very large/small numbers in exponential form
				// 非常大/小的数字使用指数形式
				string s = value.ToString("E6", CultureInfo.InvariantCulture);
				s = s.Replace("E-00", "E-0");  // 美化指数部分
				return s;
				
			} else {
				// all others in decimal form, with 1-6 digits past the decimal point;
				// and take care not to display "-0" for "negative" 0.0
				// 所有其他数字使用小数形式,小数点后1-6位;
				// 注意不要为"负"0.0显示"-0"
				string result = value.ToString("0.0#####", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";
				return result;
			}
		}

		/// <summary>
		/// 转换为整数(截断小数部分)
		/// </summary>
		public override int IntValue() {
			return (int)value;
		}

		/// <summary>
		/// 返回实际的double值
		/// </summary>
		public override double DoubleValue() {
			return value;
		}
		
		/// <summary>
		/// 转换为布尔值: 任何非零值为真
		/// </summary>
		public override bool BoolValue() {
			// Any nonzero value is considered true, when treated as a bool.
			// 当作为布尔值处理时,任何非零值都被认为是真。
			return value != 0;
		}

		/// <summary>
		/// 类型检查: 是否为number类型
		/// </summary>
		public override bool IsA(Value type, TAC.Machine vm) {
			if (type == null) return false;
			return type == vm.numberType;
		}

		/// <summary>
		/// 计算哈希值(使用.NET的double哈希算法)
		/// </summary>
		public override int Hash() {
			return value.GetHashCode();
		}

		/// <summary>
		/// 相等性检查: 必须都是数字且值相等
		/// </summary>
		public override double Equality(Value rhs) {
			return rhs is ValNumber && ((ValNumber)rhs).value == value ? 1 : 0;
		}

		// 缓存常用值的静态实例,避免重复创建
		static ValNumber _zero = new ValNumber(0), _one = new ValNumber(1);
		
		/// <summary>
		/// Handy accessor to a shared "zero" (0) value.
		/// IMPORTANT: do not alter the value of the object returned!
		/// 
		/// 方便访问共享的"零"(0)值的访问器。
		/// 重要: 不要修改返回对象的值!
		/// </summary>
		public static ValNumber zero { get { return _zero; } }
		
		/// <summary>
		/// Handy accessor to a shared "one" (1) value.
		/// IMPORTANT: do not alter the value of the object returned!
		/// 
		/// 方便访问共享的"一"(1)值的访问器。
		/// 重要: 不要修改返回对象的值!
		/// </summary>
		public static ValNumber one { get { return _one; } }
		
		/// <summary>
		/// Convenience method to get a reference to zero or one, according
		/// to the given boolean.  (Note that this only covers Boolean
		/// truth values; MiniScript also allows fuzzy truth values, like
		/// 0.483, but obviously this method won't help with that.)
		/// IMPORTANT: do not alter the value of the object returned!
		/// 
		/// 便捷方法,根据给定的布尔值获取零或一的引用。
		/// (注意这只涵盖布尔真值; MiniScript也允许模糊真值,
		/// 如0.483,但显然这个方法对此无能为力。)
		/// 重要: 不要修改返回对象的值!
		/// 
		/// @param truthValue - 返回1(真)还是0(假)
		/// @return ValNumber.one或ValNumber.zero
		/// </summary>
		public static ValNumber Truth(bool truthValue) {
			return truthValue ? one : zero;  // 三元运算符: condition ? true_value : false_value
		}
		
		/// <summary>
		/// Basically this just makes a ValNumber out of a double,
		/// BUT it is optimized for the case where the given value
		///	is either 0 or 1 (as is usually the case with truth tests).
		/// 
		/// 基本上这只是从double创建一个ValNumber,
		/// 但它针对给定值为0或1的情况进行了优化
		/// (这通常是真值测试的情况)。
		/// 
		/// 优化说明:
		/// - 对于0和1,返回缓存的实例,避免创建新对象
		/// - 减少垃圾回收压力
		/// - 提高性能
		/// 
		/// @param truthValue - 真值(通常是0.0或1.0)
		/// @return 对应的ValNumber
		/// </summary>
		public static ValNumber Truth(double truthValue) {
			if (truthValue == 0.0) return zero;
			if (truthValue == 1.0) return one;
			return new ValNumber(truthValue);  // new关键字: 创建新对象
		}
	}
	
	/// <summary>
	/// ValString represents a string (text) value.
	/// ValString表示字符串(文本)值。
	/// 
	/// 设计说明:
	/// - 包装C#的string类型
	/// - 字符串是不可变的(immutable)
	/// - 提供索引访问(当作字符数组)
	/// </summary>
	public class ValString : Value {
		// 字符串最大长度限制(约16MB)
		public static long maxSize = 0xFFFFFF;		// about 16M elements
		                                            // 约1600万个元素
		
		public string value;  // 存储实际的字符串

		/// <summary>
		/// 构造函数: 创建字符串值
		/// 
		/// ?? 运算符说明(null合并运算符):
		/// - 如果左侧不为null,返回左侧值
		/// - 如果左侧为null,返回右侧值
		/// - 相当于: value != null ? value : _empty.value
		/// 
		/// @param value - 要存储的字符串
		/// </summary>
		public ValString(string value) {
			this.value = value ?? _empty.value;
		}

		/// <summary>
		/// 直接返回字符串内容
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			return value;
		}

		/// <summary>
		/// 返回MiniScript代码形式(加上引号和转义)
		/// 
		/// 转义规则:
		/// - 双引号变成两个双引号 (MiniScript的字符串转义规则)
		/// 
		/// @param vm - 虚拟机实例
		/// @param recursionLimit - 递归限制(字符串不需要)
		/// @return 带引号的字符串字面量
		/// </summary>
		public override string CodeForm(TAC.Machine vm, int recursionLimit=-1) {
			return "\"" + value.Replace("\"", "\"\"") + "\"";
		}

		/// <summary>
		/// 布尔值: 非空字符串为真
		/// 
		/// string.IsNullOrEmpty说明:
		/// - C#标准库方法
		/// - 检查字符串是否为null或空字符串""
		/// - 比手动检查更安全、更清晰
		/// </summary>
		public override bool BoolValue() {
			// Any nonempty string is considered true.
			// 任何非空字符串被认为是真。
			return !string.IsNullOrEmpty(value);
		}

		/// <summary>
		/// 类型检查: 是否为string类型
		/// </summary>
		public override bool IsA(Value type, TAC.Machine vm) {
			if (type == null) return false;
			return type == vm.stringType;
		}

		/// <summary>
		/// 计算哈希值(使用C#字符串的哈希算法)
		/// </summary>
		public override int Hash() {
			return value.GetHashCode();
		}

		/// <summary>
		/// 相等性检查: 字符串内容相同即相等
		/// 
		/// 说明:
		/// - 使用C#的字符串相等比较(区分大小写)
		/// - 两个ValString必须内容完全相同才相等
		/// </summary>
		public override double Equality(Value rhs) {
			// String equality is treated the same as in C#.
			// 字符串相等性与C#中的处理方式相同。
			return rhs is ValString && ((ValString)rhs).value == value ? 1 : 0;
		}

		/// <summary>
		/// 获取字符串中的单个字符
		/// 
		/// 功能说明:
		/// - 支持负索引(从末尾计数)
		/// - 返回单字符的字符串(不是char)
		/// - 索引越界抛出异常
		/// 
		/// @param index - 字符位置索引
		/// @return 包含单个字符的ValString
		/// @throws KeyException - 如果索引不是数字
		/// @throws IndexException - 如果索引越界
		/// </summary>
		public Value GetElem(Value index) {
			if (!(index is ValNumber)) throw new KeyException("String index must be numeric", null);
			var i = index.IntValue();
			if (i < 0) i += value.Length;  // 负索引: -1表示最后一个字符
			if (i < 0 || i >= value.Length) {
				throw new IndexException("Index Error (string index " + index + " out of range)");

			}
			// Substring(起始位置, 长度): 提取子字符串
			return new ValString(value.Substring(i, 1));
		}

		/// <summary>
		/// Magic identifier for the is-a entry in the class system:
		/// 类系统中is-a条目的魔法标识符:
		/// 
		/// 说明:
		/// - "__isa"是MiniScript中实现原型继承的特殊键
		/// - 指向父类/原型对象
		/// - 类似JavaScript的__proto__
		/// </summary>
		public static ValString magicIsA = new ValString("__isa");
		
		// 缓存的空字符串实例
		static ValString _empty = new ValString("");
		
		/// <summary>
		/// Handy accessor for an empty ValString.
		/// IMPORTANT: do not alter the value of the object returned!
		/// 
		/// 方便访问空ValString的访问器。
		/// 重要: 不要修改返回对象的值!
		/// </summary>
		public static ValString empty { get { return _empty; } }

	}
	
	/// <summary>
	/// We frequently need to generate a ValString out of a string for fleeting purposes,
	/// like looking up an identifier in a map (which we do ALL THE TIME).  So, here's
	/// a little recycling pool of reusable ValStrings, for this purpose only.
	/// 
	/// 我们经常需要为短暂的目的从字符串生成ValString,
	/// 比如在映射中查找标识符(我们一直在做这件事)。所以,这里有
	/// 一个可重用ValString的小型回收池,仅用于此目的。
	/// 
	/// 对象池模式说明:
	/// - 预先创建一组对象并重复使用
	/// - 减少频繁的内存分配和垃圾回收
	/// - 特别适合生命周期短、使用频繁的对象
	/// 
	/// 性能优化:
	/// - 标识符查找非常频繁
	/// - 避免每次查找都创建新的ValString对象
	/// - 使用后立即归还到池中
	/// 
	/// 重要警告:
	/// - 只能用于临时查找
	/// - 不能长期持有返回的对象
	/// - 使用完必须调用Release归还
	/// </summary>
	class TempValString : ValString {
		private TempValString next;  // 链表指针,指向池中下一个对象

		/// <summary>
		/// 私有构造函数,防止外部直接创建
		/// </summary>
		private TempValString(string s) : base(s) {  // base关键字: 调用父类构造函数
			this.next = null;
		}

		// 池的头指针(链表头)
		private static TempValString _tempPoolHead = null;
		
		// 锁对象,用于线程安全
		// object类型说明: C#中所有类型的基类
		private static object lockObj = new object();
		
		/// <summary>
		/// 从池中获取一个临时ValString
		/// 
		/// lock语句说明:
		/// - 确保线程安全,同一时间只有一个线程可以执行lock块内的代码
		/// - 防止多线程同时访问导致的数据竞争
		/// - 在多线程环境中必须使用
		/// 
		/// @param s - 要存储的字符串
		/// @return 临时ValString对象
		/// </summary>
		public static TempValString Get(string s) {
			lock(lockObj) {
				if (_tempPoolHead == null) {
					// 池为空,创建新对象
					return new TempValString(s);
				} else {
					// 从池中取出头部对象
					var result = _tempPoolHead;
					_tempPoolHead = _tempPoolHead.next;
					result.value = s;  // 重新设置值
					return result;
				}
			}
		}
		
		/// <summary>
		/// 将临时ValString归还到池中
		/// 
		/// @param temp - 要归还的对象
		/// </summary>
		public static void Release(TempValString temp) {
			lock(lockObj) {
				// 插入到链表头部
				temp.next = _tempPoolHead;
				_tempPoolHead = temp;
			}
		}
	}
	
	
	/// <summary>
	/// ValList represents a MiniScript list (which, under the hood, is
	/// just a wrapper for a List of Values).
	/// 
	/// ValList表示MiniScript列表(其底层只是Value的List的包装器)。
	/// 
	/// 设计说明:
	/// - 动态数组,可以增长和收缩
	/// - 元素可以是任何MiniScript值
	/// - 支持负索引(从末尾计数)
	/// - 是引用类型(修改会影响所有引用)
	/// 
	/// List<T>说明:
	/// - C#泛型动态数组
	/// - T是类型参数,这里是Value
	/// - 提供Add, Remove, Count等方法
	/// - 自动管理容量
	/// </summary>
	public class ValList : Value {
		// 列表最大长度限制(约16MB)
		public static long maxSize = 0xFFFFFF;		// about 16 MB
		
		public List<Value> values;  // 存储列表元素

		/// <summary>
		/// 构造函数: 创建新列表或包装现有列表
		/// 
		/// 默认参数说明:
		/// - values = null 表示如果不提供参数,默认为null
		/// - 调用时可以省略参数: new ValList()
		/// 
		/// @param values - 要包装的列表,null则创建新列表
		/// </summary>
		public ValList(List<Value> values = null) {
			this.values = values == null ? new List<Value>() : values;
		}

		/// <summary>
		/// 完全求值列表,解析所有变量引用
		/// 
		/// 功能说明:
		/// - 遍历列表中的每个元素
		/// - 如果元素是变量或临时值,解析为实际值
		/// - 使用写时复制(copy-on-write)优化
		/// 
		/// 写时复制优化:
		/// - 只在实际需要修改时才创建副本
		/// - 如果所有元素都不需要解析,返回原列表
		/// - 节省内存和CPU
		/// 
		/// @param context - 求值上下文
		/// @return 完全求值后的列表
		/// </summary>
		public override Value FullEval(TAC.Context context) {
			// Evaluate each of our list elements, and if any of those is
			// a variable or temp, then resolve those now.
			// 求值每个列表元素,如果其中任何一个是变量或临时值,则立即解析。
			
			// CAUTION: do not mutate our original list!  We may need
			// it in its original form on future iterations.
			// 注意: 不要改变我们的原始列表! 在将来的迭代中我们可能需要它的原始形式。
			
			ValList result = null;  // 只在需要时创建
			for (var i = 0; i < values.Count; i++) {
				var copied = false;
				// 检查是否是需要求值的类型
				if (values[i] is ValTemp || values[i] is ValVar) {
					Value newVal = values[i].Val(context);
					if (newVal != values[i]) {
						// OK, something changed, so we're going to need a new copy of the list.
						// 好的,有东西改变了,所以我们需要列表的新副本。
						if (result == null) {
							result = new ValList();
							// 复制已处理的元素
							for (var j = 0; j < i; j++) result.values.Add(values[j]);
						}
						result.values.Add(newVal);
						copied = true;
					}
				}
				if (!copied && result != null) {
					// No change; but we have new results to return, so copy it as-is
					// 没有改变;但我们有新结果要返回,所以原样复制
					result.values.Add(values[i]);
				}
			}
			// ?? 运算符: 如果result为null,返回this
			return result ?? this;
		}

		/// <summary>
		/// 创建列表的求值副本
		/// 
		/// 用途说明:
		/// - 当列表字面量出现在源代码中时使用
		/// - 确保每次执行都得到新的、独立的可变对象
		/// - 而不是多次得到同一个对象
		/// 
		/// 示例:
		/// x = [1, 2, 3]  // 每次执行这行都创建新列表
		/// y = x          // 这不创建新列表,y引用x
		/// 
		/// @param context - 求值上下文
		/// @return 新的列表副本
		/// </summary>
		public ValList EvalCopy(TAC.Context context) {
			// Create a copy of this list, evaluating its members as we go.
			// 创建此列表的副本,同时求值其成员。
			
			// This is used when a list literal appears in the source, to
			// ensure that each time that code executes, we get a new, distinct
			// mutable object, rather than the same object multiple times.
			// 当列表字面量出现在源代码中时使用,以确保每次执行该代码时,
			// 我们得到一个新的、独立的可变对象,而不是多次得到同一个对象。
			
			var result = new ValList();
			for (var i = 0; i < values.Count; i++) {
				// 三元运算符链: null检查 + 求值
				result.values.Add(values[i] == null ? null : values[i].Val(context));
			}
			return result;
		}

		/// <summary>
		/// 返回MiniScript代码形式
		/// 
		/// 格式说明:
		/// - 使用方括号 []
		/// - 元素用逗号分隔
		/// - 递归处理嵌套结构
		/// - 有深度限制防止无限递归
		/// 
		/// @param vm - 虚拟机实例
		/// @param recursionLimit - 递归深度限制
		/// @return 代码形式的字符串
		/// </summary>
		public override string CodeForm(TAC.Machine vm, int recursionLimit=-1) {
			if (recursionLimit == 0) return "[...]";  // 达到深度限制
			
			// 尝试查找简短名称(如果列表被赋值给了变量)
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(this);
				if (shortName != null) return shortName;
			}
			
			// 转换每个元素
			var strs = new string[values.Count];  // 数组初始化
			for (var i = 0; i < values.Count; i++) {
				if (values[i] == null) strs[i] = "null";
				else strs[i] = values[i].CodeForm(vm, recursionLimit - 1);
			}
			
			// string.Join说明: 用指定分隔符连接字符串数组
			return "[" + string.Join(", ", strs) + "]";
		}

		/// <summary>
		/// 字符串表示(限制递归深度为3)
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			return CodeForm(vm, 3);
		}

		/// <summary>
		/// 布尔值: 非空列表为真
		/// </summary>
		public override bool BoolValue() {
			// A list is considered true if it is nonempty.
			// 列表如果非空则被认为是真。
			return values != null && values.Count > 0;
		}

		/// <summary>
		/// 类型检查: 是否为list类型
		/// </summary>
		public override bool IsA(Value type, TAC.Machine vm) {
			if (type == null) return false;
			return type == vm.listType;
		}

		/// <summary>
		/// 计算哈希值(使用递归哈希算法)
		/// </summary>
		public override int Hash() {
			return RecursiveHash();
		}

		/// <summary>
		/// 相等性检查
		/// 
		/// 检查逻辑:
		/// 1. 类型必须都是列表
		/// 2. 如果是同一个对象,直接返回相等
		/// 3. 长度必须相同
		/// 4. 递归比较每个元素
		/// 
		/// @param rhs - 右侧值
		/// @return 相等程度(0或1)
		/// </summary>
		public override double Equality(Value rhs) {
			// Quick bail-out cases:
			// 快速退出情况:
			if (!(rhs is ValList)) return 0;
			List<Value> rhl = ((ValList)rhs).values;
			if (rhl == values) return 1;  // (same list) (同一个列表)
			int count = values.Count;
			if (count != rhl.Count) return 0;

			// Otherwise, we have to do:
			// 否则,我们必须这样做:
			return RecursiveEqual(rhs) ? 1 : 0;
		}

		/// <summary>
		/// 列表可以设置元素
		/// </summary>
		public override bool CanSetElem() { return true; }

		/// <summary>
		/// 设置列表元素
		/// 
		/// 功能说明:
		/// - 支持负索引
		/// - 索引越界抛出异常
		/// - 直接修改列表(引用语义)
		/// 
		/// @param index - 元素索引
		/// @param value - 要设置的值
		/// @throws IndexException - 索引越界
		/// </summary>
		public override void SetElem(Value index, Value value) {
			var i = index.IntValue();
			if (i < 0) i += values.Count;  // 负索引处理
			if (i < 0 || i >= values.Count) {
				throw new IndexException("Index Error (list index " + index + " out of range)");
			}
			values[i] = value;
		}

		/// <summary>
		/// 获取列表元素
		/// 
		/// @param index - 元素索引
		/// @return 该位置的值
		/// @throws KeyException - 索引不是数字
		/// @throws IndexException - 索引越界
		/// </summary>
		public Value GetElem(Value index) {
			if (!(index is ValNumber)) throw new KeyException("List index must be numeric", null);
			var i = index.IntValue();
			if (i < 0) i += values.Count;
			if (i < 0 || i >= values.Count) {
				throw new IndexException("Index Error (list index " + index + " out of range)");

			}
			return values[i];
		}

	}
	
	/// <summary>
	/// ValMap represents a MiniScript map, which under the hood is just a Dictionary
	/// of Value, Value pairs.
	/// 
	/// ValMap表示MiniScript映射,其底层只是Value、Value对的字典。
	/// 
	/// 设计说明:
	/// - 实现了MiniScript的对象系统
	/// - 支持原型继承(通过__isa)
	/// - 可以作为字典或对象使用
	/// - 支持自定义求值和赋值行为
	/// 
	/// Dictionary<TKey, TValue>说明:
	/// - C#的哈希表实现
	/// - 键值对集合
	/// - 提供O(1)的查找性能
	/// - TKey和TValue是泛型参数
	/// </summary>
	public class ValMap : Value {

		/// <summary>
		/// Define a maximum depth we will allow an inheritance ("__isa") chain to be.
		/// This is used to avoid locking up the app if some bozo creates a loop in
		/// the __isa chain, but it also means we can't allow actual inheritance trees
		/// to be longer than this.  So, use a reasonably generous value.
		/// 
		/// 定义我们允许继承("__isa")链的最大深度。
		/// 这用于避免如果某人在__isa链中创建循环导致应用程序锁定,
		/// 但这也意味着我们不能允许实际的继承树长于此值。
		/// 因此,使用一个相当宽松的值。
		/// 
		/// const说明:
		/// - 编译时常量,值在编译时确定
		/// - 不能修改
		/// - 比readonly更严格
		/// </summary>
		public const int maxIsaDepth = 256;

		// 字典存储键值对
		public Dictionary<Value, Value> map;

		/// <summary>
		/// Assignment override function: return true to cancel (override)
		/// the assignment, or false to allow it to happen as normal.
		/// 
		/// 赋值覆盖函数: 返回true取消(覆盖)赋值,
		/// 返回false允许正常进行。
		/// 
		/// delegate说明:
		/// - 委托是C#的函数指针/回调机制
		/// - 可以引用方法
		/// - 类型安全的函数引用
		/// - 可以像变量一样传递
		/// 
		/// 用途:
		/// - 允许C#代码拦截MiniScript的赋值操作
		/// - 可以实现只读属性、验证等
		/// 
		/// @param key - 要设置的键
		/// @param value - 要设置的值
		/// @return true取消赋值,false允许赋值
		/// </summary>
		public delegate bool AssignOverrideFunc(Value key, Value value);
		public AssignOverrideFunc assignOverride;

		/// <summary>
		/// Can store arbitrary data. Useful for retaining a C# object
		/// passed into scripting.
		/// 
		/// 可以存储任意数据。用于保留传递到脚本中的C#对象。
		/// 
		/// 用途:
		/// - 在MiniScript和C#之间传递数据
		/// - 存储C#对象的引用
		/// - 实现宿主对象绑定
		/// </summary>
		public object userData;

		/// <summary>
		/// Evaluation override function: Allows map to be fully backed
		/// by a C# object (or otherwise intercept map indexing).
		/// Return true to return the out value to the caller, or false
		/// to proceed with normal map look-up.
		/// 
		/// 求值覆盖函数: 允许映射完全由C#对象支持
		/// (或以其他方式拦截映射索引)。
		/// 返回true将out值返回给调用者,返回false继续正常的映射查找。
		/// 
		/// 用途:
		/// - 实现动态属性
		/// - 将MiniScript对象映射到C#对象
		/// - 实现计算属性
		/// 
		/// @param key - 要查找的键
		/// @param value - 输出找到的值
		/// @return true使用输出值,false继续查找
		/// </summary>
		public delegate bool EvalOverrideFunc(Value key, out Value value);
		public EvalOverrideFunc evalOverride;

		/// <summary>
		/// 构造函数: 创建新的映射
		/// 
		/// RValueEqualityComparer说明:
		/// - 自定义相等比较器
		/// - 确保字典使用MiniScript的相等语义
		/// - 而不是C#的默认引用相等
		/// </summary>
		public ValMap() {
			this.map = new Dictionary<Value, Value>(RValueEqualityComparer.instance);
		}
		
		/// <summary>
		/// 布尔值: 非空映射为真
		/// </summary>
		public override bool BoolValue() {
			// A map is considered true if it is nonempty.
			// 映射如果非空则被认为是真。
			return map != null && map.Count > 0;
		}

		/// <summary>
		/// Convenience method to check whether the map contains a given string key.
		/// 便捷方法,检查映射是否包含给定的字符串键。
		/// 
		/// 实现说明:
		/// - 使用临时ValString对象池
		/// - 避免创建新的ValString对象
		/// - 使用后立即归还到池中
		/// 
		/// @param identifier - 要检查的字符串键
		/// @return 是否包含该键
		/// </summary>
		public bool ContainsKey(string identifier) {
			var idVal = TempValString.Get(identifier);  // 从池中获取
			bool result = map.ContainsKey(idVal);
			TempValString.Release(idVal);  // 归还到池中
			return result;
		}
		
		/// <summary>
		/// Convenience method to check whether this map contains a given key
		/// (of arbitrary type).
		/// 便捷方法,检查此映射是否包含给定的键(任意类型)。
		/// 
		/// @param key - 要检查的键
		/// @return 是否包含该键
		/// </summary>
		public bool ContainsKey(Value key) {
			if (key == null) key = ValNull.instance;  // null转为ValNull
			return map.ContainsKey(key);
		}
		
		/// <summary>
		/// Get the number of entries in this map.
		/// 获取此映射中的条目数。
		/// 
		/// 属性(只读):
		/// - 只有get访问器
		/// - 返回字典的Count
		/// </summary>
		public int Count {
			get { return map.Count; }
		}
		
		/// <summary>
		/// Return the KeyCollection for this map.
		/// 返回此映射的键集合。
		/// 
		/// KeyCollection说明:
		/// - Dictionary的嵌套类
		/// - 表示字典中所有键的集合
		/// - 可以用foreach遍历
		/// </summary>
		public Dictionary<Value, Value>.KeyCollection Keys {
			get { return map.Keys; }
		}
		
		
		/// <summary>
		/// Accessor to get/set on element of this map by a string key, walking
		/// the __isa chain as needed.  (Note that if you want to avoid that, then
		/// simply look up your value in .map directly.)
		/// 
		/// 通过字符串键获取/设置此映射的元素的访问器,
		/// 根据需要遍历__isa链。(注意,如果你想避免这种情况,
		/// 那么直接在.map中查找你的值即可。)
		/// 
		/// 索引器说明:
		/// - this[...] 语法
		/// - 允许像数组一样访问: obj["key"]
		/// - 同时提供get和set
		/// 
		/// @param identifier - 字符串键
		/// @return 关联的值
		/// </summary>
		public Value this [string identifier] {
			get { 
				var idVal = TempValString.Get(identifier);
				Value result = Lookup(idVal);  // 遍历__isa链查找
				TempValString.Release(idVal);
				return result;
			}
			set { map[new ValString(identifier)] = value; }  // 直接设置
		}
		
		/// <summary>
		/// Look up the given identifier as quickly as possible, without
		/// walking the __isa chain or doing anything fancy.  (This is used
		/// when looking up local variables.)
		/// 
		/// 尽可能快地查找给定的标识符,
		/// 不遍历__isa链或做任何花哨的事情。(这用于查找局部变量。)
		/// 
		/// 性能优化:
		/// - 小映射(<5个元素): 直接遍历比哈希查找更快
		/// - 大映射: 使用字典的哈希查找
		/// - 针对常见情况(局部变量作用域通常很小)优化
		/// 
		/// @param identifier - 要查找的标识符
		/// @param value - 输出找到的值
		/// @return 是否找到
		/// </summary>
		public bool TryGetValue(string identifier, out Value value) {
			if (map.Count < 5) {
				// new approach: just iterate!  This is faster for small maps (which are common).
				// 新方法: 只是迭代! 这对小映射更快(这很常见)。
				
				foreach (var kv in map) {
					// 类型检查和比较一步完成
					if (kv.Key is ValString ks && ks.value == identifier) {
						value = kv.Value;
						return true;
					}
				}
				value = null;
				return false;
			}
			
			// old method, and still better on big maps: use dictionary look-up.
			// 旧方法,在大映射上仍然更好: 使用字典查找。
			var idVal = TempValString.Get(identifier);
			bool result = TryGetValue(idVal, out value);
			TempValString.Release(idVal);
			return result;
		}

		/// <summary>
		/// Look up the given identifier in the backing map (unless overridden
		/// by the evalOverride function).
		/// 
		/// 在后备映射中查找给定的标识符
		/// (除非被evalOverride函数覆盖)。
		/// 
		/// @param key - 要查找的标识符
		/// @param value - 输出对应的值
		/// @return 是否找到
		/// </summary>
		public bool TryGetValue(Value key, out Value value)
		{
			// 先尝试覆盖函数
			if (evalOverride != null && evalOverride(key, out value)) return true;
			// 否则使用字典查找
			return map.TryGetValue(key, out value);
		}

		/// <summary>
		/// Look up a value in this dictionary, walking the __isa chain to find
		/// it in a parent object if necessary.  
		/// 
		/// 在此字典中查找值,如有必要,
		/// 遍历__isa链在父对象中查找。
		/// 
		/// 原型继承说明:
		/// - __isa指向父对象/原型
		/// - 查找时逐级向上查找
		/// - 类似JavaScript的原型链
		/// - 有深度限制防止循环
		/// 
		/// @param key - 要搜索的键
		/// @return 与该键关联的值,找不到返回null
		/// @throws LimitExceededException - __isa深度超限
		/// </summary>
		public Value Lookup(Value key) {
			if (key == null) key = ValNull.instance;
			Value result = null;
			ValMap obj = this;
			int chainDepth = 0;
			
			// 沿着__isa链向上查找
			while (obj != null) {
				if (obj.TryGetValue(key, out result)) return result;
				
				Value parent;
				if (!obj.TryGetValue(ValString.magicIsA, out parent)) break;
				
				// 防止无限循环
				if (chainDepth++ > maxIsaDepth) {
					throw new LimitExceededException("__isa depth exceeded (perhaps a reference loop?)");
				}
				
				obj = parent as ValMap;  // 转到父对象
			}
			return null;
		}

		/// <summary>
		/// Look up a value in this dictionary, walking the __isa chain to find
		/// it in a parent object if necessary; return both the value found and
		/// (via the output parameter) the map it was found in.
		/// 
		/// 在此字典中查找值,如有必要遍历__isa链在父对象中查找;
		/// 返回找到的值和(通过输出参数)找到它的映射。
		/// 
		/// @param key - 要搜索的键
		/// @param valueFoundIn - 输出找到值的映射
		/// @return 与该键关联的值
		/// </summary>
		public Value Lookup(Value key, out ValMap valueFoundIn) {
			if (key == null) key = ValNull.instance;
			Value result = null;
			ValMap obj = this;
			int chainDepth = 0;
			
			while (obj != null) {
				if (obj.TryGetValue(key, out result)) {
					valueFoundIn = obj;  // 记录在哪个映射中找到的
					return result;
				}
				
				Value parent;
				if (!obj.TryGetValue(ValString.magicIsA, out parent)) break;
				
				if (chainDepth++ > maxIsaDepth) {
					throw new LimitExceededException("__isa depth exceeded (perhaps a reference loop?)");
				}
				
				obj = parent as ValMap;
			}
			valueFoundIn = null;
			return null;
		}
		
		/// <summary>
		/// 完全求值映射,解析所有变量引用
		/// 
		/// 功能说明:
		/// - 解析键和值中的变量引用
		/// - 直接修改字典(不创建副本)
		/// - 如果键改变了,需要重新插入
		/// 
		/// ToArray()说明:
		/// - 创建键的数组副本
		/// - 避免在遍历时修改集合的错误
		/// - LINQ方法
		/// 
		/// @param context - 求值上下文
		/// @return this(已修改)
		/// </summary>
		public override Value FullEval(TAC.Context context) {
			// Evaluate each of our elements, and if any of those is
			// a variable or temp, then resolve those now.
			// 求值我们的每个元素,如果其中任何一个是变量或临时值,则立即解析。
			
			foreach (Value k in map.Keys.ToArray()) {	// TODO: something more efficient here.
				                                        // TODO: 这里需要更高效的方法。
				Value key = k;		// stupid C#! (愚蠢的C#!)
				Value value = map[key];
				
				// 如果键是变量,解析并重新插入
				if (key is ValTemp || key is ValVar) {
					map.Remove(key);
					key = key.Val(context);
					map[key] = value;
				}
				
				// 如果值是变量,解析
				if (value is ValTemp || value is ValVar) {
					map[key] = value.Val(context);
				}
			}
			return this;
		}

		/// <summary>
		/// 创建映射的求值副本
		/// 
		/// 用途: 与列表类似,用于字面量
		/// 
		/// @param context - 求值上下文
		/// @return 新的映射副本
		/// </summary>
		public ValMap EvalCopy(TAC.Context context) {
			// Create a copy of this map, evaluating its members as we go.
			// 创建此映射的副本,同时求值其成员。
			
			// This is used when a map literal appears in the source, to
			// ensure that each time that code executes, we get a new, distinct
			// mutable object, rather than the same object multiple times.
			// 当映射字面量出现在源代码中时使用,以确保每次执行该代码时,
			// 我们得到一个新的、独立的可变对象,而不是多次得到同一个对象。
			
			var result = new ValMap();
			foreach (Value k in map.Keys) {
				Value key = k;		// stupid C#! (愚蠢的C#!)
				Value value = map[key];
				
				// 求值键
				if (key is ValTemp || key is ValVar || value is ValSeqElem) key = key.Val(context);
				// 求值值
				if (value is ValTemp || value is ValVar || value is ValSeqElem) value = value.Val(context);
				
				result.map[key] = value;
			}
			return result;
		}

		/// <summary>
		/// 返回MiniScript代码形式
		/// 
		/// 格式说明:
		/// - 使用花括号 {}
		/// - 键值对格式: key: value
		/// - 用逗号分隔
		/// - __isa特殊处理(限制深度为1)
		/// 
		/// @param vm - 虚拟机实例
		/// @param recursionLimit - 递归深度限制
		/// @return 代码形式的字符串
		/// </summary>
		public override string CodeForm(TAC.Machine vm, int recursionLimit=-1) {
			if (recursionLimit == 0) return "{...}";
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(this);
				if (shortName != null) return shortName;
			}
			
			var strs = new string[map.Count];
			int i = 0;
			foreach (KeyValuePair<Value, Value> kv in map) {
				int nextRecurLimit = recursionLimit - 1;
				// __isa只显示一层,避免输出太长
				if (kv.Key == ValString.magicIsA) nextRecurLimit = 1;
				
				// string.Format说明: 格式化字符串
				// {0}, {1}是占位符,按顺序替换
				strs[i++] = string.Format("{0}: {1}", kv.Key.CodeForm(vm, nextRecurLimit), 
					kv.Value == null ? "null" : kv.Value.CodeForm(vm, nextRecurLimit));
			}
			
			// String.Join: 用分隔符连接字符串数组
			return "{" + String.Join(", ", strs) + "}";
		}

		/// <summary>
		/// 字符串表示(限制递归深度为3)
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			return CodeForm(vm, 3);
		}

		/// <summary>
		/// 类型检查,支持原型继承
		/// 
		/// 检查逻辑:
		/// 1. 如果type是map类型,直接返回true
		/// 2. 否则沿着__isa链查找
		/// 3. 如果找到匹配返回true
		/// 
		/// @param type - 要检查的类型
		/// @param vm - 虚拟机实例
		/// @return 是否为该类型
		/// </summary>
		public override bool IsA(Value type, TAC.Machine vm) {
			if (type == null) return false;
			
			// If the given type is the magic 'map' type, then we're definitely
			// one of those.  Otherwise, we have to walk the __isa chain.
			// 如果给定的类型是魔法'map'类型,那么我们肯定是其中之一。
			// 否则,我们必须遍历__isa链。
			if (type == vm.mapType) return true;
			
			Value p = null;
			TryGetValue(ValString.magicIsA, out p);
			int chainDepth = 0;
			
			while (p != null) {
				if (p == type) return true;
				if (!(p is ValMap)) return false;
				
				if (chainDepth++ > maxIsaDepth) {
					throw new LimitExceededException("__isa depth exceeded (perhaps a reference loop?)");
				}
				
				((ValMap)p).TryGetValue(ValString.magicIsA, out p);
			}
			return false;
		}

		/// <summary>
		/// 计算哈希值(使用递归哈希算法)
		/// </summary>
		public override int Hash() {
			return RecursiveHash();
		}

		/// <summary>
		/// 相等性检查
		/// 
		/// 检查逻辑:
		/// 1. 类型必须都是映射
		/// 2. 如果是同一个对象,返回相等
		/// 3. 大小必须相同
		/// 4. 递归比较所有键值对
		/// 
		/// @param rhs - 右侧值
		/// @return 相等程度(0或1)
		/// </summary>
		public override double Equality(Value rhs) {
			// Quick bail-out cases:
			// 快速退出情况:
			if (!(rhs is ValMap)) return 0;
			Dictionary<Value, Value> rhm = ((ValMap)rhs).map;
			if (rhm == map) return 1;  // (same map) (同一个映射)
			int count = map.Count;
			if (count != rhm.Count) return 0;

			// Otherwise:
			// 否则:
			return RecursiveEqual(rhs) ? 1 : 0;
		}

		/// <summary>
		/// 映射可以设置元素
		/// </summary>
		public override bool CanSetElem() { return true; }

		/// <summary>
		/// Set the value associated with the given key (index).  This is where
		/// we take the opportunity to look for an assignment override function,
		/// and if found, give that a chance to handle it instead.
		/// 
		/// 设置与给定键(索引)关联的值。这里我们利用机会
		/// 查找赋值覆盖函数,如果找到,让它有机会代为处理。
		/// 
		/// @param index - 键
		/// @param value - 值
		/// </summary>
		public override void SetElem(Value index, Value value) {
			if (index == null) index = ValNull.instance;
			// 如果有覆盖函数且返回true,则不执行默认赋值
			if (assignOverride == null || !assignOverride(index, value)) {
				map[index] = value;
			}
		}

		/// <summary>
		/// Get the indicated key/value pair as another map containing "key" and "value".
		/// (This is used when iterating over a map with "for".)
		/// 
		/// 将指示的键/值对作为包含"key"和"value"的另一个映射获取。
		/// (这在用"for"迭代映射时使用。)
		/// 
		/// for循环说明:
		/// for item in myMap
		///   print item.key + " = " + item.value
		/// end for
		/// 
		/// ElementAt<T>说明:
		/// - LINQ方法
		/// - 获取集合中指定索引的元素
		/// - 对于字典的Keys,可能不是最高效的
		/// 
		/// @param index - 键/值对的0基索引
		/// @return 包含"key"和"value"的新映射
		/// @throws IndexException - 索引越界
		/// </summary>
		public ValMap GetKeyValuePair(int index) {
			Dictionary<Value, Value>.KeyCollection keys = map.Keys;
			if (index < 0 || index >= keys.Count) {
				throw new IndexException("index " + index + " out of range for map");
			}
			
			Value key = keys.ElementAt<Value>(index);   // (TODO: consider more efficient methods here)
			                                             // (TODO: 这里考虑更高效的方法)
			var result = new ValMap();
			// ValNull转为实际的null
			result.map[keyStr] = (key is ValNull ? null : key);
			result.map[valStr] = map[key];
			return result;
		}
		
		// 缓存的键名
		static ValString keyStr = new ValString("key");
		static ValString valStr = new ValString("value");

	}
	
	/// <summary>
	/// Function: our internal representation of a MiniScript function.  This includes
	/// its parameters and its code.  (It does not include a name -- functions don't 
	/// actually HAVE names; instead there are named variables whose value may happen 
	/// to be a function.)
	/// 
	/// Function: 我们对MiniScript函数的内部表示。这包括
	/// 它的参数和代码。(它不包括名称 -- 函数实际上
	/// 没有名称;而是有命名变量,其值恰好可能是一个函数。)
	/// 
	/// 设计说明:
	/// - 函数是一等公民(first-class)
	/// - 可以赋值给变量、作为参数传递
	/// - 支持默认参数值
	/// - 编译为TAC(三地址码)形式
	/// </summary>
	public class Function {
		/// <summary>
		/// Param: helper class representing a function parameter.
		/// Param: 表示函数参数的辅助类。
		/// 
		/// 功能说明:
		/// - 参数名
		/// - 默认值(可选)
		/// </summary>
		public class Param {
			public string name;
			public Value defaultValue;

			public Param(string name, Value defaultValue) {
				this.name = name;
				this.defaultValue = defaultValue;
			}
		}
		
		// Function parameters
		// 函数参数
		public List<Param> parameters;
		
		// Function code (compiled down to TAC form)
		// 函数代码(编译为TAC形式)
		// TAC.Line说明: TAC(Three Address Code)三地址码指令
		public List<TAC.Line> code;

		/// <summary>
		/// 构造函数: 创建函数
		/// 
		/// @param code - 函数的TAC代码
		/// </summary>
		public Function(List<TAC.Line> code) {
			this.code = code;
			parameters = new List<Param>();
		}

		/// <summary>
		/// 转换为字符串表示
		/// 
		/// StringBuilder说明:
		/// - 可变字符串类
		/// - 比字符串连接更高效
		/// - 适合构建长字符串
		/// 
		/// Append方法: 添加内容到末尾
		/// Count()方法: LINQ方法,获取元素数量
		/// 
		/// @param vm - 虚拟机实例
		/// @return 函数签名的字符串表示
		/// </summary>
		public string ToString(TAC.Machine vm) {
			var s = new System.Text.StringBuilder();
			s.Append("FUNCTION(");			
			for (var i=0; i < parameters.Count(); i++) {
				if (i > 0) s.Append(", ");
				s.Append(parameters[i].name);
				// 如果有默认值,显示它
				if (parameters[i].defaultValue != null) s.Append("=" + parameters[i].defaultValue.CodeForm(vm));
			}
			s.Append(")");
			return s.ToString();
		}
	}
	
	/// <summary>
	/// ValFunction: a Value that is, in fact, a Function.
	/// ValFunction: 一个实际上是函数的值。
	/// 
	/// 设计说明:
	/// - 包装Function对象
	/// - 存储外部作用域(闭包支持)
	/// - 实现函数值的语义
	/// </summary>
	public class ValFunction : Value {
		public Function function;
		
		// local variables where the function was defined (usually, the module)
		// 函数定义处的局部变量(通常是模块)
		// readonly说明: 只能在构造函数中赋值,之后不能修改
		public readonly ValMap outerVars;	

		/// <summary>
		/// 构造函数: 创建函数值(不带外部变量)
		/// </summary>
		public ValFunction(Function function) {
			this.function = function;
		}
		
		/// <summary>
		/// 构造函数: 创建函数值(带外部变量,支持闭包)
		/// 
		/// 闭包说明:
		/// - 函数记住其创建时的作用域
		/// - 可以访问外部变量
		/// - 即使在外部作用域结束后仍然有效
		/// </summary>
		public ValFunction(Function function, ValMap outerVars) {
			this.function = function;
            this.outerVars = outerVars;
		}

		/// <summary>
		/// 字符串表示(显示函数签名)
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			return function.ToString(vm);
		}

		/// <summary>
		/// 布尔值: 函数总是为真
		/// </summary>
		public override bool BoolValue() {
			// A function value is ALWAYS considered true.
			// 函数值总是被认为是真。
			return true;
		}

		/// <summary>
		/// 类型检查: 是否为function类型
		/// </summary>
		public override bool IsA(Value type, TAC.Machine vm) {
			if (type == null) return false;
			return type == vm.functionType;
		}

		/// <summary>
		/// 哈希值(使用底层Function对象的哈希)
		/// </summary>
		public override int Hash() {
			return function.GetHashCode();
		}

		/// <summary>
		/// 相等性检查: 必须是完全相同的函数对象
		/// 
		/// 注意:
		/// - 即使两个函数代码相同,也不相等
		/// - 只有引用同一个Function对象才相等
		/// 
		/// @param rhs - 右侧值
		/// @return 相等程度(0或1)
		/// </summary>
		public override double Equality(Value rhs) {
			// Two Function values are equal only if they refer to the exact same function
			// 两个函数值只有在引用完全相同的函数时才相等
			if (!(rhs is ValFunction)) return 0;
			var other = (ValFunction)rhs;
			return function == other.function ? 1 : 0;
		}

		/// <summary>
		/// 绑定上下文变量并复制
		/// 
		/// 功能说明:
		/// - 创建函数的副本
		/// - 绑定新的外部变量作用域
		/// - 用于实现闭包
		/// 
		/// @param contextVariables - 要绑定的上下文变量
		/// @return 新的函数值
		/// </summary>
        public ValFunction BindAndCopy(ValMap contextVariables) {
            return new ValFunction(function, contextVariables);
        }

	}

	/// <summary>
	/// ValTemp: 表示临时变量的值
	/// 
	/// 用途说明:
	/// - 编译时使用
	/// - 表示中间计算结果
	/// - 在TAC代码中引用临时变量
	/// 
	/// 示例:
	/// x = a + b * c
	/// 编译为:
	/// _1 = b * c    // _1是临时变量
	/// x = a + _1
	/// </summary>
	public class ValTemp : Value {
		public int tempNum;  // 临时变量编号

		public ValTemp(int tempNum) {
			this.tempNum = tempNum;
		}

		/// <summary>
		/// 从上下文中获取临时变量的实际值
		/// </summary>
		public override Value Val(TAC.Context context) {
			return context.GetTemp(tempNum);
		}

		/// <summary>
		/// 带输出参数的求值版本
		/// </summary>
		public override Value Val(TAC.Context context, out ValMap valueFoundIn) {
			valueFoundIn = null;
			return context.GetTemp(tempNum);
		}

		/// <summary>
		/// 字符串表示(显示为 _数字)
		/// 
		/// CultureInfo.InvariantCulture说明:
		/// - 确保数字格式化不受区域设置影响
		/// - 保证跨平台一致性
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			return "_" + tempNum.ToString(CultureInfo.InvariantCulture);
		}

		/// <summary>
		/// 哈希值(使用临时变量编号)
		/// </summary>
		public override int Hash() {
			return tempNum.GetHashCode();
		}

		/// <summary>
		/// 相等性检查: 编号相同即相等
		/// </summary>
		public override double Equality(Value rhs) {
			return rhs is ValTemp && ((ValTemp)rhs).tempNum == tempNum ? 1 : 0;
		}

	}

	/// <summary>
	/// ValVar: 表示变量引用的值
	/// 
	/// 用途说明:
	/// - 表示对变量的引用(而不是变量的值)
	/// - 用于编译时和运行时
	/// - 支持地址操作符@
	/// 
	/// 示例:
	/// x = 42        // x是ValVar
	/// f = @foo      // @表示引用而不是调用
	/// </summary>
	public class ValVar : Value {
		/// <summary>
		/// 局部作用域模式枚举
		/// 
		/// 说明:
		/// - Off: 正常查找,包括外部作用域
		/// - Warn: 警告但允许访问外部作用域
		/// - Strict: 严格模式,只在局部作用域查找
		/// </summary>
		public enum LocalOnlyMode { Off, Warn, Strict };
		
		public string identifier;  // 变量名
		
		// reflects use of "@" (address-of) operator
		// 反映"@"(取地址)操作符的使用
		public bool noInvoke;
		
		// whether to look this up in the local scope only
		// 是否仅在局部作用域中查找
		public LocalOnlyMode localOnly = LocalOnlyMode.Off;
		
		public ValVar(string identifier) {
			this.identifier = identifier;
		}

		/// <summary>
		/// 从上下文中获取变量的值
		/// 
		/// 特殊处理:
		/// - self变量直接从上下文获取
		/// - 其他变量通过GetVar查找
		/// </summary>
		public override Value Val(TAC.Context context) {
			if (this == self) return context.self;
			return context.GetVar(identifier);
		}

		/// <summary>
		/// 带输出参数的求值版本
		/// </summary>
		public override Value Val(TAC.Context context, out ValMap valueFoundIn) {
			valueFoundIn = null;
			if (this == self) return context.self;
			return context.GetVar(identifier, localOnly);
		}

		/// <summary>
		/// 字符串表示
		/// 
		/// 格式:
		/// - 普通: 变量名
		/// - 地址操作: @变量名
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			if (noInvoke) return "@" + identifier;
			return identifier;
		}

		/// <summary>
		/// 哈希值(使用变量名的哈希)
		/// </summary>
		public override int Hash() {
			return identifier.GetHashCode();
		}

		/// <summary>
		/// 相等性检查: 变量名相同即相等
		/// </summary>
		public override double Equality(Value rhs) {
			return rhs is ValVar && ((ValVar)rhs).identifier == identifier ? 1 : 0;
		}

		/// <summary>
		/// Special name for the implicit result variable we assign to on expression statements:
		/// 用于表达式语句赋值的隐式结果变量的特殊名称:
		/// 
		/// 说明:
		/// - 表达式语句的结果存储在_中
		/// - 可以在REPL中访问上一个表达式的结果
		/// </summary>
		public static ValVar implicitResult = new ValVar("_");

		/// <summary>
		/// Special var for 'self'
		/// 'self'的特殊变量
		/// 
		/// 说明:
		/// - 在方法中引用当前对象
		/// - 类似其他语言的this
		/// </summary>
		public static ValVar self = new ValVar("self");
	}

	/// <summary>
	/// ValSeqElem: 表示序列元素访问的值
	/// 
	/// 用途说明:
	/// - 表示索引操作: list[i], map[key], obj.prop
	/// - 延迟求值直到实际需要
	/// - 支持地址操作符@
	/// 
	/// 示例:
	/// x = myList[5]     // ValSeqElem(myList, 5)
	/// y = myMap["key"]  // ValSeqElem(myMap, "key")
	/// z = obj.field     // ValSeqElem(obj, "field")
	/// </summary>
	public class ValSeqElem : Value {
		public Value sequence;  // 序列(列表、映射或对象)
		public Value index;     // 索引或键
		
		// reflects use of "@" (address-of) operator
		// 反映"@"(取地址)操作符的使用
		public bool noInvoke;

		public ValSeqElem(Value sequence, Value index) {
			this.sequence = sequence;
			this.index = index;
		}

		/// <summary>
		/// Look up the given identifier in the given sequence, walking the type chain
		/// until we either find it, or fail.
		/// 
		/// 在给定序列中查找给定的标识符,遍历类型链
		/// 直到找到它或失败。
		/// 
		/// 查找逻辑:
		/// 1. 如果是映射,直接查找
		/// 2. 如果映射中没有,查找__isa(父类)
		/// 3. 如果不是映射,查找对应的类型对象(listType, stringType等)
		/// 4. 在类型对象中查找方法
		/// 
		/// 这实现了MiniScript的类型系统:
		/// - 所有值都有类型
		/// - 类型提供方法和属性
		/// - 支持原型继承
		/// 
		/// @param sequence - 要查找的序列(对象)
		/// @param identifier - 要查找的标识符
		/// @param context - 上下文
		/// @param valueFoundIn - 输出找到值的映射
		/// @return 找到的值
		/// @throws LimitExceededException - __isa深度超限
		/// @throws KeyException - 找不到标识符
		/// @throws TypeException - 类型错误
		/// </summary>
		public static Value Resolve(Value sequence, string identifier, TAC.Context context, out ValMap valueFoundIn) {
			var includeMapType = true;
			valueFoundIn = null;
			int loopsLeft = ValMap.maxIsaDepth;
			
			while (sequence != null) {
				// 先求值序列本身
				if (sequence is ValTemp || sequence is ValVar) sequence = sequence.Val(context);
				
				if (sequence is ValMap) {
					// If the map contains this identifier, return its value.
					// 如果映射包含此标识符,返回其值。
					Value result = null;
					var idVal = TempValString.Get(identifier);
					bool found = ((ValMap)sequence).TryGetValue(idVal, out result);
					TempValString.Release(idVal);
					if (found) {
						valueFoundIn = (ValMap)sequence;
						return result;
					}
					
					// Otherwise, if we have an __isa, try that next.
					// 否则,如果我们有__isa,接下来尝试它。
					if (loopsLeft < 0) throw new LimitExceededException("__isa depth exceeded (perhaps a reference loop?)"); 
					if (!((ValMap)sequence).TryGetValue(ValString.magicIsA, out sequence)) {
						// ...and if we don't have an __isa, try the generic map type if allowed
						// ...如果我们没有__isa,在允许的情况下尝试通用映射类型
						if (!includeMapType) throw new KeyException(identifier);
						sequence = context.vm.mapType ?? Intrinsics.MapType();
						includeMapType = false;
					}
					
				} else if (sequence is ValList) {
					sequence = context.vm.listType ?? Intrinsics.ListType();
					includeMapType = false;
					
				} else if (sequence is ValString) {
					sequence = context.vm.stringType ?? Intrinsics.StringType();
					includeMapType = false;
					
				} else if (sequence is ValNumber) {
					sequence = context.vm.numberType ?? Intrinsics.NumberType();
					includeMapType = false;
					
				} else if (sequence is ValFunction) {
					sequence = context.vm.functionType ?? Intrinsics.FunctionType();
					includeMapType = false;
					
				} else {
					throw new TypeException("Type Error (while attempting to look up " + identifier + ")");
				}
				loopsLeft--;
			}
			return null;
		}

		/// <summary>
		/// 求值序列元素访问
		/// </summary>
		public override Value Val(TAC.Context context) {
			ValMap ignored;
			return Val(context, out ignored);
		}
		
		/// <summary>
		/// 求值序列元素访问(带输出参数)
		/// 
		/// 处理逻辑:
		/// 1. 处理self特殊情况
		/// 2. 求值索引
		/// 3. 如果索引是字符串,使用Resolve查找
		/// 4. 否则根据序列类型处理:
		///    - 映射: 直接查找
		///    - 列表: 数字索引
		///    - 字符串: 字符索引
		/// 
		/// @param context - 求值上下文
		/// @param valueFoundIn - 输出找到值的映射
		/// @return 找到的值
		/// @throws UndefinedIdentifierException - self未定义
		/// @throws KeyException - 键不存在
		/// @throws TypeException - 类型错误
		/// </summary>
		public override Value Val(TAC.Context context, out ValMap valueFoundIn) {
			Value baseSeq = sequence;
			// 处理self
			if (sequence == ValVar.self) {
				baseSeq = context.self;
				if (baseSeq == null) throw new UndefinedIdentifierException("self");
			}
			valueFoundIn = null;
			
			// 求值索引
			Value idxVal = index == null ? null : index.Val(context);
			
			// 如果索引是字符串,使用标识符查找
			if (idxVal is ValString) return Resolve(baseSeq, ((ValString)idxVal).value, context, out valueFoundIn);
			
			// Ok, we're searching for something that's not a string;
			// this can only be done in maps, lists, and strings (and lists/strings, only with a numeric index).
			// 好的,我们正在搜索非字符串的东西;
			// 这只能在映射、列表和字符串中完成(列表/字符串只能用数字索引)。
			
			Value baseVal = baseSeq.Val(context);
			if (baseVal is ValMap) {
				Value result = ((ValMap)baseVal).Lookup(idxVal, out valueFoundIn);
				if (valueFoundIn == null) throw new KeyException(idxVal == null ? "null" : idxVal.CodeForm(context.vm, 1));
				return result;
				
			} else if (baseVal is ValList) {
				return ((ValList)baseVal).GetElem(idxVal);
				
			} else if (baseVal is ValString) {
				return ((ValString)baseVal).GetElem(idxVal);
				
			} else if (baseVal is null) {
				throw new TypeException("Null Reference Exception: can't index into null");
			}
				
			throw new TypeException("Type Exception: can't index into this type");
		}

		/// <summary>
		/// 字符串表示
		/// 
		/// string.Format说明:
		/// - 格式化字符串
		/// - {0}, {1}, {2}是占位符
		/// - 按顺序替换为后续参数
		/// 
		/// 格式:
		/// - 普通: sequence[index]
		/// - 地址操作: @sequence[index]
		/// </summary>
		public override string ToString(TAC.Machine vm) {
			return string.Format("{0}{1}[{2}]", noInvoke ? "@" : "", sequence, index);
		}

		/// <summary>
		/// 哈希值(组合序列和索引的哈希)
		/// 
		/// ^运算符: 按位异或,用于组合哈希值
		/// </summary>
		public override int Hash() {
			return sequence.Hash() ^ index.Hash();
		}

		/// <summary>
		/// 相等性检查: 序列和索引都相同才相等
		/// 
		/// ==运算符: 引用相等(对于引用类型)
		/// </summary>
		public override double Equality(Value rhs) {
			return rhs is ValSeqElem && ((ValSeqElem)rhs).sequence == sequence
				&& ((ValSeqElem)rhs).index == index ? 1 : 0;
		}

	}

	/// <summary>
	/// RValueEqualityComparer: 自定义相等比较器
	/// 
	/// IEqualityComparer<T>接口说明:
	/// - 定义相等比较和哈希计算方法
	/// - 用于自定义Dictionary的键比较行为
	/// - 必须实现Equals和GetHashCode方法
	/// 
	/// 用途:
	/// - 让Dictionary使用MiniScript的值语义
	/// - 而不是C#的默认引用语义
	/// - 例如: 两个内容相同的列表应该被认为是相同的键
	/// 
	/// 单例模式:
	/// - 全局只需要一个实例
	/// - 通过静态属性访问
	/// </summary>
	public class RValueEqualityComparer : IEqualityComparer<Value>
	{
		/// <summary>
		/// 实现Equals方法
		/// 
		/// @param val1 - 第一个值
		/// @param val2 - 第二个值
		/// @return 是否相等
		/// </summary>
		public bool Equals(Value val1, Value val2) {
			// 使用MiniScript的Equality方法
			// > 0 表示至少部分相等(支持模糊逻辑)
			return val1.Equality(val2) > 0;
		}

		/// <summary>
		/// 实现GetHashCode方法
		/// 
		/// @param val - 要计算哈希的值
		/// @return 哈希码
		/// </summary>
		public int GetHashCode(Value val) {
			// 使用Value的Hash方法
			return val.Hash();
		}

		// 单例实例
		static RValueEqualityComparer _instance = null;
		
		/// <summary>
		/// 获取单例实例
		/// 
		/// 懒加载模式:
		/// - 第一次访问时创建
		/// - 之后重复使用同一实例
		/// </summary>
		public static RValueEqualityComparer instance {
			get {
				if (_instance == null) _instance = new RValueEqualityComparer();
				return _instance;
			}
		}
	}
	
}