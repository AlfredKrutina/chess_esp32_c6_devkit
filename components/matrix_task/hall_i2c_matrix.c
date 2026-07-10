#include "hall_i2c_matrix.h"
#include "hall_i2c_spec.h"
#include "sdkconfig.h"
#include <string.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "HALL_I2C";

#if CONFIG_CHESS_HALL_REG_START != HALL_I2C_REG_POINTER_RAW
#warning "CHESS_HALL_REG_START differs from hall_i2c_spec.h HALL_I2C_REG_POINTER_RAW"
#endif

static bool s_i2c_ready;
/** Bit i = segment i už dostal jednorázové ESP_LOGW při selhání čtení (reset při úspěchu). */
static uint8_t s_read_fail_warned_mask;

/** Explicitní interní pull-up na pinech sběrnice (doplňuje i2c_config_t). */
static void hall_i2c_apply_bus_pullups(void) {
  gpio_num_t sda = (gpio_num_t)CONFIG_CHESS_HALL_I2C_SDA_GPIO;
  gpio_num_t scl = (gpio_num_t)CONFIG_CHESS_HALL_I2C_SCL_GPIO;
  esp_err_t e1 = gpio_set_pull_mode(sda, GPIO_PULLUP_ONLY);
  esp_err_t e2 = gpio_set_pull_mode(scl, GPIO_PULLUP_ONLY);
  if (e1 != ESP_OK || e2 != ESP_OK) {
    ESP_LOGW(TAG, "pull-up SDA/SCL: %s / %s", esp_err_to_name(e1),
             esp_err_to_name(e2));
  } else {
    ESP_LOGI(TAG, "SDA=%d SCL=%d: GPIO_PULLUP_ONLY (ESP interní)", (int)sda,
             (int)scl);
  }
}

/** Mapování: segment 0 = DESKA u ESP (a–d × řady 1–4), dál po CCW jako čtvrtky šachovnice. */
static uint8_t hall_map_segment_field_to_square(unsigned seg, unsigned field) {
  unsigned lr = field / 4;
  unsigned lc = field % 4;
  switch (seg) {
  case 0:
    return (uint8_t)(lr * 8 + lc);
  case 1:
    return (uint8_t)(lr * 8 + (4 + lc));
  case 2:
    return (uint8_t)((4 + lr) * 8 + (4 + lc));
  case 3:
    return (uint8_t)((4 + lr) * 8 + lc);
  default:
    return 0;
  }
}

static bool hall_pair_occupied(uint16_t r0, uint16_t r1) {
#if CONFIG_CHESS_HALL_DETECT_DIFF
  int d = (int)r0 - (int)r1;
  if (d < 0) {
    d = -d;
  }
  return d >= CONFIG_CHESS_HALL_PAIR_DIFF_THRESHOLD;
#else
  uint16_t m = r0 > r1 ? r0 : r1;
  return m >= CONFIG_CHESS_HALL_MAX_THRESHOLD;
#endif
}

static uint8_t segment_addr(unsigned seg) {
  switch (seg) {
  case 0:
    return CONFIG_CHESS_HALL_SEG0_ADDR;
  case 1:
    return CONFIG_CHESS_HALL_SEG1_ADDR;
  case 2:
    return CONFIG_CHESS_HALL_SEG2_ADDR;
  case 3:
    return CONFIG_CHESS_HALL_SEG3_ADDR;
  default:
    return CONFIG_CHESS_HALL_SEG0_ADDR;
  }
}

esp_err_t hall_i2c_matrix_init(void) {
  i2c_port_t port = (i2c_port_t)CONFIG_CHESS_HALL_I2C_PORT_NUM;

  i2c_config_t cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = CONFIG_CHESS_HALL_I2C_SDA_GPIO,
      .scl_io_num = CONFIG_CHESS_HALL_I2C_SCL_GPIO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = CONFIG_CHESS_HALL_I2C_FREQ_HZ,
      .clk_flags = 0,
  };

  esp_err_t err = i2c_param_config(port, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_param_config: %s", esp_err_to_name(err));
    return err;
  }

  err = i2c_driver_install(port, cfg.mode, 0, 0, 0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "i2c_driver_install: %s", esp_err_to_name(err));
    return err;
  }

  hall_i2c_apply_bus_pullups();

  s_i2c_ready = true;
  ESP_LOGI(TAG,
           "I2C Hall ready port=%d SDA=%d SCL=%d %d Hz segy 0x%02x 0x%02x 0x%02x 0x%02x",
           (int)port, CONFIG_CHESS_HALL_I2C_SDA_GPIO,
           CONFIG_CHESS_HALL_I2C_SCL_GPIO, CONFIG_CHESS_HALL_I2C_FREQ_HZ,
           CONFIG_CHESS_HALL_SEG0_ADDR, CONFIG_CHESS_HALL_SEG1_ADDR,
           CONFIG_CHESS_HALL_SEG2_ADDR, CONFIG_CHESS_HALL_SEG3_ADDR);
  return ESP_OK;
}

esp_err_t hall_i2c_matrix_probe_segment(uint8_t segment_0_to_3, int timeout_ms) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (!s_i2c_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  if (segment_0_to_3 > 3u) {
    return ESP_ERR_INVALID_ARG;
  }
  if (timeout_ms <= 0) {
    timeout_ms = 80;
  }

  uint8_t addr = segment_addr(segment_0_to_3);
  uint8_t reg = (uint8_t)CONFIG_CHESS_HALL_REG_START;
  uint8_t buf[2];
  esp_err_t err = i2c_master_write_read_device(
      (i2c_port_t)CONFIG_CHESS_HALL_I2C_PORT_NUM, addr, &reg, 1, buf,
      sizeof(buf), pdMS_TO_TICKS(timeout_ms));
  if (err != ESP_OK) {
    ESP_LOGD(TAG, "probe seg%u addr 0x%02x: %s", segment_0_to_3, addr,
             esp_err_to_name(err));
  }
  return err;
#else
  (void)segment_0_to_3;
  (void)timeout_ms;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

void hall_i2c_matrix_fill_state(uint8_t matrix_state[64]) {
  if (!s_i2c_ready || matrix_state == NULL) {
    return;
  }

  i2c_port_t port = (i2c_port_t)CONFIG_CHESS_HALL_I2C_PORT_NUM;
  uint8_t reg = (uint8_t)CONFIG_CHESS_HALL_REG_START;
  uint8_t buf[HALL_I2C_PAYLOAD_BYTES];
  static uint32_t s_scan;

  s_scan++;

#if CONFIG_CHESS_HALL_LOG_INTERVAL_SCANS > 0
  bool do_log = (s_scan % (uint32_t)CONFIG_CHESS_HALL_LOG_INTERVAL_SCANS) == 1;
#else
  bool do_log = false;
#endif

  /* Po přidání Kconfig položky může starý sdkconfig makro nemít — výchozí 4 segmenty. */
#if defined(CONFIG_CHESS_HALL_SEGMENT_COUNT)
  unsigned seg_max = (unsigned)CONFIG_CHESS_HALL_SEGMENT_COUNT;
#else
  unsigned seg_max = 4u;
#endif
  if (seg_max > 4u) {
    seg_max = 4u;
  }
  if (seg_max < 1u) {
    seg_max = 1u;
  }

  for (unsigned seg = seg_max; seg < 4u; seg++) {
    for (unsigned field = 0; field < HALL_I2C_FIELDS_PER_SEGMENT; field++) {
      uint8_t sq = hall_map_segment_field_to_square(seg, field);
      matrix_state[sq] = 0;
    }
  }

  for (unsigned seg = 0; seg < seg_max; seg++) {
    uint8_t addr = segment_addr(seg);
    esp_err_t err = i2c_master_write_read_device(
        port, addr, &reg, 1, buf, sizeof(buf), pdMS_TO_TICKS(80));

    if (err != ESP_OK) {
      uint8_t bit = (uint8_t)(1u << seg);
      if ((s_read_fail_warned_mask & bit) == 0) {
        ESP_LOGW(TAG,
                 "segment %u addr 0x%02x read failed: %s "
                 "(další selhání stejného segmentu jen DEBUG)",
                 seg, addr, esp_err_to_name(err));
        s_read_fail_warned_mask |= bit;
      } else {
        ESP_LOGD(TAG, "segment %u addr 0x%02x read failed: %s", seg, addr,
                 esp_err_to_name(err));
      }
      continue;
    }

    s_read_fail_warned_mask &= (uint8_t)~((uint8_t)(1u << seg));

    for (unsigned field = 0; field < HALL_I2C_FIELDS_PER_SEGMENT; field++) {
      unsigned off = field * sizeof(uint16_t) * HALL_I2C_SENSORS_PER_FIELD;
      uint16_t r0 = hall_i2c_spec_read_le16(&buf[off]);
      uint16_t r1 = hall_i2c_spec_read_le16(&buf[off + sizeof(uint16_t)]);
      uint8_t sq = hall_map_segment_field_to_square(seg, field);
      matrix_state[sq] = hall_pair_occupied(r0, r1) ? 1 : 0;
    }

    if (do_log) {
      ESP_LOGI(TAG,
               "[staging] seg%u addr=0x%02x first_pair raw=(%u,%u) occ=%u",
               seg, addr, hall_i2c_spec_read_le16(&buf[0]),
               hall_i2c_spec_read_le16(&buf[sizeof(uint16_t)]),
               (unsigned)matrix_state[hall_map_segment_field_to_square(seg, 0)]);
    }
  }
}

#else /* !CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

esp_err_t hall_i2c_matrix_init(void) { return ESP_OK; }

esp_err_t hall_i2c_matrix_probe_segment(uint8_t segment_0_to_3, int timeout_ms) {
  (void)segment_0_to_3;
  (void)timeout_ms;
  return ESP_ERR_NOT_SUPPORTED;
}

void hall_i2c_matrix_fill_state(uint8_t matrix_state[64]) {
  if (matrix_state) {
    memset(matrix_state, 0, 64);
  }
}

#endif
