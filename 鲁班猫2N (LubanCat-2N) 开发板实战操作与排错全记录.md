# 鲁班猫2N (LubanCat-2N) 开发板实战操作与排错全记录

本文档完整总结了在配置鲁班猫2N（Debian 10 XFCE桌面版）过程中，从硬件连接、网络调试、远程控制到系统备份的完整流程，并详细记录了历史报错与解决方案。

> 2026-11 起补充了**云主机编译环境**与**驱动模块上板流程**（第八～十二章），以及**内核符号查看**与**Docker 命令解释**（第十三～十四章）。本文档只记"实际操作与踩坑"，学习路线与完整可复现 Runbook 见同目录《鲁班猫2N-Linux驱动开发学习大纲.md》，两边不重复。

## 目录

- [一、 硬件连接与串口调试 (初期踩坑)](#一-硬件连接与串口调试-初期踩坑)
- [二、 网络连接与驱动排错 (核心难点)](#二-网络连接与驱动排错-核心难点)
- [三、 SSH 远程登录与文件传输](#三-ssh-远程登录与文件传输)
- [四、 VNC 与图形桌面配置](#四-vnc-与图形桌面配置)
- [五、 硬件信息与系统底层查询](#五-硬件信息与系统底层查询)
- [六、 TF卡挂载与系统备份排错](#六-tf卡挂载与系统备份排错)
- [七、 常用系统维护命令备忘](#七-常用系统维护命令备忘)
- [八、 云主机开发环境](#八-云主机开发环境)
- [九、 云上内核编译环境](#九-云上内核编译环境)
- [十、 模块编译与上板](#十-模块编译与上板)
- [十一、 远程通道与断线处理](#十一-远程通道与断线处理)
- [十二、 代码同步与提交仓库](#十二-代码同步与提交仓库)
- [十三、 内核符号与定义查看](#十三-内核符号与定义查看)
- [十四、 Docker 命令解释](#十四-docker-命令解释)

---

## 一、 硬件连接与串口调试 (初期踩坑)

### 1. 串口模块排查 (CH340)
*   **现象**：环回测试（短接TX/RX）在SSCOM收不到字符，TX灯不闪。接开发板无启动日志。
*   **结论**：CH340模块可能存在兼容性问题或损坏，但这不影响后续网络连接。
*   **串口参数**：鲁班猫默认波特率为特殊的 **`1500000`**（非115200），数据位8，停止位1，无校验，**流控必须设为 None**。

### 2. 串口乱码与死锁救援
*   **现象**：在高波特率下，MobaXterm串口出现乱码后，`Ctrl+C` 和回车全部失效。
*   **解决**：直接盲打 **`reset`** 并回车，强制重置终端状态。若无效，盲打 `stty sane`。
*   **用户权限报错**：普通用户执行 `ping baidu.com` 报错 `ping: socket: Operation not permitted`。
*   **解决**：执行 `sudo ping baidu.com`，或永久赋予权限 `sudo setcap cap_net_raw+ep $(which ping)`。

---

## 二、 网络连接与驱动排错 (核心难点)

### 1. 有线网络 (以太网)
*   **硬件**：网线插入板载千兆网口（HanRun Hy91130A）。系统识别为 `eth0`。
*   **初期故障**：`ip addr` 显示 `eth0` 为 `UP,LOWER_UP` 但只有 IPv6，无 IPv4 地址，导致 `apt update` 报错 `Temporary failure resolving`。
*   **解决**：通过 `sudo dhclient eth0` 或 `nmtui` 重新请求 DHCP，成功获取 IP `192.168.1.18`。此时 `route -n` 显示默认网关为 `192.168.1.1`。
*   **注意**：如果 `ping 114.114.114.114` 通但 `ping baidu.com` 不通，需执行 `echo "nameserver 114.114.114.114" | sudo tee /etc/resolv.conf` 修复 DNS。

### 2. USB RNDIS 直连 (Windows 11 兼容性排错)
*   **现象**：通过 Type-C 线连接 OTG，SSH 频繁断开，报错 `Host does not exist`。电脑端 RNDIS 网卡显示 `169.254.x.x` 自动配置 IP。
*   **排查**：确认电脑端为 Win11，设备管理器能看到 `Remote NDIS based Internet Sharing Device`，但系统自带驱动不稳定。
*   **解决**：在设备管理器中，右键该设备 -> 更新驱动 -> 浏览电脑 -> **“让我从列表选取”** -> 选择“网络适配器” -> 厂商选 `Microsoft`，型号选 **`远程 NDIS 兼容设备`**。更换后连接稳定。
*   **网段**：电脑端手动配置 `192.168.7.1`，开发板默认 IP 为 `192.168.7.2`。

---

## 三、 SSH 远程登录与文件传输

### 1. SSH 连接与左侧文件树
*   **使用 IP**：在 MobaXterm 中，**必须使用 IP 地址（如 `192.168.1.18`）连接**，不要用主机名 `lubancat`。用主机名会导致 SFTP 解析失败，左侧不显示文件树目录。
*   **连接参数**：用户名 `cat`，密码 `temppwd`。
*   **文件树优势**：连接成功后左侧自动显示 SFTP 目录，可直接拖拽上传/下载文件，双击可直接用自带编辑器修改代码。

---

## 四、 VNC 与图形桌面配置

### 1. x11vnc 安装与启动
* **报错**：安装报错 `E: Unable to locate package x11vnc`。原因是软件源需要更新或未包含。
* **启动命令**：
    ```bash
    export DISPLAY=:0
    # 必须用 -forever 代替 -once -loop，否则客户端断开后服务会无限死循环重启
    x11vnc -auth guess -forever -noxdamage -repeat -rfbauth /home/cat/.vnc/passwd -rfbport 5900 -shared
    ```

- **生成密码**：`x11vnc -storepasswd 你的密码 /home/cat/.vnc/passwd`

### 2. 系统环境确认

- 通过 `ls /usr/share/xsessions/` 看到 `xfce.desktop`，且 `dpkg -l | grep -i xfce` 输出大量组件，确认系统为**桌面版**而非纯终端版。
- 在 SSH 中 `echo $XDG_CURRENT_DESKTOP` 为空是正常的，因为 SSH 不加载图形环境变量。

------

## 五、 硬件信息与系统底层查询

### 1. 核心硬件信息

- **CPU**：`cat /proc/cpuinfo` 显示 4核 Cortex-A55 (`CPU part : 0xd05`)。
- **系统**：`cat /etc/os-release` 显示 Debian 10 (buster)。
- **内核**：`cat /proc/version` 显示 Linux 4.19.232，由野火官方编译。
- **硬件型号**：`Hardware : EmbedFire LubanCat-2N-V3`。

### 2. 存储与文件系统

- `cat /proc/partitions`：确认 `mmcblk0` 为 7.3G eMMC，分为 p1(引导), p2(内核), p3(根目录)。
- `df -h`：根目录剩余 3.7G，空间健康。
- `cat /proc/filesystems`：带 `nodev` 为虚拟文件系统，不带的如 `ext4`, `vfat` 为真实磁盘文件系统。

### 3. sysfs 硬件控制 (LED 测试)

- 查看设备：`ls /sys/class/leds/` (包含 fan0, fan1, mmc0::, sys_led)。

- **控制原理**：写入文件即控制硬件。由于 `>` 重定向由当前 Shell 执行，普通用户无权限，必须加上 `sudo sh -c` 包裹。

  ```bash
  sudo sh -c 'echo 1 > /sys/class/leds/sys_led/brightness'   # 点亮
  sudo sh -c 'echo heartbeat > /sys/class/leds/sys_led/trigger' # 恢复心跳闪烁
  ```

------

## 六、 TF卡挂载与系统备份排错

### 1. TF 卡识别

- **未识别**：插卡后 `lsblk` 无 `mmcblk1`，且 `dmesg -w` 无输出。原因为**未插紧**。

- **识别成功**：插紧后日志显示 `mmcblk1: ... 29.1 GiB`，且包含一个 FAT32 分区 `mmcblk1p1`。

- **挂载命令**：

  ```bash
  sudo mkdir -p /mnt/tfcard
  sudo mount /dev/mmcblk1p1 /mnt/tfcard
  ```

### 2. 系统备份工具报错与解决

- **野火官方工具报错**：`检测到不是SD卡启动系统，无法备份eMMC！`

- **原因**：官方工具为了防止读取正在运行的系统导致崩溃，要求必须从 TF 卡启动系统后，才能备份 eMMC。

- **替代备份方案**：

  - **方案 A (打包备份)**：直接打包根目录文件，体积小，绕过 4GB 限制。

    ```bash
    sudo tar -czvf /mnt/tfcard/system_backup.tar.gz --exclude=/proc --exclude=/sys --exclude=/dev --exclude=/mnt --exclude=/tmp /
    ```

  - **方案 B (DD 全盘克隆)**：**注意 FAT32 不支持大于 4GB 文件**。需先将 TF 卡格式化为 `ext4` 才能进行全盘镜像克隆。

    ```bash
    sudo umount /mnt/tfcard
    sudo mkfs.ext4 /dev/mmcblk1p1
    sudo mount /dev/mmcblk1p1 /mnt/tfcard
    sudo dd if=/dev/mmcblk0 of=/mnt/tfcard/emmc_backup.img bs=4M status=progress
    ```

  - **方案 C (电脑端工具)**：使用瑞芯微 `RKDevTool`，进入 Loader 模式，通过“高级功能 -> 读取”直接克隆底层固件到电脑硬盘。

------

## 七、 常用系统维护命令备忘

```bash
# 退出 root 用户回到 cat 用户
exit

# 清理孤儿软件包
sudo apt autoremove

# 查看 CPU 运行频率
sudo cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq
```

---

## 八、 云主机开发环境

### 1. 为什么从"板子本机"迁到云主机

| 项 | 结论 |
|---|---|
| 本机条件 | Windows 11，无 Ubuntu 虚拟机 |
| 已有资源 | 阿里云 ECS（Ubuntu，root 登录，2 vCPU / 1.6 GiB / 40G 盘） |
| 最终方案 | **云上交叉编译 + VS Code Remote-SSH 编辑 + Docker 容器内的 gcc 9**；板子只负责加载验证 |
| 保留的板端操作 | 串口（第一章）、SSH/SFTP（第三章）、VNC（第四章）、TF 卡备份（第六章）仍按原样用于板上 |

### 2. VS Code Remote-SSH：连的是"主机"，不是"目录"

- 换目录：`Ctrl+K Ctrl+O`（File → Open Folder），输入远端绝对路径即可，新窗口复用同一条 SSH 连接，不重装 server。
- 同时看两个项目：`File → Add Folder to Workspace…` → `File → Save Workspace As…` 存成 `*.code-workspace`，以后一键打开。
- 终端默认 `cwd` 等于工作区根目录——工作区指错了，`make` 就会报 `No makefile found`。

**踩坑（目录双层嵌套）**：一开始打开了 `/root/usercode`，结果 Explorer 显示 `usercode → ZLToolKit/ZLToolKit`，要翻两层才能写代码。修法不是删文件，而是**把工作区指到内层仓库根**（单窗口 `Ctrl+K Ctrl+O`，或多根里 `Add Folder` 后右键移除旧的根）。副作用：`/root/usercode/.vscode` 里的设置不再生效。

### 3. 登录身份：SSH 不必须 root，但我们这里就是 root

- 实测提示符 `root@iZ7xv8840gbh80ttanp6zmZ`，家目录 `/root`，工程在 `/root/usercode/lubancat-driver`。
- `sshd` 由 root 启动，但登录身份由 `User`/密钥决定；普通用户 + `sudo` 完全够用。
- **忘了 root 密码**：用 Workbench 一键连接（Admin 用户）进去 `sudo passwd root` 即可，**立即生效、不用重启**。控制台的"重置实例密码"和"绑定密钥对"都要求重启，会打断编译，能不用就不用。
- 我这里的密码是nudt_victor3L
- ⚠️ `/root/.config` 是 **VS Code 远端的配置目录**（里面有 `Code/`），备份内核配置时千万别往 `/root/` 里拷 `.config`，会撞名报错，也别误删。

### 4. 目录组织与清理

```
/root/usercode/lubancat-driver/
├── kernel/            # 内核源码 + 构建树（.config / Module.symvers 在此）
├── linux_driver/      # 野火官方例程（01_helloworld …）
├── my_drivers/        # 自己写的驱动（01_mychar …）
└── lubancat_rk_code_storage/   # 例程仓库本体，取出 linux_driver 后可删
```

- `linux_driver` 必须与 `kernel` **同级**：例程 Makefile 里写死 `KERNEL_DIR=../../kernel/`。
- 空目录用 `rmdir`（非空会报错，自带安全阀）；`rm -rf` 只在确认后使用。
- **危险写法**：`cd /root/usercode && rm -rf lubancat-driver` 会把整个工程根删掉。删之前先 `pwd`、`ls`，用相对名删。
- 备份内核配置到独立目录，别混在 `/root` 根下：

```bash
mkdir -p /root/lubancat-backup
cp kernel/.config  /root/lubancat-backup/kernel-config-4.19.232
cp kernel/Module.symvers /root/lubancat-backup/
```

### 5. 备选：临时升配 2C4G（最终未采用）

当时评估过"按量付费临时升配"，操作要点记录下来备用：

- 先做云盘快照 → 停止实例时勾选 **「节省停机模式」**（保留公网 IP、停计费）→ 更改配置选 `ecs.u1-c1m2.large` 或 `ecs.c7.large` → 启动；
- **不要选 t6/t5 突发型**：CPU 积分耗尽会限频，编内核反而更慢；
- 按量付费同一实例两次变配间隔 ≥ 5 分钟；包年包月降配受限，"升了再降"只对按量划算；
- 变配必然重启，正在跑的编译会被杀——顺序必须是"先升配再编"。

最终没升配，改用 **swap + 链接参数** 解决（见第九章第 4 节），成本为零。

---

## 九、 云上内核编译环境

### 1. 宿主机直接编的三个坑（全部踩过）

| # | 现象 | 原因 | 结论 |
|---|---|---|---|
| 1 | `/usr/bin/env: 'python': No such file or directory` + `init/Kconfig:17: syntax error` | Ubuntu 22.04+ 只有 `python3`，没有 `python` 别名 | `apt install python-is-python3` |
| 2 | `error, forbidden warning: list.h:63` / `[-Wdangling-pointer=]` | 宿主机是 Ubuntu 24.04，交叉 gcc ≥12 的新分析把 4.19 的合法写法误报；内核"警告即错误" | 宿主机编不动 |
| 3 | 后面链接又 OOM | 见第 4 节 | — |

**决策**：新建 `ubuntu:20.04` 容器（gcc 9.4），把 `/root/usercode` 挂进去编。理由：gcc 9 没有那个分析、环境可复现、不污染宿主机。

### 2. 容器怎么起、怎么用

```bash
docker run -dit --name kbuild -v /root/usercode:/work -w /work ubuntu:20.04 bash
docker exec -it kbuild bash -c "sed -i 's@//.*archive.ubuntu.com@//mirrors.aliyun.com@g' /etc/apt/sources.list"
docker exec -it kbuild bash -c "apt update && apt install -y build-essential bc bison flex libssl-dev libelf-dev libncurses-dev device-tree-compiler u-boot-tools gcc-aarch64-linux-gnu python3 python-is-python3"
```

- **必须 `-dit` 常驻**，不能用 `docker run -it --rm`：`-it` 一断 SSH 容器就退出，`--rm` 还会把编译产物一起删掉。
- 容器里的 `-j1` 编译、产物直接落在宿主机 `/root/usercode/...`（挂载），不用拷。
- 容器不会开机自启，机器重启后要 `docker start kbuild`。

### 3. 长任务必须后台化（两次血的教训）

`docker exec -it ... make` 里的 make **挂在 SSH 会话下**，Workbench 标签页断开/超时会 SIGHUP 掉它，日志就停在某一行的 `.o` 上。两次都断在编译中途。

正确姿势：

```bash
docker exec -d kbuild bash -lc 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j1 > build.log 2>&1'

# 随时查（断了也不影响）
docker exec kbuild tail -3 /work/lubancat-driver/kernel/build.log
```

`-d` 与终端彻底解耦，这是 1.6G 小机器上唯一可靠的方式。

### 4. 链接 vmlinux 被 OOM 杀（本次最大坑）

**现象**：

```
scripts/link-vmlinux.sh: line 101: 29175 Killed   ${LD} ... -r -o ${1} ...
make: *** [Makefile:1174: vmlinux] Error 137
ls: cannot access 'arch/arm64/boot/Image': No such file or directory
```

**排查证据**：

```bash
dmesg -T | grep -iE 'killed process|oom' | tail -3
# oom-kill:constraint=CONSTRAINT_NONE,... global_oom
# Out of memory: Killed process 27775 (aarch64-linux-g) total-vm:1260864kB, anon-rss:1179564kB

cat /proc/sys/vm/swappiness      # → 0   ← 元凶
swapon --show                    # → 6G，USED 0B  ← swap 形同虚设
grep CONFIG_DEBUG_INFO .config   # → y   ← 加剧
```

**根因**：阿里云镜像默认 `vm.swappiness=0`，内核几乎不换出匿名页；`ld` 要 1.18G 匿名内存，物理只有 1.6G，于是"有 6G 空闲 swap 却被 OOM 杀"。

**解决（三步，实测通过）**：

```bash
# ① 让内核愿意用 swap
sysctl -w vm.swappiness=100
printf 'vm.swappiness=100\n' > /etc/sysctl.d/zzz-swap.conf

# ② 让 ld 少吃内存
docker exec -d kbuild bash -lc 'cd /work/lubancat-driver/kernel && make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j1 KBUILD_LDFLAGS="--no-keep-memory --reduce-memory-overheads" > build.log 2>&1'

# ③ 还不行：换 lld（内存约减半）或临时升配
docker exec -it kbuild apt install -y lld
docker exec -d kbuild bash -lc '... make ... LD=ld.lld > build.log 2>&1'
```

成功时 `free -h` 会看到 `Swap used` 从 0 涨到约 130Mi，随后出现 `LD [M] xxx.ko`（模块链接），说明 `vmlinux` 已过。

**经验**：`-j1` 只保护**编译期**；**链接期是独立的内存峰值**，小内存机器必须靠 swap 或 `lld`。另外 `CONFIG_DEBUG_INFO=y` 让 `ld` 峰值从 ~600M 涨到 1.18G，长期在这台机器上迭代可考虑关掉（会触发一次全量重编）。

### 5. 编完的验收结果（2026-11-15）

```bash
$ ls -lh arch/arm64/boot/Image Module.symvers
-rwxr-xr-x 1 root root  25M  arch/arm64/boot/Image
-rw-r--r-- 1 root root 698K  Module.symvers

$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- kernelrelease
4.19.232
$ grep -c Error build.log
0
```

`kernelrelease` 与板端 `uname -r` 一致是**模块能否 insmod 的前提**。收尾时 `cp` 一份 `.config` / `Module.symvers` 到 `/root/lubancat-backup/`。

### 6. 空间回收的命令梯度

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean   # 删产物，留 .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- mrproper # 连 .config/Module.symvers 一起删
git clean -dxf                                            # 回到刚 clone 的状态，最快
```

⚠️ 编成功后**不要随手 `mrproper`**，否则要重新 `defconfig` + 全量重编。

---

## 十、 模块编译与上板

> 实验设计、可运行脚本、预期值与实测数据见大纲 **步骤 1.3a**；本节只记操作流程与踩坑。

### 1. 命令流水线

```bash
# 云上（容器内）编模块
docker exec -it kbuild bash -lc 'cd /work/lubancat-driver/my_drivers/01_mychar && make'

# 宿主机可见（挂载）
ls -lh /root/usercode/lubancat-driver/my_drivers/01_mychar/mychar.ko

# 板子主动来云上拉（板子在 NAT 后，云推不进去）
scp root@<云公网IP>:/root/usercode/lubancat-driver/my_drivers/01_mychar/mychar.ko /tmp/

# 板上
sudo dmesg -C
sudo insmod /tmp/mychar.ko
dmesg | tail -3
sudo rmmod mychar          # 卸载用模块名，不带 .ko
```

### 2. 实测结果（`mychar` 字符设备）

```text
$ modinfo mychar.ko | grep -E 'license|vermagic'
license:        GPL
vermagic:       4.19.232 SMP mod_unload aarch64     # 无 preempt → 板端 CONFIG_PREEMPT_NONE

$ sudo insmod mychar.ko
mychar: major=236 minor=0 ready
$ ls -l /dev/mychar
crw------- 1 root root 236, 0            # udev 自动建节点

$ echo "hello lubancat" | sudo tee /dev/mychar
mychar: open
hello lubancat
mychar: write 15 bytes
mychar: release

$ sudo rmmod mychar
mychar: exit
```

### 3. 操作层面的坑

| 现象 | 原因 | 处理 |
|---|---|---|
| `modpost: missing MODULE_LICENSE()` + `module license 'unspecified' taints kernel` | 源码写了 `MODULE_LICENSE("GPL")` 但**编辑器里没保存**（标签页有圆点），make 没重编 | 看编译输出有没有 `CC .../xxx.o`；`grep -n MODULE_LICENSE xxx.c` 确认磁盘内容；`make clean && make` |
| `rmmod: ERROR: Module xxx is not currently loaded` | 本来就没装 / 已卸载 / 板子重启过 | `lsmod \| grep xxx` 确认，直接 `insmod` |
| `Disabling lock debugging due to kernel taint` | 加载过 license 缺失的模块，**taint 是粘性的** | 修好 license 并 `reboot` 板子才彻底干净；不影响功能 |
| `insmod: invalid module format` | vermagic 与板端内核不一致 | 对比 `modinfo \| grep vermagic` 与 `uname -r`；必要时用板端 `/boot/config-4.19.232` 覆盖云上 `.config` 重编 |

> `loading out-of-tree module taints kernel` 是**正常现象**（外部模块必然带 `O` 标记），不是错误。

### 4. 阶段 1.3（`race_demo`）操作错误清单

按类别记录"当时怎么错的、正确怎么做"。实验设计与会跑通的命令见大纲 **步骤 1.3a**，此处只记踩坑。

> **格式约定**：每个阶段结束后在本章追加一节（1.2 → 第 3 节，1.3 → 第 4 节，1.4 → 第 5 节……），统一用「现象 / 根因 / 正确做法」三列；只记**实际踩到的**与**高风险未踩**的，正确步骤不在此重复（避免与大纲冲突）。

**A. 命名与日志（最容易带偏排查方向）**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| `ls /dev/racedev` 不存在、`grep racedev /proc/devices` 无输出，一度怀疑驱动没注册 | 代码里 `DEV_NAME` 实际是 **`race_dev`**（带下划线），与文档/命令里的 `racedev` 不一致 | 以代码为准反查：`ls /sys/class/`、`cat /sys/class/*/dev`、`grep race_dev /proc/devices` |
| `dmesg` 打印 `racedev: ready`，但设备叫 `race_dev` | `pr_info("racedev: ...")` **写死**名字，不随 `DEV_NAME` 变化 | 日志一律 `pr_info("%s: ...", DEV_NAME, ...)` |
| `rmmod race_dev` → `Module race_dev is not currently loaded` | 把**设备名**当**模块名** | `rmmod` 用 `lsmod` 第一列（模块名 `race_demo`） |

**B. 权限（sudo）**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| `rmmod race_demo.ko` → `Operation not permitted`（EPERM） | **没加 `sudo`**；注意 EPERM 不是"被占用"（那是 EBUSY） | `sudo rmmod race_demo` |
| `./race_test.sh` 满屏 `/dev/race_dev: Permission denied`，四项计数全 0、耗时 90ms 无意义 | 以 `cat` 用户跑；节点是 `crw-------`；脚本开头写 sysfs 的 `reset` 也需要 root | `sudo /tmp/race_test.sh 10 100`（脚本已加 root 自检） |
| 之前 `chmod 666` 过的节点，重编重载后又不能写 | `insmod` 时 devtmpfs/udev **重建节点**，权限回到 0600 | 每次重载后 `chmod 666`，或加 udev 规则 `KERNEL=="race_dev", MODE="0666"` |

**C. 命令参数形式**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| `rmmod race_demo.ko` 报错里出现 `could not remove module race_demo.ko` | `rmmod` 只接受**模块名**；libkmod 虽会剥后缀，但报错措辞误导 | `rmmod race_demo`（无路径、无 `.ko`） |
| 差点直接粘贴含占位符的命令 `sudo mknod /dev/race_dev c <major> 0` | `<major>` 是占位符，shell 会把它当**重定向符**，生成名为 `major` 的文件 | 先 `cat /sys/class/race_dev/race_dev/dev` 取真实号（如 `236:0`），再把数字写进命令 |

**D. 机器与路径混淆**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| Windows 上 `scp root@8.148.180.123:/tmp/race_test.sh .` 报 `No such file or directory` | 脚本在**板子**的 `/tmp`，而 `8.148.180.123` 是**云主机**；两台机器的 `/tmp` 互不相干 | 看清 `user@host:path` 的 host 是哪台：板子拉云、Windows 拉云或直连板子 |
| 脚本只存在于板子 `/tmp`，重启即丢、也进不了 Git | 图省事在板子上 `cat > /tmp/xxx.sh` | 脚本放 `my_drivers/02_race/`，随代码同步；板子只做临时拷贝 |

**E. 实验操作风险（未踩，但要知道）**

| 风险 | 后果 | 正确做法 |
|---|---|---|
| 注释锁时把整段（读、`udelay`、写）都注释掉 | 计数器恒为 0，看不到"丢更新" | **只注释 `lock`/`unlock` 两行**，保留中间三行 |
| 只注释 `lock` 不注释 `unlock` | 对未持有的锁 unlock，`rmmod` 报 `mutex_destroy() ... busy` 甚至卡死 | 两行**成对**处理 |
| 改完源码没重编 / 重编了没重传 `.ko` | 行为完全没变，误以为"锁没作用" | 编译输出必须出现 `CC race_demo.o`；`md5sum` 对比云上/板上 |
| 重载后主设备号变了 | 旧 `/dev` 节点失效 | `cat /sys/class/race_dev/race_dev/dev` 核对，变了就删节点重建 |
| 卸载时报 `Module is in use` | 残留测试子进程开着 `/dev` | `pkill -f race_test` + `fuser -v /dev/race_dev` 后再 `rmmod` |

---

### 5. 阶段 1.4（`sys_led` 定时器）错误清单

> 现象 / 根因 / 正确做法。可运行代码与实验设计见大纲 **步骤 1.4a**。

**A. 变量作用域（静默失效，最耗时）**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| ioctl 返回成功、`period` 属性却仍是 1000，速率恒 1Hz | ioctl 里声明了局部 `unsigned long period_ms`，**遮蔽全局同名变量**，赋值只写到局部 | 局部只声明 `flags, period`；或编译时加 `KCFLAGS="-Wshadow"`，它会直接报 `declaration of 'period_ms' shadows a global declaration` |
| 同上，但这次是 `period_ms` 只被**读**、从未被**写** | ioctl 的 `else` 分支只写了 `period = period_ms; mod_timer(...)`，漏了 `period_ms = ms;` | 每个"设置"接口都要有"回读"点（如 `period` 属性），设完立刻 `cat` 校验 |

**B. 构建与部署（"改了没生效"的三大来源）**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| 改完代码行为不变 | 加载的是旧 `.ko`（没重编 / `scp` 没覆盖 / `insmod` 报 `File exists` 被忽略） | ① 编译输出必须有 `CC sys_led.o`；② `md5sum` 对比云上与板上；③ **在 `pr_info` 里加版本标记**（如 `ready v2`），`dmesg` 见到 v2 才算新代码生效 |
| `cat /sys/module/sys_led/srcversion` 报 No such file | `srcversion` 只在 `CONFIG_MODVERSIONS=y` 的内核存在；本板 vermagic 无 `modversions` 字段 | 改用 `md5sum` 或版本标记法；别把"字段不存在"当成"模块没加载" |

**C. 观察手段缺失（导致反复误判）**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| 只有 `tick` 一个观测量，分不清"参数没进驱动"还是"进了没用上" | 缺少参数回读接口 | 每个可调参数配一个只读 sysfs 属性（`period`），与设定值对照 |
| `tick` 在 `ledctl 0` 后不归零，误以为没生效 | `tick` 是只增计数器，`del_timer_sync` 只停止后续触发 | 判据看**增量**（3 秒 +0），不是绝对值；要归零得额外实现 reset |

**D. 崩溃级后果（漏 `del_timer_sync`）**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| `rmmod` 后 SSH 立即断开，`ping` 都 `无法访问目标主机`，必须断电重启 | 模块代码页已释放，定时器仍在软中断上下文执行 `sys_led_timer_fn` → `Fatal exception in interrupt` → **panic**（不是普通 oops，内核无法只杀进程） | `del_timer_sync()` 放 `exit` 第一行；任何"定时器/工作队列/中断 + 卸载"都要用对应的 `*_sync` / `cancel_*` |
| 崩溃现场没留下日志 | panic 时 SSH 必然断，`dmesg` 也随重启清空 | **这类实验必须挂串口控制台**（1500000 8N1，流控 None），串口会完整打印 oops 与 panic 文本 |

**E. 板端存储：`/tmp` 是 tmpfs**

| 现象 | 根因 | 正确做法 |
|---|---|---|
| 重启后 `/tmp` 下的 `.ko`、`race_test.sh`、`ledctl.c`、`ledctl` 全没了 | `df -h` 可见 `/tmp` 挂的是 `tmpfs 982M`（内存盘），掉电即清空 | **源码/脚本/测试程序放云上 `my_drivers/<实验>/` 并入 Git**；`/tmp` 只放可再生的编译产物；需长期留在板上的放 `/home/cat/` |

---

## 十一、 远程通道与断线处理

### 1. 五种入口的定位

| 入口 | 是否要密码 | 断线后果 | 适用场景 |
|---|---|---|---|
| **MobaXterm + 公网 IP** | 要（root 密码或密钥） | 前台进程被杀，`-d` 的后台任务不受影响 | 日常首选，可多开、有 SFTP 面板 |
| **Workbench 一键连接**（Admin） | 不要（阿里云代填） | 会话结束，`docker exec -it` 的进程被杀 | 快速登录、`sudo passwd root` |
| **Workbench 密码登录** | 要 | 同上 | 一键连接失败时 |
| **救援登录**（原 VNC 带外控制台） | 要**系统 root 密码** + 6 位控制台密码 | 不影响其他会话 | sshd 都起不来的极端情况 |
| **云助手「发送命令」** | 不要 | 与登录无关 | 机器卡死/连不上时抓状态、杀进程、启动容器 |

### 2. 阿里云 Cloud Shell 别当成自己的服务器

排查时曾把命令跑在了 Cloud Shell 里，得到一堆误导数据（`Mem 2.0Gi`、无 docker、`up 0 min`、`/root` 无权限）。**辨别方法**：

```bash
hostname                 # Alicloud → 是 Cloud Shell；iZ7xv8840... → 是你的 ECS
```

### 3. 断线应急顺序

1. 先重连 MobaXterm（`R` 重启会话）；
2. 连不上就用**云助手发送命令**（不走 sshd）：

```bash
ps -eo pid,pcpu,etime,args | grep -E '[c]c1|[m]ake' | head -6
tail -3 /root/usercode/lubancat-driver/kernel/build.log
docker ps -a
free -h
```

3. 判断：`make` 已死 → `docker start kbuild` 后用 `-d` 重跑（`.o` 会复用，不从头编）；`make` 活着 → 等；
4. 加分项：给 SSH 配 keepalive（MobaXterm 会话设置 15s；服务端 `ClientAliveInterval 30`），减少莫名断开。

---

## 十二、 代码同步与提交仓库

- **编辑只在云上**（VS Code Remote-SSH）；**仓库在 Windows**（`github.com/leelianglong/learning_lubancat_linux`）；**推送在 Windows 做**——因为阿里云到 GitHub 不稳（实测 `clone` 会卡死）。
- 每次同步代码一条命令：

```powershell
cd D:\AI\deepseek\lubancat
New-Item -ItemType Directory -Force -Path .\code | Out-Null
scp -r root@<云公网IP>:/root/usercode/lubancat-driver/my_drivers .\code\
git add code/ ; git commit -m "feat(driver): <改了什么>" ; git push
```

- 坑 1：**先建 `.\code` 再 scp**，否则 `scp` 把 `code` 当成文件名。
- 坑 2：`scp -r` 会连带 `.ko/.o` 下来，靠 `.gitignore` 挡（`*.ko *.o *.a *.mod *.mod.c *.cmd *.dtbo .tmp_versions/ Module.symvers modules.order`，`kernel/` 整目录忽略）。提交前可预览：`git add -n code/`、`git check-ignore -v <文件>`。
- 坑 3：Windows 上跑 `.ps1` 报"未对文件进行数字签名" → 用 `pwsh -NoProfile -ExecutionPolicy Bypass -File xxx.ps1`，或干脆只跑 `scp`（外部程序不受执行策略限制）。
- 反向（Windows 改动上云）：`scp .\code\my_drivers\01_mychar\mychar.c root@<云IP>:/root/usercode/lubancat-driver/my_drivers/01_mychar/`。
- 纪律：**同一文件不要两边同时改**，以云上为唯一编辑地。

---

## 十三、 内核符号与定义查看

查 `struct cdev`、`cdev_add()` 这类定义，最终选的是**文本搜索**（ripgrep），比配 IntelliSense 快、也不吃内存。

### 1. 先装 ripgrep，再注意两个坑

```bash
apt install -y ripgrep          # root 用户，不用 sudo
cd /root/usercode/lubancat-driver
rg -n "struct cdev \{" kernel/include
# kernel/include/linux/cdev.h:11:struct cdev {
```

| 坑 | 现象 | 处理 |
|---|---|---|
| **路径层级错** | `rg: kernel/include: No such file` | 当时人在 `my_drivers/` 下，`kernel/` 在上一级；先 `cd` 到 `lubancat-driver` 根，或写 `../kernel/include` |
| **`{` 没转义** | 报正则语法错 | 写 `"struct cdev \{"`，或直接搜 `"struct cdev"` |

### 2. 查内核符号的五个固定套路

```bash
cd /root/usercode/lubancat-driver

# ① 结构体/联合体定义
rg -n "struct file_operations \{" kernel/include/linux/fs.h

# ② 函数声明（头文件里）
rg -n "\bcdev_init\(" kernel/include

# ③ 函数实现（.c 里）
rg -n "^void cdev_init|^int cdev_add|^void cdev_del" kernel/fs/char_dev.c

# ④ 宏定义（很多"函数"其实是宏）
rg -n "define copy_to_user|define timer_setup|define module_param" kernel/include

# ⑤ 谁导出了这个符号 / 谁在用它
rg -n "EXPORT_SYMBOL.*cdev_add" kernel
rg -n "class_create" kernel/include/linux/device.h kernel/drivers/base/class.c
```

### 3. 常用定义速查（可直接复制）

```bash
rg -n "struct cdev \{" -A6  kernel/include/linux/cdev.h
rg -n "^void cdev_init|^int cdev_add" -A20 kernel/fs/char_dev.c
rg -n "struct class \{" -A8   kernel/include/linux/device.h
rg -n "struct device \{" -A12 kernel/include/linux/device.h
rg -n "define class_create|__class_create" kernel/include/linux/device.h
rg -n "^struct device \*device_create" -A12 kernel/drivers/base/core.c
rg -n "define copy_to_user" -A8 kernel/include/linux/uaccess.h
rg -n "define timer_setup|define mod_timer" kernel/include/linux/timer.h
rg -n "define module_param\b" -A6 kernel/include/linux/moduleparam.h
```

看完整定义也可以直接用行号打开：

```bash
less +11 kernel/include/linux/cdev.h
# struct cdev { struct kobject kobj; struct module *owner;
#               const struct file_operations *ops; struct list_head list;
#               dev_t dev; unsigned int count; } __randomize_layout;
```

### 4. 给常用目录加个快捷函数（写进 `~/.bashrc`）

```bash
ks() {
	rg -n -C2 "$1" \
		/root/usercode/lubancat-driver/kernel/include \
		/root/usercode/lubancat-driver/kernel/kernel \
		/root/usercode/lubancat-driver/kernel/fs \
		/root/usercode/lubancat-driver/kernel/drivers/base \
		/root/usercode/lubancat-driver/kernel/drivers/misc 2>/dev/null | head -40
}
```

```bash
source ~/.bashrc
ks "struct cdev \{"
ks "cdev_add"
```

### 5. VS Code 里搜索（不用装系统 rg）

`Ctrl+Shift+F` 用的是 VS Code 自带的 ripgrep，与系统 `rg` 无关。提效三招：

| 场景 | 做法 |
|---|---|
| 只搜头文件 | `files to include` 填 `kernel/include/**` |
| 排除文档噪音 | `files to exclude` 填 `Documentation/**,*.rst` |
| 搜含 `{` 的结构体 | 关掉搜索框右侧的 `.*` 正则开关，直接搜 `struct cdev {` |

### 6. 备选：VS Code C/C++ 智能感知（F12 跳转）

如果想让 `F12` 直接跳定义，需要三件事（**扩展必须装在远端**：扩展面板里点「Install in SSH: aliyun-dev」）：

1. 工作区同时包含 `kernel/` 与 `my_drivers/`（`Add Folder to Workspace…`，存成 `.code-workspace`）；
2. 在**工作区根**放 `.vscode/c_cpp_properties.json`；
3. 确认 `kernel/include/generated/autoconf.h` 存在（编过内核就有）。

```json
{
  "configurations": [{
    "name": "LubanCat Kernel 4.19.232 (arm64)",
    "includePath": [
      "${workspaceFolder}/kernel/include",
      "${workspaceFolder}/kernel/include/uapi",
      "${workspaceFolder}/kernel/arch/arm64/include",
      "${workspaceFolder}/kernel/arch/arm64/include/uapi",
      "${workspaceFolder}/kernel/include/generated",
      "${workspaceFolder}/kernel/include/generated/uapi",
      "${workspaceFolder}/kernel/arch/arm64/include/generated"
    ],
    "defines": ["__KERNEL__", "MODULE", "CONFIG_ARM64=1"],
    "forcedInclude": ["${workspaceFolder}/kernel/include/generated/autoconf.h"],
    "cStandard": "gnu89",
    "intelliSenseMode": "linux-gcc-arm64",
    "browse": {
      "path": ["${workspaceFolder}/kernel/include", "${workspaceFolder}/kernel/arch/arm64/include"],
      "limitSymbolsToIncludedHeaders": true
    }
  }],
  "version": 4
}
```

三个关键项：`arch/arm64/include`（放 `asm/*`）、`defines: __KERNEL__`（否则走错 `#ifdef` 分支）、`forcedInclude: autoconf.h`（否则 `CONFIG_*` 全报未定义）。

小内存机器（本机 1.6G）加两条减负：

```json
"C_Cpp.errorSquiggles": "disabled"
```

并**不要**点 "C/C++: Rescan Workspace"（会把内核几万文件全解析，内存吃紧）。

> 备选索引方案：`apt install universal-ctags` 后在 `kernel/` 里 `ctags -R --languages=C,Asm .`，生成 `tags`，配 `Ctags Support` 扩展；不依赖编译参数，最省内存。

---

## 十四、 Docker 命令解释

以实际最常用的一条为例：

```bash
docker exec -it kbuild bash -lc 'cd /work/lubancat-driver/my_drivers/02_race && make'
```

### 1. 分层结构

```text
你（MobaXterm / VS Code 终端，root@云主机）
 └─ docker exec -it kbuild bash -lc '…'              ← 进入容器命名空间，新起一个进程
     └─ cd /work/lubancat-driver/my_drivers/02_race && make
         └─ make -C ../../kernel M=/work/... modules ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
             └─ aarch64-linux-gnu-gcc … → race_demo.ko
```

### 2. 逐段解释

| 片段 | 含义 | 备注 |
|---|---|---|
| `docker` | 客户端，经 `/var/run/docker.sock` 与 dockerd 通信 | 需 root 或 docker 组权限 |
| `exec` | 在**已存在且正在运行**的容器里执行命令 | 对比 `docker run`：那是新建容器 |
| `-i` | 保持 STDIN 打开 | 否则交互式命令立即退出 |
| `-t` | 分配伪终端 | 输出重定向到文件时**去掉**，否则混入 `\r` |
| `kbuild` | 容器名 | 容器被删/停止会分别报 `No such container` / `is not running` |
| `bash` | 容器里的 shell | `ubuntu:20.04` 自带 |
| `-l` | 登录式 shell，读 `/etc/profile`、`~/.profile` | 影响 `PATH`；有的镜像会打印 MOTD |
| `-c '…'` | 把整串当命令执行 | **单引号**保证变量由容器内 bash 解释 |
| `cd … && make` | 进目录成功才编译 | 目录写错时不会在错误目录里乱编 |

### 3. `/work` 与 `/root/usercode` 是同一份文件

容器启动时挂载（详见第九章第 2 节）：

```bash
docker run -dit --name kbuild -v /root/usercode:/work -w /work ubuntu:20.04 bash
```

| 宿主机 | 容器内 | 同一文件？ |
|---|---|---|
| `/root/usercode/lubancat-driver/my_drivers/02_race` | `/work/lubancat-driver/my_drivers/02_race` | ✅ 同一个 inode |

所以 `.ko` 编完**直接出现在宿主机**，不需要 `docker cp`。

### 4. 等价写法

```bash
# 用 -w 指定工作目录，省掉 cd
docker exec -it -w /work/lubancat-driver/my_drivers/02_race kbuild make

# 不需要登录 shell
docker exec -it kbuild bash -c 'cd /work/... && make'

# 短命令不必 -it
docker exec kbuild ls /work/lubancat-driver/my_drivers

# 长任务用 -d（后台，与终端解耦；终端断开不会杀 make）
docker exec -d kbuild bash -c 'cd /work/.../02_race && make > build.log 2>&1'
```

### 5. 退出码与常见报错

`docker exec` 会**原样返回**容器内命令的退出码：

```bash
docker exec -it kbuild bash -lc 'cd /work/... && make' && echo "编译成功"
```

| 报错 | 原因 | 处理 |
|---|---|---|
| `No such container: kbuild` | 容器被删 | 重新 `docker run -dit --name kbuild …` |
| `Container kbuild is not running` | 关机重启后容器停了 | `docker start kbuild` |
| `bash: cd: /work/...: No such file` | 目录名错（`my_driver` 少个 s 之类） | `docker exec kbuild ls /work/lubancat-driver/my_drivers` 核对 |
| `make: *** No rule to make target 'modules'` | 目录里没 Makefile，或 `KERNEL_DIR` 指错 | 查 `pwd` 与 Makefile |
| `the input device is not a TTY` | 非终端环境用了 `-t` | 去掉 `-t` |

### 6. 引号坑

```bash
# ✅ 单引号：$PWD 由容器内 bash 展开
docker exec -it kbuild bash -lc 'cd /work/... && make KERNEL_DIR=$PWD/../../kernel'

# ❌ 双引号：$PWD 会被宿主机先展开，路径变错
docker exec -it kbuild bash -lc "cd /work/... && make KERNEL_DIR=$PWD/../../kernel"
```

### 7. 一句话记住

> `docker exec` = "进到已经在跑的那个环境里做一件事"；`-it` 是"像开终端一样做"，`-d` 是"丢后台做"；`/work` 是宿主机 `/root/usercode` 的镜像路径。

