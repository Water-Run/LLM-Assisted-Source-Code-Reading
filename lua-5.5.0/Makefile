# Makefile for installing Lua
# See doc/readme.html for installation and customization instructions.
#
# 说明（给没学过 Makefile 的你）：
# - Makefile 是“构建/安装脚本”，由 make 读取并执行。
# - 基本结构：target: prerequisites（目标: 依赖） + 若干行命令（recipe）。
# - 执行：`make <target>`，例如 `make`, `make linux`, `make install`。
# - 变量：NAME= value；引用变量用 $(NAME) 或 $V 这类写法（本文件两种都用）。
# - 行首的 `@`：让 make 不打印该命令本身，只打印命令输出（更干净）。
#
# 下面是在“不改变任何功能和代码行为”的前提下，增加大量注释解释每一段做什么。

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================
# == 下面这段是“用户可配置项”，通常只需要改这里 ===============================

# Your platform. See PLATS for possible values.
# 你的目标平台（编译平台）名称。这里默认是 guess（让 src/Makefile 去猜测平台）。
# 运行 `make linux` 或 `make macosx` 会覆盖它（因为 all: $(PLAT)）。
PLAT= guess

# Where to install.
# 安装到哪里：Lua 的安装会在 src/ 和 doc/ 目录中执行复制/创建目录等动作。
# 注意：如果 INSTALL_TOP 不是绝对路径，且你在别的目录执行 make，路径可能会出乎意料。
# （所以一般写 /usr/local 这种绝对路径更安全）
#
# 你可能希望 INSTALL_LMOD / INSTALL_CMOD 与 luaconf.h 中的 LUA_ROOT/LUA_LDIR/LUA_CDIR 保持一致。
# - INSTALL_LMOD：Lua 纯脚本模块（.lua）的安装位置
# - INSTALL_CMOD：Lua C 扩展模块（.so/.dll 等）的安装位置
INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin          # 可执行文件安装目录：lua, luac
INSTALL_INC= $(INSTALL_TOP)/include      # 头文件安装目录：lua.h 等
INSTALL_LIB= $(INSTALL_TOP)/lib          # 库文件安装目录：liblua.a
INSTALL_MAN= $(INSTALL_TOP)/man/man1     # man 手册安装目录：lua.1 luac.1
INSTALL_LMOD= $(INSTALL_TOP)/share/lua/$V  # Lua 脚本模块目录（按版本分层）
INSTALL_CMOD= $(INSTALL_TOP)/lib/lua/$V    # C 模块目录（按版本分层）

# How to install.
# “安装”具体用哪个命令：通常使用系统的 install 工具。
# - install -p：保留时间戳等属性（不同实现可能略有差异）
# - INSTALL_EXEC：安装可执行文件时使用（0755：可执行）
# - INSTALL_DATA：安装数据/头文件/文档时使用（0644：只读数据）
#
# 备注：如果你的 install 程序不支持 -p，那么安装 liblua.a 后可能需要 ranlib（这在部分平台/工具链上需要）。
INSTALL= install -p
INSTALL_EXEC= $(INSTALL) -m 0755
INSTALL_DATA= $(INSTALL) -m 0644
#
# If you don't have "install" you can use "cp" instead.
# 如果系统没有 install，可以退化成 cp。
# 注意：这会改变文件权限/属性处理方式，但这是原 Makefile 提供的可选方案。
# INSTALL= cp -p
# INSTALL_EXEC= $(INSTALL)
# INSTALL_DATA= $(INSTALL)

# Other utilities.
# 其他工具命令变量化：便于跨平台替换（例如某些系统 mkdir 参数不同）。
MKDIR= mkdir -p   # mkdir -p：递归创建目录，已存在则不报错
RM= rm -f         # rm -f：强制删除文件，不存在也不报错

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======
# == 用户设置结束；以下一般不需要改 ===========================================

# Convenience platforms targets.
# 平台目标集合：这些名字既是“可用平台值”，也是 Makefile 中的 target（可直接 make linux）。
# 例如：`make linux` 会触发下面的规则：$(PLATS) help test clean: ...
PLATS= guess aix bsd c89 freebsd generic ios linux macosx mingw posix solaris

# What to install.
# 需要安装的文件列表，供 install/uninstall 规则复用。
# - TO_BIN：最终产生的可执行文件
# - TO_INC：需要安装的头文件
# - TO_LIB：静态库（这里是 liblua.a）
# - TO_MAN：man 手册
TO_BIN= lua luac
TO_INC= lua.h luaconf.h lualib.h lauxlib.h lua.hpp
TO_LIB= liblua.a
TO_MAN= lua.1 luac.1

# Lua version and release.
# Lua 版本号变量：
# - V：主版本号（这里是 5.5）
# - R：发布版本号（这里是 5.5.0）
#
# 注意：这里用的是 `$V` / `$R` 这种写法，而不是 $(V)。
# 在 GNU make 中：`$V` 等价于 `$(V)`（单字符变量引用）。
V= 5.5
R= $V.0

# Targets start here.
# 目标从这里开始。

# 默认目标：all
# 当你直接运行 `make` 时，默认执行第一个目标（这里是 all）。
# all 依赖 $(PLAT)，因此会继续去执行与平台同名的目标，例如 guess/linux/macosx...
all:	$(PLAT)

# 平台目标 + help/test/clean 的统一转发规则
# 这条规则一次性声明多个 target：$(PLATS) help test clean
# 含义：无论你执行 `make linux` 还是 `make test`，都会进入 src 目录并执行 `make <同名目标>`。
#
# - `$@`：make 的自动变量，表示“当前目标名”。
#   例如你运行 `make test`，那么 `$@` 就是 test；
#   运行 `make linux`，那么 `$@` 就是 linux。
# - `$(MAKE)`：递归调用 make 的推荐写法（会带上 make 的一些内部标志）。
# - `@`：不打印该命令本身（但命令输出仍会显示）。
$(PLATS) help test clean:
	@cd src && $(MAKE) $@

# install 目标：安装 Lua 到系统目录
# `install: dummy` 表示 install 依赖 dummy（一个“空目标”，用于避免目录名 install/ 干扰）
# 下面每一行都是一个 shell 命令：先进入 src，再创建目录，再拷贝文件；最后进入 doc 拷贝 man 手册。
install: dummy
	# 在 src 目录里创建所有需要的安装目录（bin/include/lib/man/lua模块目录/c模块目录）
	cd src && $(MKDIR) $(INSTALL_BIN) $(INSTALL_INC) $(INSTALL_LIB) $(INSTALL_MAN) $(INSTALL_LMOD) $(INSTALL_CMOD)
	# 安装可执行文件 lua/luac 到 INSTALL_BIN
	cd src && $(INSTALL_EXEC) $(TO_BIN) $(INSTALL_BIN)
	# 安装头文件到 INSTALL_INC
	cd src && $(INSTALL_DATA) $(TO_INC) $(INSTALL_INC)
	# 安装静态库到 INSTALL_LIB
	cd src && $(INSTALL_DATA) $(TO_LIB) $(INSTALL_LIB)
	# 安装 man 手册到 INSTALL_MAN（注意：此步在 doc 目录执行）
	cd doc && $(INSTALL_DATA) $(TO_MAN) $(INSTALL_MAN)

# uninstall 目标：卸载
# 与 install 相反：进入安装目录并删除对应文件列表。
# 这里用 `cd src && cd $(INSTALL_BIN)` 这种写法，是为了复用从 src 发起的相对路径逻辑；
# 但本质上第二个 cd 后就到了安装目录。
uninstall:
	# 删除安装在 bin 里的可执行文件（lua luac）
	cd src && cd $(INSTALL_BIN) && $(RM) $(TO_BIN)
	# 删除安装在 include 里的头文件
	cd src && cd $(INSTALL_INC) && $(RM) $(TO_INC)
	# 删除安装在 lib 里的静态库
	cd src && cd $(INSTALL_LIB) && $(RM) $(TO_LIB)
	# 删除安装在 man/man1 里的手册
	cd doc && cd $(INSTALL_MAN) && $(RM) $(TO_MAN)

# local 目标：本地安装到 ../install（不写入系统目录）
# 关键点：这不是修改变量本身，而是“对这次递归 make 调用”临时覆盖 INSTALL_TOP。
local:
	$(MAKE) install INSTALL_TOP=../install

# make may get confused with install/ if it does not support .PHONY.
# dummy 目标：空目标（没有 recipe），仅用于“占位/避免歧义”。
# 背景：若目录中存在名为 install 的文件或目录，且 make 不完全理解 .PHONY，
# 可能会判断 install 已是“最新”而不执行命令。通过依赖 dummy 可规避部分实现的问题。
dummy:

# Echo config parameters.
# echo 目标：打印当前配置参数，便于你确认实际使用的路径/变量值。
# - 第一行：进入 src，执行其 Makefile 中的 echo（用 -s 静默 make 的命令回显）
# - 后续：打印本 Makefile 的关键变量
echo:
	@cd src && $(MAKE) -s echo
	@echo "PLAT= $(PLAT)"
	@echo "V= $V"
	@echo "R= $R"
	@echo "TO_BIN= $(TO_BIN)"
	@echo "TO_INC= $(TO_INC)"
	@echo "TO_LIB= $(TO_LIB)"
	@echo "TO_MAN= $(TO_MAN)"
	@echo "INSTALL_TOP= $(INSTALL_TOP)"
	@echo "INSTALL_BIN= $(INSTALL_BIN)"
	@echo "INSTALL_INC= $(INSTALL_INC)"
	@echo "INSTALL_LIB= $(INSTALL_LIB)"
	@echo "INSTALL_MAN= $(INSTALL_MAN)"
	@echo "INSTALL_LMOD= $(INSTALL_LMOD)"
	@echo "INSTALL_CMOD= $(INSTALL_CMOD)"
	@echo "INSTALL_EXEC= $(INSTALL_EXEC)"
	@echo "INSTALL_DATA= $(INSTALL_DATA)"

# Echo pkg-config data.
# pc 目标：输出用于 pkg-config 的关键字段（可被重定向到 .pc 文件）。
# 这通常用于让其他项目用 `pkg-config --cflags --libs lua` 类似方式找到 Lua。
pc:
	@echo "version=$R"
	@echo "prefix=$(INSTALL_TOP)"
	@echo "libdir=$(INSTALL_LIB)"
	@echo "includedir=$(INSTALL_INC)"

# Targets that do not create files (not all makes understand .PHONY).
# .PHONY 声明：这些目标不是生成某个同名文件，而是“总要执行”的伪目标。
# 这能避免目录中存在同名文件/目录时 make 误判“已是最新”而跳过执行。
.PHONY: all $(PLATS) help test clean install uninstall local dummy echo pc

# (end of Makefile)
