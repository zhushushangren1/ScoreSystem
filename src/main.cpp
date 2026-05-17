#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

const char* ssid = "ScoreSystem";
const char* password = "12345678";

WebServer server(80);

#define MAX_CLIENTS 5
#define JUDGE_COUNT 3

struct ClientScore {
    String clientId;
    int scoreA;
    int scoreB;
    bool active;
};

ClientScore clients[MAX_CLIENTS];

const char* JUDGE_IDS[JUDGE_COUNT] = {"client1", "client2", "client3"};

int roundScoreA[JUDGE_COUNT] = {0, 0, 0};
int roundScoreB[JUDGE_COUNT] = {0, 0, 0};
bool roundSubmitted[JUDGE_COUNT] = {false, false, false};

int totalScoreA = 0;
int totalScoreB = 0;

bool roundOpen = true;
unsigned long roundId = 1;

String teamAName = "Player A";
String teamBName = "Player B";

unsigned long countdownEndMs = 0;
bool countdownActive = false;
int countdownDuration = 0;

String jsonEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c == '\\' || c == '"') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

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
    for (int i = 0; i < JUDGE_COUNT; i++) {
        if (clientId == JUDGE_IDS[i]) {
            return i;
        }
    }
    return -1;
}

bool isRoundComplete() {
    return roundSubmitted[0] && roundSubmitted[1] && roundSubmitted[2];
}

int calculateFinalScore(int s1, int s2, int s3) {
    if (s1 == s2 || s1 == s3) return s1;
    if (s2 == s3) return s2;

    int minScore = s1;
    if (s2 < minScore) minScore = s2;
    if (s3 < minScore) minScore = s3;
    return minScore;
}

void resetRound() {
    for (int i = 0; i < JUDGE_COUNT; i++) {
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

void openNextRound() {
    resetRound();
    resetClientScores();
    roundOpen = true;
    roundId++;
}

long getCountdownRemainingMs() {
    if (!countdownActive) return 0;
    long remaining = (long)(countdownEndMs - millis());
    if (remaining <= 0) {
        countdownActive = false;
        return 0;
    }
    return remaining;
}

void startCountdown(int seconds) {
    countdownDuration = seconds;
    countdownEndMs = millis() + (unsigned long)seconds * 1000UL;
    countdownActive = true;
}

void stopCountdown() {
    countdownActive = false;
    countdownEndMs = 0;
    countdownDuration = 0;
}

String getHTML(const char* path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.print("Failed to open ");
        Serial.println(path);
        return String("<html><body>page not found. Run: pio run -t uploadfs</body></html>");
    }

    String html = file.readString();
    file.close();
    html.replace("%TEAM_A%", teamAName);
    html.replace("%TEAM_B%", teamBName);
    return html;
}

void handleRoot() {
    server.send(200, "text/html", getHTML("/index.html"));
}

void handleControlPage() {
    server.send(200, "text/html", getHTML("/control.html"));
}

void handleScore() {
    String judgesJson;
    for (int i = 0; i < JUDGE_COUNT; i++) {
        if (i > 0) judgesJson += ",";
        judgesJson += "{\"id\":\"" + String(JUDGE_IDS[i]) +
                      "\",\"scoreA\":" + String(roundScoreA[i]) +
                      ",\"scoreB\":" + String(roundScoreB[i]) +
                      ",\"submitted\":" + String(roundSubmitted[i] ? "true" : "false") + "}";
    }

    long remainingMs = getCountdownRemainingMs();
    String json = "{\"teamA\":\"" + jsonEscape(teamAName) +
                  "\",\"teamB\":\"" + jsonEscape(teamBName) +
                  "\",\"totalA\":" + String(totalScoreA) +
                  ",\"totalB\":" + String(totalScoreB) +
                  ",\"roundOpen\":" + String(roundOpen ? "true" : "false") +
                  ",\"roundId\":" + String(roundId) +
                  ",\"countdownActive\":" + String(countdownActive ? "true" : "false") +
                  ",\"countdownRemainingMs\":" + String(remainingMs) +
                  ",\"countdownDuration\":" + String(countdownDuration) +
                  ",\"judges\":[" + judgesJson + "]}";
    server.send(200, "application/json", json);
}

void handleRoundStatus() {
    bool clientSubmitted = false;
    if (server.hasArg("client")) {
        int judgeIdx = findJudgeIndexById(server.arg("client"));
        if (judgeIdx >= 0) {
            clientSubmitted = roundSubmitted[judgeIdx];
        }
    }

    String json = "{\"roundOpen\":" + String(roundOpen ? "true" : "false") +
                  ",\"roundId\":" + String(roundId) +
                  ",\"submitted\":" + String(clientSubmitted ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}

void handleSetNames() {
    if (server.hasArg("a")) {
        teamAName = server.arg("a");
    }
    if (server.hasArg("b")) {
        teamBName = server.arg("b");
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleSyncDisplay() {
    if (server.hasArg("a")) {
        teamAName = server.arg("a");
    }
    if (server.hasArg("b")) {
        teamBName = server.arg("b");
    }
    if (server.hasArg("totalA")) {
        totalScoreA = server.arg("totalA").toInt();
    }
    if (server.hasArg("totalB")) {
        totalScoreB = server.arg("totalB").toInt();
    }

    String json = "{\"ok\":true,\"teamA\":\"" + jsonEscape(teamAName) +
                  "\",\"teamB\":\"" + jsonEscape(teamBName) +
                  "\",\"totalA\":" + String(totalScoreA) +
                  ",\"totalB\":" + String(totalScoreB) + "}";
    server.send(200, "application/json", json);
}

void handleResetTotal() {
    totalScoreA = 0;
    totalScoreB = 0;
    resetRound();
    resetClientScores();
    roundOpen = true;
    roundId++;
    stopCountdown();

    Serial.println("Total score and judge scores reset from web button");
    String json = "{\"ok\":true,\"roundId\":" + String(roundId) + "}";
    server.send(200, "application/json", json);
}

void handleNextRound() {
    openNextRound();

    int seconds = 0;
    if (server.hasArg("seconds")) {
        seconds = server.arg("seconds").toInt();
    }
    if (seconds > 0) {
        startCountdown(seconds);
        Serial.print("Countdown started: ");
        Serial.print(seconds);
        Serial.println("s");
    } else {
        stopCountdown();
    }

    Serial.print("Next round opened, roundId=");
    Serial.println(roundId);

    String json = "{\"ok\":true,\"roundId\":" + String(roundId) +
                  ",\"countdownActive\":" + String(countdownActive ? "true" : "false") +
                  ",\"countdownDuration\":" + String(countdownDuration) + "}";
    server.send(200, "application/json", json);
}

void handleUpdateScore() {
    if (!(server.hasArg("client") && server.hasArg("red") && server.hasArg("blue"))) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_args\"}");
        return;
    }

    const String clientId = server.arg("client");
    const int red = server.arg("red").toInt();
    const int blue = server.arg("blue").toInt();

    const int judgeIdx = findJudgeIndexById(clientId);
    if (judgeIdx < 0) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown_client\"}");
        return;
    }

    if (!roundOpen) {
        server.send(409, "application/json", "{\"ok\":false,\"error\":\"round_closed\"}");
        return;
    }

    if (roundSubmitted[judgeIdx]) {
        server.send(409, "application/json", "{\"ok\":false,\"error\":\"already_submitted\"}");
        return;
    }

    const int index = findOrCreateClient(clientId);
    if (index >= 0) {
        clients[index].scoreA = red;
        clients[index].scoreB = blue;
    }

    roundScoreA[judgeIdx] = red;
    roundScoreB[judgeIdx] = blue;
    roundSubmitted[judgeIdx] = true;

    Serial.print("Score from ");
    Serial.print(clientId);
    Serial.print(": ");
    Serial.print(red);
    Serial.print(" - ");
    Serial.println(blue);

    if (isRoundComplete()) {
        const int roundFinalA = calculateFinalScore(roundScoreA[0], roundScoreA[1], roundScoreA[2]);
        const int roundFinalB = calculateFinalScore(roundScoreB[0], roundScoreB[1], roundScoreB[2]);

        totalScoreA += roundFinalA;
        totalScoreB += roundFinalB;
        roundOpen = false;

        Serial.println("=== Round Completed ===");
        Serial.print("Round judge scores A: ");
        Serial.print(roundScoreA[0]); Serial.print(", "); Serial.print(roundScoreA[1]); Serial.print(", "); Serial.println(roundScoreA[2]);
        Serial.print("Round judge scores B: ");
        Serial.print(roundScoreB[0]); Serial.print(", "); Serial.print(roundScoreB[1]); Serial.print(", "); Serial.println(roundScoreB[2]);
        Serial.print("Round final: ");
        Serial.print(roundFinalA); Serial.print(" : "); Serial.println(roundFinalB);
        Serial.print("Total score: ");
        Serial.print(totalScoreA); Serial.print(" : "); Serial.println(totalScoreB);
        Serial.println("Round locked. Waiting for next round.");
    }

    String json = "{\"ok\":true,\"roundOpen\":" + String(roundOpen ? "true" : "false") +
                  ",\"roundId\":" + String(roundId) + "}";
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = false;
    }

    resetRound();

    WiFi.mode(WIFI_AP);
    // WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.softAP(ssid, password);

    Serial.println("WiFi started");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/control", handleControlPage);
    server.on("/score", handleScore);
    server.on("/roundStatus", handleRoundStatus);
    server.on("/setNames", HTTP_POST, handleSetNames);
    server.on("/syncDisplay", HTTP_POST, handleSyncDisplay);
    server.on("/nextRound", HTTP_POST, handleNextRound);
    server.on("/resetTotal", HTTP_POST, handleResetTotal);
    server.on("/updateScore", handleUpdateScore);

    server.begin();
}

void loop() {
    server.handleClient();
}
