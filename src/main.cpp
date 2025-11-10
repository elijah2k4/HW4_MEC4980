#include <WiFi.h>
#include <time.h>
#include "Stepper.h"

// --- Stepper setup ---
const int STEPS_PER_REV = 200;                
const int STEPS_PER_MINUTE = 170;               
const int STEPS_PER_HALF_HOUR = STEPS_PER_MINUTE * 30;
const int HOUR_HAND_STEPS_PER_HALF_HOUR = 160;  

// --- Motor pin setup ---
Stepper mHand = Stepper(9, 10, 0, 0, 0);   // minute motor
Stepper hHand = Stepper(11, 12, 0, 0, 0);  // hour motor

// --- WiFi + Time setup ---
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -6 * 3600;  
const int DAYLIGHT_OFFSET_SEC = 0;

int currentHour, currentMinute;

void setup() {
  Serial.begin(115200);
  WiFi.begin("NSA Security Van HQ", "windowstothehallway");
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  mHand.begin();
  mHand.enable();
  mHand.setMoveSpeed(200.0f); 
  hHand.begin();
  hHand.enable();
  hHand.setMoveSpeed(200.0f); 


  currentMinute = timeinfo.tm_min;
  currentHour = timeinfo.tm_hour;

  // Initialize positions 
  // minute hand - find its correct step position within the current half hour
  int minuteWithinHalfHour = currentMinute % 30;
  long startingMinuteStep = minuteWithinHalfHour * STEPS_PER_MINUTE;
  mHand.moveTo(startingMinuteStep);

  // hour hand - move according to which half hour its in
  int hourHalfCycle = (currentHour * 2) + (currentMinute >= 30 ? 1 : 0);
  long startingHourStep = hourHalfCycle * HOUR_HAND_STEPS_PER_HALF_HOUR;
  hHand.moveTo(startingHourStep);

  Serial.printf("Start time: %02d:%02d → moving to minute step: %ld, hour step: %ld\n",
                currentHour, currentMinute, startingMinuteStep, startingHourStep);
}

void loop() {
  // Run the initialization just once
  static bool initialized = false;       
  static unsigned long lastPrint = 0;    
  static int lastMinute = -1;
  static int lastHour = -1;

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int currentHour   = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentSecond = timeinfo.tm_sec;

    // Print current time once per second 
    if (millis() - lastPrint >= 1000) {
      Serial.printf("Current time: %02d:%02d:%02d\n", currentHour, currentMinute, currentSecond);
      lastPrint = millis();
    }

    // One time initialization
    if (!initialized) {
      // Minute hand - position within the current 30-minute half-cycle
      int minuteWithinHalfHour = currentMinute % 30;
      long startingMinuteStep = minuteWithinHalfHour * STEPS_PER_MINUTE;

      // Hour hand - step position based on hours + minute offset
      int halfHoursSinceMidnight = (currentHour * 2) + (currentMinute >= 30 ? 1 : 0);
      long startingHourStep = (halfHoursSinceMidnight % 24) * HOUR_HAND_STEPS_PER_HALF_HOUR;

      Serial.printf("Initializing minute hand to step %ld (minute %d)\n",
                    startingMinuteStep, currentMinute);
      Serial.printf("Initializing hour hand to step %ld (hour %d)\n",
                    startingHourStep, currentHour);

      // Move both hands to their initial positions
      mHand.moveTo(startingMinuteStep);
      hHand.moveTo(startingHourStep);

      // Run both motors until they reach their positions
      unsigned long start = millis();
      while (((mHand.getPosition() != startingMinuteStep) ||
              (hHand.getPosition() != startingHourStep)) &&
              (millis() - start < 8000)) {
        unsigned long nowMicros = micros();
        mHand.update(nowMicros);
        hHand.update(nowMicros);
      }
      
      // Done initializing
      initialized = true;  
      lastMinute = currentMinute;
      lastHour = currentHour;
    }

    // Step once per new minute 
    if (currentMinute != lastMinute) {
      // Move minute hand one step set per minute
      mHand.moveTo(mHand.getTarget() + STEPS_PER_MINUTE);

      // If it has completed 30 minutes, reset to zero
      if ((currentMinute % 30) == 0) {
        mHand.moveTo(0);
      }

      lastMinute = currentMinute;
    }
    
    
    static int lastHalfHour = -1;
    int currentHalfHour = (currentHour * 2) + (currentMinute >= 30 ? 1 : 0);
    if (currentHalfHour != lastHalfHour) {
      hHand.moveTo(hHand.getTarget() + HOUR_HAND_STEPS_PER_HALF_HOUR);
      lastHalfHour = currentHalfHour;
    }


  // Updating the steppers
  unsigned long nowMicros = micros();
  mHand.update(nowMicros);
  hHand.update(nowMicros);
}
}
