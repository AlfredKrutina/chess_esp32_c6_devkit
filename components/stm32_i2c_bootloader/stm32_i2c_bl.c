#include "stm32_i2c_bl.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <string.h>

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE && CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "nvs.h"
#endif

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE

static const char *TAG = "STM32_I2C_BL";

extern bool matrix_scanning_enabled;

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

static i2c_port_t bl_port(void) {
#if CONFIG_CHESS_STM32_BL_SHARE_I2C_HALL
  return (i2c_port_t)CONFIG_CHESS_HALL_I2C_PORT_NUM;
#else
  return (i2c_port_t)CONFIG_CHESS_STM32_BL_I2C_PORT_NUM;
#endif
}

static uint8_t bl_addr7(void) {
  return (uint8_t)CONFIG_CHESS_STM32_BL_TARGET_ADDR7;
}

static void bl_pause_scan(bool *was_enabled) {
  *was_enabled = matrix_scanning_enabled;
  matrix_scanning_enabled = false;
  vTaskDelay(pdMS_TO_TICKS(30));
}

static void bl_resume_scan(bool was_enabled) {
  matrix_scanning_enabled = was_enabled;
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

static esp_err_t bl_i2c_write(const uint8_t *data, size_t len) {
  return i2c_master_write_to_device(bl_port(), bl_addr7(), data, len,
                                    pdMS_TO_TICKS(500));
}

static esp_err_t bl_i2c_read(uint8_t *data, size_t len, int timeout_ms) {
  return i2c_master_read_from_device(bl_port(), bl_addr7(), data, len,
                                     pdMS_TO_TICKS(timeout_ms));
}

static esp_err_t bl_wait_ack(uint32_t timeout_ms) {
  const TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
  unsigned busy_cnt = 0;
  for (;;) {
    uint8_t b = 0;
    esp_err_t e = bl_i2c_read(&b, 1, 120);
    if (e != ESP_OK) {
      ESP_LOGW(TAG, "[bl] čtení ACK: %s", esp_err_to_name(e));
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
        ESP_LOGI(TAG, "[bl] čekám na dokončení (BUSY × %u)…", busy_cnt);
      }
      busy_cnt++;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    ESP_LOGW(TAG, "[bl] neočekávaný bajt 0x%02x", b);
    if (xTaskGetTickCount() >= end) {
      ESP_LOGE(TAG, "[bl] timeout ACK/BUSY");
      return ESP_ERR_TIMEOUT;
    }
  }
}

static esp_err_t bl_send_command_byte_pair(uint8_t cmd) {
  ESP_LOGI(TAG, "[bl] příkaz 0x%02X / ~0x%02X → I2C write", cmd,
           (unsigned)(uint8_t)~cmd);
  uint8_t pair[2] = {cmd, (uint8_t)~cmd};
  esp_err_t e = bl_i2c_write(pair, sizeof(pair));
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "[bl] I2C write cmd selhal: %s", esp_err_to_name(e));
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

/** GO příkaz — čip už musí být v ROM bootloaderu (bez select/release). */
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
 * WRITE MEMORY — předpoklad: cílový segment už vybrán NRST, bez uvolnění sběrnice.
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

/** Po i2c_param_config / install — výslovně zapne interní pull-up na nožičkách sběrnice. */
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
  i2c_config_t cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = CONFIG_CHESS_STM32_BL_I2C_SDA_GPIO,
      .scl_io_num = CONFIG_CHESS_STM32_BL_I2C_SCL_GPIO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = CONFIG_CHESS_STM32_BL_I2C_FREQ_HZ,
      .clk_flags = 0,
  };
  ESP_RETURN_ON_ERROR(i2c_param_config(bl_port(), &cfg), TAG,
                      "i2c_param_config");
  esp_err_t er =
      i2c_driver_install(bl_port(), I2C_MODE_MASTER, 0, 0, 0);
  if (er != ESP_OK && er != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "i2c_driver_install: %s", esp_err_to_name(er));
    return er;
  }
  bl_apply_bus_pullups(CONFIG_CHESS_STM32_BL_I2C_SDA_GPIO,
                       CONFIG_CHESS_STM32_BL_I2C_SCL_GPIO);
#else
  {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_CHESS_HALL_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_CHESS_HALL_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_CHESS_STM32_BL_SHARED_I2C_FREQ_HZ,
        .clk_flags = 0,
    };
    esp_err_t er = i2c_param_config(bl_port(), &cfg);
    if (er != ESP_OK) {
      ESP_LOGE(TAG, "shared I2C param: %s", esp_err_to_name(er));
      return er;
    }
    bl_apply_bus_pullups(CONFIG_CHESS_HALL_I2C_SDA_GPIO,
                         CONFIG_CHESS_HALL_I2C_SCL_GPIO);
    /* Driver drží hall_i2c_matrix_init(); druhý i2c_driver_install vrací ESP_FAIL. */
    ESP_LOGI(TAG,
             "STM32 BL sdílí Hall I2C port %d @ %d Hz (Hall init použil %d Hz)",
             (int)bl_port(), CONFIG_CHESS_STM32_BL_SHARED_I2C_FREQ_HZ,
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
      ESP_LOGI(TAG, "[init] NRST seg%u → GPIO%d (výchozí HIGH = run)", s, p);
    }
  }

  if (CONFIG_CHESS_STM32_BL_BOOT0_GPIO >= 0) {
    gpio_num_t bg = (gpio_num_t)CONFIG_CHESS_STM32_BL_BOOT0_GPIO;
    gpio_reset_pin(bg);
    gpio_set_direction(bg, GPIO_MODE_OUTPUT);
    gpio_set_level(bg, 0);
    ESP_LOGI(TAG, "[init] BOOT0 → GPIO%d (výchozí LOW)", CONFIG_CHESS_STM32_BL_BOOT0_GPIO);
  } else {
    ESP_LOGI(TAG, "[init] BOOT0 GPIO nepřiřazen (-1)");
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
   * STM vzorkuje BOOT0 při náběžné hře NRST. Pokud byl NRST už HIGH z init a jen
   * přepneme BOOT0, čip neprojde resetem — zůstane v uživatelském bootu (u prázdného
   * flash často „mrtvý“ stav, žádný I²C ROM bootloader). Nutný pulz NRST.
   *
   * Když AUTO_USE_BOOT0=n (BOOT0 jen hardwarově na VDD), boot0_enter_bootloader je z
   * volajícího false, ale pulz NRST pořád potřebujeme — jinak nový čip z obchodu
   * nikdy nevstoupí do system memory.
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
             "[select] NRST všech segmentů LOW (reset pulz; BOOT0=%s), 30 ms",
             boot0_enter_bootloader ? "HIGH (ESP)" : "HW/chování AUTO_USE_BOOT0=n");
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
             release ? "HIGH (uvolnění resetu)" : "LOW (reset držet)");
  }

  vTaskDelay(pdMS_TO_TICKS(CONFIG_CHESS_STM32_BL_POST_NRST_DELAY_MS));
  ESP_LOGI(TAG,
           "[select] aktivní segment %u, čekání %d ms po uvolnění NRST",
           segment_0_to_3, CONFIG_CHESS_STM32_BL_POST_NRST_DELAY_MS);
  return ESP_OK;
}

/** BOOT0 LOW; volitelný NRST pulz (BOOT0 LOW při náběžné hře) pro start aplikace z flash. */
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
           "[release] BOOT0 LOW, NRST HIGH, čekání %d ms na Hall/I2C aplikaci",
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

  err = bl_send_command_byte_pair(0x02);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[get_id] GET_ID cmd fail: %s", esp_err_to_name(err));
    goto done;
  }

  bl_inter_frame();
  uint8_t blk[5];
  memset(blk, 0, sizeof(blk));
  ESP_LOGI(TAG, "[get_id] čtu 5 B odpovědi…");
  err = bl_i2c_read(blk, sizeof(blk), 400);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[get_id] read: %s", esp_err_to_name(err));
    goto done;
  }

  ESP_LOGI(TAG, "[get_id] raw: %02X %02X %02X %02X %02X", blk[0], blk[1],
           blk[2], blk[3], blk[4]);

  if (blk[0] != STM32_ACK) {
    ESP_LOGE(TAG, "[get_id] první bajt není ACK (0x%02x)", blk[0]);
    err = ESP_ERR_INVALID_RESPONSE;
    goto done;
  }

  *pid_out = (uint16_t)(((uint16_t)blk[2] << 8) | blk[3]);
  ESP_LOGI(TAG, "[get_id] PID=0x%04X", (unsigned)*pid_out);
  if (blk[4] != STM32_ACK) {
    ESP_LOGW(TAG, "[get_id] závěrečný bajt 0x%02x (oček. ACK)", blk[4]);
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
  ESP_LOGI(TAG, "[erase_all] posílám special erase 0xFFFF…");
  err = bl_i2c_write(nblock, sizeof(nblock));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[erase_all] write: %s", esp_err_to_name(err));
    goto done;
  }

  bl_inter_frame();
  ESP_LOGI(TAG, "[erase_all] čekám na erase (timeout %d ms)…",
           CONFIG_CHESS_STM32_BL_ERASE_ACK_MS);
  err = bl_wait_ack(CONFIG_CHESS_STM32_BL_ERASE_ACK_MS);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "[erase_all] dokončeno OK");
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

done:
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
  err = bl_send_command_byte_pair(0x02);
  if (err == ESP_FAIL) {
    ESP_LOGW(TAG,
             "[flash_bin] GET_ID 1. pokus NACK/ESP_FAIL — čekám 120 ms a opakuji…");
    vTaskDelay(pdMS_TO_TICKS(120));
    err = bl_send_command_byte_pair(0x02);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG,
             "[flash_bin] GET_ID write selhal (%s) na 7-bit 0x%02x — typicky NACK: STM "
             "není v system memory nebo špatná sběrnice. Zkontroluj: kontinuitu ESP "
             "SDA/SCL (GPIO Hall) ↔ STM PB7/PB6, společnou zem, BOOT0 při NRST↑, NRST "
             "polaritu; jiný čip než C031 → adresa v AN2606 (C011=0x64).",
             esp_err_to_name(err), (unsigned)bl_addr7());
    goto restore;
  }
  bl_inter_frame();
  {
    uint8_t gid[5];
    memset(gid, 0, sizeof(gid));
    err = bl_i2c_read(gid, sizeof(gid), 400);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "[flash_bin] GET_ID read: %s", esp_err_to_name(err));
      goto restore;
    }
    ESP_LOGI(TAG,
             "[flash_bin] GET_ID raw: %02X %02X %02X %02X %02X",
             gid[0], gid[1], gid[2], gid[3], gid[4]);
    if (gid[0] != STM32_ACK) {
      ESP_LOGE(TAG,
               "[flash_bin] GET_ID: první bajt 0x%02x (oček. ACK 0x79) — špatný "
               "protokol nebo jiné zařízení na sběrnici",
               gid[0]);
      err = ESP_ERR_INVALID_RESPONSE;
      goto restore;
    }
    uint16_t pid =
        (uint16_t)(((uint16_t)gid[2] << 8) | (uint16_t)gid[3]);
    ESP_LOGI(TAG,
             "[flash_bin] PID=0x%04X — pokračuji mass erase (0x44)",
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
             "[flash_bin] mass erase: %s → zkouším page erase",
             esp_err_to_name(err));
    err = stm32_i2c_bl_select_target(segment_0_to_3, boot0_enter);
    if (err != ESP_OK) {
      goto restore;
    }

    const unsigned psize = (unsigned)CONFIG_CHESS_STM32_BL_FLASH_PAGE_SIZE;
    unsigned pages = (unsigned)((bin_len + psize - 1) / psize);
    ESP_LOGI(TAG, "[flash_bin] page erase: %u stránek à %u B", pages, psize);
    for (unsigned pg = 0; pg < pages && err == ESP_OK; pg++) {
      ESP_LOGI(TAG, "[flash_bin] mazání stránky %u / %u…", pg + 1, pages);
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
               "[flash_bin] zápis chunk %u / %u @ off=%u (%u B)",
               ci, total_chunks, (unsigned)off, (unsigned)n);
      err = fill(fill_ctx, off, chunk_buf, n);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "[flash_bin] čtení zdroje @%u: %s", (unsigned)off,
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
               "[flash_bin] GO @0x%08" PRIx32 " selhal (%s) — záložně NRST pulz",
               flash_base, esp_err_to_name(go_err));
    } else {
      ESP_LOGI(TAG, "[flash_bin] GO @0x%08" PRIx32 " OK", flash_base);
      go_ok = true;
      vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif
    ESP_LOGI(TAG, "[flash_bin] KONEC seg=%u OK", segment_0_to_3);
  } else {
    ESP_LOGE(TAG, "[flash_bin] KONEC seg=%u chyba: %s", segment_0_to_3,
             esp_err_to_name(err));
  }

  /* GO úspěšné → aplikace už běží, NRST nepulzovat. Jinak BOOT0 LOW + NRST↑. */
  (void)bl_release_run_app(err != ESP_OK || !go_ok);
  bl_resume_scan(scan_was);
  return err;
}

esp_err_t stm32_i2c_bl_flash_binary(uint8_t segment_0_to_3, bool boot0_enter,
                                    uint32_t flash_base, const uint8_t *bin,
                                    size_t bin_len) {
  ESP_RETURN_ON_FALSE(bin && bin_len, ESP_ERR_INVALID_ARG, TAG, "bin");
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

static esp_err_t stm32_bl_fill_from_partition(void *ctx, size_t off, uint8_t *dst,
                                              size_t len) {
  return esp_partition_read((const esp_partition_t *)ctx, off, dst, len);
}

/** Délka obsahu bez koncových 0xFF (čtení po blocích od konce — bez velkého malloc). */
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
  return i2c_master_write_read_device(bl_port(), addr, &reg, 1, buf,
                                      sizeof(buf), pdMS_TO_TICKS(timeout_ms));
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

/** Ověří Hall odpověď u všech segmentů, které mají NRST GPIO. */
static bool bl_probe_all_nrst_hall_segments(bool log_ok) {
  bool all_ok = true;
  bool any = false;

  for (unsigned seg = 0; seg < 4; seg++) {
    if (NRST_CFG(seg) < 0) {
      continue;
    }
    any = true;
    esp_err_t pe = bl_probe_hall_segment_retries((uint8_t)seg);
    if (pe != ESP_OK) {
      ESP_LOGW(TAG,
               "[hall_probe] seg%u addr 0x%02x neodpovídá (%s)",
               seg, bl_hall_segment_addr7(seg), esp_err_to_name(pe));
      all_ok = false;
    } else if (log_ok) {
      ESP_LOGI(TAG, "[hall_probe] seg%u addr 0x%02x OK", seg,
               bl_hall_segment_addr7(seg));
    }
  }

  if (!any) {
    ESP_LOGD(TAG, "[hall_probe] žádný NRST GPIO — přeskočeno");
    return true;
  }
  return all_ok;
}

#endif /* BL + HALL */

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
             "boot_flash_sync: timeout — STM32 auto-flash nedokončeno v čase "
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

  ESP_LOGI(TAG_A, "=== auto-flash on boot (oddíl '%s') ===",
           CONFIG_CHESS_STM32_BL_AUTO_PARTITION_LABEL);

#if CONFIG_CHESS_STM32_BL_AUTO_USE_BOOT0
  if (CONFIG_CHESS_STM32_BL_BOOT0_GPIO < 0) {
    ESP_LOGE(TAG_A,
             "AUTO_USE_BOOT0 je zapnuto, ale CHESS_STM32_BL_BOOT0_GPIO=-1. "
             "Tovární / prázdný STM na hlavní flash nespustí tvůj Hall kód ani ROM bootloader "
             "na sběrnici — bez vstupu do system memory (BOOT0=VDD při uvolnění NRST) ESP "
             "neosloví bootloader na adrese 0x%02x. Nastav v menuconfig BOOT0 GPIO a zapoj "
             "jej na pin BOOT0 MCU (nebo vypni AUTO_USE_BOOT0 a drž BOOT0 ručně jumperem).",
             (unsigned)CONFIG_CHESS_STM32_BL_TARGET_ADDR7);
    goto out;
  }
#endif

  const esp_partition_t *part = esp_partition_find_first(
      ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,
      CONFIG_CHESS_STM32_BL_AUTO_PARTITION_LABEL);
  if (part == NULL) {
    ESP_LOGW(TAG_A, "oddíl '%s' nenalezen — auto-flash přeskočen",
             CONFIG_CHESS_STM32_BL_AUTO_PARTITION_LABEL);
    goto out;
  }

  ESP_LOGI(TAG_A, "oddíl: offset=0x%08" PRIx32 " size=%" PRIu32 " B",
           (uint32_t)part->address, (uint32_t)part->size);

  size_t eff = 0;
  esp_err_t tre = auto_part_effective_bytes(part, &eff);
  if (tre != ESP_OK) {
    ESP_LOGE(TAG_A, "ořez konce oddílu: %s", esp_err_to_name(tre));
    goto out;
  }

  if (eff < 32) {
    ESP_LOGW(TAG_A, "image z oddílu je prázdná nebo jen 0xFF (eff=%u B)",
             (unsigned)eff);
    goto out;
  }

  if (CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES > 0 &&
      eff > (size_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES) {
    eff = (size_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_MAX_BYTES;
  }

  uint32_t crc = 0;
  esp_err_t cre = auto_part_crc32(part, eff, &crc);
  if (cre != ESP_OK) {
    ESP_LOGE(TAG_A, "CRC z oddílu: %s", esp_err_to_name(cre));
    goto out;
  }

  ESP_LOGI(TAG_A,
           "image po ořezu konce 0xFF: %" PRIu32 " B, CRC32=0x%08" PRIx32
           " (čtení po blocích, bez malloc %" PRIu32 ")",
           (uint32_t)eff, crc, (uint32_t)part->size);

#if CONFIG_CHESS_STM32_BL_AUTO_FLASH_SKIP_IF_UNCHANGED
  /* FORCE v sdkconfig často chybí jako makro (# is not set) — nepoužívat přímo !CONFIG_FORCE */
#if !(defined(CONFIG_CHESS_STM32_BL_AUTO_FLASH_FORCE) && CONFIG_CHESS_STM32_BL_AUTO_FLASH_FORCE)
  {
    nvs_handle_t h;
    if (nvs_open("stm32_bl", NVS_READONLY, &h) == ESP_OK) {
      uint32_t oc = 0;
      uint32_t ol = 0;
      esp_err_t ge1 = nvs_get_u32(h, "af_crc", &oc);
      esp_err_t ge2 = nvs_get_u32(h, "af_len", &ol);
      nvs_close(h);
      if (ge1 == ESP_OK && ge2 == ESP_OK && oc == crc &&
          ol == (uint32_t)eff) {
#if CONFIG_CHESS_STM32_BL_REFLASH_IF_HALL_MISSING &&                          \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        if (!bl_probe_all_nrst_hall_segments(false)) {
          ESP_LOGW(TAG_A,
                   "NVS říká stejný image, ale Hall neodpovídá — "
                   "vynucuji auto-flash (výměna STM / prázdný čip?)");
        } else
#endif
        {
          ESP_LOGI(TAG_A,
                   "auto-flash přeskočen (stejný obsah CRC=0x%08" PRIx32
                   ", %" PRIu32 " B)",
                   crc, (uint32_t)eff);
#if CONFIG_CHESS_STM32_BL_VERIFY_HALL_AFTER_FLASH &&                            \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
          (void)bl_probe_all_nrst_hall_segments(true);
#endif
          goto out;
        }
      }
    }
  }
#endif
#endif

  bool use_boot = CONFIG_CHESS_STM32_BL_AUTO_USE_BOOT0 &&
                  CONFIG_CHESS_STM32_BL_BOOT0_GPIO >= 0;
  uint32_t base = (uint32_t)CONFIG_CHESS_STM32_BL_AUTO_FLASH_BASE;
  ESP_LOGI(TAG_A,
           "parametry: FLASH_BASE=0x%08" PRIx32 " USE_BOOT0=%d "
           "(BOOT0 GPIO=%d)",
           base, use_boot ? 1 : 0, CONFIG_CHESS_STM32_BL_BOOT0_GPIO);

  bool any_nrst = false;
  bool all_ok = true;
  for (unsigned seg = 0; seg < 4; seg++) {
    int nrst = seg == 0   ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0
               : seg == 1 ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1
               : seg == 2 ? CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2
                          : CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3;
    if (nrst < 0) {
      continue;
    }
    any_nrst = true;
    ESP_LOGI(TAG_A, "segment %u: nahrávám %" PRIu32 " B → STM @0x%08" PRIx32,
             seg, (uint32_t)eff, base);
    esp_err_t fe = stm32_i2c_bl_flash_binary_impl(
        (uint8_t)seg, use_boot, base, eff, stm32_bl_fill_from_partition,
        (void *)part);
    if (fe != ESP_OK) {
      ESP_LOGE(TAG_A, "segment %u: %s", seg, esp_err_to_name(fe));
      all_ok = false;
    } else {
      ESP_LOGI(TAG_A, "segment %u: hotovo", seg);
    }
  }

  if (!any_nrst) {
    ESP_LOGW(TAG_A,
             "žádný NRST GPIO — nastav CHESS_STM32_BL_NRST_GPIO_SEG* v menuconfig");
  }

  if (all_ok && any_nrst) {
    bool hall_ok = true;
#if CONFIG_CHESS_STM32_BL_VERIFY_HALL_AFTER_FLASH &&                          \
    CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    hall_ok = bl_probe_all_nrst_hall_segments(true);
    if (!hall_ok) {
      ESP_LOGE(TAG_A,
               "auto-flash zápis OK, ale Hall neodpovídá — NVS se neuloží, "
               "další boot zkusí flash znovu (NRST/BOOT0/I2C PB6/PB7)");
    }
#endif
    if (hall_ok) {
      nvs_handle_t hw;
      if (nvs_open("stm32_bl", NVS_READWRITE, &hw) == ESP_OK) {
        nvs_set_u32(hw, "af_crc", crc);
        nvs_set_u32(hw, "af_len", (uint32_t)eff);
        nvs_commit(hw);
        nvs_close(hw);
        ESP_LOGI(TAG_A,
                 "NVS: uložen CRC+délka (další boot přeskočí při stejném obsahu)");
      }
    }
  } else if (any_nrst && !all_ok) {
    ESP_LOGE(TAG_A,
             "auto-flash selhal na alespoň jednom segmentu — Hall čtení "
             "nebude fungovat");
  }

out:
  stm32_i2c_bl_boot_flash_sync_signal_done();

#endif /* AUTO_FLASH_ON_BOOT && BL */
}
