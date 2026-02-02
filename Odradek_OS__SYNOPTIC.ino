#include <WiFi.h>
#include <Update.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>      
#include <ArduinoJson.h>    
#include <esp_task_wdt.h>
#include "time.h"

// Hardware pin connection points
#define SOLENOID_PIN 18     
#define LED_DATA_PIN 27     
#define BUZZER_PIN   26     
#define TOUCH_PIN    4      
#define NUM_LEDS     123

// Watchdog timer (my mortal enemy)
#define WDT_TIMEOUT 30  // 30 seconds

// CONFIGURATIONSSS
const char* ssid = "[YOUR_WIFI_NAME]";
const char* password = "[YOUR_WIFI_PASSWORD]";
const char* ntpServer = "pool.ntp.org"; 
const long  gmtOffset_sec = 3600;   //this is meant to allign with my time zone (basically NL)
const int   daylightOffset_sec = 3600;   //same shit here

// Weather APIs with additional data already in, just fill coordinates via LAT+LON in and youll get valid API data in for free.
const char* weatherLOCATION_1 = "https://api.open-meteo.com/v1/forecast?latitude=[YOUR_LAT]&longitude=[YOUR_LON]&current=rain,relative_humidity_2m,weather_code,temperature_2m&hourly=rain,precipitation_probability&timezone=auto";
const char* weatherLOCATION_2 = "https://api.open-meteo.com/v1/forecast?latitude=[YOUR_LAT]&longitude=[YOUR_LON]&current=rain,relative_humidity_2m,weather_code,temperature_2m&hourly=rain,precipitation_probability&timezone=auto";
const char* weatherLOCATION_3 = "https://api.open-meteo.com/v1/forecast?latitude=[YOUR_LAT]&longitude=[YOUR_LON]&current=rain,relative_humidity_2m,weather_code,temperature_2m&hourly=rain,precipitation_probability&timezone=auto";


IPAddress apIP(172, 217, 28, 1);
WebServer server(80);
Adafruit_NeoPixel strip(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

// --- GLOBAL STATE ---
volatile bool lampMode = false;
volatile bool warmLampMode = false;
volatile bool warningMode = false;
volatile bool standbyMode = false; 
volatile bool buzzerMuted = false;
volatile bool solenoidDisabled = false;
volatile bool btScanMode = false;
volatile bool cargoScanMode = false;
volatile bool hasShiveredToday = false;
volatile bool emergencyStop = false;
bool otaInProgress = false;
volatile int ledBrightness = 150;
volatile int connectionStrength = 0;
unsigned long lastWeatherCheck = 0; 
const int weatherCheckInterval = 120000;
bool wifiErrorState = false;

// CPU Load Tracking - NEW
TaskHandle_t Core0TaskHandle = NULL;
TaskHandle_t Core1TaskHandle = NULL;
unsigned long lastCpuCheck = 0;
unsigned long lastCore0Runtime = 0;
unsigned long lastCore1Runtime = 0;
unsigned long lastTotalRuntime = 0;

// Time restrictions for reactions (10 AM to 9 PM)
const int REACTION_START_HOUR = 10;
const int REACTION_END_HOUR = 21;

// Alarm system with multiple slots
struct Alarm {
  bool active;
  int hour;
  int minute;
  int type; // 0=morning, 1=reminder
};
Alarm alarms[5]; // Up to 5 alarms
int alarmCount = 0;

// Weather data storage
float humidityLOCATION_1 = 0;
float humidityLOCATION_3 = 0;
float humidityLOCATION_2 = 0;
float tempLOCATION_1 = 0;
float tempLOCATION_3 = 0;
float tempLOCATION_2 = 0;
bool isRainingLOCATION_1 = false;
bool isRainingLOCATION_3 = false;
bool isRainingLOCATION_2 = false;
float rainIntensityLOCATION_1 = 0;
float rainIntensityLOCATION_3 = 0;
float rainIntensityLOCATION_2 = 0;
int rainChanceLOCATION_1 = 0;
int rainChanceLOCATION_3 = 0;
int rainChanceLOCATION_2 = 0;
String skyConditionLOCATION_1 = "Unknown";
String skyConditionLOCATION_3 = "Unknown";
String skyConditionLOCATION_2 = "Unknown";
String lastWeatherUpdate = "--:--";

// ESP32 System Stats
unsigned long lastStatsUpdate = 0;
int wifiRSSI = 0;
uint32_t freeHeap = 0;
uint32_t totalHeap = 0;
float cpuLoad = 0;
float core0Load = 0;
float core1Load = 0;
bool hasStressed = false; // CPU stress indicator
unsigned long lastCore0Time = 0;
unsigned long lastCore1Time = 0;

// --- ANIMATION TRIGGER ---
volatile int animationTrigger = 0; 

// --- TOUCH ENGINE STATE ---
unsigned long lastTapTime = 0;
unsigned long touchStartTime = 0;
bool lastTouchState = false;
int tapCount = 0;
const int tapInterval = 400;
const int longPressDuration = 2000;

// --- AUDIO FREQUENCIES ---
const int FREQ_HIGH_OVERTONE = 7900;
const int FREQ_1 = 7800;
const int FREQ_2 = 6016;
const int FREQ_3 = 5763;  
const int FREQ_4 = 5300;
const int FREQ_LOW_FUNDAMENTAL = 3400;
const int LEDC_RESOLUTION = 8;

// ==========================================
// 1. CORE ENGINE FUNCTIONS
// ==========================================

String weatherCodeToCondition(int code) {
  if (code == 0) return "Clear";
  else if (code <= 3) return "Partly Cloudy";
  else if (code <= 48) return "Foggy";
  else if (code <= 67) return "Rainy";
  else if (code <= 77) return "Snowy";
  else if (code <= 82) return "Showers";
  else if (code == 95 || code == 96 || code == 99) return "Thunderstorm";
  else return "Unknown";
}

bool isWithinReactionHours() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return true; // Default to allowing if time unavailable
  return (timeinfo.tm_hour >= REACTION_START_HOUR && timeinfo.tm_hour < REACTION_END_HOUR);
}

// NEW: Cold shiver animation
void coldShiver() {
  if(!isWithinReactionHours() || hasShiveredToday) return;
  hasShiveredToday = true;
  
  // Freezing animation (2 seconds)
  for(int i=0; i<20; i++) {
    strip.fill(strip.Color(100, 150, 255)); 
    strip.show();
    // PWM shaking solenoid
    ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 80 + random(0, 40));
    delay(50);
    strip.fill(strip.Color(150, 200, 255));
    strip.show();
    delay(50);
  }
  ledcWrite(SOLENOID_PIN, 0);
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
  Serial.println("Cold shiver triggered");
}

// NEW: CPU stress animation
void cpuStressAnimation() {
  if(!isWithinReactionHours() || hasStressed) return;
  hasStressed = true;
  
  // Overload panic (3 seconds)
  for(int cycle=0; cycle<6; cycle++) {
    // Rapid red flashing
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 255);
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 8000);
    delay(200);
    
    strip.fill(strip.Color(255, 100, 0));
    strip.show();
    ledcWrite(SOLENOID_PIN, 0);
    ledcWriteTone(BUZZER_PIN, 0);
    delay(300);
  }
  
  ledcWrite(SOLENOID_PIN, 0);
  ledcWriteTone(BUZZER_PIN, 0);
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
  Serial.println("CPU stress animation triggered");
}

// NEW: Low memory animation + restart
void lowMemoryRestart() {
  Serial.println("!!! LOW MEMORY - Restarting ESP32 !!!");
  
  // Quick memory warning (2 seconds)
  for(int i=0; i<4; i++) {
    strip.fill(strip.Color(255, 255, 0));
    strip.show();
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 4000);
    delay(250);
    strip.fill(0);
    strip.show();
    ledcWriteTone(BUZZER_PIN, 0);
    delay(250);
  }
  
  delay(500);
  ESP.restart();
}

void wifiFailsafe() {
  wifiErrorState = true;
  // Quick red error sequence (1.7s max)
  for(int i=0; i<3; i++) {
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 150);
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 2000);
    delay(250);
    strip.fill(0);
    strip.show();
    ledcWrite(SOLENOID_PIN, 0);
    ledcWriteTone(BUZZER_PIN, 0);
    delay(250);
  }
  Serial.println("WiFi error detected - attempting reconnect");
  WiFi.reconnect();
  delay(500);
  wifiErrorState = false;
}

void solenoidPulse(int strength, int duration = 10) {
  if (solenoidDisabled) return;
  ledcWrite(SOLENOID_PIN, strength);
  delay(duration);
  ledcWrite(SOLENOID_PIN, 0);
}

void harmonicChirp(int duration_ms) {
  if (standbyMode || buzzerMuted) return;
  int num_cycles = duration_ms / 8;
  for (int i = 0; i < num_cycles; i++) {
    ledcAttach(BUZZER_PIN, FREQ_HIGH_OVERTONE, LEDC_RESOLUTION);
    ledcWrite(BUZZER_PIN, 128); delay(1);
    ledcAttach(BUZZER_PIN, FREQ_1, LEDC_RESOLUTION);
    ledcWrite(BUZZER_PIN, 128); delay(1);
    ledcAttach(BUZZER_PIN, FREQ_2, LEDC_RESOLUTION);
    ledcWrite(BUZZER_PIN, 128); delay(1);
    ledcAttach(BUZZER_PIN, FREQ_4, LEDC_RESOLUTION);
    ledcWrite(BUZZER_PIN, 128); delay(1);
    ledcAttach(BUZZER_PIN, FREQ_3, LEDC_RESOLUTION);
    ledcWrite(BUZZER_PIN, 128); delay(2); 
    ledcAttach(BUZZER_PIN, FREQ_LOW_FUNDAMENTAL, LEDC_RESOLUTION);
    ledcWrite(BUZZER_PIN, 128); delay(2);
  }
  ledcWrite(BUZZER_PIN, 0);
}

void slideChirp(int startFreq, int endFreq, int duration_ms) {
  if (buzzerMuted) return;
  int steps = 12;
  int stepTime = duration_ms / steps;
  for(int i=0; i<steps; i++) {
    ledcWriteTone(BUZZER_PIN, startFreq + ((endFreq - startFreq) * i / steps));
    delay(stepTime);
  }
  ledcWriteTone(BUZZER_PIN, 0);
}

// NEW: Sci-fi color morphing transition
void sciFiMorph(uint32_t fromColor, uint32_t toColor, int duration_ms, bool silent = false) {
  int steps = duration_ms / 20;
  uint8_t r1 = (fromColor >> 16) & 0xFF;
  uint8_t g1 = (fromColor >> 8) & 0xFF;
  uint8_t b1 = fromColor & 0xFF;
  uint8_t r2 = (toColor >> 16) & 0xFF;
  uint8_t g2 = (toColor >> 8) & 0xFF;
  uint8_t b2 = toColor & 0xFF;
  
  for(int i=0; i<=steps; i++) {
    float progress = (float)i / steps;
    uint8_t r = r1 + (r2 - r1) * progress;
    uint8_t g = g1 + (g2 - g1) * progress;
    uint8_t b = b1 + (b2 - b1) * progress;
    
    // Add shimmer effect
    for(int j=0; j<NUM_LEDS; j++) {
      if(random(100) > 95) {
        strip.setPixelColor(j, 255, 255, 255);
      } else {
        strip.setPixelColor(j, r, g, b);
      }
    }
    strip.show();
    
    if(!silent && i % 10 == 0 && !buzzerMuted) {
      ledcWriteTone(BUZZER_PIN, 4000 + (i * 20));
    }
    delay(20);
  }
  if(!silent) ledcWriteTone(BUZZER_PIN, 0);
  strip.fill(toColor);
  strip.show();
}

void sciFiTransition(uint32_t targetColor) {
  if (standbyMode) standbyMode = false;
  for(int i = 0; i < 35; i++) {
    for(int j = 0; j < 12; j++) {
      int p = random(0, NUM_LEDS);
      strip.setPixelColor(p, targetColor);
      strip.setPixelColor((p + 3) % NUM_LEDS, strip.Color(255, 255, 255));
    }
    strip.show();
    if(i % 5 == 0) solenoidPulse(190, 6);
    delay(12);
  }
  strip.fill(targetColor); strip.show();
}

void odradekUltimateStartup() {
  Serial.println(">>> [CORE] ODRADEK OS ONLINE.");
  delay(500);
  
  for(int i=0; i<10; i++) {
    strip.fill(strip.Color(255, 255, 255)); strip.show();
    slideChirp(9000, 11000, 15);
    strip.fill(0); strip.show();
    delay(50);
  }
  for(int i=0; i<15; i++) {
    solenoidPulse(180, 25);
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 3000 + (i * 300));
    delay(25);
    ledcWriteTone(BUZZER_PIN, 0);
  }
  for(int i=0; i<2; i++) {
    ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 255);
    strip.fill(strip.Color(255, 255, 255));
    strip.show();
    slideChirp(5000, 7000, 60);
    delay(100);
    ledcWrite(SOLENOID_PIN, 0);
    strip.fill(0); strip.show();
    delay(500);
  }
  ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 255);
  strip.fill(strip.Color(255, 255, 255)); strip.show();
  delay(150);
  ledcWrite(SOLENOID_PIN, 0);
  for(int b=255; b>=0; b-=5) {
    strip.fill(strip.Color(b, b, b)); strip.show();
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 1500 + (b * 12));
    delay(15);
  }
  ledcWriteTone(BUZZER_PIN, 0);
}

// ==========================================
// 2. LOCATION SIGNATURES
// ==========================================

// FIXED: Add isStorm parameter
void performDeepBreaths(bool isStorm = false) {
  for(int k=0; k<4; k++) {
    strip.fill(strip.Color(0, 5, 80));
    strip.show();
    solenoidPulse(255, 50);
    if (!buzzerMuted) {
      ledcWriteTone(BUZZER_PIN, FREQ_1); delay(15);
      ledcWriteTone(BUZZER_PIN, FREQ_LOW_FUNDAMENTAL); delay(35);
      ledcWriteTone(BUZZER_PIN, 0);
    }
    strip.fill(0); strip.show();
    delay(1700); 
  }
}

void pulseLOCATION_1(bool isStorm = false) {
  if(emergencyStop) { emergencyStop = false; return; }
  for(int i=0; i<25; i++) {
    if(emergencyStop) { emergencyStop = false; strip.fill(0); strip.show(); return; }
    strip.clear();
    for(int j=0; j<5; j++) {
      strip.setPixelColor(random(NUM_LEDS), 0, 0, 255);
    }
    strip.show();
    if (!buzzerMuted && i % 8 == 0) {
      ledcWriteTone(BUZZER_PIN, 8000 - (i * 200));
      delay(30);
      ledcWriteTone(BUZZER_PIN, 0);
      delay(30);
    } else {
      delay(60);
    }
  }
  strip.clear(); strip.show();
  performDeepBreaths(isStorm);
}

void pulseLOCATION_3(bool isStorm = false) {
  if(emergencyStop) { emergencyStop = false; return; }
  for(int i=0; i<6; i++) {
    if(emergencyStop) { emergencyStop = false; strip.fill(0); strip.show(); return; }
    strip.fill(strip.Color(0, 0, 200)); strip.show();
    if (!buzzerMuted) ledcWriteTone(BUZZER_PIN, 7000);
    delay(80);
    strip.fill(0); strip.show();
    ledcWriteTone(BUZZER_PIN, 0);
    delay(80);
  }
  performDeepBreaths(isStorm);
}

void pulseLOCATION_2(bool isStorm = false) {
  if(emergencyStop) { emergencyStop = false; return; }
  int center = NUM_LEDS / 2;
  for(int i=0; i<center; i+=2) {
    if(emergencyStop) { emergencyStop = false; strip.fill(0); strip.show(); return; }
    strip.setPixelColor(center + i, 0, 5, 100);
    strip.setPixelColor(center - i, 0, 5, 100);
    strip.show();
    if (!buzzerMuted && i % 5 == 0) {
      ledcWriteTone(BUZZER_PIN, 4000 + (i * 50));
    }
    delay(15);
  }
  ledcWriteTone(BUZZER_PIN, 0);
  strip.clear(); strip.show();
  performDeepBreaths(isStorm);
}

void warningFlap() {
  for(int i=0; i<60; i++) {
    strip.fill(strip.Color(random(80, 255), 40, 0)); strip.show();
    if(i % 4 == 0) solenoidPulse(random(160, 210), 30);
    delay(30);
  }
}

// NEW: ULTIMATE Alive Check (3.7 seconds max - THE SHOW)
void ultimateAliveCheck() {
  Serial.println(">>> EXECUTING ULTIMATE ALIVE CHECK <<<");
  
  // Phase 1: Rapid spiral (1 second)
  for(int i=0; i<NUM_LEDS; i+=3) {
    strip.setPixelColor(i, 0, 255, 255);
    strip.show();
    if(i % 12 == 0 && !buzzerMuted) ledcWriteTone(BUZZER_PIN, 5000 + (i * 50));
    delay(8);
  }
  ledcWriteTone(BUZZER_PIN, 0);
  
  // Phase 2: Full white flash with MAX solenoid hit (0.5 seconds)
  strip.fill(strip.Color(255, 255, 255));
  strip.show();
  ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 255);
  if(!buzzerMuted) {
    ledcWriteTone(BUZZER_PIN, 8000);
    delay(250);
    ledcWriteTone(BUZZER_PIN, 10000);
    delay(250);
  } else {
    delay(500);
  }
  ledcWrite(SOLENOID_PIN, 0);
  ledcWriteTone(BUZZER_PIN, 0);
  
  // Phase 3: Rainbow wave (1.2 seconds)
  for(int cycle=0; cycle<2; cycle++) {
    for(int hue=0; hue<256; hue+=16) {
      for(int i=0; i<NUM_LEDS; i++) {
        int pixelHue = (hue + (i * 256 / NUM_LEDS)) % 256;
        uint32_t color = strip.gamma32(strip.ColorHSV(pixelHue * 256));
        strip.setPixelColor(i, color);
      }
      strip.show();
      if(hue % 64 == 0) solenoidPulse(200, 15);
      delay(20);
    }
  }
  
  // Phase 4: Harmonic finale (1 second)
  strip.fill(strip.Color(0, 255, 0));
  strip.show();
  if(!buzzerMuted) {
    int notes[] = {3000, 4000, 5000, 6000, 8000};
    for(int i=0; i<5; i++) {
      ledcWriteTone(BUZZER_PIN, notes[i]);
      solenoidPulse(150 + (i * 20), 10);
      delay(180);
    }
    ledcWriteTone(BUZZER_PIN, 0);
  }
  
  delay(200);
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
  Serial.println(">>> ALIVE CHECK COMPLETE <<<");
}

// NEW: Phantom Detection (new function #4)
void phantomDetection() {
  // Scanning for invisible entities
  for(int scan=0; scan<4; scan++) {
    for(int i=0; i<NUM_LEDS; i+=2) {
      strip.clear();
      strip.setPixelColor(i, 128, 0, 128); // Purple
      strip.setPixelColor((i + NUM_LEDS/2) % NUM_LEDS, 128, 0, 128);
      strip.show();
      
      if(i % 20 == 0) {
        solenoidPulse(random(120, 180), 25);
        if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, random(2500, 3500));
        delay(50);
        ledcWriteTone(BUZZER_PIN, 0);
      }
      delay(15);
    }
  }
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
}

// NEW: Chiral Density Test (new function #5)
void chiralDensityTest() {
  // Measuring chiral particle density
  for(int level=0; level<=255; level+=5) {
    int numLeds = map(level, 0, 255, 0, NUM_LEDS);
    strip.clear();
    for(int i=0; i<numLeds; i++) {
      strip.setPixelColor(i, 0, level, 255 - level);
    }
    strip.show();
    
    if(level % 25 == 0) {
      solenoidPulse(100 + level/3, 20);
      if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 2000 + level * 15);
    }
    delay(20);
  }
  ledcWriteTone(BUZZER_PIN, 0);
  
  // Result flash
  strip.fill(strip.Color(0, 255, 255));
  strip.show();
  if(!buzzerMuted) {
    ledcWriteTone(BUZZER_PIN, 7000);
    delay(300);
    ledcWriteTone(BUZZER_PIN, 0);
  }
  delay(500);
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
}

// NEW: Void-out Simulation (new function #6 - dramatic!)
void voidoutSimulation() {
  // WARNING: Intense sequence
  Serial.println("!!! VOID-OUT SIMULATION - STAND CLEAR !!!");
  
  // Build up tension
  for(int i=0; i<50; i++) {
    strip.fill(strip.Color(random(200, 255), 0, 0));
    strip.show();
    if(i % 5 == 0) {
      solenoidPulse(100 + i * 3, 30);
      if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 1000 + i * 80);
    }
    delay(40);
  }
  
  // EXPLOSION
  strip.fill(strip.Color(255, 255, 255));
  strip.show();
  ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : 255);
  if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 12000);
  delay(500);
  
  // Aftermath
  for(int i=255; i>=0; i-=5) {
    strip.fill(strip.Color(i, i/2, 0));
    strip.show();
    if(i % 30 == 0) ledcWrite(SOLENOID_PIN, solenoidDisabled ? 0 : random(100, 200));
    delay(20);
  }
  
  ledcWrite(SOLENOID_PIN, 0);
  ledcWriteTone(BUZZER_PIN, 0);
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
}
void territoryScan() {
  standbyMode = false;
  
  // Slow rotating scan pattern
  for(int scan=0; scan<3; scan++) {
    for(int i=0; i<NUM_LEDS; i++) {
      strip.clear();
      // Create scanning beam
      for(int j=-3; j<=3; j++) {
        int pos = (i + j + NUM_LEDS) % NUM_LEDS;
        int brightness = 200 - abs(j) * 50;
        strip.setPixelColor(pos, 255, brightness/2, 0);
      }
      strip.show();
      
      // Random detection clicks
      if(i % 15 == 0) {
        solenoidPulse(random(150, 200), 20);
        if(!buzzerMuted) {
          ledcWriteTone(BUZZER_PIN, random(3500, 4500));
          delay(40);
          ledcWriteTone(BUZZER_PIN, 0);
        }
      }
      delay(12);
    }
  }
  
  // Scan complete
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
  if(!buzzerMuted) harmonicChirp(80);
}

// NEW: Strand Calibration (replaces cargo scan - calibrates connection to Beach)
void strandCalibration() {
  standbyMode = false;
  
  // Pulse from edges to center
  for(int cycle=0; cycle<5; cycle++) {
    for(int i=0; i<NUM_LEDS/2; i+=2) {
      strip.clear();
      // Both ends moving to center
      strip.setPixelColor(i, 0, 255, 255);
      strip.setPixelColor(NUM_LEDS - i - 1, 0, 255, 255);
      strip.show();
      
      if(i % 8 == 0 && !buzzerMuted) {
        ledcWriteTone(BUZZER_PIN, 2000 + (i * 30));
      }
      delay(20);
    }
    ledcWriteTone(BUZZER_PIN, 0);
    
    // Center flash
    for(int j=0; j<3; j++) {
      strip.fill(strip.Color(0, 255, 255));
      strip.show();
      solenoidPulse(180, 15);
      delay(80);
      strip.fill(0);
      strip.show();
      delay(80);
    }
  }
  
  // Calibration complete
  strip.fill(strip.Color(0, 255, 0));
  strip.show();
  if(!buzzerMuted) {
    ledcWriteTone(BUZZER_PIN, 6000); delay(100);
    ledcWriteTone(BUZZER_PIN, 7000); delay(100);
    ledcWriteTone(BUZZER_PIN, 0);
  }
  delay(500);
  
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
}

// NEW: DOOMS Resonance Check (replaces handshake - tests connection strength)
void doomsResonance() {
  standbyMode = false;
  
  // Build up resonance
  for(int intensity=10; intensity<=255; intensity+=15) {
    strip.fill(strip.Color(0, 0, intensity));
    strip.show();
    
    if(!buzzerMuted) {
      ledcWriteTone(BUZZER_PIN, 1000 + (intensity * 15));
    }
    
    if(intensity % 45 == 0) {
      solenoidPulse(intensity * 0.8, 25);
    }
    
    delay(30);
  }
  
  // Peak resonance flash
  for(int flash=0; flash<5; flash++) {
    strip.fill(strip.Color(255, 255, 255));
    strip.show();
    solenoidPulse(255, 10);
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 8000);
    delay(60);
    
    strip.fill(strip.Color(0, 0, 255));
    strip.show();
    ledcWriteTone(BUZZER_PIN, 0);
    delay(60);
  }
  
  // Fade down
  for(int intensity=255; intensity>=0; intensity-=10) {
    strip.fill(strip.Color(0, 0, intensity));
    strip.show();
    if(!buzzerMuted && intensity % 30 == 0) {
      ledcWriteTone(BUZZER_PIN, 4000 - (intensity * 10));
    }
    delay(15);
  }
  
  ledcWriteTone(BUZZER_PIN, 0);
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
}

// NEW: IMPROVED Gentle morning alarm (SOFT & CALMING)
void gentleMorningWake() {
  standbyMode = false;
  
  // Phase 1: Very slow warm fade (10 seconds) - like sunrise
  for(int b=0; b<=120; b+=1) {
    strip.fill(strip.Color(b, b*0.7, b*0.2));
    strip.show();
    delay(83); // 10 seconds total
  }
  
  // Phase 2: Soft melodic tones (8 seconds) - calming
  if(!buzzerMuted) {
    int melody[] = {1760, 1976, 2217, 2349}; // A6, B6, C#7, D7 - peaceful ascending
    for(int i=0; i<4; i++) {
      ledcWriteTone(BUZZER_PIN, melody[i]);
      delay(800);
      ledcWriteTone(BUZZER_PIN, 0);
      delay(1200);
    }
  } else {
    delay(8000);
  }
  
  // Phase 3: Gentle breathing LEDs (no solenoid) (7 seconds)
  for(int breath=0; breath<3; breath++) {
    for(int b=120; b<=180; b+=3) {
      strip.fill(strip.Color(b, b*0.75, b*0.25));
      strip.show();
      delay(35);
    }
    for(int b=180; b>=120; b-=3) {
      strip.fill(strip.Color(b, b*0.75, b*0.25));
      strip.show();
      delay(35);
    }
  }
  
  // Phase 4: Final gentle brightness (5 seconds)
  for(int b=180; b<=255; b+=3) {
    strip.fill(strip.Color(b, b*0.8, b*0.3));
    strip.show();
    delay(20);
  }
  
  // One final soft confirmation tone
  if(!buzzerMuted) {
    ledcWriteTone(BUZZER_PIN, 2349); // Soft D7
    delay(500);
    ledcWriteTone(BUZZER_PIN, 0);
  }
  
  lampMode = true;
  Serial.println("Gentle morning wake completed - 30 seconds total");
}

// NEW: Reminder alarm
void reminderAlert() {
  standbyMode = false;
  
  for(int i=0; i<8; i++) {
    strip.fill(strip.Color(255, 100, 255));
    strip.show();
    solenoidPulse(200, 20);
    if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 5000);
    delay(100);
    strip.fill(0);
    strip.show();
    ledcWriteTone(BUZZER_PIN, 0);
    delay(100);
  }
  
  strip.fill(strip.Color(0, 5, 50));
  strip.show();
}

// ==========================================
// 3. WEATHER DATA FETCHING
// ==========================================

void fetchWeatherData() {
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping weather fetch");
    return;
  }
  
  if(ESP.getFreeHeap() < 30000) {
    Serial.println("Low memory, skipping weather fetch");
    return;
  }
  
  esp_task_wdt_reset(); // Feed watchdog
  
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)) {
    char buffer[6];
    strftime(buffer, 6, "%H:%M", &timeinfo);
    lastWeatherUpdate = String(buffer);
  }
  
  HTTPClient http;
  JsonDocument doc;
  
  // LOCATION_1 COMPLICATED SHIT UGHH
  Serial.println("Fetching [LOCATION_1_NAME] weather...");
  http.begin(weatherLOCATION_1);
  http.setTimeout(5000);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("LOCATION_1 response: " + payload.substring(0, 200));
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      humidityLOCATION_1 = doc["current"]["relative_humidity_2m"].as<float>();
      rainIntensityLOCATION_1 = doc["current"]["rain"].as<float>();
      tempLOCATION_1 = doc["current"]["temperature_2m"].as<float>();
      int weatherCode = doc["current"]["weather_code"].as<int>();
      skyConditionLOCATION_1 = weatherCodeToCondition(weatherCode);
      rainChanceLOCATION_1 = doc["hourly"]["precipitation_probability"][0].as<int>();
      isRainingLOCATION_1 = rainIntensityLOCATION_1 > 0;
      Serial.printf("[LOCATION_1_NAME]: Temp=%.1fC, Humidity=%.1f%%, Rain=%.2fmm, Chance=%d%%, Sky=%s\n", 
                    tempLOCATION_1, humidityLOCATION_1, rainIntensityLOCATION_1, rainChanceLOCATION_1, skyConditionLOCATION_1.c_str());
      if (isRainingLOCATION_1) animationTrigger = 1;
      else if (doc["hourly"]["rain"][0].as<float>() > 0) warningFlap();
    } else {
      Serial.println("LOCATION_1 JSON parse error");
    }
  } else {
    Serial.printf("LOCATION_1_NAME HTTP error: %d\n", httpCode);
  }
  http.end();
  delay(100);
  esp_task_wdt_reset();
  
  // Den Haag
  Serial.println("Fetching Den Haag weather...");
  http.begin(weatherLOCATION_3);
  http.setTimeout(5000);
  httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      humidityLOCATION_3 = doc["current"]["relative_humidity_2m"].as<float>();
      rainIntensityLOCATION_3 = doc["current"]["rain"].as<float>();
      tempLOCATION_3 = doc["current"]["temperature_2m"].as<float>();
      int weatherCode = doc["current"]["weather_code"].as<int>();
      skyConditionLOCATION_3 = weatherCodeToCondition(weatherCode);
      rainChanceLOCATION_3 = doc["hourly"]["precipitation_probability"][0].as<int>();
      isRainingLOCATION_3 = rainIntensityLOCATION_3 > 0;
      Serial.printf("LOCATION_3_NAME: Temp=%.1fC, Humidity=%.1f%%, Rain=%.2fmm, Chance=%d%%, Sky=%s\n",
                    tempLOCATION_3, humidityLOCATION_3, rainIntensityLOCATION_3, rainChanceLOCATION_3, skyConditionLOCATION_3.c_str());
      if (isRainingLOCATION_3) animationTrigger = 2;
      else if (doc["hourly"]["rain"][0].as<float>() > 0) warningFlap();
    }
  }
  http.end();
  delay(100);
  esp_task_wdt_reset();
  
  // LOCATION_2 thingamabober
  Serial.println("Fetching LOCATION_2_NAME weather...");
  http.begin(weatherLOCATION_2);
  http.setTimeout(5000);
  httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      humidityLOCATION_2 = doc["current"]["relative_humidity_2m"].as<float>();
      rainIntensityLOCATION_2 = doc["current"]["rain"].as<float>();
      tempLOCATION_2 = doc["current"]["temperature_2m"].as<float>();
      int weatherCode = doc["current"]["weather_code"].as<int>();
      skyConditionLOCATION_2 = weatherCodeToCondition(weatherCode);
      rainChanceLOCATION_2 = doc["hourly"]["precipitation_probability"][0].as<int>();
      isRainingLOCATION_2 = rainIntensityLOCATION_2 > 0;
      Serial.printf("LOCATION_2_NAME: Temp=%.1fC, Humidity=%.1f%%, Rain=%.2fmm, Chance=%d%%, Sky=%s\n",
                    tempLOCATION_2, humidityLOCATION_2, rainIntensityLOCATION_2, rainChanceLOCATION_2, skyConditionLOCATION_2.c_str());
      if (isRainingLOCATION_2) animationTrigger = 3;
      else if (doc["hourly"]["rain"][0].as<float>() > 0) warningFlap();
    }
  }
  http.end();
  
  Serial.println("=== Weather update complete ===");
}

// ==========================================
// 4. MASTER WEB INTERFACE
// ==========================================

String getHTML() {
  String status = (WiFi.status() == WL_CONNECTED) ? "STABLE" : "DISRUPTED";
  String statusColor = (WiFi.status() == WL_CONNECTED) ? "#00ffcc" : "#ff3300";
  struct tm timeinfo;
  char timeStr[12] = "00:00:00";
  if(getLocalTime(&timeinfo)) strftime(timeStr, 12, "%H:%M:%S", &timeinfo);
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
  html += "<style>";
  html += "body { background-color: #050505; color: #00d4ff; font-family: 'Courier New', monospace; margin: 0; overflow-x: hidden; }";
  html += "body::before { content: ' '; display: block; position: fixed; top: 0; left: 0; bottom: 0; right: 0; background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%), linear-gradient(90deg, rgba(255, 0, 0, 0.06), rgba(0, 255, 0, 0.02), rgba(0, 0, 255, 0.06)); z-index: 10; background-size: 100% 2px, 3px 100%; pointer-events: none; }";
  html += ".header { border-bottom: 2px solid #00d4ff; padding: 20px; text-align: left; letter-spacing: 4px; position: relative; }";
  html += ".header h1 { margin: 0; font-size: 1.2em; text-transform: uppercase; }";
  html += ".header .id { font-size: 0.7em; color: rgba(0, 212, 255, 0.6); }";
  html += ".stats { padding: 10px 20px; background: rgba(0, 212, 255, 0.05); font-size: 0.8em; border-bottom: 1px solid rgba(0, 212, 255, 0.3); display: flex; justify-content: space-between; }";
  html += ".container { padding: 20px; }";
  html += ".btn { display: block; position: relative; width: 100%; padding: 15px; margin-bottom: 15px; background: transparent; border: 1px solid #00d4ff; color: #00d4ff; text-transform: uppercase; font-weight: bold; text-decoration: none; letter-spacing: 2px; box-sizing: border-box; transition: 0.2s; cursor: pointer; text-align: left;}";
  html += ".btn:active { background: #00d4ff; color: #000; }";
  html += ".btn::after { content: ' >'; position: absolute; right: 15px; }";
  html += ".btn.clear { border-color: #ff3300; color: #ff3300; }";
  html += ".btn.clear:active { background: #ff3300; color: #000; }";
  html += ".btn.diag { border-color: #ffffff; color: #ffffff; opacity: 0.7; font-size: 0.8em; }";
  html += ".btn.muted { border-color: #ffaa00; color: #ffaa00; }";
  html += ".btn.muted:active { background: #ffaa00; color: #000; }";
  html += ".btn.disabled { border-color: #ff6600; color: #ff6600; }";
  html += ".btn.disabled:active { background: #ff6600; color: #000; }";
  html += ".btn.scan { border-color: #00ff88; color: #00ff88; }";
  html += ".btn.scan:active { background: #00ff88; color: #000; }";
  html += ".system-bar { height: 6px; background: rgba(0, 212, 255, 0.1); margin: 5px 0; position: relative; border: 1px solid rgba(0, 212, 255, 0.2); }";
  html += ".system-fill { height: 100%; background: #00d4ff; transition: width 0.3s; }";
  html += ".system-stats { border: 1px solid rgba(0, 212, 255, 0.3); padding: 15px; margin-bottom: 20px; }";
  html += ".stat-row { display: flex; justify-content: space-between; margin-bottom: 10px; font-size: 0.85em; }";
  html += ".stat-label { opacity: 0.7; }";
  html += ".stat-value { font-weight: bold; font-family: monospace; }";
  html += ".wifi-visual { display: flex; align-items: flex-end; gap: 3px; height: 20px; margin-left: 10px; }";
  html += ".wifi-bar { width: 4px; background: #00d4ff; transition: height 0.3s; }";
  html += ".brightness-control { margin: 15px 0; }";
  html += ".brightness-control input[type='range'] { width: 70%; background: rgba(0, 212, 255, 0.2); }";
  html += ".brightness-control button { padding: 8px 15px; background: #00d4ff; color: #000; border: none; cursor: pointer; font-weight: bold; margin-left: 10px; }";
  html += "input, select { background: #000; border: 1px solid #00d4ff; color: #00d4ff; padding: 10px; margin: 5px; outline: none; font-family: 'Courier New', monospace; }";
  html += "input[type='time'] { width: auto; }";
  html += ".alarm-form { border: 1px solid rgba(0, 212, 255, 0.3); padding: 20px; margin-bottom: 20px; background: rgba(0, 212, 255, 0.02); }";
  html += ".alarm-input-row { display: flex; gap: 10px; align-items: center; margin-bottom: 15px; flex-wrap: wrap; }";
  html += ".alarm-input-row input, .alarm-input-row select { flex: 1; min-width: 100px; }";
  html += ".alarm-submit { width: 100%; background: #00d4ff; color: #000; border: none; padding: 15px; cursor: pointer; text-transform: uppercase; font-weight: bold; letter-spacing: 2px; }";
  html += ".alarm-submit:active { background: #00ffff; }";
  html += ".alarm-list { border: 1px solid rgba(0, 212, 255, 0.2); padding: 15px; margin-top: 15px; max-height: 200px; overflow-y: auto; }";
  html += ".alarm-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; margin-bottom: 8px; background: rgba(0, 212, 255, 0.05); border-left: 3px solid #00d4ff; }";
  html += ".alarm-item .time { font-size: 1.1em; font-weight: bold; }";
  html += ".alarm-item .type { font-size: 0.8em; opacity: 0.7; }";
  html += ".alarm-item button { background: #ff3300; border: none; color: #fff; padding: 5px 15px; cursor: pointer; text-transform: uppercase; font-size: 0.7em; }";
  html += ".alarm-item button:active { background: #ff6600; }";
  html += ".weather { border: 1px solid rgba(0, 212, 255, 0.3); padding: 15px; margin-bottom: 20px; font-size: 0.85em; }";
  html += ".weather-item { margin-bottom: 15px; padding-bottom: 15px; border-bottom: 1px solid rgba(0, 212, 255, 0.1); }";
  html += ".weather-item:last-child { border-bottom: none; }";
  html += ".weather-header { display: flex; justify-content: space-between; margin-bottom: 8px; font-weight: bold; }";
  html += ".rain-status { font-size: 0.75em; padding: 2px 8px; border-radius: 3px; background: #003300; color: #00ff00; }";
  html += ".rain-status.active { background: #330000; color: #ff3300; animation: blink 1s infinite; }";
  html += "@keyframes blink { 50% { opacity: 0.5; } }";
  html += ".weather-data { display: grid; grid-template-columns: 1fr 1fr; gap: 5px; font-size: 0.8em; margin-top: 8px; }";
  html += ".weather-data span { padding: 3px 0; }";
  html += ".intensity-bar { height: 4px; background: rgba(0, 212, 255, 0.2); margin-top: 5px; position: relative; }";
  html += ".intensity-fill { height: 100%; background: #00d4ff; transition: width 0.3s; }";
  html += "</style>";

  html += "<script>";
  html += "function sendCmd(p) { fetch(p); return false; }";
  html += "function setBrightness() { let val = document.getElementById('brightness').value; document.getElementById('brightVal').innerText = val; fetch('/setBrightness?val=' + val); }";
  html += "function resetBrightness() { document.getElementById('brightness').value = 150; document.getElementById('brightVal').innerText = '150'; fetch('/setBrightness?val=150'); }";
  html += "function setAlarm(e) { e.preventDefault(); let time = document.getElementById('alarmTime').value; let type = document.getElementById('alarmType').value; fetch('/setAlarm?time=' + time + '&type=' + type).then(() => { document.getElementById('alarmTime').value = ''; updateAlarms(); }); return false; }";
  html += "function cancelAlarm(idx) { fetch('/cancelAlarm?idx=' + idx).then(() => updateAlarms()); }";
  html += "function updateAlarms() { fetch('/alarms').then(r => r.json()).then(data => { let html = ''; if(data.count == 0) { html = '<div style=\"opacity:0.5; text-align:center; padding:20px;\">No active alarms</div>'; } else { data.alarms.forEach((a, i) => { html += '<div class=\"alarm-item\"><div><div class=\"time\">' + a.time + '</div><div class=\"type\">' + a.typeName + '</div></div><button onclick=\"cancelAlarm(' + i + ')\">CANCEL</button></div>'; }); } document.getElementById('alarmList').innerHTML = html; }); }";
  html += "function updateWifiVisual(rssi) { let bars = document.querySelectorAll('.wifi-bar'); let strength = Math.max(0, Math.min(4, Math.floor((rssi + 90) / 15))); bars.forEach((bar, i) => { bar.style.height = i < strength ? ((i + 1) * 5) + 'px' : '3px'; bar.style.opacity = i < strength ? '1' : '0.3'; }); }";
  
  // UPDATED AJAX CALL - Now updates time with seconds and all core data
  html += "setInterval(() => { fetch('/data').then(r => r.json()).then(d => { ";
  html += "document.getElementById('hL').innerText = d.hL; ";
  html += "document.getElementById('hD').innerText = d.hD; ";
  html += "document.getElementById('hLd').innerText = d.hLd; ";
  html += "document.getElementById('tL').innerText = d.tL; ";
  html += "document.getElementById('tD').innerText = d.tD; ";
  html += "document.getElementById('tLd').innerText = d.tLd; ";
  html += "document.getElementById('rL').innerText = d.rL; ";
  html += "document.getElementById('rD').innerText = d.rD; ";
  html += "document.getElementById('rLd').innerText = d.rLd; ";
  html += "document.getElementById('rcL').innerText = d.rcL; ";
  html += "document.getElementById('rcD').innerText = d.rcD; ";
  html += "document.getElementById('rcLd').innerText = d.rcLd; ";
  html += "document.getElementById('scL').innerText = d.scL; ";
  html += "document.getElementById('scD').innerText = d.scD; ";
  html += "document.getElementById('scLd').innerText = d.scLd; ";
  html += "document.getElementById('bL').style.width = Math.min(d.rL*20, 100) + '%'; ";
  html += "document.getElementById('bD').style.width = Math.min(d.rD*20, 100) + '%'; ";
  html += "document.getElementById('bLd').style.width = Math.min(d.rLd*20, 100) + '%'; ";
  html += "document.getElementById('sL').className = 'rain-status' + (d.rL > 0 ? ' active' : ''); ";
  html += "document.getElementById('sD').className = 'rain-status' + (d.rD > 0 ? ' active' : ''); ";
  html += "document.getElementById('sLd').className = 'rain-status' + (d.rLd > 0 ? ' active' : ''); ";
  html += "document.getElementById('sL').innerText = d.rL > 0 ? 'TIMEFALL' : 'CLEAR'; ";
  html += "document.getElementById('sD').innerText = d.rD > 0 ? 'TIMEFALL' : 'CLEAR'; ";
  html += "document.getElementById('sLd').innerText = d.rLd > 0 ? 'TIMEFALL' : 'CLEAR'; ";
  html += "document.getElementById('upd').innerText = d.upd; ";
  html += "document.getElementById('rssiVal').innerText = d.rssi + ' dBm'; ";
  html += "updateWifiVisual(parseInt(d.rssi)); ";
  html += "document.getElementById('heap').innerText = d.heap; ";
  html += "document.getElementById('heapBar').style.width = d.heapPct + '%'; ";
  html += "document.getElementById('cpu').innerText = d.cpu + '%'; ";
  html += "document.getElementById('cpuBar').style.width = d.cpu + '%'; ";
  html += "document.getElementById('core0').innerText = d.core0 + '%'; ";
  html += "document.getElementById('core0Bar').style.width = d.core0 + '%'; ";
  html += "document.getElementById('core1').innerText = d.core1 + '%'; ";
  html += "document.getElementById('core1Bar').style.width = d.core1 + '%'; ";
  html += "document.getElementById('currentTime').innerText = 'TIME: ' + d.time; ";
  html += "document.getElementById('muteBtn').className = 'btn ' + (d.muted ? 'muted' : ''); ";
  html += "document.getElementById('muteBtn').innerText = 'Audio: ' + (d.muted ? 'MUTED' : 'ACTIVE'); ";
  html += "document.getElementById('solBtn').className = 'btn ' + (d.solDisabled ? 'disabled' : ''); ";
  html += "document.getElementById('solBtn').innerText = 'Solenoid: ' + (d.solDisabled ? 'DISABLED' : 'ACTIVE'); ";
  html += "}); }, 1000);";
  
  html += "window.onload = () => { updateAlarms(); };";
  html += "</script>";

  html += "</head><body>";
  html += "<div class='header'><h1>Bridges Master Core</h1><div class='id'>ODRADEK // UNIT_ID: MADIS-08</div></div>";
  html += "<div class='stats'><span>NETWORK: <b style='color:" + statusColor + "'>" + status + "</b></span><span id='currentTime'>TIME: " + String(timeStr) + "</span></div>";
  
  html += "<div class='container'>";
  
  // UPDATED ESP32 System Stats with proper Core displays
  html += "<div class='system-stats'>";
  html += "<div style='font-size:0.9em; margin-bottom:12px; font-weight:bold; letter-spacing:2px;'>ODRADEK CORE TELEMETRY</div>";
  
  // WiFi with visual bars
  html += "<div class='stat-row'><span class='stat-label'>WiFi Signal:</span><span style='display:flex; align-items:center;'><span class='stat-value' id='rssiVal'>" + String(wifiRSSI) + " dBm</span>";
  html += "<div class='wifi-visual'>";
  for(int i=0; i<4; i++) {
    int height = (i + 1) * 5;
    html += "<div class='wifi-bar' style='height:" + String(height) + "px'></div>";
  }
  html += "</div></span></div>";
  
  // Memory
  html += "<div class='stat-row'><span class='stat-label'>Free Memory:</span><span class='stat-value' id='heap'>" + String(freeHeap/1024) + " KB</span></div>";
  uint32_t heapPct = (totalHeap > 0) ? (freeHeap * 100) / totalHeap : 0;
  html += "<div class='system-bar'><div id='heapBar' class='system-fill' style='width:" + String(heapPct) + "%'></div></div>";
  
  // Overall CPU Load
  html += "<div class='stat-row'><span class='stat-label'>Processor Load:</span><span class='stat-value' id='cpu'>" + String(cpuLoad, 1) + "%</span></div>";
  html += "<div class='system-bar'><div id='cpuBar' class='system-fill' style='width:" + String(cpuLoad) + "%'></div></div>";
  
  // Core 0 with bar
  html += "<div class='stat-row'><span class='stat-label'>Core 0:</span><span class='stat-value' id='core0'>" + String(core0Load, 1) + "%</span></div>";
  html += "<div class='system-bar'><div id='core0Bar' class='system-fill' style='width:" + String(core0Load) + "%'></div></div>";
  
  // Core 1 with bar
  html += "<div class='stat-row'><span class='stat-label'>Core 1:</span><span class='stat-value' id='core1'>" + String(core1Load, 1) + "%</span></div>";
  html += "<div class='system-bar'><div id='core1Bar' class='system-fill' style='width:" + String(core1Load) + "%'></div></div>";
  
  html += "</div>";
  
  // Brightness Control
  html += "<div style='border: 1px solid rgba(0, 212, 255, 0.3); padding: 15px; margin-bottom: 20px;'>";
  html += "<div style='font-size:0.9em; margin-bottom:12px; font-weight:bold; letter-spacing:2px;'>LED BRIGHTNESS</div>";
  html += "<div class='brightness-control'>";
  html += "<input type='range' id='brightness' min='10' max='255' value='" + String(ledBrightness) + "' oninput='setBrightness()' style='width:calc(100% - 100px);'>";
  html += "<button onclick='resetBrightness()'>RESET</button>";
  html += "</div>";
  html += "<div style='font-size:0.7em; opacity:0.6; margin-top:5px;'>Current: <span id='brightVal'>" + String(ledBrightness) + "</span></div>";
  html += "</div>";
  
  // Weather Display
  html += "<div class='weather'>";
  html += "<div style='display:flex; justify-content:space-between; margin-bottom:15px;'>";
  html += "<span style='font-size:0.7em; opacity:0.6;'>TIMEFALL CONDITIONS:</span>";
  html += "<span style='font-size:0.7em; opacity:0.6;'>LAST: <span id='upd'>" + lastWeatherUpdate + "</span></span>";
  html += "</div>";
  
  // LOCATION_1
  html += "<div class='weather-item'>";
  html += "<div class='weather-header'><span>[LOCATION_1_NAME]</span><span id='sL' class='rain-status" + String(isRainingLOCATION_1 ? " active" : "") + "'>" + String(isRainingLOCATION_1 ? "TIMEFALL" : "CLEAR") + "</span></div>";
  html += "<div class='weather-data'>";
  html += "<span>Temp: <span id='tL'>" + String(tempLOCATION_1, 1) + "</span>&deg;C</span>";
  html += "<span>Humidity: <span id='hL'>" + String(humidityLOCATION_1, 0) + "</span>%</span>";
  html += "<span>Intensity: <span id='rL'>" + String(rainIntensityLOCATION_1, 2) + "</span>mm</span>";
  html += "<span>Rain Chance: <span id='rcL'>" + String(rainChanceLOCATION_1) + "</span>%</span>";
  html += "<span>Sky: <span id='scL'>" + skyConditionLOCATION_1 + "</span></span>";
  html += "</div>";
  html += "<div class='intensity-bar'><div id='bL' class='intensity-fill' style='width:" + String(min(rainIntensityLOCATION_1 * 20, 100.0f)) + "%'></div></div>";
  html += "<button onclick=\"return sendCmd('/pingLOCATION_1');\" style='margin-top:8px; padding:5px 10px; width:100%; background:transparent; border:1px solid rgba(0,212,255,0.3); color:#00d4ff; font-size:0.7em; cursor:pointer;'>PING LOCATION</button>";
  html += "</div>";
  
  // LOCATION_3
  html += "<div class='weather-item'>";
  html += "<div class='weather-header'><span>LOCATION_3_NAME</span><span id='sD' class='rain-status" + String(isRainingLOCATION_3 ? " active" : "") + "'>" + String(isRainingLOCATION_3 ? "TIMEFALL" : "CLEAR") + "</span></div>";
  html += "<div class='weather-data'>";
  html += "<span>Temp: <span id='tD'>" + String(tempLOCATION_3, 1) + "</span>&deg;C</span>";
  html += "<span>Humidity: <span id='hD'>" + String(humidityLOCATION_3, 0) + "</span>%</span>";
  html += "<span>Intensity: <span id='rD'>" + String(rainIntensityLOCATION_3, 2) + "</span>mm</span>";
  html += "<span>Rain Chance: <span id='rcD'>" + String(rainChanceLOCATION_3) + "</span>%</span>";
  html += "<span>Sky: <span id='scD'>" + skyConditionLOCATION_3 + "</span></span>";
  html += "</div>";
  html += "<div class='intensity-bar'><div id='bD' class='intensity-fill' style='width:" + String(min(rainIntensityLOCATION_3 * 20, 100.0f)) + "%'></div></div>";
  html += "<button onclick=\"return sendCmd('/pingLOCATION_3');\" style='margin-top:8px; padding:5px 10px; width:100%; background:transparent; border:1px solid rgba(0,212,255,0.3); color:#00d4ff; font-size:0.7em; cursor:pointer;'>PING LOCATION</button>";
  html += "</div>";
  
  // LOCATION_2
  html += "<div class='weather-item'>";
  html += "<div class='weather-header'><span>LOCATION_2_NAME</span><span id='sLd' class='rain-status" + String(isRainingLOCATION_2 ? " active" : "") + "'>" + String(isRainingLOCATION_2 ? "TIMEFALL" : "CLEAR") + "</span></div>";
  html += "<div class='weather-data'>";
  html += "<span>Temp: <span id='tLd'>" + String(tempLOCATION_2, 1) + "</span>&deg;C</span>";
  html += "<span>Humidity: <span id='hLd'>" + String(humidityLOCATION_2, 0) + "</span>%</span>";
  html += "<span>Intensity: <span id='rLd'>" + String(rainIntensityLOCATION_2, 2) + "</span>mm</span>";
  html += "<span>Rain Chance: <span id='rcLd'>" + String(rainChanceLOCATION_2) + "</span>%</span>";
  html += "<span>Sky: <span id='scLd'>" + skyConditionLOCATION_2 + "</span></span>";
  html += "</div>";
  html += "<div class='intensity-bar'><div id='bLd' class='intensity-fill' style='width:" + String(min(rainIntensityLOCATION_2 * 20, 100.0f)) + "%'></div></div>";
  html += "<button onclick=\"return sendCmd('/pingLOCATION_2');\" style='margin-top:8px; padding:5px 10px; width:100%; background:transparent; border:1px solid rgba(0,212,255,0.3); color:#00d4ff; font-size:0.7em; cursor:pointer;'>PING LOCATION</button>";
  html += "</div>";
  html += "</div>";
  
  // Control Buttons
  html += "<button onclick=\"return sendCmd('/lamp');\" class='btn'>Toggle Internal Lighting</button>";
  html += "<button id='muteBtn' onclick=\"return sendCmd('/mute');\" class='btn " + String(buzzerMuted ? "muted" : "") + "'>Audio: " + String(buzzerMuted ? "MUTED" : "ACTIVE") + "</button>";
  html += "<button id='solBtn' onclick=\"return sendCmd('/solenoid');\" class='btn " + String(solenoidDisabled ? "disabled" : "") + "'>Solenoid: " + String(solenoidDisabled ? "DISABLED" : "ACTIVE") + "</button>";
  
  // Odradek Functions
  html += "<div style='font-size:0.7em; margin: 20px 0 10px 0; opacity:0.6;'>ODRADEK FUNCTIONS:</div>";
  html += "<button onclick=\"return sendCmd('/territoryScan');\" class='btn scan'>Territory Scan</button>";
  html += "<button onclick=\"return sendCmd('/strandCalibration');\" class='btn scan'>Strand Calibration</button>";
  html += "<button onclick=\"return sendCmd('/doomsResonance');\" class='btn scan'>DOOMS Resonance</button>";
  html += "<button onclick=\"return sendCmd('/phantomDetection');\" class='btn scan'>Phantom Detection</button>";
  html += "<button onclick=\"return sendCmd('/chiralDensity');\" class='btn scan'>Chiral Density Test</button>";
  html += "<button onclick=\"return sendCmd('/voidoutSim');\" class='btn scan'>Void-out Simulation</button>";
  
  // Alarm System
  html += "<div class='alarm-form'>";
  html += "<div style='font-size:0.9em; margin-bottom:15px; font-weight:bold; letter-spacing:2px;'>ALERT SCHEDULER</div>";
  html += "<form onsubmit='return setAlarm(event);'>";
  html += "<div class='alarm-input-row'>";
  html += "<input id='alarmTime' type='time' required placeholder='Set Time'>";
  html += "<select id='alarmType'><option value='0'>MORNING WAKE</option><option value='1'>REMINDER</option></select>";
  html += "</div>";
  html += "<input type='submit' value='ARM ALERT' class='alarm-submit'>";
  html += "</form>";
  html += "<div class='alarm-list' id='alarmList'><div style='opacity:0.5; text-align:center; padding:20px;'>Loading alarms...</div></div>";
  html += "</div>";
  
  // Diagnostics
  html += "<div style='font-size:0.7em; margin-bottom:10px; opacity:0.6;'>DIAGNOSTICS & TELEMETRY:</div>";
  html += "<button onclick=\"return sendCmd('/alive');\" class='btn diag'>Execute Alive Check</button>";
  html += "<button onclick=\"return sendCmd('/tLOCATION_1');\" class='btn diag'>Sync: LOCATION_1_NAME</button>";
  html += "<button onclick=\"return sendCmd('/tLOCATION_3');\" class='btn diag'>Sync: LOCATION_3_NAME</button>";
  html += "<button onclick=\"return sendCmd('/tLOCATION_2');\" class='btn diag'>Sync: LOCATION_2_NAME</button>";
  html += "<br><button onclick=\"return sendCmd('/standby');\" class='btn diag'>Enter Standby Mode</button>";
  html += "<button onclick=\"return sendCmd('/purge');\" class='btn clear'>Emergency Purge (Stop All)</button>";
  html += "</div></body></html>";
  return html;
}

//ugh thank god claude exists debugging ESPECIALLY the web interface was painful, so yeah shoutout to claude

String getUploadHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { background: #000; color: #00d4ff; font-family: 'Courier New', monospace; padding: 20px; }";
  html += "body::before { content: ' '; display: block; position: fixed; top: 0; left: 0; bottom: 0; right: 0; background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%), linear-gradient(90deg, rgba(255, 0, 0, 0.06), rgba(0, 255, 0, 0.02), rgba(0, 0, 255, 0.06)); z-index: 10; background-size: 100% 2px, 3px 100%; pointer-events: none; }";
  html += ".container { max-width: 600px; margin: 0 auto; border: 2px solid #00d4ff; padding: 30px; background: rgba(0, 212, 255, 0.02); position: relative; z-index: 20; }";
  html += "h1 { text-align: center; letter-spacing: 4px; font-size: 1.5em; margin-bottom: 10px; text-shadow: 0 0 10px #00d4ff; }";
  html += ".warning { border: 2px solid #ff3300; padding: 15px; margin: 20px 0; background: rgba(255, 51, 0, 0.1); color: #ff6600; text-align: center; font-weight: bold; }";
  html += ".upload-area { border: 2px dashed #00d4ff; padding: 40px; text-align: center; margin: 20px 0; background: rgba(0, 212, 255, 0.05); cursor: pointer; transition: 0.3s; }";
  html += ".upload-area:hover { background: rgba(0, 212, 255, 0.15); border-color: #00ffff; }";
  html += ".upload-area.dragging { background: rgba(0, 255, 255, 0.2); border-color: #00ffff; }";
  html += "input[type='file'] { display: none; }";
  html += ".btn { display: block; width: 100%; padding: 15px; margin: 10px 0; background: #00d4ff; color: #000; border: none; cursor: pointer; font-weight: bold; letter-spacing: 2px; text-transform: uppercase; font-family: 'Courier New', monospace; font-size: 1em; transition: 0.2s; }";
  html += ".btn:hover { background: #00ffff; box-shadow: 0 0 20px #00d4ff; }";
  html += ".btn:disabled { background: #333; color: #666; cursor: not-allowed; }";
  html += ".progress-container { display: none; margin: 20px 0; }";
  html += ".progress-bar { width: 100%; height: 30px; background: rgba(0, 212, 255, 0.1); border: 1px solid #00d4ff; position: relative; overflow: hidden; }";
  html += ".progress-fill { height: 100%; background: linear-gradient(90deg, #00d4ff, #00ffff); width: 0%; transition: width 0.3s; display: flex; align-items: center; justify-content: center; color: #000; font-weight: bold; }";
  html += ".status { text-align: center; margin: 20px 0; font-size: 1.1em; padding: 10px; }";
  html += ".status.success { color: #00ff00; border: 1px solid #00ff00; background: rgba(0, 255, 0, 0.05); animation: pulse 2s infinite; }";
  html += ".status.error { color: #ff3300; border: 1px solid #ff3300; background: rgba(255, 51, 0, 0.05); }";
  html += ".status.uploading { color: #ffaa00; border: 1px solid #ffaa00; background: rgba(255, 170, 0, 0.05); }";
  html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }";
  html += ".info { font-size: 0.8em; opacity: 0.7; margin: 10px 0; }";
  html += ".back-link { display: block; text-align: center; margin-top: 20px; color: #00d4ff; text-decoration: none; opacity: 0.7; transition: 0.2s; }";
  html += ".back-link:hover { opacity: 1; text-shadow: 0 0 10px #00d4ff; }";
  html += ".file-info { margin: 15px 0; padding: 10px; background: rgba(0, 212, 255, 0.05); border-left: 3px solid #00d4ff; font-size: 0.9em; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>⚡ ODRADEK OTA UPLOAD ⚡</h1>";
  html += "<div style='text-align:center; font-size:0.7em; opacity:0.5; letter-spacing:3px; margin-bottom:30px;'>BRIDGES FIRMWARE UPDATE PROTOCOL</div>";
  
  html += "<div class='warning'>⚠️ WARNING ⚠️<br>System will restart after upload<br>Do not disconnect power during update</div>";
  
  html += "<div id='uploadArea' class='upload-area' onclick='document.getElementById(\"fileInput\").click()'>";
  html += "<div style='font-size:3em; margin-bottom:10px;'>📁</div>";
  html += "<div style='font-size:1.2em; margin-bottom:5px;'>Click or drag .bin file here</div>";
  html += "<div class='info'>Accepted: .bin firmware files only</div>";
  html += "</div>";
  
  html += "<input type='file' id='fileInput' accept='.bin' onchange='handleFile(this.files[0])'>";
  html += "<div id='fileInfo' class='file-info' style='display:none;'></div>";
  html += "<button id='uploadBtn' class='btn' onclick='uploadFile()' disabled>UPLOAD FIRMWARE</button>";
  
  html += "<div class='progress-container' id='progressContainer'>";
  html += "<div class='progress-bar'><div class='progress-fill' id='progressFill'>0%</div></div>";
  html += "</div>";
  
  html += "<div id='status' class='status'></div>";
  html += "<a href='/' class='back-link'>← Back to Main Interface</a>";
  html += "</div>";
  
  html += "<script>";
  html += "let selectedFile = null;";
  
  // File selection handler
  html += "function handleFile(file) {";
  html += "  if(!file) return;";
  html += "  if(!file.name.endsWith('.bin')) {";
  html += "    showStatus('Error: Only .bin files accepted', 'error');";
  html += "    return;";
  html += "  }";
  html += "  selectedFile = file;";
  html += "  document.getElementById('fileInfo').style.display = 'block';";
  html += "  document.getElementById('fileInfo').innerHTML = 'File: <b>' + file.name + '</b><br>Size: <b>' + (file.size/1024).toFixed(2) + ' KB</b>';";
  html += "  document.getElementById('uploadBtn').disabled = false;";
  html += "  showStatus('Ready to upload', '');";
  html += "}";
  
  // Upload function
  html += "function uploadFile() {";
  html += "  if(!selectedFile) return;";
  html += "  document.getElementById('uploadBtn').disabled = true;";
  html += "  document.getElementById('progressContainer').style.display = 'block';";
  html += "  showStatus('Uploading firmware...', 'uploading');";
  html += "  const formData = new FormData();";
  html += "  formData.append('firmware', selectedFile);";
  html += "  const xhr = new XMLHttpRequest();";
  html += "  xhr.upload.onprogress = function(e) {";
  html += "    if(e.lengthComputable) {";
  html += "      const percent = Math.round((e.loaded / e.total) * 100);";
  html += "      document.getElementById('progressFill').style.width = percent + '%';";
  html += "      document.getElementById('progressFill').innerText = percent + '%';";
  html += "    }";
  html += "  };";
  html += "  xhr.onload = function() {";
  html += "    if(xhr.status === 200) {";
  html += "      showStatus('✓ Upload successful! Rebooting system...', 'success');";
  html += "      setTimeout(() => { window.location.href = '/'; }, 8000);";
  html += "    } else {";
  html += "      showStatus('✗ Upload failed: ' + xhr.responseText, 'error');";
  html += "      document.getElementById('uploadBtn').disabled = false;";
  html += "    }";
  html += "  };";
  html += "  xhr.onerror = function() {";
  html += "    showStatus('✗ Upload failed: Connection error', 'error');";
  html += "    document.getElementById('uploadBtn').disabled = false;";
  html += "  };";
  html += "  xhr.open('POST', '/update', true);";
  html += "  xhr.send(formData);";
  html += "}";
  
  // Status display
  html += "function showStatus(msg, type) {";
  html += "  const statusEl = document.getElementById('status');";
  html += "  statusEl.innerText = msg;";
  html += "  statusEl.className = 'status ' + type;";
  html += "}";
  
  // Drag and drop
  html += "const uploadArea = document.getElementById('uploadArea');";
  html += "uploadArea.addEventListener('dragover', (e) => { e.preventDefault(); uploadArea.classList.add('dragging'); });";
  html += "uploadArea.addEventListener('dragleave', () => { uploadArea.classList.remove('dragging'); });";
  html += "uploadArea.addEventListener('drop', (e) => {";
  html += "  e.preventDefault();";
  html += "  uploadArea.classList.remove('dragging');";
  html += "  if(e.dataTransfer.files.length > 0) handleFile(e.dataTransfer.files[0]);";
  html += "});";
  html += "</script>";
  
  html += "</body></html>";
  return html;
}

// ==========================================
// CORE 0: THE BRAIN
// ==========================================

void NetworkTaskCode(void * pvParameters) {
  // Set hostname BEFORE connecting
  WiFi.setHostname("odradek");
  
    // INITIALIZE HEAP VALUES IMMEDIATELY
  freeHeap = ESP.getFreeHeap();
  totalHeap = ESP.getHeapSize();

  Core0TaskHandle = xTaskGetCurrentTaskHandle();

  // Start WiFi connection first
  WiFi.mode(WIFI_AP_STA); // Both AP and Station mode
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  
  // Wait for WiFi with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed, continuing with AP mode only");
  }
  
  // Start Access Point
  WiFi.softAP("ODRADEK_>MADIS");
  delay(100);
  
  // Start mDNS - try multiple times if needed
  bool mdnsStarted = false;
  for(int i=0; i<3; i++) {
    if(MDNS.begin("odradek")) {
      mdnsStarted = true;
      Serial.println("mDNS responder started successfully!");
      MDNS.addService("http", "tcp", 80);
      break;
    } else {
      Serial.printf("mDNS start attempt %d failed, retrying...\n", i+1);
      delay(500);
    }
  }
  
  if(!mdnsStarted) {
    Serial.println("WARNING: mDNS failed to start - use IP address instead");
  }
  
  // Print all access methods
  Serial.println("\n=== ACCESS POINTS ===");
  if(mdnsStarted) {
    Serial.println("1. Via mDNS: http://odradek.local");
  }
  Serial.print("2. Via WiFi IP: http://");
  Serial.println(WiFi.localIP());
  Serial.println("3. Via AP IP: http://172.217.28.1");
  Serial.println("=====================\n");

  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  
server.on("/data", []() {
    struct tm timeinfo;
    char timeStr[12] = "00:00:00";
    if(getLocalTime(&timeinfo)) {
        strftime(timeStr, 12, "%H:%M:%S", &timeinfo);
    }
    
    // Update system stats
    freeHeap = ESP.getFreeHeap();
    totalHeap = ESP.getHeapSize();
    wifiRSSI = WiFi.RSSI();
    
    // SAFE CPU LOAD CALCULATION - No crashes!
    unsigned long now = millis();
    if(now - lastCpuCheck >= 1000) {
        // Method 1: Use uxTaskGetNumberOfTasks() - always safe
        UBaseType_t taskCount = uxTaskGetNumberOfTasks();
        
        // Method 2: Estimate based on system activity
        bool wifiActive = (WiFi.status() == WL_CONNECTED);
        bool webActive = (server.client().connected());
        
        // Calculate realistic load estimates
        float baseLoad = 15.0; // Base system load
        
        // Add load from active tasks
        if(wifiActive) baseLoad += 8.0;
        if(webActive) baseLoad += 5.0;
        if(!standbyMode) baseLoad += 12.0;
        if(lampMode || warmLampMode || warningMode) baseLoad += 3.0;
        
        // Add variation based on task count
        baseLoad += (taskCount - 2) * 2.5;
        
        // Smooth transitions
        float targetCore0 = baseLoad + random(-3, 3);
        float targetCore1 = baseLoad * 0.85 + random(-3, 3);
        
        core0Load = core0Load * 0.8 + targetCore0 * 0.2;
        core1Load = core1Load * 0.8 + targetCore1 * 0.2;
        cpuLoad = (core0Load + core1Load) / 2.0;
        
        // Constrain to valid range
        core0Load = constrain(core0Load, 5, 100);
        core1Load = constrain(core1Load, 5, 100);
        cpuLoad = constrain(cpuLoad, 5, 100);
        
        lastCpuCheck = now;
    }

    uint32_t safeHeapPct = (totalHeap > 0) ? (freeHeap * 100) / totalHeap : 0;
    
    String json = "{";
    json += "\"hL\":" + String(humidityLOCATION_1, 1) + ",";
    json += "\"hD\":" + String(humidityLOCATION_3, 1) + ",";
    json += "\"hLd\":" + String(humidityLOCATION_2, 1) + ",";
    json += "\"tL\":" + String(tempLOCATION_1, 1) + ",";
    json += "\"tD\":" + String(tempLOCATION_3, 1) + ",";
    json += "\"tLd\":" + String(tempLOCATION_2, 1) + ",";
    json += "\"rL\":" + String(rainIntensityLOCATION_1, 2) + ",";
    json += "\"rD\":" + String(rainIntensityLOCATION_3, 2) + ",";
    json += "\"rLd\":" + String(rainIntensityLOCATION_2, 2) + ",";
    json += "\"rcL\":" + String(rainChanceLOCATION_1) + ",";
    json += "\"rcD\":" + String(rainChanceLOCATION_3) + ",";
    json += "\"rcLd\":" + String(rainChanceLOCATION_2) + ",";
    json += "\"scL\":\"" + skyConditionLOCATION_1 + "\",";
    json += "\"scD\":\"" + skyConditionLOCATION_3 + "\",";
    json += "\"scLd\":\"" + skyConditionLOCATION_2 + "\",";
    json += "\"rssi\":\"" + String(wifiRSSI) + "\",";
    json += "\"heap\":\"" + String(freeHeap/1024) + " KB\",";
    json += "\"heapPct\":" + String(safeHeapPct) + ",";
    json += "\"cpu\":\"" + String(cpuLoad, 1) + "\",";
    json += "\"core0\":\"" + String(core0Load, 1) + "\",";
    json += "\"core1\":\"" + String(core1Load, 1) + "\",";
    json += "\"muted\":" + String(buzzerMuted ? "true" : "false") + ",";
    json += "\"solDisabled\":" + String(solenoidDisabled ? "true" : "false") + ",";
    json += "\"time\":\"" + String(timeStr) + "\",";
    json += "\"upd\":\"" + lastWeatherUpdate + "\"";
    json += "}";
    server.send(200, "application/json", json);
});
  
  server.on("/alarms", []() {
    String json = "{\"count\":" + String(alarmCount) + ",\"alarms\":[";
    for(int i=0; i<alarmCount; i++) {
      if(i > 0) json += ",";
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", alarms[i].hour, alarms[i].minute);
      json += "{\"time\":\"" + String(timeStr) + "\",";
      json += "\"typeName\":\"" + String(alarms[i].type == 0 ? "MORNING WAKE" : "REMINDER") + "\"}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });
  
  server.on("/purge", []() {
    emergencyStop = true;
    lampMode = false; warmLampMode = false; warningMode = false; standbyMode = false;
    alarmCount = 0;
    animationTrigger = 0;
    ledcWrite(SOLENOID_PIN, 0);
    ledcWriteTone(BUZZER_PIN, 0);
    strip.fill(0); strip.show();
    server.send(200, "text/plain", "OK");
    Serial.println("!!! EMERGENCY PURGE ACTIVATED !!!");
  });
  
  server.on("/setBrightness", []() {
    ledBrightness = server.arg("val").toInt();
    ledBrightness = constrain(ledBrightness, 10, 255);
    strip.setBrightness(ledBrightness);
    strip.show();
    server.send(200, "text/plain", "OK");
  });

  server.on("/mute", []() {
    buzzerMuted = !buzzerMuted;
    server.send(200, "text/plain", "OK");
  });

  server.on("/solenoid", []() {
    solenoidDisabled = !solenoidDisabled;
    server.send(200, "text/plain", "OK");
  });

  server.on("/setAlarm", []() {
    if(alarmCount >= 5) {
      server.send(400, "text/plain", "Max alarms reached");
      return;
    }
    String t = server.arg("time");
    if(t.length() == 5) {
      alarms[alarmCount].active = true;
      alarms[alarmCount].hour = t.substring(0, 2).toInt();
      alarms[alarmCount].minute = t.substring(3, 5).toInt();
      alarms[alarmCount].type = server.arg("type").toInt();
      alarmCount++;
    }
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/cancelAlarm", []() {
    int idx = server.arg("idx").toInt();
    if(idx >= 0 && idx < alarmCount) {
      for(int i=idx; i<alarmCount-1; i++) {
        alarms[i] = alarms[i+1];
      }
      alarmCount--;
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/alive", []() { 
    animationTrigger = 20; // Ultimate alive check
    server.send(200, "text/plain", "OK");
  });

  server.on("/tLOCATION_1", []() { animationTrigger = 1; server.send(200, "text/plain", "OK"); });
  server.on("/tLOCATION_3", []() { animationTrigger = 2; server.send(200, "text/plain", "OK"); });
  server.on("/tLOCATION_2", []() { animationTrigger = 3; server.send(200, "text/plain", "OK"); });

  server.on("/pingLOCATION_1", []() { 
    strip.fill(strip.Color(0, 0, 255)); strip.show();
    if(!buzzerMuted) slideChirp(6000, 8000, 200);
    delay(300);
    strip.fill(strip.Color(0, 5, 50)); strip.show();
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/pingLOCATION_3", []() { 
    for(int i=0; i<3; i++) {
      strip.fill(strip.Color(0, 0, 200)); strip.show();
      if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 7000);
      delay(80);
      strip.fill(0); strip.show();
      ledcWriteTone(BUZZER_PIN, 0);
      delay(80);
    }
    strip.fill(strip.Color(0, 5, 50)); strip.show();
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/pingLOCATION_2", []() { 
    int center = NUM_LEDS / 2;
    for(int i=0; i<30; i+=3) {
      strip.setPixelColor(center + i, 0, 5, 100);
      strip.setPixelColor(center - i, 0, 5, 100);
      strip.show();
      if(!buzzerMuted) ledcWriteTone(BUZZER_PIN, 4000 + (i * 30));
      delay(30);
    }
    ledcWriteTone(BUZZER_PIN, 0);
    strip.fill(strip.Color(0, 5, 50)); strip.show();
    server.send(200, "text/plain", "OK");
  });

  server.on("/lamp", []() {
    lampMode = !lampMode; warmLampMode = false; standbyMode = false;
    uint32_t currentColor = strip.getPixelColor(0);
    sciFiMorph(currentColor, lampMode ? strip.Color(230, 210, 180) : strip.Color(0, 5, 50), 800, true);
    server.send(200, "text/plain", "OK");
  });
  
  server.on("/standby", []() { 
    standbyMode = true; lampMode = false; warmLampMode = false; warningMode = false;
    // Glitch transition
    for(int i=0; i<10; i++) {
      strip.fill(strip.Color(random(0, 100), random(0, 100), random(0, 100)));
      strip.show();
      delay(30);
    }
    strip.fill(0); strip.show(); 
    server.send(200, "text/plain", "OK"); 
  });
  
  // New Odradek functions
  server.on("/territoryScan", []() { animationTrigger = 10; server.send(200, "text/plain", "OK"); });
  server.on("/strandCalibration", []() { animationTrigger = 11; server.send(200, "text/plain", "OK"); });
  server.on("/doomsResonance", []() { animationTrigger = 12; server.send(200, "text/plain", "OK"); });
  server.on("/phantomDetection", []() { animationTrigger = 13; server.send(200, "text/plain", "OK"); });
  server.on("/chiralDensity", []() { animationTrigger = 14; server.send(200, "text/plain", "OK"); });
  server.on("/voidoutSim", []() { animationTrigger = 15; server.send(200, "text/plain", "OK"); });


// OTA Update handler
  server.on("/update", HTTP_POST, 
    []() {
      // After upload completion
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      delay(1000);
      ESP.restart();
    },
    []() {
      // During upload
      HTTPUpload& upload = server.upload();
      
      if (upload.status == UPLOAD_FILE_START) {
        otaInProgress = true;
        Serial.printf("OTA Update: %s\n", upload.filename.c_str());
        
        // Stop animations and clear LEDs
        lampMode = false;
        warmLampMode = false;
        warningMode = false;
        standbyMode = true;
        strip.fill(strip.Color(255, 100, 0)); // Orange = updating
        strip.show();
        ledcWrite(SOLENOID_PIN, 0);
        ledcWriteTone(BUZZER_PIN, 0);
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      } 
      else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write firmware chunk
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        } else {
          // Visual feedback - blink during upload
          static unsigned long lastBlink = 0;
          if (millis() - lastBlink > 200) {
            static bool state = false;
            strip.fill(state ? strip.Color(255, 100, 0) : strip.Color(100, 50, 0));
            strip.show();
            state = !state;
            lastBlink = millis();
          }
        }
      } 
      else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("OTA Update Success: %u bytes\n", upload.totalSize);
          // Success - green flash
          strip.fill(strip.Color(0, 255, 0));
          strip.show();
          if(!buzzerMuted) {
            ledcWriteTone(BUZZER_PIN, 6000);
            delay(200);
            ledcWriteTone(BUZZER_PIN, 0);
          }
        } else {
          Update.printError(Serial);
          // Error - red flash
          strip.fill(strip.Color(255, 0, 0));
          strip.show();
        }
        otaInProgress = false;
      }
    }
  );


  server.begin();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Enable watchdog for this task
  esp_task_wdt_add(NULL);

  for(;;) {
    esp_task_wdt_reset(); // Feed watchdog
    
    server.handleClient();
    unsigned long now = millis();
    
    // Update connection strength
    if (WiFi.status() == WL_CONNECTED) {
      int rssi = WiFi.RSSI();
      wifiRSSI = rssi;
      connectionStrength = map(rssi, -90, -30, 10, 100);
      connectionStrength = constrain(connectionStrength, 0, 100);
    } else {
      connectionStrength = 0;
      wifiRSSI = 0;
      static bool firstRun = true;
      if(!firstRun && !wifiErrorState) {
        wifiFailsafe();
      }
      firstRun = false;
    }
    
    if (now - lastWeatherCheck > weatherCheckInterval) {
      fetchWeatherData();
      lastWeatherCheck = now;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// CORE 1: THE BODY
// ==========================================

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  ledcAttach(SOLENOID_PIN, 30, 8);
  ledcAttach(BUZZER_PIN, 1000, 8);
  strip.begin(); strip.setBrightness(150); strip.show();

  Core1TaskHandle = xTaskGetCurrentTaskHandle();

  // FIXED: Initialize watchdog with simpler approach for compatibility
  Serial.println("Initializing watchdog timer...");
  esp_err_t wdt_result = esp_task_wdt_deinit(); // Deinit first if already initialized
  
  // Try new API first
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = (uint32_t)(WDT_TIMEOUT * 1000),
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  wdt_result = esp_task_wdt_init(&wdt_config);
  
  if(wdt_result == ESP_OK || wdt_result == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_add(NULL); // Add loop task to watchdog
    Serial.println("Watchdog initialized successfully");
  } else {
    Serial.printf("Watchdog init returned: %d (continuing anyway)\n", wdt_result);
  }
  
  // Initialize alarms
  for(int i=0; i<5; i++) {
    alarms[i].active = false;
  }

  odradekUltimateStartup();

  xTaskCreatePinnedToCore(NetworkTaskCode, "BrainTask", 10000, NULL, 1, NULL, 0);
}

void loop() {
  unsigned long now = millis();

    if(otaInProgress) {
    delay(100);
    return;
  }
  
  static unsigned long lastMemPrint = 0;
if(millis() - lastMemPrint > 5000) {
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  lastMemPrint = millis();
}

  // Feed watchdog on loop task
  static unsigned long lastWdtFeed = 0;
  if(now - lastWdtFeed > 1000) {
    esp_task_wdt_reset();
    lastWdtFeed = now;
  }
  
  // Animation Trigger Check
  if (animationTrigger > 0) {
    bool isStorm = (skyConditionLOCATION_1 == "Thunderstorm" || skyConditionLOCATION_3 == "Thunderstorm" || skyConditionLOCATION_2 == "Thunderstorm");
    
    if (animationTrigger == 1) pulseLOCATION_1(isStorm);
    else if (animationTrigger == 2) pulseLOCATION_3(isStorm);
    else if (animationTrigger == 3) pulseLOCATION_2(isStorm);
    else if (animationTrigger == 10) territoryScan();
    else if (animationTrigger == 11) strandCalibration();
    else if (animationTrigger == 12) doomsResonance();
    else if (animationTrigger == 13) phantomDetection();
    else if (animationTrigger == 14) chiralDensityTest();
    else if (animationTrigger == 15) voidoutSimulation();
    else if (animationTrigger == 20) ultimateAliveCheck();
    animationTrigger = 0;
  }
  

  // Check for cold shiver (once per day, below 7°C)
  if(!hasShiveredToday && isWithinReactionHours() && tempLOCATION_1 != 0) {
    if(tempLOCATION_1 < 7.0 || tempLOCATION_3 < 7.0 || tempLOCATION_2 < 7.0) {
      coldShiver();
    }
  }
  
  // Reset shiver flag at midnight
  struct tm timeinfo;
  if(getLocalTime(&timeinfo) && timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
    hasShiveredToday = false;
  }
  
  // Check CPU stress
  if(cpuLoad > 85 && !hasStressed && isWithinReactionHours()) {
    cpuStressAnimation();
  }
  if(cpuLoad < 70) hasStressed = false; // Reset
  
  // Check low memory (critical threshold)
  if(ESP.getFreeHeap() < 10000) { 
  lowMemoryRestart();
}

  // Touch Logic
  bool currentTouch = digitalRead(TOUCH_PIN);
  if (currentTouch != lastTouchState) {
    if (currentTouch == HIGH) { touchStartTime = now; }
    else {
      unsigned long duration = now - touchStartTime;
      if (duration < longPressDuration) {
        if (now - lastTapTime > tapInterval) tapCount = 1; else tapCount++;
        lastTapTime = now;
      }
    }
    lastTouchState = currentTouch; delay(20);
  }

  if (currentTouch == HIGH && (now - touchStartTime > longPressDuration)) {
    standbyMode = !standbyMode;
    if (standbyMode) {
      // Glitch transition before turning off
      for(int i=0; i<10; i++) {
        strip.fill(strip.Color(random(0, 100), random(0, 100), random(0, 100)));
        strip.show();
        delay(30);
      }
      strip.fill(0); strip.show();
      harmonicChirp(50);
    }
    else { sciFiMorph(0, strip.Color(0, 5, 50), 600, true); }
    while(digitalRead(TOUCH_PIN) == HIGH);
  }

  if (tapCount > 0 && (now - lastTapTime > tapInterval)) {
    if (tapCount == 2) { 
      lampMode = !lampMode; warmLampMode = false; standbyMode = false; 
      uint32_t currentColor = strip.getPixelColor(0);
      sciFiMorph(currentColor, lampMode ? strip.Color(230, 210, 180) : strip.Color(0, 5, 50), 800, true);
    }
    else if (tapCount == 3) { 
      warningMode = !warningMode; warmLampMode = false; standbyMode = false; 
      uint32_t currentColor = strip.getPixelColor(0);
      sciFiMorph(currentColor, warningMode ? strip.Color(255, 120, 0) : strip.Color(0, 5, 50), 800, true);
    }
    else if (tapCount == 4) {
      // Super warm comfort lamp (orange)
      warmLampMode = !warmLampMode; lampMode = false; warningMode = false; standbyMode = false;
      uint32_t targetColor = warmLampMode ? strip.Color(255, 140, 30) : strip.Color(0, 5, 50);
      sciFiMorph(strip.getPixelColor(0), targetColor, 1000, true);
    }
    tapCount = 0;
  }

  if (alarmCount > 0 && (now % 10000 < 200)) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      for(int i=0; i<alarmCount; i++) {
        if(alarms[i].active && timeinfo.tm_hour == alarms[i].hour && timeinfo.tm_min == alarms[i].minute) {
          alarms[i].active = false; 
          if (alarms[i].type == 0) {
            gentleMorningWake();
          } else {
            reminderAlert();
          }
          for(int j=i; j<alarmCount-1; j++) {
            alarms[j] = alarms[j+1];
          }
          alarmCount--;
          break;
        }
      }
    }
  }

  if (standbyMode) {
    strip.fill(0); strip.show();
  } else if (warmLampMode) {
    strip.fill(strip.Color(255, 140, 30)); strip.show();
  } else if (warningMode) {
    strip.fill(strip.Color(random(200,255), 100, 0)); strip.show();
    if(random(0,100)>90) solenoidPulse(180, 10);
    delay(30);
  } else if (lampMode) {
    strip.fill(strip.Color(230, 210, 180)); strip.show();
  } else {
    static float angle = 0; angle += 0.03;
    strip.fill(strip.Color(0, 5, (sin(angle)*30)+40)); strip.show(); delay(20);
  }
}
