#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include "font.h"
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

// Software Serial for MP3 Player
SoftwareSerial mySoftwareSerial(D3, D4); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

// Which pins are connected to the Raspberry Pi for network activity
#define pi_tx D7
#define pi_rx D0

// Which pin is connected to the NeoPixels?
#define neopixel_pin        D8

// How many NeoPixels are attached?
#define neopixel_count      4

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(neopixel_count, neopixel_pin, NEO_GRB + NEO_KHZ800);

// Maximum length of message we will display
#define message_buffer_size 64

//Pin connected to ST_CP (pin 12) of 74HC595
int latchPin = D5;
//Pin connected to SH_CP (pin 11) of 74HC595
int clockPin = D2;
////Pin connected to DS (pin 14) of 74HC595
int dataPin = D6;

bool audioIsPlaying = false;
int audioQueue[20];
int audioPosition = 0;
int audioLength = 0;
unsigned long audioLastMillis = 0;

const char* ssid = "ssid";
const char* password = "password";
const char* wifi_hostname = "supra";

ESP8266WebServer server(80);

// Screen Saver pixel structure
struct ss_pixel
{
  unsigned char x;
  unsigned char y;
  unsigned char d;
  unsigned char v;
  unsigned char a;
  unsigned char b;
};

// Screen Saver led structure
struct ss_led
{
  int value;
  int direction;
};

// Maximum number of pixels displayed on screen saver at once
#define ss_pixel_count 20
// Number of neopixels to use for screensaver
#define ss_led_count 4
// Maximum brightness of screensaver neopixels
#define ss_pixel_max_value 8

// Array of ss_pixel, used to keep track of individual pixels in screen saver
struct ss_pixel ss_pixels[ss_pixel_count];
// Array of ss_led, used to keep track of individual neopixels in screen saver
struct ss_led ss_leds[ss_led_count];

int animation = 0;

// If true, neopixels 1 and 2 will turn on/off based on state of tx_pi and rx_pi
bool piNetworkLEDs = true;


void setup() {
  randomSeed(analogRead(0));

  // Set pi network activity pins to INPUT
  pinMode(pi_rx, INPUT);
  pinMode(pi_tx, INPUT);
  
  Serial.begin(115200);
  
  // set shift register pins to output
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  WiFi.hostname( wifi_hostname );

  WiFi.begin(ssid, password);

  // Start neopixels
  pixels.begin();

  // Set TR light to ON by default, the rest to off.
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.setPixelColor(1, pixels.Color(0, 0, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 0));
  pixels.setPixelColor(3, pixels.Color(11, 0, 0));
  pixels.show();
}

// Used for translating bytes to binary format for debug output
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

unsigned char bufferMatrix[7]; // The incoming character
unsigned char leftMatrix[7];   // Data currently displayed on the left side matrix
unsigned char rightMatrix[7];  // Data currently displayed on the right side matrix
unsigned char gapBits[7];         // bit buffer for the gap between displays

char message[message_buffer_size] = "OK";
int msPerFrame = 40;

int framePosition = 0;  // Used for animation routines
int stringPosition = 0; // Character position in the message, wraps at end of message
int bufferPosition = 0; // Current bit offset within buffer. When this reaches 8, we need to bring in a new character from the message
unsigned char wifiConnected = false;

void resetScrollingMessage() {
  // Reset positions
  stringPosition = 0;
  bufferPosition = 0;
  framePosition = 0;
  animation = 0;

  // Clear bit buffers
  memset(bufferMatrix, 0, 7);
  memset(leftMatrix, 0, 7);
  memset(rightMatrix, 0, 7);
  memset(gapBits, 0, 7);
}

void handleRequest() {
  server.send(200, "text/plain", "OK MESSAGE RECEIVED");
  msPerFrame = 40;
  bool setPixels = false;
   for (uint8_t i=0; i<server.args(); i++){
    if (strcmp(server.argName(i).c_str(), "message") == 0) {
      // clear message buffer
      memset(message, 0, message_buffer_size); 
      // copy incoming message to message buffer, up to maximum message buffer size minus one (to allow for null termination)
      memcpy(message, server.arg(i).c_str(), strlen(server.arg(i).c_str())>=message_buffer_size ? message_buffer_size-1 : strlen(server.arg(i).c_str()));
      
      resetScrollingMessage();
    } else
    if (strcmp(server.argName(i).c_str(), "speed") == 0) {
      msPerFrame = atoi(server.arg(i).c_str());
    } else
    if (strcmp(server.argName(i).c_str(), "matrix") == 0) {
      framePosition = 0;
      animation = 1;
      piNetworkLEDs = false; // disable network LEDs while running screensaver
    } else
    if (strcmp(server.argName(i).c_str(), "dial") == 0) {
      char arg[20];
      
      audioLength = strlen(server.arg(i).c_str());
      if (audioLength > 20) audioLength = 20;
      memcpy(arg, server.arg(i).c_str(), audioLength);
      
      for (int i=0;i<audioLength;i++) {
        if (arg[i] >= '0' && arg[i] <= '9') audioQueue[i] = (arg[i] - '0') + 10;
        if (arg[i] >= 'A' && arg[i] <= 'Z') audioQueue[i] = (arg[i] - 'A') + 1;
      }
      audioPosition = 0;
    } else
    if (strcmp(server.argName(i).c_str(), "netleds") == 0) {
      piNetworkLEDs = true;
    } else
    if (strncmp(server.argName(i).c_str(), "led", 3) == 0) {
      if (strlen(server.arg(i).c_str())>=6) {
        char arg[8];
        char argName[8];
        char r[3];
        char g[3];
        char b[3];
        unsigned char led;

        memcpy(argName, server.argName(i).c_str(), 4);
        memcpy(arg, server.arg(i).c_str(), 7);
        led = argName[3]-'1';

        memset(r, 0, 3);
        memset(g, 0, 3);
        memset(b, 0, 3);

        memcpy(r, arg, 2);
        memcpy(g, arg+2, 2);
        memcpy(b, arg+4, 2);

        char debug[64];
        
        pixels.setPixelColor(led, pixels.Color((unsigned char)strtol(r, NULL, 16),(unsigned char)strtol(g, NULL, 16),(unsigned char)strtol(b, NULL, 16)));
        setPixels = true;
        if (led == 2 || led == 3) piNetworkLEDs = false;
      }
    }
  }

  if (setPixels) pixels.show();
}

int nextMarqueeFrame() {
  if (framePosition == 0) {
    for (int i=0;i<7;i++) {
      rightMatrix[i] = 0;
      leftMatrix[i] = 0;
      bufferMatrix[i] = console_font_6x8[((int)(message[stringPosition])*8)+i];
    }
  } else {
    if (strlen(message)>2) {
      for (int i=0;i<7;i++) {
        unsigned char rightWrap = rightMatrix[i] & 16;
        unsigned char leftWrap = leftMatrix[i] & 16;
        unsigned char bufferWrap = bufferMatrix[i] & 128;

        // shift gap bits over by one
        gapBits[i] = gapBits[i] << 1;
  
        // shift both matrices over by one
        rightMatrix[i] = rightMatrix[i] << 1;
        if (!(strlen(message)==2 && framePosition > 11)) {
          leftMatrix[i] = leftMatrix[i] << 1;
        }
        bufferMatrix[i] = bufferMatrix[i] << 1;
        
        // The physical gap between our two displays is about three pixel wide. If we have a bit in the third position of gapBits, OR it on to the left matrix
        if (gapBits[i] & 0b00000100) leftMatrix[i] |= 0b00000001;
        // If we have an overflow bit from the right display, OR it in to the gap bits
        if (rightWrap) gapBits[i] |= 0b00000001;
       
        
        if (bufferWrap) rightMatrix[i] |= 0b00000001;
  
        // clear any high bits leftover
        rightMatrix[i] = rightMatrix[i] << 3;
        rightMatrix[i] = rightMatrix[i] >> 3;
        leftMatrix[i] = leftMatrix[i] << 3;
        leftMatrix[i] = leftMatrix[i] >> 3;
      }

      // bring in the next character, if any
      bufferPosition++;
      if (bufferPosition == 8) {
        bufferPosition = 0;
        stringPosition++;
        if (stringPosition >= strlen(message)) {
          stringPosition = 0;  
        }
        for (int j=0;j<7;j++) {
          bufferMatrix[j] = console_font_6x8[(message[stringPosition]*8)+j];
        }
      }
      
    } else { // If we only have two characters to display, just show them without any scrolling
      for (int i=0;i<7;i++) {
        leftMatrix[i] = console_font_6x8[((int)(message[0])*8)+i] >> 2;
        rightMatrix[i] = console_font_6x8[((int)(message[1])*8)+i] >> 2;
      }
    }
  }
  framePosition++;

  return msPerFrame;
}

int nextScreenSaverFrame() {
  if (framePosition == 0) {
    for (int i = 0; i < ss_pixel_count; i++) {
      ss_pixels[i].y = 7;
    }
    for (int i = 0; i < ss_led_count; i++) {
      ss_leds[i].value = (i*2)+1;
      ss_leds[i].direction = 1;
    }
    framePosition++;
  }

  // clear display buffers
  memset(rightMatrix, 0, 8);
  memset(leftMatrix, 0, 8);

  // "matrix" style raining pixels
  for (int i = 0; i < ss_pixel_count; i++) {
    if (ss_pixels[i].y < 7) {
      if (random(0,10) <= ss_pixels[i].b || ss_pixels[i].y == 0) { // brightness
        if (ss_pixels[i].d == 0) {
          leftMatrix[ss_pixels[i].y] |= 1 << ss_pixels[i].x;
        } else {
          rightMatrix[ss_pixels[i].y] |= (unsigned char)(1 << ss_pixels[i].x);
        }
        ss_pixels[i].a++;
        if (ss_pixels[i].a >= ss_pixels[i].v) {
          ss_pixels[i].y++;
          ss_pixels[i].a = 0;
        }
      }
    } else if (random(0,10)<3) {
      ss_pixels[i].x = random(0,5);
      ss_pixels[i].d = random(0,2);
      ss_pixels[i].v = (random(5,10)*2) + (random(0,50)<10 ? 20 : 0);
      ss_pixels[i].b = random(2,10);
      ss_pixels[i].y = 0;
      ss_pixels[i].a = 0;
    }
  }

  // pulse the LEDs
  if (framePosition == 1 || framePosition == 10) {
    for (int i = 0; i < ss_led_count; i++) {

      // change direction if we hit zero or ss_pixel_max_value
      if (ss_leds[i].value == 0 || ss_leds[i].value >= ss_pixel_max_value) ss_leds[i].direction *= -1;

      // move value in direction
      ss_leds[i].value += ss_leds[i].direction;

      // Set pixel color
      pixels.setPixelColor(i, pixels.Color(ss_leds[i].value/2,ss_leds[i].value,ss_leds[i].value/2));
    }

    // send current colors to pixels
    pixels.show();

    // reset frame position
    framePosition = 2;
  } else framePosition++;

  // tell loop we'd like 1ms before being called again
  return 1;
}

int nextFrame() {
  if (animation == 0) {
    return nextMarqueeFrame();
  } else {
    return nextScreenSaverFrame();
  }
}

void displayFrame() {
    unsigned char sequence[] = {128,64,32,16,8,4,2}; // bitmasks for row selection
  
    /*
     * Retrieve and display a frame for X milliseconds
     */
    int displayFrameFor = nextFrame();

    unsigned int startMillis = millis();
    while (millis() < startMillis + displayFrameFor) {
      for (int i=0;i<7;i++) {
        unsigned char displayByte = rightMatrix[i];
        unsigned char displayByte2 = leftMatrix[i];

        /*
         First Shift Register Bits:  AR0 AR1 AR2 AR3 AR4 AR5 AR6 BR0    
         Second Shift Register Bits: BR1 BR2 BR3 BR4 BR5 BR6 AC0 AC1  
         Third Shift Register Bits:  AC2 AC3 AC4 BC0 BC1 BC2 BC3 BC4

         A/B = Matrix A or B
         R/C = Row or Column

         AR0 = Matrix A, Row 0
         BC0 = Matrix B, Column 0
         
         */

        // disp1 will hold the 2 most significant bits of the right matrix
        unsigned char disp1 = displayByte;
        disp1 = disp1 >> 3; // shift over so that we only have 2 of the 5 used bits remaining

        // disp2 will hold the three least significant bits of the right matrix
        unsigned char disp2 = displayByte;
        disp2 = disp2 << 5; // shift to the left so that 3 of the 5 used bits are aligned to the most significant side of the byte

        // mask the sequence
        unsigned char seq1 = (unsigned char)(((sequence[i] ^ 255)));
        unsigned char seq2 = seq1 << 1; // most significant bit of seq2 will be stored in BR0

        // Clear the two least significant bits of seq2 since we'll OR in AC0 and AC1 from disp1 later
        seq2 = seq2 >> 2;
        seq2 = seq2 << 2;
        
        unsigned char BR0 = 1; // By default, we aren't displaying row zero, so this pin should be high.
        if (sequence[i] == 128) BR0 = 0; // If we are currently displaying row zero, lower (ground) BR0

        // Clear LSbit of seq1 since we'll add BR0 in to it later
        seq1 = seq1 >> 1;
        seq1 = seq1 << 1;

        unsigned char rA = seq1 | BR0; // AR0 thru AR6 are in seq1, mask in BR0
        unsigned char rB = seq2 | disp1;   // BR1 thru BR6 are in seq2, we mask in AC0 and AC1 from disp1
        unsigned char rC = disp2 | displayByte2; // AC2 thru AC4 are in disp2, we mask in BC0 thru BC4 from displayByte2

        // Clock out all three bytes. Since the shift registers are daisy chained, 
        // the first byte we send out will be shifted over by the next, and the next,
        // so that the first one we send ends up in Register C, the second in register B,
        // and the third in register A.
        digitalWrite(latchPin, LOW);
        shiftOut(dataPin, clockPin, LSBFIRST, (unsigned char)rC);
        shiftOut(dataPin, clockPin, LSBFIRST, (unsigned char)rB);
        shiftOut(dataPin, clockPin, LSBFIRST, (unsigned char)rA);
        digitalWrite(latchPin, HIGH);
        delay(1); // no delay seems to upset esp8266
      }
    }
}

void processAudio() {
  #ifdef HAS_DFPLAYER
  if (audioIsPlaying) {
    if (myDFPlayer.available()) {
      uint8_t type = myDFPlayer.readType();
      int value = myDFPlayer.read();

      if (type == DFPlayerPlayFinished || millis() > (audioLastMillis+2000)) audioIsPlaying = false;
    }
  } else {
    if (audioPosition < audioLength && millis() > (audioLastMillis+500)) {
      char debug[40];
      sprintf(debug, "Playing pos %u track %u", audioPosition, audioQueue[audioPosition]);
      Serial.println(debug);
      audioIsPlaying = true;
      myDFPlayer.play(audioQueue[audioPosition]);
      audioPosition++;
      audioLastMillis = millis();
    }
  }
  #endif
}

void processPiNetworkLEDs() {
  if (piNetworkLEDs) {
    if (digitalRead(pi_rx)) pixels.setPixelColor(1, pixels.Color(11, 0, 0));
    else pixels.setPixelColor(1, pixels.Color(0,0,0));
    if (digitalRead(pi_tx)) pixels.setPixelColor(2, pixels.Color(11, 0, 0));
    else pixels.setPixelColor(2, pixels.Color(0,0,0));
    pixels.show();
  }
}

void loop() {
  //return;

  while (true) {
    if (!wifiConnected) {
      if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;

        // Copy IP address in to message buffer
        sprintf(message, "%s   ", WiFi.localIP().toString().c_str());
        resetScrollingMessage();
        

        MDNS.begin(wifi_hostname);

        server.on("/", handleRequest);
        server.begin();

        //start sound - wait til wifi is connected, powering on the dfplayer beforehand seems to cause issues w connection.
        #ifdef HAS_DFPLAYER
          mySoftwareSerial.begin(9600); // for df player
          myDFPlayer.begin(mySoftwareSerial);
          myDFPlayer.setTimeOut(500);
          myDFPlayer.volume(30);
          myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
        #endif
      }
    } else {
      server.handleClient();
    }
    
    displayFrame();
    processAudio();
    processPiNetworkLEDs();
  }
  

/*char str[64];

        Serial.println("------");
         sprintf(str, "m: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  BYTE_TO_BINARY(disp1>>8), BYTE_TO_BINARY(disp1));
        Serial.println(str);

        sprintf(str, "m: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  BYTE_TO_BINARY(seq3>>8), BYTE_TO_BINARY(seq3));
        Serial.println(str);

        sprintf(str, "m: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  BYTE_TO_BINARY(rB>>8), BYTE_TO_BINARY(rB));
        Serial.println(str);*/

}
