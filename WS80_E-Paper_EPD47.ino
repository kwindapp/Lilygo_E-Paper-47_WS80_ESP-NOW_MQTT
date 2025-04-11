#include <WiFi.h>
#include <esp_now.h>
#include <Arduino.h>
#include "epd_driver.h"
#include "logo.h"
#include "logo1.h"
#include "logo2.h"
#include "logo3.h"
#include "logo4.h"
#include "logo5.h"
#include "hum.h"
#include "titel.h"
#include "bat.h"
#include "qr.h"
#include "opensans10b.h"
#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include <Wire.h>
#include <SPI.h>





#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> Tools -> PSRAM -> OPI !!!"
#endif
String localMac;
// === DISPLAY ===
uint8_t *framebuffer;

// === STRUCT TO RECEIVE VIA ESP-NOW ===
struct struct_message {
  int windDir;
  float windSpeed;
  float windGust;
  float temperature;
  float humidity;
  float BatVoltage;
};
struct_message receivedData;



// === UI Positions ===
int cursor_x = 0;
int cursor_y = 0;
int custom_y = 80;
// === GET CARDINAL DIRECTION ===
String getCardinalDirection(int windDir) {
  if (windDir >= 0 && windDir < 22.5) return "N";
  if (windDir >= 22.5 && windDir < 67.5) return "NE";
  if (windDir >= 67.5 && windDir < 112.5) return "E";
  if (windDir >= 112.5 && windDir < 157.5) return "SE";
  if (windDir >= 157.5 && windDir < 202.5) return "S";
  if (windDir >= 202.5 && windDir < 247.5) return "SW";
  if (windDir >= 247.5 && windDir < 292.5) return "W";
  if (windDir >= 292.5 && windDir < 337.5) return "NW";
  return "N";
}


// === SETUP ===
void setup() {
  Serial.begin(115200);

  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
    Serial.println("❌ Framebuffer allocation failed!");
    while (true)
      ;
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);  // clear

  epd_poweron();
  epd_clear();

  WiFi.mode(WIFI_STA);
  localMac = WiFi.macAddress();
  Serial.print("📡 MAC: ");
  Serial.println(localMac);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  drawLayout();
  delay(8000);  // layout boxes and labels once
                // initial blank/empty data
}


void drawLayout() {
  // Initial display text, labels, and empty areas

  cursor_x = 120;
  cursor_y = 60;
  //writeln((GFXfont *)&OpenSans24B, "....KWind WS80 ON E-PAPER....", &cursor_x, &cursor_y, NULL);
  // Clear the framebuffer
  memset(framebuffer, 0, sizeof(framebuffer));

  // Draw a horizontal "fat" line at a fixed y-coordinate
  int fixed_y = 80;         // Fixed y-coordinate for the horizontal line
  int line_thickness = 10;  // The thickness of the line

  // Draw multiple lines to create the "fat" line
  for (int i = 0; i < line_thickness; i++) {
    epd_draw_hline(0, fixed_y + i, EPD_WIDTH - 0, 0, framebuffer);  // Draw a line at y + offset
    epd_draw_hline(0, 480 + i, EPD_WIDTH - 0, 0, framebuffer);      // Draw a line at y + offset
  }

  // Display the updated framebuffer
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);

  cursor_x = 20;
  cursor_y = 140;
  writeln((GFXfont *)&OpenSans24B, "Wind          :", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 200;
  writeln((GFXfont *)&OpenSans24B, "Gust           :", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 260;
  writeln((GFXfont *)&OpenSans24B, "Dir              :", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 320;
  writeln((GFXfont *)&OpenSans24B, "Temp         :", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 380;
  writeln((GFXfont *)&OpenSans24B, "Hum          :", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 440;
  writeln((GFXfont *)&OpenSans24B, "Bat             :", &cursor_x, &cursor_y, NULL);

  epd_draw_rect(10, (10, EPD_HEIGHT), (10, 60), (10, 120), 0, framebuffer);
}

void refreshData() {

  // Area 1: Update Wind Speed
  Rect_t area1 = { 440, 20 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area1);  // Clear previous data in the Wind Speed area
  char windSpeed[16];
  snprintf(windSpeed, sizeof(windSpeed), "%.1f    knt", receivedData.windSpeed);
  cursor_x = 450;            // Starting X position for values
  cursor_y = 60 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, windSpeed, &cursor_x, &cursor_y, NULL);


  // Area 2: Update Gust Speed
  Rect_t area2 = { 440, 80 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area2);  // Clear previous data in the Gust Speed area
  char gustSpeed[16];
  snprintf(gustSpeed, sizeof(gustSpeed), "%.1f    knt", receivedData.windGust);
  cursor_x = 450;             // Starting X position for values
  cursor_y = 120 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, gustSpeed, &cursor_x, &cursor_y, NULL);


  // Area 3: Update Wind Direction
  Rect_t area3 = { 440, 140 + custom_y, .width = 260, .height = 50 };
  epd_clear_area(area3);  // Clear previous data in the Wind Direction area
  char windDirection[16];
  snprintf(windDirection, sizeof(windDirection), "%d°  (%s)", receivedData.windDir, getCardinalDirection(receivedData.windDir).c_str());
  cursor_x = 450;             // Starting X position for values
  cursor_y = 180 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, windDirection, &cursor_x, &cursor_y, NULL);


  //Area 4: Update Temperature
  Rect_t area4 = { 440, 200 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area4);  // Clear previous data in the Temperature area
  char temperature[16];
  snprintf(temperature, sizeof(temperature), "%.1f   °C", receivedData.temperature);
  cursor_x = 450;             // Starting X position for values
  cursor_y = 240 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, temperature, &cursor_x, &cursor_y, NULL);


  // Area 5: Humidity
  Rect_t area5 = { 440, 260 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area5);  // Clear previous data in the Weather Status area
  char humidity[16];
  snprintf(humidity, sizeof(humidity), "%.0f       %%", receivedData.humidity);
  cursor_x = 450;             // Starting X position for values
  cursor_y = 300 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, humidity, &cursor_x, &cursor_y, NULL);

  // Area 5: Batt
  Rect_t area6 = { 440, 320 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area6);  // Clear previous data in the Weather Status area
  char BatVoltage[16];
  snprintf(BatVoltage, sizeof(BatVoltage), "%.2f    V", receivedData.BatVoltage);
  cursor_x = 450;             // Starting X position for values
  cursor_y = 360 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, BatVoltage, &cursor_x, &cursor_y, NULL);
  delay(1000);
 ///*
 Rect_t areat = {
  .x = 130,
  .y = 0,  //titel
.width = titel_width,
  .height = titel_height,
};

epd_draw_grayscale_image(areat, (uint8_t *)titel_data);
epd_draw_image(areat, (uint8_t *)titel_data, BLACK_ON_WHITE);


  Rect_t area = {
        .x = 700,
        .y = 80,  //Kwind logo
      .width = logo1_width,
        .height = logo1_height,
   };

   epd_draw_grayscale_image(area, (uint8_t *)logo1_data);
  epd_draw_image(area, (uint8_t *)logo1_data, BLACK_ON_WHITE);



  Rect_t areaqr = {
    .x = 700,
    .y = 160,
  .width = qr_width,
    .height = qr_height,
};

epd_draw_grayscale_image(areaqr, (uint8_t *)qr_data);
epd_draw_image(areaqr, (uint8_t *)qr_data, BLACK_ON_WHITE);


  Rect_t areap2 = {
    .x = 760,
    .y = 380,  //ecowitt logo
  .width = logo2_width,
    .height = logo2_height,
};

epd_draw_grayscale_image(areap2, (uint8_t *)logo2_data);
epd_draw_image(areap2, (uint8_t *)logo2_data, BLACK_ON_WHITE);

Rect_t areap3 = { //direction
  .x = 310,
  .y = 190,
.width = logo3_width,
  .height = logo3_height,
};

epd_draw_grayscale_image(areap3, (uint8_t *)logo3_data);
epd_draw_image(areap3, (uint8_t *)logo3_data, BLACK_ON_WHITE);

Rect_t areap4 = { //wind
  .x = 315,
  .y = 110,
.width = logo4_width,
  .height = logo4_height,
};

epd_draw_grayscale_image(areap4, (uint8_t *)logo4_data);
epd_draw_image(areap4, (uint8_t *)logo4_data, BLACK_ON_WHITE);




Rect_t areap5 = {
  .x = 330,
  .y = 290,
.width = logo5_width,
  .height = logo5_height,
};

epd_draw_grayscale_image(areap5, (uint8_t *)logo5_data);
epd_draw_image(areap5, (uint8_t *)logo5_data, BLACK_ON_WHITE);


Rect_t areap6 = {
  .x = 330,
  .y = 345,
.width = hum_width,
  .height = hum_height,
};

epd_draw_grayscale_image(areap6, (uint8_t *)hum_data);
epd_draw_image(areap6, (uint8_t *)hum_data, BLACK_ON_WHITE);

Rect_t areap7 = {
  .x = 330,
  .y = 400,
.width = bat_width,
  .height = bat_height,
};

epd_draw_grayscale_image(areap7, (uint8_t *)bat_data);
epd_draw_image(areap7, (uint8_t *)bat_data, BLACK_ON_WHITE);



   // */
}


// === ESP-NOW RECEIVE CALLBACK ===
void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  Serial.println("\n📩 Data Received:");
  Serial.printf("Length: %d\n", len);

  if (len == sizeof(struct_message)) {
    memcpy(&receivedData, data, sizeof(struct_message));

    Serial.printf("🧭 WindDir: %d° (%s)\n", receivedData.windDir, getCardinalDirection(receivedData.windDir).c_str());
    Serial.printf("💨 Wind: %.1f km/h\n", receivedData.windSpeed);
    Serial.printf("💨 Gust: %.1f km/h\n", receivedData.windGust);
    Serial.printf("🌡️ Temp: %.1f°C\n", receivedData.temperature);
    Serial.printf("💧 Humidity: %.1f%%\n", receivedData.humidity);
    Serial.printf("🔋 Battery: %.2f V\n", receivedData.BatVoltage);
    delay(2000);
    refreshData();  // Refresh display with new data
  } else {
    Serial.println("⚠️ Invalid data size. Ignoring packet.");
  }
}



// === MAIN LOOP ===
void loop() {

  delay(1000);  // just idle
}
