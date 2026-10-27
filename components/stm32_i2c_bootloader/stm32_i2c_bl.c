#include "stm32_i2c_bl.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "nvs.h"
#endif

static const char *CHESS_I2C_TAG = "CHESS_I2C";

#define CHESS_I2C_DEV_SLOTS 8

typedef struct {
  bool used;
  uint8_t addr7;
  i2c_master_dev_handle_t dev;
} chess_i2c_dev_slot_t;

static i2c_master_bus_handle_t s_i2c_bus;
static bool s_i2c_bus_ready;
static SemaphoreHandle_t s_i2c_lock;
static chess_i2c_dev_slot_t s_i2c_devs[CHESS_I2C_DEV_SLOTS];

static void chess_i2c_recover(void) {
  if (s_i2c_bus != NULL) {
    i2c_master_bus_reset(s_i2c_bus);
  }
}

esp_err_t chess_i2c_bus_init(int port, int sda_gpio, int scl_gpio) {
  if (s_i2c_bus_ready) {
    return ESP_OK;
  }
  if (s_i2c_lock == NULL) {
    s_i2c_lock = xSemaphoreCreateMutex();
    if (s_i2c_lock == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }
  i2c_master_bus_config_t cfg = {
      .i2c_port = (i2c_port_num_t)port,
      .sda_io_num = (gpio_num_t)sda_gpio,
      .scl_io_num = (gpio_num_t)scl_gpio,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = 1,
  };
  esp_err_t err = i2c_new_master_bus(&cfg, &s_i2c_bus);
  if (err != ESP_OK) {
    ESP_LOGE(CHESS_I2C_TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
    return err;
  }
  s_i2c_bus_ready = true;
  ESP_LOGI(CHESS_I2C_TAG, "i2c_master bus port=%d SDA=%d SCL=%d", port, sda_gpio,
           scl_gpio);
  return ESP_OK;
}

static void chess_i2c_lock(void) {
  if (s_i2c_lock != NULL) {
    xSemaphoreTake(s_i2c_lock, portMAX_DELAY);
  }
}

static void chess_i2c_unlock(void) {
  if (s_i2c_lock != NULL) {
    xSemaphoreGive(s_i2c_lock);
  }
}

/* Caller holds s_i2c_lock. The transfer must stay inside the same hold:
 * a second caller used to reset the bus while the first was still in
 * i2c_master_transmit_receive, which surfaced as ESP_ERR_INVALID_STATE. */
static esp_err_t chess_i2c_dev(uint8_t addr7, uint32_t freq_hz,
                               uint32_t scl_wait_us,
                               i2c_master_dev_handle_t *out) {
  if (!s_i2c_bus_ready || s_i2c_bus == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  if (freq_hz == 0 || out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = ESP_OK;
  i2c_master_dev_handle_t found = NULL;
  for (int i = 0; i < CHESS_I2C_DEV_SLOTS; i++) {
    if (s_i2c_devs[i].used && s_i2c_devs[i].addr7 == addr7) {
      found = s_i2c_devs[i].dev;
      break;
    }
  }
  if (found == NULL) {
    int slot = -1;
    for (int i = 0; i < CHESS_I2C_DEV_SLOTS; i++) {
      if (!s_i2c_devs[i].used) {
        slot = i;
        break;
      }
    }
    if (slot < 0) {
      err = ESP_ERR_NO_MEM;
    } else {
      i2c_device_config_t dev_cfg = {
          .dev_addr_length = I2C_ADDR_BIT_LEN_7,
          .device_address = addr7,
          .scl_speed_hz = freq_hz,
          .scl_wait_us = scl_wait_us,
      };
      err = i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &found);
      if (err == ESP_OK) {
        s_i2c_devs[slot].used = true;
        s_i2c_devs[slot].addr7 = addr7;
        s_i2c_devs[slot].dev = found;
      }
    }
  }
  if (err == ESP_OK) {
    *out = found;
  }
  return err;
}

static esp_err_t chess_i2c_checked(esp_err_t err) {
  if (err == ESP_ERR_TIMEOUT || err == ESP_ERR_INVALID_STATE) {
    chess_i2c_recover();
  }
  return err;
}

esp_err_t chess_i2c_transmit(uint8_t addr7, uint32_t freq_hz, uint32_t scl_wait_us,
                             const uint8_t *data, size_t len, int timeout_ms) {
  chess_i2c_lock();
  i2c_master_dev_handle_t dev = NULL;
  esp_err_t err = chess_i2c_dev(addr7, freq_hz, scl_wait_us, &dev);
  if (err == ESP_OK) {
    err = chess_i2c_checked(i2c_master_transmit(dev, data, len, timeout_ms));
  }
  chess_i2c_unlock();
  return err;
}

esp_err_t chess_i2c_receive(uint8_t addr7, uint32_t freq_hz, uint32_t scl_wait_us,
                            uint8_t *data, size_t len, int timeout_ms) {
  chess_i2c_lock();
  i2c_master_dev_handle_t dev = NULL;
  esp_err_t err = chess_i2c_dev(addr7, freq_hz, scl_wait_us, &dev);
  if (err == ESP_OK) {
    err = chess_i2c_checked(i2c_master_receive(dev, data, len, timeout_ms));
  }
  chess_i2c_unlock();
  return err;
}

esp_err_t chess_i2c_transmit_receive(uint8_t addr7, uint32_t freq_hz,
                                     uint32_t scl_wait_us, const uint8_t *wdata,
                                     size_t wlen, uint8_t *rdata, size_t rlen,
                                     int timeout_ms) {
  chess_i2c_lock();
  i2c_master_dev_handle_t dev = NULL;
  esp_err_t err = chess_i2c_dev(addr7, freq_hz, scl_wait_us, &dev);
  if (err == ESP_OK) {
    err = chess_i2c_checked(i2c_master_transmit_receive(dev, wdata, wlen, rdata,
                                                        rlen, timeout_ms));
  }
  chess_i2c_unlock();
  return err;
}

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE

static const char *TAG = "STM32_I2C_BL";

extern bool squares_scanning_enabled;

#define STM32_ACK 0x79
#define STM32_NACK 0x1F
#define STM32_BUSY 0x76

#define NRST_CFG(seg)                                                          \
  ((seg) == 0                                                                \
       ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0                                  \
       : (seg) == 1                                                            \
             ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1                            \
             : (seg) == 2                                                      \
                   ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2                      \
                   : CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3)

static bool s_inited;

static int bl_port(void) {
#if CONFIG_CHESS_STM32_BL_SHARE_I2C_HALL
  return CONFIG_CHESS_HALL_I2C_PORT_NUM;
#else
  return CONFIG_CHESS_STM32_BL_I2C_PORT_NUM;
#endif
}

static uint32_t bl_i2c_freq_hz(void) {
#if CONFIG_CHESS_STM32_BL_SHARE_I2C_HALL
  return (uint32_t)CONFIG_CHESS_STM32_BL_SHARED_I2C_FREQ_HZ;
#else
  return (uint32_t)CONFIG_CHESS_STM32_BL_I2C_FREQ_HZ;
#endif
}

static uint8_t bl_addr7(void) {
  return (uint8_t)CONFIG_CHESS_STM32_BL_TARGET_ADDR7;
}

static void bl_pause_scan(bool *was_enabled) {
  *was_enabled = squares_scanning_enabled;
  squares_scanning_enabled = false;
  vTaskDelay(pdMS_TO_TICKS(30));
}

static void bl_resume_scan(bool was_enabled) {
  squares_scanning_enabled = was_enabled;
}

static void bl_inter_frame(void) {
  vTaskDelay(pdMS_TO_TICKS(CONFIG_CHESS_STM32_BL_INTER_FRAME_MS));
}

static uint8_t bl_xor_buf(const uint8_t *p, size_t n) {
  uint8_t x = 0;
  for (size_t i = 0; i < n; i++) {
    x ^= p[i];
  }
  return x;
}

/** Bootloader clock-stretch budget. Replaces legacy i2c_set_timeout(0x1F). */
#define BL_SCL_WAIT_US 100000u

static esp_err_t bl_i2c_write(const uint8_t *data, size_t len) {
  return chess_i2c_transmit(bl_addr7(), bl_i2c_freq_hz(), BL_SCL_WAIT_US, data,
                            len, 500);
}

static esp_err_t bl_i2c_read(uint8_t *data, size_t len, int timeout_ms) {
  return chess_i2c_receive(bl_addr7(), bl_i2c_freq_hz(), BL_SCL_WAIT_US, data,
                           len, timeout_ms);
}

static int bl_sda_gpio(void) {
#if CONFIG_CHESS_STM32_BL_SHARE_I2C_HALL
  return CONFIG_CHESS_HALL_I2C_SDA_GPIO;
#else
  return CONFIG_CHESS_STM32_BL_I2C_SDA_GPIO;
#endif
}

static int bl_scl_gpio(void) {
#if CONFIG_CHESS_STM32_BL_SHARE_I2C_HALL
  return CONFIG_CHESS_HALL_I2C_SCL_GPIO;
#else
  return CONFIG_CHESS_STM32_BL_I2C_SCL_GPIO;
#endif
}

/** DEBUG: raw SDA/SCL pad levels (who holds the bus low). */
static void bl_log_bus(const char *tag) {
  ESP_LOGI(TAG, "[bus] %s: SDA=%d SCL=%d", tag,
           gpio_get_level((gpio_num_t)bl_sda_gpio()),
           gpio_get_level((gpio_num_t)bl_scl_gpio()));
}

/** DEBUG: one read transaction with raw bytes + elapsed ms in the log. */
static esp_err_t bl_dbg_read(const char *name, uint8_t *buf, size_t len,
                             int timeout_ms) {
  TickType_t t0 = xTaskGetTickCount();
  esp_err_t e = bl_i2c_read(buf, len, timeout_ms);
  uint32_t dt_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - t0);
  char hex[40] = {0};
  size_t n = len > 8 ? 8 : len;
  for (size_t i = 0; i < n; i++) {
    snprintf(hex + 3 * i, sizeof(hex) - 3 * i, "%02X ", buf[i]);
  }
  if (e == ESP_OK) {
    ESP_LOGI(TAG, "[dbg] %s: read %u B OK in %u ms: %s", name, (unsigned)len,
             (unsigned)dt_ms, hex);
  } else {
    ESP_LOGE(TAG, "[dbg] %s: read %u B %s after %u ms (buf: %s)", name,
             (unsigned)len, esp_err_to_name(e), (unsigned)dt_ms, hex);
  }
  return e;
}

/**
 * GET_ID frames: cmd write | 1 B ACK | 3 B (N,PID,PID) | 1 B ACK.
 * The closing ACK is its own read. A 4-byte read left SCL low on the legacy driver.
 */
static esp_err_t bl_get_id(uint16_t *pid_out) {
  uint8_t pair[2] = {0x02u, (uint8_t)~0x02u};
  ESP_LOGI(TAG, "[get_id] DEBUG: cmd write | ACK(1) | data(3) | ACK(1)");
  bl_log_bus("get_id start");
  /* Address NACK is logged by the driver as an error and returned as
   * INVALID_STATE. The bootloader protocol never started; treat it as NACK. */
  esp_log_level_t saved_i2c_log = esp_log_level_get("i2c.master");
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  esp_err_t err = bl_i2c_write(pair, sizeof(pair));
  esp_log_level_set("i2c.master", saved_i2c_log);
  if (err == ESP_ERR_INVALID_STATE) {
    return ESP_FAIL;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[get_id] cmd write: %s", esp_err_to_name(err));
    return err;
  }
  bl_log_bus("after cmd write");
  bl_inter_frame();

  uint8_t ack = 0;
  err = bl_dbg_read("frame1 ACK", &ack, 1, 200);
  bl_log_bus("after frame1");
  if (err != ESP_OK) {
    return err;
  }
  if (ack != STM32_ACK) {
    ESP_LOGE(TAG, "[get_id] frame1 byte 0x%02x (expect ACK 0x79)", ack);
    return ESP_ERR_INVALID_RESPONSE;
  }
  bl_inter_frame();

  uint8_t d[3] = {0};
  err = bl_dbg_read("frame2 data", d, sizeof(d), 200);
  bl_log_bus("after frame2");
  if (err != ESP_OK) {
    return err;
  }
  if (d[0] != 1u) {
    ESP_LOGE(TAG, "[get_id] N=0x%02x (expect 0x01): data not in this frame",
             d[0]);
    return ESP_ERR_INVALID_RESPONSE;
  }
  uint16_t pid = (uint16_t)(((uint16_t)d[1] << 8) | d[2]);
  bl_inter_frame();
  uint8_t ack2 = 0;
  err = bl_dbg_read("frame3 ACK", &ack2, 1, 200);
  bl_log_bus("after frame3");
  if (err != ESP_OK) {
    return err;
  }
  if (ack2 != STM32_ACK) {
    ESP_LOGW(TAG, "[get_id] trailing byte 0x%02x (expect ACK 0x79)", ack2);
  }
  ESP_LOGI(TAG, "[get_id] PID=0x%04X", (unsigned)pid);
  if (pid_out != NULL) {
    *pid_out = pid;
  }
  return ESP_OK;
}

static esp_err_t bl_wait_ack(uint32_t timeout_ms) {
  const TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
  unsigned busy_cnt = 0;
  for (;;) {
    uint8_t b = 0;
    esp_err_t e = bl_i2c_read(&b, 1, 120);
    if (e != ESP_OK) {
      ESP_LOGW(TAG, "[bl] read ACK: %s", esp_err_to_name(e));
      return e;
    }
    if (b == STM32_ACK) {
      if (busy_cnt) {
        ESP_LOGI(TAG, "[bl] ACK po %u× BUSY (0x76)", busy_cnt);
      }
      return ESP_OK;
    }
    if (b == STM32_NACK) {
      ESP_LOGW(TAG, "[bl] NACK (0x1F)");
      return ESP_FAIL;
    }
    if (b == STM32_BUSY) {
      if ((busy_cnt % 20u) == 0u && busy_cnt > 0) {
        ESP_LOGI(TAG, "[bl] waiting for completion (BUSY × %u)…", busy_cnt);
      }
      busy_cnt++;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    ESP_LOGW(TAG, "[bl] unexpected byte 0x%02x", b);
    if (xTaskGetTickCount() >= end) {
      ESP_LOGE(TAG, "[bl] timeout ACK/BUSY");
      return ESP_ERR_TIMEOUT;
    }
  }
}

static esp_err_t bl_send_command_byte_pair(uint8_t cmd) {
  ESP_LOGI(TAG, "[bl] command 0x%02X / ~0x%02X → I2C write", cmd,
           (unsigned)(uint8_t)~cmd);
  uint8_t pair[2] = {cmd, (uint8_t)~cmd};
  esp_err_t e = bl_i2c_write(pair, sizeof(pair));
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "[bl] I2C write cmd failed: %s", esp_err_to_name(e));
    return e;
  }
  bl_inter_frame();
  e = bl_wait_ack(800);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "[bl] ACK po cmd 0x%02X: %s", cmd, esp_err_to_name(e));
  } else {
    ESP_LOGI(TAG, "[bl] ACK OK po cmd 0x%02X", cmd);
  }
  return e;
}

/** GO command — the chip must already be in the ROM bootloader (without select/release). */
static esp_err_t bl_send_go_at_addr(uint32_t addr) {
  esp_err_t err = bl_send_command_byte_pair(0x21);
  if (err != ESP_OK) {
    return err;
  }

  uint8_t ab[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16),
                   (uint8_t)(addr >> 8), (uint8_t)addr};
  uint8_t aw[5];
  memcpy(aw, ab, 4);
  aw[4] = bl_xor_buf(ab, 4);
  bl_inter_frame();
  err = bl_i2c_write(aw, sizeof(aw));
  if (err != ESP_OK) {
    return err;
  }
  bl_inter_frame();
  return bl_wait_ack(500);
}

/**
 * WRITE MEMORY — prerequisite: target segment already selected by NRST, without freeing the bus.
 */
static esp_err_t bl_write_memory_payload(uint32_t addr, const uint8_t *data,
                                         size_t len) {
  ESP_RETURN_ON_FALSE(len > 0 && len <= 256, ESP_ERR_INVALID_ARG, TAG,
                      "payload len");

  ESP_LOGI(TAG, "[write] WRITE MEMORY addr=0x%08" PRIx32 " len=%u", addr,
           (unsigned)len);

  esp_err_t err = bl_send_command_byte_pair(0x31);
  if (err != ESP_OK) {
    return err;
  }

  uint8_t ab[4] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16),
                   (uint8_t)(addr >> 8), (uint8_t)addr};
  uint8_t aw[5];
  memcpy(aw, ab, 4);
  aw[4] = bl_xor_buf(ab, 4);
  bl_inter_frame();
  err = bl_i2c_write(aw, sizeof(aw));
  if (err != ESP_OK) {
    return err;
  }
  bl_inter_frame();
  err = bl_wait_ack(1500);
  if (err != ESP_OK) {
    return err;
  }

  uint8_t nbyte = (uint8_t)(len - 1u);
  uint8_t pkt[1 + 256 + 1];
  pkt[0] = nbyte;
  memcpy(pkt + 1, data, len);
  pkt[1 + len] = bl_xor_buf(pkt, 1 + len);

  bl_inter_frame();
  err = bl_i2c_write(pkt, (size_t)(1 + len + 1));
  if (err != ESP_OK) {
    return err;
  }
  bl_inter_frame();
  err = bl_wait_ack(5000);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "[write] blok na 0x%08" PRIx32 " (%u B) — hotovo", addr,
             (unsigned)len);
  }
  return err;
}

/** After i2c_param_config / install — explicitly turns on the internal pull-up on the bus pins. */
static void bl_apply_bus_pullups(int sda_gpio, int scl_gpio) {
  if (sda_gpio < 0 || scl_gpio < 0) {
    return;
  }
  esp_err_t e1 =
      gpio_set_pull_mode((gpio_num_t)sda_gpio, GPIO_PULLUP_ONLY);
  esp_err_t e2 =
      gpio_set_pull_mode((gpio_num_t)scl_gpio, GPIO_PULLUP_ONLY);
  if (e1 != ESP_OK || e2 != ESP_OK) {
    ESP_LOGW(TAG, "I2C pull-up SDA=%d SCL=%d: %s / %s", sda_gpio, scl_gpio,
             esp_err_to_name(e1), esp_err_to_name(e2));
  }
}

static void bl_configure_nrst_pin(int pin, bool output_high) {
  if (pin < 0) {
    return;
  }
  gpio_num_t g = (gpio_num_t)pin;
  gpio_reset_pin(g);
  gpio_set_direction(g, GPIO_MODE_OUTPUT);
  gpio_set_level(g, output_high ? 1 : 0);
}

esp_err_t stm32_i2c_bl_init(void) {
  if (s_inited) {
    return ESP_OK;
  }

#if !CONFIG_CHESS_STM32_BL_SHARE_I2C_HALL
  ESP_RETURN_ON_ERROR(chess_i2c_bus_init(CONFIG_CHESS_STM32_BL_I2C_PORT_NUM,
                                         CONFIG_CHESS_STM32_BL_I2C_SDA_GPIO,
                                         CONFIG_CHESS_STM32_BL_I2C_SCL_GPIO),
                      TAG, "i2c bus");
  bl_apply_bus_pullups(CONFIG_CHESS_STM32_BL_I2C_SDA_GPIO,
                       CONFIG_CHESS_STM32_BL_I2C_SCL_GPIO);
#else
  {
    esp_err_t er = chess_i2c_bus_init(CONFIG_CHESS_HALL_I2C_PORT_NUM,
                                      CONFIG_CHESS_HALL_I2C_SDA_GPIO,
                                      CONFIG_CHESS_HALL_I2C_SCL_GPIO);
    if (er != ESP_OK) {
      ESP_LOGE(TAG, "shared I2C bus: %s", esp_err_to_name(er));
      return er;
    }
    bl_apply_bus_pullups(CONFIG_CHESS_HALL_I2C_SDA_GPIO,
                         CONFIG_CHESS_HALL_I2C_SCL_GPIO);
    ESP_LOGI(TAG,
             "STM32 BL shares Hall i2c_master port %d @ %d Hz (Hall init used %d Hz)",
             bl_port(), CONFIG_CHESS_STM32_BL_SHARED_I2C_FREQ_HZ,
             CONFIG_CHESS_HALL_I2C_FREQ_HZ);
  }
#endif

  ESP_LOGI(TAG, "[init] mapa NRST: seg0=%d seg1=%d seg2=%d seg3=%d",
           CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0,
           CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1,
           CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2,
           CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3);

  for (unsigned s = 0; s < 4; s++) {
    int p = NRST_CFG(s);
    if (p >= 0) {
      bl_configure_nrst_pin(p, true);
      ESP_LOGI(TAG, "[init] NRST seg%u → GPIO%d (default HIGH = run)", s, p);
    }
  }

  if (CONFIG_CHESS_STM32_BL_BOOT0_GPIO >= 0) {
    gpio_num_t bg = (gpio_num_t)CONFIG_CHESS_STM32_BL_BOOT0_GPIO;
    gpio_reset_pin(bg);
    gpio_set_direction(bg, GPIO_MODE_OUTPUT);
    gpio_set_level(bg, 0);
    ESP_LOGI(TAG, "[init] BOOT0 → GPIO%d (default LOW)", CONFIG_CHESS_STM32_BL_BOOT0_GPIO);
  } else {
    ESP_LOGI(TAG, "[init] BOOT0 GPIO unassigned (-1)");
  }

  s_inited = true;
  ESP_LOGI(TAG,
           "[init] OK | I2C port=%d addr7=0x%02x inter_frame=%d ms",
           (int)bl_port(), bl_addr7(), CONFIG_CHESS_STM32_BL_INTER_FRAME_MS);
  return ESP_OK;
}

esp_err_t stm32_i2c_bl_select_target(uint8_t segment_0_to_3,
                                     bool boot0_enter_bootloader) {
  ESP_RETURN_ON_FALSE(segment_0_to_3 < 4, ESP_ERR_INVALID_ARG, TAG, "seg");
  ESP_RETURN_ON_ERROR(stm32_i2c_bl_init(), TAG, "init");

  if (CONFIG_CHESS_STM32_BL_BOOT0_GPIO >= 0) {
    gpio_set_level((gpio_num_t)CONFIG_CHESS_STM32_BL_BOOT0_GPIO,
                   boot0_enter_bootloader ? 1 : 0);
    ESP_LOGI(TAG, "[select] BOOT0=%s", boot0_enter_bootloader ? "HIGH" : "LOW");
  }

  /*
   * STM samples BOOT0 at NRST pregame. If NRST was already HIGH from init and only
   * we switch BOOT0, the chip will not go through a reset — it will remain in the user boot (with an empty
   * flash often "dead" state, no I²C ROM bootloader). NRST pulse required.
   *
   * When AUTO_USE_BOOT0=n (BOOT0 only hardware on VDD), boot0_enter_bootloader is from
   * caller false, but we still need the NRST pulse — otherwise, a new chip from the store
   * never enters system memory.
   */
#if CONFIG_CHESS_STM32_BL_AUTO_USE_BOOT0
  const bool pulse_nrst = boot0_enter_bootloader;
#else
  const bool pulse_nrst = true;
#endif
  if (pulse_nrst) {
    vTaskDelay(pdMS_TO_TICKS(5));
    for (unsigned s = 0; s < 4; s++) {
      int p = NRST_CFG(s);
      if (p < 0) {
        continue;
      }
      gpio_set_level((gpio_num_t)p, 0);
    }
    ESP_LOGI(TAG,
             "[select] NRST of all segments LOW (pulse reset; BOOT0=%s), 30 ms",
             boot0_enter_bootloader ? "HIGH (ESP)" : "HW/Behavior AUTO_USE_BOOT0=n");
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  for (unsigned s = 0; s < 4; s++) {
    int p = NRST_CFG(s);
    if (p < 0) {
      continue;
    }
    bool release = (s == segment_0_to_3);
    gpio_set_level((gpio_num_t)p, release ? 1 : 0);
    ESP_LOGI(TAG, "[select] seg%u GPIO%d → %s", s, p,
             release ? "HIGH (reset release)" : "LOW (reset hold)");
  }

  vTaskDelay(pdMS_TO_TICKS(CONFIG_CHESS_STM32_BL_POST_NRST_DELAY_MS));
  ESP_LOGI(TAG,
           "[select] active segment %u, waiting %d ms after NRST release",
           segment_0_to_3, CONFIG_CHESS_STM32_BL_POST_NRST_DELAY_MS);
  return ESP_OK;
}

/** BOOT0 LOW; optional NRST pulse (BOOT0 LOW during start-up game) to start the application from flash. */
static esp_err_t bl_release_run_app(bool pulse_nrst) {
  ESP_RETURN_ON_ERROR(stm32_i2c_bl_init(), TAG, "init");

  if (CONFIG_CHESS_STM32_BL_BOOT0_GPIO >= 0) {
    gpio_set_level((gpio_num_t)CONFIG_CHESS_STM32_BL_BOOT0_GPIO, 0);
  }
  vTaskDelay(pdMS_TO_TICKS(5));

  if (pulse_nrst) {
    bool any_nrst = false;
    for (unsigned s = 0; s < 4; s++) {
      int p = NRST_CFG(s);
      if (p < 0) {
        continue;
      }
      gpio_set_level((gpio_num_t)p, 0);
      any_nrst = true;
    }
    if (any_nrst) {
      ESP_LOGI(TAG,
               "[release] NRST pulz LOW 30 ms (BOOT0 LOW → boot z flash, ne ROM BL)");
      vTaskDelay(pdMS_TO_TICKS(30));
    }
  }

  for (unsigned s = 0; s < 4; s++) {
    int p = NRST_CFG(s);
    if (p >= 0) {
      gpio_set_level((gpio_num_t)p, 1);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(CONFIG_CHESS_STM32_BL_APP_BOOT_SETTLE_MS));
  ESP_LOGI(TAG,
           "[release] BOOT0 LOW, NRST HIGH, waiting %d ms for Hall/I2C application",
           CONFIG_CHESS_STM32_BL_APP_BOOT_SETTLE_MS);
  return ESP_OK;
}

esp_err_t stm32_i2c_bl_release_all_run_app(void) {
  return bl_release_run_app(true);
}

esp_err_t stm32_i2c_bl_cmd_get_id(uint8_t segment_0_to_3, bool boot0_enter,
                                  uint16_t *pid_out) {
  ESP_RETURN_ON_FALSE(pid_out, ESP_ERR_INVALID_ARG, TAG, "pid");

  bool scan_was = false;
  bl_pause_scan(&scan_was);

  ESP_LOGI(TAG, "[get_id] segment=%u boot0_mode=%d", segment_0_to_3,
           (int)boot0_enter);

  esp_err_t err =
      stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
  if (err != ESP_OK) {
    bl_resume_scan(scan_was);
    return err;
  }

  err = bl_get_id(pid_out);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[get_id] GET_ID cmd fail: %s", esp_err_to_name(err));
    goto done;
  }

done:
  (void)stm32_i2c_bl_release_all_run_app();
  bl_resume_scan(scan_was);
  return err;
}

esp_err_t stm32_i2c_bl_erase_all(uint8_t segment_0_to_3, bool boot0_enter) {
  bool scan_was = false;
  bl_pause_scan(&scan_was);

  ESP_LOGI(TAG, "[erase_all] segment=%u mass 0xFFFF", segment_0_to_3);

  esp_err_t err =
      stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
  if (err != ESP_OK) {
    bl_resume_scan(scan_was);
    return err;
  }

  err = bl_send_command_byte_pair(0x44);
  if (err != ESP_OK) {
    goto done;
  }

  uint8_t nblock[3] = {0xFF, 0xFF, 0x00};
  nblock[2] = bl_xor_buf(nblock, 2);
  bl_inter_frame();
  ESP_LOGI(TAG, "[erase_all] sending special erase 0xFFFF…");
  err = bl_i2c_write(nblock, sizeof(nblock));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[erase_all] write: %s", esp_err_to_name(err));
    goto done;
  }

  bl_inter_frame();
  ESP_LOGI(TAG, "[erase_all] waiting for erase (timeout %d ms)…",
           CONFIG_CHESS_STM32_BL_ERASE_ACK_MS);
  err = bl_wait_ack(CONFIG_CHESS_STM32_BL_ERASE_ACK_MS);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "[erase_all] completed OK");
  }

done:
  (void)stm32_i2c_bl_release_all_run_app();
  bl_resume_scan(scan_was);
  return err;
}

esp_err_t stm32_i2c_bl_write_memory(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t addr, const uint8_t *data,
                                    size_t len) {
  ESP_RETURN_ON_FALSE(data && len > 0 && len <= 256, ESP_ERR_INVALID_ARG, TAG,
                      "len");

  bool scan_was = false;
  bl_pause_scan(&scan_was);

  esp_err_t err =
      stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
  if (err != ESP_OK) {
    bl_resume_scan(scan_was);
    return err;
  }

  err = bl_write_memory_payload(addr, data, len);

  (void)stm32_i2c_bl_release_all_run_app();
  bl_resume_scan(scan_was);
  return err;
}

esp_err_t stm32_i2c_bl_go(uint8_t segment_0_to_3, bool boot0_enter,
                          uint32_t addr) {
  bool scan_was = false;
  bl_pause_scan(&scan_was);

  esp_err_t err =
      stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
  if (err != ESP_OK) {
    bl_resume_scan(scan_was);
    return err;
  }

  err = bl_send_go_at_addr(addr);

  (void)stm32_i2c_bl_release_all_run_app();
  bl_resume_scan(scan_was);
  return err;
}

typedef esp_err_t (*stm32_bl_flash_fill_fn)(void *ctx, size_t off, uint8_t *dst,
                                            size_t len);

static esp_err_t stm32_bl_fill_from_ram(void *ctx, size_t off, uint8_t *dst,
                                        size_t len) {
  memcpy(dst, (const uint8_t *)ctx + off, len);
  return ESP_OK;
}

static esp_err_t stm32_i2c_bl_flash_binary_impl(
    uint8_t segment_0_to_3, bool boot0_enter, uint32_t flash_base, size_t bin_len,
    stm32_bl_flash_fill_fn fill, void *fill_ctx) {
  const size_t chunk = 128;
  bool scan_was = false;
  bl_pause_scan(&scan_was);

  ESP_LOGI(TAG,
           "[flash_bin] START seg=%u boot0=%d base=0x%08" PRIx32 " size=%u B",
           segment_0_to_3, (int)boot0_enter, flash_base, (unsigned)bin_len);

  esp_err_t err =
      stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
  if (err != ESP_OK) {
    bl_resume_scan(scan_was);
    return err;
  }

  ESP_LOGI(TAG, "[flash_bin] GET_ID (0x02) — kontrola ROM I²C bootloaderu…");
  {
    uint16_t pid = 0;
    err = bl_get_id(&pid);
    if (err == ESP_FAIL) {
      ESP_LOGW(TAG, "[flash_bin] GET_ID NACK — wait 120 ms and repeat");
      vTaskDelay(pdMS_TO_TICKS(120));
      err = bl_get_id(&pid);
    }
    if (err == ESP_FAIL) {
      ESP_LOGW(TAG,
               "[flash_bin] seg%u: ROM bootloader neodpověděl — aplikace zůstává",
               segment_0_to_3);
      err = ESP_ERR_NOT_FOUND;
      goto restore;
    }
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "[flash_bin] GET_ID: %s", esp_err_to_name(err));
      goto restore;
    }
    ESP_LOGI(TAG, "[flash_bin] PID=0x%04X — continuing mass erase (0x44)",
             (unsigned)pid);
  }

  err = bl_send_command_byte_pair(0x44);
  if (err != ESP_OK) {
    goto restore;
  }

  uint8_t mass[3] = {0xFF, 0xFF, 0x00};
  mass[2] = bl_xor_buf(mass, 2);
  bl_inter_frame();
  ESP_LOGI(TAG, "[flash_bin] erase: pokus o mass erase…");
  err = bl_i2c_write(mass, sizeof(mass));
  if (err != ESP_OK) {
    goto restore;
  }
  bl_inter_frame();
  err = bl_wait_ack(CONFIG_CHESS_STM32_BL_ERASE_ACK_MS);
  if (err != ESP_OK) {
    ESP_LOGW(TAG,
             "[flash_bin] mass erase: %s → trying page erase",
             esp_err_to_name(err));
    err = stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
    if (err != ESP_OK) {
      goto restore;
    }

    const unsigned psize = (unsigned)CONFIG_CHESS_STM32_BL_FLASH_PAGE_SIZE;
    unsigned pages = (unsigned)((bin_len + psize - 1) / psize);
    ESP_LOGI(TAG, "[flash_bin] page erase: %u pages à %u B", pages, psize);
    for (unsigned pg = 0; pg < pages && err == ESP_OK; pg++) {
      ESP_LOGI(TAG, "[flash_bin] deleting page %u / %u…", pg + 1, pages);
      err = bl_send_command_byte_pair(0x44);
      if (err != ESP_OK) {
        break;
      }
      uint8_t nb[3] = {0x00, 0x00, 0x00};
      nb[2] = bl_xor_buf(nb, 2);
      bl_inter_frame();
      err = bl_i2c_write(nb, sizeof(nb));
      if (err != ESP_OK) {
        break;
      }
      bl_inter_frame();
      err = bl_wait_ack(800);
      if (err != ESP_OK) {
        break;
      }

      uint8_t pn[3] = {(uint8_t)((pg >> 8) & 0xFF), (uint8_t)(pg & 0xFF), 0};
      pn[2] = bl_xor_buf(pn, 2);
      bl_inter_frame();
      err = bl_i2c_write(pn, sizeof(pn));
      if (err != ESP_OK) {
        break;
      }
      bl_inter_frame();
      err = bl_wait_ack(CONFIG_CHESS_STM32_BL_ERASE_ACK_MS);
    }
    if (err != ESP_OK) {
      goto restore;
    }
  } else {
    ESP_LOGI(TAG, "[flash_bin] mass erase OK");
  }

  {
    uint8_t chunk_buf[128];
    unsigned total_chunks = (unsigned)((bin_len + chunk - 1) / chunk);
    unsigned ci = 0;
    for (size_t off = 0; off < bin_len && err == ESP_OK; off += chunk) {
      size_t n = bin_len - off;
      if (n > chunk) {
        n = chunk;
      }
      ci++;
      ESP_LOGI(TAG,
               "[flash_bin] write chunk %u / %u @ off=%u (%u B)",
               ci, total_chunks, (unsigned)off, (unsigned)n);
      err = fill(fill_ctx, off, chunk_buf, n);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "[flash_bin] reading source @%u: %s", (unsigned)off,
                 esp_err_to_name(err));
        break;
      }
      err = bl_write_memory_payload(flash_base + (uint32_t)off, chunk_buf, n);
    }
  }

restore:
  bool go_ok = false;
  if (err == ESP_OK) {
#if CONFIG_CHESS_STM32_BL_GO_AFTER_FLASH
    esp_err_t go_err = bl_send_go_at_addr(flash_base);
    if (go_err != ESP_OK) {
      ESP_LOGW(TAG,
               "[flash_bin] GO @0x%08" PRIx32 " failed (%s) — backup NRST pulse",
               flash_base, esp_err_to_name(go_err));
    } else {
      ESP_LOGI(TAG, "[flash_bin] GO @0x%08" PRIx32 " OK", flash_base);
      go_ok = true;
      vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif
    ESP_LOGI(TAG, "[flash_bin] KONEC seg=%u OK", segment_0_to_3);
  } else if (err == ESP_ERR_NOT_FOUND) {
    ESP_LOGW(TAG, "[flash_bin] END seg=%u bez bootloaderu", segment_0_to_3);
  } else {
    ESP_LOGE(TAG, "[flash_bin] END seg=%u error: %s", segment_0_to_3,
             esp_err_to_name(err));
  }

  /* GO successful → the application is already running, do not pulse NRST. Otherwise BOOT0 LOW + NRST↑. */
  (void)bl_release_run_app(err != ESP_OK || !go_ok);
  bl_resume_scan(scan_was);
  return err;
}

esp_err_t stm32_i2c_bl_flash_binary(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t flash_base, const uint8_t *bin,
                                    size_t bin_len) {
  ESP_RETURN_ON_FALSE(bin && bin_len, ESP_ERR_INVALID_ARG, TAG, "bin");
  /* STM32C031F6P6 (czechmate PCB) and C031C6 (Nucleo) are 32 KiB.
   * A tighter Kconfig cap is honored; never accept a 512 KiB partition. */
  size_t cap = 32u * 1024u;
#if defined(CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES) &&                     \
    (CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES > 0)
  if ((size_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES < cap) {
    cap = (size_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES;
  }
#endif
  if (bin_len > cap) {
    ESP_LOGE(TAG,
             "[STAGING] flash image %u B exceeds STM32C031 flash cap %u B — refused",
             (unsigned)bin_len, (unsigned)cap);
    return ESP_ERR_INVALID_SIZE;
  }
  return stm32_i2c_bl_flash_binary_impl(segment_0_to_3, boot0_enter, flash_base,
                                        bin_len, stm32_bl_fill_from_ram,
                                        (void *)bin);
}

#else /* !CONFIG_CHESS_STM32_I2C_BL_ENABLE */

esp_err_t stm32_i2c_bl_init(void) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t stm32_i2c_bl_select_target(uint8_t segment_0_to_3,
                                     bool boot0_enter_bootloader) {
  (void)segment_0_to_3;
  (void)boot0_enter_bootloader;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stm32_i2c_bl_release_all_run_app(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stm32_i2c_bl_cmd_get_id(uint8_t segment_0_to_3, bool boot0_enter,
                                  uint16_t *pid_out) {
  (void)segment_0_to_3;
  (void)boot0_enter;
  (void)pid_out;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stm32_i2c_bl_erase_all(uint8_t segment_0_to_3, bool boot0_enter) {
  (void)segment_0_to_3;
  (void)boot0_enter;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stm32_i2c_bl_write_memory(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t addr, const uint8_t *data,
                                    size_t len) {
  (void)segment_0_to_3;
  (void)boot0_enter;
  (void)addr;
  (void)data;
  (void)len;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stm32_i2c_bl_go(uint8_t segment_0_to_3, bool boot0_enter,
                          uint32_t addr) {
  (void)segment_0_to_3;
  (void)boot0_enter;
  (void)addr;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t stm32_i2c_bl_flash_binary(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t flash_base, const uint8_t *bin,
                                    size_t bin_len) {
  (void)segment_0_to_3;
  (void)boot0_enter;
  (void)flash_base;
  (void)bin;
  (void)bin_len;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* !CONFIG_CHESS_STM32_I2C_BL_ENABLE */

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT

static const char k_hall_addr_magic[8] = {'H', 'A', 'L', 'L', 'A', 'D', 'D', 'R'};

typedef struct {
  const esp_partition_t *part;
  uint8_t addr7;
  size_t addr_off;
} stm32_part_fill_ctx_t;

static esp_err_t stm32_bl_find_addr_slot(const esp_partition_t *part, size_t eff,
                                         size_t *addr_off) {
  uint8_t buf[256];
  uint8_t prev[7];
  size_t prev_n = 0;
  size_t off = 0;

  *addr_off = SIZE_MAX;
  while (off < eff) {
    size_t n = eff - off;
    if (n > sizeof(buf)) {
      n = sizeof(buf);
    }
    esp_err_t e = esp_partition_read(part, off, buf, n);
    if (e != ESP_OK) {
      return e;
    }
    uint8_t look[sizeof(buf) + 7];
    memcpy(look, prev, prev_n);
    memcpy(look + prev_n, buf, n);
    size_t look_n = prev_n + n;
    size_t origin = off - prev_n;
    for (size_t i = 0; i + 8 <= look_n; i++) {
      if (memcmp(look + i, k_hall_addr_magic, 8) == 0) {
        *addr_off = origin + i + 8;
        return ESP_OK;
      }
    }
    prev_n = look_n >= 7 ? 7 : look_n;
    memcpy(prev, look + look_n - prev_n, prev_n);
    off += n;
  }
  return ESP_ERR_NOT_FOUND;
}

static esp_err_t stm32_bl_fill_patched(void *ctx, size_t off, uint8_t *dst,
                                       size_t len) {
  stm32_part_fill_ctx_t *fill = (stm32_part_fill_ctx_t *)ctx;
  esp_err_t e = esp_partition_read(fill->part, off, dst, len);
  if (e != ESP_OK || fill->addr_off == SIZE_MAX) {
    return e;
  }
  if (fill->addr_off >= off && fill->addr_off < off + len) {
    dst[fill->addr_off - off] = fill->addr7;
  }
  return ESP_OK;
}

/** Content length without trailing 0xFF (read in blocks from the end — no big malloc). */
static esp_err_t auto_part_effective_bytes(const esp_partition_t *part,
                                           size_t *effp) {
  uint8_t buf[2048];
  size_t eff = part->size;
  while (eff > 0) {
    const size_t chunk_sz = eff > sizeof(buf) ? sizeof(buf) : eff;
    const size_t pos = eff - chunk_sz;
    esp_err_t e = esp_partition_read(part, pos, buf, chunk_sz);
    if (e != ESP_OK) {
      return e;
    }
    size_t i = chunk_sz;
    while (i > 0 && buf[i - 1] == 0xFF) {
      i--;
    }
    if (i > 0) {
      *effp = pos + i;
      return ESP_OK;
    }
    eff = pos;
  }
  *effp = 0;
  return ESP_OK;
}

static esp_err_t auto_part_crc32(const esp_partition_t *part, size_t len,
                                 uint32_t *crc_out) {
  uint8_t buf[1024];
  uint32_t crc = 0;
  size_t off = 0;
  while (off < len) {
    size_t n = len - off > sizeof(buf) ? sizeof(buf) : len - off;
    esp_err_t e = esp_partition_read(part, off, buf, n);
    if (e != ESP_OK) {
      return e;
    }
    crc = esp_rom_crc32_le(crc, buf, n);
    off += n;
  }
  *crc_out = crc;
  return ESP_OK;
}

#endif /* BL_ENABLE && AUTO_FLASH_ON_BOOT */

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

static uint8_t bl_hall_segment_addr7(unsigned seg) {
  switch (seg) {
  case 0:
    return (uint8_t)CONFIG_CHESS_HALL_SEG0_ADDR;
  case 1:
    return (uint8_t)CONFIG_CHESS_HALL_SEG1_ADDR;
  case 2:
    return (uint8_t)CONFIG_CHESS_HALL_SEG2_ADDR;
  case 3:
    return (uint8_t)CONFIG_CHESS_HALL_SEG3_ADDR;
  default:
    return (uint8_t)CONFIG_CHESS_HALL_SEG0_ADDR;
  }
}

static esp_err_t bl_probe_hall_segment_once(uint8_t seg, int timeout_ms) {
  uint8_t addr = bl_hall_segment_addr7(seg);
  uint8_t reg = (uint8_t)CONFIG_CHESS_HALL_REG_START;
  uint8_t buf[2];
  return chess_i2c_transmit_receive(addr, (uint32_t)CONFIG_CHESS_HALL_I2C_FREQ_HZ,
                                    20000, &reg, 1, buf, sizeof(buf),
                                    timeout_ms);
}

static esp_err_t bl_probe_hall_segment_retries(uint8_t seg) {
  const unsigned tries =
      (unsigned)CONFIG_CHESS_STM32_BL_HALL_PROBE_RETRIES;
  const int gap_ms = CONFIG_CHESS_STM32_BL_HALL_PROBE_RETRY_MS;
  esp_err_t last = ESP_FAIL;

  for (unsigned t = 0; t < tries; t++) {
    last = bl_probe_hall_segment_once(seg, 120);
    if (last == ESP_OK) {
      return ESP_OK;
    }
    if (t + 1u < tries) {
      vTaskDelay(pdMS_TO_TICKS(gap_ms));
    }
  }
  return last;
}

/** Hall check. fitted_mask 0 probes every segment in range; otherwise only those bits. */
static bool bl_probe_all_nrst_hall_segments(bool log_ok, uint32_t fitted_mask) {
  bool all_ok = true;
  bool any = false;

  for (unsigned seg = 0; seg < 4; seg++) {
    if (NRST_CFG(seg) < 0) {
      continue;
    }
    if (fitted_mask != 0 && (fitted_mask & (1u << seg)) == 0) {
      continue;
    }
#if defined(CONFIG_CHESS_HALL_SEGMENT_COUNT)
    /* PCB brings out NRST for four footprints; only populated segments are probed. */
    if (fitted_mask == 0 && seg >= (unsigned)CONFIG_CHESS_HALL_SEGMENT_COUNT) {
      continue;
    }
#endif
    any = true;
    esp_err_t pe = bl_probe_hall_segment_retries((uint8_t)seg);
    if (pe != ESP_OK) {
      ESP_LOGW(TAG,
               "[hall_probe] seg%u addr 0x%02x does not match (%s)",
               seg, bl_hall_segment_addr7(seg), esp_err_to_name(pe));
      all_ok = false;
    } else if (log_ok) {
      ESP_LOGI(TAG, "[hall_probe] seg%u addr 0x%02x OK", seg,
               bl_hall_segment_addr7(seg));
    }
  }

  if (!any) {
    ESP_LOGD(TAG, "[hall_probe] no NRST GPIO — skipped");
    return true;
  }
  return all_ok;
}

#endif /* BL + HALL */

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT && \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
/** Image we will not push again while Hall stays up. A new CRC tries once. */
static void bl_remember_held_image(uint32_t crc, uint32_t len) {
  nvs_handle_t hw;
  if (nvs_open("stm32_bl", NVS_READWRITE, &hw) != ESP_OK) {
    return;
  }
  nvs_set_u32(hw, "af_hold", crc);
  nvs_set_u32(hw, "af_hold_len", len);
  nvs_commit(hw);
  nvs_close(hw);
}
#endif

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT
static SemaphoreHandle_t s_boot_flash_done_sem;

void stm32_i2c_bl_boot_flash_sync_prepare(void) {
  if (s_boot_flash_done_sem != NULL) {
    return;
  }
  s_boot_flash_done_sem = xSemaphoreCreateBinary();
  if (s_boot_flash_done_sem == NULL) {
    ESP_LOGE(TAG, "boot_flash_sync: xSemaphoreCreateBinary failed");
  }
}

static void stm32_i2c_bl_boot_flash_sync_signal_done(void) {
  if (s_boot_flash_done_sem != NULL) {
    (void)xSemaphoreGive(s_boot_flash_done_sem);
  }
}

void stm32_i2c_bl_boot_flash_sync_wait(TickType_t timeout_ticks) {
  if (s_boot_flash_done_sem == NULL) {
    return;
  }
  if (xSemaphoreTake(s_boot_flash_done_sem, timeout_ticks) != pdTRUE) {
    ESP_LOGW(TAG,
             "boot_flash_sync: timeout — STM32 auto-flash not finished in time "
             "(zkontroluj I²C / NRST / BOOT0)");
  }
}
#else
void stm32_i2c_bl_boot_flash_sync_prepare(void) {}

void stm32_i2c_bl_boot_flash_sync_wait(TickType_t timeout_ticks) {
  (void)timeout_ticks;
}
#endif

void stm32_i2c_bl_maybe_auto_flash_on_boot(void) {
#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT
  static const char *TAG_A = "STM32_AUTO";

  esp_err_t ie = stm32_i2c_bl_init();
  if (ie != ESP_OK) {
    ESP_LOGE(TAG_A, "stm32_i2c_bl_init: %s", esp_err_to_name(ie));
    goto out;
  }

  ESP_LOGI(TAG_A, "=== auto-flash on boot (partition '%s') ===",
           CONFIG_CHESS_STM32_BL_AUTO_PARTITION_LABEL);

#if CONFIG_CHESS_STM32_BL_AUTO_USE_BOOT0
  if (CONFIG_CHESS_STM32_BL_BOOT0_GPIO < 0) {
    ESP_LOGE(TAG_A,
             "AUTO_USE_BOOT0 je zapnuto, ale CHESS_STM32_BL_BOOT0_GPIO=-1. "
             "A factory / blank STM on the main flash will not run your Hall code or ROM bootloader "
             "on the bus — without entering the system memory (BOOT0=VDD when releasing NRST) ESP "
             "will not address the bootloader at 0x%02x. Set BOOT0 GPIO in menuconfig and connect "
             "it to the MCU's BOOT0 pin (or disable AUTO_USE_BOOT0 and hold BOOT0 manually with a jumper).",
             (unsigned)CONFIG_CHESS_STM32_BL_TARGET_ADDR7);
    goto out;
  }
#endif

  const esp_partition_t *part = esp_partition_find_first(
      ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,
      CONFIG_CHESS_STM32_BL_AUTO_PARTITION_LABEL);
  if (part == NULL) {
    ESP_LOGW(TAG_A, "partition '%s' not found — auto-flash skipped",
             CONFIG_CHESS_STM32_BL_AUTO_PARTITION_LABEL);
    goto out;
  }

  ESP_LOGI(TAG_A, "partition: offset=0x%08" PRIx32 " size=%" PRIu32 " B",
           (uint32_t)part->address, (uint32_t)part->size);

  size_t eff = 0;
  esp_err_t tre = auto_part_effective_bytes(part, &eff);
  if (tre != ESP_OK) {
    ESP_LOGE(TAG_A, "trimming end of section: %s", esp_err_to_name(tre));
    goto out;
  }

  if (eff < 32) {
    ESP_LOGW(TAG_A, "partition image is empty or just 0xFF (eff=%u B)",
             (unsigned)eff);
    goto out;
  }

  if (CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES > 0 &&
      eff > (size_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES) {
    ESP_LOGE(TAG_A,
             "partition image is %u B, larger than target flash max %d B — refusing",
             (unsigned)eff, CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES);
    goto out;
  }

  uint32_t crc = 0;
  esp_err_t cre = auto_part_crc32(part, eff, &crc);
  if (cre != ESP_OK) {
    ESP_LOGE(TAG_A, "CRC from partition: %s", esp_err_to_name(cre));
    goto out;
  }

  ESP_LOGI(TAG_A,
           "image after cropping end 0xFF: %" PRIu32 " B, CRC32=0x%08" PRIx32
           " (read by blocks, no malloc %" PRIu32 ")",
           (uint32_t)eff, crc, (uint32_t)part->size);

#if CONFIG_CHESS_STM32_BL_AUTO_FLASH_SKIP_IF_UNCHANGED
  /* FORCE is often missing in sdkconfig as a macro (# is not set) — don't use !CONFIG_FORCE directly */
#if !(defined(CONFIG_CHESS_STM32_BL_AUTO_FLASH_FORCE) && CONFIG_CHESS_STM32_BL_AUTO_FLASH_FORCE)
  {
    nvs_handle_t h;
    if (nvs_open("stm32_bl", NVS_READONLY, &h) == ESP_OK) {
      uint32_t oc = 0;
      uint32_t ol = 0;
      uint32_t omask = 0;
      esp_err_t ge1 = nvs_get_u32(h, "af_crc", &oc);
      esp_err_t ge2 = nvs_get_u32(h, "af_len", &ol);
      esp_err_t ge3 = nvs_get_u32(h, "af_mask", &omask);
      nvs_close(h);
      if (ge3 != ESP_OK) {
        omask = 0;
      }
      if (ge1 == ESP_OK && ge2 == ESP_OK && oc == crc &&
          ol == (uint32_t)eff) {
#if CONFIG_CHESS_STM32_BL_REFLASH_IF_HALL_MISSING &&                          \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        if (!bl_probe_all_nrst_hall_segments(false, omask)) {
          ESP_LOGW(TAG_A,
                   "NVS says the same image, but Hall doesn't answer — "
                   "force auto-flash (STM replacement / blank chip?)");
        } else
#endif
        {
          ESP_LOGI(TAG_A,
                   "auto-flash skipped (same content CRC=0x%08" PRIx32
                   ", %" PRIu32 " B)",
                   crc, (uint32_t)eff);
#if CONFIG_CHESS_STM32_BL_VERIFY_HALL_AFTER_FLASH &&                            \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
          (void)bl_probe_all_nrst_hall_segments(true, omask);
#endif
          goto out;
        }
      }
    }
  }
#endif
#endif

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL &&                                      \
    !(defined(CONFIG_CHESS_STM32_BL_AUTO_FLASH_FORCE) &&                       \
      CONFIG_CHESS_STM32_BL_AUTO_FLASH_FORCE)
  /* Hall up: do not reset into the ROM on every boot. Remember this image.
   * A different CRC gets one attempt, then is remembered too if the ROM
   * still does not answer. */
  bool nrst_set = CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0 >= 0 ||
                  CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1 >= 0 ||
                  CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2 >= 0 ||
                  CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3 >= 0;
  if (nrst_set && bl_probe_all_nrst_hall_segments(false, 0)) {
    uint32_t hold = 0;
    uint32_t hold_len = 0;
    bool known = false;
    nvs_handle_t hh;
    if (nvs_open("stm32_bl", NVS_READONLY, &hh) == ESP_OK) {
      known = nvs_get_u32(hh, "af_hold", &hold) == ESP_OK &&
              nvs_get_u32(hh, "af_hold_len", &hold_len) == ESP_OK;
      nvs_close(hh);
    }
    if (!known || (hold == crc && hold_len == (uint32_t)eff)) {
      if (!known) {
        bl_remember_held_image(crc, (uint32_t)eff);
        ESP_LOGW(TAG_A, "Hall běží, tento obraz do bootloaderu neposílám");
      } else {
        ESP_LOGI(TAG_A, "auto-flash skipped, Hall běží, stejný obraz");
      }
      goto out;
    }
    ESP_LOGW(TAG_A, "nový obraz STM32, jeden pokus o bootloader");
  }
#endif

  bool use_boot = CONFIG_CHESS_STM32_BL_AUTO_USE_BOOT0 &&
                  CONFIG_CHESS_STM32_BL_BOOT0_GPIO >= 0;
  uint32_t base = (uint32_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_BASE;
  ESP_LOGI(TAG_A,
           "parametry: FLASH_BASE=0x%08" PRIx32 " USE_BOOT0=%d "
           "(BOOT0 GPIO=%d)",
           base, use_boot ? 1 : 0, CONFIG_CHESS_STM32_BL_BOOT0_GPIO);

  size_t addr_off = SIZE_MAX;
  if (stm32_bl_find_addr_slot(part, eff, &addr_off) != ESP_OK) {
    ESP_LOGE(TAG_A,
             "STM image has no HALLADDR slot — rebuild firmware/stm32_hall_c031 "
             "into embedded/stm32_fw_embedded.bin before flashing");
    goto out;
  }

  bool any_nrst = false;
  bool all_ok = true;
  uint32_t written_mask = 0;
  for (unsigned seg = 0; seg < 4; seg++) {
    int nrst = seg == 0   ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0
               : seg == 1 ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1
               : seg == 2 ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2
                          : CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3;
    if (nrst < 0) {
      continue;
    }
#if defined(CONFIG_CHESS_HALL_SEGMENT_COUNT)
    if (seg >= (unsigned)CONFIG_CHESS_HALL_SEGMENT_COUNT) {
      continue;
    }
#endif
    any_nrst = true;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    uint8_t addr7 = bl_hall_segment_addr7(seg);
#else
    uint8_t addr7 = (uint8_t)(0x30u + seg);
#endif
    stm32_part_fill_ctx_t fill_ctx = {
        .part = part,
        .addr7 = addr7,
        .addr_off = addr_off,
    };
    ESP_LOGI(TAG_A,
             "segment %u: uploading %" PRIu32 " B → STM @0x%08" PRIx32
             " as I2C 0x%02x",
             seg, (uint32_t)eff, base, addr7);
    esp_err_t fe = stm32_i2c_bl_flash_binary_impl(
        (uint8_t)seg, use_boot, base, eff, stm32_bl_fill_patched, &fill_ctx);
    if (fe == ESP_ERR_NOT_FOUND) {
      ESP_LOGW(TAG_A, "segment %u: ROM bootloader neodpověděl", seg);
    } else if (fe != ESP_OK) {
      ESP_LOGE(TAG_A, "segment %u: %s", seg, esp_err_to_name(fe));
      all_ok = false;
    } else {
      written_mask |= (1u << seg);
      ESP_LOGI(TAG_A, "segment %u: hotovo (I2C 0x%02x)", seg, addr7);
    }
  }

  if (!any_nrst) {
    ESP_LOGW(TAG_A,
             "no NRST GPIO — set CHESS_STM32_BL_NRST_GPIO_SEG* in menuconfig");
  }

  if (all_ok && written_mask != 0) {
    bool hall_ok = true;
#if CONFIG_CHESS_STM32_BL_VERIFY_HALL_AFTER_FLASH &&                          \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    hall_ok = bl_probe_all_nrst_hall_segments(true, written_mask);
    if (!hall_ok) {
      ESP_LOGE(TAG_A,
               "auto-flash write OK, but Hall doesn't respond — NVS won't save, "
               "next boot will try flash again (NRST/BOOT0/I2C PB6/PB7)");
    }
#endif
    if (hall_ok) {
      nvs_handle_t hw;
      if (nvs_open("stm32_bl", NVS_READWRITE, &hw) == ESP_OK) {
        nvs_set_u32(hw, "af_crc", crc);
        nvs_set_u32(hw, "af_len", (uint32_t)eff);
        nvs_set_u32(hw, "af_mask", written_mask);
        nvs_commit(hw);
        nvs_close(hw);
        ESP_LOGI(TAG_A,
                 "NVS: stored CRC+length (next boot skips with the same content)");
      }
    }
  } else if (any_nrst && !all_ok) {
    ESP_LOGE(TAG_A,
             "auto-flash failed on at least one segment — Hall reading "
             "nebude fungovat");
  } else if (any_nrst && written_mask == 0) {
    ESP_LOGW(TAG_A,
             "ROM bootloader neodpověděl, STM32 jedou dál ze své aplikace");
#if CONFIG_CHESS_STM32_BL_VERIFY_HALL_AFTER_FLASH &&                            \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    if (!bl_probe_all_nrst_hall_segments(true, 0)) {
      ESP_LOGE(TAG_A, "po návratu z bootloaderu Hall neodpovídá");
    } else {
      bl_remember_held_image(crc, (uint32_t)eff);
    }
#endif
  }

out:
  stm32_i2c_bl_boot_flash_sync_signal_done();

#endif /* AUTO_FLASH_ON_BOOT && BL */
}
