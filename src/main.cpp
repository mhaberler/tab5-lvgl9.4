#include <lvgl.h>
#include <M5Unified.h>
#include "display_driver.h"
#include "ui.h"

void setup()
{
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);
  delay(100);
  display_init();
  M5.Display.setBrightness(200);
  ui_init();
}

void loop()
{
  M5.update();
  display_update();
  delay(5);
}