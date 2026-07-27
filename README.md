# AC-Remote 空调遥控器与智能控制系统 (ESP32-S3)

基于 **ESP32-S3-WROOM-1-N16R8** 开发的高性能、高可扩展性空调遥控与多功能控制系统嵌入式固件。

---

## 🌟 核心特性与亮点

* **层次化模块解耦架构**：
  * `driver/`：硬件外设与存储驱动层
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
| **`task status`** | 查看当前所有任务的运行状态 | `task status` |
| **`task start <task>`**| 动态启动指定任务 (如 net) | `task start net` |
| **`task stop <task>`** | 动态停止指定任务 | `task stop net` |
| **`nvs_list`** | 查看存储在 NVS 中的所有键值对 | `nvs_list nvs -n storage` |
| **`nvs_set`** | 写入设置某个 Key 的值 | `nvs_set nvs storage wifi_ssid str -v MyWiFi` |
| **`tasks`** | 列出系统底层所有 FreeRTOS 线程 CPU/内存占用 | `tasks` |
| **`restart`** | 重新启动 ESP32-S3 | `restart` |
