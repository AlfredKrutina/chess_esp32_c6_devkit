#include "hall_i2c_squares.h"
#include "hall_i2c_spec.h"
#include "freertos_chess.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "led_board_restore_guide.h"
#include "led_mapping.h"
#include "led_task.h"
#include "stm32_i2c_bl.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "HALL_I2C";

#if CONFIG_CHESS_HALL_REG_START != HALL_I2C_REG_POINTER_RAW
#warning "CHESS_HALL_REG_START differs from hall_i2c_spec.h HALL_I2C_REG_POINTER_RAW"
#endif

static bool s_i2c_ready;
/** Bit i = segment i has already received a one-time ESP_LOGW on read failure (reset on success). */
static uint8_t s_read_fail_warned_mask;

/* ========================================================================
 * POC algorithm constants — copied from HAL_czechmate_POC/main/main.c.
 * Names are kept verbose so the origin is obvious.
 * ======================================================================== */

/** Sum of |dev| that latches a piece. Empty-board noise sits just under this. */
#define HALL_POC_DELTA_ON               270
/** Piece stays until the sum of |dev| falls under this. */
#define HALL_POC_DELTA_OFF              160
/** Scans the peak energy must stop rising before the type is named. */
#define HALL_POC_SETTLE_SCANS           25
/** Energy below the weakest real piece (pawn, measured 51k–67k) is not a type. */
#define HALL_POC_ENERGY_DEADBAND        15000

/** POC: quick samples per decision (trimmed mean of middle 3 after sort).
 *  V2 keeps the last N raw I2C pairs per square (one pair per matrix scan). */
#define HALL_POC_SAMPLES_PER_CYCLE      5

/** POC: delay between the quick samples above (ms).  In V2 the sample pair
 *  arrives as one I2C payload, so this is a traceability constant only. */
#define HALL_POC_SAMPLE_DELAY_MS        2

/** POC: main loop period in the single-square firmware (10 Hz). */
#define HALL_POC_CYCLE_DELAY_MS         100

/** POC: number of consecutive identical raw classifications required
 *  before the stable result changes. */
#define HALL_POC_DEBOUNCE_TICKS         3

/** V2 matrix scan period — must stay aligned with TOTAL_CYCLE_TIME_MS. */
#define HALL_POC_V2_SCAN_PERIOD_MS      TOTAL_CYCLE_TIME_MS

/** V2 debounce scan counter equivalent to POC DEBOUNCE_TICKS * CYCLE_DELAY_MS. */
#define HALL_POC_DEBOUNCE_SCANS         ((HALL_POC_DEBOUNCE_TICKS * HALL_POC_CYCLE_DELAY_MS) / HALL_POC_V2_SCAN_PERIOD_MS)

/** POC: auto-zero only starts after this many empty scans.
 *  POC value 600 ticks * 100 ms = 60 s; scaled to V2 scan period. */
#define HALL_POC_AUTOZERO_EMPTY_SCANS   ((600 * HALL_POC_CYCLE_DELAY_MS) / HALL_POC_V2_SCAN_PERIOD_MS)

/** POC: auto-zero baseline bump interval.
 *  POC value 100 ticks * 100 ms = 10 s; scaled to V2 scan period. */
#define HALL_POC_AUTOZERO_BUMP_SCANS    ((100 * HALL_POC_CYCLE_DELAY_MS) / HALL_POC_V2_SCAN_PERIOD_MS)

/** POC: auto-zero only runs when both deviations are very small (< 15). */
#define HALL_POC_AUTOZERO_DEV_LIMIT     15

/** POC: 20 % hysteresis margin for the currently locked piece type. */
#define HALL_POC_HYSTERESIS_PERCENT     20

/** Boot calibration: POC averages 20 ADC reads per channel (5 ms apart);
 *  V2 uses one I2C pair per scan → 20 scans (~500 ms at 25 ms) before lock. */
#define HALL_POC_BOOT_CAL_SCANS         20

/** Sentinel for raw ADC values that carry no real Hall data (Nucleo mock). */
#define HALL_POC_NO_DATA_RAW            0u

typedef struct {
    int32_t min_energy;
    int32_t max_energy;
    hall_piece_type_t type;
} hall_poc_threshold_t;

/* Raw s0^2+s1^2 from this board. Pawn 51k–67k, the next magnet 114k–182k,
 * the one above that settles 300k–382k. */
static const hall_poc_threshold_t s_poc_thresholds[] = {
    { 15000,    90000,    HALL_PIECE_TYPE_PAWN   },
    { 90001,    240000,   HALL_PIECE_TYPE_KNIGHT },
    { 240001,   520000,   HALL_PIECE_TYPE_BISHOP },
    { 520001,   800000,   HALL_PIECE_TYPE_ROOK   },
    { 800001,   1200000,  HALL_PIECE_TYPE_QUEEN  },
    { 1200001,  8000000,  HALL_PIECE_TYPE_KING   },
};
#define HALL_POC_THRESHOLD_COUNT (sizeof(s_poc_thresholds) / sizeof(s_poc_thresholds[0]))

typedef struct {
    int32_t midpoint[2];              /**< Per-sensor baseline, auto-zeroed. */
    hall_piece_type_t current_piece;
    hall_piece_color_t current_color;
    hall_piece_type_t pending_piece;
    hall_piece_color_t pending_color;
    int stable_ticks;               /**< Consecutive scans with the same raw result. */
    int empty_ticks;                /**< Consecutive scans eligible for auto-zero. */
    int32_t cal_sum[2];             /**< Boot calibration accumulator. */
    uint8_t cal_count;              /**< Boot calibration sample count. */
    bool initialized;               /**< True after first baseline is set. */
    uint16_t ring_r0[HALL_POC_SAMPLES_PER_CYCLE];
    uint16_t ring_r1[HALL_POC_SAMPLES_PER_CYCLE];
    uint8_t ring_count;             /**< Raw pairs collected (0..5) for trimmed mean. */
    bool confirmed_empty;           /**< Midpoint captured while the square was empty. */
    bool provisional;               /**< Midpoint copied from other empty squares. */
    bool ever_locked;               /**< Debounce has accepted one result since baseline. */
    bool occupied;                  /**< Latched by |dev| sum, before the type is named. */
    int32_t peak_energy;            /**< Highest energy since the piece arrived. */
    int16_t peak_dev0;
    int16_t peak_dev1;
    uint8_t settle_scans;           /**< Scans since the peak last rose. */
    int32_t live_energy;            /**< Last settled-candidate energy. */
    int32_t live_dev0;
    int32_t live_dev1;
    bool live_valid;
} hall_poc_square_state_t;

/** Per-square state — hysteresis and debounce are NOT global across the board. */
static hall_poc_square_state_t s_poc_state[64];
/** Magnet bench: -1 = baseline may drift, else that square stays frozen. */
static int s_mag_freeze_sq = -1;
static bool s_mag_on;
static uint8_t s_mag_sq = 255;
static esp_log_level_t s_mag_log_was = ESP_LOG_INFO;
static bool s_mag_log_saved;
static void hall_mag_light_locked(void);

/** 1 = logical game says this square is empty and may be zeroed from live ADC. */
static uint8_t s_logical_empty[64];
static bool s_logical_known;
static bool s_baseline_dirty;
static bool s_baseline_need_soon;
static uint32_t s_baseline_saved_ms;
static uint32_t s_baseline_seq;
static uint8_t s_baseline_slot; /* 0 none, 1 base_a, 2 base_b */

#define HALL_BASE_VERSION 2
#define HALL_BASE_NVS_NS "hall_cal"
#define HALL_BASE_KEY_A "base_a"
#define HALL_BASE_KEY_B "base_b"
#define HALL_BASE_SAVE_SOON_MS 2000u
#define HALL_BASE_SAVE_DRIFT_MS 600000u
#define HALL_BASE_MIN_CONFIRMED 4

#pragma pack(push, 1)
typedef struct {
  uint32_t version;
  uint32_t crc32;
  uint32_t seq;
  uint8_t valid[8];
  int16_t mid0[64];
  int16_t mid1[64];
} hall_baseline_blob_t;
#pragma pack(pop)

/** Explicit internal pull-up on bus pins (complements i2c_master bus). */
static void hall_i2c_apply_bus_pullups(void) {
  gpio_num_t sda = (gpio_num_t)CONFIG_CHESS_HALL_I2C_SDA_GPIO;
  gpio_num_t scl = (gpio_num_t)CONFIG_CHESS_HALL_I2C_SCL_GPIO;
  esp_err_t e1 = gpio_set_pull_mode(sda, GPIO_PULLUP_ONLY);
  esp_err_t e2 = gpio_set_pull_mode(scl, GPIO_PULLUP_ONLY);
  if (e1 != ESP_OK || e2 != ESP_OK) {
    ESP_LOGW(TAG, "pull-up SDA/SCL: %s / %s", esp_err_to_name(e1),
             esp_err_to_name(e2));
  } else {
    ESP_LOGI(TAG, "SDA=%d SCL=%d: GPIO_PULLUP_ONLY (ESP internal)", (int)sda,
             (int)scl);
  }
}

/* Native STM field = both halls of one square (U6 I0+I1 is field 0).
 * Quadrants from the PCB 4x4 and the squares that already lit correctly. */
static const uint8_t s_hall_square[4][16] = {
    {63, 62, 61, 60, 55, 54, 53, 52, 47, 46, 45, 44, 39, 38, 37, 36},
    {56, 48, 40, 32, 57, 49, 41, 33, 58, 50, 42, 34, 59, 51, 43, 35},
    {0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27},
    {7, 15, 23, 31, 6, 14, 22, 30, 5, 13, 21, 29, 4, 12, 20, 28},
};

static uint8_t hall_map_segment_field_to_square(unsigned seg, unsigned field) {
  if (seg > 3u || field > 15u) {
    return 0xFFu;
  }
  return s_hall_square[seg][field];
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

static void hall_poc_sort_5(int arr[5]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

/** POC alpha-trimmed mean: sort 5 values, average the middle 3. */
static int hall_poc_trimmed_mean_u16(const uint16_t samples[5]) {
    int arr[5];
    for (int i = 0; i < HALL_POC_SAMPLES_PER_CYCLE; i++) {
        arr[i] = (int)samples[i];
    }
    hall_poc_sort_5(arr);
    return (arr[1] + arr[2] + arr[3]) / 3;
}

static void hall_poc_ring_push(hall_poc_square_state_t *st, uint16_t r0, uint16_t r1) {
    if (st->ring_count < HALL_POC_SAMPLES_PER_CYCLE) {
        st->ring_r0[st->ring_count] = r0;
        st->ring_r1[st->ring_count] = r1;
        st->ring_count++;
        return;
    }
    memmove(st->ring_r0, &st->ring_r0[1],
            (HALL_POC_SAMPLES_PER_CYCLE - 1) * sizeof(st->ring_r0[0]));
    memmove(st->ring_r1, &st->ring_r1[1],
            (HALL_POC_SAMPLES_PER_CYCLE - 1) * sizeof(st->ring_r1[0]));
    st->ring_r0[HALL_POC_SAMPLES_PER_CYCLE - 1] = r0;
    st->ring_r1[HALL_POC_SAMPLES_PER_CYCLE - 1] = r1;
}

/**
 * POC classification with true hysteresis for the currently locked piece.
 * Returns a pointer into s_poc_thresholds or NULL if energy is below the
 * empty deadband or outside every threshold.
 */
static const hall_poc_threshold_t* hall_poc_classify(int32_t energy,
                                                     hall_piece_type_t locked_piece) {
    if (energy < HALL_POC_ENERGY_DEADBAND) {
        return NULL;
    }

    /* KROK 1: keep the locked piece inside a 20 % expanded window so small
     * drift near the boundary does not flicker the type. */
    if (locked_piece != HALL_PIECE_TYPE_NONE) {
        for (int i = 0; i < HALL_POC_THRESHOLD_COUNT; i++) {
            if (s_poc_thresholds[i].type == locked_piece) {
                int32_t margin_min = s_poc_thresholds[i].min_energy -
                                     (s_poc_thresholds[i].min_energy / (100 / HALL_POC_HYSTERESIS_PERCENT));
                int32_t margin_max = s_poc_thresholds[i].max_energy +
                                     (s_poc_thresholds[i].max_energy / (100 / HALL_POC_HYSTERESIS_PERCENT));
                if (energy >= margin_min && energy <= margin_max) {
                    return &s_poc_thresholds[i];
                }
                break;
            }
        }
    }

    /* KROK 2: strict lookup when the piece changed or no piece was locked. */
    for (int i = 0; i < HALL_POC_THRESHOLD_COUNT; i++) {
        if (energy >= s_poc_thresholds[i].min_energy &&
            energy <= s_poc_thresholds[i].max_energy) {
            return &s_poc_thresholds[i];
        }
    }
    return NULL;
}

static uint32_t hall_crc32(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261u;
  if (data == NULL) {
    return h;
  }
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

static bool hall_baseline_blob_ok(const hall_baseline_blob_t *blob, size_t len) {
  if (blob == NULL || len != sizeof(*blob) || blob->version != HALL_BASE_VERSION) {
    return false;
  }
  uint32_t crc = hall_crc32(((const uint8_t *)blob) + 8, sizeof(*blob) - 8);
  return crc == blob->crc32;
}

static int hall_baseline_popcount(const hall_baseline_blob_t *blob) {
  int n = 0;
  for (int i = 0; i < 8; i++) {
    uint8_t v = blob->valid[i];
    while (v != 0) {
      n += (int)(v & 1u);
      v >>= 1;
    }
  }
  return n;
}

static bool hall_baseline_load_key(const char *key, hall_baseline_blob_t *out) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(HALL_BASE_NVS_NS, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    return false;
  }
  size_t len = sizeof(*out);
  err = nvs_get_blob(handle, key, out, &len);
  nvs_close(handle);
  return err == ESP_OK && hall_baseline_blob_ok(out, len) &&
         hall_baseline_popcount(out) > 0;
}

static void hall_baseline_apply(const hall_baseline_blob_t *blob) {
  for (int sq = 0; sq < 64; sq++) {
    if ((blob->valid[sq / 8] & (uint8_t)(1u << (sq % 8))) == 0) {
      continue;
    }
    hall_poc_square_state_t *st = &s_poc_state[sq];
    st->midpoint[0] = blob->mid0[sq];
    st->midpoint[1] = blob->mid1[sq];
    st->initialized = true;
    st->confirmed_empty = true;
    st->provisional = false;
  }
}

static void hall_baseline_load(void) {
  hall_baseline_blob_t a = {0};
  hall_baseline_blob_t b = {0};
  bool have_a = hall_baseline_load_key(HALL_BASE_KEY_A, &a);
  bool have_b = hall_baseline_load_key(HALL_BASE_KEY_B, &b);
  const hall_baseline_blob_t *best = NULL;
  if (have_a && have_b) {
    best = (b.seq > a.seq) ? &b : &a;
    s_baseline_slot = (best == &b) ? 2u : 1u;
  } else if (have_a) {
    best = &a;
    s_baseline_slot = 1u;
  } else if (have_b) {
    best = &b;
    s_baseline_slot = 2u;
  }
  if (best == NULL) {
    ESP_LOGI(TAG, "Hall baseline: none stored — will not zero occupied squares");
    return;
  }
  hall_baseline_apply(best);
  s_baseline_seq = best->seq;
  ESP_LOGI(TAG, "Hall baseline restored seq=%lu squares=%d slot=%u",
           (unsigned long)best->seq, hall_baseline_popcount(best),
           (unsigned)s_baseline_slot);
}

static void hall_baseline_flush(void) {
  if (!s_baseline_dirty) {
    return;
  }
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
  uint32_t gap = s_baseline_need_soon ? HALL_BASE_SAVE_SOON_MS : HALL_BASE_SAVE_DRIFT_MS;
  if (s_baseline_saved_ms != 0 && (now - s_baseline_saved_ms) < gap) {
    return;
  }

  hall_baseline_blob_t blob = {0};
  blob.version = HALL_BASE_VERSION;
  blob.seq = s_baseline_seq + 1u;
  int n = 0;
  for (int sq = 0; sq < 64; sq++) {
    if (!s_poc_state[sq].confirmed_empty) {
      continue;
    }
    blob.valid[sq / 8] |= (uint8_t)(1u << (sq % 8));
    blob.mid0[sq] = (int16_t)s_poc_state[sq].midpoint[0];
    blob.mid1[sq] = (int16_t)s_poc_state[sq].midpoint[1];
    n++;
  }
  if (n == 0) {
    s_baseline_dirty = false;
    s_baseline_need_soon = false;
    return;
  }
  blob.crc32 = hall_crc32(((const uint8_t *)&blob) + 8, sizeof(blob) - 8);

  const char *key = (s_baseline_slot == 1u) ? HALL_BASE_KEY_B : HALL_BASE_KEY_A;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(HALL_BASE_NVS_NS, NVS_READWRITE, &handle);
  if (err == ESP_OK) {
    err = nvs_set_blob(handle, key, &blob, sizeof(blob));
    if (err == ESP_OK) {
      err = nvs_commit(handle);
    }
    nvs_close(handle);
  }
  s_baseline_saved_ms = now;
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Hall baseline save %s failed: %s", key, esp_err_to_name(err));
    return;
  }
  s_baseline_seq = blob.seq;
  s_baseline_slot = (s_baseline_slot == 1u) ? 2u : 1u;
  s_baseline_dirty = false;
  s_baseline_need_soon = false;
  ESP_LOGI(TAG, "Hall baseline saved seq=%lu squares=%d key=%s",
           (unsigned long)blob.seq, n, key);
}

static void hall_sort_i32(int32_t *arr, int n) {
  for (int i = 1; i < n; i++) {
    int32_t v = arr[i];
    int j = i;
    while (j > 0 && arr[j - 1] > v) {
      arr[j] = arr[j - 1];
      j--;
    }
    arr[j] = v;
  }
}

/** Occupied square: copy the median of squares whose zero was taken while empty. */
static bool hall_assign_provisional(hall_poc_square_state_t *st) {
  int32_t a0[64];
  int32_t a1[64];
  int n = 0;
  for (int i = 0; i < 64; i++) {
    if (!s_poc_state[i].confirmed_empty) {
      continue;
    }
    a0[n] = s_poc_state[i].midpoint[0];
    a1[n] = s_poc_state[i].midpoint[1];
    n++;
  }
  if (n < HALL_BASE_MIN_CONFIRMED) {
    return false;
  }
  hall_sort_i32(a0, n);
  hall_sort_i32(a1, n);
  st->midpoint[0] = a0[n / 2];
  st->midpoint[1] = a1[n / 2];
  st->initialized = true;
  st->provisional = true;
  st->confirmed_empty = false;
  return true;
}

void hall_i2c_squares_set_logical_empty(const uint8_t empty_squares[64]) {
  if (empty_squares == NULL) {
    return;
  }
  memcpy(s_logical_empty, empty_squares, sizeof(s_logical_empty));
  s_logical_known = true;
  for (int sq = 0; sq < 64; sq++) {
    hall_poc_square_state_t *st = &s_poc_state[sq];
    if (empty_squares[sq] == 0 || !st->provisional || st->confirmed_empty) {
      continue;
    }
    /* This square is now supposed to be empty. Drop the borrowed zero and
     * measure it, instead of keeping a neighbor's midpoint. */
    st->initialized = false;
    st->provisional = false;
    st->cal_count = 0;
    st->cal_sum[0] = 0;
    st->cal_sum[1] = 0;
    st->ever_locked = false;
    st->ring_count = 0;
    st->stable_ticks = 0;
    st->current_piece = HALL_PIECE_TYPE_NONE;
    st->current_color = HALL_PIECE_COLOR_NONE;
    st->occupied = false;
    st->peak_energy = 0;
    st->settle_scans = 0;
  }
}

bool hall_i2c_squares_readings_usable(void) {
  if (!s_i2c_ready || !s_logical_known) {
    return false;
  }
  for (int sq = 0; sq < 64; sq++) {
    const hall_poc_square_state_t *st = &s_poc_state[sq];
    if (!st->initialized || !st->ever_locked ||
        st->ring_count < HALL_POC_SAMPLES_PER_CYCLE) {
      return false;
    }
  }
  return true;
}

/**
 * Run one POC state-machine step for a single square.
 * @param sq        square index 0..63
 * @param r0, r1    raw 12-bit ADC pair from the STM32 slave
 * @return          stable occupancy bit (1 = occupied, 0 = empty)
 */
static uint8_t hall_poc_update_square(uint8_t sq, uint16_t r0, uint16_t r1) {
    hall_poc_square_state_t *st = &s_poc_state[sq];

    /* Nucleo / no-real-data guard: a zero-filled payload is not a Hall sample. */
    if (r0 == HALL_POC_NO_DATA_RAW && r1 == HALL_POC_NO_DATA_RAW) {
        st->current_piece = HALL_PIECE_TYPE_NONE;
        st->current_color = HALL_PIECE_COLOR_NONE;
        st->pending_piece = HALL_PIECE_TYPE_NONE;
        st->pending_color = HALL_PIECE_COLOR_NONE;
        st->stable_ticks = 0;
        st->occupied = false;
        st->peak_energy = 0;
        st->settle_scans = 0;
        return 0;
    }

    /* Zero only from a stored empty baseline, or from squares the logical
     * game says are empty. Never treat "whatever is on the board" as empty. */
    if (!st->initialized) {
        bool allow_empty_cal = s_logical_known && s_logical_empty[sq] != 0;
        if (allow_empty_cal) {
            if (st->cal_count < HALL_POC_BOOT_CAL_SCANS) {
                st->cal_sum[0] += r0;
                st->cal_sum[1] += r1;
                st->cal_count++;
                if (st->cal_count < HALL_POC_BOOT_CAL_SCANS) {
                    return 0;
                }
                st->midpoint[0] = st->cal_sum[0] / HALL_POC_BOOT_CAL_SCANS;
                st->midpoint[1] = st->cal_sum[1] / HALL_POC_BOOT_CAL_SCANS;
                st->initialized = true;
                st->confirmed_empty = true;
                st->provisional = false;
                s_baseline_dirty = true;
                s_baseline_need_soon = true;
            }
        } else if (s_logical_known && s_logical_empty[sq] == 0) {
            if (!hall_assign_provisional(st)) {
                return 0;
            }
        } else {
            return 0;
        }
    }

    hall_poc_ring_push(st, r0, r1);
    if (st->ring_count < HALL_POC_SAMPLES_PER_CYCLE) {
        return st->occupied ? 1u : 0u;
    }

    int filt0 = hall_poc_trimmed_mean_u16(st->ring_r0);
    int filt1 = hall_poc_trimmed_mean_u16(st->ring_r1);

    int32_t dev0 = (int32_t)filt0 - st->midpoint[0];
    int32_t dev1 = (int32_t)filt1 - st->midpoint[1];
    int32_t ad0 = abs(dev0);
    int32_t ad1 = abs(dev1);
    int32_t delta = ad0 + ad1;
    int32_t energy = dev0 * dev0 + dev1 * dev1;
    st->live_energy = energy;
    st->live_dev0 = dev0;
    st->live_dev1 = dev1;
    st->live_valid = true;
    bool present = st->occupied ? (delta >= HALL_POC_DELTA_OFF)
                                : (delta >= HALL_POC_DELTA_ON);

    if (!present) {
        st->occupied = false;
        st->peak_energy = 0;
        st->peak_dev0 = 0;
        st->peak_dev1 = 0;
        st->settle_scans = 0;
        st->pending_piece = HALL_PIECE_TYPE_NONE;
        st->pending_color = HALL_PIECE_COLOR_NONE;
        st->stable_ticks = 0;
        if (st->current_piece != HALL_PIECE_TYPE_NONE) {
            st->current_piece = HALL_PIECE_TYPE_NONE;
            st->current_color = HALL_PIECE_COLOR_NONE;
            if (!s_mag_on) {
                ESP_LOGW(TAG, ">>> POC sq %02u changed to [Empty None] (E=%ld) <<<",
                         (unsigned)sq, (long)energy);
            }
        }
        st->ever_locked = true;
    } else {
        st->occupied = true;
        int32_t rise = st->peak_energy / 8;
        if (rise < 1) {
            rise = 1;
        }
        if (energy > st->peak_energy + rise) {
            st->peak_energy = energy;
            st->peak_dev0 = (int16_t)dev0;
            st->peak_dev1 = (int16_t)dev1;
            st->settle_scans = 0;
        } else if (st->settle_scans < HALL_POC_SETTLE_SCANS) {
            st->settle_scans++;
        }
        /* Live band, no locked window. A magnet between two types must keep
         * switching (knight, bishop, ...) while the threshold is tuned. */
        const hall_poc_threshold_t *thr =
            hall_poc_classify(energy, HALL_PIECE_TYPE_NONE);
        hall_piece_type_t raw_piece =
            (thr != NULL) ? thr->type : HALL_PIECE_TYPE_NONE;
        int32_t dominant = (abs(dev0) > abs(dev1)) ? dev0 : dev1;
        hall_piece_color_t raw_color = (dominant > 0)
                                           ? HALL_PIECE_COLOR_WHITE
                                           : HALL_PIECE_COLOR_BLACK;
        if (raw_piece != HALL_PIECE_TYPE_NONE &&
            (st->current_piece != raw_piece || st->current_color != raw_color)) {
            st->ever_locked = true;
            st->current_piece = raw_piece;
            st->current_color = raw_color;
            if (!s_mag_on) {
                ESP_LOGW(TAG, ">>> POC sq %02u changed to [%s %s] (E=%ld) <<<",
                         (unsigned)sq,
                         hall_i2c_squares_piece_color_name(st->current_color),
                         hall_i2c_squares_piece_type_name(st->current_piece),
                         (long)energy);
            }
        }
    }

    /* Auto-zero only on a square that is really empty, not while a piece is settling.
     * The magnet bench freezes its square so later magnets share one baseline. */
    if ((int)sq == s_mag_freeze_sq) {
        st->empty_ticks = 0;
    } else if (!st->occupied &&
        abs(dev0) < HALL_POC_AUTOZERO_DEV_LIMIT &&
        abs(dev1) < HALL_POC_AUTOZERO_DEV_LIMIT) {
        st->empty_ticks++;
        if (st->empty_ticks > HALL_POC_AUTOZERO_EMPTY_SCANS &&
            (st->empty_ticks % HALL_POC_AUTOZERO_BUMP_SCANS) == 0) {
            if (filt0 > st->midpoint[0]) st->midpoint[0]++;
            else if (filt0 < st->midpoint[0]) st->midpoint[0]--;
            if (filt1 > st->midpoint[1]) st->midpoint[1]++;
            else if (filt1 < st->midpoint[1]) st->midpoint[1]--;
            if (!st->confirmed_empty) {
                st->confirmed_empty = true;
                st->provisional = false;
                s_baseline_need_soon = true;
            }
            s_baseline_dirty = true;
        }
    } else {
        st->empty_ticks = 0;
    }

    return st->occupied ? 1u : 0u;
}

static esp_err_t hall_i2c_read_at(uint8_t addr7, uint8_t *buf, size_t len,
                                  int timeout_ms) {
  uint8_t reg = (uint8_t)CONFIG_CHESS_HALL_REG_START;
  /* STM already packs adjacent mux channels. On this board those two
   * channels are the two halls of one square, so the payload is used as-is. */
  return chess_i2c_transmit_receive(
      addr7, (uint32_t)CONFIG_CHESS_HALL_I2C_FREQ_HZ, 20000, &reg, 1, buf, len,
      timeout_ms);
}

esp_err_t hall_i2c_squares_init(void) {
  esp_err_t err = chess_i2c_bus_init(CONFIG_CHESS_HALL_I2C_PORT_NUM,
                                     CONFIG_CHESS_HALL_I2C_SDA_GPIO,
                                     CONFIG_CHESS_HALL_I2C_SCL_GPIO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "chess_i2c_bus_init: %s", esp_err_to_name(err));
    return err;
  }

  hall_i2c_apply_bus_pullups();

  memset(s_poc_state, 0, sizeof(s_poc_state));
  memset(s_logical_empty, 0, sizeof(s_logical_empty));
  s_logical_known = false;
  s_baseline_dirty = false;
  s_baseline_need_soon = false;
  s_baseline_saved_ms = 0;
  s_baseline_seq = 0;
  s_baseline_slot = 0;
  hall_baseline_load();

  s_i2c_ready = true;
  ESP_LOGI(TAG,
           "I2C Hall ready (i2c_master) port=%d SDA=%d SCL=%d %d Hz segy 0x%02x 0x%02x 0x%02x 0x%02x POC_DEBOUNCE=%d",
           CONFIG_CHESS_HALL_I2C_PORT_NUM, CONFIG_CHESS_HALL_I2C_SDA_GPIO,
           CONFIG_CHESS_HALL_I2C_SCL_GPIO, CONFIG_CHESS_HALL_I2C_FREQ_HZ,
           CONFIG_CHESS_HALL_SEG0_ADDR, CONFIG_CHESS_HALL_SEG1_ADDR,
           CONFIG_CHESS_HALL_SEG2_ADDR, CONFIG_CHESS_HALL_SEG3_ADDR,
           HALL_POC_DEBOUNCE_SCANS);
  return ESP_OK;
}

esp_err_t hall_i2c_squares_probe_segment(uint8_t segment_0_to_3, int timeout_ms) {
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
  uint8_t buf[2];
  esp_err_t err = hall_i2c_read_at(addr, buf, sizeof(buf), timeout_ms);
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

void hall_i2c_squares_fill_state(uint8_t squares_state[64]) {
  if (!s_i2c_ready || squares_state == NULL) {
    return;
  }

  uint8_t buf[HALL_I2C_PAYLOAD_BYTES];
  static uint32_t s_scan;

  s_scan++;

#if CONFIG_CHESS_HALL_LOG_INTERVAL_SCANS > 0
  bool do_log = (s_scan % (uint32_t)CONFIG_CHESS_HALL_LOG_INTERVAL_SCANS) == 1;
#else
  bool do_log = false;
#endif

  /* After adding a Kconfig entry, the old sdkconfig macro may not have — default 4 segments. */
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
      if (sq >= 64u) {
        continue;
      }
      squares_state[sq] = 0;
    }
  }

  for (unsigned seg = 0; seg < seg_max; seg++) {
    uint8_t addr = segment_addr(seg);
    esp_err_t err = hall_i2c_read_at(addr, buf, sizeof(buf), 80);

    if (err != ESP_OK) {
      uint8_t bit = (uint8_t)(1u << seg);
      if ((s_read_fail_warned_mask & bit) == 0) {
        ESP_LOGW(TAG,
                 "segment %u addr 0x%02x read failed: %s "
                 "(another failure of the same segment only DEBUG)",
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
      if (sq >= 64u) {
        continue;
      }
      squares_state[sq] = hall_poc_update_square(sq, r0, r1);
    }

    if (do_log && !s_mag_on) {
      uint8_t sq0 = hall_map_segment_field_to_square(seg, 0);
      if (sq0 >= 64u) {
        continue;
      }
      ESP_LOGI(TAG,
               "[staging] seg%u addr=0x%02x first_pair raw=(%u,%u) occ=%u type=%s color=%s",
               seg, addr, hall_i2c_spec_read_le16(&buf[0]),
               hall_i2c_spec_read_le16(&buf[sizeof(uint16_t)]),
               (unsigned)squares_state[sq0],
               hall_i2c_squares_piece_type_name(s_poc_state[sq0].current_piece),
               hall_i2c_squares_piece_color_name(s_poc_state[sq0].current_color));
    }
  }
  hall_baseline_flush();
  hall_mag_light_locked();
}

unsigned hall_i2c_squares_segment_count(void) {
#if defined(CONFIG_CHESS_HALL_SEGMENT_COUNT)
  unsigned n = (unsigned)CONFIG_CHESS_HALL_SEGMENT_COUNT;
#else
  unsigned n = 4u;
#endif
  if (n < 1u) {
    n = 1u;
  }
  if (n > 4u) {
    n = 4u;
  }
  return n;
}

esp_err_t hall_i2c_squares_read_segment_raw(uint8_t segment_0_to_3,
                                            uint8_t *addr_out,
                                            uint16_t raw[32]) {
  if (segment_0_to_3 > 3u || raw == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  uint8_t addr = segment_addr(segment_0_to_3);
  if (addr_out != NULL) {
    *addr_out = addr;
  }
  if (!s_i2c_ready) {
    return ESP_ERR_INVALID_STATE;
  }
  uint8_t buf[HALL_I2C_PAYLOAD_BYTES];
  esp_err_t err = hall_i2c_read_at(addr, buf, sizeof(buf), 80);
  if (err != ESP_OK) {
    return err;
  }
  for (unsigned i = 0; i < HALL_I2C_UINT16_PER_SEGMENT; i++) {
    raw[i] = hall_i2c_spec_read_le16(&buf[i * sizeof(uint16_t)]);
  }
  return ESP_OK;
}

extern bool squares_scanning_enabled;

#if HALL_DEBUG_LIGHT_WHERE_PIECE
#define HALL_DBG_REST_SCANS 20
#define HALL_DBG_DELTA 270
#define HALL_DBG_DELTA_OFF 160
#define HALL_DBG_HOLD 3
#define HALL_DBG_HOLD_OFF 8
#define HALL_DBG_SETTLE 25

static uint32_t s_dbg_sum[4][32];
static uint16_t s_dbg_rest[4][32];
static uint8_t s_dbg_rest_n;
static bool s_dbg_rest_request;
static uint8_t s_dbg_hold[4][16];
static uint8_t s_dbg_off[4][16];
static uint8_t s_dbg_announced[4][16];
static uint8_t s_dbg_sq_users[64];
static int32_t s_dbg_peak_e[4][16];
static int16_t s_dbg_peak_s0[4][16];
static int16_t s_dbg_peak_s1[4][16];
static uint8_t s_dbg_settle[4][16];
static uint8_t s_dbg_named[4][16];

static hall_piece_type_t hall_dbg_piece_type(int32_t energy) {
  /* Pawn 51k–67k. Knight magnet 114k–182k. The next magnet settles
   * anywhere from ~300k to ~382k, so that span is bishop. */
  static const hall_poc_threshold_t dbg_piece[] = {
      {15000, 90000, HALL_PIECE_TYPE_PAWN},
      {90001, 240000, HALL_PIECE_TYPE_KNIGHT},
      {240001, 520000, HALL_PIECE_TYPE_BISHOP},
      {520001, 800000, HALL_PIECE_TYPE_ROOK},
      {800001, 1200000, HALL_PIECE_TYPE_QUEEN},
      {1200001, 8000000, HALL_PIECE_TYPE_KING},
  };
  const hall_poc_threshold_t *thr = &dbg_piece[0];
  for (unsigned i = 0; i < sizeof(dbg_piece) / sizeof(dbg_piece[0]); i++) {
    if (energy >= dbg_piece[i].min_energy) {
      thr = &dbg_piece[i];
    }
  }
  return thr->type;
}

static void hall_dbg_log_settled(unsigned seg, unsigned field, uint8_t row,
                                 uint8_t col) {
  int32_t energy = s_dbg_peak_e[seg][field];
  int s0 = s_dbg_peak_s0[seg][field];
  int s1 = s_dbg_peak_s1[seg][field];
  int d0 = (s0 < 0) ? -s0 : s0;
  int d1 = (s1 < 0) ? -s1 : s1;
  hall_piece_type_t piece = hall_dbg_piece_type(energy);
  int dominant = (d0 > d1) ? s0 : s1;
  hall_piece_color_t color =
      (dominant > 0) ? HALL_PIECE_COLOR_WHITE : HALL_PIECE_COLOR_BLACK;
  ESP_LOGW(TAG, "FIGURKA seg %u field %u -> %c%c %s %s delta=%d E=%ld", seg,
           field, 'a' + col, '1' + row, hall_i2c_squares_piece_color_name(color),
           hall_i2c_squares_piece_type_name(piece), d0 + d1, (long)energy);
}

void hall_i2c_squares_request_rest_measure(void) {
  s_dbg_rest_request = true;
}

static void hall_dbg_rest_save(void) {
  nvs_handle_t handle;
  if (nvs_open(HALL_BASE_NVS_NS, NVS_READWRITE, &handle) != ESP_OK) {
    return;
  }
  uint32_t magic = 0x4B4C4944u;
  if (nvs_set_blob(handle, "dbg_magic", &magic, sizeof(magic)) == ESP_OK &&
      nvs_set_blob(handle, "dbg_rest", s_dbg_rest, sizeof(s_dbg_rest)) ==
          ESP_OK) {
    (void)nvs_commit(handle);
  }
  nvs_close(handle);
}

void hall_i2c_squares_debug_light_pieces(void) {
  if (!s_i2c_ready || led_is_booting()) {
    return;
  }
  squares_scanning_enabled = false;
  led_board_restore_guide_clear();

  if (s_dbg_rest_request) {
    s_dbg_rest_request = false;
    s_dbg_rest_n = 0;
    memset(s_dbg_sum, 0, sizeof(s_dbg_sum));
  }

  if (s_dbg_rest_n < HALL_DBG_REST_SCANS) {
    if (s_dbg_rest_n == 0) {
      ESP_LOGW(TAG, "KLID: nech desku prazdnou, merim %u snimku",
               HALL_DBG_REST_SCANS);
    }
    uint16_t frame[4][32];
    for (unsigned seg = 0; seg < 4u; seg++) {
      if (hall_i2c_squares_read_segment_raw((uint8_t)seg, NULL, frame[seg]) !=
          ESP_OK) {
        return;
      }
    }
    for (unsigned seg = 0; seg < 4u; seg++) {
      for (unsigned i = 0; i < 32u; i++) {
        s_dbg_sum[seg][i] += frame[seg][i];
      }
    }
    s_dbg_rest_n++;
    if (s_dbg_rest_n < HALL_DBG_REST_SCANS) {
      return;
    }
    for (unsigned seg = 0; seg < 4u; seg++) {
      for (unsigned i = 0; i < 32u; i++) {
        s_dbg_rest[seg][i] =
            (uint16_t)(s_dbg_sum[seg][i] / HALL_DBG_REST_SCANS);
      }
    }
    led_clear_all_safe();
    hall_dbg_rest_save();
    ESP_LOGW(TAG, "KLID hotovo. Poloz figurku, rozsviti se prirazene pole.");
    return;
  }

  for (unsigned seg = 0; seg < 4u; seg++) {
    uint16_t raw[32];
    if (hall_i2c_squares_read_segment_raw((uint8_t)seg, NULL, raw) != ESP_OK) {
      continue;
    }
    for (unsigned field = 0; field < 16u; field++) {
      int s0 = (int)raw[field * 2u] - (int)s_dbg_rest[seg][field * 2u];
      int s1 = (int)raw[field * 2u + 1u] - (int)s_dbg_rest[seg][field * 2u + 1u];
      int d0 = (s0 < 0) ? -s0 : s0;
      int d1 = (s1 < 0) ? -s1 : s1;
      int delta = d0 + d1;
      uint8_t sq = hall_map_segment_field_to_square(seg, field);
      if (sq >= 64u) {
        if (s_dbg_announced[seg][field] == 0) {
          if (delta < HALL_DBG_DELTA) {
            s_dbg_hold[seg][field] = 0;
            continue;
          }
          if (s_dbg_hold[seg][field] < HALL_DBG_HOLD) {
            s_dbg_hold[seg][field]++;
          }
          if (s_dbg_hold[seg][field] < HALL_DBG_HOLD) {
            continue;
          }
          s_dbg_announced[seg][field] = 1;
          ESP_LOGW(TAG, "FIGURKA seg %u field %u -> BEZ MAPY delta=%d", seg,
                   field, delta);
        } else if (delta < HALL_DBG_DELTA_OFF) {
          s_dbg_announced[seg][field] = 0;
          s_dbg_hold[seg][field] = 0;
        }
        continue;
      }
      uint8_t row = (uint8_t)(sq / 8u);
      uint8_t col = (uint8_t)(sq % 8u);
      uint8_t led = chess_pos_to_led_index(row, col);
      bool lit = s_dbg_announced[seg][field] != 0;

      if (!lit) {
        s_dbg_off[seg][field] = 0;
        if (delta < HALL_DBG_DELTA) {
          s_dbg_hold[seg][field] = 0;
          continue;
        }
        if (s_dbg_hold[seg][field] < HALL_DBG_HOLD) {
          s_dbg_hold[seg][field]++;
        }
        if (s_dbg_hold[seg][field] < HALL_DBG_HOLD) {
          continue;
        }
        s_dbg_announced[seg][field] = 1;
        s_dbg_named[seg][field] = 0;
        s_dbg_settle[seg][field] = 0;
        s_dbg_peak_e[seg][field] = (int32_t)s0 * (int32_t)s0 + (int32_t)s1 * (int32_t)s1;
        s_dbg_peak_s0[seg][field] = (int16_t)s0;
        s_dbg_peak_s1[seg][field] = (int16_t)s1;
        if (s_dbg_sq_users[sq] == 0) {
          led_set_pixel_safe(led, 0, 180, 0);
        }
        s_dbg_sq_users[sq]++;
        continue;
      }

      s_dbg_hold[seg][field] = HALL_DBG_HOLD;
      if (delta >= HALL_DBG_DELTA_OFF) {
        int32_t energy = (int32_t)s0 * (int32_t)s0 + (int32_t)s1 * (int32_t)s1;
        int32_t peak = s_dbg_peak_e[seg][field];
        s_dbg_off[seg][field] = 0;
        if (peak <= 0 || energy > peak + peak / 8) {
          s_dbg_peak_e[seg][field] = energy;
          s_dbg_peak_s0[seg][field] = (int16_t)s0;
          s_dbg_peak_s1[seg][field] = (int16_t)s1;
          s_dbg_settle[seg][field] = 0;
          s_dbg_named[seg][field] = 0;
        } else if (s_dbg_settle[seg][field] < HALL_DBG_SETTLE) {
          s_dbg_settle[seg][field]++;
        }
        if (!s_dbg_named[seg][field] &&
            s_dbg_settle[seg][field] >= HALL_DBG_SETTLE) {
          hall_dbg_log_settled(seg, field, row, col);
          s_dbg_named[seg][field] = 1;
        }
        continue;
      }
      if (s_dbg_off[seg][field] < HALL_DBG_HOLD_OFF) {
        s_dbg_off[seg][field]++;
      }
      if (s_dbg_off[seg][field] < HALL_DBG_HOLD_OFF) {
        continue;
      }
      s_dbg_announced[seg][field] = 0;
      s_dbg_named[seg][field] = 0;
      s_dbg_settle[seg][field] = 0;
      s_dbg_peak_e[seg][field] = 0;
      s_dbg_hold[seg][field] = 0;
      s_dbg_off[seg][field] = 0;
      if (s_dbg_sq_users[sq] > 0) {
        s_dbg_sq_users[sq]--;
      }
      if (s_dbg_sq_users[sq] == 0) {
        led_set_pixel_safe(led, 0, 0, 0);
      }
    }
  }
}
#else
void hall_i2c_squares_debug_light_pieces(void) {}
void hall_i2c_squares_request_rest_measure(void) {}
#endif

void hall_i2c_squares_corner_probe(void) {
  if (!s_i2c_ready) {
    return;
  }
  static const struct {
    uint8_t row;
    uint8_t col;
    const char *name;
  } corners[] = {
      {0, 0, "a1"},
      {0, 7, "h1"},
      {7, 7, "h8"},
      {7, 0, "a8"},
  };
  bool scan_was = squares_scanning_enabled;
  squares_scanning_enabled = false;
  vTaskDelay(pdMS_TO_TICKS(50));
  ESP_LOGW(TAG, "ROHY: postupne a1, h1, h8, a8. Figurku nech lezet na zelenem poli.");

  for (unsigned c = 0; c < 4; c++) {
    uint16_t base[4][32];
    memset(base, 0, sizeof(base));
    for (unsigned seg = 0; seg < 4; seg++) {
      if (hall_i2c_squares_read_segment_raw((uint8_t)seg, NULL, base[seg]) != ESP_OK) {
        ESP_LOGW(TAG, "ROH %s: seg %u cteci chyba, beru nulu", corners[c].name, seg);
      }
    }
    uint8_t led = chess_pos_to_led_index(corners[c].row, corners[c].col);
    ESP_LOGW(TAG, "ROH %s: poloz figurku na zelene pole (LED %u)", corners[c].name, led);
    int stable_seg = -1;
    int stable_field = -1;
    int stable_hits = 0;
    bool found = false;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(45000);
    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
      esp_task_wdt_reset();
      led_clear_all_safe();
      led_set_pixel_safe(led, 0, 255, 0);
      int best_seg = -1;
      int best_field = -1;
      int best_delta = 0;
      for (unsigned seg = 0; seg < 4; seg++) {
        uint16_t now[32];
        if (hall_i2c_squares_read_segment_raw((uint8_t)seg, NULL, now) != ESP_OK) {
          continue;
        }
        for (unsigned field = 0; field < HALL_I2C_FIELDS_PER_SEGMENT; field++) {
          int d0 = (int)now[field * 2] - (int)base[seg][field * 2];
          int d1 = (int)now[field * 2 + 1] - (int)base[seg][field * 2 + 1];
          if (d0 < 0) {
            d0 = -d0;
          }
          if (d1 < 0) {
            d1 = -d1;
          }
          int delta = d0 + d1;
          if (delta > best_delta) {
            best_delta = delta;
            best_seg = (int)seg;
            best_field = (int)field;
          }
        }
      }
      if (best_delta > 600 && best_seg == stable_seg && best_field == stable_field) {
        stable_hits++;
      } else if (best_delta > 600) {
        stable_seg = best_seg;
        stable_field = best_field;
        stable_hits = 1;
      } else {
        stable_hits = 0;
      }
      if (stable_hits >= 3) {
        uint8_t mapped = hall_map_segment_field_to_square((unsigned)stable_seg,
                                                          (unsigned)stable_field);
        ESP_LOGW(TAG, "ROH %s = seg %u field %u delta=%d (mapa ted rika pole %u)",
                 corners[c].name, (unsigned)stable_seg, (unsigned)stable_field,
                 best_delta, mapped);
        found = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!found) {
      ESP_LOGW(TAG, "ROH %s: nic nepribylo, jdu dal", corners[c].name);
    }
  }
  led_clear_all_safe();
  squares_scanning_enabled = scan_was;
  ESP_LOGW(TAG, "ROHY hotovo. Podle radku ROH = seg/field se opravi mapa.");
}

#else /* !CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

esp_err_t hall_i2c_squares_init(void) { return ESP_OK; }

esp_err_t hall_i2c_squares_probe_segment(uint8_t segment_0_to_3, int timeout_ms) {
  (void)segment_0_to_3;
  (void)timeout_ms;
  return ESP_ERR_NOT_SUPPORTED;
}

void hall_i2c_squares_fill_state(uint8_t squares_state[64]) {
  if (squares_state) {
    memset(squares_state, 0, 64);
  }
}

unsigned hall_i2c_squares_segment_count(void) { return 0; }

void hall_i2c_squares_corner_probe(void) {}

void hall_i2c_squares_debug_light_pieces(void) {}
void hall_i2c_squares_request_rest_measure(void) {}

esp_err_t hall_i2c_squares_read_segment_raw(uint8_t segment_0_to_3,
                                            uint8_t *addr_out,
                                            uint16_t raw[32]) {
  (void)segment_0_to_3;
  (void)raw;
  if (addr_out != NULL) {
    *addr_out = 0;
  }
  return ESP_ERR_NOT_SUPPORTED;
}

bool hall_i2c_squares_readings_usable(void) { return true; }

void hall_i2c_squares_set_logical_empty(const uint8_t empty_squares[64]) {
  (void)empty_squares;
}

#endif

/* ========================================================================
 * Single definition of the public helper/getter API for all build configs.
 * When I2C Hall is enabled, these read the POC per-square state.
 * When disabled, they return safe defaults so the header is always usable.
 * ======================================================================== */

const char* hall_i2c_squares_piece_type_name(hall_piece_type_t type) {
  switch (type) {
  case HALL_PIECE_TYPE_PAWN:   return "Pawn";
  case HALL_PIECE_TYPE_KNIGHT: return "Knight";
  case HALL_PIECE_TYPE_BISHOP: return "Bishop";
  case HALL_PIECE_TYPE_ROOK:   return "Rook";
  case HALL_PIECE_TYPE_QUEEN:  return "Queen";
  case HALL_PIECE_TYPE_KING:   return "King";
  default:                     return "None";
  }
}

const char* hall_i2c_squares_piece_color_name(hall_piece_color_t color) {
  switch (color) {
  case HALL_PIECE_COLOR_WHITE: return "WHITE";
  case HALL_PIECE_COLOR_BLACK: return "BLACK";
  default:                     return "----";
  }
}

hall_piece_type_t hall_i2c_squares_get_piece_type(uint8_t square) {
  if (square >= 64) {
    return HALL_PIECE_TYPE_NONE;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return s_poc_state[square].current_piece;
#else
  return HALL_PIECE_TYPE_NONE;
#endif
}

hall_piece_color_t hall_i2c_squares_get_piece_color(uint8_t square) {
  if (square >= 64) {
    return HALL_PIECE_COLOR_NONE;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return s_poc_state[square].current_color;
#else
  return HALL_PIECE_COLOR_NONE;
#endif
}

static void hall_mag_light_locked(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (!s_mag_on || s_mag_sq >= 64u) {
    return;
  }
  uint8_t led = chess_pos_to_led_index((uint8_t)(s_mag_sq / 8u),
                                       (uint8_t)(s_mag_sq % 8u));
  led_set_pixel_safe(led, 0, 180, 0);
#else
  return;
#endif
}

bool hall_magnet_bench_start(uint8_t square) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  s_mag_on = true;
  if (!s_mag_log_saved) {
    s_mag_log_was = esp_log_level_get("*");
    s_mag_log_saved = true;
  }
  esp_log_level_set("*", ESP_LOG_NONE);
  if (square < 64u) {
    s_mag_sq = square;
    s_mag_freeze_sq = (int)square;
    led_clear_board_only();
    hall_mag_light_locked();
  } else {
    s_mag_sq = 255;
    s_mag_freeze_sq = -1;
  }
  return true;
#else
  (void)square;
  return false;
#endif
}

void hall_magnet_bench_stop(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (s_mag_sq < 64u) {
    uint8_t led = chess_pos_to_led_index((uint8_t)(s_mag_sq / 8u),
                                         (uint8_t)(s_mag_sq % 8u));
    led_set_pixel_safe(led, 0, 0, 0);
  }
  s_mag_on = false;
  s_mag_sq = 255;
  s_mag_freeze_sq = -1;
  if (s_mag_log_saved) {
    esp_log_level_set("*", s_mag_log_was);
    s_mag_log_saved = false;
  }
#endif
}

bool hall_magnet_bench_active(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return s_mag_on;
#else
  return false;
#endif
}

uint8_t hall_magnet_bench_square(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return s_mag_on ? s_mag_sq : 255;
#else
  return 255;
#endif
}

void hall_magnet_bench_capture(void) {
#if !CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return;
#else
  if (!s_mag_on) {
    return;
  }

  uint8_t sq = s_mag_sq;
  if (sq >= 64u) {
    int found = -1;
    int occupied_n = 0;
    int32_t best = -1;
    for (uint8_t i = 0; i < 64u; i++) {
      if (!s_poc_state[i].occupied || !s_poc_state[i].live_valid) {
        continue;
      }
      occupied_n++;
      if (s_poc_state[i].live_energy > best) {
        best = s_poc_state[i].live_energy;
        found = (int)i;
      }
    }
    if (occupied_n != 1 || found < 0) {
      return;
    }
    sq = (uint8_t)found;
    s_mag_sq = sq;
    s_mag_freeze_sq = (int)sq;
    led_clear_board_only();
    hall_mag_light_locked();
  }

  const hall_poc_square_state_t *st = &s_poc_state[sq];
  if (!st->live_valid) {
    return;
  }
  printf("%ld\n", (long)st->live_energy);
  fflush(stdout);
#endif
}
