# 固件升级操作指导

> 语言 / Language: **中文** | [English](TEST_GUIDE.en.md)

本文档指导在树莓派设备上通过 DJI Assistant 2（DA2）对 Payload 进行固件升级，
覆盖「收到固件 → 覆盖运行中的二进制 → 重启 → 开机自启新版本 → 上报 COMPLETE」的完整闭环。

部署形态：**单 bin + systemd 常驻服务**。开机由 systemd 无参裸启 `sample_things_model`，
程序检测到升级标记/重启状态文件即自动进入常驻升级任务（后台待命）：

- 依赖库从 bin 旁的 `./lib` 自动解析（rpath `$ORIGIN/lib`），无需 LD_LIBRARY_PATH / ldconfig；
- widget 配置从 bin 旁的 `./config` 自动解析，与启动时的工作目录无关；
- 固件安装路径默认取 `/proc/self/exe`（覆盖运行中的 bin 自身），无需传参。

## 零、两道门闩（真装+重启必须都开）

默认样例是 **stage-only**：跑完整协议但**不覆盖二进制、不重启**。真机实测需同时开两道门：

1. **编译门** `-DUPGRADE_REAL_INSTALL=ON`
   不开则安装/重启/重启状态/网络代码根本不编进二进制。
   校验：`strings sample_things_model | grep -c 'rebooting system now'` → 真装版为 `1`，stage 版为 `0`。
2. **运行门** `PSDK_UPGRADE_APPLY`（由 systemd unit 注入）
   | 值 | 行为 |
   |---|---|
   | 不设 | stage-only（写固件，不覆盖，不重启） |
   | `dry` | 覆盖目标二进制但**不重启**（任意机器上验证整条管线） |
   | `1`/`real` | 覆盖 + 持久化重启状态 + **重启**（需 root） |

> `deploy/install.sh` 会用 `strings` 校验第 1 道门确已编入，并安装带 `PSDK_UPGRADE_APPLY=1` 的
> systemd unit 帮你合上第 2 道门。

## 一、编译真装固件（开发机，aarch64 交叉）

```bash
cmake -B build -S projects/linux \
  -DSAMPLE_BUILD=cross \
  -DUPGRADE_REAL_INSTALL=ON \
  -DSAMPLE_DEVICE_FIRMWARE_VERSION=01.00.00.00
cmake --build build -j"$(nproc)"
# 校验真装代码已编入（应输出 1）
strings build/sample_things_model | grep -c 'rebooting system now'
```

产物：`build/sample_things_model`（自包含，rpath `$ORIGIN/lib`）、`build/config/`。

## 二、部署到设备并「武装」自启

把二进制 + config + deploy 脚本拷到设备（示例目录 `~/psdk/`）：

```bash
scp -r build/sample_things_model build/config samples/upgrade/deploy  rsp@<设备IP>:~/psdk/
# 系统没有预装依赖库时，再拷一份 lib/（libmosquitto/libssl/libcrypto）到 bin 同级：
# scp -r samples/third_party/aarch64-linux-gnu/lib  rsp@<设备IP>:~/psdk/lib
```

登录设备，用仓库自带的 systemd 安装器（需 root）：

```bash
ssh rsp@<设备IP>
cd ~/psdk
sudo ./deploy/install.sh ./sample_things_model
```

`install.sh` 做四件事：
1. 装二进制到 `/usr/local/bin/sample_things_model`（+ 同级 `config/`）；
2. 装网络前置脚本到 `/usr/local/bin/psdk-net-setup.sh`；
3. 装 systemd unit `psdk-iot-upgrade.service`（`ExecStart=/usr/local/bin/sample_things_model`，
   `Environment=PSDK_UPGRADE_APPLY=1`，`ExecStartPre=-/usr/local/bin/psdk-net-setup.sh`）；
4. `systemctl enable --now`，服务立即常驻，重启后自动拉起。

```bash
journalctl -u psdk-iot-upgrade -f     # 实时日志
```

应看到：（配网）→ 连 broker → "Registered 4 upgrade command handlers, waiting...".

> 想关掉终端仍在运行 / 交给自启：`sudo reboot` 即可，systemd 会拉起。
> 升级标记 `/var/lib/psdk_iot/upgrade_autorun` 与重启状态 `/var/lib/psdk_iot/upgrade_reboot_state`
> 由程序自维护，无需手动 touch。删除标记即恢复无参数启动进交互菜单的调试行为。

### config 控件资源包（手动推送，随目录走）

widget 控件资源（`config/en`、`config/zh` 下的 `widget_config.json` + png 图标）用于 DA2 把
Payload 列为可管理设备并显示控件界面。程序在 `on_session_ready` 里用**运行二进制的同级目录**
解析它（`SelfDirPath("config")`，与启动 cwd 无关），所以资源包必须落在：

```
/usr/local/bin/config/           ← 与 /usr/local/bin/sample_things_model 同级
├── en/{widget_config.json, *.png}
└── zh/{widget_config.json, *.png}
```

- **首次部署**：上面 `install.sh` 已自动把 `~/psdk/config` 拷到 `/usr/local/bin/config`，无需额外操作。
- **单独/重新推送**（例如换了图标、改了 `widget_config.json`）：直接手动推目录即可，**无需重编二进制**：

  ```bash
  scp -r build/config  rsp@<设备IP>:/tmp/config
  ssh rsp@<设备IP> 'sudo rm -rf /usr/local/bin/config && sudo cp -r /tmp/config /usr/local/bin/config'
  ssh rsp@<设备IP> 'sudo systemctl restart psdk-iot-upgrade'   # 重新注册 get_file_list
  ```

> ⚠️ **OTA 不携带 config**：固件升级只原子替换二进制本身（见第五节），`config/` 保持原样。
> 若某个新版本**改动了控件资源**，升级后必须**手动补推** `config` 目录到 `/usr/local/bin/config`
> （用上面的命令），否则 DA2 显示的仍是旧资源。控件资源没变则无需动。
> 缺失 config 不影响升级本身：升级四命令+重启+COMPLETE 与 widget 无关，仅 DA2 的控件列表会
> 回 "directory not found"（可 `THING_MODEL_UPGRADE_WIDGET=0` 显式关掉 widget 注册）。


### 网络配置（前置脚本，非自动内建）

升级重启后新进程要先联网才能连 Broker 上报 COMPLETE。网络 bring-up 由 systemd unit 的
`ExecStartPre=-/usr/local/bin/psdk-net-setup.sh` 负责（`install.sh` 已一并安装）。脚本幂等：
等 uplink 拿到 DHCP 租约 → 探测 Broker → 不通则 IP 挪到网桥（网桥存在时）→ 加 Broker 网段路由。

> **拓扑关键点**：本载板上 `usb0` 是网桥 `pi4br0` 的桥端口（二者共用 MAC），桥端口只做二层
> 转发、L3 报文发不出去。所以脚本把飞机网段 IP 从 usb0 **挪到 pi4br0**、路由也加在 pi4br0 上
> （`ip route add <broker网段> via <网关> dev pi4br0 src <飞机IP>`），与仓库根目录 `main.c` 一致。

默认拓扑参数与本载板一致，可在 unit 里用 `Environment=` 覆盖：

| 环境变量 | 默认值 | 说明 |
|---|---|---|
| `THING_MODEL_NET_UPLINK` | `usb0` | 飞机侧 DHCP 网卡 |
| `THING_MODEL_NET_BRIDGE` | `pi4br0` | IP 迁移目标网桥（不存在则留在 uplink） |
| `THING_MODEL_NET_BROKER_SUBNET` | `192.0.2.0/24` | Broker 网段路由 |
| `THING_MODEL_NET_GATEWAY` | 自动探测 | Broker 网段下一跳；uplink 无 default 路由时**必须显式设置**，否则路由不下发 |
| `THING_MODEL_NET_WAIT` | `45` | 开机等 uplink 出现并拿到租约的秒数（USB gadget/网桥晚于服务启动，必须等） |
| `THING_MODEL_BROKER` | 连接配置里的 broker | 已能 ping 通则脚本直接跳过配网 |

> 若设备已用 netplan / NetworkManager 把飞机侧网络配好，删掉 unit 里的 `ExecStartPre` 行即可。
> 注意清理旧残留：若 `/etc/environment` 残留指向不存在文件的 `PSDK_EXCE_FILE`（如恢复步骤写入的
> `/not/exist/none`），程序会告警并自动回退为覆盖运行中的 bin 自身，但建议删除该残留行避免混淆。

## 三、确认服务在线

```bash
ps aux | grep sample_things_model        # 进程在
sudo systemctl status psdk-iot-upgrade   # 服务 active (running)
journalctl -u psdk-iot-upgrade -f        # 实时日志：配网 → 连 broker → 升级待命
```

DA2 的设备列表中应出现该 Payload。自启日志也可落盘 `/var/log/psdk_iot.log`
（`THING_MODEL_LOG_FILE` 改路径，设 `0`/`off` 关闭；超 8MB 轮转到 `.1`）。开机横幅含固件版本与 pid：

```bash
grep "autorun boot" /var/log/psdk_iot.log   # 历次开机横幅，可核对升级后版本是否变化
```

## 四、升级

1. DA2 连接设备，进入「固件升级」页面。
2. 选择**新版本**固件文件（见「五、验证版本变化」），点击升级。
3. 等待进度完成。日志走完整链路：

   ```
   enter_upgrade_mode → HTTP server on ... (web root /tmp/upgrade_share)
   [http] PUT /rpi_test_V<新版本>.bin -> saved N bytes     # 飞机把固件 PUT 到设备
   check_firmware -> passed
   [UPG] Flash progress 0..100%
   installing new firmware /tmp/upgrade_share/... -> /usr/local/bin/sample_things_model
   persisted reboot state (end_state=1 ver=<新版本>)
   pushing REBOOTING → [platform] rebooting system now...
   ```
4. 设备自动重启 → systemd 拉起**新** `/usr/local/bin/sample_things_model`（检测到重启状态文件自动进常驻）
   → push COMPLETE → DA2 显示升级成功、固件版本号更新为新版本。

## 五、验证版本变化：准备一个「新版本」固件

固件版本在编译期由 `SAMPLE_DEVICE_FIRMWARE_VERSION` 烧入（单一真源 `sample_device_info.h`）。
要观察到升级后版本变化，需让**设备上在跑的版本**与**推给 DA2 的版本**不同：设备装 v1，DA2 推 v2。

```bash
# v2：更高版本，作为升级包（就是高版本号的同一个二进制）
cmake -B build_v2 -S projects/linux \
  -DSAMPLE_BUILD=cross -DUPGRADE_REAL_INSTALL=ON \
  -DSAMPLE_DEVICE_FIRMWARE_VERSION=01.00.00.01
cmake --build build_v2 -j"$(nproc)"
cp build_v2/sample_things_model  rpi_test_V01.00.00.01.bin   # 拷到 DA2 电脑，升级时选它
```

> ⚠️ 单 bin 升级只替换二进制文件本身，**不替换** `config/`、`lib/`。若新版本改动了控件
> 资源（`config`）或依赖库（`lib`），升级后需手动补推对应目录到 `/usr/local/bin/`（config
> 的推送命令见「二·config 控件资源包」），仅二进制变化则无需。

## 六、（可选）真重启前的 dry-run 预演

真机重启前，可在任意机器上用 `dry` 模式跑通「覆盖二进制但不重启」的完整管线：

```bash
cmake -B build_host -S projects/linux -DSAMPLE_BUILD=host -DUPGRADE_REAL_INSTALL=ON
cmake --build build_host -j"$(nproc)"
PSDK_UPGRADE_APPLY=dry ./build_host/sample_things_model upgrade
# 日志出现 "(dry: install only, no reboot)" 即管线正常
```

## 七、环境恢复

```bash
sudo systemctl disable --now psdk-iot-upgrade.service
sudo rm -f /etc/systemd/system/psdk-iot-upgrade.service \
           /usr/local/bin/sample_things_model \
           /usr/local/bin/psdk-net-setup.sh
sudo rm -rf /var/lib/psdk_iot /tmp/upgrade_share      # 清升级标记/状态/暂存固件
sudo systemctl daemon-reload
sudo reboot
```

## 八、常见问题

| 现象 | 处理 |
|------|------|
| DA2 看不到设备 | 确认服务 active（`systemctl status psdk-iot-upgrade`）、能连 Broker |
| 程序启动即退出（找不到 .so） | 把 `lib/`（libmosquitto/libssl/libcrypto）放到 bin 同级，或系统安装后 `ldconfig` |
| 升级"成功"但版本没变 | ①二进制是 stage 版（`strings ... grep -c 'rebooting system now'` 为 0，需重编开 `-DUPGRADE_REAL_INSTALL=ON`）；②手动从别处目录跑导致覆盖了别的副本——统一走 systemd 那份 `/usr/local/bin/...`；③DA2 选的固件版本与当前相同 |
| 升级进度卡住 | 确认设备已联网、能访问 Broker；检查 `psdk-net-setup.sh` 是否配通（`journalctl` 里的 `[psdk-net-setup]` 行） |
| 卡在 `[platform] rebooting` | reboot 需 root/CAP_SYS_BOOT；systemd 以 root 运行 unit 时正常 |
| widget `load_config ... FAIL` | widget 样例按相对 `./config` 找资源；unit 需 `WorkingDirectory=/usr/local/bin`（本仓库 unit 已内置），或确认 `/usr/local/bin/config` 存在 |
| restart 后卡在 CONNACK 不动 | 同 client_id 重复连接互踢（残留手动实例/旧会话）；`pkill -f sample_things_model` 后重启，或整机 reboot |
