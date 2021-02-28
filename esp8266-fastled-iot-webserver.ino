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
#define FASTLED_INTERRUPT_RETRY_COUNT 1
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE
#include <FS.h>
#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#endif
#include <EEPROM.h>
#include "GradientPalettes.h"
#include "Field.h"

/*
    ______ _____  ____   ____  ___   _____  _____
   / ____// ___/ / __ \ ( __ )|__ \ / ___/ / ___/
  / __/   \__ \ / /_/ // __  |__/ // __ \ / __ \
 / /___  ___/ // ____// /_/ // __// /_/ // /_/ /
/_____/ /____//_/     \____//____/\____/ \____/
    ______ ___    _____ ______ __     ______ ____
   / ____//   |  / ___//_  __// /    / ____// __ \
  / /_   / /| |  \__ \  / /  / /    / __/  / / / /
 / __/  / ___ | ___/ / / /  / /___ / /___ / /_/ /
/_/    /_/  |_|/____/ /_/  /_____//_____//_____/
    ____ ____  ______
   /  _// __ \/_  __/
   / / / / / / / /
 _/ / / /_/ / / /
/___/ \____/ /_/
 _       __ ______ ____  _____  ______ ____  _    __ ______ ____
| |     / // ____// __ )/ ___/ / ____// __ \| |  / // ____// __ \
| | /| / // __/  / __  |\__ \ / __/  / /_/ /| | / // __/  / /_/ /
| |/ |/ // /___ / /_/ /___/ // /___ / _, _/ | |/ // /___ / _, _/
|__/|__//_____//_____//____//_____//_/ |_|  |___//_____//_/ |_|

*/ 

/*######################## MAIN CONFIG ########################*/
#define LED_TYPE            WS2812B                     // You might also use a WS2811 or any other strip that is Fastled compatible 
#define DATA_PIN            D3                          // Be aware: the pin mapping might be different on boards like the NodeMCU
//#define CLK_PIN           D5                          // Only required when using 4-pin SPI-based LEDs
#define MOSFET_PIN          D4                          // Pin for a MOSFET, can be used to physical power off the LED stripe
#define MOSFET_LEVEL        (HIGH)                      // logic level for LED state 'on'
#define CORRECTION          UncorrectedColor            // If colors are weird use TypicalLEDStrip
#define COLOR_ORDER         GRB                         // Change this if colors are swapped (in my case, red was swapped with green)
#define MILLI_AMPS          10000                       // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA)
#define VOLTS               5                           // Voltage of the Power Supply

#define LED_DEBUG 0                     // enable debug messages on serial console, set to 0 to disable debugging

#define DEFAULT_HOSTNAME "LEDs"         // Name that appears in your network, don't use whitespaces, use "-" instead

#define LED_DEVICE_TYPE 0               // The following types are available

/*
    0: Generic LED-Strip: a regular LED-Strip without any special arrangement (and Infinity Mirror + Bottle Lighting Pad)
        * Easiest: 5V WS2812B LED-Strip:            https://s.click.aliexpress.com/e/_dZ1hCJ7
        * (Long Ranges) 12V WS2811 LED-Strip:       https://s.click.aliexpress.com/e/_d7Ehe3L
        * (High-Speed) 5V SK9822 LED-Strip:         https://s.click.aliexpress.com/e/_d8pzc89
        * (Expensive) 5V APA102 LED-Strip:          https://s.click.aliexpress.com/e/_Bf9wVZUD
        * (Flexible) 5V WS2812 S LED-Strip:         https://s.click.aliexpress.com/e/_d6XxPOH
        * Wemos D1 Mini:                            https://s.click.aliexpress.com/e/_dTVGMGl
        * 5V Power Supply:                          https://s.click.aliexpress.com/e/_dY5zCWt
        * Solderless LED-Connector:                 https://s.click.aliexpress.com/e/_dV4rsjF
        * 3D-Printed Wemos-D1 case:                 https://www.thingiverse.com/thing:3544576
    1: LED-Matrix: With a flexible LED-Matrix you can display the audio like a Audio Visualizer
        * Flexible WS2812 Matrix:                   https://s.click.aliexpress.com/e/_d84R5kp
        * Wemos D1 Mini:                            https://s.click.aliexpress.com/e/_dTVGMGl
        * 5V Power Supply:                          https://s.click.aliexpress.com/e/_dY5zCWt
    2: 3D-Printed 7-Segment Clock, display the time in a cool 7-segment style, syncs with a ntp of your choice
        * unfortunatly the "thing's" description isn't updated yet to the new standalone system
        * Project link, small version:              https://www.thingiverse.com/thing:3117494
        * Project link, large version:              https://www.thingiverse.com/thing:2968056
    3: 3D-Printed Desk Lamp, a Lamp that reacts to sound for your desk
        * Project link, twisted version:            https://www.thingiverse.com/thing:4129249
        * Project link, round version:              https://www.thingiverse.com/thing:3676533
    4: 3D-Printed Nanoleafs, a Nanoleaf clone that can be made for cheap
        * Project link:                             https://www.thingiverse.com/thing:3354082
    5: 3D-Printed Animated RGB Logos
        * Project link, Twenty-One-Pilots:          https://www.thingiverse.com/thing:3523487
        * Project link, Thingiverse:                https://www.thingiverse.com/thing:3531086
*/

//---------------------------------------------------------------------------------------------------------//
// Device Configuration:
//---------------------------------------------------------------------------------------------------------//
#if LED_DEVICE_TYPE == 0                // Generic LED-Strip
    #define NUM_LEDS 24
    //#define NUM_LEDS 33
    //#define NUM_LEDS 183
    #define BAND_GROUPING    1            // Groups part of the band to save performance and network traffic
#elif LED_DEVICE_TYPE == 1              // LED MATRIX
    #define LENGTH 32
    #define HEIGHT 8
    //#define AddLogoVisualizers          // (only 32x8) Adds Visualization patterns with logo (currently only HBz)
#elif LED_DEVICE_TYPE == 2              // 7-Segment Clock
    #define NTP_REFRESH_INTERVAL_SECONDS 600            // 10 minutes
    const char* ntpServerName = "at.pool.ntp.org";      // Austrian ntp-timeserver
    int t_offset = 1;                                   // offset added to the time from the ntp server
    bool updateColorsEverySecond = false;               // if set to false it will update colors every minute (time patterns only)
    const int NTP_PACKET_SIZE = 48;
    bool switchedTimePattern = true;
    #define NUM_LEDS 30
    #define Digit1 0
    #define Digit2 7
    #define Digit3 16
    #define Digit4 23
    // Values for the Big Clock: 58, 0, 14, 30, 44

#elif LED_DEVICE_TYPE == 3              // Desk Lamp
    #define LINE_COUNT    8             // Amount of led strip pieces
    #define LEDS_PER_LINE 10            // Amount of led pixel per single led strip piece

#elif LED_DEVICE_TYPE == 4              // Nanoleafs
    #define LEAFCOUNT 12                // Amount of triangles
    #define PIXELS_PER_LEAF 12          // Amount of LEDs inside 1x Tringle

#elif LED_DEVICE_TYPE == 5              // Animated Logos
    // Choose your logo below, remove the comment in front of your design
    // Important: see "LOGO CONFIG" below

    #define TWENTYONEPILOTS
    //#define THINGIVERSE     // FIXME: THIS IS BROKEN

#endif

//---------------------------------------------------------------------------------------------------------//
// Feature Configuration: Enabled by removing the "//" in front of the define statements
//---------------------------------------------------------------------------------------------------------//
    //#define ENABLE_OTA_SUPPORT                // requires ArduinoOTA - library, not working on esp's with 1MB memory (esp-01, Wemos D1 lite ...)
        //#define OTA_PASSWORD "passwd123"      //  password that is required to update the esp's firmware wireless

    #define ENABLE_MULTICAST_DNS              // allows to access the UI via "http://<HOSTNAME>.local/", implemented by GitHub/WarDrake

    #define RANDOM_AUTOPLAY_PATTERN             // if enabled the next pattern for autoplay is choosen at random
    #define AUTOPLAY_IGNORE_UDP_PATTERNS        // remove visualization patterns from autoplay

    //#define ENABLE_ALEXA_SUPPORT              // Espalexa library required

    //#define SOUND_SENSOR_SUPPORT              // allows to control the leds using a physical sound-sensor, configuration below

    //#define ENABLE_SERIAL_AMBILIGHT           // allows to function as an ambilight behind a monitor by using data from usb-serial (integration of adalight)

    //#define ENABLE_MQTT_SUPPORT               // allows integration in homeassistant/googlehome/mqtt
                                                // mqtt server required, see MQTT Configuration for more, implemented by GitHub/WarDrake

    //#define ENABLE_UDP_VISUALIZATION          // allows to sync the LEDs with pc-music using https://github.com/NimmLor/IoT-Audio-Visualization-Center

    //#define ENABLE_HOMEY_SUPPORT              // Add support for Homey integration (Athom Homey library required)


//---------------------------------------------------------------------------------------------------------//


/*############ Alexa Configuration ############*/
/* This part configures the devices that can be detected,
 * by your Amazon Alexa device. In order to Connect the device,
 * open http://ip_of_the_esp8266/alexa in your browser. Or click
 * the alexa button in the navbar.
 * Afterwards tell say "Alexa, discover devices" to your device,
 * after around 30 seconds it should respond with the new devices
 * it has detected.
 *
 * In order to be able to control mutliple parameters of the strip,
 * the code must create multiple alexa devices. However you can
 * use
 *
 * To add those extra devices remove the two "//" in front of the,
 * defines below.
 *
 * The Devices with specific pattern, require the corresponding SpecificPatternX 
 * statement, as argument you have to provide the zero-based index of the `patterns`
 * array below. (Hint: CTRL + F -> "patterns =")
 * 
 */
#ifdef ENABLE_ALEXA_SUPPORT
    //#define ALEXA_DEVICE_NAME           DEFAULT_HOSTNAME
    //#define AddAutoplayDevice             ((String)DEFAULT_HOSTNAME + (String)" Autoplay")
    //#define AddStrobeDevice               ((String)DEFAULT_HOSTNAME + (String)" Strobe")
    //#define AddSpecificPatternDeviceA     ((String)DEFAULT_HOSTNAME + (String)" Party")
    //#define AddSpecificPatternDeviceB     ((String)DEFAULT_HOSTNAME + (String)" Chill")
    //#define AddSpecificPatternDeviceC     ((String)DEFAULT_HOSTNAME + (String)" Rainbow")
    //#define AddAudioDevice                ((String)DEFAULT_HOSTNAME + (String)" Audio")
    //#define SpecificPatternA 37           // Parameter defines what pattern gets executed
    //#define SpecificPatternB 12           // Parameter defines what pattern gets executed
    //#define SpecificPatternC 0            // Parameter defines what pattern gets executed
    //#define AudioPattern 44               // Parameter defines what pattern gets executed

#endif // ENABLE_ALEXA_SUPPORT
/*########## Alexa Configuration END ##########*/



/*######################## ANIMATED RGB LOGO CONFIG ########################*/

#ifdef TWENTYONEPILOTS
    #define RING_LENGTH 24                                      // amount of pixels for the Ring (should be 24)
    #define DOUBLE_STRIP_LENGTH 2                               // amount of pixels used for the straight double line
    #define DOT_LENGTH 1                                        // amount of pixels used for the dot
    #define ITALIC_STRIP_LENGTH 2                               // amount of pixels used for the 
    #define ANIMATION_NAME "Twenty One Pilots - Animated"       // name for the Logo animation, displayed on the webserver
    #define ANIMATION_NAME_STATIC "Twenty One Pilots - Static"  // logo for the static logo, displayed on the webserver
    #define ANIMATION_RING_DURATION 30                          // longer values result into a longer loop duration
    #define STATIC_RING_COLOR CRGB(222,255,5)                   // Color for the outer ring in static mode
    #define STATIC_LOGO_COLOR CRGB(150,240,3)                   // Color for the inner logo in static mode
/*
Wiring order:
The array below will determine the order of the wiring,
  the first value is for the ring, I've hooked it up after the inner part,
  so it's the start value is the total length of all other pixels (2+1+2)
the second one is for the vertical double line
  in my case it was the first one that is connected to the esp8266,
the third one is for the dot and the fourth one for the angled double line
if you have connected the ring first it should look like this: const int twpOffsets[] = { 0,24,26,27 };
*/
// Syntax: { <RING>, <VERTICAL>, <HORIZONTAL_DOT>, <ANGLED> };
// The values represent the zero-based index on the strip of the element
    const int twpOffsets[] = { 5,0,2,3 };
#endif  // TWENTYONEPILOTS

#ifdef THINGIVERSE
    #define RING_LENGTH 24                                  // amount of pixels for the Ring (should be 24)
    #define HORIZONTAL_LENGTH 3                             // amount of pixels used for the straight double line
    #define VERTICAL_LENGTH 2                               // amount of pixels used for the straight double line
    #define ANIMATION_NAME "Thingiverse - Animated"         // name for the Logo animation, displayed on the webserver
    #define ANIMATION_NAME_STATIC "Thingiverse - Static"    // logo for the static logo, displayed on the webserver
    #define ANIMATION_RING_DURATION 30                      // longer values result into a longer loop duration
    #define STATIC_RING_COLOR CRGB(0,149,255)               // Color for the outer ring in static mode
    #define STATIC_LOGO_COLOR CRGB(0,149,255)               // Color for the inner logo in static mode
    #define RINGFIRST false                                 // change this to <true> if you have wired the ring first
    #define HORIZONTAL_BEFORE_VERTICAL true                 // change this to <true> if you have wired the horizontal strip before the vertical
#endif  // THINGIVERSE

/*###################### ANIMATED RGB LOGO CONFIG END ######################*/

/*-------- SOUND SENSOR (LEGACY) --------*/
#ifdef SOUND_SENSOR_SUPPORT
    #define SOUND_SENSOR_PIN A0       // An Analog sensor should be connected to an analog pin
    #define SENSOR_TYPE 1             // 0: Dumb Sensors, 1: MAX4466 Sound Sensor, 2: MAX9814 Sound Sensor
    // Values for MAX44666:
    int16_t audioMinVol = 135;        // minimal Voltage from Mic, higher Value = less sensitive
    int16_t audioMaxVol = 145;        // maximal Voltage from Mic, lower Value = more LED on with lower Volume
#endif
/*------ SOUND SENSOR (LEGACY) END ------*/


/*######################## MQTT Configuration ########################*/
#ifdef ENABLE_MQTT_SUPPORT
    // these are deafault settings which can be changed in the web interface "settings" page
    #define MQTT_ENABLED 0
    #define MQTT_HOSTNAME "homeassistant.local"
    #define MQTT_PORT 1883
    #define MQTT_USER "MyUserName"
    #define MQTT_PASS ""
    #define MQTT_TOPIC_SET "/set"                                       // MQTT Topic to subscribe to for changes(Home Assistant)
    #if LED_DEVICE_TYPE == 0
        #define MQTT_TOPIC "homeassistant/light/ledstrip"               // MQTT Topic to Publish to for state and config (Home Assistant)
        #define MQTT_DEVICE_NAME "Ledstrip"
    #elif LED_DEVICE_TYPE == 1
        #define MQTT_TOPIC "homeassistant/light/ledmatrix"              // MQTT Topic to Publish to for state and config (Home Assistant)
        #define MQTT_DEVICE_NAME "Led Matrix"
    #elif LED_DEVICE_TYPE == 2
        #define MQTT_TOPIC "homeassistant/light/7-segment-clock"        // MQTT Topic to Publish to for state and config (Home Assistant)
        #define MQTT_DEVICE_NAME "7 Segment Clock"
    #elif LED_DEVICE_TYPE == 3
        #define MQTT_TOPIC "homeassistant/light/desklamp"               // MQTT Topic to Publish to for state and config (Home Assistant)
        #define MQTT_DEVICE_NAME "Led Desk Lamp"
    #elif LED_DEVICE_TYPE == 4
        #define MQTT_TOPIC "homeassistant/light/nanoleafs"              // MQTT Topic to Publish to for state and config (Home Assistant)
        #define MQTT_DEVICE_NAME "Nanoleafs"
    #elif LED_DEVICE_TYPE == 5
        #define MQTT_TOPIC "homeassistant/light/ledlogo"                // MQTT Topic to Publish to for state and config (Home Assistant)
        #define MQTT_DEVICE_NAME "Animated Logo"
    #endif
    #define MQTT_UNIQUE_IDENTIFIER WiFi.macAddress()                    // A Unique Identifier for the device in Homeassistant (MAC Address used by default)
    #define MQTT_MAX_PACKET_SIZE 1024
    #define MQTT_MAX_TRANSFER_SIZE 1024

    #include <PubSubClient.h>                                           // Include the MQTT Library, must be installed via the library manager
    #include <ArduinoJson.h> 
    WiFiClient espClient;
    PubSubClient mqttClient(espClient);
#endif
/*###################### MQTT Configuration END ######################*/

/*#########################################################################################################//
-----------------------------------------------------------------------------------------------------------//
  _____ ____   _  __ ____ ____ _____    ____ _  __ ___ 
 / ___// __ \ / |/ // __//  _// ___/   / __// |/ // _ \
/ /__ / /_/ //    // _/ _/ / / (_ /   / _/ /    // // /
\___/ \____//_/|_//_/  /___/ \___/   /___//_/|_//____/ 
-----------------------------------------------------------------------------------------------------------//
###########################################################################################################*/

#define VERSION "4.5"
#define VERSION_DATE "2020-02-14"

// define debugging MACROS
#if LED_DEBUG != 0
#define SERIAL_DEBUG_ADD(s) Serial.print(s);
#define SERIAL_DEBUG_ADDF(format, ...) Serial.printf(format, __VA_ARGS__);
#define SERIAL_DEBUG_EOL Serial.print("\n");
#define SERIAL_DEBUG_BOL Serial.printf("DEBUG [%lu]: ", millis());
#define SERIAL_DEBUG_LN(s) SERIAL_DEBUG_BOL SERIAL_DEBUG_ADD(s) SERIAL_DEBUG_EOL
#define SERIAL_DEBUG_LNF(format, ...) SERIAL_DEBUG_BOL SERIAL_DEBUG_ADDF(format, __VA_ARGS__) SERIAL_DEBUG_EOL
#else
#define SERIAL_DEBUG_ADD(s) do{}while(0);
#define SERIAL_DEBUG_ADDF(format, ...) do{}while(0);
#define SERIAL_DEBUG_EOL do{}while(0);
#define SERIAL_DEBUG_BOL do{}while(0);
#define SERIAL_DEBUG_LN(s) do{}while(0);
#define SERIAL_DEBUG_LNF(format, ...) do{}while(0);
#endif

#ifdef LED_DEVICE_TYPE
#include <WiFiUdp.h>

#if LED_DEVICE_TYPE == 1
    #define PACKET_LENGTH LENGTH
    #define NUM_LEDS (HEIGHT * LENGTH)
    #define PACKET_LENGTH LENGTH
    #define BAND_GROUPING    1

#elif LED_DEVICE_TYPE == 2
    #define PACKET_LENGTH NUM_LEDS
    #define BAND_GROUPING    1
    IPAddress timeServerIP;
    WiFiUDP udpTime;

    byte packetBuffer[NTP_PACKET_SIZE];
    int hours = 0; int mins = 0; int secs = 0;
    unsigned int localPortTime = 2390;
    unsigned long update_timestamp = 0;
    unsigned long last_diff = 0;
    unsigned long ntp_timestamp = 0;

#elif LED_DEVICE_TYPE == 3
    #define NUM_LEDS      (LINE_COUNT * LEDS_PER_LINE)
    #define PACKET_LENGTH LEDS_PER_LINE
    #define BAND_GROUPING    1

#elif LED_DEVICE_TYPE == 4
    #define NUM_LEDS (PIXELS_PER_LEAF * LEAFCOUNT)
    #define PACKET_LENGTH (LEAFCOUNT * 3)
    #define BAND_GROUPING    1

#elif LED_DEVICE_TYPE == 5
    #define BAND_GROUPING    1
    #ifdef TWENTYONEPILOTS
        #define NUM_LEDS      (RING_LENGTH+DOT_LENGTH+DOUBLE_STRIP_LENGTH+ITALIC_STRIP_LENGTH)
    #endif
    #ifdef THINGIVERSE
        #define NUM_LEDS      (RING_LENGTH+HORIZONTAL_LENGTH+VERTICAL_LENGTH)
    #endif
    #define PACKET_LENGTH NUM_LEDS

#else
    #ifdef BAND_GROUPING
        #define PACKET_LENGTH (int)((NUM_LEDS/BAND_GROUPING))
    #else
        #define PACKET_LENGTH NUM_LEDS
    #endif
#endif

    // wifi definition
    #include <WiFiManager.h> // https://github.com/tzapu/WiFiManager/tree/development
    WiFiManager wifiManager;
    bool wifiMangerPortalRunning = false;
    bool wifiConnected = false;

    // Misc Params
    #define AVG_ARRAY_SIZE 10
    #define BAND_START 0
    #define BAND_END 3        // can be increased when working with bigger spectrums (40+)
    #define UDP_PORT 4210     // used for UDP visualization

    WiFiUDP Udp;              // used for NTP and visualization
    unsigned int localUdpPort = UDP_PORT;  // local port to listen on
    uint8_t incomingPacket[PACKET_LENGTH + 1];
#endif

// include config management
#include "config.h"

#ifdef ESP8266
ESP8266WebServer webServer(80);
#elif defined(ESP32)
WebServer webServer(80);
#endif

// #include "FSBrowser.h" currently not used
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define FRAMES_PER_SECOND  120  // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.
#define SOUND_REACTIVE_FPS FRAMES_PER_SECOND

#ifdef ENABLE_OTA_SUPPORT
#include <ArduinoOTA.h>
#endif

#ifdef ENABLE_ALEXA_SUPPORT
#if LED_DEBUG != 0
#define ESPALEXA_DEBUG
#endif
#include <Espalexa.h>
void mainAlexaEvent(EspalexaDevice*);
Espalexa espalexa;
#ifdef ESP8266
ESP8266WebServer webServer2(80);
#elif defined(ESP32)
WebServer webServer2(80);
#endif
EspalexaDevice* alexa_main;
#endif // ENABLE_ALEXA_SUPPORT

#ifdef ENABLE_MULTICAST_DNS
#ifdef ESP8266
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <ESPmDNS.h>
#endif //ESP32
#endif // ENABLE_MULTICAST_DNS

#ifdef ENABLE_HOMEY_SUPPORT
#include <Homey.h>              //Athom Homey library
#endif

CRGB leds[NUM_LEDS];

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 5, 32, 64, 128, 255 };
uint8_t brightnessIndex = 3;

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

uint8_t currentPatternIndex = 2; // Index number of which pattern is current
uint8_t previousPatternIndex = 2; // Index number of last pattern
uint8_t autoplay = 0;

uint8_t autoplayDuration = 10;
unsigned long autoPlayTimeout = 0;

uint8_t currentPaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
uint8_t slowHue = 0; // slower gHue
uint8_t verySlowHue = 0; // very slow gHue

CRGB solidColor = CRGB::Blue;

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

// I just don't know why. Anyone an idea?
void IfThisIsRemovedTheScatchWillFailToBuild(void) {};

typedef void(*Pattern)();
typedef Pattern PatternList[];
typedef struct {
    Pattern pattern;
    String name;
    // these settings decide if certain controls/fields are displayed in the web interface
    bool show_palette;
    bool show_speed;
    bool show_color_picker;
    bool show_cooling_sparking;
    bool show_twinkle;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

#include "TwinkleFOX.h"
#include "Twinkles.h"

// List of patterns to cycle through.  Each is defined as a separate function below.

PatternAndNameList patterns = {

    // Time patterns
#if LED_DEVICE_TYPE == 2                             // palet  speed  color  spark  twinkle
    { displayTimeStatic,        "Time",                 true,  true,  true,  false, false},
    { displayTimeColorful,      "Time Colorful",        true,  true,  false, false, false},
    { displayTimeGradient,      "Time Gradient",        true,  true,  false, false, false},
    { displayTimeGradientLarge, "Time Gradient large",  true,  true,  false, false, false},
    { displayTimeRainbow,       "Time Rainbow",         true,  true,  false, false, false},
#endif

#if LED_DEVICE_TYPE == 3                             // palet  speed  color  spark  twinkle
    { pride_Waves,            "Pride Waves",            true,  true,  false, false, false},
    { pride_Rings,            "Pride Rings",            true,  true,  false, false, false},
    { colorWaves_hori,        "Vertical Waves",         true,  true,  false, false, false},
    { colorWaves_vert,        "Color Rings",            true,  true,  false, false, false},
    { rainbow_vert,           "Vertical Rainbow",       true,  true,  false, false, false},
#endif

    // animation patterns                            // palet  speed  color  spark  twinkle
    { pride,                  "Pride",                  false, false, false, false, false},
    { colorWaves,             "Color Waves",            false, false, false, false, false},
    { rainbow,                "Horizontal Rainbow",     false, true,  false, false, false},
    { rainbowSolid,           "Solid Rainbow",          false, true,  false, false, false},
    { confetti,               "Confetti",               false, true,  false, false, false},
    { sinelon,                "Sinelon",                true,  true,  false, false, false},
    { bpm,                    "Beat",                   true,  true,  false, false, false},
    { juggle,                 "Juggle",                 false, true,  false, false, false},
    { fire,                   "Fire",                   false, true,  false, true,  false},
    { water,                  "Water",                  false, true,  false, true,  false},
    { solid_strobe,           "Strobe",                 false, true,  true,  false, false},
    { rainbow_strobe,         "Rainbow Strobe",         false, true,  false, false, false},
    { smooth_rainbow_strobe,  "Smooth Rainbow Strobe",  false, true,  false, false, false},

    // DigitalJohnson patterns                       // palet  speed  color  spark  twinkle
    { rainbowRoll,            "Rainbow Roll",           false, true,  false, false, false},
    { rainbowBeat,            "Rainbow Beat",           false, true,  false, false, false},
    { randomPaletteFades,     "Palette Fades",          true,  true,  false, false, false},
    { rainbowChase,           "Rainbow Chase",          false, true,  false, false, false},
    { randomDots,             "Rainbow Dots",           false, true,  false, false, false},
    { randomFades,            "Rainbow Fades",          false, true,  false, false, false},
    { policeLights,           "Police Lights",          false, true,  false, false, false},
    { glitter,                "Glitter",                false, true,  false, false, false},
    { snowFlakes,             "Snow Flakes",            false, true,  false, false, false},
    { lightning,              "Lightning",              false, false, false, false, false},

    // twinkle patterns                              // palet  speed  color  spark  twinkle
    { paletteTwinkles,        "Palette Twinkles",       true,  true,  false, false, true},
    { snowTwinkles,           "Snow Twinkles",          false, true,  false, false, true},
    { incandescentTwinkles,   "Incandescent Twinkles",  false, true,  false, false, true},

    // TwinkleFOX patterns                                 // palet  speed  color  spark  twinkle
    { retroC9Twinkles,        "Retro C9 Twinkles",            false, true,  false, false, true},
    { redWhiteTwinkles,       "Red & White Twinkles",         false, true,  false, false, true},
    { blueWhiteTwinkles,      "Blue & White Twinkles",        false, true,  false, false, true},
    { redGreenWhiteTwinkles,  "Red, Green & White Twinkles",  false, true,  false, false, true},
    { fairyLightTwinkles,     "Fairy Light Twinkles",         false, true,  false, false, true},
    { snow2Twinkles,          "Snow 2 Twinkles",              false, true,  false, false, true},
    { hollyTwinkles,          "Holly Twinkles",               false, true,  false, false, true},
    { iceTwinkles,            "Ice Twinkles",                 false, true,  false, false, true},
    { partyTwinkles,          "Party Twinkles",               false, true,  false, false, true},
    { forestTwinkles,         "Forest Twinkles",              false, true,  false, false, true},
    { lavaTwinkles,           "Lava Twinkles",                false, true,  false, false, true},
    { fireTwinkles,           "Fire Twinkles",                false, true,  false, false, true},
    { cloud2Twinkles,         "Cloud 2 Twinkles",             false, true,  false, false, true},
    { oceanTwinkles,          "Ocean Twinkles",               false, true,  false, false, true},

#ifdef ENABLE_UDP_VISUALIZATION
    // Visualization Patterns
#if LED_DEVICE_TYPE == 1                  // Matrix                          // palet  speed  color  spark  twinkle
    { RainbowVisualizer,                  "Rainbow Visualization",              true,  true,  false, false, false},
    { SingleColorVisualizer,              "Single Color Visualization",         true,  true,  true,  false, false},
    { RainbowVisualizerDoubleSided,       "Rainbow Visualization Outside",      true,  true,  false, false, false},
    { SingleColorVisualizerDoubleSided,   "Single Color Visualization Outside"  true,  true,  true,  false, false},
    
    #ifdef AddLogoVisualizers
        #if LENGTH == 32 && HEIGHT == 8   // Logo Visualizers
        { HbzVisualizerRainbow,           "Hbz Visualizer Spectrum",            true,  true,  false, false, false},
        { HbzVisualizerWhite,             "Hbz Visualizer",                     true,  true,  false, false, false},
        #endif
    #endif
#endif

  #ifdef LED_DEVICE_TYPE        // Generic Visualization Patterns                // palet  speed  color  spark  twinkle
    { vuMeterSolid,                 "Solid Volume Visualizer",                      true,  true,  false, false, false},
    { vuMeterStaticRainbow,         "Static Rainbow Volume Visualizer",             true,  true,  false, false, false},
    { vuMeterRainbow,               "Flowing Rainbow Volume Visualizer",            true,  true,  false, false, false},
    { vuMeterTriColor,              "Tri-Color Volume Visualizer",                  true,  true,  false, false, false},
    { RefreshingVisualizer,         "Wave Visualizer",                              true,  true,  false, false, false},
    { CentralVisualizer,            "Center Visualizer",                            true,  true,  false, false, false},
    { SolidColorDualTone,           "Solid-Color Pair Bullet Visualizer",           true,  true,  true,  false, false},
    { SolidColorComplementary,      "Solid-Color Complementary Bullet Visualizer",  true,  true,  true,  false, false},
    { BluePurpleBullets,            "Blue/Purple Bullet Visualizer",                true,  true,  false, false, false},
    { BulletVisualizer,             "Beat-Bullet Visualization",                    true,  true,  false, false, false},
    //{ RainbowPeaks,                 "Rainbow Peak Visualizer"},                     // broken
    { RainbowBassRings,             "Bass Ring Visualizer",                         true,  true,  false, false, false},
    { RainbowKickRings,             "Kick Ring Visualizer",                         true,  true,  false, false, false},
    //{ TrailingBulletsVisualizer,    "Trailing Bullet Visualization"},               // obsolete
    //{ BrightnessVisualizer,         "Brightness Visualizer"},                       // broken
    { RainbowBandVisualizer,        "Rainbow Band Visualizer",                      true,  true,  false, false, false},
    { SingleColorBandVisualizer,    "Single Color Band Visualizer",                 true,  true,  true,  false, false},
  #endif

#if LED_DEVICE_TYPE == 4                                                   // palet  speed  color  spark  twinkle
    { NanoleafWaves,                "Nanoleaf Wave Visualizer",               true,  true,  false, false, false},
    { NanoleafBand,                 "Nanoleaf Rainbow Band Visualizer",       true,  true,  false, false, false},
    { NanoleafSingleBand,           "Nanoleaf Solid Color Band Visualizer",   true,  true,  true,  false, false},
#endif
#endif // ENABLE_UDP_VISUALIZATION

#ifdef ENABLE_SERIAL_AMBILIGHT                         // palet  speed  color  spark  twinkle
    { ambilight,                    "⋆Serial Ambilight",  true,  true,  false, false, false},
#endif // ENABLE_SERIAL_AMBILIGHT
#ifdef SOUND_SENSOR_SUPPORT
    { soundReactive,                "Sound Reactive",     true,  true,  false, false, false},
#endif

    { showSolidColor,               "Solid Color",        false, false, true,  false, false}
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

#include "Fields.h"

// ######################## define setup() and loop() ####################

void setup() {
#ifdef ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif
    WiFi.mode(WIFI_STA);    // avoid creating a seperate AP
    Serial.begin(115200);

    delay(100);
    Serial.print("\n\n");

#if LED_TYPE == WS2812 || LED_TYPE == WS2812B || LED_TYPE == WS2811 || LED_TYPE == WS2813 || LED_TYPE == NEOPIXEL
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // WS2812 (Neopixel)
#elif defined CLK_PIN
    FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS); // for APA102 (Dotstar)
#else
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // misc. 3-pin
#endif
    
    FastLED.setDither(false);
    FastLED.setCorrection(CORRECTION);
    FastLED.setBrightness(brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MILLI_AMPS);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // set a default config to be used on config reset
    default_cfg.brightness = brightness;
    default_cfg.currentPatternIndex = currentPatternIndex;
    default_cfg.power = power;
    default_cfg.autoplay = autoplay;
    default_cfg.autoplayDuration = autoplayDuration;
    default_cfg.currentPaletteIndex = currentPaletteIndex;
    default_cfg.speed = speed;

    loadConfig();

    FastLED.setBrightness(brightness);

    //  irReceiver.enableIRIn(); // Start the receiver

    SERIAL_DEBUG_EOL
    SERIAL_DEBUG_LN(F("System Information:"))
    SERIAL_DEBUG_LNF("Version: %s (%s)", VERSION, VERSION_DATE)
    SERIAL_DEBUG_LNF("Heap: %d", system_get_free_heap_size())
    SERIAL_DEBUG_LNF("SDK: %s", system_get_sdk_version())
#ifdef ESP8266
    SERIAL_DEBUG_LNF("Boot Vers: %d", system_get_boot_version())
    SERIAL_DEBUG_LNF("CPU Speed: %d MHz", system_get_cpu_freq())
    SERIAL_DEBUG_LNF("Chip ID: %d", system_get_chip_id())
    SERIAL_DEBUG_LNF("Flash ID: %d", spi_flash_get_id())
    SERIAL_DEBUG_LNF("Flash Size: %dKB", ESP.getFlashChipRealSize())
    SERIAL_DEBUG_LNF("Vcc: %d", ESP.getVcc())
#elif defined(ESP32)
    SERIAL_DEBUG_LNF("CPU Speed: %d MHz", ESP.getCpuFreqMHz())
    SERIAL_DEBUG_LNF("Flash Size: %dKB", ESP.getFlashChipSize())
#endif
    SERIAL_DEBUG_LNF("MAC address: %s", WiFi.macAddress().c_str())
    SERIAL_DEBUG_EOL

#ifdef SOUND_REACTIVE
#if SENSOR_TYPE == 0
    pinMode(SOUND_SENSOR_PIN, INPUT);
#endif
#endif // SOUND_REACTIVE

#if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)           
    pinMode(MOSFET_PIN, OUTPUT);
    digitalWrite(MOSFET_PIN, power == 1 ? MOSFET_LEVEL : !MOSFET_LEVEL);
#endif

    // starting file system
    if (!SPIFFS.begin ()) {
        Serial.println(F("An Error has occurred while mounting SPIFFS"));
        return;
    }

    // setting up Wifi
    String macID = WiFi.macAddress().substring(12, 14) +
        WiFi.macAddress().substring(15, 17);
    macID.toUpperCase();

    String nameString = String(cfg.hostname) + String(" - ") + macID;

    char nameChar[nameString.length() + 1];
    nameString.toCharArray(nameChar, sizeof(nameChar));

    // setup wifiManager
    wifiManager.setHostname(cfg.hostname); // set hostname
    wifiManager.setConfigPortalBlocking(false); // config portal is not blocking (LEDs light up in AP mode)
    wifiManager.setSaveConfigCallback(handleReboot); // after the wireless settings have been saved a reboot will be performed
    #if LED_DEBUG != 0
        wifiManager.setDebugOutput(true);
    #else
        wifiManager.setDebugOutput(false);
    #endif

    //automatically connect using saved credentials if they exist
    //If connection fails it starts an access point with the specified name
    if (wifiManager.autoConnect(nameChar)) {
        Serial.println("INFO: Wi-Fi connected");
    } else {
        Serial.printf("INFO: Wi-Fi manager portal running. Connect to the Wi-Fi AP '%s' to configure your wireless connection\n", nameChar);
        wifiMangerPortalRunning = true;
    }

    // FS debug information
    // THIS NEEDS TO BE PAST THE WIFI SETUP!! OTHERWISE WIFI SETUP WILL BE DELAYED
    #if LED_DEBUG != 0
        SERIAL_DEBUG_LN(F("SPIFFS contents:"))
        #ifdef ESP8266
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            SERIAL_DEBUG_LNF("FS File: %s, size: %lu", dir.fileName().c_str(), dir.fileSize())
        }
        SERIAL_DEBUG_EOL
        FSInfo fs_info;
        SPIFFS.info(fs_info);
        unsigned int totalBytes = fs_info.totalBytes;
        unsigned int usedBytes = fs_info.usedBytes;
        #elif defined(ESP32)
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        while (file) {
            SERIAL_DEBUG_LNF("FS File: %s, size: %lu", file.name(), file.size())
            file = root.openNextFile();
        }
        SERIAL_DEBUG_EOL
        unsigned int totalBytes = SPIFFS.totalBytes();
        unsigned int usedBytes = SPIFFS.usedBytes();
        #endif
        if (usedBytes == 0) {
            SERIAL_DEBUG_LN(F("NO WEB SERVER FILES PRESENT! SEE: https://github.com/NimmLor/esp8266-fastled-iot-webserver/blob/master/Software_Installation.md#32-sketch-data-upload\n"))
        }
        SERIAL_DEBUG_LNF("FS Size: %luKB, used: %luKB, %0.2f%%", \
                          totalBytes, usedBytes, \
                          (float) 100 / totalBytes * usedBytes)
        SERIAL_DEBUG_EOL
    #endif

    // print setup details
    #ifdef ESP8266
    SERIAL_DEBUG_LNF("Arduino Core Version: %s", ARDUINO_ESP8266_RELEASE)
    #elif defined(ESP32) && defined(ARDUINO_ESP32_RELEASE)
    SERIAL_DEBUG_LNF("Arduino Core Version: %s", ARDUINO_ESP32_RELEASE)
    #endif
    SERIAL_DEBUG_LN(F("Enabled Features:"))
    #ifdef ENABLE_MULTICAST_DNS
        SERIAL_DEBUG_LN(F("Feature: mDNS support enabled"))
    #endif
    #ifdef ENABLE_OTA_SUPPORT
        SERIAL_DEBUG_LN(F("Feature: OTA support enabled"))
    #endif
    #ifdef ENABLE_ALEXA_SUPPORT
        SERIAL_DEBUG_LN(F("Feature: Alexa support enabled"))
    #endif
    #ifdef SOUND_SENSOR_SUPPORT
        SERIAL_DEBUG_LN(F("Feature: Sound sensor support enabled"))
    #endif
    #ifdef ENABLE_MQTT_SUPPORT
        SERIAL_DEBUG_LNF("Feature: MQTT support enabled (mqtt version: %s)", String(MQTT_VERSION).c_str())
    #endif
    #ifdef ENABLE_SERIAL_AMBILIGHT
        SERIAL_DEBUG_LN(F("Feature: Serial ambilight support enabled"))
    #endif
    #ifdef ENABLE_UDP_VISUALIZATION
        SERIAL_DEBUG_LN(F("Feature: UDP visualization support enabled"))
    #endif
    #ifdef ENABLE_HOMEY_SUPPORT
        SERIAL_DEBUG_LNF("Feature: Homey support enabled (version: %s)", HOMEYDUINO_VERSION)
    #endif
    SERIAL_DEBUG_EOL

    switch(LED_DEVICE_TYPE) {
        case 0: SERIAL_DEBUG_LN("Configured device type: LED strip (0)") break;
        case 1: SERIAL_DEBUG_LN("Configured device type: LED MATRIX (1)") break;
        case 2: SERIAL_DEBUG_LN("Configured device type: 7-Segment Clock (2)") break;
        case 3: SERIAL_DEBUG_LN("Configured device type: Desk Lamp (3)") break;
        case 4: SERIAL_DEBUG_LN("Configured device type: Nanoleafs (4)") break;
        case 5: SERIAL_DEBUG_LN("Configured device type: Animated Logos (5)") break;
    }

    SERIAL_DEBUG_LNF("NUM_LEDS: %d", NUM_LEDS)
    SERIAL_DEBUG_LNF("BAND_GROUPING: %d", BAND_GROUPING)
    SERIAL_DEBUG_LNF("PACKET_LENGTH: %d", PACKET_LENGTH)

    #ifdef ENABLE_HOMEY_SUPPORT
        //Start Homey library
        Homey.begin(cfg.hostname);
        Homey.setClass("light");
        Homey.addCapability("onoff", homeyLightOnoff);                          //boolean
        Homey.addCapability("dim", homeyLightDim);                              //number 0.00 - 1.00
        Homey.addCapability("light_hue", homeyLightHue);                        //number 0.00 - 1.00
        Homey.addCapability("light_saturation", homeyLightSaturation);          //number 0.00 - 1.00
        Homey.addCapability("speaker_next", homeyNext);                         //boolean
        Homey.addCapability("speaker_prev", homeyPrev);                         //boolean
    
        Homey.setCapabilityValue("onoff", cfg.power);                           //Set initial value
        Homey.setCapabilityValue("dim", getBrightnessMapped(0.0f, 1.0f));       //Set initial value
        Homey.setCapabilityValue("light_hue", getHueMapped(0.0f, 1.0f));        //Set initial value
        Homey.setCapabilityValue("light_saturation", getSatMapped(0.0f, 1.0f)); //Set initial value
    #endif

#ifdef ENABLE_OTA_SUPPORT

    webServer.on("/ota", HTTP_GET, []() {
        IPAddress ip = WiFi.localIP();
        String h = "<font face='arial'><h1> OTA Update Mode</h1>";
        h += "<h2>Procedure: </h3>";
        h += "The UI won't be available until reset.<br>";
        h += "<b>Open your Arduino IDE and select the new PORT in Tools menu and upload the code!</b>";
        h += "<br>Exit OTA mode: <a href=\"http://" + ip.toString() + "/reboot\"); ' value='Reboot'>Reboot</a>";
        h += "</font>";

        webServer.send(200, "text/html", h);
        delay(100);

        ArduinoOTA.setHostname(cfg.hostname);
#ifdef OTA_PASSWORD
        ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
        ArduinoOTA.onStart([]() {
            SPIFFS.end();
            if (ArduinoOTA.getCommand() == U_FLASH) {
                Serial.println("Start updating sketch");
            } else { // U_FS
                Serial.println("Start updating filesystem");
            }

            // NOTE: if updating FS this would be the place to unmount FS using FS.end()
            });
        ArduinoOTA.onEnd([]() {
            Serial.println("\nFinished OTA Update\nRebooting");
            delay(500);
            ESP.restart();
            });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
            });
        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {
                Serial.println("Auth Failed");
            }
            else if (error == OTA_BEGIN_ERROR) {
                Serial.println("Begin Failed");
            }
            else if (error == OTA_CONNECT_ERROR) {
                Serial.println("Connect Failed");
            }
            else if (error == OTA_RECEIVE_ERROR) {
                Serial.println("Receive Failed");
            }
            else if (error == OTA_END_ERROR) {
                Serial.println("End Failed");
            }
            });
        ArduinoOTA.begin();
        delay(100);
        while (1) {
            ArduinoOTA.handle();
            delay(1);
            webServer.handleClient();
            delay(1);
        }
        }); // GET /ota
#endif

#ifdef ENABLE_ALEXA_SUPPORT
#ifdef ALEXA_DEVICE_NAME
    alexa_main = new EspalexaDevice(ALEXA_DEVICE_NAME, mainAlexaEvent, EspalexaDeviceType::color);
#else
    alexa_main = new EspalexaDevice(cfg.hostname, mainAlexaEvent, EspalexaDeviceType::color);
#endif
    espalexa.addDevice(alexa_main);
#ifdef AddAutoplayDevice
    espalexa.addDevice(AddAutoplayDevice, AlexaAutoplayEvent, EspalexaDeviceType::onoff); //non-dimmable device
#endif
#ifdef AddStrobeDevice
    espalexa.addDevice(AddStrobeDevice, AlexaStrobeEvent, EspalexaDeviceType::color); //non-dimmable device
#endif
#ifdef AddSpecificPatternDeviceA
    espalexa.addDevice(AddSpecificPatternDeviceA, AlexaSpecificEventA, EspalexaDeviceType::onoff); //non-dimmable device
#endif
#ifdef AddSpecificPatternDeviceB
    espalexa.addDevice(AddSpecificPatternDeviceB, AlexaSpecificEventB, EspalexaDeviceType::onoff); //non-dimmable device
#endif
#ifdef AddSpecificPatternDeviceC
    espalexa.addDevice(AddSpecificPatternDeviceC, AlexaSpecificEventC, EspalexaDeviceType::onoff); //non-dimmable device
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

    addRebootPage(0);

    webServer.on("/alexa", HTTP_GET, []() {
        IPAddress ip = WiFi.localIP();
        String h = "<font face='arial'><h1> Alexa pairing mode</h1>";
        h += "<h2>Procedure: </h3>";
        h += "The webserver will reboot and the UI won't be available.<br>";
        h += "<b>Now. Say to Alexa: 'Alexa, discover devices'.<b><br><br>";
        h += "Alexa should tell you that it found a new device, if it did reset the esp8266 to return to the normal mode.";
        h += "<br>Exit pairing mode: <a href=\"http://" + ip.toString() + "/reboot\"); ' value='Reboot'>Reboot</a>";
        h += "</font>";

        webServer.send(200, "text/html", h);
        webServer2.on("/alexa", [&]() {webServer2.send(200, "text/html", h); });
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
        addRebootPage(2);
        delay(100);
        webServer.stop();
        delay(500);
        webServer.close();
        delay(500);
        espalexa.begin(&webServer2);
        delay(100);
        while (1)
        {
            espalexa.loop();
            delay(1);
        }
        });
#else
    addRebootPage(0);
#endif

    webServer.on("/config.json", HTTP_GET, []() {
        String json = getFieldsJson(fields, fieldCount);
        json += ",{\"name\":\"lines\",\"label\":\"Amount of Lines for the Visualizer\",\"type\":\"String\",\"value\":";
        json += PACKET_LENGTH;
        json += "}";
        json += ",{\"name\":\"settings\",\"label\":\"Device settings\",\"type\":\"Setting\",\"value\":";
        json += "{\"deviceHostname\":\"" + String(cfg.hostname) + "\"";
        json += ",\"otaSupport\":";
#ifdef ENABLE_OTA_SUPPORT
        json += "true";
#else
        json += "false";
#endif
        json += ", \"alexaSupport\":";
#ifdef ENABLE_ALEXA_SUPPORT
        json += "true";
#else
        json += "false";
#endif
        json += ", \"mqttSupport\":";
#ifdef ENABLE_MQTT_SUPPORT
        json += "true";
#else
        json += "false";
#endif
#ifdef ENABLE_MQTT_SUPPORT
        json += ",\"mqttEnabled\":" + String(cfg.MQTTEnabled);
        json += ",\"mqttHostname\":\"" + String(cfg.MQTTHost) + "\"";
        json += ",\"mqttPort\":\"" + String(cfg.MQTTPort) + "\"";
        json += ",\"mqttUsername\":\"" + String(cfg.MQTTUser) + "\"";
        json += ",\"mqttTopic\":\"" + String(cfg.MQTTTopic) + "\"";
        json += ",\"mqttSetTopic\":\"" + String(cfg.MQTTSetTopic) + "\"";
        json += ",\"mqttDevicename\":\"" + String(cfg.MQTTDeviceName) + "\"";
#endif
        json += "}}]";
        webServer.send(200, "application/json", json);
        });

    webServer.on("/settings", []() {

        bool force_restart = false;

        String ssid = webServer.arg("ssid");
        String password = webServer.arg("password");

        if (ssid.length() != 0 && password.length() != 0) {
            setWiFiConf(ssid, password);
            force_restart = true;
        }

        String new_hostname = webServer.arg("hostname");

        if (new_hostname.length() != 0 && String(cfg.hostname) != new_hostname) {
            setHostname(new_hostname);
            force_restart = true;
        }

#ifdef ENABLE_MQTT_SUPPORT
        uint8_t mqtt_enabled = uint8_t(webServer.arg("mqtt-enabled").toInt());
        String mqtt_hostname = webServer.arg("mqtt-hostname");
        uint16_t mqtt_port = uint16_t(webServer.arg("mqtt-port").toInt());
        String mqtt_username = webServer.arg("mqtt-user");
        String mqtt_password = webServer.arg("mqtt-password");
        String mqtt_topic = webServer.arg("mqtt-topic");
        String mqtt_set_topic = webServer.arg("mqtt-set-topic");
        String mqtt_device_name = webServer.arg("mqtt-device-name");

        if (cfg.MQTTEnabled != mqtt_enabled) {
            cfg.MQTTEnabled = mqtt_enabled;
            setConfigChanged();
        }
        if (cfg.MQTTPort != mqtt_port) {
            cfg.MQTTPort = mqtt_port;
            force_restart = true;
        }
        if (mqtt_hostname.length() > 0 && String(cfg.MQTTHost) != mqtt_hostname) {
            mqtt_hostname.toCharArray(cfg.MQTTHost, sizeof(cfg.MQTTHost));
            force_restart = true;
        }
        if (mqtt_username.length() > 0 && String(cfg.MQTTUser) != mqtt_username) {
            mqtt_username.toCharArray(cfg.MQTTUser, sizeof(cfg.MQTTUser));
            force_restart = true;
        }
        if (mqtt_password.length() > 0 && String(cfg.MQTTPass) != mqtt_password) {
            mqtt_password.toCharArray(cfg.MQTTPass, sizeof(cfg.MQTTPass));
            force_restart = true;
        }
        if (mqtt_topic.length() > 0 && String(cfg.MQTTTopic) != mqtt_topic) {
            mqtt_topic.toCharArray(cfg.MQTTTopic, sizeof(cfg.MQTTTopic));
            force_restart = true;
        }
        if (mqtt_set_topic.length() > 0 && String(cfg.MQTTSetTopic) != mqtt_set_topic) {
            mqtt_set_topic.toCharArray(cfg.MQTTSetTopic, sizeof(cfg.MQTTSetTopic));
            force_restart = true;
        }
        if (mqtt_device_name.length() > 0 && String(cfg.MQTTDeviceName) != mqtt_device_name) {
            mqtt_device_name.toCharArray(cfg.MQTTDeviceName, sizeof(cfg.MQTTDeviceName));
            force_restart = true;
        }
#endif
        if (force_restart) {
            SERIAL_DEBUG_LN("Saving settings and rebooting...")
            saveConfig(true);
            handleReboot();
        } else {
            webServer.send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"0; url=/settings.htm\"/></head><body></body>");
        }
        });

    webServer.on("/reset", HTTP_POST, []() {

        // delete EEPROM settings
        if (webServer.arg("type") == String("all")) {
            resetConfig();
            SERIAL_DEBUG_LN("Resetting config")
        }

        // delete wireless config
        if (webServer.arg("type") == String("wifi") || webServer.arg("type") == String("all")) {
            setWiFiConf(String(""), String(""));
            SERIAL_DEBUG_LN("Resetting wifi settings");
        }
        webServer.send(200, "text/html", "<html><head></head><body><font face='arial'><b><h2>Config reset finished. Device is rebooting now and you need to connect to the wireless again.</h2></b></font></body></html>");
        delay(500);
        ESP.restart();
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

    webServer.on("/power", []() {
        String value = webServer.arg("value");
        value.toLowerCase();
        if (value == String("1") || value == String("on")) {
            setPower(1);
        } else if (value == String("0") || value == String("off")) {
            setPower(0);
        } else if (value == String("toggle")) {
            setPower((power == 1) ? 0 : 1);
        }
        sendInt(power);
        });

    webServer.on("/cooling", []() {
        String value = webServer.arg("value");
        cooling = value.toInt();
        broadcastInt("cooling", cooling);
        sendInt(cooling);
        });

    webServer.on("/sparking", []() {
        String value = webServer.arg("value");
        sparking = value.toInt();
        broadcastInt("sparking", sparking);
        sendInt(sparking);
        });

    webServer.on("/speed", []() {
        String value = webServer.arg("value");
        setSpeed(value.toInt());
        sendInt(speed);
        });

    webServer.on("/twinkleDensity", []() {
        String value = webServer.arg("value");
        twinkleDensity = value.toInt();
        SERIAL_DEBUG_LNF("Setting: twinkle density %d", twinkleDensity)
        broadcastInt("twinkleDensity", twinkleDensity);
        sendInt(twinkleDensity);
        });

    webServer.on("/solidColor", []() {
        String r = webServer.arg("r");
        String g = webServer.arg("g");
        String b = webServer.arg("b");
        setSolidColor(r.toInt(), g.toInt(), b.toInt(), false);
#ifdef ENABLE_ALEXA_SUPPORT
        alexa_main->setColor(r.toInt(), g.toInt(), b.toInt());
#endif
        sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
        });

    webServer.on("/hue", []() {
        String value = webServer.arg("value");
        setSolidColorHue(value.toInt(), false);
#ifdef ENABLE_ALEXA_SUPPORT
        alexa_main->setColor(solidColor.r, solidColor.g, solidColor.b);
#endif
        sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
        });

    webServer.on("/saturation", []() {
        String value = webServer.arg("value");
        setSolidColorSat(value.toInt(), false);
#ifdef ENABLE_ALEXA_SUPPORT
        alexa_main->setColor(solidColor.r, solidColor.g, solidColor.b);
#endif
        sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
        });

    webServer.on("/pattern", []() {
        String value = webServer.arg("value");
        #if LED_DEVICE_TYPE == 2
        switchedTimePattern = true;
        #endif
        setPattern(value.toInt());
        sendInt(currentPatternIndex);
        });

    webServer.on("/patternName", []() {
        String value = webServer.arg("value");
        setPatternName(value);
        sendInt(currentPatternIndex);
        });

    webServer.on("/palette", []() {
        String value = webServer.arg("value");
        setPalette(value.toInt());
        sendInt(currentPaletteIndex);
        });

    webServer.on("/paletteName", []() {
        String value = webServer.arg("value");
        setPaletteName(value);
        sendInt(currentPaletteIndex);
        });

    webServer.on("/brightness", []() {
        String value = webServer.arg("value");
        setBrightness(value.toInt());
#ifdef ENABLE_ALEXA_SUPPORT
        alexa_main->setValue(brightness);
#endif
        sendInt(brightness);
        });

    webServer.on("/autoplay", []() {
        String value = webServer.arg("value");
        value.toLowerCase();
        if (value == String("1") || value == String("on")) {
            setAutoplay(1);
        } else if (value == String("0") || value == String("off")) {
            setAutoplay(0);
        } else if (value == String("toggle")) {
            setAutoplay((autoplay == 1) ? 0 : 1);
        }
        sendInt(autoplay);
        });

    webServer.on("/autoplayDuration", []() {
        String value = webServer.arg("value");
        setAutoplayDuration(value.toInt());
        sendInt(autoplayDuration);
        });


    //list directory
    /* // Currently no directory/file functions are used
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
        */
    webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");

#ifdef ENABLE_ALEXA_SUPPORT
    espalexa.begin(&webServer);
#endif
#ifndef ENABLE_ALEXA_SUPPORT
    webServer.begin();
#endif

    Serial.println("INFO: HTTP web server started");

#if LED_DEVICE_TYPE == 2
    udpTime.begin(localPortTime);
#endif

#ifdef ENABLE_UDP_VISUALIZATION
    Udp.begin(localUdpPort);
#endif // ENABLE_UDP_VISUALIZATION

    autoPlayTimeout = millis() + (autoplayDuration * 1000);
}

void loop() {

    static unsigned int loop_counter = 0;
    static unsigned int current_fps = FRAMES_PER_SECOND;
    static unsigned int frame_delay = (1000 / FRAMES_PER_SECOND) * 1000; // in micro seconds

    // insert a delay to keep the framerate modest
    // delayMicroseconds max value is 16383
    if (frame_delay < 16000){
        delayMicroseconds(frame_delay);
    } else {
        delay(frame_delay / 1000);
    }

    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(65535));

#ifdef ENABLE_ALEXA_SUPPORT
    espalexa.loop();
#else
    webServer.handleClient();
#endif

    if (wifiMangerPortalRunning) {
        wifiManager.process();
    }

#ifdef ENABLE_HOMEY_SUPPORT
    Homey.loop();
#endif

    EVERY_N_SECONDS(1) {
        int currentWifiStatus = wifiManager.getLastConxResult();

        if (currentWifiStatus != WL_CONNECTED && !wifiMangerPortalRunning) {
            SERIAL_DEBUG_LN("Trying to connect to Wifi")
            wifiConnected = false;
        }
        if (currentWifiStatus == WL_CONNECTED && !wifiConnected) {
            wifiConnected = true;
            Serial.print("INFO: WiFi Connected! Open http://");
            Serial.print(WiFi.localIP());
            Serial.println(" in your browser");
#ifdef ENABLE_MULTICAST_DNS
            if (!MDNS.begin(cfg.hostname)) {
                Serial.println("\nERROR: problem while setting up MDNS responder! \n");
            } else {
                Serial.printf("INFO: mDNS responder started. Try to open http://%s.local in your browser\n", cfg.hostname);
                MDNS.addService("http", "tcp", 80);
            }
#endif
        }
#if defined(ENABLE_MULTICAST_DNS) && defined(ESP8266)
        MDNS.update();
#endif // ENABLE_MULTICAST_DNS
    }

#ifdef ENABLE_MQTT_SUPPORT
    static bool mqttConnected = false;

    if (cfg.MQTTEnabled == 1)
        mqttClient.loop();
    else
        mqttConnected = false;

    EVERY_N_SECONDS(10) {
        if (!mqttClient.connected() && cfg.MQTTEnabled != 0) {
            mqttClient.setServer(cfg.MQTTHost, cfg.MQTTPort);
            mqttClient.setCallback(mqttCallback);
            mqttConnected = false;
        }
        if (!mqttConnected && cfg.MQTTEnabled != 0) {
            mqttConnected = true;
            SERIAL_DEBUG_BOL
            SERIAL_DEBUG_ADD("Connecting to MQTT...");
            if (mqttClient.connect(cfg.hostname, cfg.MQTTUser, cfg.MQTTPass)) {
                mqttClient.setKeepAlive(10);
                SERIAL_DEBUG_ADD("connected\n")

                SERIAL_DEBUG_LN("Subscribing to MQTT Topics");
                char mqttSetTopicC[129];
                strlcpy(mqttSetTopicC, cfg.MQTTTopic, sizeof(mqttSetTopicC));
                strlcat(mqttSetTopicC, cfg.MQTTSetTopic, sizeof(mqttSetTopicC));
                mqttClient.subscribe(mqttSetTopicC);

                char mqttSetTopicS[66];
                strcpy(mqttSetTopicS, "~");
                strlcat(mqttSetTopicS, cfg.MQTTSetTopic, sizeof(mqttSetTopicS));

                DynamicJsonDocument JSONencoder(4096);
                    JSONencoder["~"] = cfg.MQTTTopic,
                    JSONencoder["name"] = cfg.MQTTDeviceName,
                    JSONencoder["dev"]["ids"] = MQTT_UNIQUE_IDENTIFIER,
                    JSONencoder["dev"]["mf"] = "Surrbradl08",
                    JSONencoder["dev"]["mdl"] = VERSION,
                    JSONencoder["dev"]["name"] = cfg.MQTTDeviceName,
                    JSONencoder["stat_t"] = "~",
                    JSONencoder["cmd_t"] = mqttSetTopicS,
                    JSONencoder["brightness"] = true,
                    JSONencoder["rgb"] = true,
                    JSONencoder["effect"] = true,
                    JSONencoder["uniq_id"] = MQTT_UNIQUE_IDENTIFIER,
                    JSONencoder["schema"] = "json";

                JsonArray effect_list = JSONencoder.createNestedArray("effect_list");
                for (uint8_t i = 0; i < patternCount; i++) {
                    effect_list.add(patterns[i].name);
                }
                size_t n = measureJson(JSONencoder);
                char mqttConfigTopic[85];
                strlcat(mqttConfigTopic, cfg.MQTTTopic, sizeof(mqttConfigTopic));
                strcat(mqttConfigTopic, "/config");
                if (mqttClient.beginPublish(mqttConfigTopic, n, true) == true) {
                    SERIAL_DEBUG_LN("Configuration Publishing Begun")
                    if (serializeJson(JSONencoder, mqttClient) == n){
                        SERIAL_DEBUG_LN("Configuration Sent")
                    }
                    if (mqttClient.endPublish() == true) {
                        SERIAL_DEBUG_LN("Configuration Publishing Finished")
                        mqttSendStatus();
                        SERIAL_DEBUG_LN("Sending Initial Status")
                    }
                } else {
                    SERIAL_DEBUG_LN("Error sending Configuration")
                }
            } else {
                SERIAL_DEBUG_ADDF("failed with state %s\n", mqttClient.state())
            }
        }
    }

    EVERY_N_SECONDS(90) {
        mqttSendStatus();
    }
#endif

    EVERY_N_SECONDS(10) {
      SERIAL_DEBUG_LNF("Heap: %d", system_get_free_heap_size())
    }

    // change to a new cpt-city gradient palette
    EVERY_N_SECONDS(secondsPerPalette) {
        gCurrentPaletteNumber = addmod8(gCurrentPaletteNumber, 1, gGradientPaletteCount);
        gTargetPalette = gGradientPalettes[gCurrentPaletteNumber];
    }

    EVERY_N_MILLISECONDS(40) {
        // slowly blend the current palette to the next
        nblendPaletteTowardPalette(gCurrentPalette, gTargetPalette, 8);
    }

    updateHue();

    if (autoplay && (millis() > autoPlayTimeout)) {
        adjustPattern(true);
        autoPlayTimeout = millis() + (autoplayDuration * 1000);
    }

    if (power == 0) {
        fadeToBlackBy(leds, NUM_LEDS, 5);
    } else {
        // Call the current pattern function once, updating the 'leds' array
        patterns[currentPatternIndex].pattern();
    }

    FastLED.show();

    // init time
    // FIXME: use this to keep time updated. Don't rely on pattern to do this.
#if LED_DEVICE_TYPE == 2
    EVERY_N_MILLISECONDS(200) {
        if (wifiConnected && ntp_timestamp == 0) {
            GetTime();
        }
    }
#endif

    // call to save config if config has changed
    saveConfig();

    // every second calculate the FPS and adjust frame delay to keep FPS smooth
    EVERY_N_SECONDS(1) {
        current_fps = loop_counter;
        // frame delay stepping: 50 us
        // fps sliding window +/- 1 frame
        // too fast, we need to slow down. Don't increase the frame delay past 20 ms
        if (current_fps > FRAMES_PER_SECOND + 1 && frame_delay <= 20000) {
            int factor = current_fps - FRAMES_PER_SECOND; // factor for faster speed adjustment
            if (factor < 1) factor = 1;
            frame_delay += (50 * factor);

        // too slow, we need to speed up a little bit
        } else if (current_fps < FRAMES_PER_SECOND - 1 && frame_delay > 0) {
            int factor = FRAMES_PER_SECOND - current_fps;
            if (factor < 1) factor = 1;

            if (frame_delay < (50 * factor)) {
                frame_delay = 0;
            } else {
                frame_delay -= (50 * factor);
            }
        }
        SERIAL_DEBUG_LNF("Stats: %lu frames/s, frame delay: %d us", current_fps, frame_delay)
        loop_counter = 0;
    }
    loop_counter += 1;
    previousPatternIndex = currentPatternIndex;
}

void loadConfig() {

    SERIAL_DEBUG_LN(F("Loading config"))

    // Loads configuration from EEPROM into RAM
    EEPROM.begin(4095);
    EEPROM.get(0, cfg );
    EEPROM.end();

    brightness = cfg.brightness;

    currentPatternIndex = cfg.currentPatternIndex;
    if (currentPatternIndex < 0)
        currentPatternIndex = 0;
    else if (currentPatternIndex >= patternCount)
        currentPatternIndex = patternCount - 1;

    byte r = cfg.red;
    byte g = cfg.green;
    byte b = cfg.blue;

    if (r != 0 && g != 0 && b != 0) {
        solidColor = CRGB(r, g, b);
    }

    power = cfg.power;

    autoplay = cfg.autoplay;
    autoplayDuration = cfg.autoplayDuration;

    currentPaletteIndex = cfg.currentPaletteIndex;
    if (currentPaletteIndex < 0)
        currentPaletteIndex = 0;
    else if (currentPaletteIndex >= paletteCount)
        currentPaletteIndex = paletteCount - 1;

    speed = cfg.speed;
    twinkleSpeed = map(speed, 0, 255, 0, 8);

    if (!isValidHostname(cfg.hostname, sizeof(cfg.hostname))) {
        strncpy(cfg.hostname, DEFAULT_HOSTNAME, sizeof(cfg.hostname));
        setConfigChanged();
    }

#ifdef ENABLE_MQTT_SUPPORT
    // fall back to default settings if hostname is invalid
    if (!isValidHostname(cfg.MQTTHost, sizeof(cfg.MQTTHost))) {
        cfg.MQTTEnabled = MQTT_ENABLED;
        strncpy(cfg.MQTTHost, MQTT_HOSTNAME, sizeof(cfg.MQTTHost));
        cfg.MQTTPort = uint16_t(MQTT_PORT);
        strncpy(cfg.MQTTUser, MQTT_USER, sizeof(cfg.MQTTUser));
        strncpy(cfg.MQTTPass, MQTT_PASS, sizeof(cfg.MQTTPass));
        strncpy(cfg.MQTTTopic, MQTT_TOPIC, sizeof(cfg.MQTTTopic));
        strncpy(cfg.MQTTSetTopic, MQTT_TOPIC_SET, sizeof(cfg.MQTTSetTopic));
        strncpy(cfg.MQTTDeviceName, MQTT_DEVICE_NAME, sizeof(cfg.MQTTDeviceName));
        setConfigChanged();
    }
#endif
}

// ######################## web server functions #########################

String getRebootString() {
    return "<html><head><meta http-equiv=\"refresh\" content=\"4; url=/\"/></head><body><font face='arial'><b><h2>Rebooting... returning in 4 seconds</h2></b></font></body></html>";
}

void handleReboot() {
    webServer.send(200, "text/html", getRebootString());
    delay(500);
    ESP.restart();
}

#ifdef ENABLE_ALEXA_SUPPORT
void handleReboot2() {
    webServer2.send(200, "text/html", getRebootString());
    delay(500);
    ESP.restart();
}
#endif // ENABLE_ALEXA_SUPPORT

void addRebootPage(int webServerNr) {
    if (webServerNr < 2) {
        webServer.on("/reboot", handleReboot);
    }
    #ifdef ENABLE_ALEXA_SUPPORT
    else if (webServerNr == 2) {
        webServer2.on("/reboot", handleReboot2);
    }
    #endif // ENABLE_ALEXA_SUPPORT
}

void sendInt(uint8_t value) {
    sendString(String(value));
}

void sendString(String value) {
    webServer.send(200, "text/plain", value);
}

// These are old functions from previous websocket implementation
// but we keep then as this could be still used in the future
void broadcastInt(String name, uint8_t value) {
    //String json = "{\"name\":\"" + name + "\",\"value\":" + String(value) + "}";
    #ifdef ENABLE_MQTT_SUPPORT
        mqttSendStatus();
    #endif
}

void broadcastString(String name, String value) {
    //String json = "{\"name\":\"" + name + "\",\"value\":\"" + String(value) + "\"}";
    #ifdef ENABLE_MQTT_SUPPORT
        mqttSendStatus();
    #endif
}

// ############## functions to update current settings ###################

void setSolidColor(uint8_t r, uint8_t g, uint8_t b, bool updatePattern)
{
    solidColor = CRGB(r, g, b);

    cfg.red = r;
    cfg.green = g;
    cfg.blue = b;
    setConfigChanged();

    if (updatePattern && currentPatternIndex != patternCount - 2)setPattern(patternCount - 1);

    SERIAL_DEBUG_LNF("Setting: solid Color: red %d, green %d, blue %d", r, g ,b)
    broadcastString("color", String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
}

void setSolidColor(CRGB color, bool updatePattern)
{
    setSolidColor(color.r, color.g, color.b, updatePattern);
}

void setSolidColorHue(uint8_t hue, bool updatePattern)
{
    CRGB color = solidColor;
    CHSV temp_chsv = rgb2hsv_approximate(color);
    temp_chsv.hue = hue;
    hsv2rgb_rainbow(temp_chsv, color);
    setSolidColor(color.r, color.g, color.b, updatePattern);
}

void setSolidColorSat(uint8_t sat, bool updatePattern)
{
    CRGB color = solidColor;
    CHSV temp_chsv = rgb2hsv_approximate(color);
    temp_chsv.sat = sat;
    hsv2rgb_rainbow(temp_chsv, color);
    setSolidColor(color.r, color.g, color.b, updatePattern);
}

void setPower(uint8_t value)
{
    power = value == 0 ? 0 : 1;

    cfg.power = power;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: power %s", (power == 0) ? "off" : "on")
    broadcastInt("power", power);

    #if defined(MOSFET_PIN) && defined(MOSFET_LEVEL)
        digitalWrite(MOSFET_PIN, power == 1 ? MOSFET_LEVEL : !MOSFET_LEVEL);
    #endif
}

void setAutoplay(uint8_t value)
{
    autoplay = value == 0 ? 0 : 1;

    cfg.autoplay = autoplay;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: autoplay %s", (autoplay == 0) ? "off" : "on")
    broadcastInt("autoplay", autoplay);
}

void setAutoplayDuration(uint8_t value)
{
    autoplayDuration = value;

    cfg.autoplayDuration = autoplayDuration;
    setConfigChanged();

    autoPlayTimeout = millis() + (autoplayDuration * 1000);
    SERIAL_DEBUG_LNF("Setting: autoplay duration: %d seconds", autoplayDuration)
    broadcastInt("autoplayDuration", autoplayDuration);
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
    if (autoplay == 1) {
#ifdef RANDOM_AUTOPLAY_PATTERN
        uint8_t lastpattern = currentPatternIndex;
        while (currentPatternIndex == lastpattern)
        {
            uint8_t newpattern = random8(0, patternCount - 1);
            if (newpattern != lastpattern) currentPatternIndex = newpattern;
        }
#else // RANDOM_AUTOPLAY_PATTERN
        currentPatternIndex++;
#endif
    }

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
    if (currentPatternIndex >= patternCount)
        currentPatternIndex = 0;

    if (autoplay == 0) {
        cfg.currentPatternIndex = currentPatternIndex;
        setConfigChanged();
    }

#ifdef AUTOPLAY_IGNORE_UDP_PATTERNS
    if (autoplay == 1)
    {
        if (((String)patterns[currentPatternIndex].name).indexOf("Visual") > 0) adjustPattern(true);    // new pattern if it is a udp pattern
        else if (((String)patterns[currentPatternIndex].name).indexOf("Serial") > 0) adjustPattern(true);    // new pattern if it is a serial pattern
    }
#endif

    SERIAL_DEBUG_LNF("Setting: pattern: %s", patterns[currentPatternIndex].name.c_str())

    broadcastInt("pattern", currentPatternIndex);
}

void setPattern(uint8_t value)
{
    if (value >= patternCount)
        value = patternCount - 1;

    currentPatternIndex = value;

    if (autoplay != 1) {
        cfg.currentPatternIndex = currentPatternIndex;
        setConfigChanged();
    }

    SERIAL_DEBUG_LNF("Setting: pattern: %s", patterns[currentPatternIndex].name.c_str())

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

    cfg.currentPaletteIndex = currentPaletteIndex;
    setConfigChanged();

    SERIAL_DEBUG_LNF("Setting: pallette: %s", paletteNames[currentPaletteIndex].c_str())
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

    setBrightness(brightnessMap[brightnessIndex]);
}

void setBrightness(uint8_t value)
{
    if (value > 255)
        value = 255;
    else if (value < 0) value = 0;

    brightness = value;

    FastLED.setBrightness(brightness);

    cfg.brightness = brightness;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: brightness: %d", brightness)
    broadcastInt("brightness", brightness);
}

void setSpeed(uint8_t value)
{
    if (value > 255)
        value = 255;
    else if (value < 0) value = 0;

    speed = value;

    twinkleSpeed = map(speed, 0, 255, 0, 8);

    cfg.speed = speed;
    setConfigChanged();
    SERIAL_DEBUG_LNF("Setting: speed: %d", speed)
    broadcastInt("speed", speed);
}

// genric functions to map current values to desired range
float getBrightnessMapped(float min, float max) {
    return mapfloat((float) brightness, 0.0, 255.0, min, max);
}
uint8_t getBrightnessMapped(uint8_t min, uint8_t max) {
    return map(brightness, 0, 255, min, max);
}
float getHueMapped(float min, float max) {
    return mapfloat(rgb2hsv_approximate(solidColor).hue, 0.0, 255.0, min, max);
}
uint8_t getHueMapped(uint8_t min, uint8_t max) {
    return map(rgb2hsv_approximate(solidColor).hue, 0, 255, min, max);
}
float getSatMapped(float min, float max) {
    return map(rgb2hsv_approximate(solidColor).sat, 0.0, 255.0, min, max);
}
uint8_t getSatMapped(uint8_t min, uint8_t max) {
    return map(rgb2hsv_approximate(solidColor).sat, 0, 255, min, max);
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ######################### pattern functions ###########################

void updateHue()
{
    uint8_t hueUpdateInterval = 40;
    uint8_t hueStep = 1;
    static unsigned long nextHueUpdate = millis();

    // adds speed control for some Rainbow patterns
    if (patterns[currentPatternIndex].name == String("Horizontal Rainbow") or \
        patterns[currentPatternIndex].name == String("Solid Rainbow") or \
        patterns[currentPatternIndex].name == String("Rainbow Roll")) {

        if (speed < 128) {
            hueUpdateInterval = map(speed, 0, 255, 100, 0);
        } else {
            hueUpdateInterval = map(speed, 0, 255, 200, 0);
            hueStep = 2;
        }
    }

    if (millis() > nextHueUpdate) {
        gHue += hueStep;  // slowly cycle the "base color" through the rainbow
        if (gHue % 16 == 0)slowHue++;
        if (gHue % 127 == 0)verySlowHue++;
        nextHueUpdate = millis() + hueUpdateInterval;
    }
}

bool updatePatternBasedOnSpeedSetting(uint8_t max_delay)
{
    uint8_t updateInterval = 0;
    static unsigned long nexUpdate = millis();

    updateInterval = map(speed, 0, 255, max_delay, 0);

    if (millis() > nexUpdate) {
        nexUpdate = millis() + updateInterval;
        return true;
    }

    return false;
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
    fill_solid(leds, NUM_LEDS, solidColor);
}

// Patterns from FastLED example DemoReel100: https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino

void smooth_rainbow_strobe()
{
    if (autoplay == 1)adjustPattern(true);
    uint8_t beat = beatsin8(speed, 0, 255);
    fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, beat));
}

void strobe(bool rainbow)
{
    if (autoplay == 1)adjustPattern(true);
    static bool p = false;
    static long lm = 0;
    if (millis() - lm > (128 - (speed / 2)))
    {
        if (p) fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
        else {
            if (rainbow) {
                fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
            } else {
                fill_solid(leds, NUM_LEDS, solidColor);
            }
        }
        lm = millis();
        p = !p;
    }
}

void rainbow_strobe()
{
    strobe(true);
}

void solid_strobe()
{
    strobe(false);
}

void rainbow()
{
#if LED_DEVICE_TYPE == 4
    for (int i = 0; i < LEAFCOUNT; i++)
    {
        uint8_t myHue = (gHue + i * (255 / LEAFCOUNT));
        gHue = gHue > 255 ? gHue - 255 : gHue;
        fill_solid(leds + i * PIXELS_PER_LEAF, PIXELS_PER_LEAF, CHSV(myHue, 255, 255));
    }
#else
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
#endif
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
    if (updatePatternBasedOnSpeedSetting(100) == false)
        return;

    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, NUM_LEDS, 10);
#if LED_DEVICE_TYPE == 4
    int pos = random16(LEAFCOUNT * 3);
    int val = gHue + random8(64);
    for (int i = 0; i < (PIXELS_PER_LEAF / 3); i++)
    {

        leds[i + pos * (PIXELS_PER_LEAF / 3)] += ColorFromPalette(palettes[currentPaletteIndex], val);
    }
#else
    int pos = random16(NUM_LEDS);
    // leds[pos] += CHSV( gHue + random8(64), 200, 255);
    leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue + random8(64));
#endif
}

void sinelon()
{
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(speed / 4, 0, NUM_LEDS);
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
#if LED_DEVICE_TYPE == 4
    for (int i = 0; i < LEAFCOUNT; i++) {
        for (int i2 = 0; i2 < PIXELS_PER_LEAF; i2++)leds[i * PIXELS_PER_LEAF + i2] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
#else
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
#endif
}

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

    if (updatePatternBasedOnSpeedSetting(100) == false)
        return;

    if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
        lastSecond = secondHand;
        switch (secondHand) {
        //case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
        case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
        case 20: numdots = 8; basebeat = 5; hueinc = 0; faderate = 8; thishue = random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
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
#if LED_DEVICE_TYPE == 4
    for (uint16_t i = 0; i < (LEAFCOUNT * 3); i++) {
#else
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
#endif
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
#if LED_DEVICE_TYPE == 4
        pixelnumber = ((LEAFCOUNT * 3) - 1) - pixelnumber;
        for (int i2 = 0; i2 < (PIXELS_PER_LEAF / 3); i2++)
        {
            nblend(leds[pixelnumber * (PIXELS_PER_LEAF / 3) + i2], newcolor, 64);
        }
#else
        pixelnumber = (NUM_LEDS - 1) - pixelnumber;
        nblend(leds[pixelnumber], newcolor, 64);
#endif
    }
}

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
    if (updatePatternBasedOnSpeedSetting(50) == false)
        return;

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

uint8_t beatsaw8(accum88 beats_per_minute, uint8_t lowest, uint8_t highest,
    uint32_t timebase, uint8_t phase_offset)
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
#if LED_DEVICE_TYPE == 4
    colorwaves(leds, LEAFCOUNT * 3, gCurrentPalette);
#else
    colorwaves(leds, NUM_LEDS, gCurrentPalette);
#endif
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
#if LED_DEVICE_TYPE == 4
        pixelnumber = ((LEAFCOUNT * 3) - 1) - pixelnumber;
        for (int i2 = 0; i2 < (PIXELS_PER_LEAF / 3); i2++)
        {
            nblend(leds[pixelnumber * (PIXELS_PER_LEAF / 3) + i2], newcolor, 128);
        }
#else
        pixelnumber = (numleds - 1) - pixelnumber;

        nblend(ledarray[pixelnumber], newcolor, 128);
#endif
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



//########################### Patterns by DigitalJohnson ###########################

TBlendType    blendType;
TBlendType currentBlending; // Current blending type

struct timer_struct
{
    unsigned long period;
    unsigned long mark;
    bool enabled = false;
};

// Brightness level per pattern
const uint8_t brightVal[ARRAY_SIZE(patterns)] =
{
    192, 192, 225, 225, 225, 225, 225, 255, 255, 192, 225
};

// Delay for incrementing gHue variable per pattern
const uint8_t hueStep[ARRAY_SIZE(patterns)] =
{
    10, 15, 8, 1, 10, 1, 1, 1, 1, 1, 1
};

// Delay inserted into loop() per pattern
unsigned long patternDelay[ARRAY_SIZE(patterns)] =
{
    0, 0, 0, 55, 55, 5, 10, 15, 15, 15, 0
};

// ######################### pattern functions ###########################

void rainbowRoll()
{
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

void rainbowBeat()
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t beat = beatsin8(speed, 64, 255); // Beat advances and retreats in a sine wave
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = ColorFromPalette(palettes[0], gHue + (i * 2), beat - gHue + (i * 10));
    }
}

// LEDs turn on one at a time at full brightness and slowly fade to black
// Uses colors from a palette of colors
void randomPaletteFades()
{
    if (updatePatternBasedOnSpeedSetting(100) == false)
        return;

    uint16_t i = random16(0, (NUM_LEDS - 1)); // Pick a random LED
    {
        uint8_t colorIndex = random8(0, 255); // Pick a random color (from palette)
        if (CRGB(0, 0, 0) == CRGB(leds[i])) // Only set new color to LED that is off
        {
            leds[i] = ColorFromPalette(palettes[currentPaletteIndex], colorIndex, 255, currentBlending);
            blur1d(leds, NUM_LEDS, 32); // Blur colors with neighboring LEDs
        }
    }
    fadeToBlackBy(leds, NUM_LEDS, 8); // Slowly fade LEDs to black
}

// Theater style chasing lights rotating in one direction while the
// rainbow colors rotate in the opposite direction.
void rainbowChase()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    static int q = 0;
    fill_gradient(leds, (NUM_LEDS - 1), CHSV(gHue, 200, 255), 0, CHSV((gHue + 1), 200, 255), LONGEST_HUES);

    for (int i = 0; (NUM_LEDS - 3) > i; i += 3)
    {
        leds[((i + q) + 1)] = CRGB(0, 0, 0);
        leds[((i + q) + 2)] = CRGB(0, 0, 0);
    }
    if (2 > q) {
        q++;
    } else {
        q = 0;
    }
}

void randomDots() // Similar to randomFades(), colors flash on/off quickly
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    uint16_t pos;
    pos = random16(0, (NUM_LEDS - 1));
    if (CRGB(0, 0, 0) == CRGB(leds[pos]))
    {
        leds[pos] = CHSV((random8() % 256), 200, 255);
    }
    fadeToBlackBy(leds, NUM_LEDS, 64);
}

// Same as randomPaletteFades() but with completely random colors
void randomFades()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    uint16_t pos;
    pos = random16(0, (NUM_LEDS - 1));
    if (CRGB(0, 0, 0) == CRGB(leds[pos]))
    {
        leds[pos] = CHSV((random8() % 256), 200, 255);
    }
    fadeToBlackBy(leds, NUM_LEDS, 8);
}

// Same as randomDots() but with red and blue flashes only
void policeLights()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    fadeToBlackBy(leds, NUM_LEDS, 128);
    uint16_t p = random16(0, (NUM_LEDS - 1));
    uint8_t n = (1 & random8());
    if (n)
    {
        leds[p] = CRGB(255, 0, 0);
    }
    else
    {
        leds[p] = CRGB(0, 0, 255);
    }
}

// Same as randomDots() but faster white flashes only
void glitter()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    fadeToBlackBy(leds, NUM_LEDS, 128);
    if (random8() < 225)
    {
        leds[random16(0, (NUM_LEDS - 1))] = CRGB::White;
    }
}

// Twinkling random dim white LEDs mixed with glitter() above
void snowFlakes()
{
    if (updatePatternBasedOnSpeedSetting(200) == false)
        return;

    uint8_t shader;
    for (int x = 0; NUM_LEDS > x; x++)
    {
        shader = random8(20, 30);
        leds[x] = CRGB(shader, shader, shader);
    }
    leds[random16(0, (NUM_LEDS - 1))] = CRGB::White;
}

// Simulates lightning with randomly timed and random size bolts
void lightning()
{
    static timer_struct boltTimer;
    if (previousPatternIndex != currentPatternIndex)
    {
        // slowly fade previous patern to black
        for (uint8_t i = 0; i < 90; i++) {
            fadeToBlackBy( leds, NUM_LEDS, 5);
            LEDS.show();
            delay(2);
        }
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        LEDS.show();
        boltTimer.period = 0;
        boltTimer.mark = millis();
    }
    if (boltTimer.period < (millis() - boltTimer.mark))
    {
        uint16_t boltLength = random16(5, 30);
        uint8_t numStrobes = random8(1, 3);
        uint32_t highPulseTime[numStrobes];
        uint32_t lowPulseTime[numStrobes];
        for (uint8_t i = 0; numStrobes > i; i++)
        {
            highPulseTime[i] = (uint32_t)(random16(60, 250));
            lowPulseTime[i] = (uint32_t)(random16(50, 300));
        }
        uint16_t pos = random16(0, ((NUM_LEDS - 1) - boltLength));
        for (uint8_t i = 0; numStrobes > i; i++)
        {
            for (uint16_t x = pos; (pos + boltLength) > x; x++)
            {
                leds[x] = CRGB(255, 255, 255);
                LEDS.show();
                delay(3);
            }
            delay(highPulseTime[i]);
            if (numStrobes > 1)
            {
                for (uint16_t x = pos; (pos + boltLength) > x; x++)
                {
                    leds[x] = CRGB(127, 127, 127);
                    LEDS.show();
                    delay(3);
                }
                delay(lowPulseTime[i]);
            }
        }
        for (uint16_t x = pos; (pos + boltLength) > x; x++)
        {
            leds[x] = CRGB(0, 0, 0);
        }
        boltTimer.period = (unsigned long)(random16(1500, 5000));
        boltTimer.mark = millis();
    }
}

//######################### Patterns by Resseguie/FastLED-Patterns END #########################


//##################### Desk Lamp
#if LED_DEVICE_TYPE == 3

void pride_Waves()
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

    for (uint16_t i = 0; i < LINE_COUNT; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (LINE_COUNT - 1) - pixelnumber;

        for (int l = 0; l < LEDS_PER_LINE; l++)
        {
            nblend(leds[pixelnumber * LEDS_PER_LINE + l], newcolor, 64);
        }
    }
}

void pride_Rings()
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

    for (uint16_t i = 0; i < LEDS_PER_LINE; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (LEDS_PER_LINE - 1) - pixelnumber;

        for (int p = 0; p < LINE_COUNT; p++)
        {
            if (p % 2 == 0) nblend(leds[p * LEDS_PER_LINE + pixelnumber], newcolor, 64);
            else nblend(leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pixelnumber - 1)], newcolor, 64);
        }
    }
}

void ColorSingleRing(int pos, CHSV c)
{
    for (int p = 0; p < LINE_COUNT; p++)
    {
        if (p % 2 == 0) leds[p * LEDS_PER_LINE + pos] = c;
        else leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pos - 1)] = c;
    }
}

void ColorSingleRing(int pos, CRGB c)
{
    for (int p = 0; p < LINE_COUNT; p++)
    {
        if (p % 2 == 0) leds[p * LEDS_PER_LINE + pos] = c;
        else leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pos - 1)] = c;
    }
}

void colorWaves_hori()
{
    colorwaves_Lamp(leds, LINE_COUNT, gCurrentPalette, 1);
}

void colorWaves_vert()
{
    colorwaves_Lamp(leds, LEDS_PER_LINE, gCurrentPalette, 0);
}

void colorwaves_Lamp(CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette, uint8_t horizonal)
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
        pixelnumber = (numleds - 1) - pixelnumber;

        nblend(ledarray[pixelnumber], newcolor, 128);
        if (horizonal != 0)
        {
            for (int l = 0; l < LEDS_PER_LINE; l++)
            {
                nblend(ledarray[pixelnumber * LEDS_PER_LINE + l], newcolor, 128);
            }
        } else {
            for (int p = 0; p < LINE_COUNT; p++)
            {
                if (p % 2 == 0) nblend(leds[p * LEDS_PER_LINE + pixelnumber], newcolor, 128);
                else nblend(leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - pixelnumber - 1)], newcolor, 128);
            }
        }
    }
}

void rainbow_vert()
{
    for (int l = 0; l < LEDS_PER_LINE; l++)
    {
        for (int p = 0; p < LINE_COUNT; p++)
        {
            if (p % 2 == 0) leds[p * LEDS_PER_LINE + l] = CHSV((((255.00 / (LEDS_PER_LINE)) * l) + gHue), 255, 255);
            else leds[p * LEDS_PER_LINE + (LEDS_PER_LINE - l - 1)] = CHSV((((255.00 / (LEDS_PER_LINE)) * l) + gHue), 255, 255);
        }
    }
}


#endif

// #################### Clock
#if LED_DEVICE_TYPE == 2
// slightly adopted from https://www.geekstips.com/arduino-time-sync-ntp-server-esp8266-udp/
void sendNTPpacket(IPAddress& address) {

    SERIAL_DEBUG_LNF("sending NTP packet to %s...", address.toString().c_str())

    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udpTime.beginPacket(address, 123); //NTP requests are to port 123
    udpTime.write(packetBuffer, NTP_PACKET_SIZE);
    udpTime.endPacket();
}

void PrintTime() {
    SERIAL_DEBUG_LNF("INFO: Current time: %02d:%02d:%02d\n", hours, mins, secs)
}


bool GetTime() {
    static bool ntp_package_sent = false;
    static unsigned long last_package_sent = 0;

    WiFi.hostByName(ntpServerName, timeServerIP);

    if (!ntp_package_sent || last_package_sent + 1000 < millis()) {
        sendNTPpacket(timeServerIP);
        ntp_package_sent = true;
        last_package_sent = millis();
        return false;
    }

    if (ntp_package_sent) {
        int cb = udpTime.parsePacket();
        if (!cb) {
            return false;
        }

        SERIAL_DEBUG_LNF("packet received, length=%lu", cb)

        udpTime.read(packetBuffer, NTP_PACKET_SIZE);
        ntp_timestamp = millis();

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        hours = (epoch % 86400L) / 3600;
        hours += t_offset;
        if (hours >= 24)hours -= 24;
        if (hours < 0)    hours += 24;
        mins = (epoch % 3600) / 60;
        secs = (epoch % 60);

        PrintTime();
        return true;
    }
}

bool shouldUpdateNTP()
{
    if (switchedTimePattern || (millis() - ntp_timestamp) > (NTP_REFRESH_INTERVAL_SECONDS * 1000)) {
        switchedTimePattern = false;
        return true;
    }
    return false;
}

bool shouldUpdateTime()
{
    if ((millis() - update_timestamp) > (1000))return true;
    return false;
}

void DrawDots(int r, int g, int b, int hueMode)
{
    for (int i = 2 * Digit2; i < Digit3; i++) {
        if (hueMode != 0) {
            int hue = map(i, 0, NUM_LEDS, 0, (int)((double)255 / (double)hueMode)) + verySlowHue;
            if (hue >= 255) hue -= 255;
            leds[i] = CHSV(hue, 255,255);
        }
        else leds[i] = CRGB(r, g, b);
    }
}

void displayTime(CRGB x = CRGB(0, 0, 0))
{
    CRGB c = CRGB(0, 0, 0);
    if (x.r == 0 && x.g == 0 && x.b == 0)
    {
        hsv2rgb_rainbow(CHSV(gHue, 255, 255), c);
        DrawTime(c.r, c.g, c.b, 0);
        DrawDots(c.r, c.g, c.b, 0);
    }
    else
    {
        DrawTime(x.r, x.g, x.b, 0);
        DrawDots(x.r, x.g, x.b, 0);
    }
}

void displayTimeStatic()
{
    bool fresh_update = false;
    if (shouldUpdateNTP())
    {
        fresh_update = GetTime();
    }
    if (fresh_update || shouldUpdateTime())
    {
        if (incrementTime() || fresh_update)
        {
            displayTime(solidColor);
        }
    }
}

void displayTimeRainbow()
{
    bool fresh_update = false;
    if (shouldUpdateNTP())
    {
        fresh_update = GetTime();
    }
    if (fresh_update || shouldUpdateTime())
    {
        if (incrementTime()  || fresh_update)
        {
            displayTime();
        }
    }
}

void displayTimeColorful()
{
    bool fresh_update = false;
    if (shouldUpdateNTP())
    {
        fresh_update = GetTime();
    }
    if (fresh_update || shouldUpdateTime())
    {
        if (incrementTime() || fresh_update)
        {
            CRGB x = CRGB(255, 0, 0);
            DrawTime(x.r, x.g, x.b, 1);
            DrawDots(x.r, x.g, x.b, 1);
        }
    }
    else if (updateColorsEverySecond) {
        CRGB x = CRGB(255, 0, 0);
        DrawTime(x.r, x.g, x.b, 1);
        DrawDots(x.r, x.g, x.b, 1);
    }
}

void displayTimeGradient()
{
    bool fresh_update = false;
    if (shouldUpdateNTP())
    {
        fresh_update = GetTime();
    }
    if (fresh_update || shouldUpdateTime())
    {
        if (incrementTime() || fresh_update)
        {
            CRGB x = CRGB(255, 0, 0);
            DrawTime(x.r, x.g, x.b, 5);
            DrawDots(x.r, x.g, x.b, 5);
        }
    }
    else if(updateColorsEverySecond){
        CRGB x = CRGB(255, 0, 0);
        DrawTime(x.r, x.g, x.b, 5);
        DrawDots(x.r, x.g, x.b, 5);
    }
}

void displayTimeGradientLarge()
{
    bool fresh_update = false;
    if (shouldUpdateNTP())
    {
        fresh_update = GetTime();
    }
    if (fresh_update || shouldUpdateTime())
    {
        if (incrementTime() || fresh_update)
        {
            CRGB x = CRGB(255, 0, 0);
            DrawTime(x.r, x.g, x.b, 3);
            DrawDots(x.r, x.g, x.b, 3);
        }
    }
    else if (updateColorsEverySecond) {
        CRGB x = CRGB(255, 0, 0);
        DrawTime(x.r, x.g, x.b, 3);
        DrawDots(x.r, x.g, x.b, 3);
    }
}

bool incrementTime()
{
    bool retval = false;
    secs++;
    update_timestamp = millis();
    if (secs >= 60)
    {
        secs -= 60;
        mins++;
        retval = true;
    }
    if (mins >= 60)
    {
        mins -= 60;
        hours++;
        retval = true;
    }
    if (hours >= 24) hours -= 24;
    PrintTime();
    last_diff = millis() - update_timestamp - 1000;
    return retval;
}


void DrawTime(int r, int g, int b, int hueMode)
{
#define LEDS_PER_SEGMENT (Digit2 / 7)
    for (int l = 0; l < LEDS_PER_SEGMENT; l++)
    {
        if (hours < 10) DrawDigit(Digit1 + l, LEDS_PER_SEGMENT, r, g, b, -1, hueMode);        // Turn off leading zero
        else DrawDigit(Digit1 + l, LEDS_PER_SEGMENT, r, g, b, hours / 10, hueMode); //Draw the first digit of the hour
        DrawDigit(Digit2 + l, LEDS_PER_SEGMENT, r, g, b, hours - ((hours / 10) * 10), hueMode); //Draw the second digit of the hour

        DrawDigit(Digit3 + l, LEDS_PER_SEGMENT, r, g, b, mins / 10, hueMode); //Draw the first digit of the minute
        DrawDigit(Digit4 + l, LEDS_PER_SEGMENT, r, g, b, mins - ((mins / 10) * 10), hueMode); //Draw the second digit of the minute
    }
}

void dDHelper(int offset, int seg, int segmentLedCount, int hueMode, CRGB rgb = CRGB(0, 0, 0) )
{
    if (hueMode != 0) {
        for (int i = 0; i < segmentLedCount; i++)
        {
            int pos = offset + seg + i + seg * (segmentLedCount - 1);
            int hue = map(pos, 0, NUM_LEDS, 0, (int)((double)255 / (double)hueMode)) + verySlowHue;
            if (hue >= 255) hue -= 255;
            CHSV col = CHSV(hue, 255, 255);
            leds[pos] = col;
        }
    }
    else {
        for (int i = 0; i < segmentLedCount; i++)
        {
            leds[offset + seg + i + seg * (segmentLedCount - 1)] = rgb;
        }
    }
}

/*
 * Function: DrawDigit
 * Lights up segments depending on the value
 * Parameters:
 * - offset: position on the clock (1-4)
 * - segmentLedCount: amount of led's per segment
 * - r: red component (0-255)
 * - g: green component (0-255)
 * - b: blue component (0-255)
 * - n: value to be drawn (0-9)
 */
void DrawDigit(int offset, int segmentLedCount, int r, int g, int b, int n, int hueMode)
{
    int s = segmentLedCount;
    CRGB rgb = CRGB(r, g, b);
    if (n == 2 || n == 3 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) //MIDDLE
    {
        dDHelper(offset, 0, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 0, s, 0);
    }
    if (n == 0 || n == 1 || n == 2 || n == 3 || n == 4 || n == 7 || n == 8 || n == 9) //TOP RIGHT
    {
        dDHelper(offset, 1, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 1, s, 0);
    }
    if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) //TOP
    {
        dDHelper(offset, 2, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 2, s, 0);
    }
    if (n == 0 || n == 4 || n == 5 || n == 6 || n == 8 || n == 9) //TOP LEFT
    {
        dDHelper(offset, 3, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 3, s, 0);
    }
    if (n == 0 || n == 2 || n == 6 || n == 8) //BOTTOM LEFT
    {
        dDHelper(offset, 4, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 4, s, 0);
    }
    if (n == 0 || n == 2 || n == 3 || n == 5 || n == 6 || n == 8 || n == 9) //BOTTOM
    {
        dDHelper(offset, 5, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 5, s, 0);
    }
    if (n == 0 || n == 1 || n == 3 || n == 4 || n == 5 || n == 6 || n == 7 || n == 8 || n == 9) //BOTTOM RIGHT
    {
        dDHelper(offset, 6, s, hueMode, rgb);
    }
    else
    {
        dDHelper(offset, 6, s, 0);
    }
}
#endif


// #################### Visualization

#ifdef ENABLE_UDP_VISUALIZATION
bool parseUdp()
{
    static int nopackage = 0;
    int packetSize = Udp.parsePacket();
    if (Udp.available() == 0)nopackage++;
    if (nopackage >= 10)
    {
        fadeToBlackBy(leds, NUM_LEDS, 10);
    }
    if (packetSize)
    {
        nopackage = 0;
        // receive incoming UDP packets
        SERIAL_DEBUG_LNF("Received %d bytes from %s, port %d", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort())
        int len = Udp.readBytes(incomingPacket, PACKET_LENGTH);
        if (len > 0)
        {
            incomingPacket[len] = '\0';
        }
        SERIAL_DEBUG_LNF("UDP packet contents: %s", incomingPacket)
        SERIAL_DEBUG_LNF("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", incomingPacket[0], incomingPacket[1], incomingPacket[2], incomingPacket[3], incomingPacket[4], incomingPacket[5], incomingPacket[6], incomingPacket[7], incomingPacket[8], incomingPacket[9], incomingPacket[10], incomingPacket[11], incomingPacket[12], incomingPacket[13], incomingPacket[14], incomingPacket[15])
        //Serial.println(incomingPacket);
        //PrintBar();
        return true;
    }
    else return false;
}


//############################## Misc

int getVolume(uint8_t vals[], int start, int end, double factor)
{
    double result = 0;
    int iter = 0;
    int cnt = 0;
    SERIAL_DEBUG_LNF("Nr: %d, %d, start: %d, end: %d", iter, vals[iter], start, end)
    for (iter = start; iter <= end && vals[iter] != '\0'; iter++)
    {
        SERIAL_DEBUG_LNF("Nr: %d, %d, start: %d, end: %d", iter, vals[iter], start, end)
        result += ((double)vals[iter]*factor)/(end-start + 1);
    }
    SERIAL_DEBUG_LNF("Result: %f", result)
    if (result <= 1) result = 0;
    if (result > 255)result = 255;
    return result;
}

//############################## Patterns

void BrightnessVisualizer()
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int lastBrightness = 0;
    static int i_pos = 0;
    static bool lastFinished = true;
    static CRGB lastCol;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;

    CHSV toSend;

    if (!parseUdp())return;
    //if (incomingPacket[0] != 0) 
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    //Serial.printf("%d, %d, %lf\n", currentVolume, avgVolume, cd);
    if (currentVolume < 30)cd = 0;

    if (!lastFinished) {
        lastFinished = FadeUp(lastCol, 0, NUM_LEDS, map(speed, 0, 255, 1, 15), 50, false);
        return;
    };

    int decay = map(speed, 0, 255, 8, 200);
    int b = map(cd * 100, 30, 130, 0, 255);
    if (b < 0)b = 0; else if (b > 255)b = 255;
    if (b == 0) fadeToBlackBy(leds, NUM_LEDS, decay);
    //else fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, b));
    else
    {

    }
    lastBrightness = b;

    i_pos++;
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}


bool FadeUp(CRGB c, int start, int end, int update_rate, int starting_brightness, bool isNew)
{
    static uint8_t b = 0; // start out at 0
    if (isNew)b = starting_brightness;
    bool retval = false;

    // slowly increase the brightness
  //EVERY_N_MILLISECONDS(update_rate)
  //{
    if (b < 255) {
        b += update_rate;
    }
    else retval = true;
    //}
    //return false;

    CRGB color((int)(c.r * ((double)(b / 255.0))), (int)(c.g * ((double)(b / 255.0))), (int)(c.b * ((double)(b / 255.0))));
    fill_solid(leds + start, end - start, color);
    return retval;
}

void TrailingBulletsVisualizer()
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int i_pos = 0;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;

    CHSV toSend;

    if (!parseUdp())
    {
        ShiftLeds(1);
        return;
    }
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    if (currentVolume < 25)cd = 0;
    if (currentVolume > 230) cd += 0.15;


    toSend = getVisualizerBulletValue(cd);

    int update_rate = map(speed, 0, 255, 1, 15);
    ShiftLeds(update_rate);
    SendTrailingLeds(toSend, update_rate);
    ShiftLeds(update_rate / 2);
    SendTrailingLeds(CHSV(0, 0, 0), update_rate);
    i_pos++;
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}

CHSV getVisualizerBulletValue(double cd)
{
    CHSV toSend;
    if (cd < 1.05)toSend = CHSV(0, 0, 0);
    if (cd < 1.10)toSend = CHSV(gHue, 255, 5);
    else if (cd < 1.15)toSend = CHSV(gHue, 255, 150);
    else if (cd < 1.20)toSend = CHSV(gHue + 20, 255, 200);
    else if (cd < 1.25)toSend = CHSV(gHue + 40, 255, 255);
    else if (cd < 1.30)toSend = CHSV(gHue + 60, 255, 255);
    else if (cd < 1.45)toSend = CHSV(gHue + 100, 255, 255);
    else toSend = CHSV(gHue, 255, 255);
    return toSend;
}

CHSV getVisualizerBulletValue(int hue, double cd)
{
    CHSV toSend;
    if (cd < 1.05)toSend = CHSV(0, 0, 0);
    if (cd < 1.10)toSend = CHSV(hue, 255, 5);
    else if (cd < 1.15)toSend = CHSV(hue, 255, 150);
    else if (cd < 1.20)toSend = CHSV(hue, 255, 200);
    else if (cd < 1.25)toSend = CHSV(hue, 255, 255);
    else if (cd < 1.30)toSend = CHSV(hue, 255, 255);
    else if (cd < 1.45)toSend = CHSV(hue, 255, 255);
    else toSend = CHSV(hue, 255, 255);
    return toSend;
}



void vuMeter(CHSV c, int mode)
{
    int vol = getVolume(incomingPacket, BAND_START, BAND_END, 1.75);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
#if LED_DEVICE_TYPE == 3
    int toPaint = map(vol, 0, 255, 0, LEDS_PER_LINE);
    if(mode == 0)    for (int i = 0; i < toPaint; i++)ColorSingleRing(i,c);
    else if(mode == 1) for (int i = 0; i < toPaint; i++)ColorSingleRing(i,CHSV(map(i, 0, LEDS_PER_LINE, 0,255),255,255));
    else for (int i = 0; i < toPaint; i++)ColorSingleRing(i,CHSV((uint8_t)(((uint8_t)map(i, 0, LEDS_PER_LINE, 0,255))+gHue),255,255));
#else
    int toPaint = map(vol, 0, 255, 0, NUM_LEDS);
    
    if (mode == 0)    for (int i = 0; i < toPaint; i++)for (int i = 0; i < toPaint; i++)leds[i] = c;
    else if (mode == 1) for (int i = 0; i < toPaint; i++)for (int i = 0; i < toPaint; i++)leds[i] = CHSV(map(i, 0, NUM_LEDS, 0, 255), 255, 255);
    else for (int i = 0; i < toPaint; i++)leds[i] = CHSV(((uint8_t)map(i, 0, NUM_LEDS, 0, 255)) + gHue, 255, 255);
#endif
}

void vuMeterTriColor()
{
    if (!parseUdp())
    {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        return;
    }
    int vol = getVolume(incomingPacket, BAND_START, BAND_END, 1.75);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
#if LED_DEVICE_TYPE == 3
    int toPaint = map(vol, 0, 255, 0, LEDS_PER_LINE);
    if (vol < 153) for(int i = 0;i<toPaint;i++)ColorSingleRing(i,CRGB::Green);
    else if (vol < 204)
    {
        for (int i = 0; i < (LEDS_PER_LINE * 0.6); i++)ColorSingleRing(i, CRGB::Green);
        for (int i = LEDS_PER_LINE * 0.6; i < toPaint; i++)ColorSingleRing(i, CRGB::Orange);
    }
    else if (vol >= 204)
    {
        for (int i = 0; i < (LEDS_PER_LINE * 0.6); i++)ColorSingleRing(i, CRGB::Green);
        for (int i = LEDS_PER_LINE * 0.6; i < (LEDS_PER_LINE * 0.8); i++)ColorSingleRing(i, CRGB::Orange);
        for (int i = LEDS_PER_LINE * 0.8; i < toPaint; i++)ColorSingleRing(i, CRGB::Red);
    }
#else
    int toPaint = map(vol, 0, 255, 0, NUM_LEDS);
    if (vol < 153) fill_solid(leds, toPaint, CRGB::Green);
    else if (vol < 204)
    {
        fill_solid(leds, NUM_LEDS*0.6, CRGB::Green);
        fill_solid(leds+(int(NUM_LEDS * 0.6)), toPaint - NUM_LEDS * 0.6, CRGB::Orange);
    }
    else if (vol >= 204)
    {
        fill_solid(leds, NUM_LEDS * 0.6, CRGB::Green);
        fill_solid(leds + (int(NUM_LEDS * 0.6)), NUM_LEDS * 0.2, CRGB::Orange);
        fill_solid(leds + (int(NUM_LEDS * 0.8)), toPaint - NUM_LEDS * 0.8, CRGB::Red);
    }
#endif
}

int getPeakPosition() {
    int pos = 0;
    byte posval = 0;
    for (int i = 0; i <= (PACKET_LENGTH - 1) && incomingPacket[i] != '\0'; i++)
    {
        if (incomingPacket[i] > posval) {
            pos = i;
            posval = incomingPacket[i];
        }
    }
    if (posval < 30) pos = -1;
    return pos;
}

void printPeak(CHSV c, int pos, int grpSize) {
    fadeToBlackBy(leds, NUM_LEDS, 12);
    leds[pos] = c;
    CHSV c2 = c;
    c2.v = 150;
    for (int i = 0; i < ((grpSize - 1) / 2); i++)
    {
        leds[pos + i] = c;
        leds[pos - i] = c;
    }
    
}

void peakVisualizer(CHSV c, bool newValues) {
    static int lastPos = 0;
    static int moveDir = 1; // 1: up, 0: down, 2: static
    int newPos = lastPos;
    if (newValues) {
        newPos = getPeakPosition();
        if (newPos > lastPos) moveDir = 1;
        else if (newPos < lastPos) moveDir = 0;
        else moveDir = 2;
    }

    int v = 3;
    if (moveDir == 1 && (lastPos +v) <= newPos) {
        lastPos += v;
    }
    else if (moveDir == 0 && (lastPos - v) >= newPos) {
        lastPos -= v;
    }

    int update_rate = map(speed, 0, 70, 100, 0);
    if (newPos == -1 || lastPos < 0) {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        lastPos == -1;
        moveDir = 2;
    }
    else if (update_rate >= 0)
    {
        printPeak(c, lastPos, v);
        delay(update_rate);
    }
    else
    {
        int steps = map(update_rate, -264, 0, 8, 0);
        ShiftLeds(steps);
    }
    //lastPos = newPos;
}

void RainbowPeaks()
{
    if (!parseUdp())
    {
        peakVisualizer(rgb2hsv_approximate(solidColor), false);
    } else peakVisualizer(rgb2hsv_approximate(solidColor), true);
}

void RainbowKickRings()
{
    if (!parseUdp())
    {
        kickRingVisualizer(CHSV(gHue, 255, 255), false);
    }
    else kickRingVisualizer(CHSV(gHue, 255, 255), true);
}

void RainbowBassRings()
{
    if (!parseUdp())
    {
        bassRingVisualizer(CHSV(gHue, 255, 255), false);
    }
    else bassRingVisualizer(CHSV(gHue, 255, 255), true);
}

#define RING_FILL_PERCENTAGE 0.35
#define RING_FADED_PERCENTAGE 0.25
void bassRingVisualizer(CHSV c, bool newValues) {
    static double position = 5.0;
    if (newValues)
    {
        double currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1) / 200.0;
        position += currentVolume * ((double)(map(speed, 0, 255, 10, 300) / 100.0)) * BAND_GROUPING;
        if (position >= NUM_LEDS)position -= NUM_LEDS;
    }
    paintRing(c, position, NUM_LEDS * RING_FILL_PERCENTAGE, NUM_LEDS * RING_FADED_PERCENTAGE);
}

void kickRingVisualizer(CHSV c, bool newValues) {
    static double position = 5.0;
    static int i_pos = 0;
    static int arrsize = 5;
    static uint8_t lastVals[5] = { 1,1,1,1,1 };
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;
    if (newValues)
    {
        currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
        avgVolume = getVolume(lastVals, 0, arrsize - 1, 1);
        if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
        if (currentVolume < 33)cd = 0;
        if (currentVolume > 230) cd += 0.15;
        cd -= 0.2;
        if (cd <= 0 || currentVolume < 33)cd = 0;
        else cd += 0.2;
        position += cd * 2 * ((double)(map(speed, 0, 255, 10, 300) / 100.0)) * BAND_GROUPING;
        //Serial.printf("cur: %d, avg: %d, cd: %lf, pos: %lf\n", currentVolume, avgVolume, cd, position);
        while (position >= NUM_LEDS)position -= NUM_LEDS;

        if (i_pos >= arrsize)i_pos = 0;
        lastVals[i_pos] = currentVolume;
        i_pos++;
    }
    paintRing(c, position, NUM_LEDS * RING_FILL_PERCENTAGE, NUM_LEDS * RING_FADED_PERCENTAGE);
}

void paintRing(CHSV c, int pos, int fillLength, int fadedLength)
{
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < fadedLength; i++)
    {
        leds[(i + pos) > (NUM_LEDS - 1) ? (i + pos - NUM_LEDS) : (i + pos)] = getFadedColor(c, fadedLength - i, fadedLength);
    }
    for (int i = 0; i < fillLength; i++)
    {
        leds[(i + pos + fadedLength) > (NUM_LEDS - 1) ? (i + pos + fadedLength - NUM_LEDS) : (i + pos + fadedLength)] = c;
    }
    for (int i = 0; i < fadedLength; i++)
    {
        leds[(i + pos + fadedLength + fillLength) > (NUM_LEDS - 1) ? (i + pos + fadedLength + fillLength - NUM_LEDS) : (i + pos + fadedLength + fillLength)] = getFadedColor(c, i, fadedLength);;
    }
}

CHSV getFadedColor(CHSV c, int iter, int amount)
{
    c.val = (int)((double)c.val * ((double)(amount - iter)) / ((double)(amount+1.0)));
    return c;
}

//void paintRing(CHSV c, int fillStart, int fillEnd, int fadedStart, int fadedEnd)
//{
//    return;
//}

void vuMeterSolid()
{
    if (!parseUdp())
    {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        return;
    }
    vuMeter(rgb2hsv_approximate(solidColor), 0);
}

void vuMeterStaticRainbow()
{
    if (!parseUdp())
    {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        return;
    }
    vuMeter(rgb2hsv_approximate(solidColor), 1);
}

void vuMeterRainbow()
{
    if (!parseUdp())
    {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        return;
    }
    vuMeter(rgb2hsv_approximate(solidColor), 2);
}

void vuMeterFading()
{
    if (!parseUdp())
    {
        fadeToBlackBy(leds, NUM_LEDS, 5);
        return;
    }
    vuMeter(CHSV(gHue, 255, 255), 0);
}

void NanoleafWaves()
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int i_pos = 0;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;

    CHSV toSend;

    if (!parseUdp())
    {
        ShiftLeds(1);
        return;
    }
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    if (currentVolume < 25)cd = 0;
    if (currentVolume > 230) cd += 0.15;

    toSend = getVisualizerBulletValue(cd);

    int update_rate = map(speed, 0, 70, 100, 0);
    if (update_rate >= 0)
    {
        ShiftLeds(3);
        SendLeds(toSend, 3);
        delay(update_rate);
    }
    else
    {
        int steps = map(update_rate, -264, 0, 8, 0);
        steps /= 2;
        steps *= 3;
        ShiftLeds(steps);
        SendLeds(toSend, steps);
    }

    i_pos++;
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}

void RefreshingVisualizer()
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int i_pos = 0;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;

    CHSV toSend;

    if (!parseUdp())
    {
        ShiftLeds(1);
        return;
    }
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    if (currentVolume < 25)cd = 0;
    if (currentVolume > 230) cd += 0.15;

    toSend = getVisualizerBulletValue(cd);

    int update_rate = map(speed, 0, 70, 100, 0);
    if (update_rate >= 0)
    {
        ShiftLeds(1);
        SendLeds(toSend, 1);
        delay(update_rate);
    }
    else
    {
        int steps = map(update_rate, -264, 0, 8, 0);
        ShiftLeds(steps);
        SendLeds(toSend, steps);
    }

    i_pos++;
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}

void DualToneBullets(int hueA, int hueB, int grpsz)
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int i_pos = 0;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;
    static int hSwitch = 0;
    static bool h = false;

    CHSV toSend;

    if (!parseUdp())
    {
        ShiftLeds(1);
        return;
    }
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    if (currentVolume < 25)cd = 0;
    if (currentVolume > 230) cd += 0.15;

    toSend = getVisualizerBulletValue(h ? hueA : hueB, cd);

    int update_rate = map(speed, 0, 70, 100, 0);
    if (update_rate >= 0)
    {
        ShiftLeds(1);
        SendLeds(toSend, 1);
        delay(update_rate);
    }
    else
    {
        int steps = map(update_rate, -264, 0, 8, 0);
        ShiftLeds(steps);
        SendLeds(toSend, steps);
    }

    i_pos++;
    hSwitch++;
    if (hSwitch >= grpsz) {
        hSwitch = 0;
        h = !h;
    }
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}

int getCounterHue(int h) {
    if (h <= 127) return h + 127;
    if (h > 127) return h - 127;
}

int getPairHue(int h) {
    if (h <= 84) return h + 84;
    if (h > 84) return h - 84;
}

#define BULLET_COLOR_SIZE ((int)(NUM_LEDS/7.9))

void SolidColorComplementary() {
    int h1 = rgb2hsv_approximate(solidColor).hue;
    int h2 = getCounterHue(h1);
    DualToneBullets(h1, h2, BULLET_COLOR_SIZE);
}

void SolidColorDualTone() {
    int h1 = rgb2hsv_approximate(solidColor).hue;
    int h2 = getPairHue(h1);
    DualToneBullets(h1, h2, BULLET_COLOR_SIZE);
}

void BluePurpleBullets() {

    DualToneBullets(240, 170, BULLET_COLOR_SIZE);
}

int expo(double x, double start)
{
    double res = 10.0 * (pow(1.02, x)) - 10.0 + start;
    if (res > 255)res = 255;
    if (res <= 0)res = 0;
    return (int)res;
}

void Band(int grpSize, CRGB x, int mergePacket)
{
    if (!parseUdp())
    {
        fadeToBlackBy(leds, NUM_LEDS, 50);
        return;
    }
    CHSV c = CHSV(0, 0, 0);


    for (int i = 0; i <= (PACKET_LENGTH - 1) && incomingPacket[i] != '\0'; i+=mergePacket)
    {
        int avg = 0;
        for (int t = i; t < (i + mergePacket); t++)avg += incomingPacket[t];
        avg /= (double)mergePacket;
        //uint8_t mult = expo(getVolume(incomingPacket, i, i+(mergePacket-1)), 6.0);
        uint8_t mult = expo(avg, 0.0);

        if (x.r == 0 && x.g == 0 && x.b == 0)
        {
            uint8_t h = gHue + 255.0 * (((double)i) / ((double)PACKET_LENGTH));
#if LED_DEVICE_TYPE==3
            ColorSingleRing(i, CHSV(h, 255, mult));
#else
            //leds[i] = CHSV(h, 255, mult);
            fill_solid(leds + i * (grpSize/mergePacket), grpSize, CHSV(h, 255, mult));
#endif
        }
        else
        {
            CRGB y = CRGB(x.r * (mult / 255.0), x.g * (mult / 255.0), x.b * (mult / 255.0));
#if LED_DEVICE_TYPE==3
            ColorSingleRing(i, y);
#else
            fill_solid(leds + i * (grpSize / mergePacket), grpSize, y);
#endif
            //leds[i] = x;
        }
    }/*
    for (int i = 0; i < grpCnt; i++)
    {

    }*/
}

void SingleColorBandVisualizer()
{
    int grp = (NUM_LEDS) / (PACKET_LENGTH);
    Band(grp, solidColor,1);
}

void RainbowBandVisualizer()
{
    int grp = (NUM_LEDS) / (PACKET_LENGTH);
    Band(grp,CRGB(0, 0, 0), 1);
}

void BulletVisualizer()
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int i_pos = 0;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;

    CHSV toSend;

    if (!parseUdp())
    {
        ShiftLeds(1);
        return;
    }
    //if (incomingPacket[0] != 0) 
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    if (currentVolume < 25)cd = 0;
    if (currentVolume > 230) cd += 0.15;


    toSend = getVisualizerBulletValue(cd);

    int update_rate = map(speed, 0, 255, 1, 15);
    ShiftLeds(update_rate);
    SendLeds(toSend, update_rate);
    i_pos++;
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}
#if LED_DEVICE_TYPE == 4
void NanoleafBand()
{
    int grp = PIXELS_PER_LEAF;    // All pixels in one leaf will be merged into one element
    Band(grp, CRGB(0, 0, 0), 3);
}

void NanoleafSingleBand()
{
    int grp = PIXELS_PER_LEAF;    // All pixels in one leaf will be merged into one element
    Band(grp, solidColor, 3);
}
#endif

void CentralVisualizer()
{
    static uint8_t lastVals[AVG_ARRAY_SIZE] = { 1,1,1,1,1,1,1,1,1,1 };
    static int i_pos = 0;
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;

    CHSV toSend;

    if (!parseUdp())
    {
        ShiftLedsCenter(1);
        return;
    }
    currentVolume = getVolume(incomingPacket, BAND_START, BAND_END, 1);
    avgVolume = getVolume(lastVals, 0, AVG_ARRAY_SIZE - 1, 1);
    if (avgVolume != 0)cd = ((double)currentVolume) / ((double)avgVolume);
    if (currentVolume < 25)cd = 0;
    if (currentVolume > 230) cd += 0.15;


    toSend = getVisualizerBulletValue(cd);

    int update_rate = map(speed, 0, 70, 100, 0);
    if (update_rate >= 0)
    {
        ShiftLedsCenter(1);
        SendLedsCenter(toSend);
        delay(update_rate);
    }
    else
    {
        int steps = map(update_rate, -264, 0, 8, 0);
        
        for (int i = 0; i < steps; i++)
        {
            ShiftLedsCenter(1);
            SendLedsCenter(toSend);
        }
    }
    i_pos++;
    if (i_pos >= AVG_ARRAY_SIZE)i_pos = 0;
    lastVals[i_pos] = currentVolume;
}

void ShiftLedsCenter(int shiftAmount)
{
#if LED_DEVICE_TYPE == 4
    shiftAmount *= (PIXELS_PER_LEAF / 3);
#endif
    for (int cnt = 0; cnt < shiftAmount; cnt++)
    {
#if LED_DEVICE_TYPE == 3
        for (int i = 0; i < (LEDS_PER_LINE / 2); i++)
        {
            ColorSingleRing(i, leds[i + 1]);
            ColorSingleRing(LEDS_PER_LINE - i, leds[LEDS_PER_LINE - i - 1]);
        }
//#elif LED_DEVICE_TYPE == 4
//        for (int i = 0; i < (NUM_LEDS / 2); i++)
//        {
//            leds[i] = leds[i + 1];
//            leds[NUM_LEDS - i] = leds[NUM_LEDS - i - 1];
//        }
#else
        if ((NUM_LEDS % 2) == 0)
        {
            for (int i = 0; i < (NUM_LEDS / 2); i++)
            {
                leds[i] = leds[i + 1];
                leds[NUM_LEDS - i] = leds[NUM_LEDS - i - 1];
            }
        }else for (int i = 0; i <= (NUM_LEDS / 2); i++)
        {
            leds[i] = leds[i + 1];
            leds[(NUM_LEDS-1) - i] = leds[(NUM_LEDS-1) - i - 1];
        }
#endif
    }
}

void SendLedsCenter(CHSV c)
{
#if LED_DEVICE_TYPE == 3
    if ((LEDS_PER_LINE % 2) == 0) ColorSingleRing((LEDS_PER_LINE / 2) - 1, c);
    ColorSingleRing(LEDS_PER_LINE / 2.0, c);
#elif LED_DEVICE_TYPE == 4
    int p = (LEAFCOUNT * 3) / 2.0;
    ColorSingleNanoleafCorner(p, c);
    if (((LEAFCOUNT * 3) % 2) == 0) ColorSingleNanoleafCorner(p-1, c);
#else
    if ((NUM_LEDS % 2) == 0)
    {
        leds[(NUM_LEDS / 2)] = c;
        leds[(NUM_LEDS / 2) - 1] = c;
    }
    else
    {
        leds[(int)(NUM_LEDS / 2.0)] = c;
    }
#endif
}

void SendLeds(CHSV c, int shiftAmount)
{
    for (int i = 0; i < shiftAmount; i++) {
#if LED_DEVICE_TYPE == 3
    ColorSingleRing(i, c);
#elif LED_DEVICE_TYPE == 4
        ColorSingleNanoleafCorner(i, c);
#else
        leds[i] = c;
#endif
    }
}

void SendTrailingLeds(CHSV c, int shiftAmount)
{
    double shiftMult = 1.0 / ((double)shiftAmount);
    int i = shiftAmount;
    leds[i] = c;
    for (int t = shiftAmount - 1; t >= 0; t--)
    {
        CHSV c2 = rgb2hsv_approximate(leds[i]);
        c2.v *= (shiftAmount - t) * shiftMult;
        ShiftLeds(1);
        SendLeds(c2,1);
    }
}

#if LED_DEVICE_TYPE == 4
void ColorSingleNanoleafCorner(int pos, CRGB x)
{
    int start = pos * (PIXELS_PER_LEAF / 3); 
    fill_solid(leds + start, (PIXELS_PER_LEAF / 3), x);
}
#endif

void ShiftLeds(int shiftAmount)
{
    int cnt = shiftAmount;
#if LED_DEVICE_TYPE == 3
    for (int i = LEDS_PER_LINE - 1; i >= cnt; i--)
        ColorSingleRing(i,leds[i-cnt]);
#elif LED_DEVICE_TYPE == 4
    cnt *= (PIXELS_PER_LEAF / 3);
    for (int i = NUM_LEDS - 1; i >= cnt; i--) {
        leds[i] = leds[i - cnt];
    }
#else
    for (int i = NUM_LEDS - 1; i >= cnt; i--) {
        leds[i] = leds[i - cnt];
    }
#endif
}





#if LED_DEVICE_TYPE == 1

void RainbowVisualizer()
{
    if (!parseUdp())return;
    fadeToBlackBy(leds, NUM_LEDS, 150);
    for (int i = 0; i <= (LENGTH - 1) || incomingPacket[i] != '\0'; i++)
    {
        double value = incomingPacket[i];
        //setBar(i, map(value, 0, 255, 0, HEIGHT), CHSV(map(i, 0, LENGTH-1, 0, 255), 255, 255));
        setBar(i, value, CHSV(map(i, 0, LENGTH - 1, 0, 255), 255, 255), false);
    }
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void RainbowVisualizerDoubleSided()
{
    if (!parseUdp())return;
    fadeToBlackBy(leds, NUM_LEDS, 150);
    for (int i = 0; i <= (LENGTH - 1) || incomingPacket[i] != '\0'; i++)
    {
        double value = incomingPacket[i];
        //setBar(i, map(value, 0, 255, 0, HEIGHT), CHSV(map(i, 0, LENGTH-1, 0, 255), 255, 255));
        setBarDoubleSided(i, value, CHSV(map(i, 0, LENGTH - 1, 0, 255), 255, 255), false);
    }
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void SingleColorVisualizer()
{
    if (!parseUdp())return;
    fadeToBlackBy(leds, NUM_LEDS, 150);
    for (int i = 0; i <= (LENGTH - 1) || incomingPacket[i] != '\0'; i++)
    {
        double value = incomingPacket[i];
        //setBar(i, map(value, 0, 255, 0, HEIGHT), CHSV(map(i, 0, LENGTH-1, 0, 255), 255, 255));
        setBar(i, value, solidColor);
    }
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void SingleColorVisualizerDoubleSided()
{
    if (!parseUdp())return;
    fadeToBlackBy(leds, NUM_LEDS, 150);
    for (int i = 0; i <= (LENGTH - 1) || incomingPacket[i] != '\0'; i++)
    {
        double value = incomingPacket[i];
        //setBar(i, map(value, 0, 255, 0, HEIGHT), CHSV(map(i, 0, LENGTH-1, 0, 255), 255, 255));
        setBarDoubleSided(i, value, solidColor, false);
    }
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void HbzVisualizerWhite()
{

    if (!parseUdp())
    {
        PrintHbzLogo(CRGB(255, 255, 255), 0);
        FastLED.delay(1000 / FRAMES_PER_SECOND);
        FastLED.delay(1000 / FRAMES_PER_SECOND);
        return;
    }
    fadeToBlackBy(leds + NUM_LEDS / 2, NUM_LEDS / 2, 150);
    double bright = (incomingPacket[1] + incomingPacket[2] + incomingPacket[3]) / 3;
    double mult = map(bright, 0, 255, 80, 100);
    //Serial.printf("%d, %lf\n", (int)bright, mult);
    PrintHbzLogo(CRGB((int)(255.0 * mult * 0.01), (int)(255.0 * mult * 0.01), (int)(255.0 * mult * 0.01)), 0);
    LogoSpectrum(false);
}

void HbzVisualizerRainbow()
{
    if (!parseUdp())
    {
        FastLED.delay(1000 / FRAMES_PER_SECOND);
        FastLED.delay(1000 / FRAMES_PER_SECOND);
        return;
    }
    fadeToBlackBy(leds + NUM_LEDS / 2, NUM_LEDS / 2, 150);
    double bright = (incomingPacket[1] + incomingPacket[2] + incomingPacket[3]) / 3;
    double mult = map(bright, 0, 255, 80, 100);
    //Serial.printf("%d, %lf\n", (int)bright, mult);
    PrintHbzLogo(CRGB((int)(255.0 * mult * 0.01), (int)(255.0 * mult * 0.01), (int)(255.0 * mult * 0.01)), 0);
    LogoSpectrum(true);
}

void LogoSpectrum(bool isRainbow)
{
    for (int i = 0; i <= (LENGTH / 2 - 1) && incomingPacket[i * 2 + 1] != '\0'; i++)
    {
        double val1 = incomingPacket[i * 2];
        double val2 = incomingPacket[i * 2 + 1];
        double value = (val1 + val2) / 2;
        //Serial.printf("i: %d, avg:%d, %d, %d\n",i,(int)value, (int)val1, (int)val2);
        if (isRainbow)setBar(i + LENGTH / 2, value, CHSV(map(i, 0, LENGTH / 2 - 1, 0, 255), 255, 255), false);
        else setBar(i + LENGTH / 2, value, solidColor, false);

        //FastLED.show();
        //delay(500);
    }
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void PrintHbzLogo(CRGB c, int starting_pos)
{
    static CRGB last = CRGB(0, 0, 0);
    //Serial.printf("%d, %d, %d", c.r, c.g, c.b);
    if (last.r > c.r)
    {
        fadeLightBy(leds + starting_pos, NUM_LEDS / 2, 40);
        last = c;
        return;
    }
    uint8_t logo[] = {
    0,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,0,
    0,0,0,0,0,0,0,0,
    0,0,0,1,1,0,0,0,
    0,1,1,1,1,1,1,0,
    0,0,0,0,0,0,1,0,
    0,1,0,1,1,1,1,0,
    0,1,0,0,0,0,1,0,
    0,1,1,0,1,0,1,0,
    0,1,1,0,1,1,0,0,
    0,0,0,0,0,0,0,0,
    0,1,0,0,1,0,0,0,
    0,0,0,1,0,1,1,0,
    0,1,0,1,1,0,0,0,
    0,0,0,1,0,0,1,0,
    0,0,0,0,0,0,0,0
    };
    for (int i = 0; i < sizeof(logo); i++)
    {
        if (logo[i] == 1)leds[i + starting_pos] = c;
        else leds[i + starting_pos] = CRGB(0, 0, 0);
    }
    last = c;
}

void LineVisualizer()
{

}


void setBarDoubleSided(int row, double num, CRGB color, bool fill_ends)
{
    setBarDoubleSided(row, num, rgb2hsv_approximate(color), fill_ends);
}

void setBar(int row, double num, CRGB color, bool fill_ends)
{
    setBar(row, num, rgb2hsv_approximate(color), fill_ends);
}

void setBar(int row, int num, CRGB color)
{
    setBar(row, num, rgb2hsv_approximate(color));
}

void setBarDoubleSided(int row, double num, CHSV col, bool fill_ends)
{
    double f = (num / 255.00) * (HEIGHT / 2);
    for (int l = 0; l < f; l++)
    {
        if (row % 2 == 1)
        {
            leds[row * HEIGHT + l] = col;
            leds[(row + 1) * HEIGHT - (l + 1)] = col;
        }
        else
        {
            leds[(row + 1) * HEIGHT - (l + 1)] = col;
            leds[(row)*HEIGHT + l] = col;
        }
    }
    if (fill_ends && f != HEIGHT)
    {
        if (row % 2 == 1)leds[row * HEIGHT + (int)f + 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
        else leds[(row + 1) * HEIGHT - (int)f - 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
    }
}

void setBar(int row, double num, CHSV col, bool fill_ends)
{
    //fill_solid(leds + (row * HEIGHT), HEIGHT, CRGB::Black);
    //fadeToBlackBy(leds + (row * HEIGHT), HEIGHT, 100);
    double f = (num / 255.00) * HEIGHT;
    for (int l = 0; l < f; l++)
    {
        if (row % 2 == 1)leds[row * HEIGHT + l] = col;
        else leds[(row + 1) * HEIGHT - (l + 1)] = col;
    }
    if (fill_ends && f != HEIGHT)
    {
        if (row % 2 == 1)leds[row * HEIGHT + (int)f + 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
        else leds[(row + 1) * HEIGHT - (int)f - 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
    }
}

void setBar(int row, int num, CHSV col)
{
    //fill_solid(leds + (row * HEIGHT), HEIGHT, CRGB::Black);
    //fadeToBlackBy(leds + (row * HEIGHT), HEIGHT, 100);
    if (row % 2 == 1)fill_solid(leds + (row * HEIGHT), num, col);
    else
    {
        for (int i = 0; i < num; i++)
        {
            leds[(row + 1) * HEIGHT - i] = col;
        }
    }
}

#endif // LED_DEVICE_TYPE == 1
#endif // ENABLE_UDP_VISUALIZATION

// ############################## AMBILIGHT ##############################
#ifdef ENABLE_SERIAL_AMBILIGHT

#define INITIAL_LED_TEST_ENABLED true
#define INITIAL_LED_TEST_BRIGHTNESS 16  // 0..255
#define INITIAL_LED_TEST_TIME_MS 250  // 10..
unsigned long endTime;
#define OFF_TIMEOUT 8000    // ms to switch off after no data was received, set 0 to deactivate

// function to check if serial data is available
// if timeout occured leds switch of, if configured
bool checkIncommingData() {

    boolean dataAvailable = true;
    while (!Serial.available()) {
        if (OFF_TIMEOUT > 0 && endTime < millis()) {
            memset(leds, 0, NUM_LEDS * sizeof(struct CRGB));
            FastLED.show();
            dataAvailable = false;
            endTime = millis() + OFF_TIMEOUT;
        }
    }
    return dataAvailable;
}

// ambilight, implemented by @sunzi5
void ambilight() {
    static bool is_initialized = false;
    static boolean transmissionSuccess;
    static unsigned long sum_r, sum_g, sum_b;
    static int thisPatternId = 0;
    uint8_t prefix[] = { 'A', 'd', 'a' }, hi, lo, chk, i;

    if (!is_initialized)
    {
        thisPatternId = currentPatternIndex;
        Serial.print("Ada\n"); // Send "Magic Word" string to host

        int ledCount = NUM_LEDS;
#if INITIAL_LED_TEST_ENABLED == true
        for (int v = 0; v < INITIAL_LED_TEST_BRIGHTNESS; v++)
        {
            fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
            delay(INITIAL_LED_TEST_TIME_MS / 2 / INITIAL_LED_TEST_BRIGHTNESS);
        }
        for (int v = 0; v < INITIAL_LED_TEST_BRIGHTNESS; v++)
        {
            fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
            delay(INITIAL_LED_TEST_TIME_MS / 2 / INITIAL_LED_TEST_BRIGHTNESS);
        }
        for (int v = 0; v < INITIAL_LED_TEST_BRIGHTNESS; v++)
        {
            fill_solid(leds, NUM_LEDS, CRGB(0, 0, 255));
            delay(INITIAL_LED_TEST_TIME_MS / 2 / INITIAL_LED_TEST_BRIGHTNESS);
        }
#endif
        fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));


        is_initialized = true;
    }
    for (;;)
    {
#ifdef ENABLE_ALEXA_SUPPORT
        espalexa.loop();
#else
        webServer.handleClient();
#endif
        if (currentPatternIndex != thisPatternId) break;
        if (!Serial.available() > 0) continue;
        for (int i = 0; i < sizeof prefix; ++i) {
            if (!checkIncommingData() || prefix[i] != Serial.read()) {
                i = 0;
            }
        }
        if (!checkIncommingData()) continue;
        hi = Serial.read();
        if (!checkIncommingData()) continue;
        lo = Serial.read();
        if (!checkIncommingData()) continue;
        chk = Serial.read();

        if (chk != (hi ^ lo ^ 0x55)) continue;

        int num_leds = min(NUM_LEDS, (hi << 8) + lo + 1);
        memset(leds, 0, num_leds * sizeof(struct CRGB));
        transmissionSuccess = true;
        sum_r = 0;
        sum_g = 0;
        sum_b = 0;

        for (int idx = 0; idx < num_leds; idx++) {
            byte r, g, b;
            if (!checkIncommingData()) {
                transmissionSuccess = false;
                break;
            }
            r = Serial.read();
            if (!checkIncommingData()) {
                transmissionSuccess = false;
                break;
            }
            g = Serial.read();
            if (!checkIncommingData()) {
                transmissionSuccess = false;
                break;
            }
            b = Serial.read();
            leds[idx].r = r;
            leds[idx].g = g;
            leds[idx].b = b;
        }

        // shows new values
        if (transmissionSuccess) {
            endTime = millis() + OFF_TIMEOUT;
            FastLED.show();
        }
    }
}
#endif
//############################## SERIAL AMBILIGHT END #############################

//############################## ALEXA Device Events ##############################

#ifdef ENABLE_ALEXA_SUPPORT
void mainAlexaEvent(EspalexaDevice* d) {
    if (d == nullptr) return;

    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa update: rgb: "); Serial.print(d->getR() + d->getG() + d->getB()); Serial.print(", b: "); Serial.println(d->getValue());
    #endif

    if (d->getValue() == 0)setPower(0); else {
        setPower(1);
        setBrightness(d->getValue());
    }
    static int lr;
    static int lg;
    static int lb;
    if ((lr != NULL && lr != d->getR() && lg != d->getG() && lb != d->getB()) || currentPatternIndex == patternCount - 1)
    {
        setSolidColor(d->getR(), d->getG(), d->getB(), true);
        setPattern(patternCount - 1);
    }
    lr = d->getR();
    lg = d->getG();
    lb = d->getB();
}

#ifdef AddStrobeDevice 
void AlexaStrobeEvent(EspalexaDevice* d) {
    if (d == nullptr) return;

    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa Strobe update: rgb: "); Serial.print(d->getR() + d->getG() + d->getB()); Serial.print(", b: "); Serial.println(d->getValue());
    #endif
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
        setSolidColor(d->getR(), d->getG(), d->getB(), true);
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
    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa Autoplay update: state: "); Serial.println(d->getPercent());
    #endif
    if (d->getValue() > 0)
    {
        setAutoplay(1);
        setAutoplayDuration(d->getPercent());
    }
    else setAutoplay(0);
}
#endif
#ifdef AddSpecificPatternDeviceA
void AlexaSpecificEventA(EspalexaDevice* d) {
    if (d == nullptr) return;
    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa Specific Pattern update: state: "); Serial.println(d->getValue());
    #endif
    if (d->getValue() != 0)setPattern(SpecificPatternA);
    else setPattern(patternCount - 1);
}
#endif
#ifdef AddSpecificPatternDeviceB
void AlexaSpecificEventB(EspalexaDevice* d) {
    if (d == nullptr) return;
    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa Specific Pattern update: state: "); Serial.println(d->getValue());
    #endif
    if (d->getValue() != 0)setPattern(SpecificPatternB);
    else setPattern(patternCount - 1);
}
#endif
#ifdef AddSpecificPatternDeviceC
void AlexaSpecificEventC(EspalexaDevice* d) {
    if (d == nullptr) return;
    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa Specific Pattern update: state: "); Serial.println(d->getValue());
    #endif
    if (d->getValue() != 0)setPattern(SpecificPatternC);
    else setPattern(patternCount - 1);
}
#endif
#ifdef AddAudioDevice
void AlexaAudioEvent(EspalexaDevice* d) {
    if (d == nullptr) return;
    #if LED_DEBUG != 0
    SERIAL_DEBUG_BOL
    Serial.print(" Alexa Audio update: rgb: "); Serial.print(d->getR() + d->getG() + d->getB()); Serial.print(", b: "); Serial.println(d->getValue());
    #endif
    if (d->getValue() != 0)setPattern(AUDIOPATTERN);
    else setPattern(patternCount - 1);

}
#endif
#endif

/*######################## LOGO FUNCTIONS ########################*/

void logo()
{
#ifdef TWENTYONEPILOTS
    twp();
#endif // TWENTYONEPILOTS
#ifdef THINGIVERSE
    thingiverse();
#endif
}

void logo_static()
{
#ifdef TWENTYONEPILOTS
    twp_static();
#endif // TWENTYONEPILOTS
#ifdef THINGIVERSE
    thingiverse_static();
#endif
}


#ifdef TWENTYONEPILOTS
void twp_static()
{
    fill_solid(leds + twpOffsets[1], DOUBLE_STRIP_LENGTH, STATIC_LOGO_COLOR);
    fill_solid(leds + twpOffsets[2], DOT_LENGTH, STATIC_LOGO_COLOR);
    fill_solid(leds + twpOffsets[3], ITALIC_STRIP_LENGTH, STATIC_LOGO_COLOR);
    fill_solid(leds + twpOffsets[0], RING_LENGTH, STATIC_RING_COLOR);
}

void twp()  // twenty one pilots
{
    static uint8_t    numdots = 4; // Number of dots in use.
    static uint8_t   faderate = 4; // How long should the trails be. Very low value = longer trails.
    static uint8_t     hueinc = 255 / numdots - 1; // Incremental change in hue between each dot.
    static uint8_t    thishue = 42; // Starting hue.
    static uint8_t     curhue = 42; // The current hue
    static uint8_t    thissat = 255; // Saturation of the colour.
    static uint8_t thisbright = 255; // How bright should the LED/display be.
    static uint8_t   basebeat = 5; // Higher = faster movement.

    static uint8_t lastSecond = 99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
    uint8_t secondHand = (millis() / 1000) % ANIMATION_RING_DURATION; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

    if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
        lastSecond = secondHand;
        switch (secondHand) {
        case  0: numdots = 1; basebeat = 20; hueinc = 2; faderate = 4; thishue = random(38, 42); break; // You can change values here, one at a time , or altogether.
        case 10: numdots = 4; basebeat = 10; hueinc = 2; faderate = 8; thishue = random(37, 43); break;
        case 20: numdots = 8; basebeat = 3; hueinc = 0; faderate = 8; thishue = random(37, 43); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
        case 30: break;
        }
    }

    // Several colored dots, weaving in and out of sync with each other
    curhue = thishue; // Reset the hue values.
    fadeToBlackBy(leds, NUM_LEDS, faderate);
    for (int i = 0; i < numdots; i++) {
        //leds[beatsin16(basebeat + i + numdots, 0, NUM_LEDS)] += CHSV(curhue, thissat, thisbright);
        leds[beatsin16(basebeat + i + numdots, twpOffsets[0], RING_LENGTH + twpOffsets[0])] += CHSV(curhue, thissat, thisbright);
        //leds[beatsin16(basebeat + i + numdots, twpOffsets[1], DOUBLE_STRIP_LENGTH + twpOffsets[1])] += CHSV(curhue, thissat, thisbright);
        //leds[beatsin16(basebeat + i + numdots, twpOffsets[2], DOT_LENGTH + twpOffsets[1]+ twpOffsets[2])] += CHSV(curhue, thissat, thisbright);
        //leds[beatsin16(basebeat + i + numdots, twpOffsets[3], ITALIC_STRIP_LENGTH + twpOffsets[1]+ twpOffsets[2]+ twpOffsets[3])] += CHSV(curhue, thissat, thisbright);
        curhue += hueinc;
    }

    // sinelone for the lines
    fadeToBlackBy(leds + twpOffsets[0], DOUBLE_STRIP_LENGTH + DOT_LENGTH + ITALIC_STRIP_LENGTH, 50);
    int16_t myspeed = 30 + speed * 1.5;
    if (myspeed > 255 || myspeed < 0)myspeed = 255;
    int pos = beatsin16(myspeed, twpOffsets[1], twpOffsets[1] + DOUBLE_STRIP_LENGTH + DOT_LENGTH + ITALIC_STRIP_LENGTH - 1);
    static int prevpos = 0;
    CRGB color = STATIC_LOGO_COLOR;
    if (pos < prevpos) {
        fill_solid(leds + pos, (prevpos - pos) + 1, color);
    }
    else {
        fill_solid(leds + prevpos, (pos - prevpos) + 1, color);
    }
    prevpos = pos;
}
#endif // TWENTYONEPILOTS


#ifdef THINGIVERSE
void thingiverse_static()
{
    if (RINGFIRST)
    {
        fill_solid(leds, RING_LENGTH, STATIC_RING_COLOR);
        fill_solid(leds + RING_LENGTH, HORIZONTAL_LENGTH, STATIC_LOGO_COLOR);
        fill_solid(leds + RING_LENGTH + HORIZONTAL_LENGTH, VERTICAL_LENGTH, STATIC_LOGO_COLOR);
    }
    else
    {
        fill_solid(leds, HORIZONTAL_LENGTH, STATIC_LOGO_COLOR);
        fill_solid(leds + HORIZONTAL_LENGTH, VERTICAL_LENGTH, STATIC_LOGO_COLOR);
        fill_solid(leds + HORIZONTAL_LENGTH + VERTICAL_LENGTH, RING_LENGTH, STATIC_RING_COLOR);
    }
}

void thingiverse()  // twenty one pilots
{
    static uint8_t    numdots = 4; // Number of dots in use.
    static uint8_t   faderate = 4; // How long should the trails be. Very low value = longer trails.
    static uint8_t     hueinc = 255 / numdots - 1; // Incremental change in hue between each dot.
    static uint8_t    thishue = 82; // Starting hue.
    static uint8_t     curhue = 82; // The current hue
    static uint8_t    thissat = 255; // Saturation of the colour.
    static uint8_t thisbright = 255; // How bright should the LED/display be.
    static uint8_t   basebeat = 5; // Higher = faster movement.

    static uint8_t lastSecond = 99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
    uint8_t secondHand = (millis() / 1000) % ANIMATION_RING_DURATION; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

    if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
        lastSecond = secondHand;
        switch (secondHand) {
        case  0: numdots = 1; basebeat = 20; hueinc = 2; faderate = 4; thishue = random(143, 147); break; // You can change values here, one at a time , or altogether.
        case 10: numdots = 4; basebeat = 10; hueinc = 2; faderate = 8; thishue = random(142, 148); break;
        case 20: numdots = 8; basebeat = 3; hueinc = 0; faderate = 8; thishue = random(143, 147); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
        case 30: break;
        }
    }

    // Several colored dots, weaving in and out of sync with each other
    curhue = thishue; // Reset the hue values.
    if (RINGFIRST)
    {
        fadeToBlackBy(leds, RING_LENGTH, faderate);
    }
    else fadeToBlackBy(leds + VERTICAL_LENGTH + HORIZONTAL_LENGTH, VERTICAL_LENGTH + HORIZONTAL_LENGTH + RING_LENGTH, faderate);
    for (int i = 0; i < numdots; i++) {
        if (RINGFIRST)leds[beatsin16(basebeat + i + numdots, 0, RING_LENGTH)] += CHSV(curhue, thissat, thisbright);
        else leds[beatsin16(basebeat + i + numdots, VERTICAL_LENGTH + HORIZONTAL_LENGTH, RING_LENGTH + VERTICAL_LENGTH + HORIZONTAL_LENGTH)] += CHSV(curhue, thissat, thisbright);
        curhue += hueinc;
    }

    // sinelone for the lines
    /*
    fadeToBlackBy(leds + twpOffsets[0], DOUBLE_STRIP_LENGTH + DOT_LENGTH + ITALIC_STRIP_LENGTH, 50);
    int16_t myspeed = 30 + speed * 1.5;
    if (myspeed > 255 || myspeed < 0)myspeed = 255;
    int pos = beatsin16(myspeed, twpOffsets[1], twpOffsets[1] + DOUBLE_STRIP_LENGTH + DOT_LENGTH + ITALIC_STRIP_LENGTH - 1);
    static int prevpos = 0;
    CRGB color = STATIC_LOGO_COLOR;
    if (pos < prevpos) {
      fill_solid(leds + pos, (prevpos - pos) + 1, color);
    }
    else {
      fill_solid(leds + prevpos, (pos - prevpos) + 1, color);
    }
    prevpos = pos;
    */
    //uint8_t b = beatsin8(10, 200, 255);

    uint8_t pos = 0;
    uint8_t spd = 100;
    uint8_t b = 255;
    bool even = true;
    if ((HORIZONTAL_LENGTH / 2.00) > (int)(HORIZONTAL_LENGTH / 2.00))even = false;

    if (!even)
    {
        // FIXME: This is BROKEN, beatsaw8 takes 5 arguments, 3 given here
        //pos = beatsin8(spd, 0, VERTICAL_LENGTH + (HORIZONTAL_LENGTH - 1) / 2);
        pos = beatsaw8(spd, 0, VERTICAL_LENGTH + (HORIZONTAL_LENGTH - 1) / 2);
        b = beatsaw8(spd * 2, 255 / 2, 255);
    }
    else
    {
        //pos = beatsin8(spd, 0, VERTICAL_LENGTH + (HORIZONTAL_LENGTH - 2) / 2);
    }
    if (!even)
    {
        if (pos < VERTICAL_LENGTH)
        {
            if (HORIZONTAL_BEFORE_VERTICAL)
            {
                if (!RINGFIRST) leds[HORIZONTAL_LENGTH + VERTICAL_LENGTH - pos - 1] = CHSV(145, 255, b);
                else { leds[HORIZONTAL_LENGTH + VERTICAL_LENGTH - pos - 1 + RING_LENGTH] = CHSV(145, 255, b); }
            }
            else
            {
                if (!RINGFIRST) leds[VERTICAL_LENGTH - pos - 1] = CHSV(145, 255, b);
                else { leds[VERTICAL_LENGTH - pos - 1 + RING_LENGTH] = CHSV(145, 255, b); }
            }
        }
        else if (pos == VERTICAL_LENGTH)
        {
            if (!RINGFIRST)
            {
                leds[(HORIZONTAL_LENGTH / 2)] = CHSV(145, 255, b);
            }
            else
            {
                leds[(HORIZONTAL_LENGTH / 2) + RING_LENGTH] = CHSV(145, 255, b);
            }
        }
        else
        {
            if (HORIZONTAL_BEFORE_VERTICAL)
            {
                if (!RINGFIRST)
                {
                    leds[HORIZONTAL_LENGTH - pos] = CHSV(145, 255, b);
                    leds[pos - 1] = CHSV(145, 255, b);
                }
                else
                {
                    leds[HORIZONTAL_LENGTH - pos + RING_LENGTH] = CHSV(145, 255, b);
                    leds[pos - 1 + RING_LENGTH] = CHSV(145, 255, b);
                }
            }
            else
            {
                if (!RINGFIRST)
                {
                    leds[HORIZONTAL_LENGTH - pos + VERTICAL_LENGTH] = CHSV(145, 255, b);
                    leds[pos - 1 + VERTICAL_LENGTH] = CHSV(145, 255, b);
                }
                else
                {
                    leds[HORIZONTAL_LENGTH - pos + RING_LENGTH + VERTICAL_LENGTH] = CHSV(145, 255, b);
                    leds[pos - 1 + RING_LENGTH + VERTICAL_LENGTH] = CHSV(145, 255, b);
                }
            }
        }
    }
    if (!RINGFIRST)
    {
        //fadeToBlackBy(leds, HORIZONTAL_LENGTH + VERTICAL_LENGTH, 50);
        fadeLightBy(leds, HORIZONTAL_LENGTH + VERTICAL_LENGTH, 5);
    }
    else
    {
        fadeLightBy(leds + RING_LENGTH, HORIZONTAL_LENGTH + VERTICAL_LENGTH, 5);
    }

    /*
    uint8_t b = 255;
    uint8_t pos = 0;
    if (RINGFIRST)
    {
      pos = beatsin8(30, RING_LENGTH, RING_LENGTH + VERTICAL_LENGTH + HORIZONTAL_LENGTH);
      fadeToBlackBy(leds + RING_LENGTH, RING_LENGTH+VERTICAL_LENGTH + HORIZONTAL_LENGTH, 10);

    }
    else
    {
      pos = beatsin8(30, 0, VERTICAL_LENGTH + HORIZONTAL_LENGTH);
      fadeToBlackBy(leds, VERTICAL_LENGTH + HORIZONTAL_LENGTH, 10);

    }
    //if (pos == 0 && RINGFIRST == false)fadeToBlackBy(leds, 1, 50);
    //else if(pos == RING_LENGTH && RINGFIRST == true)fadeToBlackBy(leds+RING_LENGTH, 1, 50);
    leds[pos] = CHSV(145, 255, b);
    */
}
#endif THINGIVERSE

/*###################### LOGO FUNCTIONS END ######################*/

//################## LEGACY Sound Reactive Sensor ################//
#ifdef SOUND_SENSOR_SUPPORT
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
#if LED_DEVICE_TYPE == 3
    ColorSingleRing(map(level,1,NUM_LEDS,1,LEDS_PER_LINE), CHSV(gHue, 255, 255));
#else
    fill_solid(leds, level, CHSV(gHue, 255, 255));
#endif
    fill_solid(leds, level, CHSV(gHue, 255, 255));
    fadeToBlackBy(leds + (int)level, NUM_LEDS - (int)level, decay);
    lastlevel = level;

    //Serial.print(mlevel); Serial.print(" "); Serial.println(level);
    //Serial.printf("%d, %d\n", mlevel, level);
#endif
}
#endif
//################ LEGACY Sound Reactive Sensor End ##############//

//############################## MQTT HELPER FUNCTIONS ##############################
#ifdef ENABLE_MQTT_SUPPORT
static bool mqttProcessing = false;
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    mqttProcessing = true;
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    SERIAL_DEBUG_LNF("Received MQTT package: %s", doc.as<String>().c_str())

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair p : obj) {
        const char* key = p.key().c_str();
        JsonVariant v = p.value();

        if (strcmp(key, "state") == 0) {
            String val = v.as<String>();
            val.toLowerCase();
            if (val == String("on")) {
                setPower(1);
            } else if (val == String("off")) {
                setPower(0);
            } else if (val == String("toggle")) {
                setPower((power == 1) ? 0 : 1);
            }
        }
        if (strcmp(key, "brightness") == 0) {
            uint8_t val = v.as<uint8_t>();
            setBrightness(val);
        }
        if (strcmp(key, "autoplay") == 0){
            String val = v.as<String>();
            val.toLowerCase();
            if (val == String("on")) {
                setAutoplay(1);
            } else if (val == String("off")) {
                setAutoplay(0);
            } else if (val == String("toggle")) {
                setAutoplay((autoplay == 1) ? 0 : 1);
            }
        }
        if (strcmp(key, "speed") == 0){
            int val = v.as<int>();
            setSpeed(val);
        }
        if (strcmp(key, "hue") == 0){
            uint8_t val = v.as<uint8_t>();
            setSolidColorHue(val, false);
        }
        if (strcmp(key, "saturation") == 0){
            uint8_t val = v.as<uint8_t>();
            setSolidColorSat(val, false);
        }
        if (strcmp(key, "effect") == 0) {
            String val = v.as<String>();
            setPatternName(val);
        }
        if (strcmp(key, "color") == 0) {
            uint8_t cr, cb, cg;
            JsonObject val = v.as<JsonObject>();
            for (JsonPair o : val) {
                const char* ckey = o.key().c_str();
                JsonVariant cv = o.value();
                if (strcmp(ckey, "r") == 0) {
                    cr = cv.as<uint8_t>();
                }
                if (strcmp(ckey, "g") == 0) {
                    cg = cv.as<uint8_t>();
                }
                if (strcmp(ckey, "b") == 0) {
                    cb = cv.as<uint8_t>();
                }
            }
            setSolidColor(cr, cg, cb, false);
        }
    }
    mqttProcessing = false;
    mqttSendStatus();
}

void mqttSendStatus() {
    if (cfg.MQTTEnabled != 1) return;

    StaticJsonDocument<128> JSONencoder;
      JSONencoder["state"] = (power == 1 ? "ON" : "OFF"),
      JSONencoder["brightness"] = brightness,
      JSONencoder["effect"] = patterns[currentPatternIndex].name,
      JSONencoder["autoplay"] = autoplay,
      JSONencoder["speed"] = speed;
      JSONencoder["hue"] = getHueMapped((uint8_t)0, (uint8_t)255);
      JSONencoder["saturation"] = getSatMapped((uint8_t)0, (uint8_t)255);

    uint8_t JSONmessage[128];
    size_t n = serializeJson(JSONencoder, JSONmessage);
    if (!mqttProcessing){
        mqttClient.publish(cfg.MQTTTopic, JSONmessage, n, true);
        SERIAL_DEBUG_LNF("Sending MQTT package: %s", JSONencoder.as<String>().c_str())
    }
}
#endif // ENABLE_MQTT_SUPPORT
//############################## MQTT HELPER FUNCTIONS END ##############################

// ###################### Homey support functions ########################

#ifdef ENABLE_HOMEY_SUPPORT
// here we do some conversion between Homey values and convert them to RGB
void homeyLightOnoff( void ) {
    setPower(Homey.value.toInt());
    SERIAL_DEBUG_LNF("Homey set power: %s", (power == 1) ? "on" : "off")
}
// hsv2rgb_rainbow
void homeyLightDim( void ) {
    float mappedBrightness = 0.0;
    float homeyBrightness = Homey.value.toFloat();
    mappedBrightness = mapfloat(homeyBrightness, 0.0, 1.0, 0.0, 255.0);
    SERIAL_DEBUG_LNF("Homey set brightness: %0.2f == %d", homeyBrightness, (uint8_t) mappedBrightness)
    setBrightness((uint8_t) mappedBrightness);
}

void homeyLightHue( void ) {
    float homeyHue = Homey.value.toFloat();
    SERIAL_DEBUG_LNF("Homey set hue: %0.3f", homeyHue)
    setSolidColorHue((uint8_t) mapfloat(homeyHue, 0.0, 1.0, 0.0, 255.0), true);
    setAutoplay(false);
}

void homeyLightSaturation( void ) {
    float homeySat = Homey.value.toFloat();
    SERIAL_DEBUG_LNF("Homey set saturation: %0.2f", homeySat)
    setSolidColorSat((uint8_t) mapfloat(homeySat, 0.0, 1.0, 0.0, 255.0), true);
    setAutoplay(false);
}

void homeyNext( void ) {
    SERIAL_DEBUG_LN(F("Homey set next pattern"))
    setAutoplay(false);
    adjustPattern(true);
}

void homeyPrev( void ) {
    SERIAL_DEBUG_LN(F("Homey set previous pattern"))
    setAutoplay(false);
    adjustPattern(false);
}
#endif
