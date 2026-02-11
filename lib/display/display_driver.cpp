#include <M5Unified.h>

#ifdef LVGL_UI
#include "display_driver.h"

M5GFX display;

bool is_pressed = false;
int32_t last_x = 0;
int32_t last_y = 0;

static lv_display_t *disp;
static lv_color_t *buf1;
static lv_color_t *buf2;
static lv_indev_t *indev;

extern "C"
{
  void set_brightness(int32_t value)
  {
    M5.Display.setBrightness(value);
  }
}

static uint32_t my_tick_get_cb(void)
{
  return millis();
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.pushPixels((uint16_t *)px_map, w * h, true);
  M5.Display.endWrite();
  lv_display_flush_ready(disp);
}

void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  data->point.x = last_x;
  data->point.y = last_y;
  data->state = is_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void display_init()
{

  uint16_t screenWidth = M5.Display.width();
  uint16_t screenHeight = M5.Display.height();

  log_i("resolution: %d x %d", screenWidth, screenHeight);

  lv_init();
  lv_tick_set_cb(my_tick_get_cb);

  // ===== BUFFERS MÁS GRANDES CON PSRAM =====
  size_t buffer_pixels = screenWidth * 80;

  buf1 = (lv_color_t *)heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  buf2 = (lv_color_t *)heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

  if (!buf1 || !buf2)
  {
    Serial.println("PSRAM no disponible, usando DMA");
    if (buf1)
      heap_caps_free(buf1);
    if (buf2)
      heap_caps_free(buf2);

    buffer_pixels = screenWidth * 40;
    buf1 = (lv_color_t *)heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = (lv_color_t *)heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
  }

  Serial.printf("Buffer: %d pixels\n", buffer_pixels);

  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf1, buf2, buffer_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touch_read);
}

void display_update()
{
  auto t = M5.Touch.getDetail();
  is_pressed = t.isPressed();
  if (is_pressed)
  {
    last_x = t.x;
    last_y = t.y;
  }

  lv_timer_handler();
}

#endif