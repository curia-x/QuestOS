# Linux 机制实现说明

Quest OS 是一个 AArch64 bringup 实验项目。项目聚焦 Linux-like 内核的早期
启动和基础运行路径: 从 reset 入口开始，依次完成 MMU、设备树、中断、调度、
系统调用和 EL0 用户态 demo。

仓库保留了从 QEMU `-bios` 入口到 EL0 demo 输出的完整路径，便于逐步跟踪
boot 参数、页表切换、中断进入、系统调用和进程切换等状态变化。

## 项目主线

项目由两部分组成:

- `QLoader`: 一个 AArch64 loader，可以作为 QEMU `-bios` 镜像启动。它完成早期
  EL/MMU/重定位/串口/FDT 初始化，并能按 Linux arm64 boot protocol 跳转 Linux。
- `Quest OS`: 仓库内置的小型内核路径。它复刻 Linux arm64 bringup 的关键阶段，
  初始化内存、中断、timer、调度器和 syscall，然后运行打包进镜像的用户态 demo。

两条路径共用早期平台初始化和 FDT 处理逻辑。Linux 路径用于校验 boot protocol
和 loader 行为，Quest OS 路径用于运行仓库内核实现。

## 已实现的 Linux 机制

| 机制 | 项目中的实现 | 对应的 Linux 思路 | 技术重点 |
| --- | --- | --- | --- |
| Linux arm64 boot protocol | QLoader 跳转 Linux 前关闭 MMU、传递 FDT、清理参数寄存器并进入 kernel entry | Linux arm64 对 bootloader 的入口约定 | 启动环境约束、FDT 传递、MMU 状态管理 |
| AArch64 head bringup | 汇编入口完成异常级别处理、idmap、TCR/MAIR/TTBR 配置、MMU 开启、BSS 清零和进入 C | Linux `head.S` 的启动阶段划分 | 早期执行环境、地址切换、cache/TLB 同步 |
| 镜像重定位 | QLoader 在早期页表环境下把镜像搬移到目标地址并处理相对重定位 | Linux/bootloader 都需要解决的运行地址和链接地址问题 | 位置相关代码、重定位、运行地址管理 |
| 设备树 | 使用 libfdt 解析 `compatible`、`reg`、`ranges`、`/chosen`，并把 FDT 传给 Linux 或 Quest OS | Linux 用 DTB 描述平台资源 | 平台资源发现、地址翻译、bootargs 管理 |
| memblock | 用 memblock 管理早期可用内存和 reserved 区域，保留内核 text/rodata/init/data 等段 | Linux early boot 阶段的物理内存管理 | buddy/slab 前的物理内存记录和早期分配 |
| fixmap 和 early ioremap | 通过固定虚拟地址槽位临时映射 FDT 和 MMIO | Linux 早期访问设备和 boot resource 的方式 | 完整 MM 子系统就绪前的临时映射能力 |
| 页表管理 | 实现 PGD/PUD/PMD/PTE 层级，支持 block mapping、page mapping、linear map、vmap 和用户页表 | Linux arm64 多级页表和地址空间划分 | 页表项属性、内核/用户地址空间、动态建表 |
| 内核分配 | 实现轻量 `kmalloc/kfree`，用于支撑内核对象和进程装载 | Linux `kmalloc` 风格的内核对象分配入口 | 内核对象分配和上层机制支撑 |
| 异常向量 | 建立 AArch64 exception vector，保存 `pt_regs`，分发 EL0/EL1 同步异常和 IRQ | Linux 异常入口和寄存器现场保存模型 | 异常入口、C handler、返回用户态控制流 |
| 系统调用 | EL0 通过 `svc` 进入内核，使用 `x8` syscall number 和 syscall table 分发，支持 `write/exit` | Linux arm64 syscall ABI 和表驱动分发 | 用户态到内核态服务入口 |
| uaccess | `copy_from_user` 在 syscall 路径中检查并复制用户态 buffer | Linux 用户指针不能被内核直接信任的边界模型 | 内核态和用户态地址空间边界 |
| generic IRQ | 实现 `irq_desc`、`irqaction`、`irq_chip`，支持 request/enable/disable/EOI | Linux generic IRQ layer | 中断描述、控制器抽象、handler 解耦 |
| GICv3 | 初始化 Distributor、Redistributor、CPU interface，并注册 root IRQ handler | Linux GICv3 driver 的寄存器模型 | ARM64 平台中断控制器 bringup |
| ARM generic timer | 初始化 arch timer，通过 timer interrupt 驱动调度检查 | Linux 使用 arch timer 作为时钟事件来源 | 硬件 timer、中断和调度连接 |
| 调度类 | 设计 `sched_class` 抽象并实现 round-robin 调度器 | Linux scheduler class 的分层思想 | 调度策略和通用调度框架分离 |
| 上下文切换 | 切换内核栈、用户寄存器、`TTBR0_EL1`、`SP_EL0`，通过 `eret` 返回 EL0 | Linux arm64 进程切换和异常返回路径 | 用户进程运行所需 CPU 状态切换 |
| `current` | 使用 `sp_el0` 保存当前进程指针 | Linux arm64 获取 `current` 的经典做法 | ARM64 架构相关的当前任务访问方式 |
| 用户进程装载 | 解析内置 process package，装载 ELF `PT_LOAD` segment，构造用户栈、argv/env/auxv | Linux `execve` 和 ELF loader 的核心思想 | ELF 装载、虚拟地址空间和初始用户态上下文 |
| 用户态 package | Python 工具把 demo ELF 和元数据打包进镜像 | 类似 initramfs/initrd 提供早期用户态载体 | 无 VFS 阶段的用户态验证闭环 |
| 构建系统 | 顶层 Makefile 编排 kernel/app/package，`scripts/Makefile.build` 递归构建 `obj-y` 和 `built-in.a` | Linux Kbuild 的分层组织方式 | 模块化扩展、清晰产物路径、可读编译日志 |

## 启动路径

QLoader 的启动路径覆盖了从裸机入口到跳转内核的完整链路:

- 识别当前异常级别，建立早期页表，配置内存属性并开启 MMU。
- 完成镜像重定位和 BSS 清零，保证 C runtime 可以正常工作。
- 初始化串口和 FDT，通过串口输出跟踪早期启动状态。
- 通过 QEMU fw_cfg 准备 Linux kernel、initrd 和 cmdline。
- 按 Linux arm64 boot protocol 跳转 Linux，也可以跳转到 Quest OS。

QLoader 不实现通用固件接口，当前范围集中在 QEMU `virt` 平台上的 Linux boot
protocol、ARM64 早期 MMU 和内核跳转流程。

## Quest OS 内核路径

Quest OS 的初始化流程是按 Linux-like 内核主线组织的:

```text
head.S
  -> mm_early_init()
  -> setup_machine_fdt()
  -> uart_init()
  -> mm_post_init()
  -> gic_v3_init()
  -> arch_timer_init()
  -> sched_init()
  -> process_init()
  -> run_scheduler()
```

这条路径把早期内存、设备树、串口、中断、timer、调度器和用户进程串在同一次
启动流程中。前一阶段完成的状态会被后一阶段继续使用，例如 FDT 映射、linear
map、GICv3 初始化和 timer tick 都会进入后续内核路径。

一次 QEMU `virt` 运行中的串口输出节选如下，重复的用户态 demo 输出已裁剪:

```text
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
...
```

## 内存管理实现

当前内存管理路径覆盖了从早期静态页表到 C 侧动态建表的过渡:

- 早期通过 idmap 保证开启 MMU 前后的执行地址连续。
- 用 `memblock` 记录可用物理内存和内核 reserved 区域。
- 用 fixmap/early ioremap 临时映射 FDT 和 MMIO。
- 建立 linear mapping，让内核可以稳定访问物理内存。
- 支持 vmap，用于把不连续物理页映射到连续虚拟地址。
- 为每个用户进程创建独立页表，并在调度时切换 `TTBR0_EL1`。

这些步骤把 boot 阶段的临时映射、内核线性映射、设备访问和用户进程地址空间
放在同一套页表管理代码下处理。

## 异常、中断和 syscall 实现

项目实现了 ARM64 内核从异常入口进入 C handler 的基础框架:

- exception vector 按 ARM64 规则组织 EL1t、EL1h、EL0t 入口。
- 入口代码保存通用寄存器，构造 `pt_regs`。
- EL0 `svc` 进入 syscall handler，通过 syscall table 分发。
- `write` syscall 通过 `copy_from_user` 从用户态读取 buffer。
- GICv3 中断进入 generic IRQ 层，再调用 arch timer handler。
- timer handler 设置调度检查标志，异常返回路径可以切回 idle/scheduler。

EL0 syscall、外设中断和 timer tick 都经过异常入口进入内核，再根据保存的
寄存器现场返回用户态、继续当前进程或回到调度路径。

## 调度和用户态实现

项目实现了一个能运行 EL0 程序的最小进程系统:

- 构建 `process_struct` 和 `memory_struct`，保存进程状态、页表、内核栈和
  用户态寄存器。
- 解析打包后的 ELF，映射用户代码段和数据段。
- 构造用户栈、argv、env 和 auxv。
- 使用 `sched_class` 注册 round-robin 调度器。
- 调度时切换 `TTBR0_EL1`、内核栈、用户寄存器和 `SP_EL0`。
- 通过 `eret` 进入 EL0，用户程序再通过 `svc` 回到内核。

用户程序通过 `eret` 进入 EL0，通过 `svc` 返回内核，timer interrupt 再触发调度
检查。进程、地址空间、系统调用和调度在这条路径中连续发生。

## 设备和平台支持

当前平台聚焦 QEMU `virt`，驱动覆盖 bringup 必需设备:

- PL011 UART: 提供早期和内核阶段串口输出。
- QEMU fw_cfg: 从 QEMU 获取 Linux kernel、initrd、cmdline 等启动资源。
- FDT/libfdt: 解析平台设备信息和 `/chosen`。
- GICv3: 提供中断控制器初始化和中断分发基础。
- ARM generic timer: 提供调度 tick。

这些驱动覆盖 bringup 所需的最小硬件集合: 启动资源来自 fw_cfg，日志输出走
PL011，中断由 GICv3 分发，调度 tick 来自 ARM generic timer。

## 构建系统

构建流程也按可扩展和结构化的方式组织。顶层 `Makefile` 负责编排 kernel、
app、链接脚本、最终二进制和组合镜像，目录内的 `Makefile` 只声明本目录需要
参与构建的对象或子目录。

主要特点:

- 使用类似 Linux Kbuild 的 `obj-y` 组织源码目录，新模块通常只需要在对应
  `Makefile` 中追加对象或子目录。
- `scripts/Makefile.build` 统一处理 C/Assembly 编译、递归子目录构建和
  `built-in.a` 归档，减少各目录重复规则。
- `scripts/Makefile.flags` 集中维护内核侧编译、汇编和链接参数，让构建选项
  和目录结构分离。
- `app/Makefile` 独立处理用户态 demo、公共启动库、应用链接、process package
  生成和 `objcopy` 转换，kernel 与 app 构建边界清晰。
- `OUT_DIR`、`CROSS_COMPILE` 等变量可覆盖，便于切换输出目录和工具链。
- 编译输出使用 `CC`、`AS`、`AR`、`LD`、`LDS`、`PACK`、`OBJCOPY` 等短标签，
  日志能直接看出当前步骤、产物路径和失败位置。

新增内核目录、驱动、用户态 demo 或打包产物时，可以沿用现有 Makefile 分层和
日志格式，不需要为每个目录重新编写完整构建规则。

## 设计取舍

项目当前范围集中在 AArch64 bringup 和 Linux 核心机制复现，
所以没有把工程复杂度投入到完整 POSIX 兼容层、VFS、文件系统、SMP、CFS、slab、
page reclaim、signal、namespace、cgroup 或完整驱动模型上。

这些模块可以在现有 boot、MM、IRQ、scheduler、syscall 和 app package 基础上
继续补充；当前实现先保留一条从 bootloader 到 EL0 用户程序的可运行路径。

## 代码阅读入口

推荐从几条主线阅读代码:

- 启动和跳转: `src/arch/aarch64/bootloader.S`、`src/bootloader/boot/` 和
  `src/arch/aarch64/quest_os_boot.S`。
- 内核初始化: `src/main.c` 展示 Quest OS 从早期 MM 到调度器启动的主流程。
- 内存管理: `src/mm/` 和 `src/arch/aarch64/mmu.c` 覆盖 memblock、fixmap、
  ioremap、linear map、vmap 和页表创建。
- 中断、调度和 syscall: `src/arch/aarch64/exception_entry.S`、`src/kernel/`、
  `src/driver/`、`src/schedule/` 和 `src/syscall/` 组成 EL0/EL1 运行闭环。
- 构建和用户态 package: `Makefile`、`scripts/`、`app/Makefile` 和
  `app/tools/proc_pack.py` 展示 kernel/app/package 的结构化构建流程。

## 总结

Quest OS 当前已经覆盖 ARM64 reset 入口、MMU 和页表、设备树、中断、调度、
syscall 和 EL0 用户程序。后续扩展可以继续沿着现有主线补充更完整的内存分配、
进程生命周期、文件系统和驱动模型。
