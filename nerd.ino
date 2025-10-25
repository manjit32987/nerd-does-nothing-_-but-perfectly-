#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "time.h"

// --- Wi-Fi credentials ---
const char* ssid = "Moto";
const char* password = "12345679";

// --- Timezone info ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST +5:30
const int   daylightOffset_sec = 0;

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// Eye variables
int leftEyeX = 45, rightEyeX = 80, eyeY = 18;
int eyeWidth = 25, eyeHeight = 30;
int targetOffsetX = 0, targetOffsetY = 0;
int moveSpeed = 2, blinkState = 0;
int blinkDelay = 4000;
unsigned long lastBlinkTime = 0, moveTime = 0;

// Touch + emotion
#define TOUCH_PIN 4
bool angryActive = false;
bool showText = false;
bool showTime = false;
unsigned long angryStartTime = 0;
unsigned long touchStartTime = 0;
const int angryDuration = 5000;       // 5 seconds angry face
const int textDuration = 1000;        // 1 second text
const int longPressTime = 2000;       // 2 sec long press
const int timeDisplayDuration = 5000; // 5 sec display of time
int lastTouchState = 0;               // For edge detection

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  display.begin();

  // Wi-Fi connection
  WiFi.begin(ssid, password);
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB14_tr);
  display.drawStr(10, 35, "Connecting WiFi...");
  display.sendBuffer();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Startup message
  display.clearBuffer();
  display.drawStr(35, 35, "Hello!");
  display.sendBuffer();
  delay(2000);
}

void loop() {
  unsigned long currentTime = millis();
  int touchState = digitalRead(TOUCH_PIN);

  // --- Long press detection ---
  if (touchState == 1 && lastTouchState == 0) {
    touchStartTime = currentTime; // start timing
  }

  // --- Trigger long press ---
  if (touchState == 1 && (currentTime - touchStartTime >= longPressTime) && !showTime) {
    showTime = true;         // start showing time
    showText = false;
    angryActive = false;
    angryStartTime = currentTime; // reference for 5 sec display
  }

  // --- Trigger short tap on release ---
  if (touchState == 0 && lastTouchState == 1) {
    // if it was a quick tap (< longPressTime)
    if (currentTime - touchStartTime < longPressTime && !showTime) {
      showText = true;
      angryActive = false;
      angryStartTime = currentTime;
    }
  }

  lastTouchState = touchState;

  // --- Handle text display ---
  if (showText) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(10, 35, "What happened mf!");
    display.sendBuffer();

    if (currentTime - angryStartTime >= textDuration) {
      showText = false;
      angryActive = true;
      angryStartTime = currentTime;
    }
    return; // skip rest of loop while showing text
  }

  // --- Handle time display for 5 seconds ---
  if (showTime) {
    if (currentTime - angryStartTime >= timeDisplayDuration) {
      showTime = false; // stop showing after 5 seconds
    } else {
      struct tm timeinfo;
      if(!getLocalTime(&timeinfo)) return; // skip if failed

      // --- Convert to 12-hour format (no AM/PM) ---
      int hour12 = timeinfo.tm_hour % 12;
      if (hour12 == 0) hour12 = 12;

      // --- Format time and date ---
      char timeBuffer[20];
      sprintf(timeBuffer, "%02d:%02d:%02d", hour12, timeinfo.tm_min, timeinfo.tm_sec);

      char dateBuffer[20];
      sprintf(dateBuffer, "%02d %s %04d", timeinfo.tm_mday, monthShortName(timeinfo.tm_mon + 1), 1900 + timeinfo.tm_year);

      // --- Display on OLED ---
      display.clearBuffer();
      display.setFont(u8g2_font_ncenB14_tr);
      display.drawStr(28, 28, timeBuffer); // time on first line
      display.drawHLine(10, 34, 108);      // horizontal line
      display.setFont(u8g2_font_6x10_tr);
      display.drawStr(35, 52, dateBuffer); // date below
      display.sendBuffer();
      return;
    }
  }

  // --- Stop angry after duration ---
  if (angryActive && currentTime - angryStartTime >= angryDuration) {
    angryActive = false;
  }

  // --- Blink logic ---
  if (currentTime - lastBlinkTime > blinkDelay && blinkState == 0) {
    blinkState = 1;
    lastBlinkTime = currentTime;
  } else if (currentTime - lastBlinkTime > 150 && blinkState == 1) {
    blinkState = 0;
    lastBlinkTime = currentTime;
  }

  // --- Random eye movement ---
  if (currentTime - moveTime > random(1500, 3000) && blinkState == 0) {
    int movementType = random(0, 8);
    switch (movementType) {
      case 0: targetOffsetX=-10; targetOffsetY=0; break;
      case 1: targetOffsetX=10; targetOffsetY=0; break;
      case 2: targetOffsetX=-10; targetOffsetY=-8; break;
      case 3: targetOffsetX=10; targetOffsetY=-8; break;
      case 4: targetOffsetX=-10; targetOffsetY=8; break;
      case 5: targetOffsetX=10; targetOffsetY=8; break;
      default: targetOffsetX=0; targetOffsetY=0; break;
    }
    moveTime = currentTime;
  }

  static int offsetX=0, offsetY=0;
  offsetX += (targetOffsetX - offsetX)/moveSpeed;
  offsetY += (targetOffsetY - offsetY)/moveSpeed;

  // --- Draw eyes ---
  if (angryActive) {
    drawAngryEyes(offsetX, offsetY, blinkState);
  } else {
    drawNormalEyes(offsetX, offsetY, blinkState);
  }

  display.sendBuffer();
  delay(30);
}

// --- Normal Eyes ---
void drawNormalEyes(int offsetX,int offsetY,int blink) {
  display.clearBuffer();
  
  if (blink==0) {
    display.drawRBox(leftEyeX+offsetX, eyeY+offsetY, eyeWidth, eyeHeight, 5);
    display.drawRBox(rightEyeX+offsetX, eyeY+offsetY, eyeWidth, eyeHeight, 5);
  } else {
    display.drawBox(leftEyeX+offsetX, eyeY+offsetY+eyeHeight/2-2, eyeWidth, 4);
    display.drawBox(rightEyeX+offsetX, eyeY+offsetY+eyeHeight/2-2, eyeWidth, 4);
  }
}

// --- Angry Eyes ---
void drawAngryEyes(int offsetX,int offsetY,int blink) {
  display.clearBuffer();
  int angryEyeHeight = eyeHeight-10;
  
  if(blink==0){
    display.drawRBox(leftEyeX+offsetX, eyeY+5+offsetY, eyeWidth, angryEyeHeight,5);
    display.drawRBox(rightEyeX+offsetX, eyeY+5+offsetY, eyeWidth, angryEyeHeight,5);
  } else {
    display.drawBox(leftEyeX+offsetX, eyeY+5+offsetY+angryEyeHeight/2-2, eyeWidth,4);
    display.drawBox(rightEyeX+offsetX, eyeY+5+offsetY+angryEyeHeight/2-2, eyeWidth,4);
  }

  // Angry eyebrows
  display.drawLine(leftEyeX+offsetX, eyeY+offsetY, leftEyeX+eyeWidth+offsetX, eyeY-5+offsetY);
  display.drawLine(rightEyeX+offsetX, eyeY-5+offsetY, rightEyeX+eyeWidth+offsetX, eyeY+offsetY);
}

// --- Helper: month abbreviation ---
const char* monthShortName(int month) {
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    if (month < 1 || month > 12) return "??";
    return months[month-1];
}
void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
