# 鲁班猫2N (LubanCat-2N) 开发板实战操作与排错全记录

本文档完整总结了在配置鲁班猫2N（Debian 10 XFCE桌面版）过程中，从硬件连接、网络调试、远程控制到系统备份的完整流程，并详细记录了历史报错与解决方案。

## 目录

- [一、 硬件连接与串口调试 (初期踩坑)](#一-硬件连接与串口调试-初期踩坑)
- [二、 网络连接与驱动排错 (核心难点)](#二-网络连接与驱动排错-核心难点)
- [三、 SSH 远程登录与文件传输](#三-ssh-远程登录与文件传输)
- [四、 VNC 与图形桌面配置](#四-vnc-与图形桌面配置)
- [五、 硬件信息与系统底层查询](#五-硬件信息与系统底层查询)
- [六、 TF卡挂载与系统备份排错](#六-tf卡挂载与系统备份排错)
- [七、 常用系统维护命令备忘](#七-常用系统维护命令备忘)

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

