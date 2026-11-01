#include <WiFi.h>
#include <esp_now.h>

struct Message {
  uint32_t magic;     // tag so we only count boards running this sketch
  uint32_t counter;
};

const uint32_t MAGIC = 0xB10C2026;
const int MAX_BOARDS = 40;

uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t heard[MAX_BOARDS][6];
volatile int heardCount = 0;
int printedCount = 0;
uint32_t counter = 0;

String macToString(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void handleMessage(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(Message)) return;
  Message m;
  memcpy(&m, data, sizeof(m));
  if (m.magic != MAGIC) return;

  for (int i = 0; i < heardCount; i++) {
    if (memcmp(heard[i], mac, 6) == 0) return;   // already known
  }
  if (heardCount < MAX_BOARDS) {
    memcpy(heard[heardCount], mac, 6);
    heardCount++;
  }
}

// The callback signature changed in ESP32 Arduino core 3.x
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  handleMessage(info->src_addr, data, len);
}
#else
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  handleMessage(mac, data, len);
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);     // needed by ESP-NOW; does NOT connect to any Wi-Fi
  WiFi.disconnect();

  Serial.println();
  Serial.print("My board's MAC address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed. Restarting...");
    delay(1000);
    ESP.restart();
  }

  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = 0;        // use the current channel
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
  }
}

void loop() {
  static unsigned long lastSend = 0;
  static unsigned long lastSummary = 0;

  // Announce myself once per second
  if (millis() - lastSend >= 1000) {
    lastSend = millis();
    Message m = {MAGIC, counter++};
    esp_now_send(broadcastAddr, (uint8_t *)&m, sizeof(m));
  }

  // Print each new board as soon as it is heard
  while (printedCount < heardCount) {
    Serial.print("Heard new board: ");
    Serial.println(macToString(heard[printedCount]));
    printedCount++;
  }

  // Full summary every 5 seconds (good for the screenshot)
  if (millis() - lastSummary >= 5000) {
    lastSummary = millis();
    Serial.println("------------------------------");
    Serial.print("My MAC       : ");
    Serial.println(WiFi.macAddress());
    Serial.print("Boards heard : ");
    Serial.println(heardCount);
    for (int i = 0; i < heardCount; i++) {
      Serial.print("  ");
      Serial.println(macToString(heard[i]));
    }
    Serial.println("------------------------------");
  }
}