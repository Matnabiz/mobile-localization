#include <ESP8266WiFi.h>
#include <math.h>

// Calibration (from your measurements)
const float D0    = 1.0;     // reference distance (m)
const float RSSI0 = -36.0;   // RSSI @ 1 m
const float ALPHA = 3.5;     // path‑loss exponent

// EMA parameter (higher → more responsive, lower → smoother)
const float EMA_ALPHA = 0.3;

// 1D Kalman parameters (tune these)
const float KALMAN_R = 4.0;    // measurement noise covariance
const float KALMAN_Q = 1;    // process noise covariance

// ——— Kalman state ———
float kalman_x = 0;
float kalman_cov = -1;

// ——— EMA state ———
bool   ema_initialized = false;
float  ema_rssi        = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("Central Node: EMA+Kalman starting");
}

void loop() {
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rawRssi = WiFi.RSSI(i);
    if (!ssid.startsWith("BEACON")) continue;

    // 1) Exponential Moving Average
    if (!ema_initialized) {
      ema_rssi = rawRssi;
      ema_initialized = true;
    } else {
      ema_rssi = EMA_ALPHA * rawRssi + (1 - EMA_ALPHA) * ema_rssi;
    }

    // 2) Kalman filter update
    float meas = ema_rssi;
    if (kalman_cov < 0) {
      // first measurement
      kalman_x   = meas;
      kalman_cov = KALMAN_R;
    } else {
      // Predict
      float pred_x   = kalman_x;
      float pred_cov = kalman_cov + KALMAN_Q;

      // Update
      float K = pred_cov / (pred_cov + KALMAN_R);
      kalman_x   = pred_x + K * (meas - pred_x);
      kalman_cov = (1 - K) * pred_cov;
    }

    // 3) Compute distance
    float exponent = (RSSI0 - kalman_x) / (10.0 * ALPHA);
    float dist = D0 * pow(10.0, exponent);

    // 4) Print
    Serial.printf("%s | raw=%d  ema=%.1f  kal=%.1f  dist=%.2f\n",
    ssid.c_str(), rawRssi, ema_rssi, kalman_x, dist);
  }

  // ~30 Hz
  delay(5);
}