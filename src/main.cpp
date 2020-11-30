#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#include <config.h>

const int Pin_D1 = 5;
const int Pin_D2 = 4;
const int Pin_D3 = 0;
const int Pin_D4 = 2;
const int Pin_D5 = 14;
const int Pin_D6 = 12;
const int Pin_D7 = 13;
const int Pin_D8 = 15;

const int Pin_RX = 3; // Can out cannot in
const int Pin_TX = 1; // Can in cannot out, fails to boot if low

// // Handle inverted relays
// const bool On = false;
// const bool Off = true;
// Normal LEDs
const bool On = true;
const bool Off = false;

const int tally1_prog = Pin_D3;
const int tally1_prev = Pin_D4;
const int tally2_prog = Pin_D5;
const int tally2_prev = Pin_D6;
const int tally3_prog = Pin_D7;
const int tally3_prev = Pin_TX;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, Pin_D4, NEO_GRBW);

const IPAddress apIP(192, 168, 1, 1);
const char *apSSID = "TALLY_SETUP";

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

boolean settingMode;
String ssidList;
boolean hasDisplay = true;

DNSServer dnsServer;
ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

boolean checkConnection();
void startWebServer();
void setupMode();
String makePage(String title, String contents);
String urlDecode(String input);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void setLight(int lightName, bool isOn);

const int capacity = 1024;
StaticJsonDocument<capacity> doc;

void setup()
{
  Serial.begin(115200);
  bool isWipeSettings = initSettings(true);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    hasDisplay = false;
  }
  if (hasDisplay)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(20, 20);
    // Display static text
    // display.println("Hello, world!");
    // display.display();

    // display.display();
    // // Clear the buffer
    // display.clearDisplay();
  }

  if (restoreConfig())
  {
    // Init strip lights if that is requested
    if (isStripLights)
    {
      // strip
      strip.begin();
      strip.setBrightness(10);

      strip.setPixelColor(0, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(1, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(2, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(3, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(4, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(5, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(6, strip.Color(0, 0, 0, 0));
      strip.setPixelColor(7, strip.Color(0, 0, 0, 0));

      strip.show(); // Initialize all pixels to 'off'
    }
    else
    {
      // Init the pins for LED or relay on/off
      pinMode(tally1_prog, OUTPUT);
      pinMode(tally1_prev, OUTPUT);
      pinMode(tally2_prog, OUTPUT);
      pinMode(tally2_prev, OUTPUT);
      pinMode(tally3_prog, OUTPUT);
      pinMode(tally3_prev, OUTPUT);

      digitalWrite(tally1_prog, isInvertedRelay ? true : false);
      digitalWrite(tally1_prev, isInvertedRelay ? true : false);
      digitalWrite(tally2_prog, isInvertedRelay ? true : false);
      digitalWrite(tally2_prev, isInvertedRelay ? true : false);
      digitalWrite(tally3_prog, isInvertedRelay ? true : false);
      digitalWrite(tally3_prev, isInvertedRelay ? true : false);
    }

    connectToWifi();

    if (checkConnection())
    {
      settingMode = false;
      startWebServer();

      if (hasDisplay)
      {
        display.setCursor(0, 0);
        display.printf("%u: %s-%s-%s", isStripLights, tally1Name.c_str(), tally2Name.c_str(), tally3Name.c_str());
        display.setCursor(0, 16);
        display.println(WiFi.localIP());
        display.display();
      }
      return;
    }
  }

  if (hasDisplay)
  {
    if (isWipeSettings)
    {
      display.println("Was reset");
    }
    display.println("Setting mode...");
    display.display();
  }

  settingMode = true;
  setupMode();
}

void loop()
{
  if (settingMode)
  {
    dnsServer.processNextRequest();
  }
  webServer.handleClient();
  webSocket.loop();

  // digitalWrite(Pin_D1, !digitalRead(Pin_D1));
  // delay(500);
}

boolean checkConnection()
{
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  while (count < 30)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println();
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

void startWebServer()
{
  if (settingMode)
  {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><br>";
      s += "Tally 1: <input name=\"tally1\" length=10><br>";
      s += "Tally 2: <input name=\"tally2\" length=10><br>";
      s += "Tally 3: <input name=\"tally3\" length=10><br>";
      s += "Tally 4: <input name=\"tally4\" length=10><br>";
      s += "Use Strip Lights: <input name=\"useStrip\" type=\"checkbox\"><br>";
      s += "Use Inverted Relay: <input name=\"useInvert\" type=\"checkbox\"><br>";
      s += "<input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    webServer.on("/setap", []() {
      for (int i = 0; i < 512; ++i)
      {
        EEPROM.write(i, 0);
      }

      // Read the form
      String ssid = urlDecode(webServer.arg("ssid"));
      Serial.print("SSID: ");
      Serial.println(ssid);
      String pass = urlDecode(webServer.arg("pass"));
      Serial.print("Password: ");
      Serial.println(pass);

      String tally1 = urlDecode(webServer.arg("tally1"));
      String tally2 = urlDecode(webServer.arg("tally2"));
      String tally3 = urlDecode(webServer.arg("tally3"));
      String tally4 = urlDecode(webServer.arg("tally4"));
      bool stripActive = webServer.hasArg("useStrip");
      bool invertActive = webServer.hasArg("useInvert");

      Serial.println("Writing SSID to EEPROM...");
      for (unsigned int i = 0; i < ssid.length(); ++i)
      {
        EEPROM.write(i, ssid[i]);
      }
      Serial.println("Writing Password to EEPROM...");
      for (unsigned int i = 0; i < pass.length(); ++i)
      {
        EEPROM.write(32 + i, pass[i]);
      }

      // Write the tally names
      for (unsigned int i = 0; i < tally1.length(); ++i)
      {
        // SSID + Pass == 32 + 64 = 96
        EEPROM.write(96 + i, tally1[i]);
      }
      for (unsigned int i = 0; i < tally2.length(); ++i)
      {
        // SSID + Pass == 32 + 64 = 96
        EEPROM.write(96 + 10 + i, tally2[i]);
      }
      for (unsigned int i = 0; i < tally3.length(); ++i)
      {
        // SSID + Pass == 32 + 64 = 96
        EEPROM.write(96 + 20 + i, tally3[i]);
      }
      for (unsigned int i = 0; i < tally4.length(); ++i)
      {
        // SSID + Pass == 32 + 64 = 96
        EEPROM.write(96 + 30 + i, tally4[i]);
      }

      // Write the flags
      byte flags = 0;
      if (stripActive)
        flags |= 1;
      if (invertActive)
        flags |= 2;
      EEPROM.write(96 + 40, flags);

      EEPROM.commit();
      Serial.println("Write EEPROM done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else
  {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    webServer.on("/", []() {
      String s = "<h1>STA mode</h1><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("STA mode", s));
    });
    webServer.on("/reset", []() {
      for (int i = 0; i < 512; ++i)
      {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
    });
  }
  webServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void setupMode()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  for (int i = 0; i < n; ++i)
  {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  dnsServer.start(53, "*", apIP);
  startWebServer();
  Serial.print("Starting Access Point at \"");
  Serial.print(apSSID);
  Serial.println("\"");
}

String makePage(String title, String contents)
{
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

String urlDecode(String input)
{
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\n", num);

    setLight(tally1_prev, false);
    setLight(tally1_prog, false);
    setLight(tally2_prev, false);
    setLight(tally2_prog, false);
    setLight(tally3_prev, false);
    setLight(tally3_prog, false);

    if (hasDisplay)
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.printf("T: %s-%s-%s", tally1Name.c_str(), tally2Name.c_str(), tally3Name.c_str());
      display.setCursor(0, 16);
      display.println(WiFi.localIP());
      display.println("Disconnected");
      display.display();
    }

    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

    setLight(tally1_prev, false);
    setLight(tally1_prog, false);
    setLight(tally2_prev, false);
    setLight(tally2_prog, false);
    setLight(tally3_prev, false);
    setLight(tally3_prog, false);

    if (hasDisplay)
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.printf("T: %s-%s-%s", tally1Name.c_str(), tally2Name.c_str(), tally3Name.c_str());
      display.setCursor(0, 16);
      display.println(WiFi.localIP());
      display.println("Connected!");
      display.display();
    }

    // send message to client
    webSocket.sendTXT(num, "Connected");
  }
  break;
  case WStype_TEXT:
  {
    if (payload[0] == 80)
    {
      // Normal payload is JSON which is impossible to be any normal character
      // So the character "P" is used to trigger the ping/pong to prevent reconnection
      webSocket.sendTXT(num, "A");
    }
    else
    {
      // A normal payload message

      Serial.printf("[%u] get Text: %s\n", num, payload);
      DeserializationError err = deserializeJson(doc, payload);
      if (err)
      {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return;
      }

      auto cam1_prev = doc[tally1Name + "-preview"].as<int>();
      auto cam1_prog = doc[tally1Name + "-program"].as<int>();
      auto cam2_prev = doc[tally2Name + "-preview"].as<int>();
      auto cam2_prog = doc[tally2Name + "-program"].as<int>();
      auto cam3_prev = doc[tally3Name + "-preview"].as<int>();
      auto cam3_prog = doc[tally3Name + "-program"].as<int>();

      if (hasDisplay)
      {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("T: %s-%s-%s", tally1Name.c_str(), tally2Name.c_str(), tally3Name.c_str());
        display.setCursor(0, 16);
        display.println(WiFi.localIP());
        display.printf("T1 - Prog: %u Prev %u\n", cam1_prog, cam1_prev);
        display.printf("T2 - Prog: %u Prev %u\n", cam2_prog, cam2_prev);
        display.printf("T3 - Prog: %u Prev %u\n", cam3_prog, cam3_prev);

        display.display();
      }

      // Serial.printf("[%u] Camera 1 Preview %d", num, cam1_prev);
      // Serial.printf(" Camera 1 Program %d", num, cam1_prog);
      // Serial.printf(" Camera 2 Preview %d", num, cam2_prev);
      // Serial.printf(" Camera 2 Program %d", num, cam2_prog);
      // Serial.println();
      setLight(tally1_prev, cam1_prev == 1);
      setLight(tally1_prog, cam1_prog == 1);
      setLight(tally2_prev, cam2_prev == 1);
      setLight(tally2_prog, cam2_prog == 1);
      setLight(tally3_prev, cam3_prev == 1);
      setLight(tally3_prog, cam3_prog == 1);

      if (isStripLights)
        strip.show();

      // send message to client
      // webSocket.sendTXT(num, "message here");

      // send data to all connected clients
      // webSocket.broadcastTXT("message here");
    }
  }
  break;
  case WStype_BIN:
    Serial.printf("[%u] get binary length: %u\n", num, length);
    hexdump(payload, length);

    // send message to client
    // webSocket.sendBIN(num, payload, length);
    break;
  }
}

void setLight(int lightName, bool isOn)
{
  if (isStripLights)
  {
    switch (lightName)
    {
    case tally1_prog:
      strip.setPixelColor(0, isOn ? strip.Color(255, 0, 0, 0) : strip.Color(0, 0, 0, 0));
      break;
    case tally1_prev:
      strip.setPixelColor(1, isOn ? strip.Color(0, 255, 0, 0) : strip.Color(0, 0, 0, 0));
      break;
    case tally2_prog:
      strip.setPixelColor(2, isOn ? strip.Color(255, 0, 0, 0) : strip.Color(0, 0, 0, 0));
      break;
    case tally2_prev:
      strip.setPixelColor(3, isOn ? strip.Color(0, 255, 0, 0) : strip.Color(0, 0, 0, 0));
      break;
    case tally3_prog:
      strip.setPixelColor(4, isOn ? strip.Color(255, 0, 0, 0) : strip.Color(0, 0, 0, 0));
      break;
    case tally3_prev:
      strip.setPixelColor(5, isOn ? strip.Color(0, 255, 0, 0) : strip.Color(0, 0, 0, 0));
      break;
    default:
      break;
    }
  }
  else
  {
    // Invert the value if required
    if (isInvertedRelay)
      isOn = isOn ? false : true;
    // Write to the LED or Relay
    digitalWrite(lightName, isOn ? true : false);
  }
}
