# esp32-c3-adblock

[English README](README_en.md) · [日本語 README](README_JP.md)

一个运行在 **2 美元的 ESP32-C3** 上的 **类 Pi-hole 的 DNS 广告拦截器** —— *无需 PSRAM*。

> 📰 被 [Tom's Hardware](https://www.tomshardware.com/networking/clever-hacker-fits-537-000-domains-in-a-tiny-usd5-esp32-ad-blocking-dongle-firmware-uses-only-around-50kb-of-ram-and-can-answer-blocked-lookups-in-10-milliseconds)、[XDA Developers](https://www.xda-developers.com/this-tiny-esp32-powered-gadget-blocks-537000-domains-only-uses-50kb-of-ram/) 和 [Korben](https://korben.info/en/half-million-ad-blocking-domains-50kb-ram-esp32.html) 报道。

关键技巧（大多数人会忽略）：你**不需要把拦截列表放在 RAM 里**。把域名以**排序后的 40 位哈希**存进 flash，然后二分查找。**14 万+ 域名**只占约 **0.7 MB** flash，匹配耗时约 **10 ms**，RAM 占用仅 **约 50 KB**。

```
查询进来 ──▶ 提取域名 ──▶ FNV-1a 哈希（+ 父域名后缀）
         ──▶ 二分查找 flash 哈希表
              ├─ 命中 ──▶ 回答 0.0.0.0   （黑洞拦截）
              └─ 未命中 ──▶ 转发给上游解析器，转发回复
```

## 为什么这个方案有意思

大多数 ESP32 DNS 拦截器会把拦截列表（域名的*字符串*）加载进 RAM，所以需要 PSRAM。本项目改为在 flash 中存放固定的 **5 字节（40 位）哈希**：

| | 字符串放 RAM 的方案 | 本方案（哈希放 flash） |
|---|---|---|
| 硬件 | ESP32 + PSRAM（约 $8） | ESP32-C3，无 PSRAM（约 $2） |
| 14.1 万域名 | 约 2.5 MB RAM | **0.67 MB flash** |
| RAM 占用 | 几乎占满 | **约 50 KB** |
| 查找方式 | 字符串比较 | 约 18 次 flash 读取（含 WiFi 往返约 10 ms） |
| 冲突 | 无 | 14.1 万时 0 个（53.7 万时 1 个） |

**为什么是 40 位？** 这是在 flash 预算下的甜点位。冲突符合生日悖论——14.1 万域名时约 0 个冲突，53.7 万时约 1 个（即有一个"倒霉"域名会被多拦截）。降到 32 位能省 20% flash，但在 25 万时会有约 7 个冲突；升到 64 位每个域名浪费 3 字节，去解决一个根本不存在的问题。

同样的技巧在更大的芯片上也成立——这不是 C3 专属的补救方案。在 16 MB 的 ESP32-S3 上，这些哈希能容纳 **约 270 万域名**，而字符串方案在 8 MB PSRAM 里只能放约 46.6 万。哈希放 flash 基本全面胜过字符串放 PSRAM；C3 只是让这一点更无可辩驳。

## 硬件

- 任意 **ESP32-C3** 开发板（在 C3 SuperMini 上测试过），4 MB flash，**无需 PSRAM**
- 经典 **ESP32**（DevKit / WROOM，4 MB）也能编译：`pio run -e esp32dev -t upload`（社区贡献，仅编译验证过；C3 是正式测试目标）
- 用**稳定的 USB 电源**供电（手机充电头或路由器 USB 口）。廉价/松动的 USB-C→A 转接头可能在 WiFi 发射时导致掉压（brown out）
- 一个 **USB-A → USB-C 转接头** 就能直接插到大多数路由器背面的空闲 USB 口——不用额外电源、不用多一个小盒子

### 外壳

C3 SuperMini 的可打印外壳：[`hardware/esp32-c3-supermini-enclosure.stl`](hardware/esp32-c3-supermini-enclosure.stl)

打印注意事项：
- 无需支撑；0.2 mm 层高、约 15% 填充即可
- **天线端要保持净空。** C3 的 PCB 天线是 USB-C 口对面短边上那圈锯齿走线——别把它埋在实心塑料里，也别在附近放金属，否则 RSSI 会变差
- 散热孔留开：板子空闲时温度约 45–55 °C

## 编译与烧录（PlatformIO）—— 详细安装步骤

> 前提：一台装了 **Python 3.x** 的电脑，一根**能传数据**的 USB 线（充电线不行），以及 ESP32-C3 开发板。全程只**插一次 USB**——之后固件和拦截列表都能通过 WiFi 更新（见下文）。
>
> ⚠️ 请使用**较新的 PlatformIO**——VSCode 的 PlatformIO 扩展自带的 core，或 `pip install -U platformio`。发行版/apt 自带的 `platformio` 包（如 4.3.4）太旧，会报 `AttributeError: ... 'resultcallback'`（issue #4）。

### 第 0 步：接线并确认串口
把板子用数据线插进电脑。确认系统识别到了串口：
- Windows：设备管理器 → 端口(COM 和 LPT) → 应看到 **USB Serial Device**，记下它的 COM 号（如 `COM4`）。C3 用原生 USB，通常免驱动。
- 命令行统一确认：`pio device list`（能看到 `/dev/ttyACM0` 或 `COMx`）。
- 如果没识别到：换一根**数据线**（很多线只能充电）；检查板子是否供电（指示灯）。

### 第 1 步：安装 PlatformIO
- 命令行方式（推荐，跟随本文）：`pip install -U platformio`
- 或装 VSCode 的 **PlatformIO IDE** 扩展（自带 core，图形化点按钮烧录）。
- 验证：`pio --version` 能输出版本号即可。若提示找不到 `pio`，确认 Python 的 Scripts 目录在 PATH 里（Windows：`C:\Users\<你>\AppData\Local\Programs\Python\Python311\Scripts`）。

### 第 2 步：配置密钥 `src/secrets.h`
复制模板并编辑（该文件已被 gitignore，不会提交）：
```bash
# Windows:  copy src\secrets.example.h src\secrets.h
# Linux/macOS:  cp src/secrets.example.h src/secrets.h
```
编辑 `src/secrets.h`：
- **`WEB_USER` / `WEB_PASS` / `OTA_PASS` 必须改掉**。它们保护面板的状态变更接口（`/ban`、`/addblock`、`/upload`、`/update`、`/setupdate`、`/forgetwifi`）和网络 OTA。不改就用仓库里公开的默认值，局域网内任何人都能操作你的设备。
- **`WIFI_SSID` / `WIFI_PASS` 是可选的**：可留占位符，改用设备上的配网门户（见下）。

### 第 3 步：构建拦截列表
生成固件要用的哈希表文件（写入 `data/blocklist.bin`）：
```bash
# 方案 A：默认国际列表（StevenBlack base + Hagezi Light，约 10 万条，对 WhatsApp/社交友好）
python tools/build_blocklist.py data/blocklist.bin

# 方案 B：国内增强版（默认 + anti-AD，约 20.3 万条，推荐国内用户）——一键脚本，自动下载三源
python tools/build_china_blocklist.py          # 可加 --upload 直接顺带烧录
```
> `build_china_blocklist.py` 下载 `raw.githubusercontent.com` 时自带重试 + jsDelivr CDN 镜像回退，专门应对国内网络不稳。产物 `data/blocklist.bin` 大小约 0.97 MB，应 ≤ 拦截列表分区（1.06 MB）。

### 第 4 步：编译并烧录（唯一一次 USB）
在项目根目录依次执行：
```bash
pio run -t upload        # ① 编译固件并烧录（含 bootloader、分区表、固件）
pio run -t uploadfs      # ② 烧录文件系统（把 data/blocklist.bin 写进 LittleFS）
pio device monitor       # ③ 打开串口看启动日志
```
- **首次编译会联网下载** espressif32 平台、工具链、Arduino 框架（几百 MB），耗时较长属正常。
- 国内网络下载可能中途超时/报 `HTTPClientError`——**直接重跑同一条命令即可**（断点续传）；那个 `tool-xtensa-esp-elf-gdb` 是调试器，失败不影响烧录，可忽略。
- 若编译报 `program size ... greater than maximum`，是固件超了分区大小，见"踩坑记录"第 1 条。
- 烧录选对串口：若 `pio` 没自动识别，可加 `--upload-port COMx`（Windows）或 `--upload-port /dev/ttyACM0`（Linux）。

### 第 5 步：启动并使用
- 从串口日志看到 `WiFi up: 192.168.0.xxx` 和 `dashboard: http://c3adblock.local` 即成功。
- 浏览器打开 **http://c3adblock.local**（或日志里的 IP）。右上角按钮可**中英文切换**面板。
- 把设备/路由器的 DNS 指向板子的 IP，广告即被拦截（用 `dig @<c3-ip> doubleclick.net` 测试应返回 `0.0.0.0`）。
- 连不上 WiFi 时，设备会开启配网热点 `C3-AdBlock-XXXX`，手机连上选你的 WiFi 输密码即可，无需重刷。

### 自定义拦截列表

`build_blocklist.py OUT.bin [SOURCE ...]` 接受任意 URL 和本地文件的组合，支持以下任意格式：

- **hosts 文件** —— `0.0.0.0 ads.example.com tracker.example.com`（该行所有域名都会被收录）
- **纯域名列表** —— 每行一个域名
- **AdGuard / Adblock 基础规则** —— `||ads.example.com^` 表示拦截，`@@||ok.example.com^` 表示放行某域名（例如镜像 AdGuard Home 的允许列表）

被拦截的域名会连同其子域名一起拦截。DNS 哈希列表无法表达的规则（正则、通配符、`$` 修饰符、`##` 修饰类规则）会被跳过并计数。`@@` 规则只会放行那一条精确条目——它无法从被拦截的父域名中"挖"出一个子域名。如果某个源下载失败，构建会中止而不是悄悄产出更小的列表（用 `--allow-missing` 可覆盖）。

### WiFi 配置（无需重新烧录）

如果设备连不上网（或你从未设置 `secrets.h`），它会启动一个开放的接入点 **`C3-AdBlock-XXXX`**，带强制门户（captive portal）——用手机连上它，选择你的网络，输入密码，搞定。以后要换网络：点击面板上的 **Forget WiFi**，或在**上电时按住 BOOT 按钮**，配网页就会重新出现。（`/forgetwifi` 现在需要鉴权，不再是随便访问的裸 URL——见"安全"一节。）

## 无线更新（无需再插 USB）

在 **http://c3adblock.local** 的面板上全部搞定：

- **拦截列表** —— 把刚生成的 `blocklist.bin` 拖进 *Blocklist → Upload*，或设置 *Remote auto-update* 的 URL，设备会按计划定时拉取预构建的 `blocklist.bin`。GitHub Actions **每周一**会重建默认列表并发布到固定 URL，粘贴一次即可让设备自己保持最新：
  `https://github.com/M-Abozaid/esp32-c3-adblock/releases/download/blocklist/blocklist.bin`
- **固件** —— 在 *Firmware → OTA update* 上传 `.pio/build/c3/firmware.bin`；设备会校验并重启进新镜像。或者从命令行走 WiFi 推送：
  ```bash
  pio run -t upload --upload-port c3adblock.local --upload-protocol espota
  ```

**4 MB flash 的取舍：** 固件 OTA 需要*两个* app 分区，这样留给拦截列表的约只有 **1.3 MB**（**最多约 25 万域名**）。激进的 53.7 万"终极"列表只适合单 app 分区表（无固件 OTA）。在 `partitions.csv` 里选择你的取舍。

> **说明（`partitions.csv` 已更新）：** 使用较新的 PlatformIO 平台（`espressif32@8.x`，Arduino core 3.0+）时，固件体积达到约 **1.43 MB**（`firmware.bin` 约 1,497,536 字节），已放不进原来的 1.3125 MB app 分区。本仓库的 `partitions.csv` 按**国内增强版构建**（默认 StevenBlack+Hagezi + [anti-AD](https://anti-ad.net/) ≈ **20.3 万域名 / 0.97 MB**）做了平衡：每个 app 分区为 **`0x170000`**（能装下固件的最小值），拦截列表分区为 **`0x110000`**（1.06 MB，按 5 字节约 22.2 万域名），偏移在 `0x2f0000`。如果你的列表更小，可以把 app 分区调大、列表分区调小。按此布局重新烧录时，记得**也要重烧文件系统**（`pio run -t uploadfs`）。
>
> **拦截国内广告：** 默认的国际列表（StevenBlack、Hagezi）对国内广告/追踪域名覆盖很少。构建时加一个中文源即可，例如
> `python3 tools/build_blocklist.py data/blocklist.bin <stevenblack> <hagezi> https://raw.githubusercontent.com/privacy-protection-tools/anti-AD/master/anti-ad-domains.txt`。
> 注意：`raw.githubusercontent.com` 在国内网络经常不稳定——建议先把列表下载到本地再传本地文件路径（或使用 jsDelivr CDN 镜像）。效果预期：对**浏览器/网页广告**覆盖良好；但由 App 自家域名投放的**应用内广告**（微信、抖音等）无法在 DNS 层安全拦截。

## 安全

面板的只读页面（`/`、`/stats.json`）保持开放，但每个状态变更接口都要求 **HTTP Basic Auth**（`secrets.h` 里的 `WEB_USER`/`WEB_PASS`）：

- `/ban`、`/addblock`、`/unblock`、`/forgetwifi`
- `/upload`、`/update`（拦截列表和固件 OTA）
- `/setupdate`、`/fetchnow`

网络 OTA（`ArduinoOTA`，例如 `pio run -t upload --upload-port c3adblock.local --upload-protocol espota`）要求同一个文件里的 `OTA_PASS`。

否则，任何能通过局域网访问到设备的人都能在不带任何凭据的情况下给它刷任意固件或改写拦截列表——鉴于这个设备位于你网络上每条 DNS 查询的必经之路上，这一点值得知晓。自定义的被拦截域名在面板渲染前也会做 HTML 转义，堵住了 XSS 路径：否则通过 `/addblock` 加入的含标记的域名字符串会在查看页面时执行。

**这里的 Basic Auth 是"局域网可信边界"级别的控制，不是加密。** 所有流量都是 :80 明文 HTTP——这块芯片没有现实的预算去跑 TLS 服务。Basic Auth 凭据是 base64（未加密）编码，且每次鉴权请求都会发送；任何能嗅探你局域网流量的人（开放/访客 WiFi、ARP 欺骗）都能从线路上读到它们。这加固的是常见场景——网络上另一台设备不带任何凭据打 API，或浏览器标签页 CSRF 它——而不是针对在线路上的网络攻击者。

**经由缓存 Basic Auth 的 CSRF：** 浏览器会自动把缓存的 Basic Auth 凭据附加到*之后*发往同一已鉴权来源的任何请求——包括同一浏览器稍后访问的完全无关的页面所触发的请求（例如 `<img src="http://c3adblock.local/forgetwifi">`，无需 JS）。这意味着一旦你在面板登录过一次，任何网页都能在无凭据的情况下静默驱动这个 API，无论它在不在你的局域网。因此，上面每个可变接口现在都要求额外的 `X-Requested-With: c3-adblock` 请求头——普通 `<img>`/自动提交的 `<form>` CSRF 无法附加这个头（只有同源的 `fetch()` 能，面板自己的 JS 正是这么做的）——这就是为什么 `/forgetwifi` 不再是可直接访问的裸 URL；请改用面板按钮。

**默认凭据：** 如果 `secrets.h` 里还保留着 `secrets.example.h` 中的占位值 `CHANGE_ME_WEB_PASSWORD` / `CHANGE_ME_OTA_PASSWORD`，设备会以"公开"的密码启动（它就躺在这个仓库的示例文件里）。固件会在串口打一条警告，并在面板上显示横幅——但它仍会正常启动运行。所以在把这个设备放到你不完全控制的网络之前，别跳过在 `secrets.h` 里设置真实值这一步。

目前不在范围内：WiFi 配网页的接入点（`C3-AdBlock-XXXX`）仍是开放的（未加密），这是设计使然——它必须先能被无密码连接。你在配网页里输入的真实 WiFi 密码，在短暂的配置窗口内只和这段本地无线电链路一样安全。

## 使用

把某台设备的 DNS 指向 C3 的 IP，或把它作为主 DNS 后面的**备用解析器**。测试：

```bash
dig @<c3-ip> doubleclick.net   # -> 0.0.0.0  （被拦截）
dig @<c3-ip> github.com        # -> 真实 IP  （正常转发）
```

### 面板按钮说明

面板（http://c3adblock.local）右上角有**中英文切换**按钮（语言选择会记住，下次打开保持）。其余按钮含义：

- **Pause / Resume（暂停 / 恢复）**：临时或永久暂停 DNS 拦截，下拉可选 30s / 5min / 30min / 直到我重新启用。
- **Block domain（拦截域名）**：手动把一个自定义域名加入黑名单（`/addblock`）。
- **Upload blocklist（上传拦截列表）**：把本地构建好的 `blocklist.bin` 直接传上去，无需 USB（`/upload`）。
- **Save（保存）+ Fetch now（立即拉取）**：见下方"远程自动更新"。
- **Flash firmware（刷写固件）**：上传 `.pio/build/c3/firmware.bin` 做 OTA 固件更新（`/update`）。
- **Forget WiFi（忘记 WiFi）**：清空已保存的 WiFi，重启进入配网门户（`/forgetwifi`）。

**「立即拉取」（Fetch now）执行什么？** 它不是运行任何电脑上的脚本，而是**触发设备固件自身的端点 `/fetchnow`**（`src/main.cpp` 里 `fetchBlocklist(updateUrl)`）：设备自己 GET 你配置的远程 URL，把那份**预构建的 `blocklist.bin`** 下载下来、校验后**原子替换**当前列表，并返回状态（如 `ok: 203683 domains`）。要点：

- 前提：先在「远程自动更新」输入 URL、点 **Save**（写 `update.cfg`）。没配 URL 时点它会提示 `no url set`。
- URL 必须是能被设备访问到的、直接返回**同一格式**（5 字节哈希）`blocklist.bin` 的 HTTP(S) 地址（例如 GitHub release 附件；支持 https 和重定向）。**不能**是拦截列表的"源码"（hosts/域名文本），设备不负责构建。
- 配套的「Save」+ 周期（`h`）还会让设备**定时自动拉取**（`loop()` 里按间隔调用同一个函数）。
- 如果你用 USB 手动上传（Upload blocklist）就已经更新了，这个按钮可留空不用。

## 踩坑记录（血泪教训）

- **固件超分区（`program size ... greater than maximum`）**：新版 Arduino 内核把固件编到约 **1.4 MB**，超出项目原设计的分区。本仓库 `partitions.csv` 已按**国内增强版（20.3 万条 / 0.97 MB）**重新平衡：app 分区各 `0x170000`（装下 1.4 MB 固件），拦截列表分区 `0x110000`（1.06 MB，偏移 `0x2f0000`）。**如果你改了分区表或构建更大的列表，务必让固件 ≤ app 分区、`blocklist.bin` ≤ 拦截列表分区**，否则 `pio` 会直接报错（不会烧坏，改回来重烧即可）。
- **国内拉取 `raw.githubusercontent.com` 经常超时/SSL 中断**：`python tools/build_china_blocklist.py` 自带重试 + jsDelivr CDN 镜像回退，能缓解。仍失败就多跑一次，或先手动下载列表再传本地路径给 `build_blocklist.py`。
- **ModemManager**（Fedora/Ubuntu 默认自带）会抢占 `/dev/ttyACM0` 并拨动 DTR/RTS，导致**重置 C3** 并占用串口。解决：
  ```bash
  sudo systemctl stop ModemManager
  echo 'ATTRS{idVendor}=="303a", ENV{ID_MM_DEVICE_IGNORE}="1"' | sudo tee /etc/udev/rules.d/99-esp-no-modemmanager.rules
  sudo udevadm control --reload-rules && sudo udevadm trigger
  ```
- C3 的 USB-Serial-JTAG 控制台在主机连接之前可能会吞掉早期启动输出（`while(!Serial)` 有帮助）。
- DNS 客户端会加一个 **EDNS OPT** 记录；被拦截的回复只能包含 question + answer（ANCOUNT=1，NSCOUNT=ARCOUNT=0），否则会被判定为畸形包。

## 已完成 / 可扩展方向

- ✅ Web 面板——按客户端统计拦截/放行数、拉黑客户端、添加自定义域名
- ✅ mDNS（`c3adblock.local`）自动发现
- ✅ OTA——固件和拦截列表均可 WiFi 更新，外加定时远程拉取拦截列表
- ✅ 强制门户 WiFi 配置（无需硬编码凭据）+ 一键浏览器 Web 安装器
- ⬜ 分桶前缀索引——约 18 次 flash 读取/次查找 → 约 1–2 次（issue #3），吞吐量提升点
- ⬜ 兼任 DHCP 服务器（把自己作为 DNS 分发出去），实现真正的即插即用

## 致谢

灵感来自 [s60sc/ESP32_AdBlocker](https://github.com/s60sc/ESP32_AdBlocker)——"拦截列表里的域名回答 0.0.0.0"这个想法。这是一个从零开始、专注哈希放 flash 优化、面向无 PSRAM 芯片的独立实现。

## 许可证

MIT——见 [LICENSE](LICENSE)。
