# 比赛计分主机 (score_system)

武术 / 散打类比赛电子计分系统的**主机端**。创建 WiFi 热点，接收 3 位裁判（`esp32-client`）上报的分数，按 2/3 多数原则裁决本轮得分并累计总分，提供实时网页面板供观众、裁判长查看。

本项目为 [`esp32-client`](../esp32-client) 的配套主机。

---

## 功能特性

- **自建 WiFi 热点** `ScoreSystem`（密码 `12345678`），无需外部路由器
- **内置 Web 面板**，大字号总比分、实时刷新三位裁判分数
- **可编辑队伍名**（输入即自动保存）
- **一键重置总比分**
- **多数裁决算法**：
  - 2 位及以上裁判给出相同分数 → 采用该分数
  - 3 位裁判分数全不相同 → 采用最低分（防刷分兜底）
- **HTML 独立部署**：前端页面放在 LittleFS，改样式无需重编固件
- 支持 3 位固定裁判 + 最多 5 个扩展客户端（代码预留）

---

## 硬件清单

| 元件 | 数量 | 备注 |
|---|---|---|
| ESP32 开发板 | 1 | DevKit V1 或 NodeMCU-ESP32 |
| USB 电源 | 1 | 5V ≥ 500 mA |

**无外设**，只需 ESP32 一块板 + 电源即可。显示和输入全部在客户端侧。

---

## 接线

本机**无任何外部接线**，只需 USB 供电。所有显示都在浏览器上。

---

## 烧录步骤

### 前置条件

- VSCode + PlatformIO 插件
- `platformio.ini` 已包含：`board_build.filesystem = littlefs`（项目自带）

### 1. 烧录固件

- VSCode 左下状态栏点 `→` 图标
- 或 PROJECT TASKS → esp32dev → General → **Upload**

### 2. 上传 Web 页面到 LittleFS（关键步骤）

前端 `data/index.html` 存放在 ESP32 的文件系统分区，**首次部署必须上传一次**。

1. **关闭串口监视器**（串口被占会导致失败）
2. PROJECT TASKS → esp32dev → Platform → **Upload Filesystem Image**
3. 终端看到 `SUCCESS` 后按一次 ESP32 的 **EN/RST** 键

### 3. 以后改网页

只需修改 `data/index.html`，再跑一次 **Upload Filesystem Image** 即可，无需重编固件。

---

## 使用方式

### 启动

1. 给主机通电，串口会打印：
   ```
   WiFi started
   192.168.4.1
   ```
2. 手机 / 电脑连接 WiFi：`ScoreSystem` / 密码 `12345678`
3. 浏览器访问 **`http://192.168.4.1`**（注意是 http 不是 https）
4. 再给 3 台 `esp32-client` 通电（见对应项目的 README）

### 比赛流程

1. 裁判在各自手持客户端打分
2. 每轮结束，三位裁判按下客户端绿色提交键
3. 三方都上报后，主机自动按规则裁决本轮并**累加到总比分**
4. 网页每 500 ms 自动刷新显示

### 修改队伍名

点击页面顶部的 `Player A` / `Player B` 输入框，直接输入新名字，**无需保存按钮**——停止输入 300 ms 后自动持久化到本机内存（注：**断电会丢失**，若要永久保存需改用 NVS，见下方"已知限制"）。

### 重置总比分

点击页面中间的红色"重置总比分"按钮即可。

---

## 网页界面示意

```
┌───────────────────────────────────┐
│   [Player A]   vs   [Player B]   │  ← 可编辑队名
│                                   │
│             0 : 0                 │  ← 总比分（大字号）
│                                   │
│         [ 重置总比分 ]             │
│                                   │
│    裁判1 (client1): 0 : 0         │  ← 三位裁判本轮分数
│    裁判2 (client2): 0 : 0         │
│    裁判3 (client3): 0 : 0         │
└───────────────────────────────────┘
```

---

## 裁决规则说明

对于每一回合，三位裁判会给出自己的红、蓝分数。**红分和蓝分分别独立计算**：

| 三位裁判打分 | 本轮采用 | 说明 |
|---|---|---|
| 5, 5, 5 | 5 | 一致通过 |
| 5, 5, 7 | 5 | 2/3 多数 |
| 7, 5, 5 | 5 | 2/3 多数 |
| 3, 5, 7 | 3 | 全不同 → 取最低分 |
| 0, 0, 1 | 0 | 2/3 多数 |

每轮三位裁判**必须全部上报**才会累加到总分（目前无手动强制结算功能）。

---

## HTTP 接口

| 路径 | 方法 | 功能 |
|---|---|---|
| `/` | GET | 返回前端页面（读 LittleFS 的 `index.html`）|
| `/score` | GET | 返回当前分数 JSON（前端每 500ms 轮询）|
| `/updateScore` | GET | 客户端上报分数：`?client=clientN&red=X&blue=Y` |
| `/setNames` | POST | 更新队伍名：`?a=XXX&b=YYY` |
| `/resetTotal` | POST | 重置总比分为 0:0 |

`/score` 返回格式：

```json
{
  "totalA": 12,
  "totalB": 8,
  "judges": [
    { "id": "client1", "scoreA": 5, "scoreB": 3 },
    { "id": "client2", "scoreA": 5, "scoreB": 3 },
    { "id": "client3", "scoreA": 7, "scoreB": 3 }
  ]
}
```

---

## 故障排查

| 现象 | 原因与处理 |
|---|---|
| 浏览器显示 `index.html not found. Run: pio run -t uploadfs` | 没上传 LittleFS 文件系统。执行"Upload Filesystem Image" |
| 串口 `LittleFS mount failed!` | 分区表异常。代码用 `begin(true)` 会自动格式化，重启即可 |
| 某位裁判上报后网页不更新 | 检查该客户端的 `CLIENT_ID` 拼写是否为 `client1/2/3` |
| 总分迟迟不累加 | 必须 3 位裁判都上报完当前轮才会累加（缺一不行） |
| 连上 WiFi 后网页打不开 | 确认地址栏是 `http://` 不是 `https://` |
| 重启后队名丢失 / 总比分丢失 | 目前仅保存在 RAM 中，需改用 NVS 持久化（见下） |

---

## 目录结构

```
score_system/
├── data/
│   └── index.html        ← Web 前端（编辑后需 Upload Filesystem Image）
├── src/
│   └── main.cpp          ← 主机固件
├── include/              ← 头文件目录（当前为空）
├── lib/                  ← 项目私有库目录
├── test/                 ← 测试
└── platformio.ini        ← 构建配置（含 board_build.filesystem = littlefs）
```

---

## 依赖

```ini
platform = espressif32
board = esp32dev
framework = arduino
board_build.filesystem = littlefs

lib_deps =
  bblanchon/ArduinoJson
  links2004/WebSockets
```

> 注：`ArduinoJson` 与 `WebSockets` 目前**未实际使用**，保留为后续改造预留（例如用 WebSocket 推送替代当前 500 ms 轮询）。

---

## 已知限制 / 改进方向

- 队名、总比分、轮次状态**仅存于 RAM**，断电丢失。可用 `Preferences`(NVS) 几行代码持久化
- `/updateScore` 无认证，任何接入 `ScoreSystem` 热点的设备都可伪造分数
- 前端采用 HTTP 轮询（500 ms），若改用 WebSocket 可降低延迟和流量
- 某位裁判忘按绿键时本轮会无限卡住，缺手动超时/强制结算机制
- 服务器端无时间戳、无比赛历史记录导出
