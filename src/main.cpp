#include <Arduino.h>
#include <Adafruit_VL53L0X.h>
#include <M5Unified.h>
#include <Wire.h>
#include <math.h>

namespace {

enum class ViewMode {
  Bubble,
  Numbers,
  Target,
  Distance,
};

struct Attitude {
  float pitch = 0.0f;
  float roll = 0.0f;
};

constexpr float kAlpha = 0.16f;
constexpr float kLevelThresholdDeg = 1.0f;
constexpr float kNearlyLevelThresholdDeg = 3.0f;
constexpr float kBubbleRangeDeg = 12.0f;
constexpr float kButtonRepeatMs = 160.0f;
constexpr uint32_t kLongPressMs = 600;
constexpr uint32_t kDistanceReadMs = 120;
constexpr uint32_t kTofRetryMs = 2500;
constexpr uint8_t kViewModeCount = 4;
constexpr uint8_t kTofSdaPin = 9;
constexpr uint8_t kTofSclPin = 10;

ViewMode viewMode = ViewMode::Bubble;
Attitude filtered;
Attitude zeroOffset;
Adafruit_VL53L0X tof;

float targetAngleDeg = 0.0f;
uint16_t distanceMm = 0;
uint32_t lastDrawMs = 0;
uint32_t lastBeepMs = 0;
uint32_t lastTargetAdjustMs = 0;
uint32_t lastDistanceReadMs = 0;
uint32_t lastTofRetryMs = 0;
uint32_t aDownMs = 0;
uint32_t bDownMs = 0;
bool aLongAction = false;
bool bLongAction = false;
bool comboAction = false;
bool speakerEnabled = true;
bool tofAvailable = false;
bool distanceValid = false;

float clampf(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}

float maxAbs2(float a, float b) {
  return max(fabsf(a), fabsf(b));
}

String formatSigned(float value, uint8_t decimals = 1) {
  char buf[16];
  snprintf(buf, sizeof(buf), decimals == 0 ? "%+.0f" : "%+.1f", value);
  return String(buf);
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return M5.Display.color565(r, g, b);
}

Attitude readAttitude() {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 1.0f;

  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    return filtered;
  }

  Attitude raw;
  raw.roll = atan2f(ay, az) * 180.0f / PI;
  raw.pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
  raw.pitch -= zeroOffset.pitch;
  raw.roll -= zeroOffset.roll;
  return raw;
}

void updateFilter() {
  const Attitude raw = readAttitude();
  filtered.pitch += (raw.pitch - filtered.pitch) * kAlpha;
  filtered.roll += (raw.roll - filtered.roll) * kAlpha;
}

uint16_t statusColor(float error) {
  if (error <= kLevelThresholdDeg) return TFT_GREEN;
  if (error <= kNearlyLevelThresholdDeg) return TFT_YELLOW;
  return TFT_RED;
}

void drawHeader(const char* title, uint16_t color) {
  auto& d = M5.Display;
  d.fillRect(0, 0, d.width(), 22, rgb(10, 14, 18));
  d.setTextDatum(middle_left);
  d.setTextColor(color, rgb(10, 14, 18));
  d.setTextSize(1);
  d.drawString(title, 6, 11);
  d.setTextDatum(middle_right);
  d.setTextColor(rgb(135, 145, 150), rgb(10, 14, 18));
  d.drawString(speakerEnabled ? "BEEP" : "MUTE", d.width() - 6, 11);
}

void drawPanelBackground(const char* title, uint16_t color) {
  auto& d = M5.Display;
  d.fillScreen(rgb(7, 10, 13));
  for (int y = 24; y < d.height(); y += 16) {
    d.drawFastHLine(0, y, d.width(), rgb(15, 22, 26));
  }
  for (int x = 0; x < d.width(); x += 16) {
    d.drawFastVLine(x, 22, d.height() - 22, rgb(12, 18, 22));
  }
  drawHeader(title, color);
}

void drawMiniGauge(int x, int y, int w, float value, float range, uint16_t color, const char* label) {
  auto& d = M5.Display;
  const int h = 9;
  d.setTextDatum(middle_left);
  d.setTextColor(rgb(160, 170, 172), rgb(7, 10, 13));
  d.setTextSize(1);
  d.drawString(label, x, y + 4);
  const int gx = x + 16;
  const int gw = w - 16;
  d.drawRoundRect(gx, y, gw, h, 4, rgb(54, 64, 67));
  const int center = gx + gw / 2;
  d.drawFastVLine(center, y - 1, h + 2, rgb(105, 118, 120));
  const int marker = center + static_cast<int>(clampf(value / range, -1.0f, 1.0f) * (gw / 2 - 3));
  d.fillCircle(marker, y + h / 2, 4, color);
}

void drawBubbleView() {
  auto& d = M5.Display;
  const int w = d.width();
  const int h = d.height();
  const int cx = w / 2;
  const int cy = (h + 14) / 2;
  const int radius = min(w, h - 30) / 2 - 8;
  const float error = maxAbs2(filtered.pitch, filtered.roll);
  const uint16_t color = statusColor(error);

  drawPanelBackground("LEVEL", color);

  d.fillCircle(cx, cy, radius + 4, rgb(14, 21, 25));
  d.drawCircle(cx, cy, radius + 4, rgb(39, 52, 56));
  d.drawCircle(cx, cy, radius, rgb(75, 90, 94));
  d.drawCircle(cx, cy, radius / 2, rgb(48, 61, 65));
  d.drawLine(cx - radius, cy, cx + radius, cy, rgb(64, 78, 82));
  d.drawLine(cx, cy - radius, cx, cy + radius, rgb(64, 78, 82));
  d.fillCircle(cx, cy, 3, color);

  const int bx = cx + static_cast<int>(clampf(filtered.roll / kBubbleRangeDeg, -1.0f, 1.0f) * (radius - 11));
  const int by = cy + static_cast<int>(clampf(filtered.pitch / kBubbleRangeDeg, -1.0f, 1.0f) * (radius - 11));
  d.fillCircle(bx + 2, by + 2, 11, rgb(0, 0, 0));
  d.fillCircle(bx, by, 9, color);
  d.drawCircle(bx, by, 10, TFT_WHITE);

  drawMiniGauge(6, h - 21, w / 2 - 8, filtered.pitch, kBubbleRangeDeg, color, "P");
  drawMiniGauge(w / 2 + 2, h - 21, w / 2 - 8, filtered.roll, kBubbleRangeDeg, color, "R");
}

void drawNumbersView() {
  auto& d = M5.Display;
  const float error = maxAbs2(filtered.pitch, filtered.roll);
  const uint16_t color = statusColor(error);

  drawPanelBackground("ANGLE", color);

  d.setTextDatum(middle_center);
  d.fillRoundRect(8, 30, d.width() - 16, 35, 6, rgb(17, 27, 31));
  d.fillRoundRect(8, 70, d.width() - 16, 35, 6, rgb(17, 27, 31));
  d.setTextSize(2);
  d.setTextColor(TFT_WHITE, rgb(17, 27, 31));
  d.drawString("P " + formatSigned(filtered.pitch), d.width() / 2, 47);
  d.drawString("R " + formatSigned(filtered.roll), d.width() / 2, 87);

  d.setTextSize(1);
  d.setTextColor(color, rgb(7, 10, 13));
  d.drawString(error <= kLevelThresholdDeg ? "LEVEL OK" : "ADJUST", d.width() / 2, 119);
}

String guidanceText() {
  const float pitchErr = filtered.pitch - targetAngleDeg;
  const float rollErr = filtered.roll;

  if (maxAbs2(pitchErr, rollErr) <= kLevelThresholdDeg) return "ON TARGET";

  if (fabsf(rollErr) > fabsf(pitchErr)) {
    return rollErr > 0 ? "RIGHT HIGH" : "LEFT HIGH";
  }
  return pitchErr > 0 ? "FRONT HIGH" : "BACK HIGH";
}

void drawTargetView() {
  auto& d = M5.Display;
  const float pitchErr = filtered.pitch - targetAngleDeg;
  const float rollErr = filtered.roll;
  const float error = maxAbs2(pitchErr, rollErr);
  const uint16_t color = statusColor(error);

  drawPanelBackground("TARGET", color);

  d.setTextDatum(middle_center);
  d.fillRoundRect(8, 29, d.width() - 16, 25, 5, rgb(17, 27, 31));
  d.setTextColor(TFT_WHITE, rgb(17, 27, 31));
  d.setTextSize(2);
  d.drawString("T " + String(targetAngleDeg, 0) + " deg", d.width() / 2, 41);

  d.setTextSize(2);
  d.setTextColor(color, rgb(7, 10, 13));
  d.drawString(guidanceText(), d.width() / 2, 76);

  d.setTextSize(1);
  d.setTextColor(rgb(190, 205, 205), rgb(7, 10, 13));
  d.drawString("P err " + formatSigned(pitchErr), d.width() / 2, 103);
  d.drawString("R err " + formatSigned(rollErr), d.width() / 2, 119);
}

bool initTof() {
  Wire.begin(kTofSdaPin, kTofSclPin);
  Wire.setClock(400000);
  tofAvailable = tof.begin(0x29, false, &Wire);
  distanceValid = false;
  lastTofRetryMs = millis();
  return tofAvailable;
}

void updateDistance() {
  const uint32_t now = millis();
  if (!tofAvailable) {
    if (now - lastTofRetryMs > kTofRetryMs) {
      initTof();
    }
    return;
  }

  if (now - lastDistanceReadMs < kDistanceReadMs) return;
  lastDistanceReadMs = now;

  VL53L0X_RangingMeasurementData_t measure;
  tof.rangingTest(&measure, false);
  distanceValid = measure.RangeStatus != 4;
  if (distanceValid) {
    distanceMm = measure.RangeMilliMeter;
  }
}

void drawDistanceView() {
  auto& d = M5.Display;
  const uint16_t color = distanceValid ? TFT_CYAN : TFT_ORANGE;
  drawPanelBackground("DISTANCE", color);

  d.setTextDatum(middle_center);
  if (!tofAvailable) {
    d.setTextColor(TFT_ORANGE, rgb(7, 10, 13));
    d.setTextSize(2);
    d.drawString("NO TOF", d.width() / 2, 56);
    d.setTextSize(1);
    d.setTextColor(rgb(190, 205, 205), rgb(7, 10, 13));
    d.drawString("Grove: SDA G9", d.width() / 2, 88);
    d.drawString("SCL G10", d.width() / 2, 106);
    d.drawString("A retry  B mode", d.width() / 2, 123);
    return;
  }

  if (!distanceValid) {
    d.setTextColor(TFT_YELLOW, rgb(7, 10, 13));
    d.setTextSize(2);
    d.drawString("OUT RANGE", d.width() / 2, 62);
    d.setTextSize(1);
    d.setTextColor(rgb(190, 205, 205), rgb(7, 10, 13));
    d.drawString("Move target closer", d.width() / 2, 102);
    return;
  }

  const float distanceCm = distanceMm / 10.0f;
  const float distanceM = distanceMm / 1000.0f;
  d.fillRoundRect(8, 33, d.width() - 16, 58, 8, rgb(17, 27, 31));
  d.setTextColor(TFT_WHITE, rgb(17, 27, 31));
  d.setTextSize(3);
  d.drawString(String(distanceCm, 1), d.width() / 2, 58);
  d.setTextSize(1);
  d.setTextColor(TFT_CYAN, rgb(17, 27, 31));
  d.drawString("cm", d.width() - 29, 77);

  d.setTextColor(rgb(190, 205, 205), rgb(7, 10, 13));
  d.drawString(String(distanceMm) + " mm   " + String(distanceM, 3) + " m", d.width() / 2, 110);

  const int barX = 18;
  const int barY = 121;
  const int barW = d.width() - 36;
  d.drawRoundRect(barX, barY, barW, 5, 2, rgb(54, 64, 67));
  d.fillRoundRect(barX, barY, static_cast<int>(clampf(distanceMm / 1200.0f, 0.0f, 1.0f) * barW), 5, 2, TFT_CYAN);
}

void drawUi() {
  switch (viewMode) {
    case ViewMode::Bubble:
      drawBubbleView();
      break;
    case ViewMode::Numbers:
      drawNumbersView();
      break;
    case ViewMode::Target:
      drawTargetView();
      break;
    case ViewMode::Distance:
      drawDistanceView();
      break;
  }
}

void calibrateCurrentPose() {
  zeroOffset.pitch += filtered.pitch;
  zeroOffset.roll += filtered.roll;
  filtered = {};

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("CALIBRATED", M5.Display.width() / 2, M5.Display.height() / 2);
  if (speakerEnabled) M5.Speaker.tone(880, 90);
  delay(260);
}

void handleButtons() {
  if (M5.BtnA.wasPressed()) {
    aDownMs = millis();
    aLongAction = false;
  }

  if (M5.BtnB.wasPressed()) {
    bDownMs = millis();
    bLongAction = false;
  }

  if (M5.BtnA.pressedFor(700) && M5.BtnB.pressedFor(700) && !comboAction) {
    speakerEnabled = !speakerEnabled;
    M5.Speaker.stop();
    comboAction = true;
    aLongAction = true;
    bLongAction = true;
    delay(250);
  }

  const uint32_t now = millis();
  if (viewMode == ViewMode::Target && now - lastTargetAdjustMs > kButtonRepeatMs) {
    if (M5.BtnA.isPressed() && now - aDownMs > kLongPressMs) {
      targetAngleDeg = clampf(targetAngleDeg - 1.0f, -45.0f, 45.0f);
      aLongAction = true;
      lastTargetAdjustMs = now;
    }
    if (M5.BtnB.isPressed() && now - bDownMs > kLongPressMs) {
      targetAngleDeg = clampf(targetAngleDeg + 1.0f, -45.0f, 45.0f);
      bLongAction = true;
      lastTargetAdjustMs = now;
    }
  }

  if (M5.BtnA.wasReleased()) {
    if (!aLongAction && !comboAction) {
      if (viewMode == ViewMode::Distance) {
        initTof();
        if (speakerEnabled) M5.Speaker.tone(tofAvailable ? 880 : 330, 80);
      } else {
        calibrateCurrentPose();
      }
    }
    aLongAction = false;
    comboAction = M5.BtnB.isPressed();
  }

  if (M5.BtnB.wasReleased()) {
    if (!bLongAction && !comboAction) {
      viewMode = static_cast<ViewMode>((static_cast<int>(viewMode) + 1) % kViewModeCount);
      if (speakerEnabled) M5.Speaker.tone(660, 45);
    }
    bLongAction = false;
    comboAction = M5.BtnA.isPressed();
  }
}

void updateBeep() {
  if (!speakerEnabled || viewMode == ViewMode::Distance) return;

  const float error = (viewMode == ViewMode::Target)
                          ? maxAbs2(filtered.pitch - targetAngleDeg, filtered.roll)
                          : maxAbs2(filtered.pitch, filtered.roll);
  const uint32_t now = millis();

  if (error <= kLevelThresholdDeg) {
    if (now - lastBeepMs > 900) {
      M5.Speaker.tone(1320, 70);
      lastBeepMs = now;
    }
    return;
  }

  if (error <= 8.0f) {
    const uint32_t interval = static_cast<uint32_t>(clampf(error * 120.0f, 180.0f, 900.0f));
    if (now - lastBeepMs > interval) {
      M5.Speaker.tone(740, 35);
      lastBeepMs = now;
    }
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.output_power = true;
  cfg.internal_imu = true;
  cfg.internal_spk = true;
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setBrightness(150);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Smart Level+", M5.Display.width() / 2, M5.Display.height() / 2 - 8);
  M5.Display.setTextSize(1);
  M5.Display.drawString("A calibrate  B mode", M5.Display.width() / 2, M5.Display.height() / 2 + 18);
  initTof();

  delay(600);
}

void loop() {
  M5.update();
  handleButtons();
  updateFilter();
  updateDistance();
  updateBeep();

  const uint32_t now = millis();
  if (now - lastDrawMs >= 50) {
    drawUi();
    lastDrawMs = now;
  }
}
