#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// 热点信息
const char* ssid = "ScoreSystem";
const char* password = "12345678";

// 创建服务器
WebServer server(80);

// 最大客户端数量（保留扩展能力）
#define MAX_CLIENTS 5

// 客户端比分数据结构
struct ClientScore {
    String clientId;
    int scoreA;
    int scoreB;
    bool active;
};

// 客户端比分数组
ClientScore clients[MAX_CLIENTS];

// 固定三位裁判ID
const char* JUDGE_IDS[3] = {"client1", "client2", "client3"};

// 当前轮三位裁判上报分数
int roundScoreA[3] = {0, 0, 0};
int roundScoreB[3] = {0, 0, 0};
bool roundSubmitted[3] = {false, false, false};

// 总比分（按轮累计）
int totalScoreA = 0;
int totalScoreB = 0;

// 网页上可编辑队名
String teamAName = "Player A";
String teamBName = "Player B";

// 查找或创建客户端
int findOrCreateClient(const String& clientId) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].clientId == clientId) {
            return i;
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].clientId = clientId;
            clients[i].scoreA = 0;
            clients[i].scoreB = 0;
            clients[i].active = true;
            Serial.print("New client registered: ");
            Serial.println(clientId);
            return i;
        }
    }
    return -1;
}

int findJudgeIndexById(const String& clientId) {
    for (int i = 0; i < 3; i++) {
        if (clientId == JUDGE_IDS[i]) return i;
    }
    return -1;
}

bool isRoundComplete() {
    return roundSubmitted[0] && roundSubmitted[1] && roundSubmitted[2];
}

int calculateFinalScore(int s1, int s2, int s3) {
    // 两个及以上裁判给出同一分数，采用该分数
    if (s1 == s2 || s1 == s3) return s1;
    if (s2 == s3) return s2;

    // 三个分数都不同，采用最低分
    int minScore = s1;
    if (s2 < minScore) minScore = s2;
    if (s3 < minScore) minScore = s3;
    return minScore;
}

void resetRound() {
    for (int i = 0; i < 3; i++) {
        roundScoreA[i] = 0;
        roundScoreB[i] = 0;
        roundSubmitted[i] = false;
    }
}

void resetClientScores() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            clients[i].scoreA = 0;
            clients[i].scoreB = 0;
        }
    }
}

// 网页内容
String getHTML() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        Serial.println("Failed to open /index.html");
        return String("<html><body>index.html not found. Run: pio run -t uploadfs</body></html>");
    }
    String html = file.readString();
    file.close();
    html.replace("%TEAM_A%", teamAName);
    html.replace("%TEAM_B%", teamBName);
    return html;
}

// 主页
void handleRoot() {
    server.send(200, "text/html", getHTML());
}

// 比分接口（前端轮询）
void handleScore() {
    String judgesJson = "";
    for (int i = 0; i < 3; i++) {
        if (i > 0) judgesJson += ",";
        judgesJson += "{\"id\":\"" + String(JUDGE_IDS[i]) + "\",\"scoreA\":" + String(roundScoreA[i]) + ",\"scoreB\":" + String(roundScoreB[i]) + "}";
    }

    String json = "{\"totalA\":" + String(totalScoreA) + ",\"totalB\":" + String(totalScoreB) + ",\"judges\":[" + judgesJson + "]}";
    server.send(200, "application/json", json);
}

// 更新队伍名称
void handleSetNames() {
    if (server.hasArg("a")) {
        teamAName = server.arg("a");
    }
    if (server.hasArg("b")) {
        teamBName = server.arg("b");
    }
    String json = "{\"ok\":true}";
    server.send(200, "application/json", json);
}

// 重置总比分
void handleResetTotal() {
    totalScoreA = 0;
    totalScoreB = 0;
    resetRound();
    resetClientScores();

    Serial.println("Total score and judge scores reset from web button");

    String json = "{\"ok\":true}";
    server.send(200, "application/json", json);
}

// 接收客户端发送的比分
void handleUpdateScore() {
    if (server.hasArg("client") && server.hasArg("red") && server.hasArg("blue")) {
        String clientId = server.arg("client");
        int red = server.arg("red").toInt();
        int blue = server.arg("blue").toInt();

        int index = findOrCreateClient(clientId);
        if (index >= 0) {
            clients[index].scoreA = red;
            clients[index].scoreB = blue;

            Serial.print("Score from ");
            Serial.print(clientId);
            Serial.print(": ");
            Serial.print(red);
            Serial.print(" - ");
            Serial.println(blue);
        }

        int judgeIdx = findJudgeIndexById(clientId);
        if (judgeIdx >= 0) {
            // 记录当前轮该裁判上报
            roundScoreA[judgeIdx] = red;
            roundScoreB[judgeIdx] = blue;
            roundSubmitted[judgeIdx] = true;

            // 三位裁判都上报后，按规则计算本轮得分并累计到总比分
            if (isRoundComplete()) {
                int roundFinalA = calculateFinalScore(roundScoreA[0], roundScoreA[1], roundScoreA[2]);
                int roundFinalB = calculateFinalScore(roundScoreB[0], roundScoreB[1], roundScoreB[2]);

                totalScoreA += roundFinalA;
                totalScoreB += roundFinalB;

                Serial.println("=== Round Completed ===");
                Serial.print("Round judge scores A: ");
                Serial.print(roundScoreA[0]); Serial.print(", "); Serial.print(roundScoreA[1]); Serial.print(", "); Serial.println(roundScoreA[2]);
                Serial.print("Round judge scores B: ");
                Serial.print(roundScoreB[0]); Serial.print(", "); Serial.print(roundScoreB[1]); Serial.print(", "); Serial.println(roundScoreB[2]);
                Serial.print("Round final: ");
                Serial.print(roundFinalA); Serial.print(" : "); Serial.println(roundFinalB);
                Serial.print("Total score: ");
                Serial.print(totalScoreA); Serial.print(" : "); Serial.println(totalScoreB);

                // 清空本轮，准备接收下一轮
                resetRound();
            }
        }
    }

    String json = "{\"ok\":true}";
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);

    // 挂载 LittleFS 文件系统
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
    }

    // 初始化客户端数组
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = false;
    }

    // 初始化轮次数据
    resetRound();

    // 开热点
    WiFi.softAP(ssid, password);

    Serial.println("WiFi started");
    Serial.println(WiFi.softAPIP());

    // 路由
    server.on("/", handleRoot);
    server.on("/score", handleScore);
    server.on("/setNames", HTTP_POST, handleSetNames);
    server.on("/resetTotal", HTTP_POST, handleResetTotal);
    server.on("/updateScore", handleUpdateScore);

    server.begin();
}

void loop() {
    server.handleClient();
}
