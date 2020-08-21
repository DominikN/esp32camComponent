//based on https://github.com/yoursunny/esp32cam
#include <esp32cam.h>

#define USE_WIFI_NINA         false

#include <WiFiWebServer.h>
#include <WiFi.h>

#include <Husarnet.h>

#if __has_include("credentials.h")
#include "credentials.h"
#else
// WiFi credentials
const char* ssid     = "my-ssid";
const char* password = "my-pass";

// Husarnet credentials
const char* hostName = "esp32cam";
const char* husarnetJoinCode = "fc94:1b75:99f:aa78:b937:xxxx:xxxx:xxxx/xxxxxxxxxxxxxxxxxxxxxx";
const char* dashboardURL = "app.husarnet.com";
#endif

const char* streamUsername = "esp32";
const char* streamPassword = "pass32";
const char* streamRealm = "ESP32-CAM, please log in!";
const char* authFailResponse = "Sorry, login failed!";

const char* streamPath = "/stream";

static auto hiRes = esp32cam::Resolution::find(320, 240);
///static auto hiRes = esp32cam::Resolution::find(500, 425);

const uint8_t jpgqal = 80;
const uint8_t fps = 10;    //sets minimum delay between frames, HW limits of ESP32 allows about 12fps @ 800x600

WiFiWebServer server(8000);

void handleMjpeg()
{
  static bool clientConnected = 0;

  if (clientConnected == 0) {
    clientConnected = 1;
    if (!server.authenticate(streamUsername, streamPassword)) {
      Serial.println(F("STREAM auth required, sending request"));
      clientConnected = 0;
//      return server.requestAuthentication(/BASIC_AUTH, streamRealm, authFailResponse);
      return server.requestAuthentication();
    }

    if (!esp32cam::Camera.changeResolution(hiRes, 1000)) {
      Serial.println(F("SET RESOLUTION FAILED"));
    }

    struct esp32cam::CameraClass::StreamMjpegConfig mjcfg;
    mjcfg.frameTimeout = 10000;
    mjcfg.minInterval = 1000 / fps;
    mjcfg.maxFrames = -1;
    Serial.println(String (F("STREAM BEGIN @ ")) + fps + F("fps (minInterval ") + mjcfg.minInterval + F("ms)") );
    HusarnetClient client = server.client();
    auto startTime = millis();
    int res = esp32cam::Camera.streamMjpeg(client, mjcfg);
    if (res <= 0) {
      Serial.printf("STREAM ERROR %d\n", res);
      clientConnected = 0;
      return;
    }
    auto duration = millis() - startTime;
    Serial.printf("STREAM END %dfrm %0.2ffps\n", res, 1000.0 * res / duration);
  } else {
    Serial.printf("Only one client can be connected to MJPG stream\r\n");
  }
  clientConnected = 0;
}

void taskServer( void * parameter );

void setup()
{
  Serial.begin(115200);
  Serial.println();

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(jpgqal);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? F("CAMERA OK") : F("CAMERA FAIL"));
  }

  Serial.println(String(F("JPEG quality: ")) + jpgqal);
  Serial.println(String(F("Framerate: ")) + fps);

  Serial.print(F("Connecting to WiFi"));
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("."));
    delay(500);
  }

  Serial.print(F("\nCONNECTED!\nhttp://"));
  Serial.print(WiFi.localIP());
  Serial.println(streamPath);

  /* Start Husarnet */
  Husarnet.selfHostedSetup(dashboardURL);
  Husarnet.join(husarnetJoinCode, hostName);
  Husarnet.start();

  server.on(streamPath, handleMjpeg);

  server.begin();


  xTaskCreatePinnedToCore(
    taskServer,          /* Task function. */
    "taskServer",        /* String with name of task. */
    20000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    2,                /* Priority of the task. */
    NULL,             /* Task handle. */
    0);               /* Core where the task should run */
}

void taskServer( void * parameter ) {
  while (1) {
    server.handleClient();
    delay(10);
  }

}

void loop()
{
  Serial.printf("[RAM: %d]\r\n", esp_get_free_heap_size());
  delay(5000);
}
