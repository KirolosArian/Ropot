/* CloudRover V1.0 - Main ESP32 NodeMCU (DevKit V1) firmware
 * Board: "ESP32 Dev Module" | Libraries: Adafruit GFX, Adafruit ST7735 and ST7789,
 *        Adafruit VL53L0X (or Pololu VL53L1X if USE_VL53L1X is 1)
 * Eyes: ST7735 160x128 (landscape) | Sound: DAC1 GPIO25 | Motors: L298N | 4G: SIM7600E-H
 */
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define USE_VL53L1X 0               // 0 = VL53L0X, 1 = VL53L1X
#if USE_VL53L1X
  #include <VL53L1X.h>
  VL53L1X tof;
#else
  #include <Adafruit_VL53L0X.h>
  Adafruit_VL53L0X tof;
#endif

// ---------------- PIN MAP ----------------
#define TFT_CS 5
#define TFT_RST 4
#define TFT_DC 2
#define PIN_MIC 34
#define PIN_DAC 25
#define PIN_SOLAR 35                // solar sense: Solar+ -100k- GPIO35 -100k- GND
#define M_IN1 12
#define M_IN2 13
#define M_IN3 14
#define M_IN4 27
#define M_ENA 32
#define M_ENB 33
#define SIM_RX 16                   // ESP RX2 <- SIM7600 TXD
#define SIM_TX 17                   // ESP TX2 -> SIM7600 RXD

// ---------------- SETTINGS ----------------
#define STOP_MM 150                 // emergency stop distance
#define CLAP_LEVEL 900              // peak-to-peak ADC counts (0-4095) = clap
#define DRIVE_SPEED 200             // PWM 0-255
#define CMD_TIMEOUT_MS 1500         // drive command auto-stops after this

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 cv(160, 128);           // off-screen buffer = flicker-free animation
#define SIM Serial2

// ---------------- AUDIO (FreeRTOS task, non-blocking) ----------------
enum Snd { SND_NONE, SND_STARTUP, SND_ALARM, SND_CLAP, SND_CHARGE, SND_HAPPY };
QueueHandle_t sndQ;
uint8_t sineTab[256];

void playNote(int hz, int ms) {
  if (hz <= 0) { dacWrite(PIN_DAC, 128); vTaskDelay(ms / portTICK_PERIOD_MS); return; }
  uint32_t period = 1000000UL / hz, t0 = micros(), endT = t0 + (uint32_t)ms * 1000UL;
  while ((int32_t)(endT - micros()) > 0) {
    uint32_t ph = ((micros() - t0) % period) * 256UL / period;
    dacWrite(PIN_DAC, sineTab[ph & 255]);
    delayMicroseconds(25);
  }
  dacWrite(PIN_DAC, 128);
  vTaskDelay(15 / portTICK_PERIOD_MS);
}
void audioTask(void *) {
  Snd s;
  for (;;) {
    if (xQueueReceive(sndQ, &s, portMAX_DELAY) != pdTRUE) continue;
    switch (s) {
      case SND_STARTUP: playNote(523,120); playNote(659,120); playNote(784,120); playNote(1047,300); break;
      case SND_ALARM:   for (int i = 0; i < 3; i++) { playNote(1800,120); playNote(0,60); } break;
      case SND_CLAP:    for (int f = 700; f < 1900; f += 150) playNote(f, 25); break;
      case SND_CHARGE:  playNote(880,100); playNote(1175,100); playNote(1568,200); break;
      case SND_HAPPY:   playNote(988,80); playNote(1319,80); playNote(1568,160); break;
      default: break;
    }
    dacWrite(PIN_DAC, 128);
  }
}
void sound(Snd s) { xQueueSend(sndQ, &s, 0); }

// ---------------- MOTORS ----------------
unsigned long lastDriveCmd = 0;
bool driving = false;
char driveDir = 'S';

void pwmWrite(int pin, int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(pin == M_ENA ? 0 : 1, duty);
#endif
}
void motorsInit() {
  int p[] = {M_IN1, M_IN2, M_IN3, M_IN4};
  for (int i = 0; i < 4; i++) { pinMode(p[i], OUTPUT); digitalWrite(p[i], LOW); }
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(M_ENA, 5000, 8); ledcAttach(M_ENB, 5000, 8);
#else
  ledcSetup(0, 5000, 8); ledcSetup(1, 5000, 8);
  ledcAttachPin(M_ENA, 0); ledcAttachPin(M_ENB, 1);
#endif
}
void setMotors(bool l1, bool l2, bool r1, bool r2, int spd) {
  digitalWrite(M_IN1, l1); digitalWrite(M_IN2, l2);
  digitalWrite(M_IN3, r1); digitalWrite(M_IN4, r2);
  pwmWrite(M_ENA, spd); pwmWrite(M_ENB, spd);
}
// If a side runs backwards, swap its two wires on the L298N OUT terminals.
void stopMotors() { setMotors(0,0,0,0,0); driving = false; driveDir = 'S'; }
void forward(int s = DRIVE_SPEED)  { setMotors(1,0,1,0,s); driving = true; driveDir = 'F'; }
void backward(int s = DRIVE_SPEED) { setMotors(0,1,0,1,s); driving = true; driveDir = 'B'; }
void turnLeft(int s = DRIVE_SPEED) { setMotors(0,1,1,0,s); driving = true; driveDir = 'L'; }
void turnRight(int s = DRIVE_SPEED){ setMotors(1,0,0,1,s); driving = true; driveDir = 'R'; }

// ---------------- EYES ----------------
enum Mood { M_NORMAL, M_HAPPY, M_SHOCK, M_CURIOUS };
Mood mood = M_NORMAL;
unsigned long moodUntil = 0, nextBlink = 0, blinkStart = 0, nextGlance = 0;
float lookX = 0, lookTarget = 0;          // -1 (left) .. +1 (right)
const uint16_t C_EYE = 0x07FF;            // cyan
const uint16_t C_BG = 0x0000;

void setMood(Mood m, unsigned long ms) { mood = m; moodUntil = millis() + ms; }

void drawEye(int cx, int cy, int w, int h, int lid, bool happy) {
  // lid: 0..100 percent closed. happy: cheeks push the lower lid up
  int hh = h * (100 - lid) / 100; if (hh < 4) hh = 4;
  cv.fillRoundRect(cx - w / 2, cy - hh / 2, w, hh, 12, C_EYE);
  if (happy) cv.fillCircle(cx, cy + h / 2 + 6, w * 0.62, C_BG);   // masks lower half -> ^ shape
}
void drawEyes() {
  cv.fillScreen(C_BG);
  unsigned long now = millis();
  lookX += (lookTarget - lookX) * 0.25f;                   // smooth gaze
  int w = 46, h = 56, lid = 0, dx = (int)(lookX * 14);
  bool happy = false;
  if (mood == M_SHOCK) { w = 54; h = 78; }
  if (mood == M_HAPPY) happy = true;
  if (mood == M_CURIOUS) { h = 62; }
  if (blinkStart) {                                         // 220 ms blink
    unsigned long t = now - blinkStart;
    if (t > 220) { blinkStart = 0; nextBlink = now + random(1800, 5000); }
    else lid = (t < 110) ? t * 100 / 110 : (220 - t) * 100 / 110;
  } else if (now > nextBlink) blinkStart = now;
  drawEye(46 + dx, 64, w, h, lid, happy);
  drawEye(114 + dx, 64, w, h, lid, happy);
  if (mood == M_SHOCK) { cv.fillCircle(46 + dx, 64, 7, C_BG); cv.fillCircle(114 + dx, 64, 7, C_BG); }
  else if (!happy && lid < 60) { cv.fillCircle(46 + dx + dx / 3, 64, 9, C_BG); cv.fillCircle(114 + dx + dx / 3, 64, 9, C_BG); }
  tft.drawRGBBitmap(0, 0, cv.getBuffer(), 160, 128);
}

// ---------------- SENSORS ----------------
int distanceMM() {
#if USE_VL53L1X
  if (!tof.dataReady()) return -2;
  int d = tof.read(false);
  return tof.timeoutOccurred() ? -1 : d;
#else
  VL53L0X_RangingMeasurementData_t m;
  tof.rangingTest(&m, false);
  return (m.RangeStatus != 4) ? m.RangeMilliMeter : -1;
#endif
}
int lastDist = 9999;
unsigned long lastToF = 0, lastAlarm = 0, lastClap = 0;
bool obstacleLatch = false;

void checkToF() {
  if (millis() - lastToF < 60) return;
  lastToF = millis();
  int d = distanceMM();
  if (d == -2) return;
  if (d > 0 && d < 8000) lastDist = d; else if (d == -1) lastDist = 9999;
  if (lastDist < STOP_MM) {
    if (driveDir == 'F') stopMotors();
    setMood(M_SHOCK, 1200);
    if (millis() - lastAlarm > 2500) { sound(SND_ALARM); lastAlarm = millis(); }
    obstacleLatch = true;
  } else obstacleLatch = false;
}
void checkClap() {                          // peak-to-peak over ~20 ms window
  if (millis() - lastClap < 2000) return;
  int mn = 4095, mx = 0; unsigned long t0 = millis();
  while (millis() - t0 < 20) { int v = analogRead(PIN_MIC); if (v < mn) mn = v; if (v > mx) mx = v; }
  if (mx - mn > CLAP_LEVEL) {
    lastClap = millis(); sound(SND_CLAP);
    setMood(M_CURIOUS, 2500);
    lookTarget = (random(2) ? 1 : -1);
  }
}

bool solarOn = false; unsigned long lastSolarChk = 0;
void checkSolar() {                       // chime + happy eyes when sunlight starts charging
  if (millis() - lastSolarChk < 500) return;
  lastSolarChk = millis();
  bool on = analogRead(PIN_SOLAR) > 2000;  // ~>2.6V at pin = panel lit
  if (on && !solarOn) { sound(SND_CHARGE); setMood(M_HAPPY, 2500); }
  solarOn = on;
}

// ---------------- SIM7600E-H ----------------
String simBuf;
bool smsNext = false;

String simCmd(const char *c, unsigned long to = 1000) {
  SIM.println(c);
  String r; unsigned long t = millis();
  while (millis() - t < to) while (SIM.available()) r += (char)SIM.read();
  Serial.print("> "); Serial.println(c); Serial.println(r);
  return r;
}
void simInit() {
  SIM.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(500);
  for (int i = 0; i < 5 && simCmd("AT", 500).indexOf("OK") < 0; i++) delay(500);
  simCmd("ATE0");
  simCmd("AT+CPIN?");
  simCmd("AT+CREG?");
  simCmd("AT+CLIP=1");              // caller ID
  simCmd("AT+CSDVC=1");             // audio path 1 = analog handset (MIC+/EAR+)
  simCmd("AT+CMGF=1");              // SMS text mode
  simCmd("AT+CNMI=2,2,0,0,0");      // deliver SMS directly as +CMT
}
void handleCommand(String c) {      // from SMS body: F B L R S H (happy)
  c.trim(); c.toUpperCase();
  if (!c.length()) return;
  lastDriveCmd = millis();
  switch (c[0]) {
    case 'F': if (lastDist >= STOP_MM) forward(); break;
    case 'B': backward(); break;
    case 'L': turnLeft(); break;
    case 'R': turnRight(); break;
    case 'S': stopMotors(); break;
    case 'H': setMood(M_HAPPY, 3000); sound(SND_HAPPY); break;
  }
}
void pollSIM() {
  while (SIM.available()) {
    char ch = SIM.read();
    if (ch != '\n') { if (ch != '\r') simBuf += ch; continue; }
    String line = simBuf; simBuf = "";
    if (!line.length()) continue;
    Serial.println("SIM: " + line);
    if (smsNext) { smsNext = false; handleCommand(line); }
    else if (line.startsWith("+CMT:")) smsNext = true;
    else if (line.startsWith("RING")) { SIM.println("ATA"); setMood(M_HAPPY, 4000); sound(SND_HAPPY); }
    else if (line.startsWith("NO CARRIER")) setMood(M_NORMAL, 0);
  }
}

// ---------------- SETUP / LOOP ----------------
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 256; i++) sineTab[i] = 128 + (int)(70 * sin(i * 2 * PI / 256));   // volume = 70
  sndQ = xQueueCreate(6, sizeof(Snd));
  xTaskCreatePinnedToCore(audioTask, "audio", 3072, NULL, 1, NULL, 0);
  dacWrite(PIN_DAC, 128);
  motorsInit(); stopMotors();
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(C_BG);   // try INITR_GREENTAB if colours/offset wrong
  analogReadResolution(12); analogSetPinAttenuation(PIN_MIC, ADC_11db); analogSetPinAttenuation(PIN_SOLAR, ADC_11db);
  Wire.begin(21, 22);
#if USE_VL53L1X
  tof.setTimeout(500);
  if (!tof.init()) Serial.println("VL53L1X FAIL");
  else {
    tof.setDistanceMode(VL53L1X::Short);
    tof.setMeasurementTimingBudget(50000);
    tof.startContinuous(60);
  }
#else
  if (!tof.begin()) Serial.println("VL53L0X FAIL");
#endif
  sound(SND_STARTUP); setMood(M_HAPPY, 2500);
  nextBlink = millis() + 2000;
  simInit();
}
void loop() {
  unsigned long now = millis();
  if (mood != M_NORMAL && now > moodUntil) mood = M_NORMAL;
  if (mood == M_NORMAL && now > nextGlance) {          // idle: look around
    lookTarget = random(-10, 11) / 10.0f; nextGlance = now + random(1500, 4000);
  }
  checkToF();
  checkClap();
  checkSolar();
  pollSIM();
  if (driving && now - lastDriveCmd > CMD_TIMEOUT_MS) stopMotors();   // dead-man timeout
  static unsigned long lastFrame = 0;
  if (now - lastFrame >= 40) { lastFrame = now; drawEyes(); }
}
