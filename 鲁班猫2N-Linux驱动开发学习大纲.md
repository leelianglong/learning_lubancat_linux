# 鲁班猫2N (LubanCat-2N) Linux 驱动开发学习大纲与详细步骤

> 适用板卡：野火 LubanCat-2N **V3**（RK3568，板卡编号 EBF410076V3，已由 `ls -l /boot/rk-kernel.dtb` 确认）
> 当前系统：Debian 10 (buster) + Linux 4.19.232（见本目录《鲁班猫2N (LubanCat-2N) 开发板实战操作与排错全记录.md》）
> 编写日期：2026-11-10
> 配套记录：[鲁班猫2N (LubanCat-2N) 开发板实战操作与排错全记录.md](鲁班猫2N%20(LubanCat-2N)%20开发板实战操作与排错全记录.md)

## 目录

- [一、定位：从"会用板子"到"能写驱动"](#一定位从会用板子到能写驱动)
- [二、事实对齐与硬件清单](#二事实对齐与硬件清单)
- [三、总纲：六阶段 20 周路线](#三总纲六阶段-20-周路线)
- [四、阶段详细步骤](#四阶段详细步骤)
  - [阶段 0 环境与工具链（第 1 周）](#阶段-0-环境与工具链第-1-周)
  - [阶段 1 内核模块与字符设备（第 2–4 周）](#阶段-1-内核模块与字符设备第-24-周)
  - [阶段 2 设备模型 / 设备树 / 中断（第 5–7 周）](#阶段-2-设备模型--设备树--中断第-57-周)
  - [阶段 3 子系统速通（第 8–13 周）](#阶段-3-子系统速通第-813-周)
  - [阶段 4 进阶与调试（第 14–17 周）](#阶段-4-进阶与调试第-1417-周)
  - [阶段 5 综合项目（第 18–20 周）](#阶段-5-综合项目第-1820-周)
- [五、每周节奏与里程碑](#五每周节奏与里程碑)
- [六、十条工程习惯](#六十条工程习惯)
- [七、坑位速查表](#七坑位速查表)
- [八、参考资料](#八参考资料)
- [九、与现有纪实文档的衔接](#九与现有纪实文档的衔接)
- [十、附录：三端文件传输](#十附录三端文件传输)
- [十一、会话进展与待办](#十一会话进展与待办)
- [十二、内核编译环境从零重建（照着做即可）](#十二内核编译环境从零重建照着做即可)

---

## 一、定位：从"会用板子"到"能写驱动"

你现在已经完成的部分（串口 1500000 调试、DHCP/DNS 排错、RNDIS、SSH/SCP、VNC、TF 卡与 eMMC 备份、sysfs 点灯）正好是驱动开发的**全部前置技能**。缺的只有三块：

1. **内核编程能力**：没有 libc、没有浮点、不能睡眠的地方不能睡、内存靠 kmalloc —— 与此前的用户态编程是两套规矩。
2. **设备模型与设备树**：Linux 3.x 之后"硬件描述"与"驱动实现"彻底分离，不会写 dts 就写不了驱动。
3. **子系统接口**：内核不允许你"自己发明一套文件接口"，输入、I2C、SPI、PWM、IIO、RTC 都要走既有框架。

**毕业标准（可验收）**：不看教程，从零完成一个"设备树 overlay + platform 驱动 + 中断 + 某子系统接口 + sysfs 属性 + 用户态测试程序"的外设驱动，并能用 ftrace/kprobe 定位一次真实的运行时 bug。

---

## 二、事实对齐与硬件清单

| # | 项目 | 确认方法 | 本项目结论 |
|---|---|---|---|
| 1 | 主芯片 | `cat /proc/device-tree/compatible` | `rockchip,rk3568`（4×Cortex-A55 @2.0GHz，Mali-G52，带 NPU） |
| 2 | 当前内核 | `uname -a` | `4.19.232`（Debian 10 镜像），另有 6.1.99 镜像可选 |
| 3 | 板卡设备树 | `ls -l /boot/rk-kernel.dtb` | **已确认**：`rk-kernel.dtb -> dtb/rk3568-lubancat-2n-v3.dtb`（V3 版本） |
| 4 | 引导配置 | `ls -l /boot/uEnv/uEnv.txt` | **已确认**：`uEnv.txt -> uEnvLubanCat2N-V3.txt`（官方对照表写的是 `uEnvLubanCat2-V3.txt`，以板上实际文件名为准） |
| 5 | 存储余量 | `df -h /`、`df -h /boot` | 根分区 7.0G 用了 3.1G，剩 **3.7G**；`/boot` 是独立分区（mmcblk0p2）仅 124M，剩 76M —— 都放不下内核源码，源码与编译放 PC；`/boot` 只放 dtb/overlay 这类小文件 |
| 6 | 编译工具 | `aarch64-linux-gnu-gcc -v` | 板端已有**本地** gcc 8.3.0（Debian 8.3.0-6）；PC 端才是交叉编译器（同名 `aarch64-linux-gnu-gcc`，容易混淆） |

**路线选择（重要）**：以 **4.19.232 为主线**。理由：野火官方《嵌入式 Linux 驱动开发实战指南》全书、例程源码、你手上的镜像三者版本一致，`lubancat2_defconfig` 与 `stable-4.19-rk356x` 分支严格对应，踩坑最少。6.1.99（`lbc-develop-6.1` + `lubancat_linux_rk356x_defconfig`）作为阶段 4 的可选修项，用于练习 API 差异。

### 硬件清单（已确认，直接决定实验对象）

| 模块 | 接口 | 关键约束 | 落在哪一阶段 |
|---|---|---|---|
| DHT11 / DHT22 湿度传感器 | **单总线私有协议**（不是 I2C，也不是 SPI） | 3.3V；数据线要 4.7k–10k 上拉到 3.3V；DHT11 间隔 ≥1s、DHT22 ≥2s；时序是 μs 级 | 阶段 3.7 时序协议驱动 |
| SSD1306 OLED（0.96"） | **I2C**（4pin：VCC/GND/SCL/SDA）；若是 7pin 则为 SPI | 3.3V；地址 `0x3C`（少数是 `0x3D`）；只写不读 | 阶段 3.2 I2C 子系统 |
| ESP-01S（ESP8266，AT 固件） | **UART 3.3V TTL** | **必须独立 3.3V/≥300mA 供电**（板子 3.3V 排针常常带不动）；`CH_PD` 拉高；默认 115200 8N1，上电头几百毫秒吐 74880 波特率乱码属正常 | 阶段 3.8 UART，阶段 4 serdev |

这三件正好覆盖三条完全不同的外设通路：**I2C（有子系统可套）→ 单总线（无子系统，纯时序）→ UART（有 TTY/serdev 框架）**，比原计划的"I2C 温湿度"教学价值更高。原计划里的 PWM 蜂鸣器用板载 LED 与 `/sys/class/pwm` 替代即可。

---

## 三、总纲：六阶段 20 周路线

| 阶段 | 周次 | 主题 | 对应野火教材 | 交付物（验收物） |
|---|---|---|---|---|
| 0 | 1 | 环境、工具链、内核源码、模块编译闭环 | 驱动书 1 章 | `hello.ko` 在板上跑通；内核可编译 |
| 1 | 2–4 | 内核模块、字符设备、并发、定时器 | 驱动书 2–5、11–12 章 | LED/按键字符设备 + 互斥锁 + 定时器 |
| 2 | 5–7 | 设备模型、platform 总线、设备树、overlay、中断、阻塞/poll | 驱动书 6–10 章 + 框架 1–4 章 | 中断按键驱动（dts 描述 + poll 上报） |
| 3 | 8–13 | 子系统：input、I2C(SSD1306)、单总线(DHT)、UART(ESP-01S)、PWM、IIO/ADC、RTC、sysfs/debugfs、电源管理 | 框架 1–16 章 | 4 个可跑通的驱动 + 用户态程序 |
| 4 | 14–17 | DMA/IOMMU、DRM/帧缓冲、块设备、USB、网络、SMP、内核调试 | 进阶 1–10 章 | 一次 bug 定位报告 + 1 个进阶驱动 |
| 5 | 18–20 | 综合项目：多子系统联动 + overlay 打包 + 服务化 | 全篇串联 | 可交付的驱动仓库 + deb 包 |

节奏建议：每周 6–8 小时（工作日 3 次 × 1.5h + 周末 3h）。**每个阶段必须有可运行的 .ko 产出，禁止只看不写。**

---

## 四、阶段详细步骤

### 阶段 0 环境与工具链（第 1 周）

**目标**：PC 上能编译内核与模块，板子上能 `insmod` 一个自己写的模块。

#### 步骤 0.0 先选路线：板端本地编译 vs PC 交叉编译

| | 方案 A：板端本地编译（先跑） | 方案 B：PC 交叉编译（必配） |
|---|---|---|
| 前提 | `sudo apt install linux-headers-4.19.232`（装完 `/lib/modules/$(uname -r)/build` 存在） | Ubuntu 虚拟机 + 交叉工具链 + 内核源码（源码约 2G，编译产物约 3G） |
| 能力 | 只能编**模块**，不能编 dts / 内核 | 编内核、编 dtb 与 overlay、读全部内核源码 |
| 优点 | 零配置，改完 `make` 直接 `insmod`，适合阶段 1 全部实验 | 阶段 2 起（设备树、overlay）**必须用** |
| 缺点 | 板子 4 核，编大模块慢；根分区只剩 3.7G，放不下源码 | 要配环境，`.ko` 需 `scp` 传过去 |

**建议**：今天先用方案 A 拿下一血（`hello.ko`），本周内把方案 B 的虚拟机建好，阶段 2 起全程用 B。

方案 A 的三条命令（本板 gcc 8.3.0 已就绪）：

```bash
sudo apt update
sudo apt install -y linux-headers-4.19.232 make gcc
ls /lib/modules/$(uname -r)/build/Makefile     # 有输出即成功
```

> 若野火源里没有这个包名，先 `apt-cache search linux-headers` 看实际名字；再不行就从野火网盘取 `linux-headers-4.19.232_*.deb` 手动 `dpkg -i`。
> 补充：`aarch64-linux-gnu-gcc -v` 在**板子上**看到的是本地编译器；vermagic 里不含编译器版本，所以用 gcc 8.3 给 4.19.232 编模块没问题，前提是 headers 与内核严格对应。

#### 步骤 0.1 准备编译环境（0.5 天）

三条路，按"已有资源"排序：

| 方案 | 适用 | 优点 | 代价 |
|---|---|---|---|
| **A. 阿里云服务器（已有，首选）** | 你有 Ubuntu 云主机 | 不占本地资源，可长期挂机跑编译；能编内核、dtb、dtbo、模块 | 只能编译不能上板测试；需把产物传回板子 |
| B. WSL2（Win11，`wsl --install -d Ubuntu-22.04`） | 想在本机快速迭代 | 与板子同局域网，`scp` 最快 | 占本地磁盘 10–20G |
| C. VirtualBox / VMware | 习惯图形化 | 环境隔离 | 最慢，磁盘 ≥ 60G |

**关键：Ubuntu 版本要配 gcc。** 4.19 内核在老编译器上最稳：Ubuntu 20.04（gcc 9）可用；**22.04 的 gcc 11 编 4.19 容易撞 `-Werror` 与头文件问题**。若云主机是 22.04，不要用 `apt` 的交叉编译器，直接用野火自带工具链（自包含，见下）。

```bash
# 依赖（云/WSL/VM 通用）
sudo apt update
sudo apt install -y build-essential git bc bison flex libssl-dev libelf-dev \
    libncurses-dev device-tree-compiler u-boot-tools rsync cpio kmod unzip

# 野火工具链（推荐，避开发行版 gcc 版本差异；注意前缀是 aarch64-linux-gnu-）
git clone https://github.com/LubanCat/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu.git ~/toolchain
export PATH=$HOME/toolchain/bin:$PATH
aarch64-linux-gnu-gcc -v          # 确认能跑
```

> 云主机资源下限：磁盘 ≥ 20G、内存 ≥ 2G（`-j2` 编译）；1G 内存会 OOM，先加 swap：
> ```bash
> sudo fallocate -l 4G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile && sudo swapon /swapfile
> ```
> 阿里云直连 GitHub 可能很慢：例程用 gitee 镜像（`https://gitee.com/LubanCat/lubancat_rk_code_storage`）；内核只有 GitHub 源，用 `git clone --depth 1` 并在失败时重试，或在本地下 zip 再上传。

#### 步骤 0.2 获取内核源码与例程（0.5 天）

```bash
mkdir -p ~/lubancat-driver && cd ~/lubancat-driver

# 内核：4.19.232 对应 stable-4.19-rk356x 分支
git clone -b stable-4.19-rk356x --depth 1 https://github.com/LubanCat/kernel.git

# 官方例程（驱动书配套代码在 linux_driver/ 下）
git clone --depth 1 https://github.com/LubanCat/lubancat_rk_code_storage.git
# 国内网络可用镜像：https://gitee.com/LubanCat/lubancat_rk_code_storage
```

把例程放到 `kernel/` 同级目录（例程 Makefile 里默认 `KERNEL_DIR=../../kernel/`）：

```bash
mv lubancat_rk_code_storage/linux_driver ~/lubancat-driver/linux_driver
ls ~/lubancat-driver        # 应看到 kernel/ 和 linux_driver/
```

#### 步骤 0.3 完整编译一次内核（1 天，只为验证环境）

```bash
cd ~/lubancat-driver/kernel
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- lubancat2_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
```

成功标志：生成 `arch/arm64/boot/Image`、`arch/arm64/boot/dts/rockchip/rk3568-lubancat-2n-v3.dtb`（你的板子是 V3）。
**本次不烧写**——编译内核的目的只是为模块提供构建树（`Module.symvers` + 头文件）。

#### 步骤 0.4 第一个模块（0.5 天）

```bash
cd ~/lubancat-driver/linux_driver/01_helloworld
make
modinfo helloworld.ko        # 记下 vermagic 字段
scp helloworld.ko cat@192.168.1.18:/home/cat/
```

板端：

```bash
sudo dmesg -C              # 清空日志，便于观察
sudo insmod helloworld.ko
dmesg | tail -5
lsmod | grep hello
sudo rmmod helloworld
```

**验收**：模块加载/卸载各打印一条日志，`lsmod` 能看到，`rmmod` 后 `/sys/module/helloworld` 消失。

> 备选路径（不想装 PC 工具链时）：板端 `sudo apt install linux-headers-4.19.232`，然后 `make -C /lib/modules/$(uname -r)/build M=$PWD modules`。省事，但**大项目仍建议交叉编译**（板子只有 4 核且根分区仅剩 3.7G）。

#### 步骤 0.5 云编译产物怎么上板（阿里云方案的核心）

云上只负责"产出二进制"，共三种同步方式，按需选：

| 方式 | 做法 | 适用 |
|---|---|---|
| **板子主动拉（推荐）** | 板子能上外网，云主机有公网 IP：在**板端**执行 `scp 用户名@云IP:~/lubancat-driver/kernel/arch/arm64/boot/dts/rockchip/overlay/xxx.dtbo /tmp/` | 云主机固定公网 IP 时最省事 |
| 云 → 本机 → 板子 | MobaXterm 的 SFTP 面板两段拖拽（云上 `scp` 到你 PC，再由 PC 传板子） | 云主机没开 SSH 或只有内网 |
| Git 中转 | 云上 commit 到私有仓库，板子 `git pull`；`.ko` 不建议进 Git，用 `.gitignore` 排除 | 想要版本留痕 |

配套习惯：

```bash
# 云上只编你需要的目标，别每次都 make 全量
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j2 dtbs
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=~/lubancat-driver/linux_driver/01_helloworld modules

# 产物清单（一次打包带走）
cd ~/lubancat-driver && tar -czf out-$(date +%m%d).tgz \
    kernel/arch/arm64/boot/dts/rockchip/overlay/*.dtbo \
    linux_driver/*/*.ko
```

> 云上**不能**做的事：烧写镜像、串口抓 log、看 `/proc/device-tree`。所以"编"在云上、"验"一定在板上——每次上板都要 `dmesg -C` 后重新加载，确认 vermagic 与 `uname -r` 一致。

---

### 阶段 1 内核模块与字符设备（第 2–4 周）

**目标**：理解"模块生命周期 → 字符设备 → 并发保护 → 定时器"这条主线，写出第一个真正能用的驱动。

#### 步骤 1.1 模块机制（3 天）

- `module_init/module_exit`、`__init/__exit`、`MODULE_LICENSE("GPL")`、`MODULE_AUTHOR`
- 打印：`pr_info/pr_err`、`printk(KERN_INFO ...)`，对照 `dmesg -w` 与 `/proc/sys/kernel/printk` 等级
- 参数：`module_param(n, int, 0644)` → `/sys/module/xxx/parameters/`
- 符号导出：`EXPORT_SYMBOL_GPL` 的用途与风险（A 模块给 B 模块用）

#### 步骤 1.2 字符设备骨架（1 周）

按顺序实现，每步都能跑：

1. 申请设备号：`alloc_chrdev_region()` / `MKDEV()`、`MAJOR/MINOR`
2. 注册 `cdev`：`cdev_init()` + `cdev_add()`；卸载时 `cdev_del()` + `unregister_chrdev_region()`
3. `file_operations`：`.open/.release/.read/.write/.unlocked_ioctl`（4.19 只有 `unlocked_ioctl`，没有 `.ioctl`）
4. 用户态数据交换：`copy_to_user/copy_from_user`（返回"未拷贝字节数"，务必判错）
5. 自动建节点：`class_create()` + `device_create()`（依赖 `systemd-udevd`），或手写 `mknod`
6. 权限：`/etc/udev/rules.d/99-lubancat.rules` 里 `KERNEL=="mychar", MODE="0666"`

**Makefile 模板（通用，后续所有实验复用）**：

```makefile
KERNEL_DIR ?= ../../kernel
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
obj-m := mychar.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules \
	        ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean
```

#### 步骤 1.3 并发与竞争（3 天）

同一驱动里分别用三种方式保护共享变量，然后用两个终端并发读写作对比：

- 原子变量 `atomic_t`（计数器）
- 互斥锁 `mutex`（可睡眠上下文，最常用）
- 自旋锁 `spinlock`（中断上下文 / 极短临界区）
- 补充概念：信号量、`completion`、RCU（读多写少）、内存屏障与 `READ_ONCE/WRITE_ONCE`

**铁律**：中断上下文不能睡眠 → 不能 `mutex_lock`、不能 `kmalloc(GFP_KERNEL)`、不能 `copy_to_user`。

#### 步骤 1.4 定时器与内核线程（3 天）

- `timer_setup(&t, cb, 0)` + `mod_timer()` / `del_timer_sync()`（4.19 已废弃 `init_timer`+`setup_timer`）
- `jiffies` 与 `msecs_to_jiffies()`；回调在软中断上下文，不能睡眠
- 需要睡眠的周期任务用 `kthread_run()`；任务卸载用工作队列 `schedule_work()`

**验收物**：`sys_led` 驱动 —— `/dev/sys_led` 支持 `write` 开关灯、`read` 回读状态、`ioctl` 设置闪烁频率（定时器实现），全程互斥锁保护，`rmmod` 无 oops。

---

### 阶段 2 设备模型 / 设备树 / 中断（第 5–7 周）

**目标**：把"硬编码"驱动重构成"设备树描述 + platform 驱动"，并接上中断。这是**分水岭**，从这里开始才叫 Linux 驱动开发。

#### 步骤 2.1 设备模型（3 天）

- 总线 / 设备 / 驱动三者的匹配过程；`kobject`、`kset`、`sysfs` 自动生成
- 属性文件：`DEVICE_ATTR_RW()` / `attribute_group`，`/sys/...` 下读写即调回调
- 设备号与 `/dev` 的对应关系（`cat /proc/devices`）

#### 步骤 2.2 设备树语法（1 周）

必掌握语法：`/dts-v1/`、`compatible`、`reg`、`status`、`#address-cells/#size-cells`、`&label` 覆盖、`include`、`phandle` 引用。

本板相关路径：

```bash
# 板级 dts 与 SoC dtsi（V3 版文件名带 -v3，先 ls 确认）
ls ~/lubancat-driver/kernel/arch/arm64/boot/dts/rockchip/rk3568-lubancat-2n*
~/lubancat-driver/kernel/arch/arm64/boot/dts/rockchip/rk3568-lubancat-2n-v3.dts
~/lubancat-driver/kernel/arch/arm64/boot/dts/rockchip/rk3568.dtsi
# overlay 目录（40pin 外设几乎全靠它）
~/lubancat-driver/kernel/arch/arm64/boot/dts/rockchip/overlay/
```

只编设备树（比编内核快一个数量级）：

```bash
cd ~/lubancat-driver/kernel
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- lubancat2_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j4 dtbs
```

**两种生效方式，都要会**：

1. **换主设备树**：把新 `.dtb` 拷到板上替换 `/boot/dtb/rk3568-lubancat-2n-v3.dtb`（`/boot/rk-kernel.dtb` 软链指向它），重启。切换时用相对路径：`cd /boot && sudo ln -sf dtb/xxx.dtb rk-kernel.dtb`。
2. **用 overlay 打补丁（推荐）**：编出 `.dtbo` → 拷到 `/boot/dtb/overlay/` → 在 `/boot/uEnv/uEnvLubanCat2N-V3.txt` 增加一行 `dtoverlay=/dtb/overlay/你的.dtbo`（行首不要有 `#`）→ `sudo reboot`。

overlay 源文件模板（放在 `overlay/` 下，并在该目录 `Makefile` 的 `dtbo-$(CONFIG_CPU_RK3568)` 里登记）：

```dts
/dts-v1/;
/plugin/;

#include <dt-bindings/gpio/gpio.h>

/ {
    fragment@0 {
        target-path = "/";
        __overlay__ {
            my_led {
                compatible = "lubancat,my-gpio-led";
                led-gpios = <&gpio1 1 GPIO_ACTIVE_HIGH>;   /* GPIO1_A1 */
                status = "okay";
            };
        };
    };
};
```

验证：`cat /proc/device-tree/my_led/compatible`（等价路径 `/sys/firmware/devicetree/base/`）。

#### 步骤 2.3 platform 驱动（4 天）

```c
static const struct of_device_id my_led_of_match[] = {
    { .compatible = "lubancat,my-gpio-led" },
    { }
};
MODULE_DEVICE_TABLE(of, my_led_of_match);
```

- `probe()/remove()` 的时机与返回值语义；`devm_*` 系列自动释放资源
- 取资源：`platform_get_resource()`、`platform_get_irq()`、`devm_gpiod_get()`、`devm_clk_get()`
- 用 `dev_err(dev, ...)` 而不是裸 `pr_err`（日志能定位到设备）

#### 步骤 2.4 pinctrl 与 GPIO（3 天）

- 引脚复用必须先在 pinctrl 里声明，rockchip 写法：
  ```dts
  &pinctrl {
      my_led_pin: my-led-pin {
          rockchip,pins = <1 RK_PA1 RK_FUNC_GPIO &pcfg_pull_none>;
      };
  };
  ```
- 新代码用 `gpiod_*`（`gpiod_get/direction_output/set_value`），`gpio_*`（整数编号）是遗留 API
- GPIO 编号换算：`bank*32 + group*8 + pin`，`GPIO1_A1` = `1*32 + 0*8 + 1` = **33**
- 调试：`cat /sys/kernel/debug/gpio`、`cat /sys/kernel/debug/pinctrl/*/pinmux-pins`

#### 步骤 2.5 中断（4 天）

- dts 里给中断：GPIO 控制器作 `interrupt-parent`，`interrupts = <RK_PA0 IRQ_TYPE_EDGE_FALLING>`
- `request_threaded_irq()`：硬中断只做最小动作，线程化下半部做耗时逻辑
- 触发方式选择（边沿 vs 电平）、共享中断 `IRQF_SHARED`、`IRQF_ONESHOT`
- 按键消抖：定时器延迟采样，而不是 `mdelay`
- 调试：`cat /proc/interrupts`

#### 步骤 2.6 阻塞 / poll / 异步通知（3 天）

- 等待队列：`wait_event_interruptible()` + `wake_up_interruptible()`
- `poll()`：`poll_wait()` + 返回 `EPOLLIN`，实现"有按键才返回"
- `fasync` + `kill_fasync()`：信号通知用户态（对应教材"异步通知"章）

**验收物**：按键驱动 —— dts 描述按键（gpio + 中断 + 消抖定时器），`poll` 阻塞读，`/sys/class/.../press_count` 统计次数，`evtest` 之外另写一个 20 行 C 测试程序。

---

### 阶段 3 子系统速通（第 8–13 周）

**原则**：不追求深挖每个子系统，而是**每个都跑通一个最小可用驱动**。顺序建议按"由简到繁"。

#### 3.1 input 子系统（3 天）

`input_allocate_device()` → `set_bit(EV_KEY, ...)` → `input_report_key()` → `input_sync()` → `input_register_device()`。
验收：`evtest /dev/input/eventX` 能打印 `KEY_*` 事件，`libinput debug-events` 也能识别。

#### 3.2 I2C 子系统（1.5 周）

- 协议层：`i2c_driver` + `id_table`（或 `of_match_table`）、`probe` 里拿到 `i2c_client`
- 传输 API：`i2c_transfer()`（组合消息）、`i2c_smbus_read_byte_data()`（简单寄存器）
- `regmap`：把"寄存器读写"抽象掉，`devm_regmap_init_i2c()`，后续 SPI 设备可复用同一份逻辑
- 40pin 使能：在 `/boot/uEnv/uEnvLubanCat2N-V3.txt` 打开 `dtoverlay=/dtb/overlay/lubancat-i2c3-m0-overlay.dtbo`（或 i2c5），重启后 `ls /dev/i2c-*`
- 电源域、上拉电阻、`i2c_get_clientdata()`

**实验对象：你手上的 SSD1306 OLED**（I2C 版 4 根线，VCC/GND 接 3.3V 与 GND，SCL/SDA 接 40pin 的 I2C）

1. 先确认总线是否已开：`ls /dev/i2c-*`；没有就开 overlay（`lubancat-i2c3-m0` 或 `lubancat-i2c5-m0`），或确认主 dts 里该总线 `status = "okay"`。
2. `i2cdetect -y 3` 扫到 `3c`（或 `3d`）说明接线与供电正确。
3. 先跑内核现成驱动（`CONFIG_AUXDISPLAY` + `CONFIG_SSD1306`）：加载后出现 `/dev/ssd1306-0`，`echo` 字符即可看到屏幕变化 —— 这是"站在子系统肩膀上"的体验。
4. 再自己写：字符设备 + `i2c_transfer()`，发初始化序列 + 按页写 GDDRAM，实现"写满一屏"。

验收：自写驱动能显示一行文字或一张位图；连续刷新不花屏。

#### 3.3 SPI 子系统（1 周）

`spi_driver` + `spi_device`、`spi_setup()`（模式 0–3、bits_per_word、max_speed_hz）、`spi_sync/spi_async`。
40pin 用 `lubancat-spi3-m1-overlay.dtbo`。设备端 `compatible` 写进 overlay 的子节点。
验收：SPI Flash 读 JEDEC ID（`0x9F`）或 SPI OLED 点亮。

#### 3.4 PWM 子系统（3 天）

`devm_pwm_get()` → `pwm_apply_state()`（period/duty/极性）。
40pin 用 `lubancat-pwm8-m0-overlay.dtbo` 等；背光调光最直观。
验收：`/sys/class/pwm/pwmchipX/` 能看到通道，用户态可调占空比。

#### 3.5 IIO 与 ADC（1 周）

RK3568 片上 SARADC；`iio_dev` + `iio_chan_spec` + `iio_info`，数据经 `/sys/bus/iio/devices/iio:deviceX/in_voltage*_raw` 暴露。
验收：用可调电阻或 3.3V 分压，读数与实际电压误差 < 5%。

#### 3.6 RTC / sysfs / debugfs / 电源管理（各 2–3 天）

- RTC：`rtc_class_ops`（`read_time/set_time`），板载 RTC + 电池掉电保持
- sysfs：把驱动参数做成 `attribute_group`，避免"只能改代码重启"
- debugfs：`/sys/kernel/debug/<drv>/` 暴露调试计数、寄存器 dump
- 电源管理：`.suspend/.resume` 回调、`runtime_pm`、`dev_pm_ops`（VNC 挂起/唤醒场景可验证）

#### 3.7 单总线时序驱动：DHT11/DHT22（1 周）

单独列出来，因为它**没有任何子系统可套**，是全篇唯一"纯靠时序"的驱动，最能练出对内核实时性的直觉。

- 协议要点（以 DHT22 为例）：主机拉低 ≥1ms（DHT11 要 18ms）→ 释放总线 → 传感器应答 80μs 低 + 80μs 高 → 连续 40 bit，每 bit 是 50μs 低电平 + （26–28μs 为 0 / 70μs 为 1）的高电平
- 驱动骨架：platform 驱动 + `devm_gpiod_get()`，采样期间 `local_irq_save()` + `udelay()`，用 `ktime_get()` 记录每个边沿
- 校验：40 bit 的湿度/温度 + 8 bit 校验和；连续 3 次校验失败返回 `-EIO`
- 暴露接口二选一：IIO（`in_temp_input`、`in_humidityrelative_input`，单位毫摄氏度/毫百分比）或字符设备
- **现实提醒**：4 核 SMP + 抢占下偶发误码是正常的物理现象，不是你的 bug；用户态 `libgpiod` 实现更稳，但学不到内核时序

验收：`cat /sys/bus/iio/devices/iio:deviceX/in_humidityrelative_input` 与室内温湿度计误差 < 5%，连续 100 次采样成功率 > 95%。

#### 3.8 UART 与 ESP-01S（1 周）

分两段：先用户态打通，再进内核。

- 用户态打通：开 40pin UART overlay（`lubancat-uart3-m0` 之类）→ 得到 `/dev/ttyS3`
  ```bash
  sudo stty -F /dev/ttyS3 115200 raw -echo
  printf 'AT\r\n' | sudo tee /dev/ttyS3 &
  sudo cat /dev/ttyS3        # 应看到 OK
  ```
- 内核视角：TTY 子系统分层（`tty_driver` / `tty_port` / 线路规程）、`serdev` 总线如何把一路 UART 交给设备驱动（蓝牙、GPS 都是这么接的）
- 写 serdev 驱动：`serdev_device_open()` / `serdev_device_set_baudrate()` / `serdev_device_write_buf()`，在 `receive_buf` 里解析 `+IPD`，把 AT 收发抽象成一个字符设备或 sysfs 节点
- **接线坑（必看）**：ESP-01S 必须**独立 3.3V/≥300mA** 供电，`CH_PD`(EN) 接 3.3V，否则表现为"无限重启、AT 永远没回应"；TX/RX 交叉接；上电瞬间 74880 波特率乱码属正常

验收：`AT+GMR` 拿到固件版本 → `AT+CWMODE=1` + `AT+CWJAP="热点","密码"` 连上 → `AT+CIPSTART="TCP",...` + `AT+CIPSEND` 发出一条数据。

**阶段验收**：四个可跑通的驱动（input 按键 / I2C-SSD1306 / 单总线 DHT / serdev-ESP01S）+ 一个统一用户态程序：**采集温湿度 → 刷 OLED → 经 ESP-01S 上报一行 JSON**。

---

### 阶段 4 进阶与调试（第 14–17 周）

按兴趣选 3–4 个方向，**每个方向都要产出一段可运行的代码或一份分析报告**：

| 方向 | 关键点 | 本板抓手 |
|---|---|---|
| DMA / IOMMU | `dma_alloc_coherent`、`dma_map_single`、cache 一致性、`of_dma_configure` | RK3568 有 IOMMU；先跑通"内存到内存"的 DMA 传输 |
| 帧缓冲 / DRM | `fb_info` 与 DRM 的关系，VOP 显示通路，`drm_simple_display_pipe` | HDMI 或 MIPI 屏显示一块色块 |
| 块设备 | `gendisk`、`blk-mq`、`submit_bio`、分区表 | 写一个 64MB 内存盘，`mkfs.ext4` + `mount` 成功 |
| USB | gadget（ConfigFS：U 盘/串口/网卡）、host 驱动枚举 | 板子通过 Type-C 变成一个 U 盘给 Windows |
| 网络 | `net_device`、NAPI、PHY、DSA/交换机；PCIe 网卡驱动 | 2N 的 2×2.5G 口（独立网卡芯片，挂载方式以原理图为准）值得单独研究 |
| SMP 与并发 | `per_cpu`、内存屏障、中断亲和（`/proc/irq/*/smp_affinity`） | 4 核跑并发压力测试对比 |
| 内核调试 | `ftrace`/`function_graph`、`kprobe`、`dynamic_debug`、`KASAN`、`kgdb`、oops 栈解读 | 见下方"调试工具箱" |

**调试工具箱（必须练熟）**：

```bash
# 动态调试：不重编内核就能打开某个模块的 pr_debug
echo 'module mychar +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# 函数跟踪：谁调用了我的 probe
sudo mount -t debugfs none /sys/kernel/debug    # 若未挂载
cd /sys/kernel/debug/tracing
echo function_graph | sudo tee current_tracer
echo mychar_probe | sudo tee set_ftrace_filter
echo 1 | sudo tee tracing_on ; cat trace

# 内核探针：不改代码观察函数参数
echo 'p:mychar_probe my_probe fd=%di' | sudo tee /sys/kernel/debug/tracing/kprobe_events

# oops 现场：看 Call trace 与 PC 值，用 arm64 的 addr2line
aarch64-linux-gnu-addr2line -e vmlinux -f 0xffff0000xxxxxxxx
```

**阶段验收**：写一份《一个真实 bug 的定位报告》——现象、复现步骤、用了什么工具、根因、修复、回归验证。

---

### 阶段 5 综合项目（第 18–20 周）

**项目 A（推荐，直接用你手上的三件硬件）——WiFi 环境监测终端**

- 硬件：DHT11/DHT22（单总线）+ SSD1306（I2C）+ ESP-01S（UART）+ 板载 LED/按键
- 驱动：
  - `dht`：platform + gpiod + 时序，经 IIO 暴露 `in_temp_input` / `in_humidityrelative_input`
  - `esp`：serdev 字符设备，透传 AT 指令
  - `ssd1306`：I2C 字符设备（或直接用内核 auxdisplay 驱动）
  - 一个平台驱动统一调参：`sample_ms`、`report_interval`、`alarm_threshold`
- 用户态：采集 → 刷 OLED（温湿度 + 时间）→ 拼 JSON → 经 ESP-01S `AT+CIPSEND` 发到 PC（`nc -l 8080` 验证），systemd 开机自启
- 工程化：Makefile、`.deb` 打包、README + 接线图 + overlay/dts 补丁、连续跑 24h 不崩
- 加分（接阶段 4 的"网络"方向）：把 ESP-01S 做成真正的 `net_device`，或用 PPP over UART 让板子走 WiFi 上网

**项目 B——USB gadget 复合设备**：板子同时呈现为 U 盘 + CDC 串口 + RNDIS 网卡（ConfigFS 配置，正好呼应你排错过的 RNDIS 故事）。

**项目 C——块设备驱动 + 文件系统**：自写 `gendisk`，`mkfs.ext4` 后挂载，读写大文件测吞吐。

**交付清单**：代码仓库（按 `01_xxx/` 分目录，含 Makefile）、每个实验一段"现象-原因-解决"记录（**直接并入本目录的纪实文档**）、overlay 与 dts 补丁、可编译的 `.ko` 与 deb 包。

---

## 五、每周节奏与里程碑

| 周 | 内容 | 周末必达里程碑 |
|---|---|---|
| 1 | 环境、源码、`hello.ko` | 板上 insmod/rmmod 成功，内核能编译 |
| 2 | 字符设备骨架 | `/dev/mychar` 可读写 |
| 3 | 并发保护、定时器 | LED 可闪烁，并发测试不崩 |
| 4 | 复习 + 重构 | 驱动改用 devm_*，rmmod 无泄漏 |
| 5 | 设备模型、sysfs 属性 | 驱动参数可从 /sys 修改 |
| 6 | 设备树 + overlay | 自写 overlay 生效，`/proc/device-tree` 可见 |
| 7 | platform + 中断 + poll | 按键中断驱动，poll 唤醒成功 |
| 8–9 | input + I2C（SSD1306） | `evtest` 与 `i2cdetect` 双通过，屏能刷字 |
| 10–11 | 单总线 DHT + UART/ESP-01S | DHT 读数合理；`AT` 返回 `OK` |
| 12–13 | IIO/ADC + RTC + 电源管理 | ADC 读数准确，RTC 掉电走时 |
| 14–15 | DMA / 块设备 / USB | 任选两个跑通 |
| 16–17 | 调试工具 + bug 报告 | 能用 ftrace 解释一次调用链 |
| 18–20 | 综合项目 + 打包 | 上电自启的完整演示 |

---

## 六、十条工程习惯

1. **每次 `insmod` 前先 `dmesg -C`**，让日志只包含本次加载的信息。
2. **模块与内核严格同源**：`modinfo xxx.ko | grep vermagic` 必须与板端 `uname -r` 一致；不一致就重编，**不要用 `--force`**。
3. **能 overlay 就不改主 dts**：主 dts 一旦改错可能导致不开机，overlay 错误最多是"没生效"。
4. **改 `/boot` 前先备份**：`sudo cp /boot/rk-kernel.dtb{,.bak}`，`sudo cp /boot/uEnv/uEnvLubanCat2N-V3.txt{,.bak}`。
5. **实验系统与生产系统分离**：TF 卡启动一套系统做实验，eMMC 那套保持干净（你已有的 TF 卡启动 + tar/dd 备份经验正好用上）。
6. **驱动不放内核源码树里**：用外部模块（`obj-m` + `M=`），内核树保持只读，方便 `git pull`。
7. **每个实验独立 git 仓库或分支**，commit message 写清"现象/改动/验证"。
8. **错误码要有意义**：`-ENODEV`（没找到设备）、`-EBUSY`（资源被占）、`-EINVAL`（参数错）、`-EFAULT`（用户指针错），不要一律 `-1`。
9. **`remove()` 里必须对称释放**：`devm_*` 能省掉 80% 的清理 bug。
10. **写文档**：每个驱动产出"接线 + dts 片段 + 加载命令 + 现象"三件套，直接进纪实文档。

---

## 七、坑位速查表

| 现象 | 常见原因 | 处理 |
|---|---|---|
| `insmod: invalid module format` | vermagic 不匹配（内核版本/配置/编译器不同） | 用同源 SDK 与同一 `.config` 重编；对比 `modinfo` 与 `uname -r` |
| `insmod: Unknown symbol in module` | 依赖模块未加载 / 符号未导出 | `modprobe` 按依赖加载；检查 `EXPORT_SYMBOL` |
| 加载成功但没有 `/dev/xxx` | 未 `class_create/device_create`，或 udev 未运行 | `systemctl status systemd-udevd`；手动 `mknod` 验证 |
| `dmesg` 里看不到 `pr_info` | printk 等级高于 console 等级 | `echo 8 > /proc/sys/kernel/printk`，或改用 `pr_err` |
| 改了 dts 没变化 | 改的不是 `rk-kernel.dtb` 指向的文件；没重编 dtb；overlay 未在 uEnv 启用 | `ls -l /boot/rk-kernel.dtb`（本板应为 `dtb/rk3568-lubancat-2n-v3.dtb`）；确认 `uEnvLubanCat2N-V3.txt` 行首无 `#` |
| `gpiod_get` 返回 `-EBUSY` | 引脚已被其他功能占用（pinctrl 冲突） | 查 `/sys/kernel/debug/pinctrl/*/pinmux-pins`，换引脚或关掉冲突外设 |
| I2C 读返回 `-ENXIO` / `-EREMOTEIO` | 地址错、设备未上电、overlay 没开、上拉缺失 | `i2cdetect -y N` 扫地址；万用表量电平 |
| 中断一直不进 | 触发方式错、引脚复用没配对、dts 没给 `interrupts` | `cat /proc/interrupts` 看计数；确认 `interrupt-parent` |
| 内核 oops / 挂死 | 空指针、越界、在中断里睡眠 | 读 Call trace；检查 `mutex_lock`/`kmalloc(GFP_KERNEL)` 是否在中断上下文 |
| 串口没反应 | 波特率不是 1500000、流控没关 | 见纪实文档第一章：`1500000-8-N-1`，流控 None |
| 板子编译不动 | 根分区只剩 3.7G | 全部改到 PC 交叉编译，产物 scp 过去 |
| `modpost: missing MODULE_LICENSE() in xxx.o` + 加载时 `module license 'unspecified' taints kernel` | 源码里**没写** `MODULE_LICENSE("GPL")`，或写了但**没保存**（VS Code 标签页有圆点）；旧 `.o` 缓存里没有 license 段 | `grep -n MODULE_LICENSE xxx.c` 确认磁盘内容 → `make clean && make` → `modinfo xxx.ko \| grep -i license` 必须输出 `license: GPL`（**实测踩过：编辑器里看着有、磁盘上没有**） |
| 改了代码但行为没变、`modinfo` 缺字段 | 编译其实没发生：文件未保存 / 时间戳未变 / 编的是别的目录 | 编译日志里**必须出现** `CC .../xxx.o`（只有 `CC xxx.mod.o` 就是没重编）；`make clean && make`；用 `md5sum` 对比云上与板上的 `.ko` |
| `rmmod: ERROR: Module xxx is not currently loaded` | 本来就没装 / 已卸载 / 板子重启过 | `lsmod \| grep xxx` 确认；`insmod` 用**文件路径**（带 `.ko`），`rmmod` 用**模块名**（不带 `.ko`） |
| `Disabling lock debugging due to kernel taint` | 之前加载过 license 缺失的模块，taint 标记**粘性**，本次开机不会消失 | 修好 license 后 `sudo reboot` 板子才彻底干净；不影响驱动功能 |
| `rmmod` 后几秒内核 oops / `BUG: scheduling while atomic` | 定时器没停就卸载，回调还在访问已释放内存 | `mychar_exit()` **第一行**必须是 `del_timer_sync(&my_timer)` |

---

## 八、参考资料

**野火官方（与本书版本严格对应，优先看）**

- 《嵌入式 Linux 驱动开发实战指南——基于 LubanCat-RK 系列板卡》：<https://doc.embedfire.com/linux/rk356x/driver/zh/latest/index.html>
- 快速使用手册（含 40pin 引脚图、GPIO/I2C/SPI/PWM/ADC 各章）：<https://doc.embedfire.com/linux/rk356x/quick_start/zh/latest/index.html>
- 镜像构建与部署（内核编译、dtb、overlay、deb 包）：<https://doc.embedfire.com/linux/rk356x/build_and_deploy/zh/latest/building_image/building_kernel/building_kernel.html>
- 配置文件 & 设备树 & 设备树插件：<https://doc.embedfire.com/linux/rk356x/quick_start/zh/latest/quick_start/device_tree/device_tree.html>
- 配套例程源码：<https://github.com/LubanCat/lubancat_rk_code_storage>（镜像：<https://gitee.com/LubanCat/lubancat_rk_code_storage>）
- 内核源码：<https://github.com/LubanCat/kernel>（4.19 用 `stable-4.19-rk356x`，6.1 用 `lbc-develop-6.1`）

**书籍与课程**

- 宋宝华《Linux 设备驱动开发详解：基于最新的 Linux 4.0 内核》——中文体系最完整，与 4.19 基本对得上
- 《Linux Device Drivers, 3rd》（LDD3，免费 PDF）——经典但基于 2.6，读"思想"不读代码
- Bootlin 免费培训材料（`bootlin.com/docs/`）——内核与驱动专题最权威的公开课
- 内核文档：`kernel/Documentation/` 本机就有，`Documentation/devicetree/bindings/` 是 dts 属性的最终权威

**硬件参考**

- RK3568 TRM（技术参考手册）：查寄存器、时钟、pinctrl 复用表必用
- LubanCat-2N 原理图（野火资料包）：确认 40pin 每根脚接到哪个 GPIO/I2C/SPI
- 40pin 引脚对照图：<https://doc.embedfire.com/linux/rk356x/quick_start/zh/latest/quick_start/40pin/40pin.html>

---

## 九、与现有纪实文档的衔接

本大纲是"学"的部分，纪实文档是"做"的部分，建议这样合并维护：

1. 每个实验完成后，在纪实文档追加一节：`### 阶段X-Y 实验名`，内容为"接线 + dts 片段 + 命令 + 现象 + 坑"。
2. 纪实文档已记录的 `1500000` 串口参数、`eth0` DHCP、RNDIS 驱动替换、`x11vnc -forever`、sysfs 点灯、TF 卡备份，全部作为**驱动开发的环境前提**，无需重复记录。
3. 遇到新坑先加"坑位速查表"，再补详细过程——查表解决快，看过程学得深。

---

## 十、附录：三端文件传输

### 10.1 先把方向搞对（决定用哪种方法）

```
   Windows（写代码）  ──①上传/直连──▶  阿里云 Ubuntu（编译）
                                            │
                                            │ ②板子主动拉（唯一可行方向）
                                            ▼
                                    LubanCat-2N（验证）
```

板子在家庭/实验室 NAT 后面，没有公网 IP，**云主机无法主动推文件给板子**（除非架 frp 反向隧道）。所以"云 → 板"只有一个方向：**板子用 `scp` 去云上拉**。

### 10.2 Windows → 云：三种方式

| 方式 | 操作 | 适合 | 备注 |
|---|---|---|---|
| **VS Code Remote-SSH（推荐）** | 装 VS Code + 扩展 `Remote - SSH` → `F1` → `Remote-SSH: Connect to Host` → `user@云IP` → 打开 `/home/user/lubancat-driver` | 长期开发 | **代码本体就在云上**，保存即生效，内置终端直接 `make`，等于"在 Windows 上写、在云上编"，没有上传动作 |
| WinSCP / MobaXterm SFTP | 左侧本地、右侧云，拖拽 | 偶尔传几个文件 | 你已有 MobaXterm，连板子那套界面同样能连云 |
| 命令行 `scp`（Win10/11 自带） | `scp -r .\01_hello\ user@云IP:~/lubancat-driver/linux_driver/` | 脚本化、批量 | PowerShell 里直接用，反斜杠路径即可 |

推荐 `~/.ssh/config` 免登录（Windows 上放在 `C:\Users\<你>\.ssh\config`）：

```
Host cloud
    HostName 47.x.x.x
    User ubuntu
    IdentityFile ~/.ssh/id_ed25519
```

之后 `ssh cloud`、`scp xxx cloud:~/` 都免输密码。

### 10.3 云 → 板：板子主动拉

一次性配置（在**板子**上）：

```bash
ssh-keygen -t ed25519 -N '' -f ~/.ssh/id_ed25519      # 无密码密钥
ssh-copy-id -i ~/.ssh/id_ed25519.pub ubuntu@<云公网IP>  # 输一次密码，之后免密
```

云上放一个打包脚本 `~/deploy.sh`：

```bash
#!/bin/bash
set -e
cd ~/lubancat-driver
make -C kernel ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j2 dtbs
make -C linux_driver/01_hello ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- || true
tar -czf out.tgz kernel/arch/arm64/boot/dts/rockchip/overlay/*.dtbo linux_driver/*/*.ko 2>/dev/null || true
sha256sum out.tgz
```

板子上一条命令取回并校验：

```bash
scp ubuntu@<云公网IP>:~/lubancat-driver/out.tgz /tmp/ && \
  sha256sum /tmp/out.tgz && \
  tar -xzf /tmp/out.tgz -C /tmp && ls /tmp/linux_driver
```

### 10.4 三端各放什么

| 内容 | 放哪 | 是否进 Git |
|---|---|---|
| 驱动源码 `.c`、`Makefile`、`.dts`/overlay 源文件 | 云上工作目录（Windows 通过 VS Code 远程编辑） | ✅ |
| 内核源码 `kernel/` | 只放云上，绝不进你的仓库 | ❌（用 `.gitignore`） |
| 编译产物 `.ko`、`.dtbo`、`Image` | 云上 → 打包 → 板子 `/tmp` | ❌ |
| 上板验证记录、dmesg 片段 | 板子 `/tmp` → 拷回 Windows → 写进纪实文档 | ✅（文档，不是产物） |

### 10.5 三个必踩的坑

1. **CRLF**：Windows 上编辑的 `.sh`、`Makefile` 带 Windows 换行，拿到 Linux 上会报 `\r: command not found` / `missing separator`。VS Code 里设 `"files.eol": "\n"`，仓库加 `.gitattributes`：
   ```
   * text=auto eol=lf
   *.sh text eol=lf
   ```
   已经错了的用 `sed -i 's/\r$//' file.sh` 或 `dos2unix` 修。
2. **把 `.ko` 提交进 Git**：Linux 二进制会撑爆仓库，`.gitignore` 里加 `*.ko *.o *.mod *.mod.c *.dtbo out.tgz`。
3. **安全组与登录**：阿里云只需放行 22 端口；务必禁用 root 密码登录、改用密钥 + 普通用户 + `sudo`；`scp` 目标目录不存在会直接失败（先 `ssh cloud 'mkdir -p ~/lubancat-driver/linux_driver'`）。

### 10.6 云上代码 → Windows 仓库 → GitHub（实际在用的流程）

代码只在云上写（VS Code Remote-SSH），仓库在 Windows（`github.com/leelianglong/learning_lubancat_linux`），云主机到 GitHub 网络不稳，所以**推送统一在 Windows 做**。每次改完代码，三条命令：

```powershell
cd D:\AI\deepseek\lubancat
New-Item -ItemType Directory -Force -Path .\code | Out-Null
scp -r root@<云公网IP>:/root/usercode/lubancat-driver/my_drivers .\code\
git add code/ ; git commit -m "feat(driver): <改了什么>" ; git push
```

要点：

- **先建 `.\code` 再 scp**，否则 `scp` 会把 `code` 当成文件名；
- `scp -r` 会把 `.ko/.o` 一起拉下来，仓库根目录的 `.gitignore` 已挡住（`*.ko *.o *.a *.mod *.mod.c *.cmd *.dtbo .tmp_versions/ Module.symvers modules.order`），`kernel/` 也整目录忽略；
- 想确认只会提交源码：`git add -n code/`（演练，不落地）与 `git check-ignore -v <文件>`；
- 反向（Windows 改的上云）：`scp .\code\my_drivers\01_mychar\mychar.c root@<云IP>:/root/usercode/lubancat-driver/my_drivers/01_mychar/`；
- **同一文件不要两边同时改**，以云上为唯一编辑地；
- Windows 若报“未对文件进行数字签名”，用 `pwsh -NoProfile -ExecutionPolicy Bypass -File <脚本>` 绕过执行策略，或改用上面那条 `scp`（外部程序不受策略限制）。

---

## 十一、会话进展与待办

> 记录时间：2026-11-10 23:45（Asia/Shanghai）。本节是活文档：完成一项就勾掉，新确认的事实补进 11.1，新决策补进 11.2。

### 11.1 实测输出存档（原始摘录，均可复现）

```text
$ uname -a
Linux lubancat 4.19.232 #23 SMP Mon Apr 27 06:05:07 UTC 2026 aarch64 GNU/Linux

$ cat /proc/device-tree/compatible
embedfire,lubancat-2n-v3rockchip,rk3568      # 两串之间是 NUL，cat 不显示，非笔误

$ ls -l /boot/rk-kernel.dtb
lrwxrwxrwx 1 root root 29 Feb 14  2019 /boot/rk-kernel.dtb -> dtb/rk3568-lubancat-2n-v3.dtb

$ ls -l /boot/uEnv/uEnv.txt
lrwxrwxrwx 1 root root 21 Feb 14  2019 /boot/uEnv/uEnv.txt -> uEnvLubanCat2N-V3.txt

$ df -h
/dev/mmcblk0p3  7.0G  3.1G  3.7G  46% /
/dev/mmcblk0p2  124M   43M   76M  37% /boot
/dev/mmcblk1p1   30G  1.2G   28G   4% /media/usb0    # TF 卡；df 中无 /mnt/tfcard，关系待确认

$ ls -lh /mnt/tfcard/system_backup.tar.gz
-rwxr-xr-x 1 root root 1.2G Nov 10 23:09 /mnt/tfcard/system_backup.tar.gz

$ aarch64-linux-gnu-gcc -v | tail -2
gcc version 8.3.0 (Debian 8.3.0-6)           # 板端本地编译器，不是交叉编译器

$ sudo ls /boot/dtb/overlay/
（无回显 —— 可能是空目录，也可能路径不存在，待用 ls -la 区分）
```

2026-11-15：内核编完 + 第一个字符设备 `mychar` 上板实测：

```text
$ modinfo helloworld.ko | grep vermagic
vermagic:       4.19.232 SMP mod_unload aarch64      # 无 preempt → 板端 CONFIG_PREEMPT_NONE

$ sudo insmod mychar.ko
[ 1572.538162] mychar: major=236 minor=0 ready
$ ls -l /dev/mychar
crw------- 1 root root 236, 0 Nov 15 21:41 /dev/mychar    # udev 自动建节点

$ echo "hello lubancat" | sudo tee /dev/mychar
[ 1634.672691] mychar: open
hello lubancat
[ 1634.672855] mychar: write 15 bytes
[ 1634.672898] mychar: release

$ sudo cat /dev/mychar
hello lubancat

$ sudo rmmod mychar
[ 1686.123522] mychar: exit
```

### 11.2 决策记录（为什么这么定）

| # | 决策 | 理由 |
|---|---|---|
| 1 | 主线用 **4.19.232**（`stable-4.19-rk356x` + `lubancat2_defconfig`） | 教材、例程、镜像三者版本一致；6.1.99 留到阶段 4 做 API 差异练习 |
| 2 | 编译主机用 **阿里云 Ubuntu**，不用 WSL2/虚拟机 | 已有资源；编译不需要硬件；不占本地磁盘 |
| 3 | 编辑方式用 **VS Code Remote-SSH**，代码常驻云上 | 省掉"上传"环节；保存即编译，避免三端不同步 |
| 4 | 产物上板一律 **板子 `scp` 主动拉** | 板子在 NAT 后无公网 IP，云无法推入（除非 frp） |
| 5 | 阶段 3 实验对象换成 **SSD1306 / DHT / ESP-01S** | 你手上就这三件；正好覆盖 I2C、单总线、UART 三条通路 |
| 6 | 项目 A 重定义为 **WiFi 环境监测终端**（DHT 采集 → OLED 显示 → ESP-01S 上报） | 一件硬件不浪费，且串起全部子系统 |
| 7 | overlay 目录为空时，**云上 `make dtbs` 产出后 scp**，不改主 dts | 主 dts 改错会导致不开机；overlay 失败最多"没生效" |
| 8 | 内核编译放进 **Docker `ubuntu:20.04`**（gcc 9.4），不在宿主机直接编 | 宿主机交叉 gcc ≥12 会把 4.19 的合法链表写法报 `-Wdangling-pointer`，且内核"警告即错误"；gcc 9 无此分析，环境也可复现 |
| 9 | 容器用 **`-dit` 常驻 + `docker exec`**，不用 `-it --rm` | `-it` 一断线容器就退，`--rm` 还会连编译产物一起删 |
| 10 | 内核只编一次，日常只 `make`（模块）和 `make dtbs` | 外部模块 `obj-m` 不触发内核重编，改 dts 只需编 dtb |

### 11.3 待办清单（按优先级，未完成）

- [ ] **备份校验（唯一有真实风险的项）**：`findmnt -T /mnt/tfcard` + `tar -tzf` 条目统计；要求 `media=0` 且 `boot>0`，否则重做备份
- [ ] **overlay 目录定性**：`sudo ls -la /boot/dtb/overlay/`（空 vs 不存在，结论不同）
- [ ] **uEnv 内容核对**：`grep -vE '^[[:space:]]*#' uEnvLubanCat2N-V3.txt` 看有无指向不存在文件的 `dtoverlay`
- [ ] **40pin 总线状态**：`ls /dev/i2c-* /dev/spidev* /dev/ttyS*`，决定是否必须编 overlay
- [ ] 板端 `sudo apt install linux-headers-4.19.232`，`ls /lib/modules/$(uname -r)/build/Makefile` 通过
- [x] 云主机：源码已 clone（`stable-4.19-rk356x` + 例程）；依赖与工具链改为在 Docker `ubuntu:20.04` 容器内安装（见第十二节）
- [x] **内核编译成功**（2026-11-15）：`Image` 25M、`Module.symvers` 698K、`kernelrelease=4.19.232`、`grep -c Error`=0
- [x] **`hello.ko` 在板上 `insmod` 成功**（2026-11-15，`dmesg` 输出 `Hello World Module Init`；阶段 0 毕业）
- [x] **字符设备 `/dev/mychar` 跑通**（2026-11-15）：`major=236`，udev 自动建节点，写 15 字节读出一致，`rmmod` 干净退出
- [ ] SSD1306 接线 + `i2cdetect -y 3` 扫到 `3c`

### 11.4 进展日志

| 日期 | 进展 |
|---|---|
| 2026-11-10 | 确认板卡为 2N-V3；梳理事实与决策；写出本文档第一版 |
| 2026-11-15 白天 | 云主机建 `/root/usercode/lubancat-driver`，clone 内核与例程；宿主机编译因 `python` 缺失和 `-Wdangling-pointer` 失败，改用 Docker `ubuntu:20.04`（gcc 9.4）方案 |
| 2026-11-15 夜间 | 链接 `vmlinux` 连续两次 OOM（根因：`vm.swappiness=0` + `ld` 要 1.18G 峰值）；设 `swappiness=100` + `KBUILD_LDFLAGS="--no-keep-memory --reduce-memory-overheads"` 后编过：`Image` 25M、`Module.symvers` 698K、`kernelrelease` 4.19.232、`Error`=0 |
| 2026-11-15 深夜 | **阶段 0 毕业**：云上编出 `helloworld.ko`，板端 `sudo insmod` 打印 `Hello World Module Init`，`taints kernel` 属预期 |
| 2026-11-15 夜 | **阶段 1.2 完成**：`/dev/mychar` 字符设备跑通（`major=236`、udev 自动建节点、写读一致、`rmmod` 无 oops）；踩坑：源码未保存导致 `.ko` 缺 `MODULE_LICENSE` |

### 11.5 下次继续的三个入口

1. 贴回 11.3 前四条的输出 → 我把"事实对齐表"定版；
2. 进入阶段 0：`hello.ko` 编译与加载；
3. 云上工程骨架（`hello.c`、`Makefile`、`deploy.sh`、`.gitattributes`、`.gitignore`）落地。

### 11.6 尚缺的信息

- 云主机：已确认 2 vCPU / 1.6 GiB / 40G 系统盘（剩 16G）/ root 登录；仍未知 Ubuntu 版本与公网 IP 是否固定；
- 40pin 实际接线（哪根脚接 USB 不关键，但 SDA/SCL 与 UART 的引脚号需要按 V3 对照图确认）；
- TF 卡上 `/mnt/tfcard` 与 `/media/usb0` 是否为同一挂载点。

---

## 十二、内核编译环境从零重建（照着做即可）

> 目的：假设半年后完全忘了当初怎么编，只看本节也能把 4.19.232 内核编出来。
> 实际环境：阿里云 ECS，Ubuntu，root 登录，2 vCPU / 1.6 GiB 内存 / 40G 系统盘（剩 16G）。
> 宿主机 Ubuntu 版本未记录，但从 `-Wdangling-pointer` 可判定其默认交叉 gcc ≥ 12。

### 12.1 最短路径（复制粘贴即可，约 8 条）

```bash
# ① 6G swap + 调高 swappiness —— 1.6G 内存编内核的必需项，缺一不可
fallocate -l 6G /swapfile && chmod 600 /swapfile && mkswap /swapfile && swapon /swapfile
grep -q '/swapfile' /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
printf 'vm.swappiness=100\n' > /etc/sysctl.d/zzz-swap.conf && sysctl -w vm.swappiness=100
free -h | grep -i swap; cat /proc/sys/vm/swappiness    # 期望 6.0Gi 与 100

# ② Docker
apt update && apt install -y docker.io && docker --version

# ③ 工程目录与源码（已存在可跳过）
mkdir -p /root/usercode/lubancat-driver && cd /root/usercode/lubancat-driver
git clone -b stable-4.19-rk356x --depth 1 https://github.com/LubanCat/kernel.git
git clone --depth 1 https://gitee.com/LubanCat/lubancat_rk_code_storage.git
mv lubancat_rk_code_storage/linux_driver . && rm -rf lubancat_rk_code_storage
ls    # 期望：kernel  linux_driver

# ④ 起常驻容器（gcc 9.4，绕开宿主机新 gcc 的误报）
docker run -dit --name kbuild -v /root/usercode:/work -w /work ubuntu:20.04 bash
docker ps | grep kbuild

# ⑤ 容器内装依赖（先换阿里云 apt 源）
docker exec -it kbuild bash -c "sed -i 's@//.*archive.ubuntu.com@//mirrors.aliyun.com@g; s@//security.ubuntu.com@//mirrors.aliyun.com@g' /etc/apt/sources.list && apt update && apt install -y build-essential git bc bison flex libssl-dev libelf-dev libncurses-dev device-tree-compiler u-boot-tools rsync cpio kmod gcc-aarch64-linux-gnu python3 python-is-python3"

# ⑥ 确认工具链是 9.4.0（关键，见 12.6）
docker exec -it kbuild bash -c 'aarch64-linux-gnu-gcc -v | tail -1'

# ⑦ 清残留 + 生成配置（顺序不能反）
docker exec -it kbuild bash -c 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- mrproper && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- lubancat2_defconfig'

# ⑧ 编译（-j1 最稳；链接 vmlinux 是独立内存峰值，见 12.6）
docker exec -d kbuild bash -lc 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j1 KBUILD_LDFLAGS="--no-keep-memory --reduce-memory-overheads" > build.log 2>&1'

# 看进度（-d 已与终端解耦，断线不影响）
docker exec kbuild tail -3 /work/lubancat-driver/kernel/build.log
docker exec kbuild free -h

# ⑨ 验证
docker exec -it kbuild bash -c 'cd /work/lubancat-driver/kernel && ls -lh arch/arm64/boot/Image Module.symvers arch/arm64/boot/dts/rockchip/rk3568-lubancat-2n-v3.dtb && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- kernelrelease && grep -c Error build.log'

# ⑩ 备份命脉文件
cd /root/usercode/lubancat-driver/kernel
cp .config .config.bak && cp Module.symvers Module.symvers.bak
```

生成的东西直接在宿主机 `/root/usercode/lubancat-driver/kernel/` 下（容器是挂载，不用拷贝）。

### 12.2 为什么这么选（决策背景）

| 选择 | 备选方案 | 为什么落到现在这个方案 |
|---|---|---|
| 用 **Docker `ubuntu:20.04`** 编 | ① 宿主机直接编 ② 野火 gcc-6.3.1 工具链 | ① 宿主机交叉 gcc ≥12 会报 `-Wdangling-pointer=`（4.19 里把局部变量地址挂链表的写法是合法的），内核又"警告即错误"，直接编不过；② 野火工具链最配套但要走 GitHub，这台机器网络不稳。容器里 gcc 9.4 没有该分析，一次成功 |
| 容器 **`-dit` 常驻** | `docker run -it --rm` | `-it` 一断 SSH 容器就退出，`--rm` 还会把已编译的产物一起删掉，白跑一小时 |
| **`-j2`** | `-j4` / `-j1` | 只有 2 vCPU；内存 1.6G 靠 swap 顶，`-j4` 更容易 OOM |
| **6G swap** | 4G / 不开 swap | 链接 `vmlinux` 时峰值 2–4G，没有 swap 必被 OOM kill |
| 源码 **`--depth 1`** | 完整 clone | 省 1G 多空间；代价是 `kernelrelease` 可能带 `+`，见 12.5 |

### 12.3 目录结构与各目录职责

```
/root/usercode/lubancat-driver/
├── kernel/            # 内核源码 + 构建树（.config / Module.symvers / Image / dtb 都在这里）
│   ├── .config                 # 编模块的配置依据（必须与板端一致）
│   ├── Module.symvers          # 外部模块链接用的符号表
│   ├── arch/arm64/boot/Image   # 内核镜像（本项目不烧写，只为产生产物）
│   └── arch/arm64/boot/dts/rockchip/rk3568-lubancat-2n-v3.dtb
└── linux_driver/      # 野火驱动教程例程（01_helloworld 等）
    └── 01_helloworld/          # Makefile 里 KERNEL_DIR=../../kernel/
```

`linux_driver` 必须与 `kernel` 同级，否则例程 Makefile 找不到内核。

### 12.4 依赖清单（每个包为什么装）

| 包 | 作用 |
|---|---|
| `build-essential` | gcc / make / libc 头文件（编 host 工具用） |
| `gcc-aarch64-linux-gnu` | **交叉编译器**，产出 aarch64 目标文件（容器内是 9.4.0） |
| `bc` | 内核算时间常量（`kernel/time`） |
| `bison flex` | 生成 kconfig 解析器、dtc 词法分析 |
| `libssl-dev` | 内核模块签名 / 部分加密代码 |
| `libelf-dev` | `objtool` 与部分 ELF 处理 |
| `libncurses-dev` | `make menuconfig` 界面 |
| `device-tree-compiler` | 单独编 dts/dtsi 时用（`dtc`） |
| `u-boot-tools` | `mkimage`，改 `boot.scr` 时用 |
| `rsync` / `cpio` / `kmod` | 打包 initramfs、装模块 |
| `python3` + `python-is-python3` | 内核脚本调用 `python`（Ubuntu 22.04+ 默认没有这个别名） |

### 12.5 编完必须通过的验证

| 检查 | 命令 | 期望 |
|---|---|---|
| 镜像生成 | `ls -lh arch/arm64/boot/Image` | 有文件（实测 25MB，带 DEBUG_INFO） |
| 符号表生成 | `ls -lh Module.symvers` | 有文件 |
| 设备树生成 | `ls arch/arm64/boot/dts/rockchip/rk3568-lubancat-2n-v3.dtb` | 存在 |
| **版本号一致** | `make ... kernelrelease` | **必须正好是 `4.19.232`** |
| 关键配置 | `grep -E '^CONFIG_(SMP\|PREEMPT\|MODVERSIONS\|MODULE_UNLOAD)' .config` | 记下来，与板端对比 |
| 无错误 | `grep -c "Error" build.log` | `0`（warning 可忽略） |

**版本号不对怎么办**：若 `kernelrelease` 输出成 `4.19.232+` 或 `4.19.232-dirty`，说明 `--depth 1` + 树不干净触发了 scm 版本号，模块的 vermagic 会不一致，板上 `insmod` 报 `invalid module format`。处理：

```bash
grep CONFIG_LOCALVERSION .config                 # 应为 ""（空）
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION= kernelrelease
```

### 12.6 本次真实踩过的坑（都已解决）

| 现象 | 原因 | 解决 |
|---|---|---|
| `/usr/bin/env: 'python': No such file or directory` + `init/Kconfig:17: syntax error` | Ubuntu 22.04+ 只有 `python3`，没有 `python` 别名；Kconfig 里 `$(shell python ...)` 取到空串 | `apt install python-is-python3`（容器里已包含在 12.1 第 ⑤ 步） |
| `error, forbidden warning:list.h:63` / `warning: ... [-Wdangling-pointer=]` | gcc ≥12 的新分析把 4.19 的合法写法误报，内核把警告当错误 | 换 gcc 9（容器方案）；应急可 `KCFLAGS="-w"` 或 `-Wno-dangling-pointer` |
| 容器跑一半消失，产物也没了 | 用了 `docker run -it --rm`，SSH 断开触发退出 + 自动删除 | 改 `-dit` 常驻，`docker exec` 进入 |
| 编译"看起来成功"但实际有错 | `make ... \| tee build.log` 的退出码是 `tee` 的，永远是 0 | 用 `grep -c Error build.log` 判断，或先 `set -o pipefail` |
| `scripts/link-vmlinux.sh: line 101: ... Killed` + `make: *** [Makefile:1174: vmlinux] Error 137` | 链接 `vmlinux` 时 `ld` 要 **1.18G 匿名内存**，1.6G 机器扛不住；`137 = 128+9`，即 SIGKILL（OOM killer） | 加 `KBUILD_LDFLAGS="--no-keep-memory --reduce-memory-overheads"` 重跑（只重做链接，1–3 分钟）；仍不行则在容器里 `apt install -y lld` 并 `make ... LD=ld.lld`（内存约减半） |
| `Swap` 显示 6G 空闲，进程却被 OOM 杀 | 阿里云镜像默认 **`vm.swappiness=0`**，内核几乎不换出匿名页，swap 形同虚设 | `sysctl -w vm.swappiness=100` 并写入 `/etc/sysctl.d/zzz-swap.conf`；改完重跑，`free -h` 的 `Swap used` 才会涨起来（实测涨到 130Mi 就过了） |

### 12.7 日常迭代：不需要重编内核

| 要改什么 | 命令 | 耗时 |
|---|---|---|
| 驱动 `.c`（外部模块） | `cd linux_driver/01_helloworld && make` | 2–10 秒 |
| dts / dtsi / overlay | `docker exec -it kbuild bash -lc 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs'` | 10–40 秒 |
| `.config`（menuconfig） | 重跑 `make ... -j2`（增量） | 1–10 分钟 |
| 内核源码/头文件 | 重跑 `make ... -j2`（增量） | 视改动 |
| 更新内核分支 | 重跑全量 | 35–60 分钟 |

容器复用（不用重建）：

```bash
docker start kbuild
docker exec -it kbuild bash
docker stop kbuild        # 用完可停，数据都在挂载目录里
```

⚠️ **成功后不要再 `mrproper`**：它会删掉 `.config` 和 `Module.symvers`，等于把整棵树废掉。另外 `--depth 1` 的树里也别指望 `git log` 能查到历史，需要时 `git fetch --unshallow`。

### 12.8 空间不够时的回收顺序

```bash
# 1) 只删编译产物，保留 .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean

# 2) 彻底清（连 .config / Module.symvers 一起删，之后要重新 defconfig）
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- mrproper

# 3) 回到刚 clone 的状态（不依赖 make，最快）
git clean -dxf

# 4) 容器侧
docker system df
docker rm -f kbuild && docker image prune -f
```

一般 `clean` 能回收 2–3G；`--depth 1` 的干净源码约 1.2–1.5G。

### 12.9 与板端的兼容性（决定 `.ko` 能不能 insmod）

云上产物要能加载到板子上，必须满足：

1. `kernelrelease` == 板端 `uname -r`（`4.19.232`）；
2. `.config` 中影响 vermagic 的项一致：`CONFIG_SMP`、`CONFIG_PREEMPT`、`CONFIG_MODVERSIONS`、`CONFIG_MODULE_UNLOAD`；
3. 如果用 `CONFIG_MODVERSIONS=y`，还需要 `Module.symvers` 与板端内核同源。

核对方法（板上取配置）：

```bash
# 板子上
ls /boot/config-*                                   # 通常有 config-4.19.232
scp cat@192.168.1.18:/boot/config-4.19.232 /root/   # 在云上执行

# 云上
diff <(sort /root/config-4.19.232) <(sort /root/usercode/lubancat-driver/kernel/.config) | head -30
```

如果差异落在上面那四个 `CONFIG_*` 上：

```bash
cp /root/config-4.19.232 /root/usercode/lubancat-driver/kernel/.config
cd /root/usercode/lubancat-driver/kernel
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j2
```

### 12.10 一页速查（打印贴在屏幕边）

```bash
# 开机后恢复环境
docker start kbuild || docker run -dit --name kbuild -v /root/usercode:/work -w /work ubuntu:20.04 bash

# 编模块
cd /root/usercode/lubancat-driver/linux_driver/01_helloworld && make
modinfo helloworld.ko | grep vermagic

# 编设备树
docker exec -it kbuild bash -lc 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs'

# 看编译进度 / 内存
docker exec -it kbuild tail -3 /work/lubancat-driver/kernel/build.log
docker exec -it kbuild free -h

# 触发重编内核（只有改配置/改内核源码/更新分支时才需要）
docker exec -it kbuild bash -lc 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j2 2>&1 | tee build.log'
```
