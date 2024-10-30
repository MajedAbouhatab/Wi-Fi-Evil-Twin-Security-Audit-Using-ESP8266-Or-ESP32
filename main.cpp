#if defined(ARDUINO_M5STACK_Core2)
#include <M5Unified.h>
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
#endif
#include "ESPAsyncWebServer.h"
#include <DNSServer.h>
#include <SD.h>

AsyncWebServer server(80);
DNSServer dnsServer;
File logFile;

char buf[18];
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Free Wi-Fi Agreement</title>
    <style>
        body {
            margin: 0;
            padding: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            background-color: #f4f4f4;
            font-family: Arial, sans-serif;
        }
        .container {
            text-align: center;
        }
        .agreement {
            background: white;
            border: 1px solid #ccc;
            border-radius: 8px;
            padding: 20px;
            max-width: 600px;
            margin: 0 auto;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        }
        .agreement h1 {
            font-size: 24px;
            margin-bottom: 20px;
        }
        .agreement p {
            font-size: 16px;
            margin-bottom: 20px;
        }
        .agreement a {
            color: #008CBA;
            text-decoration: none;
        }
        .agreement button {
            background-color: #008CBA;
            border: none;
            color: white;
            padding: 15px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            margin: 4px 2px;
            cursor: pointer;
            border-radius: 4px;
        }
        video {
            width: 100vw;
            height: 100vh;
            object-fit: cover;
            display: none; /* Hide the video initially */
        }
    </style>
</head>
<body>
    <div class="container">
        <div id="agreement" class="agreement">
            <h1>Free Wi-Fi Access</h1>
            <p>You must agree to the <a href="#">Terms of Service</a> to access this free Wi-Fi.</p>
            <p>By clicking "Accept", you agree to abide by the terms stated.</p>
            <button id="acceptButton">Accept</button>
        </div>
        <video id="videoPlayer" controls>
            <source src="/video" type="video/mp4">
            Your browser does not support the video tag.
        </video>
    </div>
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            var acceptButton = document.getElementById('acceptButton');
            var agreement = document.getElementById('agreement');
            var video = document.getElementById('videoPlayer');

            acceptButton.addEventListener('click', function() {
                agreement.style.display = 'none'; // Hide the agreement page
                video.style.display = 'block'; // Show the video
                video.play(); // Start playing the video
            });
        });
    </script>
</body>
</html>
)rawliteral";

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    request->send_P(200, "text/html", index_html);
  }
};

void logMessage(String message)
{
  message += " (";
  message += millis() / 1000;
  message += ")";
#if defined(ARDUINO_M5STACK_Core2)
  if (M5.Display.getCursorY() > M5.Display.height() - 26)
  {
    M5.Display.scroll(0, -26);
    M5.Display.setCursor(M5.Display.getCursorX(), M5.Display.getCursorY() - 26);
  }
  M5.Display.println(message);
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
  int CharsPerLine = u8g2.getDisplayWidth() / u8g2.getMaxCharWidth();
  u8g2.clearDisplay();
  u8g2.firstPage();
  do
    for (int OLEDLine = 0; OLEDLine < u8g2.getDisplayHeight() / u8g2.getMaxCharHeight(); OLEDLine++)
      u8g2.drawStr(0, (OLEDLine + 1) * u8g2.getMaxCharHeight(), message.substring(OLEDLine * CharsPerLine, (OLEDLine + 1) * CharsPerLine).c_str());
  while (u8g2.nextPage());
#endif
  if (!logFile)
    return;
  logFile.println(message); // Log to SD card
  logFile.flush();          // Ensure the data is written
}

#if defined(ARDUINO_M5STACK_Core2)
WiFiEventFuncCb WiFiEvent = [](WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (event != ARDUINO_EVENT_WIFI_AP_STACONNECTED)
    return;
  snprintf(buf, sizeof(buf), MACSTR, MAC2STR(info.wifi_ap_staconnected.mac));
  logMessage(buf);
};
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
WiFiEventHandler stationConnectedHandler;
void onStationConnected(const WiFiEventSoftAPModeStationConnected &evt)
{
  snprintf(buf, sizeof(buf), MACSTR, MAC2STR(evt.mac));
  logMessage(buf);
}
#endif

void setup()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
#if defined(ARDUINO_M5STACK_Core2)
  M5.begin();
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_BLUE);
  SD.begin(4);
  logFile = SD.open("/log.txt", FILE_APPEND);
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
  pinMode(0, INPUT_PULLUP);
  pinMode(16, OUTPUT);
  SD.begin(SS);
  u8g2.begin();
  u8g2.setFont(u8g2_font_9x18_tf);           //(u8g2_font_inb16_mr);          // set the target font to calculate the pixel width
  u8g2.setFontMode(0);                       // enable transparent mode, which is faster
  logFile = SD.open("/log.txt", FILE_WRITE); // FILE_APPEND
  logFile.seek(EOF);
#endif
  logMessage("New session");
  String APName = "Free Wifi";
#if defined(ARDUINO_M5STACK_Core2)
  if (M5.BtnB.isPressed())
    for (int i = 0; i < WiFi.scanNetworks(); ++i)
    {
      APName = WiFi.SSID(i);
      logMessage(APName);
      M5.delay(2000);
      M5.update();
      if (!M5.BtnB.isPressed())
        break;
      else
        APName = "Free Wifi";
    }
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
  if (!digitalRead(0))
    for (int i = 0; i < WiFi.scanNetworks(); ++i)
    {
      APName = WiFi.SSID(i);
      logMessage(APName);
      delay(2000);
      if (digitalRead(0))
        break;
      else
        APName = "Free Wifi";
    }
#endif
  logMessage(APName);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(APName);
#if defined(ARDUINO_M5STACK_Core2)
  WiFi.onEvent(WiFiEvent);
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
#endif

  // Serve the video file with streaming
  server.on("/video", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              if (SD.exists("/your-video-file.mp4"))
              {
                File videoFile = SD.open("/your-video-file.mp4");
                if (videoFile)
                {
#if defined(ARDUINO_M5STACK_Core2)
                  M5.Speaker.tone(2500, 100);
                  request->send(SD, "/your-video-file.mp4", "video/mp4");
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
                  tone(16, 2000, 300);
                  request->send(SDFS, "/your-video-file.mp4", "video/mp4");
#endif
                  videoFile.close();
                }
                else
                  request->send(500, "text/plain", "Failed to open video file");
              } });
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // Only when requested from AP
  server.begin();
}

void loop()
{
  dnsServer.processNextRequest();
}