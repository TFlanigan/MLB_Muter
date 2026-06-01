#pragma once

// Board-specific GPIO map. The active board is selected by a -D flag in
// platformio.ini:
//   (default, no flag)   AI-Thinker ESP32-CAM  (plain ESP32 / Xtensa LX6)
//   -DBOARD_FREENOVE_S3  Freenove ESP32-S3-WROOM CAM (Xtensa LX7, has SIMD)
//
// IR_SEND_PIN is here too because the AI-Thinker and S3 boards have different
// free pins (GPIO 4 is the camera SDA line on the Freenove, so IR moves to 21).

#if defined(BOARD_FREENOVE_S3)
// ---- Freenove ESP32-S3 CAM Board (FNK0085, OV3660) ----
// Pinout matches Freenove's documentation for the FNK0085.
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM    13
#define IR_SEND_PIN      21
// OV3660 doesn't reliably PLL-lock at 20 MHz on this board — Freenove's own
// example uses 10 MHz. SCCB probe (ESP_ERR_NOT_FOUND on init) fails at 20 MHz.
#define XCLK_FREQ_HZ 10000000

#else
// ---- AI-Thinker ESP32-CAM (OV2640) ----
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22
#define IR_SEND_PIN       4
#define XCLK_FREQ_HZ 20000000

#endif
