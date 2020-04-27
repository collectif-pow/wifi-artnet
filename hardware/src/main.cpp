#include <ArduinoJson.h>
#include <Artnet.h>
#include <AsyncJson.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <NeoPixelBus.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <Wifi.h>

#define DEBUG 0
#define STATS 0
#define MAX_ATTEMPTS 20

Preferences preferences;
String ssid;
String password;
String nodeName;
int pixelSize;
int pixelCount;
int totalChannels;
int startUniverse;
StaticJsonDocument<256> settings;

AsyncWebServer server(80);
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
Artnet artnet;
int previousDataLength = 0;

#define LED_PIN 9
NeoPixelBus<NeoGrbwFeature, NeoEsp32I2s1800KbpsMethod>* rgbw = NULL;
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1800KbpsMethod>* rgb = NULL;
RgbwColor* rgbwColor = NULL;
RgbColor* rgbColor = NULL;

#if STATS
long lastSync;
#endif

// functions definitions
void readNVS();
void startWifi();
void startHotspot();
void startArtnetDMX();
void startServer();
void startPixels();
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);
void onSync(IPAddress remoteIP);

// core to run Show()
#define SHOW_CORE 0
// task handles for use in the notifications
static TaskHandle_t showTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

void show() {
  if (userTaskHandle == 0) {
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS(200);
    // store the handle of the current task, so that the show task can notify it when it's done
    userTaskHandle = xTaskGetCurrentTaskHandle();
    // trigger the show task
    xTaskNotifyGive(showTaskHandle);
    // wait to be notified that it's done
    ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    userTaskHandle = 0;
  }
}

void showTask(void* pvParameters) {
  // run forever...
  for (;;) {
    // wait for the trigger
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // do the show (synchronously)
    if (pixelSize == 4)
      rgbw->Show();
    else
      rgb->Show();
    // notify the calling task
    xTaskNotifyGive(userTaskHandle);
  }
}

void setup() {
#if DEBUG || STATS
  Serial.begin(115200);
#endif
#if DEBUG
  delay(3000);
#endif
  readNVS();
  startWifi();
  if (WiFi.getMode() == WIFI_STA) {
    xTaskCreatePinnedToCore(showTask, "showTask", 3000, NULL, 2 | portPRIVILEGE_BIT, &showTaskHandle,
                            SHOW_CORE);
    startPixels();
    startArtnetDMX();
  }
  startServer();
}

void loop() {
  if (WiFi.getMode() == WIFI_STA) {
    artnet.read();
  } else {
    dnsServer.processNextRequest();
  }
}

void readNVS() {
  preferences.begin("wifi-node", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  nodeName = preferences.getString("nodeName", "wifi-node");
  pixelSize = preferences.getInt("pixelSize", 4);
  pixelCount = preferences.getInt("pixelCount", 60);
  startUniverse = preferences.getInt("startUniverse", 0);
  totalChannels = pixelSize * pixelCount;
#if DEBUG
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(nodeName);
  Serial.println(pixelSize);
  Serial.println(pixelCount);
  Serial.println(startUniverse);
  Serial.println(totalChannels);
#endif
}

void wifiReset(wifi_mode_t mode) {
  WiFi.mode(mode);
  delay(2000);
}

void startWifi() {
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  if (ssid != "") {
    uint8_t attempts = 0;
    wifiReset(WIFI_STA);
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
#if DEBUG
      Serial.print("Trying to connect to: ");
      Serial.println(ssid);
#endif
      wifiReset(WIFI_STA);
      if (password != "") {
        WiFi.begin(ssid.c_str(), password.c_str());
      } else {
        WiFi.begin(ssid.c_str());
      }
      attempts++;
    }
    if (attempts >= MAX_ATTEMPTS) {
      startHotspot();
    } else {
#if DEBUG
      Serial.println("Connected to WiFi");
#endif
    }
  } else {
    startHotspot();
  }
}

void startHotspot() {
#if DEBUG
  Serial.println("Trying to setup WiFi AP...");
#endif
  wifiReset(WIFI_AP);
  WiFi.softAP(nodeName.c_str());
  delay(2000);  // see: https://github.com/espressif/arduino-esp32/issues/2025
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(53, "*", apIP);
#if DEBUG
  Serial.println("WiFi AP OK");
#endif
}

void startServer() {
  SPIFFS.begin();
  // TODO: additional headers and 404 handler ?
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                       "GET, POST, PATCH, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Origin, Content-Type, X-Auth-Token");
  server.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });
  // end of TODO
  server.on("/", HTTP_GET,
            [](AsyncWebServerRequest* request) { request->send(SPIFFS, "/index.html", "text/html"); });
  server.on("/favicon-16x16.png", HTTP_GET,
            [](AsyncWebServerRequest* request) { request->send(SPIFFS, "/favicon-16x16.png", "image/png"); });
  server.on("/fonts/MaterialIcons.woff2", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/fonts/MaterialIcons.woff2", "font/woff2");
  });
  server.on("/js/app.js", HTTP_GET,
            [](AsyncWebServerRequest* request) { request->send(SPIFFS, "/js/app.js", "text/javascript"); });
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
    settings.clear();
    settings["ssid"] = ssid;
    settings["password"] = password;
    settings["nodeName"] = nodeName;
    settings["pixelSize"] = pixelSize;
    settings["pixelCount"] = pixelCount;
    settings["startUniverse"] = startUniverse;
    String res;
    serializeJson(settings, res);
    request->send(200, "application/json", res);
  });
  AsyncCallbackJsonWebHandler* handler =
      new AsyncCallbackJsonWebHandler("/update", [](AsyncWebServerRequest* request, JsonVariant& json) {
        JsonObject jsonObj = json.as<JsonObject>();
        preferences.putString("ssid", jsonObj["ssid"].as<String>());
        preferences.putString("password", jsonObj["password"].as<String>());
        preferences.putString("nodeName", jsonObj["nodeName"].as<String>());
        preferences.putInt("pixelSize", jsonObj["pixelSize"].as<int>());
        preferences.putInt("pixelCount", jsonObj["pixelCount"].as<int>());
        preferences.putInt("startUniverse", jsonObj["startUniverse"].as<int>());
        request->send(201, "application/json", "{\"status\":\"ok\"}");
        delay(1000);
        ESP.restart();
      });
  server.addHandler(handler);
  server.begin();
}

void startArtnetDMX() {
  delay(5000);
  IPAddress addr = WiFi.localIP();
#if DEBUG
  for (uint8_t i = 0; i < 4; i++) {
    if (i != 3) {
      Serial.print(addr[i]);
      Serial.print(".");
    } else {
      Serial.println(addr[i]);
    }
  }
#endif
  artnet.setBroadcast({addr[0], addr[1], addr[2], 255});
  artnet.setArtDmxCallback(onDmxFrame);
  artnet.setArtSyncCallback(onSync);
  artnet.setName(nodeName);
  artnet.begin();
}

void startPixels() {
  if (pixelSize == 4) {
    delete rgb;
    delete rgbColor;
    if (rgbw != NULL) delete rgbw;
    rgbw = new NeoPixelBus<NeoGrbwFeature, NeoEsp32I2s1800KbpsMethod>(pixelCount, LED_PIN);
    rgbw->Begin();
    rgbwColor = new RgbwColor(0);
    show();
  } else {
    delete rgbw;
    delete rgbwColor;
    if (rgb != NULL) delete rgb;
    rgb = new NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1800KbpsMethod>(pixelCount, LED_PIN);
    rgb->Begin();
    rgbColor = new RgbColor(0);
    show();
  }
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP) {
  for (int i = 0; i < length / pixelSize; i++) {
#if DEBUG
    if (data[i * pixelSize] > 128)
      Serial.print('#');
    else
      Serial.print('.');
#endif
    int led = i + (universe - startUniverse) * (previousDataLength / pixelSize);
    if (led < pixelCount) {
      if (pixelSize == 4) {
        rgbwColor->R = data[i * pixelSize];
        rgbwColor->G = data[i * pixelSize + 1];
        rgbwColor->B = data[i * pixelSize + 2];
        rgbwColor->W = data[i * pixelSize + 3];
        rgbw->SetPixelColor(i, *rgbwColor);
      } else {
        rgbColor->R = data[i * pixelSize];
        rgbColor->G = data[i * pixelSize + 1];
        rgbColor->B = data[i * pixelSize + 2];
        rgb->SetPixelColor(i, *rgbColor);
      }
    }
  }
#if DEBUG
  Serial.println();
#endif
  previousDataLength = length;
}

void onSync(IPAddress remoteIP) {
#if DEBUG || STATS
  long start = micros();
#endif
  show();
#if DEBUG || STATS
  Serial.print("Sync delta: ");
  Serial.println((start - lastSync) / 1000);
  lastSync = start;
#endif
}
