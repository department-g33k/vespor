/*****************************************************************************
The MIT License (MIT)

Copyright (c) 2016 by bbx10node@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 **************************************************************************/

#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Change these as desired. You must enter the password to connected to this
// rover in AP.
static const char AP_SSID[] = "Vespor ROV";
static const char AP_PASS[] = "GoVespor";

static const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 128, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#include <NeoPixelBus.h>

// Rover headlights consist of an 8 LED WS2812 stick
// The Web UI can turn them all on, all 50%, and all off.
const uint16_t PixelCount = 8; // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 2;  // ignored when using DMA or UART methods

// NeoEsp8266Uart800KbpsMethod also ignores the pin parameter and uses GPI02
// This method works well even with active WiFi because it does not disable
// interrupts like bit bang methods.
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount, PixelPin);

#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <utility/Adafruit_MS_PWMServoDriver.h>

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
// Or, create it with a different I2C address (say for stacking)
// Adafruit_MotorShield AFMS = Adafruit_MotorShield(0x61);

// Select which 'port' M1, M2, M3 or M4.
Adafruit_DCMotor *leftMotor  = AFMS.getMotor(3);
Adafruit_DCMotor *rightMotor = AFMS.getMotor(4);

// This string holds HTML, CSS, and Javascript for the Web UI.
// The browser must support HTML5 WebSockets.
// In AP mode, there is no connection to the Internet so do not link to
// libraries on CDNs.
static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>ESP8266 Vespor ROV</title>
<style>
body { font-family: Arial, Helvetica, Sans-Serif; }
table { width: 100%; }
td { width: 33%; font-size: 200%; text-align: center; }
button { font-size: 150%; }
.directionButton { background-color: #E0E0E0; border: 1px solid; }
</style>
<script>
function enableTouch(objname) {
  console.log('enableTouch', objname);
  var e = document.getElementById(objname);
  if (e) {
    e.addEventListener('touchstart', function(event) {
        event.preventDefault();
        console.log('touchstart', event);
        buttondown(e);
        }, false );
    e.addEventListener('touchend',   function(event) {
        console.log('touchend', event);
        buttonup(e);
        }, false );
  }
  else {
    console.log(objname, ' not found');
  }
}

var websock;

function start() {
  websock = new WebSocket('ws://' + window.location.hostname + ':81/');
  websock.onopen = function(evt) {
    console.log('websock open');
    var e = document.getElementById('webSockStatus');
    e.style.backgroundColor = 'green';
  };
  websock.onclose = function(evt) {
    console.log('websock close');
    var e = document.getElementById('webSockStatus');
    e.style.backgroundColor = 'red';
  };
  websock.onerror = function(evt) { console.log(evt); };
  websock.onmessage = function(evt) {
    var eLedon = document.getElementById('bLedon');
    var eLed50 = document.getElementById('bLed50');
    var eLedoff = document.getElementById('bLedoff');
    if (evt.data === 'bLedon=1') {
      eLedon.style.backgroundColor = 'red';
      eLed50.style.backgroundColor = '';
      eLedoff.style.color = '';
      eLedoff.style.backgroundColor = '';
    }
    else if (evt.data === 'bLed50=1') {
      eLedon.style.backgroundColor = '';
      eLed50.style.backgroundColor = 'orange';
      eLedoff.style.color = '';
      eLedoff.style.backgroundColor = '';
    }
    else if (evt.data === 'bLedoff=1') {
      eLedon.style.backgroundColor = '';
      eLed50.style.backgroundColor = '';
      eLedoff.style.color = 'white';
      eLedoff.style.backgroundColor = 'black';
    }
    else {
      console.log('unknown event', evt.data);
    }
  };

  var allButtons = [
    'bForward',
    'bBackward',
    'bRight',
    'bLeft',
    'bLedon',
    'bLed50',
    'bLedoff'
  ];
  for (var i = 0; i < allButtons.length; i++) {
    enableTouch(allButtons[i]);
  }
}
function buttondown(e) {
  websock.send(e.id + '=1');
}
function buttonup(e) {
  websock.send(e.id + '=0');
}
</script>
</head>
<body onload="javascript:start();">
<h2>ESP8266 Vespor</h2>
<table>
  <tr>
    <td></td>
    <td id="bForward" class="directionButton"
      onmousedown="buttondown(this);" onmouseup="buttonup(this);">Forward</td>
    <td></td>
  </tr>
  <tr>
    <td id="bLeft" class="directionButton"
      onmousedown="buttondown(this);" onmouseup="buttonup(this);">Left</td>
    <td></td>
    <td id="bRight" class="directionButton"
      onmousedown="buttondown(this);" onmouseup="buttonup(this);">Right</td>
  </tr>
  <tr>
    <td></td>
    <td id="bBackward" class="directionButton"
      onmousedown="buttondown(this);" onmouseup="buttonup(this);">Back</td>
    <td></td>
  </tr>
</table>
<p>

<button id="webSockStatus" type="button" onclick="window.location.reload();">Connect</button>
<p>

<button id="bLedon"  type="button" onclick="buttondown(this);">On</button>
<button id="bLed50"  type="button" onclick="buttondown(this);">50%</button>
<button id="bLedoff" type="button" onclick="buttondown(this);">Off</button>
</body>
</html>
)rawliteral";

// Current LED status
static uint8_t LEDStatus;

// Commands sent through Web Socket
static const char LEDON[]  = "bLedon=1";
static const char LED50[]  = "bLed50=1";
static const char LEDOFF[] = "bLedoff=1";

static void writeLED(uint8_t LEDbrightness)
{
  LEDStatus = LEDbrightness;
  if (LEDbrightness == 128) {
    // Turn on every other LED
    for (int i = 0; i < PixelCount; i+=2) {
      strip.SetPixelColor(i, RgbColor(255, 255, 255));
      strip.SetPixelColor(i+1, RgbColor(0, 0, 0));
    }
  }
  else {
    for (int i = 0; i < PixelCount; i++) {
      strip.SetPixelColor(i, RgbColor(LEDbrightness, LEDbrightness, LEDbrightness));
    }
  }
  strip.Show();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  static uint32_t lastMillis = 0;

  //Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        // Send the current LED status
        switch (LEDStatus) {
          case 0:
            webSocket.sendTXT(num, LEDOFF, strlen(LEDOFF));
            break;
          case 128:
            webSocket.sendTXT(num, LED50, strlen(LED50));
            break;
          case 255:
            webSocket.sendTXT(num, LEDON, strlen(LEDON));
            break;
        }
      }
      break;
    case WStype_TEXT:
      //Serial.printf("[%u] [%u ms] get Text: %s\r", num, millis()-lastMillis, payload);
      lastMillis = millis();
      if (strcmp((const char *)payload, LEDON) == 0) {
        Serial.println(LEDON);
        writeLED(255);
        // send data to all connected clients
        webSocket.broadcastTXT(payload, length);
      }
      else if (strcmp((const char *)payload, LED50) == 0) {
        Serial.println(LED50);
        writeLED(128);
        // send data to all connected clients
        webSocket.broadcastTXT(payload, length);
      }
      else if (strcmp((const char *)payload, LEDOFF) == 0) {
        Serial.println(LEDOFF);
        writeLED(0);
        // send data to all connected clients
        webSocket.broadcastTXT(payload, length);
      }
      else if (strcmp((const char *)payload, "bForward=1") == 0) {
        Serial.println(F("Forward"));
        leftMotor->run(FORWARD);
        rightMotor->run(FORWARD);
      }
      else if (strcmp((const char *)payload, "bForward=0") == 0) {
        Serial.println(F("Stop"));
        leftMotor->run(RELEASE);
        rightMotor->run(RELEASE);
      }
      else if (strcmp((const char *)payload, "bBackward=1") == 0) {
        Serial.println(F("Backward"));
        leftMotor->run(BACKWARD);
        rightMotor->run(BACKWARD);
      }
      else if (strcmp((const char *)payload, "bBackward=0") == 0) {
        Serial.println(F("Stop"));
        leftMotor->run(RELEASE);
        rightMotor->run(RELEASE);
      }
      else if (strcmp((const char *)payload, "bLeft=1") == 0) {
        Serial.println(F("Left"));
        leftMotor->run(FORWARD);
        rightMotor->run(BACKWARD);
      }
      else if (strcmp((const char *)payload, "bLeft=0") == 0) {
        Serial.println(F("Stop"));
        leftMotor->run(RELEASE);
        rightMotor->run(RELEASE);
      }
      else if (strcmp((const char *)payload, "bRight=1") == 0) {
        Serial.println(F("Right"));
        leftMotor->run(BACKWARD);
        rightMotor->run(FORWARD);
      }
      else if (strcmp((const char *)payload, "bRight=0") == 0) {
        Serial.println(F("Stop"));
        leftMotor->run(RELEASE);
        rightMotor->run(RELEASE);
      }
      else {
        Serial.printf("Unknown command %s\r\n", payload);
      }
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}

void handleRoot()
{
  Serial.println(F("Serving index.html"));
  webServer.send(200, "text/html", INDEX_HTML);
}

void setup() {
  Serial.begin(115200);           // set up Serial library at 9600 bps
  Serial.println(F("\nMini Rover with Access Point"));

  // Turn LEDs off
  strip.Begin();
  strip.Show();

  // Initialize the motors
  AFMS.begin();  // create with the default frequency 1.6KHz

  leftMotor->setSpeed(150);
  leftMotor->run(RELEASE);

  rightMotor->setSpeed(150);
  rightMotor->run(RELEASE);

  // Start up AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  // replay to all requests with same HTML
  webServer.onNotFound(handleRoot);
  webServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  dnsServer.processNextRequest();
  webSocket.loop();
  webServer.handleClient();
}
