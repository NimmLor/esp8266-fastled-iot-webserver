/*
   ESP8266 FastLED WebServer: https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015-2018 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE
extern "C" {
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include <EEPROM.h>
#include "GradientPalettes.h"
#include "Field.h"
#include <ESP8266mDNS.h>                    //Enable mDNS availability


/*######################## MAIN CONFIG ########################*/
#define DATA_PIN      D4          // Should be GPIO02 on other boards like the NodeMCU
#define LED_TYPE      WS2812B     // You might also use a WS2811 or any other strip that is fastled compatible
#define COLOR_ORDER   GRB         // Change this if colors are swapped (in my case, red was swapped with green)
#define MILLI_AMPS    5000        // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define VOLTS         5           // Voltage of the Power Supply

#define LEAFCOUNT 16              // Amount of triangles
#define PIXELS_PER_LEAF 6        // Amount of LEDs inside 1x Tringle

const bool apMode = false;        // set to true if the esp8266 should open an access point

#define SOUND_REACTIVE            // Uncomment to enable the Sound reactive mode
#define SOUND_SENSOR_PIN A0       // An Analog sensor should be connected to an analog pin
#define SENSOR_TYPE 1             // 0: Dumb Sensors, 1: MAX4466 Sound Sensor, 2: MAX9814 Sound Sensor
int16_t audioMinVol = 135;        // minimal Voltage from Mic, higher Value = less sensitive
int16_t audioMaxVol = 145;        // maximal Voltage from Mic, lower Value = more LED on with lower Volume

#define HOSTNAME "Nanoleafs"      // Name that appears in your network
#define CORRECTION UncorrectedColor       // If colors are weird use TypicalLEDStrip

#define RANDOM_AUTOPLAY_PATTERN   // if enabled the next pattern for autoplay is choosen at random,
                                  // if commented out patterns will play in order
#define ENABLE_ALEXA_SUPPORT      // Espalexa library required

/*######################## MAIN CONFIG END ####################*/


/*############ Alexa Configuration ############*/
/* This part configures the devices that can be detected,
 * by your Amazon Alexa device. In order to Connect the device,
 * open http://ip_of_the_esp8266/alexa in your browser.
 * Afterwards tell say "Alexa, discover devices" to your device,
 * after around 30 seconds it should respond with the new devices
 * it has detected.
 *
 * In order to be able to control mutliple parameters of the nanoleafs,
 * the code creates multiple devices.
 *
 * To add those extra devices remove the two "//" in front of the,
 * defines below.
 */
#ifdef ENABLE_ALEXA_SUPPORT
#define ALEXA_DEVICE_NAME           "Nanoleafs"
#define AddAutoplayDevice           "Nanoleafs Autoplay"
#define AddStrobeDevice             "Nanoleafs Strobe"
#define AddSpecificPatternDevice    "Nanoleafs Party"
#define AddAudioDevice              "Nanoleafs Audio"


#ifdef AddSpecificPatternDevice
#define SpecificPattern 1   // Parameter defines what pattern gets executed
#endif
#endif // ENABLE_ALEXA_SUPPORT
 /*########## Alexa Configuration END ##########*/



ESP8266WebServer webServer(80);
//WebSocketsServer webSocketsServer = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdateServer;

#include "FSBrowser.h"
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define NUM_LEDS (PIXELS_PER_LEAF * LEAFCOUNT)
#define FRAMES_PER_SECOND  120  // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.
#define SOUND_REACTIVE_FPS 120


#include "Secrets.h" // this file is intentionally not included in the sketch, so nobody accidentally commits their secret information.
// create a Secrets.h file with the following:
// AP mode password
// const char WiFiAPPSK[] = "your-password";

// Wi-Fi network to connect to (if not in AP mode)
// char* ssid = "your-ssid";
// char* password = "your-password";

#ifdef ENABLE_ALEXA_SUPPORT
#include <Espalexa.h>
void mainAlexaEvent(EspalexaDevice*);
Espalexa espalexa;
ESP8266WebServer webServer2(80);
EspalexaDevice* alexa_main;
#endif // ENABLE_ALEXA_SUPPORT

CRGB leds[NUM_LEDS];

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 5, 32, 64, 128, 255 };
uint8_t brightnessIndex = 0;

char vals[4 + LEAFCOUNT * 5][4] = { "" };

uint8_t breathe = 0;  // value for starting custom pattern
uint8_t breathe_dir = 1;  // 1== rising
char cpattern[500] = "";

uint8_t allLeafs = 1;    // Sets if all leafs should get the same color
uint8_t selectedLeaf = 1;    // Sets position of leaf to color

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
uint8_t secondsPerPalette = 10;

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
uint8_t cooling = 3;

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
uint8_t sparking = 50;

uint8_t speed = 70;

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];

uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette(CRGB::Black);
CRGBPalette16 gTargetPalette(gGradientPalettes[0]);

CRGBPalette16 IceColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t currentPatternIndex = 0; // Index number of which pattern is current
uint8_t autoplay = 0;

uint8_t autoplayDuration = 10;
unsigned long autoPlayTimeout = 0;

uint8_t currentPaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB::Blue;

// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(value);
  }
}

typedef void(*Pattern)();
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

#include "Twinkles.h"
#include "TwinkleFOX.h"

// List of patterns to cycle through.  Each is defined as a separate function below.

PatternAndNameList patterns = {
  { pride,                  "Pride" },
  { colorWaves,             "Color Waves" },
  { rainbow,                "Horizontal Rainbow" },
  { rainbowSolid,           "Solid Rainbow" },
  { confetti,               "Confetti" },
  { sinelon,                "Sinelon" },
  { bpm,                    "Beat" },
  { juggle,                 "Juggle" },
  { fire,                   "Fire" },
  { water,                  "Water" },
  { strobe,                 "Strobe"},
  { rainbow_strobe,         "Rainbow Strobe"},

  // twinkle patterns
  { rainbowTwinkles,        "Rainbow Twinkles" },
  { snowTwinkles,           "Snow Twinkles" },
  { cloudTwinkles,          "Cloud Twinkles" },
  { incandescentTwinkles,   "Incandescent Twinkles" },

  // TwinkleFOX patterns
  { retroC9Twinkles,        "Retro C9 Twinkles" },
  { redWhiteTwinkles,       "Red & White Twinkles" },
  { blueWhiteTwinkles,      "Blue & White Twinkles" },
  { redGreenWhiteTwinkles,  "Red, Green & White Twinkles" },
  { fairyLightTwinkles,     "Fairy Light Twinkles" },
  { snow2Twinkles,          "Snow 2 Twinkles" },
  { hollyTwinkles,          "Holly Twinkles" },
  { iceTwinkles,            "Ice Twinkles" },
  { partyTwinkles,          "Party Twinkles" },
  { forestTwinkles,         "Forest Twinkles" },
  { lavaTwinkles,           "Lava Twinkles" },
  { fireTwinkles,           "Fire Twinkles" },
  { cloud2Twinkles,         "Cloud 2 Twinkles" },
  { oceanTwinkles,          "Ocean Twinkles" },

  { showSolidColor,         "Solid Color" }
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

typedef struct {
  CRGBPalette16 palette;
  String name;
} PaletteAndName;
typedef PaletteAndName PaletteAndNameList[];

const CRGBPalette16 palettes[] = {
  RainbowColors_p,
  RainbowStripeColors_p,
  CloudColors_p,
  LavaColors_p,
  OceanColors_p,
  ForestColors_p,
  PartyColors_p,
  HeatColors_p
};

const uint8_t paletteCount = ARRAY_SIZE(palettes);

const String paletteNames[paletteCount] = {
  "Rainbow",
  "Rainbow Stripe",
  "Cloud",
  "Lava",
  "Ocean",
  "Forest",
  "Party",
  "Heat",
};

#include "Fields.h"

void setup() {
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
#ifdef SOUND_REACTIVE
  patterns[patternCount - 2] = { soundReactive,          "Sound Reactive" };
#endif // SOUND_REACTIVE
  Serial.begin(115200);
  delay(100);
  Serial.setDebugOutput(true);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // for WS2812 (Neopixel)
//FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS); // for APA102 (Dotstar)
  FastLED.setDither(false);
  FastLED.setCorrection(CORRECTION);
  FastLED.setBrightness(brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MILLI_AMPS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  EEPROM.begin(512);
  loadSettings();

  FastLED.setBrightness(brightness);

  //  irReceiver.enableIRIn(); // Start the receiver

  Serial.println();
  Serial.print(F("Heap: ")); Serial.println(system_get_free_heap_size());
  Serial.print(F("Boot Vers: ")); Serial.println(system_get_boot_version());
  Serial.print(F("CPU: ")); Serial.println(system_get_cpu_freq());
  Serial.print(F("SDK: ")); Serial.println(system_get_sdk_version());
  Serial.print(F("Chip ID: ")); Serial.println(system_get_chip_id());
  Serial.print(F("Flash ID: ")); Serial.println(spi_flash_get_id());
  Serial.print(F("Flash Size: ")); Serial.println(ESP.getFlashChipRealSize());
  Serial.print(F("Vcc: ")); Serial.println(ESP.getVcc());
  Serial.println();

#ifdef SOUND_REACTIVE
#if SENSOR_TYPE == 0
  pinMode(SOUND_SENSOR_PIN, INPUT);
#endif
#endif // SOUND_REACTIVE


  SPIFFS.begin();
  {
    Serial.println("SPIFFS contents:");

    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
  }


  if (apMode)
  {
    WiFi.mode(WIFI_AP);

    // Do a little work to get a unique-ish name. Append the
    // last two bytes of the MAC (HEX'd) to "Thing-":
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
      String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = "ESP8266 Thing " + macID;

    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);

    for (int i = 0; i < AP_NameString.length(); i++)
      AP_NameChar[i] = AP_NameString.charAt(i);

    WiFi.softAP(AP_NameChar, WiFiAPPSK);

    Serial.printf("Connect to Wi-Fi access point: %s\n", AP_NameChar);
    Serial.println("and open http://192.168.4.1 in your browser");
  }
  else
  {
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
      WiFi.begin(ssid, password);
      WiFi.hostname(HOSTNAME);
    }
  }


#ifdef ENABLE_ALEXA_SUPPORT
#ifdef ALEXA_DEVICE_NAME
  alexa_main = new EspalexaDevice(ALEXA_DEVICE_NAME, mainAlexaEvent, EspalexaDeviceType::color);
#else
  alexa_main = new EspalexaDevice(HOSTNAME, mainAlexaEvent, EspalexaDeviceType::color);
#endif
  espalexa.addDevice(alexa_main);
#ifdef AddAutoplayDevice
  espalexa.addDevice(AddAutoplayDevice, AlexaAutoplayEvent, EspalexaDeviceType::onoff); //non-dimmable device
#endif
#ifdef AddStrobeDevice
  espalexa.addDevice(AddStrobeDevice, AlexaStrobeEvent, EspalexaDeviceType::color); //non-dimmable device
#endif
#ifdef AddSpecificPatternDevice
  espalexa.addDevice(AddSpecificPatternDevice, AlexaSpecificEvent, EspalexaDeviceType::onoff); //non-dimmable device
#endif
#ifdef AddAudioDevice
  espalexa.addDevice(AddAudioDevice, AlexaAudioEvent, EspalexaDeviceType::onoff); //non-dimmable device
#endif


  webServer.onNotFound([]() {
    if (!espalexa.handleAlexaApiCall(webServer.uri(), webServer.arg(0))) //if you don't know the URI, ask espalexa whether it is an Alexa control request
    {
      //whatever you want to do with 404s
      webServer.send(404, "text/plain", "Not found");
    }
  });



  webServer.on("/alexa", HTTP_GET, []() {
    String h = "<font face='arial'><h1> Alexa pairing mode</h1>";
    h += "<h2>Procedure: </h3>";
    h += "The webserver will reboot and the UI won't be available.<br>";
    h += "<b>Now. Say to Alexa: 'Alexa, discover devices'.<b><br><br>";
    h += "Alexa should tell you that it found a new device, if it did reset the esp8266 to return to the normal mode.</font>";
    webServer.send(200, "text/html", h);
    delay(100);
    webServer.stop();
    delay(500);
    webServer.close();
    delay(500);


    webServer2.onNotFound([]() {
      if (!espalexa.handleAlexaApiCall(webServer2.uri(), webServer2.arg(0))) //if you don't know the URI, ask espalexa whether it is an Alexa control request
      {
        //whatever you want to do with 404s
        webServer2.send(404, "text/plain", "Not found");
      }
    });
    espalexa.begin(&webServer2);
    delay(100);
    while (1)
    {
      espalexa.loop();
      delay(1);
    }
  });
#endif

  httpUpdateServer.setup(&webServer);

  webServer.on("/all", HTTP_GET, []() {
    String json = getFieldsJson(fields, fieldCount);
    webServer.send(200, "text/json", json);
  });

  webServer.on("/fieldValue", HTTP_GET, []() {
    String name = webServer.arg("name");
    String value = getFieldValue(name, fields, fieldCount);
    webServer.send(200, "text/json", value);
  });

  webServer.on("/fieldValue", HTTP_POST, []() {
    String name = webServer.arg("name");
    String value = webServer.arg("value");
    String newValue = setFieldValue(name, value, fields, fieldCount);
    webServer.send(200, "text/json", newValue);
  });

  webServer.on("/power", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPower(value.toInt());
    sendInt(power);
  });

  webServer.on("/cooling", HTTP_POST, []() {
    String value = webServer.arg("value");
    cooling = value.toInt();
    broadcastInt("cooling", cooling);
    sendInt(cooling);
  });

  webServer.on("/sparking", HTTP_POST, []() {
    String value = webServer.arg("value");
    sparking = value.toInt();
    broadcastInt("sparking", sparking);
    sendInt(sparking);
  });

  webServer.on("/speed", HTTP_POST, []() {
    String value = webServer.arg("value");
    speed = value.toInt();
    broadcastInt("speed", speed);
    sendInt(speed);
  });

  webServer.on("/twinkleSpeed", HTTP_POST, []() {
    String value = webServer.arg("value");
    twinkleSpeed = value.toInt();
    if (twinkleSpeed < 0) twinkleSpeed = 0;
    else if (twinkleSpeed > 8) twinkleSpeed = 8;
    broadcastInt("twinkleSpeed", twinkleSpeed);
    sendInt(twinkleSpeed);
  });

  webServer.on("/twinkleDensity", HTTP_POST, []() {
    String value = webServer.arg("value");
    twinkleDensity = value.toInt();
    if (twinkleDensity < 0) twinkleDensity = 0;
    else if (twinkleDensity > 8) twinkleDensity = 8;
    broadcastInt("twinkleDensity", twinkleDensity);
    sendInt(twinkleDensity);
  });

  webServer.on("/solidColor", HTTP_POST, []() {
    String r = webServer.arg("r");
    String g = webServer.arg("g");
    String b = webServer.arg("b");
    setSolidColor(r.toInt(), g.toInt(), b.toInt());
    #ifdef ENABLE_ALEXA_SUPPORT
    alexa_main->setColor(r.toInt(), g.toInt(), b.toInt());
    #endif
    sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
  });

  webServer.on("/pattern", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPattern(value.toInt());
    sendInt(currentPatternIndex);
  });

  webServer.on("/patternName", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPatternName(value);
    sendInt(currentPatternIndex);
  });

  webServer.on("/palette", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPalette(value.toInt());
    sendInt(currentPaletteIndex);
  });

  webServer.on("/paletteName", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPaletteName(value);
    sendInt(currentPaletteIndex);
  });

  webServer.on("/brightness", HTTP_POST, []() {
    String value = webServer.arg("value");
    setBrightness(value.toInt());
    #ifdef ENABLE_ALEXA_SUPPORT
    alexa_main->setValue(brightness);
    #endif
    sendInt(brightness);
  });

  webServer.on("/autoplay", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutoplay(value.toInt());
    sendInt(autoplay);
  });

  webServer.on("/autoplayDuration", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutoplayDuration(value.toInt());
    sendInt(autoplayDuration);
  });

  webServer.on("/allLeafs", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAllLeafs(value.toInt());
    sendInt(allLeafs);
  });

  webServer.on("/selectedLeaf", HTTP_POST, []() {
    String value = webServer.arg("value");
    setSelectedLeaf(value.toInt());
    sendInt(selectedLeaf);
  });


  webServer.on("/custom", HTTP_POST, []() {
    String value = webServer.arg("value");
    Serial.println(value);
    value.toCharArray(cpattern, 500);
    Serial.println(value);
    sendString(value);
    setPattern(30);
  });


  //list directory
  webServer.on("/list", HTTP_GET, handleFileList);
  //load editor
  webServer.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) webServer.send(404, "text/plain", "FileNotFound");
  });
  //create file
  webServer.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  webServer.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  webServer.on("/edit", HTTP_POST, []() {
    webServer.send(200, "text/plain", "");
  }, handleFileUpload);

  webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");
#ifdef ENABLE_ALEXA_SUPPORT
  espalexa.begin(&webServer);
#endif
#ifndef ENABLE_ALEXA_SUPPORT
  webServer.begin();
#endif

  Serial.println("HTTP web server started");

  //  webSocketsServer.begin();
  //  webSocketsServer.onEvent(webSocketEvent);
  //  Serial.println("Web socket server started");

  autoPlayTimeout = millis() + (autoplayDuration * 1000);
}

void sendInt(uint8_t value)
{
  sendString(String(value));
}

void sendString(String value)
{
  webServer.send(200, "text/plain", value);
}

void broadcastInt(String name, uint8_t value)
{
  String json = "{\"name\":\"" + name + "\",\"value\":" + String(value) + "}";
  //  webSocketsServer.broadcastTXT(json);
}

void broadcastString(String name, String value)
{
  String json = "{\"name\":\"" + name + "\",\"value\":\"" + String(value) + "\"}";
  //  webSocketsServer.broadcastTXT(json);
}

void loop() {
  MDNS.update();
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(65535));

  //  dnsServer.processNextRequest();
  //  webSocketsServer.loop();
#ifdef ENABLE_ALEXA_SUPPORT
  espalexa.loop();
#else
  webServer.handleClient();
#endif
  //  handleIrInput();

  if (power == 0) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    // FastLED.delay(15);
    return;
  }

  static bool hasConnected = false;
  EVERY_N_SECONDS(1) {
    if (WiFi.status() != WL_CONNECTED) {
      //      Serial.printf("Connecting to %s\n", ssid);
      hasConnected = false;
    }
    else if (!hasConnected) {
      hasConnected = true;
      Serial.print("Connected! Open http://");
      Serial.print(WiFi.localIP());
      Serial.println(" in your browser \n");
      if (!MDNS.begin(HOSTNAME)) {
        Serial.println("Error setting up MDNS responder! \n");
      } else {
        Serial.println("mDNS responder started \n");
        MDNS.addService("http", "tcp", 80);
      }
    }
  }

  // EVERY_N_SECONDS(10) {
  //   Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  // }

  // change to a new cpt-city gradient palette
  EVERY_N_SECONDS(secondsPerPalette) {
    gCurrentPaletteNumber = addmod8(gCurrentPaletteNumber, 1, gGradientPaletteCount);
    gTargetPalette = gGradientPalettes[gCurrentPaletteNumber];
  }

  EVERY_N_MILLISECONDS(80) {
    // slowly blend the current palette to the next
    nblendPaletteTowardPalette(gCurrentPalette, gTargetPalette, 8);
    gHue++;  // slowly cycle the "base color" through the rainbow
  }

  EVERY_N_MILLIS_I(thistimer, 128-(speed/2)) {
    if (breathe_dir == 1)breathe++; else breathe--;
    if (breathe >= 255)breathe_dir = 0;
    else if (breathe <= 0) breathe_dir = 1;
    //Serial.println(breathe);
  }
  thistimer.setPeriod(64-(speed/4));

  if (autoplay && (millis() > autoPlayTimeout)) {
    adjustPattern(true);
    autoPlayTimeout = millis() + (autoplayDuration * 1000);
  }

  // Call the current pattern function once, updating the 'leds' array
  patterns[currentPatternIndex].pattern();

  FastLED.show();

  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

//void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
//
//  switch (type) {
//    case WStype_DISCONNECTED:
//      Serial.printf("[%u] Disconnected!\n", num);
//      break;
//
//    case WStype_CONNECTED:
//      {
//        IPAddress ip = webSocketsServer.remoteIP(num);
//        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
//
//        // send message to client
//        // webSocketsServer.sendTXT(num, "Connected");
//      }
//      break;
//
//    case WStype_TEXT:
//      Serial.printf("[%u] get Text: %s\n", num, payload);
//
//      // send message to client
//      // webSocketsServer.sendTXT(num, "message here");
//
//      // send data to all connected clients
//      // webSocketsServer.broadcastTXT("message here");
//      break;
//
//    case WStype_BIN:
//      Serial.printf("[%u] get binary length: %u\n", num, length);
//      hexdump(payload, length);
//
//      // send message to client
//      // webSocketsServer.sendBIN(num, payload, lenght);
//      break;
//  }
//}



void loadSettings()
{
  brightness = EEPROM.read(0);

  currentPatternIndex = EEPROM.read(1);
  if (currentPatternIndex < 0)
    currentPatternIndex = 0;
  else if (currentPatternIndex >= patternCount)
    currentPatternIndex = patternCount - 1;

  byte r = EEPROM.read(2);
  byte g = EEPROM.read(3);
  byte b = EEPROM.read(4);

  if (r == 0 && g == 0 && b == 0)
  {
  }
  else
  {
    solidColor = CRGB(r, g, b);
  }

  power = EEPROM.read(5);

  autoplay = EEPROM.read(6);
  autoplayDuration = EEPROM.read(7);

  currentPaletteIndex = EEPROM.read(8);
  if (currentPaletteIndex < 0)
    currentPaletteIndex = 0;
  else if (currentPaletteIndex >= paletteCount)
    currentPaletteIndex = paletteCount - 1;
}

void setPower(uint8_t value)
{
  power = value == 0 ? 0 : 1;

  EEPROM.write(5, power);
  EEPROM.commit();

  broadcastInt("power", power);
}

void setAutoplay(uint8_t value)
{
  autoplay = value == 0 ? 0 : 1;

  EEPROM.write(6, autoplay);
  EEPROM.commit();

  broadcastInt("autoplay", autoplay);
}

void setAutoplayDuration(uint8_t value)
{
  autoplayDuration = value;

  EEPROM.write(7, autoplayDuration);
  EEPROM.commit();

  autoPlayTimeout = millis() + (autoplayDuration * 1000);

  broadcastInt("autoplayDuration", autoplayDuration);
}

void setAllLeafs(uint8_t value)
{
  allLeafs = value == 0 ? 0 : 1;

  EEPROM.write(8, allLeafs);
  EEPROM.commit();

  broadcastInt("allLeafs", allLeafs);
}

void setSelectedLeaf(uint8_t value)
{
  selectedLeaf = value;

  EEPROM.write(9, selectedLeaf);
  EEPROM.commit();

  broadcastInt("selectedLeaf", selectedLeaf);
}

void setSolidColor(CRGB color)
{
  setSolidColor(color.r, color.g, color.b);
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
  solidColor = CRGB(r, g, b);

  EEPROM.write(2, r);
  EEPROM.write(3, g);
  EEPROM.write(4, b);
  EEPROM.commit();

  setPatternName("Solid Color");

  broadcastString("color", String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
  FastLED.show();
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
#ifdef RANDOM_AUTOPLAY_PATTERN
  if (autoplay == 1)
  {
    uint8_t lastpattern = currentPatternIndex;
    while (currentPatternIndex == lastpattern)
    {
      uint8_t newpattern = random8(0, patternCount - 2);
      if (newpattern != lastpattern)currentPatternIndex = newpattern;
    }
  }
#else // RANDOM_AUTOPLAY_PATTERN
  if (up)
    currentPatternIndex++;
  else
    currentPatternIndex--;
#endif
  if (autoplay == 0)
  {
    if (up)
      currentPatternIndex++;
    else
      currentPatternIndex--;
  }
  // wrap around at the ends
  if (currentPatternIndex < 0)
    currentPatternIndex = patternCount - 1;
  if (currentPatternIndex >= (patternCount-1))
    currentPatternIndex = 0;

  if (autoplay == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }

  broadcastInt("pattern", currentPatternIndex);
}

void setPattern(uint8_t value)
{
  if (value >= patternCount)
    value = patternCount - 1;

  currentPatternIndex = value;

  if (autoplay == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }

  broadcastInt("pattern", currentPatternIndex);
}

void setPatternName(String name)
{
  for (uint8_t i = 0; i < patternCount; i++) {
    if (patterns[i].name == name) {
      setPattern(i);
      break;
    }
  }
}

void setPalette(uint8_t value)
{
  if (value >= paletteCount)
    value = paletteCount - 1;

  currentPaletteIndex = value;

  EEPROM.write(8, currentPaletteIndex);
  EEPROM.commit();

  broadcastInt("palette", currentPaletteIndex);
}

void setPaletteName(String name)
{
  for (uint8_t i = 0; i < paletteCount; i++) {
    if (paletteNames[i] == name) {
      setPalette(i);
      break;
    }
  }
}

void adjustBrightness(bool up)
{
  if (up && brightnessIndex < brightnessCount - 1)
    brightnessIndex++;
  else if (!up && brightnessIndex > 0)
    brightnessIndex--;

  brightness = brightnessMap[brightnessIndex];

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();

  broadcastInt("brightness", brightness);
}

void setBrightness(uint8_t value)
{
  if (value > 255)
    value = 255;
  else if (value < 0) value = 0;

  brightness = value;

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();

  broadcastInt("brightness", brightness);
}

void strandTest()
{
  static uint8_t i = 0;

  EVERY_N_SECONDS(1)
  {
    i++;
    if (i >= NUM_LEDS)
      i = 0;
  }

  fill_solid(leds, NUM_LEDS, CRGB::Black);

  leds[i] = solidColor;
}

void showSolidColor()
{
  if (allLeafs == 0 && selectedLeaf > 0 && selectedLeaf <= LEAFCOUNT)fill_solid(leds + PIXELS_PER_LEAF * (selectedLeaf - 1), PIXELS_PER_LEAF, solidColor);
  else fill_solid(leds, NUM_LEDS, solidColor);
}

// Patterns from FastLED example DemoReel100: https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino


void rainbow_strobe()
{
  if (autoplay == 1)adjustPattern(true);
  static bool p = false;
  static long lm = 0;
  if (millis() - lm > (128 - (speed / 2)))
  {
    if (p) fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
    else fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
    lm = millis();
    p = !p;
  }
}

void strobe()
{
  if (autoplay == 1)adjustPattern(true);
  static bool p = false;
  static long lm = 0;
  if (millis() - lm > (128 - (speed / 2)))
  {
    if (p) fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
    else fill_solid(leds, NUM_LEDS, solidColor);
    lm = millis();
    p = !p;
  }
}


void rainbow()
{
  // FastLED's built-in rainbow generator
  for (int i = 0; i < LEAFCOUNT; i++)
  {
    uint8_t myHue = (gHue + i * (255 / LEAFCOUNT));
    gHue = gHue > 255 ? gHue - 255 : gHue;
    //Serial.printf("I:%d \tH:%d\n", i*PIXELS_PER_LEAF, myHue);
    fill_solid(leds + i * PIXELS_PER_LEAF, PIXELS_PER_LEAF, CHSV(myHue, 255, 255));
  }
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void rainbowSolid()
{
  fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 3);
  int pos = random16(LEAFCOUNT * 3);
  // leds[pos] += CHSV( gHue + random8(64), 200, 255);
  int val = gHue + random8(64);
  for (int i = 0; i < (PIXELS_PER_LEAF / 3); i++)
  {

    leds[i + pos * (PIXELS_PER_LEAF / 3)] += ColorFromPalette(palettes[currentPaletteIndex], val);
    //Serial.printf("POS:%d\n", i + pos);
  }
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16(speed, 0, NUM_LEDS);
  static int prevpos = 0;
  CRGB color = ColorFromPalette(palettes[currentPaletteIndex], gHue, 255);
  if (pos < prevpos) {
    fill_solid(leds + pos, (prevpos - pos) + 1, color);
  }
  else {
    fill_solid(leds + prevpos, (pos - prevpos) + 1, color);
  }
  prevpos = pos;
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8(speed, 64, 255);
  CRGBPalette16 palette = palettes[currentPaletteIndex];
  for (int i = 0; i < LEAFCOUNT; i++) {
    for (int i2 = 0; i2 < PIXELS_PER_LEAF; i2++)leds[i * PIXELS_PER_LEAF + i2] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

// BACKUP
/*
  void bpm()
  {
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8(speed, 64, 255);
  CRGBPalette16 palette = palettes[currentPaletteIndex];
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
  }
*/

void juggle()
{
  static uint8_t    numdots = 4; // Number of dots in use.
  static uint8_t   faderate = 2; // How long should the trails be. Very low value = longer trails.
  static uint8_t     hueinc = 255 / numdots - 1; // Incremental change in hue between each dot.
  static uint8_t    thishue = 0; // Starting hue.
  static uint8_t     curhue = 0; // The current hue
  static uint8_t    thissat = 255; // Saturation of the colour.
  static uint8_t thisbright = 255; // How bright should the LED/display be.
  static uint8_t   basebeat = 5; // Higher = faster movement.

  static uint8_t lastSecond = 99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
  uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

  if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch (secondHand) {
      case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
      case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
      case 20: numdots = 8; basebeat = 3; hueinc = 0; faderate = 8; thishue = random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 30: break;
    }
  }

  // Several colored dots, weaving in and out of sync with each other
  curhue = thishue; // Reset the hue values.
  fadeToBlackBy(leds, NUM_LEDS, faderate);
  for (int i = 0; i < numdots; i++) {
    //beat16 is a FastLED 3.1 function
    leds[beatsin16(basebeat + i + numdots, 0, NUM_LEDS)] += CHSV(gHue + curhue, thissat, thisbright);
    curhue += hueinc;
  }
}

void fire()
{
  heatMap(HeatColors_p, true);
}

void water()
{
  heatMap(IceColors_p, false);
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88(87, 220, 250);
  uint8_t brightdepth = beatsin88(341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis;
  sLastMillis = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88(400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for (uint16_t i = 0; i < (LEAFCOUNT * 3); i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16 += brightnessthetainc16;
    uint16_t b16 = sin16(brightnesstheta16) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV(hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = ((LEAFCOUNT * 3) - 1) - pixelnumber;

    for (int i2 = 0; i2 < (PIXELS_PER_LEAF / 3); i2++)
    {
      nblend(leds[pixelnumber * (PIXELS_PER_LEAF / 3) + i2], newcolor, 64);
    }
  }
}

//#############BACKUP########################
/*
  // Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
  // This function draws rainbows with an ever-changing,
  // widely-varying set of parameters.
  void pride()
  {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
  hue16 += hueinc16;
  uint8_t hue8 = hue16 / 256;

  brightnesstheta16  += brightnessthetainc16;
  uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

  uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
  uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
  bri8 += (255 - brightdepth);

  CRGB newcolor = CHSV( hue8, sat8, bri8);

  uint16_t pixelnumber = i;
  pixelnumber = (NUM_LEDS - 1) - pixelnumber;

  nblend( leds[pixelnumber], newcolor, 64);
  }
  }

*/

void radialPaletteShift()
{
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // leds[i] = ColorFromPalette( gCurrentPalette, gHue + sin8(i*16), brightness);
    leds[i] = ColorFromPalette(gCurrentPalette, i + gHue, 255, LINEARBLEND);
  }
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void heatMap(CRGBPalette16 palette, bool up)
{
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(256));

  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  byte colorindex;

  // Step 1.  Cool down every cell a little
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for (uint16_t k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if (random8() < sparking) {
    int y = random8(7);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }

  // Step 4.  Map from heat cells to LED colors
  for (uint16_t j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    colorindex = scale8(heat[j], 190);

    CRGB color = ColorFromPalette(palette, colorindex);

    if (up) {
      leds[j] = color;
    }
    else {
      leds[(NUM_LEDS - 1) - j] = color;
    }
  }
}

void addGlitter(uint8_t chanceOfGlitter)
{
  if (random8() < chanceOfGlitter) {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
}

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

uint8_t beatsaw8(accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255,
                 uint32_t timebase = 0, uint8_t phase_offset = 0)
{
  uint8_t beat = beat8(beats_per_minute, timebase);
  uint8_t beatsaw = beat + phase_offset;
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatsaw, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

void colorWaves()
{
  colorwaves(leds, LEAFCOUNT * 3, gCurrentPalette);
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves(CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88(341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis;
  sLastMillis = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88(400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for (uint16_t i = 0; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if (h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    }
    else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16 += brightnessthetainc16;
    uint16_t b16 = sin16(brightnesstheta16) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8(index, 240);

    CRGB newcolor = ColorFromPalette(palette, index, bri8);

    uint16_t pixelnumber = i;
    //pixelnumber = (numleds - 1) - pixelnumber;
    //nblend(ledarray[pixelnumber], newcolor, 128);

    pixelnumber = ((LEAFCOUNT * 3) - 1) - pixelnumber;
    for (int i2 = 0; i2 < (PIXELS_PER_LEAF / 3); i2++)
    {
      nblend(leds[pixelnumber * (PIXELS_PER_LEAF / 3) + i2], newcolor, 128);
    }
  }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest(CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette(ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}


/*
   Function: ExtractValues
   Used to extract a given amount of values from the message with a start index
   Parameters:
   - startindex: position in the string where to start
   - valuecount: amount of values to capture
*/
void ExtractValues(char receivedChars[], int startindex, int valuecount)
{
  int pos = startindex;
  for (int c = 0; c < valuecount; c++)
  {
    int i = 0;
    while (receivedChars[pos] != ';' && receivedChars[pos] != '\0') {
      vals[c][i] = receivedChars[pos];
      pos++;
      i++;
    }
    vals[c][i] = '\0';
    pos++;
  }
#ifdef DEBUG_SERIAL
  for (int p = 0; p < valuecount; p++)
  {
    Serial.print("Extracting: "); Serial.println(vals[p]);
  }
#endif // DEBUG_SERIAL
}
void cycle(CRGB endclr, CRGB midclr, uint8_t start) {
  fill_gradient_RGB(leds, start, endclr, PIXELS_PER_LEAF/ 2, midclr);
  fill_gradient_RGB(leds, PIXELS_PER_LEAF/ 2 + 1, midclr, PIXELS_PER_LEAF, endclr);
}
// Set Custom Pattern for the node red part
void SetCustomPattern()
{
  uint8_t cnt = 0;
  uint8_t isflow = 0;

  ExtractValues(cpattern, 0, 2);
  cnt = atoi(vals[0]);
  isflow = atoi(vals[1]);
  ExtractValues(cpattern, 0, 2 + 5 * cnt);
  if (isflow == 0)
  {
    for (uint8_t i = 0; i < cnt; i++)
    {
      int8_t cmode = atoi(vals[2 + i * 5]);
      uint8_t phase = atoi(vals[2 + i * 5 + 1]);
      int mul = breathe;
      if (breathe_dir == 1)
      {
        if ((mul + phase) > 255)mul = 255 + (255 - mul - phase);
        else mul += phase;
      }
      else
      {
        if ((mul - phase) < 0)mul = -mul + phase;
        else mul -= phase;
      }
      if (cmode == 0)mul = 255;
      double fac = (mul * 100) / 255.00;
      //if(cmode==1)Serial.printf("%d ", mul);
      for (uint8_t x = 0; x < PIXELS_PER_LEAF; x++)
      {
        //Serial.printf("Setting %d to %d, %d, %d\n", cnt*PIXELS_PER_LEAF+x,atoi(vals[2+i*5+2]), atoi(vals[2+i*5+3]), atoi(vals[2+i*5+4]));
        leds[i * PIXELS_PER_LEAF + x] = CRGB((atoi(vals[2 + i * 5 + 2]) * fac) / 100.00, (atoi(vals[2 + i * 5 + 3]) * fac) / 100.00, (atoi(vals[2 + i * 5 + 4]) * fac) / 100.00);
      }
    }
  }
  else
  {
    for (int i = 0; i < cnt; i++)
    {
      if (i != (cnt - 1))
      {
        //uint8_t speed = beatsin8(6,0,255);
        CRGB endclr = blend(CRGB(atoi(vals[2 + i * 5 + 2]), atoi(vals[2 + i * 5 + 3]), atoi(vals[2 + i * 5 + 4])), CRGB(atoi(vals[2 + (i + 1) * 5 + 2]), atoi(vals[2 + (i + 1) * 5 + 3]), atoi(vals[2 + (i + 1) * 5 + 4])), breathe);
        CRGB midclr = blend(CRGB(atoi(vals[2 + (i + 1) * 5 + 2]), atoi(vals[2 + (i + 1) * 5 + 3]), atoi(vals[2 + (i + 1) * 5 + 4])), CRGB(atoi(vals[2 + i * 5 + 2]), atoi(vals[2 + i * 5 + 3]), atoi(vals[2 + i * 5 + 4])), breathe);
        cycle(endclr, midclr, i*PIXELS_PER_LEAF);
      }
      else
      {
        //uint8_t speed = beatsin8(6,0,255);
        CRGB endclr = blend(CRGB(atoi(vals[2 + 2]), atoi(vals[2 + 3]), atoi(vals[2 + 4])), CRGB(atoi(vals[2 + (i + 1) * 5 + 2]), atoi(vals[2 + (i + 1) * 5 + 3]), atoi(vals[2 + (i + 1) * 5 + 4])), breathe);
        CRGB midclr = blend(CRGB(atoi(vals[2 + (i + 1) * 5 + 2]), atoi(vals[2 + (i + 1) * 5 + 3]), atoi(vals[2 + (i + 1) * 5 + 4])), CRGB(atoi(vals[2 + 2]), atoi(vals[2 + 3]), atoi(vals[2 + 4])), breathe);
        cycle(endclr, midclr, i*PIXELS_PER_LEAF);
      }
    }
  }
  //Serial.println("");
}



// #################### Sound Reactive and Alexa below

void soundReactive()
{
  static int minlevel = 0;
  static int decay = 60;
  static double lastlevel = 2;
  static double level = 0;
#define arrsize 3
  static double measure8avg[arrsize] = { 0,0,0 };
  static int iter = 0;

#if SENSOR_TYPE == 0
  if (digitalRead(SOUND_SENSOR_PIN) > 0)level++;
  else level--;
  if (level < minlevel)level = minlevel;
  if (level > LEDS_PER_LINE)level = LEDS_PER_LINE;
  fill_solid(leds, level, CHSV(gHue, 255, 255));
  fadeToBlackBy(leds + level, LEDS_PER_LINE - level, decay);
#endif
#if SENSOR_TYPE > 0
#if SENSOR_TYPE == 2

  int measure = 0;

  for (int it = 0; it < 5; it++)
  {
    int m = analogRead(SOUND_SENSOR_PIN);
    Serial.println(m);
    if (m < 450 && m >350) m = 0;
    else if (m < 350)m = (400 - m) * 2;
    if (m > 400)m -= 400;
    measure += m;
  }
  measure /= 5;
#endif
#if SENSOR_TYPE == 1
  int measure = 0;

  for (int it = 0; it < 5; it++)
  {
    int m = analogRead(SOUND_SENSOR_PIN);

    m -= 800;
    m *= -1;
    //if (m < 100)m = 0;
    if (m < 150) m = 0;
    measure += m;
  }
  measure /= 5;
  measure /= 2;  // cut the volts in half
#endif
  //Serial.println(measure);

  iter++;
  if (iter > arrsize)iter = 0;
  measure8avg[iter] = measure;
  double avg = 0;
  for (int x = 0; x < arrsize; x++)
  {
    avg += measure8avg[x];
  }
  avg /= arrsize;

  avg = measure;

  int mlevel = map(avg, audioMinVol, audioMaxVol, 1, NUM_LEDS);
  if (mlevel < 1)mlevel = 1;
  //if (lastlevel > mlevel) level = lastlevel  - 0.8;
  //else if (lastlevel < mlevel) level = lastlevel+1;
  level = mlevel;

  if (level < minlevel)level = minlevel;
  if (level > NUM_LEDS)level = NUM_LEDS;
  fill_solid(leds, level, CHSV(gHue, 255, 255));
  fadeToBlackBy(leds + (int)level, NUM_LEDS - (int)level, decay);
  lastlevel = level;

  //Serial.print(mlevel); Serial.print(" "); Serial.println(level);
  //Serial.printf("%d, %d\n", mlevel, level);
#endif
}

//############################## ALEXA Device Events ##############################

#ifdef ENABLE_ALEXA_SUPPORT
void mainAlexaEvent(EspalexaDevice* d) {
  if (d == nullptr) return;

  Serial.print("Alexa update: rgb: "); Serial.print(d->getR() + d->getG() + d->getB()); Serial.print(", b: "); Serial.println(d->getValue());
  if (d->getValue() == 0)setPower(0); else {
    setPower(1);
    setBrightness(d->getValue());
  }
  static int lr;
  static int lg;
  static int lb;
  if ((lr != NULL && lr != d->getR() && lg != d->getG() && lb != d->getB()) || currentPatternIndex == patternCount - 1)
  {
    setSolidColor(d->getR(), d->getG(), d->getB());
    setPattern(patternCount - 1);
  }
  lr = d->getR();
  lg = d->getG();
  lb = d->getB();
}

#ifdef AddStrobeDevice
void AlexaStrobeEvent(EspalexaDevice* d) {
  if (d == nullptr) return;

  Serial.print("Alexa Strobe update: rgb: "); Serial.print(d->getR() + d->getG() + d->getB()); Serial.print(", b: "); Serial.println(d->getValue());
  if (d->getValue() == 0)setPattern(patternCount - 1); else {
    if (d->getValue() == 255)
    {
      setBrightness(255);
      setPattern(13);
    }
    else speed = d->getValue();
    d->setValue(speed);
  }
  static int lr;
  static int lg;
  static int lb;
  if ((lr != NULL && lr != d->getR() && lg != d->getG() && lb != d->getB()) || currentPatternIndex == patternCount - 1)
  {
    setSolidColor(d->getR(), d->getG(), d->getB());
    setPattern(12);
  }
  lr = d->getR();
  lg = d->getG();
  lb = d->getB();

}
#endif

#ifdef AddAutoplayDevice
void AlexaAutoplayEvent(EspalexaDevice* d) {
  if (d == nullptr) return;
  Serial.print("Alexa Autoplay update: state: "); Serial.println(d->getPercent());
  if (d->getValue() > 0)
  {
    setAutoplay(1);
    setAutoplayDuration(d->getPercent());
  }
  else setAutoplay(0);
}
#endif

#ifdef AddSpecificPatternDevice
void AlexaSpecificEvent(EspalexaDevice* d) {
  if (d == nullptr) return;
  Serial.print("Alexa Specific Pattern update: state: "); Serial.println(d->getValue());
  if (d->getValue() != 0)setPattern(SpecificPattern);
  else setPattern(patternCount - 1);
}
#endif

#ifdef AddAudioDevice
void AlexaAudioEvent(EspalexaDevice* d) {
  if (d == nullptr) return;

  Serial.print("Alexa Audio update: rgb: "); Serial.print(d->getR() + d->getG() + d->getB()); Serial.print(", b: "); Serial.println(d->getValue());
  if (d->getValue() != 0)setPattern(patternCount - 2);
  else setPattern(patternCount - 1);

}
#endif

#endif
