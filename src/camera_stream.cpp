/**
 * ESP32-CAM live MJPEG stream for alignment checking.
 *
 * Connects to WiFi, starts an HTTP server, and serves a live camera feed at
 *   http://<esp32-ip>/
 * Open that URL in iPhone Safari to see what the camera sees. The green box
 * overlay marks the 240x240 center-crop region that main.cpp's classifier
 * actually consumes — keep the TV inside the box.
 *
 * Build:  pio run -e camera_stream --target upload --target monitor
 * The serial monitor will print the IP address once WiFi connects.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "camera_pins.h"

// --- Credentials live in secrets.h (git-ignored). Copy secrets.example.h to
//     secrets.h and fill in your WiFi credentials. ---
#include "secrets.h"

static WebServer server(80);

// Landing page: shows the live stream with a green overlay marking the inference
// region. The crop guide is 75% wide × 100% tall, centered horizontally, because
// main.cpp center-crops the QVGA 320x240 frame to 240x240 before feeding the model.
static const char *INDEX_HTML = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>MLB Muter Camera</title>
<style>
  html, body { margin: 0; padding: 0; background: #111; color: #ccc; font-family: -apple-system, sans-serif; }
  .wrap { display: flex; flex-direction: column; align-items: center; min-height: 100vh; padding-top: 12px; }
  .stage { position: relative; line-height: 0; }
  img { display: block; width: 100vw; max-width: 640px; height: auto; }
  .crop {
    position: absolute; top: 0; left: 12.5%; width: 75%; height: 100%;
    border: 2px solid #0f0; box-sizing: border-box; pointer-events: none;
  }
  .label { padding: 10px; font-size: 13px; text-align: center; line-height: 1.4; }
</style>
</head>
<body>
<div class="wrap">
  <div class="stage">
    <img src="/stream">
    <div class="crop"></div>
  </div>
  <div class="label">Green box = inference region.<br>Align the TV so it fills the green box.</div>
</div>
</body>
</html>
)HTML";

static bool setupCamera() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.pixel_format = PIXFORMAT_JPEG;
    // Match main.cpp's inference resolution so "what you see is what the model sees".
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }
    return true;
}

static void handleIndex() {
    server.send(200, "text/html", INDEX_HTML);
}

static void handleStream() {
    WiFiClient client = server.client();
    if (!client) return;

    // Multipart MJPEG response — natively supported by iOS Safari via <img src>.
    client.print("HTTP/1.1 200 OK\r\n"
                 "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                 "Cache-Control: no-cache\r\n"
                 "Pragma: no-cache\r\n"
                 "Connection: close\r\n\r\n");

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            delay(50);
            continue;
        }

        char part_header[96];
        int hlen = snprintf(part_header, sizeof(part_header),
                            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                            (unsigned)fb->len);
        client.write((const uint8_t *)part_header, hlen);
        client.write(fb->buf, fb->len);
        client.write((const uint8_t *)"\r\n", 2);

        esp_camera_fb_return(fb);
        delay(50);  // ~20 fps cap — plenty for alignment
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    delay(1000);
    Serial.println("\nESP32-CAM Live Stream");
    Serial.printf("PSRAM size: %u bytes, free: %u bytes\n",
                  (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
    Serial.printf("Heap free: %u bytes\n", (unsigned)ESP.getFreeHeap());

    if (!setupCamera()) {
        Serial.println("Halting (camera init failed)");
        while (true) delay(1000);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("\n=== Open this URL in iPhone Safari ===\n  http://");
    Serial.print(WiFi.localIP());
    Serial.println("\n=====================================\n");

    server.on("/", handleIndex);
    server.on("/stream", handleStream);
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
    delay(1);
}
