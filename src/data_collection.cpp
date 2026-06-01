/**
 * ESP32-CAM Data Collection Sketch for Edge Impulse
 *
 * This sketch captures images from the camera and uploads them to your
 * Edge Impulse project with the correct labels.
 *
 * Instructions:
 * 1. Fill in your WiFi and Edge Impulse credentials below.
 * 2. Build and upload using the 'data_collection' environment in PlatformIO:
 *    `pio run -e data_collection --target upload`
 * 3. Open the Serial Monitor at 115200 baud.
 * 4. Point the camera at your TV and send commands:
 *    - 'c' to capture and label an image as 'commercial'.
 *    - 'p' to capture and label an image as 'program'.
 *    - 't' to test the connection to Edge Impulse.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "camera_pins.h"

// --- Credentials live in secrets.h (git-ignored). Copy secrets.example.h to
//     secrets.h and fill in your WiFi + Edge Impulse API key. ---
#include "secrets.h"

// --- IR Settings ---
// IR_SEND_PIN and the camera GPIO map come from camera_pins.h (board-specific).
// Marantz typically uses the RC5 protocol (12 bits).
// Address 16 (0x10) + Command 13 (0x0D) = 0x40D
#define MARANTZ_MUTE_TOGGLE 0x40D
// Placeholders for discrete Mute On/Off if your receiver supports them.
#define MARANTZ_MUTE_ON     0x40D
#define MARANTZ_MUTE_OFF    0x40D

IRsend irsend(IR_SEND_PIN);

// -----------------------------------------------------
// DISCRETE MUTE ON (Pronto Hex format)
// -----------------------------------------------------
uint16_t muteOnPronto[] = {
  0x0000, 0x0071, 0x0000, 0x0022, 0x0020, 0x0020, 0x0020, 0x0020, 
  0x0020, 0x0020, 0x0040, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 
  0x0020, 0x00A1, 0x0020, 0x0020, 0x0020, 0x0040, 0x0020, 0x0020, 
  0x0040, 0x0040, 0x0040, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 
  0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0AAB, 0x0020, 0x0020, 
  0x0020, 0x0020, 0x0020, 0x0020, 0x0040, 0x0020, 0x0020, 0x0020, 
  0x0020, 0x0020, 0x0020, 0x00A1, 0x0020, 0x0020, 0x0020, 0x0040, 
  0x0020, 0x0020, 0x0040, 0x0040, 0x0040, 0x0020, 0x0020, 0x0020,
  0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0AAB
};

// -----------------------------------------------------
// DISCRETE MUTE OFF / UNMUTE (Pronto Hex format)
// -----------------------------------------------------
uint16_t muteOffPronto[] = {
  0x0000, 0x0071, 0x0000, 0x0020, 0x0020, 0x0020, 0x0040, 0x0040, 
  0x0040, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x00A1, 
  0x0020, 0x0020, 0x0020, 0x0040, 0x0020, 0x0020, 0x0040, 0x0040, 
  0x0040, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 
  0x0020, 0x0040, 0x0020, 0x0AAB, 0x0020, 0x0020, 0x0040, 0x0040, 
  0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x00A1, 0x0020, 
  0x0020, 0x0020, 0x0040, 0x0020, 0x0020, 0x0040, 0x0040, 0x0040, 
  0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
  0x0020, 0x0040, 0x0020, 0x0AAB
};

void setup_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.pixel_format = PIXFORMAT_JPEG;
    // Use a smaller frame size for faster uploads. Edge Impulse will resize to 96x96.
    config.frame_size = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 12; // 0-63, lower is higher quality
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        // Blink onboard LED to indicate a fatal error
        pinMode(4, OUTPUT);
        while(true) {
            digitalWrite(4, !digitalRead(4));
            delay(100);
        }
    }
    Serial.println("Camera initialized.");
}

void connect_wifi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    WiFi.setSleep(false); // Disable WiFi sleep to prevent connection drops during large uploads
}

// Helper function to encode image to base64 so you can view it in your browser
String base64Encode(const uint8_t *data, size_t len) {
    static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String encoded;
    encoded.reserve(((len + 2) / 3) * 4);
    int val = 0, valb = -6;
    for (size_t i = 0; i < len; i++) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            encoded += base64_chars[(val >> valb) & 0x3F];
            valb -= 6;
        }
    }
    if (valb > -6) encoded += base64_chars[((val << 8) >> (valb + 8)) & 0x3F];
    while (encoded.length() % 4) encoded += '=';
    return encoded;
}

/**
 * @brief Uploads a captured image to Edge Impulse with a specific label.
 *
 * This function sends the image data via HTTP POST to the Edge Impulse ingestion API.
 * It uses custom headers to provide the API key, label, and filename.
 *
 * @param fb Pointer to the camera frame buffer.
 * @param label The label to assign to the image (e.g., "commercial", "program").
 */
void uploadToEdgeImpulse(camera_fb_t *fb, const char* label) {
    if (!fb) {
        Serial.println("Camera capture failed, not uploading.");
        return;
    }

    Serial.printf("Uploading image with label '%s'...\n", label);

    // Print the image as a Base64 Data URI so it can be viewed in a browser
    Serial.println("\n--- COPY THE TEXT BELOW AND PASTE INTO YOUR WEB BROWSER URL BAR ---");
    Serial.print("data:image/jpeg;base64,");
    Serial.println(base64Encode(fb->buf, fb->len));
    Serial.println("-----------------------------------------------------------------\n");

    // Give the Wi-Fi stack a moment to handle background events before heavy SSL traffic
    delay(100);

    // Create fresh instances to guarantee a completely clean SSL state
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    // Use the /api/training/files endpoint and pass the label in the query string.
    String url = "https://ingestion.edgeimpulse.com/api/training/files?label=" + String(label);
    if (!http.begin(client, url)) {
        Serial.println("Failed to begin HTTP client!");
        return;
    }
    
    // Set a longer timeout to allow for image upload over WiFi
    http.setTimeout(20000); // 20 seconds
    http.setReuse(false);   // Prevent HTTPClient from reusing stale connections

    // Set headers required by Edge Impulse
    http.addHeader("x-api-key", EI_API_KEY);

    // Construct the multipart/form-data boundary and headers
    String boundary = "----ESP32Boundary" + String(millis());
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    // Build the complete multipart body
    String headerPart = "--" + boundary + "\r\n";
    headerPart += "Content-Disposition: form-data; name=\"data\"; filename=\"" + String(label) + ".jpg\"\r\n";
    headerPart += "Content-Type: image/jpeg\r\n\r\n";
    String footerPart = "\r\n--" + boundary + "--\r\n";

    // Allocate a buffer for the entire payload. 
    // Use PSRAM to prevent starving the internal SRAM needed by the Wi-Fi and SSL (mbedTLS) stack!
    size_t totalLen = headerPart.length() + fb->len + footerPart.length();
    uint8_t* multipartBuf = (uint8_t*)ps_malloc(totalLen);
    if (!multipartBuf) {
        multipartBuf = (uint8_t*)malloc(totalLen); // Fallback to internal RAM if PSRAM is busy
    }
    if (!multipartBuf) {
        Serial.println("Failed to allocate memory for payload!");
        http.end();
        return;
    }

    // Copy the header, image data, and footer into the payload buffer
    memcpy(multipartBuf, headerPart.c_str(), headerPart.length());
    memcpy(multipartBuf + headerPart.length(), fb->buf, fb->len);
    memcpy(multipartBuf + headerPart.length() + fb->len, footerPart.c_str(), footerPart.length());

    // Send the request
    int httpResponseCode = http.POST(multipartBuf, totalLen);

    // Free the payload buffer immediately after sending
    free(multipartBuf);

    if (httpResponseCode > 0) {
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        String payload = http.getString();
        Serial.println("Server response: " + payload);
    } else {
        Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
}

void testConnection() {
    // Create fresh instances to guarantee a completely clean SSL state
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://ingestion.edgeimpulse.com/api/test";
    if (!http.begin(client, url)) {
        Serial.println("Failed to begin HTTP client for test!");
        return;
    }
    http.setTimeout(10000); // 10-second timeout for the test connection
    http.setReuse(false);
    http.addHeader("x-api-key", EI_API_KEY);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
        Serial.printf("Test Connection - HTTP Response code: %d\n", httpResponseCode);
        String payload = http.getString();
        Serial.println("Server response: " + payload);
    } else {
        Serial.printf("Test Connection failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    delay(2000); // A small delay to allow the serial monitor to connect.
    Serial.println("\nESP32-CAM Data Collection for Edge Impulse");

    irsend.begin();

    setup_camera();
    connect_wifi();

    Serial.println("\nReady to capture images.");
    Serial.println("Commands: 'c'=commercial, 'p'=program, 't'=test connection");
    Serial.println("IR Mute:  'q'=toggle, 'o'=on, 'f'=off");
}

void loop() {
    if (Serial.available() > 0) {
        char command = Serial.read();
        camera_fb_t *fb = NULL;

        switch (command) {
            case 'c':
            case 'p':
                fb = esp_camera_fb_get();
                uploadToEdgeImpulse(fb, (command == 'c' ? "commercial" : "program"));
                if (fb) {
                    esp_camera_fb_return(fb); // IMPORTANT: Return the frame buffer to be reused.
                }
                break;
            case 't':
                testConnection();
                break;
            case 'q':
                Serial.println("Testing IR: Mute Toggle");
                irsend.sendRC5(MARANTZ_MUTE_TOGGLE, 12);
                break;
            case 'o':
                Serial.println("Testing IR: Mute On");
                // Send the Pronto array with exactly 72 elements and 2 repeats
                irsend.sendPronto(muteOnPronto, sizeof(muteOnPronto) / sizeof(muteOnPronto[0]), 2);
                break;
            case 'f':
                Serial.println("Testing IR: Mute Off");
                // Send the Pronto array with exactly 68 elements and 2 repeats
                irsend.sendPronto(muteOffPronto, sizeof(muteOffPronto) / sizeof(muteOffPronto[0]), 2);
                break;
            default:
                // Ignore other characters like newline/carriage return
                break;
        }
    }
}