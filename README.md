# Quest OS

Quest OS 是一个实验性的 AArch64 Linux-like kernel bringup 项目。它对照
Linux 的实现思路，在 QEMU `virt` 平台上实现从早期启动、MMU、设备树、中断、
SMP、调度、系统调用到 EL0 用户态 demo 的完整运行路径。

仓库同时包含 QLoader 组件。QLoader 可以作为 QEMU 的 `-bios` 镜像启动，完成
早期平台初始化，并在串口菜单中选择启动 Linux 或进入 Quest OS 路径。

更详细的 Linux 机制对照、相同点、差异点和项目亮点见
[`docs/linux-mechanisms.md`](docs/linux-mechanisms.md)。

## 当前状态

仓库仍处于开发阶段。默认 `make` 会构建 Quest OS/QLoader、用户态 demo 包，
并调用 TF-A 生成 QEMU 固件镜像 `build/qemu_fw.bios`。默认运行路径是
`qemu_fw.bios` + QEMU `secure=on`，也保留了直接运行 kernel 的
`direct_kernel` 路径用于验证 QEMU `secure=off` + HVC PSCI。

`qemu_run.sh` 顶部仍保留了一些面向本机环境的默认配置；Linux 启动路径额外
依赖本地 Linux kernel image、NFS rootfs 和 tap 网络参数，换到其他机器运行前
需要按实际环境调整。

## 环境依赖

需要安装 AArch64 交叉编译工具链、Python 3、GNU Make、OpenSSL 开发包和
QEMU。以 Debian 或 Ubuntu 环境为例:

```sh
sudo apt install build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu libssl-dev python3 qemu-system-arm
```

首次 clone 后需要拉取 TF-A submodule:

```sh
git submodule update --init --recursive
```

默认工具链前缀是 `aarch64-linux-gnu-`。如果你的工具链前缀不同，可以在
构建时覆盖:

```sh
make CROSS_COMPILE=aarch64-none-elf-
```

## 构建

构建默认 TF-A 固件启动路径:

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

只构建直接运行 kernel 的镜像:

```sh
make -j"$(nproc)" direct_kernel
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
build_direct_kernel
build_app
clean
```

默认输出目录是 `build/`。常用产物包括:

```text
build/quest_os_kernel.bin
build/quest_os_pack_all.bin
build/bl1.bin
build/bl2.bin
build/bl31.bin
build/fip.bin
build/qemu_fw.bios
build/direct_kernel/quest_os_kernel.bin
build/app/quest_os_app_pack.elf
```

其中 `quest_os_kernel.bin` 是 loader/kernel 镜像，`quest_os_pack_all.bin`
会额外链接打包后的用户态 demo 程序，并作为 TF-A 的 BL33 打进 FIP。
`bl1.bin`、`bl2.bin`、`bl31.bin`、`fip.bin` 和 `qemu_fw.bios` 来自 TF-A
QEMU platform 构建。`build/direct_kernel/quest_os_kernel.bin` 是直接运行
kernel 时使用的 HVC PSCI 版本，默认不包含用户态 demo package。

## 使用 QEMU 运行

本地运行脚本:

```sh
sh ./qemu_run.sh -bios build/qemu_fw.bios
```

直接运行 kernel 镜像需要使用 HVC PSCI，并关闭 QEMU secure 模式:

```sh
make -j"$(nproc)" direct_kernel
sh ./qemu_run.sh --secure=off -bios build/direct_kernel/quest_os_kernel.bin
```

如果已经 `source ./build.sh`，也可以使用辅助函数:

```sh
run
run_kernel
qemu_info
```

开启 QEMU GDB stub:

```sh
sh ./qemu_run.sh -bios build/qemu_fw.bios debug
```

或调用:

```sh
debug
```

## 运行日志

下面是一次在 QEMU `virt` 上启动 `build/qemu_fw.bios` 并选择 Quest OS 路径后
的串口输出节选:

```sh
timeout 10s bash -c '{ sleep 2; printf q; sleep 2; printf x; } | qemu-system-aarch64 \
  -M virt,virtualization=false,secure=on,gic-version=3,its=on \
  -m size=1024M \
  -cpu cortex-a72,pmu=on \
  -smp 4 \
  -bios build/qemu_fw.bios \
  -nographic'
```

```text
NOTICE:  Booting Trusted Firmware
NOTICE:  BL1: v2.15.0(debug):v2.15.0-274-gc64fe42d7
NOTICE:  BL1: Built : 00:17:27, Jul  7 2026
INFO:    BL1: RAM 0xe0ee000 - 0xe0f6000
INFO:    BL1: cortex_a72: CPU workaround for erratum 3 was applied
WARNING: BL1: cortex_a72: CPU workaround for erratum 859971 was missing!
WARNING: BL1: cortex_a72: CPU workaround for erratum 1319367 was missing!
INFO:    BL1: cortex_a72: CPU workaround for CVE 2017_5715 was applied
INFO:    BL1: cortex_a72: CPU workaround for CVE 2018_3639 was applied
INFO:    BL1: cortex_a72: CPU workaround for CVE 2022_23960 was applied
INFO:    BL1: Loading BL2
INFO:    Loading image id=1 at address 0xe05b000
INFO:    Image id=1 loaded: 0xe05b000 - 0xe063204
NOTICE:  BL1: Booting BL2
INFO:    Entry point address = 0xe05b000
INFO:    SPSR = 0x3c5
NOTICE:  BL2: v2.15.0(debug):v2.15.0-274-gc64fe42d7
NOTICE:  BL2: Built : 00:17:28, Jul  7 2026
INFO:    BL2: Doing platform setup
INFO:    BL2: Loading image id 3
INFO:    Loading image id=3 at address 0xe090000
INFO:    Image id=3 loaded: 0xe090000 - 0xe0a089b
INFO:    BL2: Loading image id 5
INFO:    Loading image id=5 at address 0x60000000
INFO:    Image id=5 loaded: 0x60000000 - 0x60142000
NOTICE:  BL1: Booting BL31
INFO:    Entry point address = 0xe090000
INFO:    SPSR = 0x3cd
NOTICE:  BL31: v2.15.0(debug):v2.15.0-274-gc64fe42d7
NOTICE:  BL31: Built : 00:17:28, Jul  7 2026
INFO:    GICv3 without legacy support detected.
INFO:    ARM GICv3 driver initialized in EL3
INFO:    Maximum SPI INTID supported: 287
INFO:    BL31: Initializing runtime services
INFO:    BL31: cortex_a72: CPU workaround for erratum 3 was applied
WARNING: BL31: cortex_a72: CPU workaround for erratum 859971 was missing!
WARNING: BL31: cortex_a72: CPU workaround for erratum 1319367 was missing!
INFO:    BL31: cortex_a72: CPU workaround for CVE 2017_5715 was applied
INFO:    BL31: cortex_a72: CPU workaround for CVE 2018_3639 was applied
INFO:    BL31: cortex_a72: CPU workaround for CVE 2022_23960 was applied
INFO:    BL31: Preparing for EL3 exit to normal world
INFO:    Entry point address = 0x60000000
INFO:    SPSR = 0x3c5

CurrentEL:EL1
EL2 is not implemented, EL3 can be executed in either AArch64 or AArch32 state

=======Welcome to QLoader!=======
Please select boot option:
[L/l]: boot Linux
[Q/q]: boot Quest OS
q
Booting Quest OS...
fixmap_remap_fdt success
per_cpu init success.

===============Welcome to Quest OS!===============
256 SPIs implemented
0 Extended SPIs implemented
GICv3 features: 16 PPIs
GICD_CTLR.DS=0, SCR_EL3.FIQ=0
CPU0: found redistributor 0 region 0:0xFFFFFFFFFF0FC000
gic-v3 init success.
Arch timer freq:62500000 HZ
Arch timer resolution:16 ns
sched_class:sched round robin
CPU1: found redistributor 1 region 0:0xFFFFFFFFFF11C000
CPU2: found redistributor 2 region 0:0xFFFFFFFFFF13C000
CPU3: found redistributor 3 region 0:0xFFFFFFFFFF15C000
Loaded 10 user processes

=========Press any key to start user processes=========

cpu[0]: ============= app 1 ============
cpu[2]: ============= app 3 ============
cpu[3]: ============= app 4 ============
cpu[1]: ============= app 2 ============
cpu[3]: app 4 running ...
cpu[2]: app 3 running ...
cpu[1]: app 2 running ...
cpu[3]: ============= app 8 ============
cpu[0]: ============= app 5 ============
cpu[2]: ============= app 7 ============
cpu[1]: ============= app 6 ============
cpu[0]: ============= app 9 ============
cpu[1]: ============= app 10 ============
...
cpu[0]: app 9 running ...
cpu[1]: app 10 running ...
...
```

在新机器上使用前，请先修改 `qemu_run.sh` 中的这些配置:

- `KERNEL_IMAGE_PATH`: Linux kernel image 路径
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
再通过 `objcopy` 转换成 AArch64 ELF 对象，最终链接进组合镜像。当前默认
配置会打包 `app1` 到 `app10`，用于验证进程数量多于 CPU 数量时的调度路径。

## 目录结构

```text
app/              用户态 demo、应用链接脚本和打包工具
include/          项目头文件和裁剪/适配后的 Linux、U-Boot 头文件
lib/              libfdt 构建集成
scripts/          通用 Makefile 规则和编译参数
src/              loader、kernel、MM、driver、syscall、scheduler 等源码
thirdparty/       vendored Linux、U-Boot、Linux UAPI、nolibc 子集和 TF-A submodule
build.sh          本地工作流辅助函数
qemu_run.sh       本地 QEMU 启动脚本
```

## 第三方代码

仓库中包含从 Linux、U-Boot、libfdt、Linux UAPI 和 nolibc 选取的文件，并通过
`thirdparty/tf-a` submodule 引入 Trusted Firmware-A。源码文件尽量保留或补充了
SPDX license identifier。第三方组件的来源、版本和许可证摘要记录在
`THIRD_PARTY_NOTICES.md`。

## License

源码文件使用逐文件 SPDX license identifier 标注许可证。当前项目主体以
GPL-2.0 系列许可证为主，同时包含部分来自上游的 GPL、GPL/BSD 双许可证、
Linux-syscall-note、LGPL/MIT 等文件。授权策略见 `LICENSE`，精确许可证以
各文件头部的 SPDX 标识为准。
