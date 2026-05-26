#include <Arduino.h>
#include <M5Unified.h>
#include <math.h>

namespace {

enum class ViewMode {
  Bubble,
  Numbers,
  Target,
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

ViewMode viewMode = ViewMode::Bubble;
Attitude filtered;
Attitude zeroOffset;

float targetAngleDeg = 0.0f;
uint32_t lastDrawMs = 0;
uint32_t lastBeepMs = 0;
uint32_t lastTargetAdjustMs = 0;
uint32_t aDownMs = 0;
uint32_t bDownMs = 0;
bool aLongAction = false;
bool bLongAction = false;
bool comboAction = false;
bool speakerEnabled = true;

float clampf(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}

float maxAbs2(float a, float b) {
  return max(fabsf(a), fabsf(b));
}

String formatSigned(float value, uint8_t decimals = 1) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%+.1f", value);
  return String(buf);
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
  d.fillRect(0, 0, d.width(), 20, TFT_BLACK);
  d.setTextDatum(middle_left);
  d.setTextColor(color, TFT_BLACK);
  d.setTextSize(1);
  d.drawString(title, 6, 10);
  d.setTextDatum(middle_right);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.drawString(speakerEnabled ? "BEEP" : "MUTE", d.width() - 6, 10);
}

void drawBubbleView() {
  auto& d = M5.Display;
  const int w = d.width();
  const int h = d.height();
  const int cx = w / 2;
  const int cy = (h + 16) / 2;
  const int radius = min(w, h - 22) / 2 - 8;
  const float error = maxAbs2(filtered.pitch, filtered.roll);
  const uint16_t color = statusColor(error);

  d.fillScreen(TFT_BLACK);
  drawHeader("LEVEL", color);

  d.drawCircle(cx, cy, radius, TFT_DARKGREY);
  d.drawCircle(cx, cy, radius / 2, TFT_DARKGREY);
  d.drawLine(cx - radius, cy, cx + radius, cy, TFT_DARKGREY);
  d.drawLine(cx, cy - radius, cx, cy + radius, TFT_DARKGREY);
  d.fillCircle(cx, cy, 3, color);

  const int bx = cx + static_cast<int>(clampf(filtered.roll / kBubbleRangeDeg, -1.0f, 1.0f) * (radius - 11));
  const int by = cy + static_cast<int>(clampf(filtered.pitch / kBubbleRangeDeg, -1.0f, 1.0f) * (radius - 11));
  d.fillCircle(bx, by, 9, color);
  d.drawCircle(bx, by, 10, TFT_WHITE);

  d.setTextDatum(bottom_left);
  d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  d.setTextSize(1);
  d.drawString("P " + formatSigned(filtered.pitch) + "  R " + formatSigned(filtered.roll), 6, h - 3);
}

void drawNumbersView() {
  auto& d = M5.Display;
  const float error = maxAbs2(filtered.pitch, filtered.roll);
  const uint16_t color = statusColor(error);

  d.fillScreen(TFT_BLACK);
  drawHeader("ANGLE", color);

  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(3);
  d.drawString("P " + formatSigned(filtered.pitch), d.width() / 2, 48);
  d.drawString("R " + formatSigned(filtered.roll), d.width() / 2, 86);

  d.setTextSize(1);
  d.setTextColor(color, TFT_BLACK);
  d.drawString(error <= kLevelThresholdDeg ? "OK" : "ADJUST", d.width() / 2, 122);
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

  d.fillScreen(TFT_BLACK);
  drawHeader("TARGET", color);

  d.setTextDatum(middle_center);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(2);
  d.drawString("Target " + String(targetAngleDeg, 0) + " deg", d.width() / 2, 38);

  d.setTextSize(2);
  d.setTextColor(color, TFT_BLACK);
  d.drawString(guidanceText(), d.width() / 2, 72);

  d.setTextSize(1);
  d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  d.drawString("Pitch err " + formatSigned(pitchErr), d.width() / 2, 104);
  d.drawString("Roll err  " + formatSigned(rollErr), d.width() / 2, 122);
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
      calibrateCurrentPose();
    }
    aLongAction = false;
    comboAction = M5.BtnB.isPressed();
  }

  if (M5.BtnB.wasReleased()) {
    if (!bLongAction && !comboAction) {
      viewMode = static_cast<ViewMode>((static_cast<int>(viewMode) + 1) % 3);
      if (speakerEnabled) M5.Speaker.tone(660, 45);
    }
    bLongAction = false;
    comboAction = M5.BtnA.isPressed();
  }
}

void updateBeep() {
  if (!speakerEnabled) return;

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
  M5.Display.drawString("StickS3 Level", M5.Display.width() / 2, M5.Display.height() / 2 - 8);
  M5.Display.setTextSize(1);
  M5.Display.drawString("A calibrate  B mode", M5.Display.width() / 2, M5.Display.height() / 2 + 18);

  delay(600);
}

void loop() {
  M5.update();
  handleButtons();
  updateFilter();
  updateBeep();

  const uint32_t now = millis();
  if (now - lastDrawMs >= 50) {
    drawUi();
    lastDrawMs = now;
  }
}
