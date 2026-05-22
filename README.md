# Quest OS

Quest OS 是一个实验性的 AArch64 Linux-like kernel bringup 项目。它对照
Linux 的实现思路，在 QEMU `virt` 平台上实现从早期启动、MMU、设备树、中断、
调度、系统调用到 EL0 用户态 demo 的完整运行路径。

仓库同时包含 QLoader 组件。QLoader 可以作为 QEMU 的 `-bios` 镜像启动，完成
早期平台初始化，并在串口菜单中选择启动 Linux 或进入 Quest OS 路径。

更详细的 Linux 机制对照、相同点、差异点和项目亮点见
[`docs/linux-mechanisms.md`](docs/linux-mechanisms.md)。

## 当前状态

仓库仍处于开发阶段。默认 `make` 构建路径可用，但 `qemu_run.sh` 顶部仍
包含面向本机环境的默认路径、NFS rootfs 和 tap 网络配置。换到其他机器运行
前，需要先按自己的环境调整这些变量和网络参数。

## 环境依赖

需要安装 AArch64 交叉编译工具链、Python 3、GNU Make 和 QEMU。以 Debian
或 Ubuntu 环境为例:

```sh
sudo apt install build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu python3 qemu-system-arm
```

默认工具链前缀是 `aarch64-linux-gnu-`。如果你的工具链前缀不同，可以在
构建时覆盖:

```sh
make CROSS_COMPILE=aarch64-none-elf-
```

## 构建

构建默认产物:

```sh
make -j"$(nproc)"
```

只构建 kernel 镜像:

```sh
make -j"$(nproc)" kernel
```

只构建应用程序包:

```sh
make -j"$(nproc)" app
```

清理构建产物:

```sh
make clean
```

也可以先加载本地辅助函数，再调用 `build.sh` 中定义的函数:

```sh
source ./build.sh
build
build_kernel
build_app
clean
```

默认输出目录是 `build/`。常用产物包括:

```text
build/quest_os_kernel.bin
build/quest_os_pack_all.bin
build/app/quest_os_app_pack.elf
```

其中 `quest_os_kernel.bin` 是 loader/kernel 镜像，`quest_os_pack_all.bin`
会额外链接打包后的用户态 demo 程序。

## 使用 QEMU 运行

本地运行脚本:

```sh
sh ./qemu_run.sh -bios build/quest_os_pack_all.bin
```

如果已经 `source ./build.sh`，也可以使用辅助函数:

```sh
run
run_kernel
qemu_info
```

开启 QEMU GDB stub:

```sh
sh ./qemu_run.sh -bios build/quest_os_pack_all.bin debug
```

或调用:

```sh
debug
```

## 运行日志

下面是一次在 QEMU `virt` 上启动 `build/quest_os_pack_all.bin` 并选择
Quest OS 路径后的串口输出节选。重复的用户态 demo 输出已裁剪:

```sh
timeout 10s bash -c '{ sleep 1; printf q; } | qemu-system-aarch64 \
  -M virt,virtualization=false,secure=off,gic-version=3,its=on \
  -m size=1024M \
  -cpu cortex-a72,pmu=on \
  -smp 1 \
  -bios build/quest_os_pack_all.bin \
  -nographic'
```

```text
CurrentEL:EL1
EL2 is not implemented, EL3 is not implemented

=======Welcome to QLoader!=======
Please select boot option:
[L/l]: boot Linux
[Q/q]: boot Quest OS
q
Booting Quest OS...
fixmap_remap_fdt success

===============Welcome to Quest OS!===============
256 SPIs implemented
0 Extended SPIs implemented
Root IRQ handler: 0xFFFF800000006558
GICv3 features: 16 PPIs
gic-v3 init success.
Arch timer freq:62500000 HZ
Arch timer resolution:16 ns
sched_class:sched round robin
Load 2 processes
============= app 1 ============
app 1 running ...
...
============= app 2 ============
app 1 running ...
app 2 running ...
app 1 running ...
app 2 running ...
...
```

在新机器上使用前，请先修改 `qemu_run.sh` 中的这些配置:

- `KERNEL_IMAGE_PATH`: Linux kernel image 路径
- `DTB_PATH`: 设备树路径
- `ROOTFS_PATH`: NFS rootfs 路径
- tap 网卡名称和权限配置

当前脚本在非 root 用户下会通过 `sudo` 重新执行，因为 tap 网络配置通常需要
管理员权限。

## 启动菜单

QLoader 启动后会在串口输出菜单:

```text
[L/l]: boot Linux
[Q/q]: boot Quest OS
```

输入 `L` 会跳转到配置的 Linux 镜像，输入 `Q` 会进入本仓库构建出的
Quest OS 路径。

## 用户态程序包

用户态 demo 位于:

```text
app/src/demo/
```

应用程序包配置文件是:

```text
app/config/app-config.toml
```

`app/tools/proc_pack.py` 会根据配置把 demo ELF 文件打包成 process package，
再通过 `objcopy` 转换成 AArch64 ELF 对象，最终链接进组合镜像。

## 目录结构

```text
app/              用户态 demo、应用链接脚本和打包工具
include/          项目头文件和裁剪/适配后的 Linux、U-Boot 头文件
lib/              libfdt 构建集成
scripts/          通用 Makefile 规则和编译参数
src/              loader、kernel、MM、driver、syscall、scheduler 等源码
thirdparty/       vendored Linux、U-Boot、Linux UAPI、nolibc 子集
build.sh          本地工作流辅助函数
qemu_run.sh       本地 QEMU 启动脚本
```

## 第三方代码

仓库中包含从 Linux、U-Boot、libfdt、Linux UAPI 和 nolibc 选取的文件。源码
文件尽量保留或补充了 SPDX license identifier。第三方组件的来源、版本和
许可证摘要记录在 `THIRD_PARTY_NOTICES.md`。

## License

源码文件使用逐文件 SPDX license identifier 标注许可证。当前项目主体以
GPL-2.0 系列许可证为主，同时包含部分来自上游的 GPL、GPL/BSD 双许可证、
Linux-syscall-note、LGPL/MIT 等文件。授权策略见 `LICENSE`，精确许可证以
各文件头部的 SPDX 标识为准。
