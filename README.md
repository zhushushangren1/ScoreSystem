# 比赛计分主机 `score_system`

这是整套计分系统的主机端，对应裁判客户端项目 [esp32-client](../esp32-client)。

主机负责：
- 创建 WiFi 热点 `ScoreSystem`
- 接收 3 个裁判客户端提交的本轮分数
- 按 2/3 多数规则结算本轮
- 累加总比分
- 提供展示页和控制页

## 当前规则

- 每轮只接受 `client1`、`client2`、`client3` 各提交 1 次
- 同一裁判在同一轮重复提交会被拒绝
- 三名裁判全部提交后，本轮自动锁定
- 本轮锁定后，客户端会显示 `----`
- 控制页点击“下一轮”后开启下一轮
- 控制页点击“重置总比分”会清空总比分和当前裁判显示分数

## 热点信息

- SSID: `ScoreSystem`
- Password: `12345678`

## 网页地址

- `http://192.168.4.1/`
  展示页，只显示队伍名称和总比分。

- `http://192.168.4.1/control`
  控制页，用于编辑队伍名称、总比分，查看 3 名裁判当前轮分数，并控制下一轮/重置。

## 显示同步

控制页里的队伍名称和总比分是可编辑的。编辑后需要点击“同步到显示页”按钮，才会更新主机保存的队名和总比分，展示页会通过轮询自动更新，无需手动刷新。

## 烧录

### 1. 烧录固件

在 PlatformIO 中执行 `Upload`。

### 2. 上传网页文件

网页文件在：
- `data/index.html`
- `data/control.html`

修改网页后必须额外执行一次 `Upload Filesystem Image`。如果只烧录固件，不上传文件系统，网页不会更新。

## 接口

| 路径 | 方法 | 说明 |
|---|---|---|
| `/` | `GET` | 返回展示页 |
| `/control` | `GET` | 返回控制页 |
| `/score` | `GET` | 返回队名、总比分、裁判分数、轮次状态 |
| `/roundStatus` | `GET` | 返回当前轮次是否开放 |
| `/updateScore` | `GET` | 裁判客户端提交分数 |
| `/syncDisplay` | `POST` | 同步控制页编辑的队名和总比分 |
| `/nextRound` | `POST` | 开启下一轮 |
| `/resetTotal` | `POST` | 重置总比分和当前轮分数 |

### `/score` 返回示例

```json
{
  "teamA": "Player A",
  "teamB": "Player B",
  "totalA": 12,
  "totalB": 8,
  "roundOpen": false,
  "roundId": 3,
  "judges": [
    { "id": "client1", "scoreA": 5, "scoreB": 3, "submitted": true },
    { "id": "client2", "scoreA": 5, "scoreB": 3, "submitted": true },
    { "id": "client3", "scoreA": 7, "scoreB": 3, "submitted": true }
  ]
}
```

## 故障排查

| 现象 | 说明 |
|---|---|
| 展示页还是旧内容 | 执行 `Upload Filesystem Image` |
| 展示页队名/比分没变 | 控制页编辑后还没有点“同步到显示页” |
| 某裁判提交无效 | 同一轮已提交过，或当前轮已锁定 |
| 客户端一直显示 `----` | 控制页还没有点“下一轮” |
| 网页打不开 | 确认访问的是 `http://192.168.4.1`，不是 `https://` |

## 已知限制

- 队名、总比分、轮次状态目前只保存在 RAM 中，断电会丢失
- `/updateScore` 没有额外鉴权，只依赖约定的客户端 ID
- 前端仍然是 500ms 轮询，不是 WebSocket 实时推送
