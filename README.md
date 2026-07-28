# AC-Remote 空调遥控器与智能控制系统 (ESP32-S3)

基于 **ESP32-S3-WROOM-1-N16R8** 开发的高性能、高可扩展性空调遥控与多功能控制系统嵌入式固件。

---

## 🌟 核心特性与亮点

* **层次化模块解耦架构**：
  * `driver/`：硬件外设与存储驱动层
  * `hardware/`：板载器件抽象层（红外收发器、AHT20 等）
  * `protocol/`：空调控制意图、协议路由和品牌红外编码层
  * `ac/`：空调通用类型与状态管理层
  * `config/`：应用配置与数据持久化层
  * `wifi/` & `ble/`：无线通信协议层
  * `task/`：独立的 FreeRTOS 任务调度管理中间层
  * `components/cmd_task/`：解耦式 Shell 控制台任务管理组件

* **NVS Key-Value 强类型存储**：
  * 基于默认物理 `nvs` 分区（已扩展至 256KB），在 `storage` 命名空间下提供强类型的独立 Key-Value 键值对存取（支持 `wifi_ssid`、`wifi_pass`、`dev_name`、`dev_id` 等独立配置）。

* **BLE 蓝牙调参与无线服务**：
  * 内置 BLE GATT Server 广播服务，默认广播名称为 `AC-Remote`，方便手机 APP 连接与参数调试。

* **WiFi 无线网络与自动重连 Task**：
  * 在 `main/task/net/` 目录下独立的 `net_task` 线程中运行，支持后台自动尝试连接与断线无限重连监控，不阻塞系统主流程。

* **解耦式 Task 管理中间层与 Shell 指令**：
  * 提供 `task_manager` 表驱动注册中台，支持控制台交互指令：`task start`、`task stop`、`task status`。

---

## 📁 工程目录结构

```text
esp32s3/
├── components/                # ESP-IDF 自定义组件
│   └── cmd_task/              # 任务控制 Shell 组件 (task start/stop/status)
├── main/
│   ├── main.c                 # 系统入口与应用初始化
│   ├── task/                  # 任务调度管理总目录
│   │   ├── task_manager.h/c   # 任务管理中间层 (表驱动注册表)
│   │   └── net/               # [网络任务子目录]
│   │       ├── net_task.h
│   │       └── net_task.c
│   ├── driver/                # 底层驱动
│   │   └── nvs/               # NVS 存储驱动 (storage 命名空间)
│   ├── hardware/              # 板载器件抽象 (红外收发、AHT20)
│   ├── protocol/              # 空调协议管理中间层
│   │   ├── protocol_types.h   # 通用控制意图与红外帧
│   │   ├── protocol_manager.* # 品牌路由、能力检查与统一编码
│   │   ├── haier/             # 海尔 YRW02 协议
│   │   └── gree/              # 格力协议
│   ├── ac/                    # 空调类型与状态管理
│   ├── config/                # 应用配置保存与读取 (sys_config_t)
│   ├── wifi/                  # WiFi STA 模式模块
│   ├── ble/                   # BLE GATT Server 蓝牙调参模块
│   └── shell/                 # 控制台 Shell 模块
├── partitions_example.csv     # 自定义 Flash 分区表
├── sdkconfig.defaults         # 默认 sdkconfig 配置文件
└── README.md
```

---

## 🛠️ 硬件与开发环境

* **主控芯片**：ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB Octal PSRAM)
* **串口下载**：原生 `USB_SERIAL_JTAG`
* **开发框架**：ESP-IDF v6.0+

---

## 🚀 编译与烧录指南

### 1. 编译工程
```powershell
idf.py build
```

### 2. 烧录并打开串口监视器 (以 COM6 为例)
```powershell
idf.py -p com6 flash monitor
```

---

## 💻 常用控制台指令指南

启动后，在控制台提示符 `control>` 下可使用以下命令：

| 终端命令 | 说明 | 示例 |
| :--- | :--- | :--- |
| **`ac` / `ac help`** | 查看空调控制命令帮助 | `ac help` |
| **`ac get`** | 查看最后发送的空调缓存状态 | `ac get` |
| **`ac learn`** | 开启红外学码接收模式 (对准 IO5 按遥控器) | `ac learn 5` |
| **`ac emit`** | 重发上一次成功学到的红外波形 | `ac emit` |
| **`ac on` / `ac off`** | 快捷开机 / 关机 | `ac on` |
| **`ac send`** | 按品牌和状态发送空调红外指令 | `ac send haier on cool 25 low` |
| **`ac timer <brand> on <minutes>`** | 设置空调自身延时开机（当前仅海尔） | `ac timer haier on 60` |
| **`ac timer <brand> off <minutes>`** | 设置空调自身延时关机（当前仅海尔） | `ac timer haier off 90` |
| **`ac timer <brand> cancel`** | 取消空调自身定时（当前仅海尔） | `ac timer haier cancel` |
| **`task status`** | 查看当前所有任务的运行状态 | `task status` |
| **`task start <task>`**| 动态启动指定任务 (如 net / control) | `task start control` |
| **`task stop <task>`** | 动态停止指定任务 | `task stop control` |
| **`nvs_list`** | 查看存储在 NVS 中的所有键值对 | `nvs_list nvs -n storage` |
| **`nvs_set`** | 写入设置某个 Key 的值 | `nvs_set nvs storage wifi_ssid str -v MyWiFi` |
| **`tasks`** | 列出系统底层所有 FreeRTOS 线程 CPU/内存占用 | `tasks` |
| **`restart`** | 重新启动 ESP32-S3 | `restart` |

### 红外协议调用链

```text
Shell / BLE / 网络控制
        ↓ ac_request_t
control_task 独占队列
        ↓
protocol_manager 品牌路由与能力检查
        ↓ ir_frame_t
ir_remote / ir_driver 发送
```

上层只描述品牌、动作和通用参数；品牌目录负责构造各自协议帧。当前
`ac_state` 保存最后一次成功发送的控制状态，后续可扩展为按设备 ID 保存多设备状态。
