# Makefile for building Lua
# See ../doc/readme.html for installation and customization instructions.
#
# 说明（给没学过 Makefile 的你）：
# - 这是 src/ 目录下的 Makefile，负责实际编译 Lua 源码（与上层 Makefile 配合）。
# - 上层 Makefile 会 cd 进来并执行 `make <平台名>`，例如 `make linux`。
# - 基本结构：target: prerequisites（目标: 依赖） + 若干行命令（recipe）。
# - 变量：NAME= value；引用变量用 $(NAME) 或 $V 这类写法。
# - 行首的 `@`：让 make 不打印该命令本身，只打印命令输出（更干净）。
#
# 下面在"不改变任何功能和代码行为"的前提下，增加大量注释解释每一段做什么。

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================
# == 下面这段是"用户可配置项"，通常只需要改这里 ===============================

# Your platform. See PLATS for possible values.
# 你的目标平台（编译平台）名称。这里默认是 guess（让脚本猜测平台）。
# 通常上层 Makefile 会通过 `make linux` 等方式覆盖它。
PLAT= guess

# C compiler and flags.
# C 编译器及编译选项：
# - CC：指定 C 编译器，这里用 gcc，要求支持 GNU99 标准（-std=gnu99）。
# - CFLAGS：编译器标志，按优先级合并多个变量：
#   * -O2：优化等级 2（平衡速度与编译时间）
#   * -Wall -Wextra：开启大量警告（帮助发现潜在问题）
#   * SYSCFLAGS：系统/平台特定的编译标志（由平台目标设置，例如 -DLUA_USE_LINUX）
#   * MYCFLAGS：用户自定义的额外编译标志
CC= gcc -std=gnu99
CFLAGS= -O2 -Wall -Wextra $(SYSCFLAGS) $(MYCFLAGS)

# Linker flags and libraries.
# 链接器标志与库：
# - LDFLAGS：链接器标志（例如 -s 去除符号表，-Wl,-E 导出符号等）
#   * SYSLDFLAGS：系统/平台特定的链接标志
#   * MYLDFLAGS：用户自定义的额外链接标志
# - LIBS：需要链接的库
#   * -lm：数学库（Lua 需要 math 函数，例如 sin/cos/pow 等）
#   * SYSLIBS：系统/平台特定的库（例如 Linux 的 -ldl 用于动态加载）
#   * MYLIBS：用户自定义的额外库
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm $(SYSLIBS) $(MYLIBS)

# Archiver and related tools.
# 归档工具（用于创建静态库 .a 文件）：
# - AR：归档命令，这里是 `ar rcu`
#   * r：替换归档中的文件（如果已存在）
#   * c：创建归档（如果不存在）
#   * u：只更新比归档中旧的文件（加速增量构建）
# - RANLIB：为归档创建索引（加速链接时的符号查找）
#   某些平台/工具链可能不需要 ranlib（它会自动处理或被 ar 替代）
AR= ar rcu
RANLIB= ranlib

# Other utilities.
# 其他工具命令：
# - RM：删除文件命令，-f 表示强制删除（文件不存在也不报错）
# - UNAME：获取系统名称命令（用于 guess 目标自动检测平台）
RM= rm -f
UNAME= uname

# Platform-specific flags (will be set by platform targets).
# 平台特定的标志（初始为空，由各平台目标覆盖）：
# - SYSCFLAGS：系统/平台特定的编译标志（例如 -DLUA_USE_LINUX）
# - SYSLDFLAGS：系统/平台特定的链接标志（例如 -Wl,-E）
# - SYSLIBS：系统/平台特定的库（例如 -ldl -lreadline）
SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

# User-customizable flags and objects.
# 用户自定义的标志与对象文件（初始为空，用户可按需添加）：
# - MYCFLAGS：用户额外的编译标志
# - MYLDFLAGS：用户额外的链接标志
# - MYLIBS：用户额外的库
# - MYOBJS：用户额外的对象文件（会被链接进 liblua.a）
MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# Special flags for compiler modules; -Os reduces code size.
# 编译器模块的特殊标志：
# - CMCFLAGS：用于词法分析器/语法分析器/代码生成器的额外标志
# - -Os：优化代码大小（这些模块通常较大，用 -Os 可减小体积）
# - 这里默认为空，但在文件末尾有特殊规则为 llex.o/lparser.o/lcode.o 单独应用此标志
CMCFLAGS= 

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======
# == 用户设置结束；以下一般不需要改 ===========================================

# Available platforms.
# 可用平台列表：这些名字既是"可用平台值"，也是 Makefile 中的 target（可直接 make linux）。
# 例如：`make linux` 会触发下面的 Linux 规则。
PLATS= guess aix bsd c89 freebsd generic ios linux macosx mingw posix solaris

# Target files.
# 目标文件定义：
# - LUA_A：Lua 静态库文件名（liblua.a）
# - CORE_O：Lua 核心对象文件列表（虚拟机、编译器、GC、字符串、表等）
# - LIB_O：Lua 标准库对象文件列表（基础库、协程库、IO库、数学库等）
# - BASE_O：核心 + 标准库 + 用户自定义对象（会被打包进 liblua.a）
LUA_A=	liblua.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
BASE_O= $(CORE_O) $(LIB_O) $(MYOBJS)

# Lua interpreter executable.
# Lua 解释器可执行文件：
# - LUA_T：可执行文件名（lua）
# - LUA_O：解释器的对象文件（lua.o，包含 main 函数）
LUA_T=	lua
LUA_O=	lua.o

# Lua compiler executable.
# Lua 编译器可执行文件：
# - LUAC_T：可执行文件名（luac，用于将 Lua 脚本预编译成字节码）
# - LUAC_O：编译器的对象文件（luac.o，包含 main 函数）
LUAC_T=	luac
LUAC_O=	luac.o

# Aggregate targets.
# 聚合目标（方便引用）：
# - ALL_O：所有对象文件（核心 + 标准库 + 解释器 + 编译器 + 用户自定义）
# - ALL_T：所有目标文件（静态库 + 解释器 + 编译器）
# - ALL_A：所有归档文件（这里只有 liblua.a）
ALL_O= $(BASE_O) $(LUA_O) $(LUAC_O)
ALL_T= $(LUA_A) $(LUA_T) $(LUAC_T)
ALL_A= $(LUA_A)

# Targets start here.
# 目标从这里开始。

# Default target: build for detected/specified platform.
# 默认目标：根据检测/指定的平台构建。
# 当你直接运行 `make` 时，默认执行第一个目标（这里是 default）。
# default 依赖 $(PLAT)，因此会继续去执行与平台同名的目标，例如 guess/linux/macosx...
default: $(PLAT)

# Build all targets (library + interpreter + compiler).
# all 目标：构建所有目标（静态库 + 解释器 + 编译器）。
# 依赖 $(ALL_T)，即 liblua.a、lua、luac。
all:	$(ALL_T)

# Build only object files.
# o 目标：只编译对象文件（.o），不链接。
# 用于调试编译过程或快速检查编译错误。
o:	$(ALL_O)

# Build only archive (static library).
# a 目标：只构建归档（静态库 liblua.a）。
# 用于只需要库文件的场景（例如嵌入 Lua 到其他项目）。
a:	$(ALL_A)

# Rule to build the Lua static library.
# 构建 Lua 静态库的规则：
# $(LUA_A) 依赖 $(BASE_O)（所有核心 + 标准库对象文件）。
# - 第一行：用 AR 命令将所有对象文件打包成归档（$@ 是目标名，即 liblua.a）
# - 第二行：用 RANLIB 为归档创建索引（加速链接时的符号查找）
$(LUA_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

# Rule to build the Lua interpreter.
# 构建 Lua 解释器的规则：
# $(LUA_T) 依赖 $(LUA_O)（解释器对象文件）和 $(LUA_A)（静态库）。
# - 链接 lua.o 与 liblua.a，生成可执行文件 lua。
# - $@：目标名（lua）
# - $(LDFLAGS)：链接器标志
# - $(LUA_O)：解释器对象文件（lua.o）
# - $(LUA_A)：静态库（liblua.a）
# - $(LIBS)：需要链接的库（-lm 等）
$(LUA_T): $(LUA_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(LUA_O) $(LUA_A) $(LIBS)

# Rule to build the Lua compiler.
# 构建 Lua 编译器的规则：
# $(LUAC_T) 依赖 $(LUAC_O)（编译器对象文件）和 $(LUA_A)（静态库）。
# - 链接 luac.o 与 liblua.a，生成可执行文件 luac。
# - 结构与 $(LUA_T) 类似。
$(LUAC_T): $(LUAC_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(LUAC_O) $(LUA_A) $(LIBS)

# Test target: run Lua interpreter to check version.
# test 目标：运行 Lua 解释器检查版本（简单的"冒烟测试"）。
# - ./$(LUA_T) -v：执行 lua -v 打印版本信息
# - 如果 lua 可执行且能正常运行，说明构建基本成功
test:
	./$(LUA_T) -v

# Clean target: remove all generated files.
# clean 目标：删除所有生成的文件（对象文件 + 可执行文件 + 静态库）。
# - $(RM) $(ALL_T) $(ALL_O)：删除所有目标文件和对象文件
# - 用于"清理构建"，然后重新编译
clean:
	$(RM) $(ALL_T) $(ALL_O)

# Dependency generation target.
# depend 目标：生成依赖关系（用于自动更新 Makefile 中的依赖列表）。
# - @$(CC) $(CFLAGS) -MM l*.c：对所有 l*.c 文件生成 Makefile 依赖规则
# - -MM：生成依赖，但不包含系统头文件（只包含项目内头文件）
# - 输出类似："lapi.o: lapi.c lprefix.h lua.h ..."
# - 这个输出可以手动复制到 Makefile 末尾（本文件末尾已有依赖列表）
depend:
	@$(CC) $(CFLAGS) -MM l*.c

# Echo configuration target.
# echo 目标：打印当前配置参数，便于调试/确认编译选项。
# - 打印所有关键变量的值（平台、编译器、标志、库等）
echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"
	@echo "UNAME= $(UNAME)"

# Convenience targets for popular platforms.
# 便捷的平台目标（下面是各平台的具体配置）。

# ALL variable for reuse.
# ALL 变量：用于复用 all 目标（大多数平台目标都是 `$(MAKE) $(ALL) ...`）。
ALL= all

# Help target: show available platforms.
# help 目标：显示可用平台列表和使用说明。
# - 打印所有 PLATS 变量中的平台名
# - 提示用户查看文档获取完整说明
help:
	@echo "Do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"
	@echo "See doc/readme.html for complete instructions."

# Guess platform automatically.
# guess 目标：自动检测平台并构建。
# - 第一步：打印正在猜测的系统名（通过 uname 获取）
# - 第二步：递归调用 make，目标名为 uname 的输出（例如 Linux/Darwin/FreeBSD）
# - 这样可以自动匹配下面的平台目标（Linux/Darwin 等）
guess:
	@echo Guessing `$(UNAME)`
	@$(MAKE) `$(UNAME)`

# AIX platform target.
# AIX 目标：IBM AIX 系统的编译配置。
# - CC="xlc"：使用 IBM XL C 编译器
# - CFLAGS="-O2 -DLUA_USE_POSIX -DLUA_USE_DLOPEN"：
#   * -DLUA_USE_POSIX：启用 POSIX API（例如文件操作）
#   * -DLUA_USE_DLOPEN：启用动态加载（loadlib 功能）
# - SYSLIBS="-ldl"：链接动态加载库
# - SYSLDFLAGS="-brtl -bexpall"：AIX 特定的链接器标志
#   * -brtl：运行时链接
#   * -bexpall：导出所有符号
AIX aix:
	$(MAKE) $(ALL) CC="xlc" CFLAGS="-O2 -DLUA_USE_POSIX -DLUA_USE_DLOPEN" SYSLIBS="-ldl" SYSLDFLAGS="-brtl -bexpall"

# BSD platform target.
# bsd 目标：通用 BSD 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN"：启用 POSIX 和动态加载
# - SYSLIBS="-Wl,-E"：链接时导出符号（-E）
#   * -Wl,：将后面的选项传递给链接器
#   * -E：导出所有符号（让动态加载的模块能访问 Lua API）
bsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN" SYSLIBS="-Wl,-E"

# C89 standard target.
# c89 目标：使用 C89 标准编译（严格的 ANSI C）。
# - SYSCFLAGS="-DLUA_USE_C89"：告诉 Lua 使用 C89 兼容模式
# - CC="gcc -std=c89"：强制 gcc 使用 C89 标准
# - 构建后打印警告：C89 不保证 64 位整数支持（Lua 需要 64 位整数）
#   提示用户确保所有外部库也用 LUA_USE_C89 编译（保持一致性）
c89:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_C89" CC="gcc -std=c89"
	@echo ''
	@echo '*** C89 does not guarantee 64-bit integers for Lua.'
	@echo '*** Make sure to compile all external Lua libraries'
	@echo '*** with LUA_USE_C89 to ensure consistency'
	@echo ''

# FreeBSD/NetBSD/OpenBSD platform target.
# freebsd 目标：FreeBSD/NetBSD/OpenBSD 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_LINUX -DLUA_USE_READLINE -I/usr/include/edit"：
#   * -DLUA_USE_LINUX：使用 Linux 兼容的 API（BSD 通常兼容）
#   * -DLUA_USE_READLINE：启用 readline 库（交互式命令行编辑）
#   * -I/usr/include/edit：包含 editline 头文件（BSD 用 editline 替代 readline）
# - SYSLIBS="-Wl,-E -ledit"：
#   * -Wl,-E：导出符号
#   * -ledit：链接 editline 库
# - CC="cc"：使用系统默认 C 编译器（通常是 clang）
FreeBSD NetBSD OpenBSD freebsd:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX -DLUA_USE_READLINE -I/usr/include/edit" SYSLIBS="-Wl,-E -ledit" CC="cc"

# Generic platform target.
# generic 目标：通用/最小配置（不依赖任何平台特性）。
# - 只构建 $(ALL)，不添加任何平台特定标志
# - 可移植性最好，但功能最少（例如没有动态加载、readline 等）
generic: $(ALL)

# iOS platform target.
# ios 目标：iOS 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_IOS"：启用 iOS 特定的 API
# - 通常用于嵌入 Lua 到 iOS 应用中
ios:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_IOS"

# Linux platform target.
# Linux 目标：Linux 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_LINUX"：启用 Linux 特定的 API
# - SYSLIBS="-Wl,-E -ldl"：
#   * -Wl,-E：导出符号
#   * -ldl：链接动态加载库（dlopen/dlsym）
Linux linux:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_LINUX" SYSLIBS="-Wl,-E -ldl"

# macOS platform target.
# macosx 目标：macOS 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_MACOSX -DLUA_USE_READLINE"：
#   * -DLUA_USE_MACOSX：启用 macOS 特定的 API
#   * -DLUA_USE_READLINE：启用 readline 库
# - SYSLIBS="-lreadline"：链接 readline 库（macOS 自带）
Darwin macos macosx:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_MACOSX -DLUA_USE_READLINE" SYSLIBS="-lreadline"

# Windows (MinGW) platform target.
# mingw 目标：Windows 系统（使用 MinGW 编译器）的编译配置。
# - 特殊点：Windows 上 Lua 通常构建成 DLL（动态链接库），而不是静态库。
# - 第一步：构建 lua.exe（解释器）
#   * "LUA_A=lua55.dll"：将静态库目标改为 DLL
#   * "LUA_T=lua.exe"：解释器名称（Windows 需要 .exe 后缀）
#   * "AR=$(CC) -shared -o"：用编译器创建 DLL（而不是 ar 创建 .a）
#   * "RANLIB=strip --strip-unneeded"：用 strip 去除不必要的符号（减小体积）
#   * "SYSCFLAGS=-DLUA_BUILD_AS_DLL"：告诉 Lua 构建为 DLL
#   * "SYSLIBS="：Windows 不需要额外的系统库
#   * "SYSLDFLAGS=-s"：链接时去除符号表（减小体积）
# - 第二步：构建 luac.exe（编译器）
#   * "LUAC_T=luac.exe"：编译器名称
mingw:
	$(MAKE) "LUA_A=lua55.dll" "LUA_T=lua.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-DLUA_BUILD_AS_DLL" "SYSLIBS=" "SYSLDFLAGS=-s" lua.exe
	$(MAKE) "LUAC_T=luac.exe" luac.exe

# POSIX platform target.
# posix 目标：通用 POSIX 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_POSIX"：只启用 POSIX API（不包含动态加载等扩展）
# - 比 generic 功能多一些（例如文件操作），但比 Linux/BSD 少一些
posix:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_POSIX"

# Solaris platform target.
# solaris 目标：Solaris 系统的编译配置。
# - SYSCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN -D_REENTRANT"：
#   * -DLUA_USE_POSIX：启用 POSIX API
#   * -DLUA_USE_DLOPEN：启用动态加载
#   * -D_REENTRANT：启用线程安全（Solaris 的多线程支持）
# - SYSLIBS="-ldl"：链接动态加载库
SunOS solaris:
	$(MAKE) $(ALL) SYSCFLAGS="-DLUA_USE_POSIX -DLUA_USE_DLOPEN -D_REENTRANT" SYSLIBS="-ldl"

# Targets that do not create files (not all makes understand .PHONY).
# .PHONY 声明：这些目标不是生成某个同名文件，而是"总要执行"的伪目标。
# 这能避免目录中存在同名文件/目录时 make 误判"已是最新"而跳过执行。
.PHONY: all $(PLATS) help test clean default o a depend echo

# Compiler modules may use special flags.
# 编译器模块的特殊编译规则：
# 这些模块（词法分析器、语法分析器、代码生成器）通常较大，
# 使用 CMCFLAGS（例如 -Os）可以减小代码体积。
#
# 这些规则覆盖了 make 的默认隐式规则（.c -> .o）：
# - 正常情况下，make 会用 $(CC) $(CFLAGS) -c xxx.c 编译 .c 文件
# - 这里显式指定规则，添加 $(CMCFLAGS) 额外标志

# llex.o: Lexical analyzer (词法分析器).
# 将 llex.c 编译为 llex.o，使用 CMCFLAGS 额外标志。
llex.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c llex.c

# lparser.o: Parser (语法分析器).
# 将 lparser.c 编译为 lparser.o，使用 CMCFLAGS 额外标志。
lparser.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lparser.c

# lcode.o: Code generator (代码生成器).
# 将 lcode.c 编译为 lcode.o，使用 CMCFLAGS 额外标志。
lcode.o:
	$(CC) $(CFLAGS) $(CMCFLAGS) -c lcode.c

# DO NOT DELETE
# 下面是自动生成的依赖关系列表（由 `make depend` 生成）。
# 格式：目标文件: 源文件 头文件1 头文件2 ...
# 作用：让 make 知道"当某个头文件改变时，哪些 .o 文件需要重新编译"。
#
# 例如：lapi.o: lapi.c lprefix.h lua.h luaconf.h lapi.h ...
# 表示 lapi.o 依赖 lapi.c 和一堆头文件，任何一个改变都会触发 lapi.o 重新编译。

lapi.o: lapi.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h \
 ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.c lprefix.h lua.h luaconf.h lauxlib.h llimits.h
lbaselib.o: lbaselib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
lcode.o: lcode.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lgc.h lstring.h ltable.h lvm.h lopnames.h
lcorolib.o: lcorolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
lctype.o: lctype.c lprefix.h lctype.h lua.h luaconf.h llimits.h
ldblib.o: ldblib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h llimits.h
ldebug.o: ldebug.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h \
 ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lvm.h
ldo.o: ldo.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h \
 lparser.h lstring.h ltable.h lundump.h lvm.h
ldump.o: ldump.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h lgc.h ltable.h lundump.h
lfunc.o: lfunc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h
lgc.o: lgc.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
linit.o: linit.c lprefix.h lua.h luaconf.h lualib.h lauxlib.h llimits.h
liolib.o: liolib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h llimits.h
llex.o: llex.c lprefix.h lua.h luaconf.h lctype.h llimits.h ldebug.h \
 lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lgc.h llex.h lparser.h \
 lstring.h ltable.h
lmathlib.o: lmathlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
lmem.o: lmem.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h
loadlib.o: loadlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
lobject.o: lobject.c lprefix.h lua.h luaconf.h lctype.h llimits.h \
 ldebug.h lstate.h lobject.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h \
 lvm.h
lopcodes.o: lopcodes.c lprefix.h lopcodes.h llimits.h lua.h luaconf.h \
 lobject.h
loslib.o: loslib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h llimits.h
lparser.o: lparser.c lprefix.h lua.h luaconf.h lcode.h llex.h lobject.h \
 llimits.h lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h \
 ldo.h lfunc.h lstring.h lgc.h ltable.h
lstate.o: lstate.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h \
 lstring.h ltable.h
lstring.o: lstring.c lprefix.h lua.h luaconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h
lstrlib.o: lstrlib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
ltable.o: ltable.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
ltablib.o: ltablib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
ltm.o: ltm.c lprefix.h lua.h luaconf.h ldebug.h lstate.h lobject.h \
 llimits.h ltm.h lzio.h lmem.h ldo.h lgc.h lstring.h ltable.h lvm.h
lua.o: lua.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h llimits.h
luac.o: luac.c lprefix.h lua.h luaconf.h lauxlib.h lapi.h llimits.h \
 lstate.h lobject.h ltm.h lzio.h lmem.h ldebug.h lopcodes.h lopnames.h \
 lundump.h
lundump.o: lundump.c lprefix.h lua.h luaconf.h ldebug.h lstate.h \
 lobject.h llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h \
 ltable.h lundump.h
lutf8lib.o: lutf8lib.c lprefix.h lua.h luaconf.h lauxlib.h lualib.h \
 llimits.h
lvm.o: lvm.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lopcodes.h \
 lstring.h ltable.h lvm.h ljumptab.h
lzio.o: lzio.c lprefix.h lua.h luaconf.h lapi.h llimits.h lstate.h \
 lobject.h ltm.h lzio.h lmem.h

# (end of Makefile)
