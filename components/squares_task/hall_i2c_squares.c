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
/* 2026-10-26: d2 pawn sat at energy 36569 with dev 187/40. Sum 227 never
 * reached 270, so the square stayed empty and the guide kept lighting it.
 * Strongest empty neighbor that reading was d8 at 15725 (king/bishop
 * crosstalk). Energy latches a piece the sum still calls a gap. */
#define HALL_POC_ENERGY_ON              25000
/** Scans the peak energy must stop rising before the type is named. */
#define HALL_POC_SETTLE_SCANS           25
/** Below this the square is not a piece. Pawn band starts higher (see thresholds). */
#define HALL_POC_ENERGY_DEADBAND        10000

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

/** 10 % hysteresis. Knight and bishop, and queen and king, are closer than 20 %. */
#define HALL_POC_HYSTERESIS_PERCENT     10

/** Boot calibration: POC averages 20 ADC reads per channel (5 ms apart);
 *  V2 uses one I2C pair per scan → 20 scans (~500 ms at 25 ms) before lock. */
#define HALL_POC_BOOT_CAL_SCANS         20

/* Stray field at another square's center. Pitch 35 mm, sensor plane 11.2 mm
 * under the magnet face, dipole scaled to the simulated on-axis Bz in
 * docs/simulace-magnetu. A square counts as touched when both Halls would
 * move by about 15 counts or more (the auto-zero floor). The steel back
 * disc is not in those curves, so bishop and above keep one extra step.
 * Offsets are (min, max) of |file| and |rank|, not including (0,0).
 *   pawn   Ø10×1  orthogonal neighbors
 *   knight Ø10×2  the eight around
 *   bishop Ø10×3  those, plus two steps straight
 *   rook   Ø10×5  bishop, plus a knight move
 *   queen  Ø10×8  rook, plus two-and-two and three straight
 *   king   Ø12×6  same as the queen
 */
static const int8_t HALL_REACH_PAWN[]   = {0, 1};
static const int8_t HALL_REACH_KNIGHT[] = {0, 1, 1, 1};
static const int8_t HALL_REACH_BISHOP[] = {0, 1, 1, 1, 0, 2};
static const int8_t HALL_REACH_ROOK[]   = {0, 1, 1, 1, 0, 2, 1, 2};
static const int8_t HALL_REACH_ROYAL[]  = {0, 1, 1, 1, 0, 2, 1, 2, 2, 2, 0, 3};

/** Occupancy is published and the type named only after the deviation has
 *  been steady this long. A piece sliding across the square does not count. */
#define HALL_STABLE_MS                  250
#define HALL_STABLE_SCANS               (HALL_STABLE_MS / TOTAL_CYCLE_TIME_MS)
/** Lift publishes after this many consecutive vacant scans (~50 ms at 25 ms). */
#define HALL_LIFT_CONFIRM_SCANS         2
/** Empty square beside a king measured 15725 (d8, 2026-10-26). Weakest
 *  seated pawn that day was 20098. 18000 is the gap: bleed releases, a
 *  real pawn still holds. Four scans are 100 ms and run inside the 250 ms
 *  stable window, so a real placement still publishes in about 250 ms. */
#define HALL_REOCCUPY_ENERGY            18000
#define HALL_REOCCUPY_SCANS             4
/** Opposite-polarity fringe under a bishop or stronger. Empty d2 beside the
 *  queen measured 20000 and was named a black pawn. A seated white pawn
 *  beside queen, bishop, or king can fall to ~20098, so the veto shares
 *  the reoccupy floor. 613705 is the bishop band in s_poc_thresholds. */
#define HALL_FRINGE_ENERGY              HALL_REOCCUPY_ENERGY
#define HALL_STRONG_NEIGHBOR_ENERGY     613705
/** Midpoint may walk toward a stable empty reading by at most this many
 *  counts. A seated piece is well past it, so drift can heal and a piece
 *  cannot be absorbed. One count per HALL_STABLE_SCANS. */
#define HALL_DRIFT_MAX_DEV              120
#define HALL_I2C_READ_TIMEOUT_MS        20
#define HALL_I2C_FAIL_BACKOFF_SCANS     16
/** Allowed scan-to-scan change while steady: max(abs, sum|dev| / div). */
#define HALL_STABLE_ABS_TOL             12
#define HALL_STABLE_REL_DIV             25

/** The empty-square context reference may sit at most this far (sum |dev|)
 *  from the zero: room for neighbor crosstalk, not for a half-placed piece.
 *  Placeholder until crosstalk is measured. */
#define HALL_CTX_MAX_SHIFT              150
/** EMA divisor for the context reference while the square is empty and steady. */
#define HALL_CTX_EMA_DIV                4
/** After a neighbor's occupancy changes, hold this square's type this long. */
#define HALL_NEIGHBOR_MASK_SCANS        HALL_STABLE_SCANS
/** A fresh zero is kept only if the square and its neighbors stay empty. */
#define HALL_ZERO_QUARANTINE_MS         2000
#define HALL_ZERO_QUARANTINE_SCANS      (HALL_ZERO_QUARANTINE_MS / TOTAL_CYCLE_TIME_MS)

/** Sentinel for raw ADC values that carry no real Hall data (Nucleo mock). */
#define HALL_POC_NO_DATA_RAW            0u

typedef struct {
    int32_t min_energy;
    int32_t max_energy;
    hall_piece_type_t type;
} hall_poc_threshold_t;

/* Measured 2026-10-23, physical pieces, energy = dev0^2+dev1^2.
 * A lone pawn sat at 77837 and above. In the starting row, 2026-10-26,
 * the neighbors pull the white pawns down to 20098..81940. The old floor
 * 71610 left six of those eight with no name, so the guide drew them as
 * missing. Floor is 10000: the strongest empty neighbor in that same
 * reading (c3, crosstalk only) was 5141.
 * Knight cluster 421057..577792. Bishop cluster 651850..1242413.
 * One bishop tail, 489376, sits inside the knights. The edge that put
 * every knight above 444887 into the bishop made knights read as bishops.
 * Edge is round(sqrt(577792 * 651850)). Bishop still reaches 1380458 so
 * 1107410 and 1242413 stay bishops, not rooks.
 * Rooks measured 2026-10-26: 1450345..2528461. The old ceiling 1724251
 * was an unmeasured gap, so seven of those nine were named queen.
 * The ceiling is the weakest seated queen, 2511730. One rook (2528461,
 * deviations 870 and 1331) still sits above her. hall_name_rook_or_royal()
 * pulls that overlap back by the product of the two deviations.
 * Queen and king share the span above the rook. A queen slid across the
 * board reads 2745865..4544912, and seated kings read 3403621..5443562. */
static const hall_poc_threshold_t s_poc_thresholds[] = {
    {   10000,   250080,   HALL_PIECE_TYPE_PAWN   },
    {  250081,   613704,   HALL_PIECE_TYPE_KNIGHT },
    {  613705,  1380458,   HALL_PIECE_TYPE_BISHOP },
    { 1380459,  2511729,   HALL_PIECE_TYPE_ROOK   },
    { 2511730,  7500000,   HALL_PIECE_TYPE_QUEEN  },
};
#define HALL_POC_THRESHOLD_COUNT (sizeof(s_poc_thresholds) / sizeof(s_poc_thresholds[0]))

/* Per-square gain, Q8 (256 = 1). Applied only after four confirmed landings.
 * Pawn / knight / bishop nominals are the measured centers. Rook and the
 * royals are not used: rook has no center, and king overlaps queen. */
#define HALL_GAIN_ONE 256
#define HALL_GAIN_MIN 218
#define HALL_GAIN_MAX 294
#define HALL_GAIN_APPLY 4
#define HALL_GAIN_DIV 16

static uint16_t s_gain_q8[64];
static uint8_t s_gain_n[64];

static int32_t hall_type_nominal(hall_piece_type_t type) {
  switch (type) {
  case HALL_PIECE_TYPE_PAWN:
    return 120000;
  case HALL_PIECE_TYPE_KNIGHT:
    return 484445;
  case HALL_PIECE_TYPE_BISHOP:
    return 787792;
  default:
    return 0;
  }
}

static void hall_gain_reset(void) {
  for (int i = 0; i < 64; i++) {
    s_gain_q8[i] = HALL_GAIN_ONE;
    s_gain_n[i] = 0;
  }
}

static int32_t hall_gain_normalize(uint8_t sq, int32_t energy) {
  if (sq >= 64 || s_gain_n[sq] < HALL_GAIN_APPLY) {
    return energy;
  }
  uint16_t g = s_gain_q8[sq];
  if (g < HALL_GAIN_MIN || g > HALL_GAIN_MAX) {
    return energy;
  }
  return (int32_t)(((int64_t)energy * HALL_GAIN_ONE) / g);
}

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
    uint8_t vacant_scans;           /**< Consecutive scans under the off threshold while occupied. */
    int32_t peak_energy;            /**< Highest energy since the piece arrived. */
    int16_t peak_dev0;
    int16_t peak_dev1;
    uint8_t settle_scans;           /**< Scans since the peak last rose. */
    int32_t live_energy;            /**< Last settled-candidate energy. */
    int32_t live_dev0;
    int32_t live_dev1;
    bool live_valid;
    int32_t prev_dev0;
    int32_t prev_dev1;
    uint8_t stable_scans;           /**< Consecutive scans with a steady deviation. */
    bool reported;                  /**< Settled occupancy published to the matrix. */
    uint8_t prove_scans;            /**< Consecutive scans at or above HALL_REOCCUPY_ENERGY. */
    bool lift_block;                /**< Published piece left; delta-hold stays off. */
    uint8_t drift_phase;            /**< Counts toward one midpoint step. */
    bool identity_settled;          /**< Type/color stable while the magnet is still seated. */
    int32_t ctx_ref[2];             /**< Last steady empty reading, neighbor crosstalk included. */
    bool ctx_valid;
    bool prev_occupied;             /**< Occupancy at the end of the previous update. */
    uint8_t mask_scans;             /**< Scans left before this square may be reclassified. */
    uint8_t ctx_arm;                /**< Scans left in which a neighbor move may move ctx_ref. */
    uint8_t zero_quarantine;        /**< Scans left before a new zero may be stored. */
    int32_t zero_backup[2];         /**< Midpoint to restore if the new zero is discarded. */
    bool zero_has_backup;
    bool zero_pending;
} hall_poc_square_state_t;

/** Per-square state — hysteresis and debounce are NOT global across the board. */
static hall_poc_square_state_t s_poc_state[64];

static void hall_gain_note(uint8_t sq, hall_piece_type_t type,
                           hall_piece_color_t color) {
  int32_t nominal = hall_type_nominal(type);
  if (sq >= 64 || nominal <= 0) {
    return;
  }
  const hall_poc_square_state_t *st = &s_poc_state[sq];
  if (!st->live_valid || !st->occupied || st->current_color != color ||
      st->current_color == HALL_PIECE_COLOR_NONE) {
    return;
  }
  int32_t energy = st->live_energy;
  if (energy < HALL_POC_ENERGY_DEADBAND) {
    return;
  }
  int32_t ratio = (int32_t)(((int64_t)energy * HALL_GAIN_ONE) / nominal);
  /* Far from the nominal is placement, not this square's gain. */
  if (ratio < 192 || ratio > 340) {
    return;
  }
  int32_t g = s_gain_q8[sq];
  g += (ratio - g) / HALL_GAIN_DIV;
  if (g < HALL_GAIN_MIN) {
    g = HALL_GAIN_MIN;
  }
  if (g > HALL_GAIN_MAX) {
    g = HALL_GAIN_MAX;
  }
  s_gain_q8[sq] = (uint16_t)g;
  if (s_gain_n[sq] < 255) {
    s_gain_n[sq]++;
  }
  if (s_gain_n[sq] == HALL_GAIN_APPLY) {
    ESP_LOGI(TAG, "square %u gain %u/256 after %u landings", (unsigned)sq,
             (unsigned)s_gain_q8[sq], (unsigned)s_gain_n[sq]);
  }
}

/** Magnet bench: -1 = baseline may drift, else that square stays frozen. */
static int s_mag_freeze_sq = -1;
static bool s_mag_on;
static uint8_t s_mag_sq = 255;
static esp_log_level_t s_mag_log_was = ESP_LOG_INFO;
static bool s_mag_log_saved;
static void hall_mag_light_locked(void);

/** 1 = logical game says this square is empty and may be zeroed from live ADC. */
static uint8_t s_logical_empty[64];
static uint8_t s_logical_type[64];
static bool s_logical_known;
static bool s_baseline_dirty;
static bool s_baseline_need_soon;
static uint32_t s_baseline_saved_ms;
static uint32_t s_baseline_seq;
static uint8_t s_baseline_slot; /* 0 none, 1 base_a, 2 base_b */

/** 3 drops baselines measured with neighbor pieces in place. */
#define HALL_BASE_VERSION 3
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
    /* The two strongest magnets sit above this. Hysteresis must not keep them. */
    if (energy > s_poc_thresholds[HALL_POC_THRESHOLD_COUNT - 1].max_energy) {
        return NULL;
    }

    /* KROK 1: keep the locked piece inside a 10 % expanded window so small
     * drift near the boundary does not flicker the type. */
    if (locked_piece != HALL_PIECE_TYPE_NONE) {
        for (int i = 0; i < HALL_POC_THRESHOLD_COUNT; i++) {
            if (s_poc_thresholds[i].type == locked_piece) {
                int32_t margin_min = s_poc_thresholds[i].min_energy -
                                     (s_poc_thresholds[i].min_energy / (100 / HALL_POC_HYSTERESIS_PERCENT));
                int32_t margin_max = s_poc_thresholds[i].max_energy +
                                     (s_poc_thresholds[i].max_energy / (100 / HALL_POC_HYSTERESIS_PERCENT));
                if (s_poc_thresholds[i].type == HALL_PIECE_TYPE_QUEEN) {
                    margin_max = s_poc_thresholds[i].max_energy;
                }
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

/* King is the wider disc. Product |dev0|*|dev1|, lo = weaker sensor.
 * Queens stay at or under 1855701 unless both sensors are close
 * (1461/1742 = 2545062, ratio 0.84). That one stays a queen below
 * 2600000. Weaker kings sit just above the queen cluster when the
 * sensors differ (1027/1905 = 1956435, 1326/1905 = 2526030) or near
 * the center (1151/1325, 1297/1467). */
#define HALL_ROYAL_KING_PRODUCT      2600000
#define HALL_ROYAL_WEAK_KING_PRODUCT 1930000
#define HALL_ROYAL_CENTER_PRODUCT    1500000
#define HALL_ROYAL_SWITCH_SCANS      (HALL_STABLE_SCANS * 3)

/* Rook products on 2026-10-26 top out at 1175640 (1212*970). The weakest
 * seated queen is 1217223 (973*1251). The cut is round(sqrt of that pair).
 * Later the same day a1 rook moved to 1220*1023 = 1248060 and crossed it,
 * so a centered rook above the cut is recovered by hall_name_rook_or_royal().
 * Queens and kings otherwise sit above the cut. */
#define HALL_ROOK_QUEEN_PRODUCT      1196251
/* Centered-rook recovery: both axes >= 1000, within 20 %, product under this.
 * a1 at 1248060 qualifies. Queen e1 is 1002*1491 (ratio 0.67). The weak
 * queen 973*1251 fails the 1000 axis. Kings are past 1.8M. */
#define HALL_ROOK_CENTER_PRODUCT     1400000

static hall_piece_type_t hall_name_royal(int32_t c0, int32_t c1) {
    int32_t lo = abs(c0);
    int32_t hi = abs(c1);
    if (lo > hi) {
        int32_t swap = lo;
        lo = hi;
        hi = swap;
    }
    if (hi <= 0) {
        return HALL_PIECE_TYPE_QUEEN;
    }
    int32_t product = lo * hi;
    if (product >= HALL_ROYAL_KING_PRODUCT) {
        return HALL_PIECE_TYPE_KING;
    }
    /* Uneven sensors: king from 1930000. Ratio under 0.82, so the
     * balanced queen at 2545062 is not pulled down with this line. */
    if (product >= HALL_ROYAL_WEAK_KING_PRODUCT && lo * 100 < hi * 82) {
        return HALL_PIECE_TYPE_KING;
    }
    /* Centered king: weaker sensor >= 1100, within 14 %, product
     * from 1500000. Measured queens in this band stay past 16 %. */
    if (product >= HALL_ROYAL_CENTER_PRODUCT && lo >= 1100 &&
        lo * 50 >= hi * 43) {
        return HALL_PIECE_TYPE_KING;
    }
    return HALL_PIECE_TYPE_QUEEN;
}

/* Rook and queen energies overlap by one measured rook. Below the product
 * cut the reading is a rook even when the energy table said queen. At or
 * above it, a rook-band reading is a queen that dipped under 2511730. */
static hall_piece_type_t hall_name_rook_or_royal(int32_t c0, int32_t c1) {
    int32_t lo = abs(c0);
    int32_t hi = abs(c1);
    if (lo > hi) {
        int32_t swap = lo;
        lo = hi;
        hi = swap;
    }
    if ((int64_t)lo * hi < HALL_ROOK_QUEEN_PRODUCT) {
        return HALL_PIECE_TYPE_ROOK;
    }
    /* 1220 and 1023: 1023*5 >= 1220*4, product 1248060. */
    if (lo >= 1000 && lo * 5 >= hi * 4 &&
        (int64_t)lo * hi < HALL_ROOK_CENTER_PRODUCT) {
        return HALL_PIECE_TYPE_ROOK;
    }
    return hall_name_royal(c0, c1);
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

static bool hall_reach_has(const int8_t *tab, int n, int lo, int hi) {
  for (int i = 0; i < n; i++) {
    if (tab[i * 2] == (int8_t)lo && tab[i * 2 + 1] == (int8_t)hi) {
      return true;
    }
  }
  return false;
}

static bool hall_piece_reaches(hall_piece_type_t type, int d_row, int d_col) {
  int ar = d_row < 0 ? -d_row : d_row;
  int ac = d_col < 0 ? -d_col : d_col;
  if (ar == 0 && ac == 0) {
    return false;
  }
  int lo = ar < ac ? ar : ac;
  int hi = ar < ac ? ac : ar;
  switch (type) {
  case HALL_PIECE_TYPE_PAWN:
    return hall_reach_has(HALL_REACH_PAWN, 1, lo, hi);
  case HALL_PIECE_TYPE_KNIGHT:
    return hall_reach_has(HALL_REACH_KNIGHT, 2, lo, hi);
  case HALL_PIECE_TYPE_BISHOP:
    return hall_reach_has(HALL_REACH_BISHOP, 3, lo, hi);
  case HALL_PIECE_TYPE_ROOK:
    return hall_reach_has(HALL_REACH_ROOK, 4, lo, hi);
  case HALL_PIECE_TYPE_QUEEN:
  case HALL_PIECE_TYPE_KING:
    return hall_reach_has(HALL_REACH_ROYAL, 6, lo, hi);
  default:
    return false;
  }
}

static hall_piece_type_t hall_influencer_type(int n) {
  if (s_logical_known && s_logical_empty[n] == 0 && s_logical_type[n] != 0) {
    return (hall_piece_type_t)s_logical_type[n];
  }
  if (s_poc_state[n].current_piece != HALL_PIECE_TYPE_NONE) {
    return s_poc_state[n].current_piece;
  }
  if (s_poc_state[n].occupied || (s_logical_known && s_logical_empty[n] == 0)) {
    return HALL_PIECE_TYPE_KING;
  }
  return HALL_PIECE_TYPE_NONE;
}

static bool hall_area_clear(uint8_t sq) {
  if (s_poc_state[sq].occupied) {
    return false;
  }
  if (s_logical_known && s_logical_empty[sq] == 0) {
    return false;
  }
  int row = sq / 8;
  int col = sq % 8;
  for (int n = 0; n < 64; n++) {
    if (n == (int)sq) {
      continue;
    }
    hall_piece_type_t type = hall_influencer_type(n);
    if (type == HALL_PIECE_TYPE_NONE) {
      continue;
    }
    int nr = n / 8;
    int nc = n % 8;
    if (hall_piece_reaches(type, row - nr, col - nc)) {
      return false;
    }
  }
  return true;
}

static bool hall_clean_neighborhood(uint8_t sq, const uint8_t logical_empty[64]) {
  (void)logical_empty;
  return hall_area_clear(sq);
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

void hall_i2c_squares_set_logical_empty(const uint8_t empty_squares[64],
                                        const uint8_t piece_types[64]) {
  if (empty_squares == NULL) {
    return;
  }
  memcpy(s_logical_empty, empty_squares, sizeof(s_logical_empty));
  if (piece_types != NULL) {
    memcpy(s_logical_type, piece_types, sizeof(s_logical_type));
  } else {
    memset(s_logical_type, 0, sizeof(s_logical_type));
  }
  s_logical_known = true;
  for (int sq = 0; sq < 64; sq++) {
    hall_poc_square_state_t *st = &s_poc_state[sq];
    /* New position: drop a stale lift veto. An empty square re-learns
     * neighbor crosstalk instead of keeping the last castle offset. */
    st->lift_block = false;
    if (!st->occupied) {
      st->ctx_arm = (uint8_t)(HALL_NEIGHBOR_MASK_SCANS + HALL_STABLE_SCANS);
    }
    if (empty_squares[sq] == 0 || !st->provisional || st->confirmed_empty ||
        !hall_clean_neighborhood((uint8_t)sq, empty_squares)) {
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
    st->vacant_scans = 0;
    st->peak_energy = 0;
    st->settle_scans = 0;
    st->stable_scans = 0;
    st->reported = false;
    st->ctx_valid = false;
    st->mask_scans = 0;
    st->ctx_arm = (uint8_t)(HALL_NEIGHBOR_MASK_SCANS + HALL_STABLE_SCANS);
    st->zero_quarantine = 0;
    st->zero_pending = false;
    st->zero_has_backup = false;
    st->prev_occupied = false;
    st->prove_scans = 0;
    st->lift_block = false;
  }
}

static void hall_mask_neighbors(uint8_t sq) {
  hall_piece_type_t type = s_poc_state[sq].current_piece;
  if (type == HALL_PIECE_TYPE_NONE) {
    type = hall_influencer_type(sq);
  }
  if (type == HALL_PIECE_TYPE_NONE) {
    type = HALL_PIECE_TYPE_KING;
  }
  int row = sq / 8;
  int col = sq % 8;
  for (int n = 0; n < 64; n++) {
    if (n == (int)sq) {
      continue;
    }
    int nr = n / 8;
    int nc = n % 8;
    if (!hall_piece_reaches(type, nr - row, nc - col)) {
      continue;
    }
    hall_poc_square_state_t *st = &s_poc_state[n];
    if (st->mask_scans < HALL_NEIGHBOR_MASK_SCANS) {
      st->mask_scans = HALL_NEIGHBOR_MASK_SCANS;
    }
    st->ctx_arm = (uint8_t)(HALL_NEIGHBOR_MASK_SCANS + HALL_STABLE_SCANS);
  }
}

static bool hall_zero_contaminated(uint8_t sq) {
  return !hall_area_clear(sq);
}

static void hall_zero_begin(hall_poc_square_state_t *st, int32_t new0, int32_t new1) {
  if (!st->zero_pending) {
    st->zero_has_backup = st->confirmed_empty;
    st->zero_backup[0] = st->midpoint[0];
    st->zero_backup[1] = st->midpoint[1];
    st->zero_pending = true;
  }
  st->midpoint[0] = new0;
  st->midpoint[1] = new1;
  st->confirmed_empty = false;
  st->zero_quarantine = HALL_ZERO_QUARANTINE_SCANS;
}

static void hall_zero_commit(hall_poc_square_state_t *st) {
  st->zero_pending = false;
  st->zero_quarantine = 0;
  st->confirmed_empty = true;
  st->provisional = false;
  st->ctx_ref[0] = st->midpoint[0];
  st->ctx_ref[1] = st->midpoint[1];
  st->ctx_valid = true;
  s_baseline_dirty = true;
  s_baseline_need_soon = true;
}

static void hall_zero_discard(uint8_t sq) {
  hall_poc_square_state_t *st = &s_poc_state[sq];
  st->zero_quarantine = 0;
  st->zero_pending = false;
  st->ctx_valid = false;
  /* Restoring the midpoint changes the next deviation. An emptied square
   * must earn occupancy again; otherwise the jump looks like the piece
   * is still standing. */
  if (!st->occupied) {
    st->reported = false;
    st->prove_scans = 0;
    st->lift_block = true;
  }
  if (st->zero_has_backup) {
    st->midpoint[0] = st->zero_backup[0];
    st->midpoint[1] = st->zero_backup[1];
    st->confirmed_empty = true;
    st->ctx_ref[0] = st->midpoint[0];
    st->ctx_ref[1] = st->midpoint[1];
    st->ctx_valid = true;
    ESP_LOGW(TAG, "zero sq %u discarded, previous restored", (unsigned)sq);
    return;
  }
  st->initialized = false;
  st->confirmed_empty = false;
  st->provisional = false;
  st->cal_count = 0;
  st->cal_sum[0] = 0;
  st->cal_sum[1] = 0;
  ESP_LOGW(TAG, "zero sq %u discarded, will remeasure", (unsigned)sq);
}

bool hall_i2c_squares_readings_usable(void) {
  if (!s_i2c_ready || !s_logical_known) {
    return false;
  }
  /* ever_locked is not required. An occupied square names itself only after
   * the deviation settles, and one noisy piece used to keep this false.
   * The guide then never ran, and the boot fade left the whole board dark. */
  for (int sq = 0; sq < 64; sq++) {
    const hall_poc_square_state_t *st = &s_poc_state[sq];
    if (!st->initialized || st->ring_count < HALL_POC_SAMPLES_PER_CYCLE) {
      return false;
    }
  }
  return true;
}

/** King or queen in the 8-neighbourhood. Crosstalk from that magnet is what
 *  keeps a lifted square looking occupied. */
static bool hall_neighbor_is_royal(uint8_t sq) {
    int row = (int)sq / 8;
    int col = (int)sq % 8;
    int32_t royal_energy =
        s_poc_thresholds[HALL_POC_THRESHOLD_COUNT - 1].min_energy;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) {
                continue;
            }
            int r = row + dr;
            int c = col + dc;
            if (r < 0 || r > 7 || c < 0 || c > 7) {
                continue;
            }
            uint8_t n = (uint8_t)(r * 8 + c);
            const hall_poc_square_state_t *nb = &s_poc_state[n];
            if (nb->current_piece == HALL_PIECE_TYPE_KING ||
                nb->current_piece == HALL_PIECE_TYPE_QUEEN) {
                return true;
            }
            if (nb->live_valid && (nb->occupied || nb->reported) &&
                nb->live_energy >= royal_energy) {
                return true;
            }
            if (s_logical_known && s_logical_empty[n] == 0 &&
                (nb->occupied || nb->reported)) {
                uint8_t lt = s_logical_type[n];
                if (lt == (uint8_t)HALL_PIECE_TYPE_KING ||
                    lt == (uint8_t)HALL_PIECE_TYPE_QUEEN) {
                    return true;
                }
            }
        }
    }
    return false;
}

/** Moderate opposite-polarity reading next to a strong piece is that
 *  piece's fringe, not a magnet on this square. Caller passes the stronger
 *  of the midpoint and ctx_ref residuals. A real pawn clears the floor. */
static bool hall_opposite_fringe(uint8_t sq, int32_t dev0, int32_t dev1,
                                 int32_t energy) {
    if (energy >= HALL_FRINGE_ENERGY) {
        return false;
    }
    int32_t dom = (abs(dev0) > abs(dev1)) ? dev0 : dev1;
    if (dom == 0) {
        return false;
    }
    int row = (int)sq / 8;
    int col = (int)sq % 8;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) {
                continue;
            }
            int r = row + dr;
            int c = col + dc;
            if (r < 0 || r > 7 || c < 0 || c > 7) {
                continue;
            }
            const hall_poc_square_state_t *nb = &s_poc_state[r * 8 + c];
            if (!nb->occupied && !nb->reported) {
                continue;
            }
            bool strong = nb->current_piece >= HALL_PIECE_TYPE_BISHOP ||
                          (nb->live_valid &&
                           nb->live_energy >= HALL_STRONG_NEIGHBOR_ENERGY);
            if (!strong) {
                continue;
            }
            int32_t nd = (abs(nb->live_dev0) > abs(nb->live_dev1))
                             ? nb->live_dev0
                             : nb->live_dev1;
            if (nd == 0 || (dom > 0) == (nd > 0)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

/**
 * Run one POC state-machine step for a single square.
 * @param sq        square index 0..63
 * @param r0, r1    raw 12-bit ADC pair from the STM32 slave
 * @return          stable occupancy bit (1 = occupied, 0 = empty)
 */
static uint8_t hall_poc_update_square(uint8_t sq, uint16_t r0, uint16_t r1) {
    hall_poc_square_state_t *st = &s_poc_state[sq];
    st->identity_settled = false;

    /* Nucleo / no-real-data guard: a zero-filled payload is not a Hall sample. */
    if (r0 == HALL_POC_NO_DATA_RAW && r1 == HALL_POC_NO_DATA_RAW) {
        st->current_piece = HALL_PIECE_TYPE_NONE;
        st->current_color = HALL_PIECE_COLOR_NONE;
        st->pending_piece = HALL_PIECE_TYPE_NONE;
        st->pending_color = HALL_PIECE_COLOR_NONE;
        st->stable_ticks = 0;
        st->occupied = false;
        st->reported = false;
        st->prev_occupied = false;
        st->stable_scans = 0;
        st->peak_energy = 0;
        st->settle_scans = 0;
        st->prove_scans = 0;
        return 0;
    }

    /* Zero only from a stored empty baseline, or from squares the logical
     * game says are empty. Never treat "whatever is on the board" as empty. */
    if (!st->initialized) {
        bool allow_empty_cal = s_logical_known && s_logical_empty[sq] != 0 &&
                               hall_clean_neighborhood(sq, s_logical_empty);
        if (allow_empty_cal) {
            if (st->cal_count < HALL_POC_BOOT_CAL_SCANS) {
                st->cal_sum[0] += r0;
                st->cal_sum[1] += r1;
                st->cal_count++;
                if (st->cal_count < HALL_POC_BOOT_CAL_SCANS) {
                    return 0;
                }
                int32_t m0 = st->cal_sum[0] / HALL_POC_BOOT_CAL_SCANS;
                int32_t m1 = st->cal_sum[1] / HALL_POC_BOOT_CAL_SCANS;
                st->initialized = true;
                st->provisional = false;
                hall_zero_begin(st, m0, m1);
            }
        } else if (s_logical_known) {
            st->cal_count = 0;
            st->cal_sum[0] = 0;
            st->cal_sum[1] = 0;
            if (!hall_assign_provisional(st)) {
                return 0;
            }
        } else {
            return 0;
        }
    }

    hall_poc_ring_push(st, r0, r1);
    if (st->ring_count < HALL_POC_SAMPLES_PER_CYCLE) {
        return st->reported ? 1u : 0u;
    }

    int filt0 = hall_poc_trimmed_mean_u16(st->ring_r0);
    int filt1 = hall_poc_trimmed_mean_u16(st->ring_r1);

    int32_t dev0 = (int32_t)filt0 - st->midpoint[0];
    int32_t dev1 = (int32_t)filt1 - st->midpoint[1];
    int32_t ad0 = abs(dev0);
    int32_t ad1 = abs(dev1);
    int32_t delta = ad0 + ad1;
    int32_t step0 = dev0 - st->prev_dev0;
    int32_t step1 = dev1 - st->prev_dev1;
    int32_t step = abs(step0) + abs(step1);
    int32_t tol = delta / HALL_STABLE_REL_DIV;
    if (tol < HALL_STABLE_ABS_TOL) {
        tol = HALL_STABLE_ABS_TOL;
    }
    if (st->live_valid && step <= tol) {
        if (st->stable_scans < 255) {
            st->stable_scans++;
        }
    } else {
        st->stable_scans = 0;
    }
    st->prev_dev0 = dev0;
    st->prev_dev1 = dev1;

    int32_t energy = dev0 * dev0 + dev1 * dev1;
    st->live_energy = energy;
    st->live_dev0 = dev0;
    st->live_dev1 = dev1;
    st->live_valid = true;
    /* Lift stays two vacant scans. What changed is which reading counts as
     * vacant: king bleed (empty d8, energy 15725, delta up to ~177) sits
     * above DELTA_OFF (160) and below DELTA_ON (270), so a lifted square
     * never opened. 18000 is under the weakest seated pawn (20098). */
    bool royal = hall_neighbor_is_royal(sq);
    /* Dump energy is the empty-reference (ctx) residual once a square is
     * stable. A stale midpoint still looks like a piece (e1 after the king
     * left: ctx energy 8744, type None, occupied 1). Veto that. Delta alone
     * must not hold or latch under the same floor. */
    int32_t c0 = dev0;
    int32_t c1 = dev1;
    int32_t ctx_energy = energy;
    bool ctx_low = false;
    if (st->ctx_valid) {
        c0 = (int32_t)filt0 - st->ctx_ref[0];
        c1 = (int32_t)filt1 - st->ctx_ref[1];
        ctx_energy = c0 * c0 + c1 * c1;
        if (ctx_energy < HALL_REOCCUPY_ENERGY &&
            st->current_piece == HALL_PIECE_TYPE_NONE) {
            ctx_low = true;
        }
    }
    /* Crosstalk stored in ctx_ref shrinks filt-midpoint. The piece still
     * shows up as filt-ctx_ref, so keep whichever residual is stronger. */
    int32_t fringe0 = dev0;
    int32_t fringe1 = dev1;
    int32_t eff_energy = energy;
    if (st->ctx_valid && ctx_energy > energy) {
        fringe0 = c0;
        fringe1 = c1;
        eff_energy = ctx_energy;
    }
    bool signal = energy >= HALL_REOCCUPY_ENERGY ||
                  (st->ctx_valid && ctx_energy >= HALL_REOCCUPY_ENERGY);
    bool above = !ctx_low &&
                 !hall_opposite_fringe(sq, fringe0, fringe1, eff_energy) &&
                 signal;
    /* A piece already published beside a king keeps its seat. A one-scan
     * spike that never became reported must still earn the four scans. */
    if (royal && st->occupied && st->reported && !st->lift_block && above &&
        st->prove_scans < HALL_REOCCUPY_SCANS) {
        st->prove_scans = HALL_REOCCUPY_SCANS;
    }
    bool strict = st->lift_block || royal;
    bool sensor_present;
    if (strict) {
        if (above) {
            if (st->prove_scans == 0 && st->stable_scans > 1) {
                st->stable_scans = 1;
            }
            if (st->prove_scans < 255) {
                st->prove_scans++;
            }
        } else if (st->prove_scans < HALL_REOCCUPY_SCANS) {
            st->prove_scans = 0;
        }
        sensor_present = above && st->prove_scans >= HALL_REOCCUPY_SCANS;
    } else if (!above) {
        sensor_present = false;
    } else {
        sensor_present = true;
    }
    if (!st->occupied || sensor_present) {
        st->vacant_scans = 0;
    } else if (st->vacant_scans < 255) {
        st->vacant_scans++;
    }
    /* Mask still rides through a dip of a piece that is really there.
     * It must not keep a square that has already fallen through the gap. */
    bool mask_hold = st->occupied && st->mask_scans > 0 && above;
    bool present = sensor_present || mask_hold ||
                   (st->occupied &&
                    st->current_piece != HALL_PIECE_TYPE_NONE &&
                    st->vacant_scans < HALL_LIFT_CONFIRM_SCANS);
    if (present != st->occupied && !(present && strict)) {
        st->stable_scans = 0;
    }
    bool hold_neighbor = st->mask_scans > 0;
    if (hold_neighbor) {
        st->mask_scans--;
    }
    bool ctx_hot = st->ctx_arm > 0;
    if (ctx_hot) {
        st->ctx_arm--;
    }

    if (!present) {
        if (st->occupied && st->reported) {
            st->lift_block = true;
        }
        if (st->occupied) {
            st->prove_scans = 0;
        }
        st->occupied = false;
        st->reported = false;
        st->peak_energy = 0;
        st->peak_dev0 = 0;
        st->peak_dev1 = 0;
        st->settle_scans = 0;
        st->pending_piece = HALL_PIECE_TYPE_NONE;
        st->pending_color = HALL_PIECE_COLOR_NONE;
        st->stable_ticks = 0;
        if (st->stable_scans >= HALL_STABLE_SCANS &&
            (st->provisional || delta <= HALL_CTX_MAX_SHIFT ||
             energy < HALL_REOCCUPY_ENERGY)) {
            int32_t held = 0;
            if (st->ctx_valid) {
                held = abs(st->ctx_ref[0] - st->midpoint[0]) +
                       abs(st->ctx_ref[1] - st->midpoint[1]);
            }
            /* A slow piece grows away from the zero. Follow that only after a
             * neighbor actually moved; otherwise the reference would swallow it. */
            bool outward = st->ctx_valid && delta > held + HALL_STABLE_ABS_TOL;
            if (!outward || st->ctx_arm > 0) {
                if (!st->ctx_valid) {
                    st->ctx_ref[0] = filt0;
                    st->ctx_ref[1] = filt1;
                    st->ctx_valid = true;
                } else {
                    st->ctx_ref[0] += ((int32_t)filt0 - st->ctx_ref[0]) / HALL_CTX_EMA_DIV;
                    st->ctx_ref[1] += ((int32_t)filt1 - st->ctx_ref[1]) / HALL_CTX_EMA_DIV;
                }
            }
        }
        if (st->current_piece != HALL_PIECE_TYPE_NONE) {
            st->current_piece = HALL_PIECE_TYPE_NONE;
            st->current_color = HALL_PIECE_COLOR_NONE;
            if (!s_mag_on) {
                ESP_LOGI(TAG, "sq %u empty E=%ld", (unsigned)sq, (long)energy);
            }
        }
        st->ever_locked = true;
    } else {
        st->occupied = true;
        /* Occupancy is published once the piece itself is steady. A neighbor
         * move may delay the name, but it must not keep the square "empty"
         * or the setup light stays on after the piece is already there. */
        if (st->stable_scans >= HALL_STABLE_SCANS && above) {
            st->reported = true;
        }
        /* Do not rename a piece from the collapsing field as it leaves.
         * A neighbor that just moved must not retitle a piece still seated. */
        bool neighbor_lock = st->occupied && st->reported &&
                             st->current_piece != HALL_PIECE_TYPE_NONE &&
                             ctx_hot;
        if (!hold_neighbor && !neighbor_lock && sensor_present &&
            st->vacant_scans == 0 &&
            st->stable_scans >= HALL_STABLE_SCANS) {
            int32_t base0 = st->ctx_valid ? st->ctx_ref[0] : st->midpoint[0];
            int32_t base1 = st->ctx_valid ? st->ctx_ref[1] : st->midpoint[1];
            int32_t c0 = (int32_t)filt0 - base0;
            int32_t c1 = (int32_t)filt1 - base1;
            energy = c0 * c0 + c1 * c1;
            st->live_energy = energy;
            st->live_dev0 = c0;
            st->live_dev1 = c1;
            /* 10 % window around the type already on this square. A new piece
             * still has to land in its own band; the window only stops flicker.
             * Gain stays at 1 until this square has four confirmed landings. */
            const hall_poc_threshold_t *thr =
                hall_poc_classify(hall_gain_normalize((uint8_t)sq, energy),
                                  st->current_piece);
            hall_piece_type_t raw_piece =
                (thr != NULL) ? thr->type : HALL_PIECE_TYPE_NONE;
            if (raw_piece == HALL_PIECE_TYPE_ROOK ||
                raw_piece == HALL_PIECE_TYPE_QUEEN) {
                raw_piece = hall_name_rook_or_royal(c0, c1);
            }
            int32_t dominant = (abs(c0) > abs(c1)) ? c0 : c1;
            hall_piece_color_t raw_color = (dominant > 0)
                                               ? HALL_PIECE_COLOR_WHITE
                                               : HALL_PIECE_COLOR_BLACK;
            if (raw_piece == HALL_PIECE_TYPE_NONE) {
                raw_color = HALL_PIECE_COLOR_NONE;
            }
            /* A seated piece keeps its color. A pawn is not promoted to
             * knight or bishop by a royal neighbor that has not lifted it. */
            if (st->occupied && st->reported &&
                st->current_piece != HALL_PIECE_TYPE_NONE &&
                st->vacant_scans == 0) {
                bool color_flip = st->current_color != HALL_PIECE_COLOR_NONE &&
                                  raw_color != HALL_PIECE_COLOR_NONE &&
                                  raw_color != st->current_color;
                bool pawn_bump = st->current_piece == HALL_PIECE_TYPE_PAWN &&
                                 (raw_piece == HALL_PIECE_TYPE_KNIGHT ||
                                  raw_piece == HALL_PIECE_TYPE_BISHOP);
                if (color_flip || pawn_bump) {
                    raw_piece = st->current_piece;
                    raw_color = st->current_color;
                }
            }
            /* The guide lights a square from this name. One noisy scan used
             * to retitle a piece that had not moved, and the color blinked.
             * The first name still lands immediately. A later change has to
             * repeat. Queen and king keep the longer switch. */
            bool same = st->current_piece == raw_piece &&
                        st->current_color == raw_color;
            if (same) {
                st->pending_piece = HALL_PIECE_TYPE_NONE;
                st->pending_color = HALL_PIECE_COLOR_NONE;
                st->stable_ticks = 0;
            } else if (st->current_piece == HALL_PIECE_TYPE_NONE &&
                       raw_piece != HALL_PIECE_TYPE_NONE) {
                st->ever_locked = true;
                st->current_piece = raw_piece;
                st->current_color = raw_color;
                st->pending_piece = HALL_PIECE_TYPE_NONE;
                st->pending_color = HALL_PIECE_COLOR_NONE;
                st->stable_ticks = 0;
                if (!s_mag_on) {
                    ESP_LOGI(TAG, "sq %u %s %s E=%ld d0=%ld d1=%ld",
                             (unsigned)sq,
                             hall_i2c_squares_piece_color_name(st->current_color),
                             hall_i2c_squares_piece_type_name(st->current_piece),
                             (long)energy, (long)c0, (long)c1);
                }
            } else if (st->pending_piece == raw_piece &&
                       st->pending_color == raw_color) {
                if (st->stable_ticks < 1000) {
                    st->stable_ticks++;
                }
            } else {
                st->pending_piece = raw_piece;
                st->pending_color = raw_color;
                st->stable_ticks = 1;
            }
            if (!same && st->stable_ticks > 0) {
                bool royal_pair =
                    (st->current_piece == HALL_PIECE_TYPE_QUEEN ||
                     st->current_piece == HALL_PIECE_TYPE_KING) &&
                    (raw_piece == HALL_PIECE_TYPE_QUEEN ||
                     raw_piece == HALL_PIECE_TYPE_KING);
                int need = royal_pair ? HALL_ROYAL_SWITCH_SCANS : HALL_STABLE_SCANS;
                if (st->stable_ticks >= need) {
                    st->ever_locked = true;
                    st->current_piece = raw_piece;
                    st->current_color = raw_color;
                    st->pending_piece = HALL_PIECE_TYPE_NONE;
                    st->pending_color = HALL_PIECE_COLOR_NONE;
                    st->stable_ticks = 0;
                    if (!s_mag_on) {
                        if (raw_piece == HALL_PIECE_TYPE_NONE) {
                            ESP_LOGI(TAG, "sq %u empty E=%ld", (unsigned)sq,
                                     (long)energy);
                        } else {
                            ESP_LOGI(TAG, "sq %u %s %s E=%ld d0=%ld d1=%ld",
                                     (unsigned)sq,
                                     hall_i2c_squares_piece_color_name(st->current_color),
                                     hall_i2c_squares_piece_type_name(st->current_piece),
                                     (long)energy, (long)c0, (long)c1);
                        }
                    }
                }
            }
        }
    }

    /* Stale midpoint after a piece leaves. Ctx still says empty. Walk one
     * count per stable window, and only inside the drift cap. */
    if (ctx_low && !st->occupied && st->stable_scans >= HALL_STABLE_SCANS) {
        int32_t md0 = (int32_t)filt0 - st->midpoint[0];
        int32_t md1 = (int32_t)filt1 - st->midpoint[1];
        if (abs(md0) <= HALL_DRIFT_MAX_DEV && abs(md1) <= HALL_DRIFT_MAX_DEV &&
            (md0 != 0 || md1 != 0)) {
            if (st->drift_phase + 1u < HALL_STABLE_SCANS) {
                st->drift_phase++;
            } else {
                st->drift_phase = 0;
                if (md0 > 0) {
                    st->midpoint[0]++;
                } else if (md0 < 0) {
                    st->midpoint[0]--;
                }
                if (md1 > 0) {
                    st->midpoint[1]++;
                } else if (md1 < 0) {
                    st->midpoint[1]--;
                }
            }
        }
    } else if (!ctx_low) {
        st->drift_phase = 0;
    }

    /* Auto-zero only on a square that is really empty, not while a piece is settling.
     * The magnet bench freezes its square so later magnets share one baseline. */
    if ((int)sq == s_mag_freeze_sq) {
        st->empty_ticks = 0;
    } else if (!st->occupied && s_logical_known &&
        hall_clean_neighborhood(sq, s_logical_empty) &&
        abs(dev0) < HALL_POC_AUTOZERO_DEV_LIMIT &&
        abs(dev1) < HALL_POC_AUTOZERO_DEV_LIMIT) {
        st->empty_ticks++;
        if (st->empty_ticks > HALL_POC_AUTOZERO_EMPTY_SCANS &&
            (st->empty_ticks % HALL_POC_AUTOZERO_BUMP_SCANS) == 0) {
            int32_t n0 = st->midpoint[0];
            int32_t n1 = st->midpoint[1];
            if (filt0 > n0) n0++;
            else if (filt0 < n0) n0--;
            if (filt1 > n1) n1++;
            else if (filt1 < n1) n1--;
            hall_zero_begin(st, n0, n1);
        }
    } else {
        st->empty_ticks = 0;
    }

    if (st->occupied != st->prev_occupied) {
        hall_mask_neighbors(sq);
        st->prev_occupied = st->occupied;
    }

    if (st->zero_quarantine > 0) {
        if (hall_zero_contaminated(sq)) {
            hall_zero_discard(sq);
        } else {
            st->zero_quarantine--;
            if (st->zero_quarantine == 0) {
                hall_zero_commit(st);
            }
        }
    }

    st->identity_settled = sensor_present && st->vacant_scans == 0 &&
                           st->occupied &&
                           st->stable_scans >= HALL_STABLE_SCANS &&
                           st->current_piece != HALL_PIECE_TYPE_NONE &&
                           st->current_color != HALL_PIECE_COLOR_NONE;
    return st->reported ? 1u : 0u;
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
  memset(s_logical_type, 0, sizeof(s_logical_type));
  hall_gain_reset();
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

static uint8_t s_seg_backoff[4];

typedef struct {
  uint8_t valid;
  uint8_t seg_ok[4];
  uint8_t payload[4][HALL_I2C_PAYLOAD_BYTES];
} hall_scan_scratch_t;

static hall_scan_scratch_t s_hall_scan;

static unsigned hall_cfg_segment_count(void) {
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
  return seg_max;
}

void hall_i2c_squares_prepare_scan(void) {
  s_hall_scan.valid = 0;
  if (!s_i2c_ready) {
    return;
  }

  unsigned seg_max = hall_cfg_segment_count();
  memset(s_hall_scan.seg_ok, 0, sizeof(s_hall_scan.seg_ok));
  for (unsigned seg = 0; seg < seg_max; seg++) {
    if (s_seg_backoff[seg] > 0) {
      s_seg_backoff[seg]--;
      continue;
    }
    uint8_t addr = segment_addr(seg);
    esp_err_t err = hall_i2c_read_at(addr, s_hall_scan.payload[seg],
                                    HALL_I2C_PAYLOAD_BYTES,
                                    HALL_I2C_READ_TIMEOUT_MS);
    if (err != ESP_OK) {
      s_seg_backoff[seg] = HALL_I2C_FAIL_BACKOFF_SCANS;
      uint8_t bit = (uint8_t)(1u << seg);
      if ((s_read_fail_warned_mask & bit) == 0) {
        ESP_LOGW(TAG,
                 "segment %u addr 0x%02x read failed: %s "
                 "(backing off %u scans)",
                 seg, addr, esp_err_to_name(err),
                 (unsigned)HALL_I2C_FAIL_BACKOFF_SCANS);
        s_read_fail_warned_mask |= bit;
      } else {
        ESP_LOGD(TAG, "segment %u addr 0x%02x read failed: %s", seg, addr,
                 esp_err_to_name(err));
      }
      continue;
    }
    s_read_fail_warned_mask &= (uint8_t)~((uint8_t)(1u << seg));
    s_hall_scan.seg_ok[seg] = 1;
  }
  s_hall_scan.valid = 1;
}

bool hall_i2c_squares_segments_ok(void) {
  if (!s_i2c_ready) {
    return false;
  }
  unsigned seg_max = hall_cfg_segment_count();
  for (unsigned seg = 0; seg < seg_max; seg++) {
    if (!s_hall_scan.seg_ok[seg]) {
      return false;
    }
  }
  return true;
}

void hall_i2c_squares_publish_scan(uint8_t squares_state[64]) {
  if (!s_i2c_ready || squares_state == NULL || !s_hall_scan.valid) {
    return;
  }
  s_hall_scan.valid = 0;

  static uint32_t s_scan;
  s_scan++;
#if CONFIG_CHESS_HALL_LOG_INTERVAL_SCANS > 0
  bool do_log = (s_scan % (uint32_t)CONFIG_CHESS_HALL_LOG_INTERVAL_SCANS) == 1;
#else
  bool do_log = false;
#endif

  unsigned seg_max = hall_cfg_segment_count();
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
    if (!s_hall_scan.seg_ok[seg]) {
      continue;
    }
    uint8_t addr = segment_addr(seg);
    const uint8_t *buf = s_hall_scan.payload[seg];
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

void hall_i2c_squares_fill_state(uint8_t squares_state[64]) {
  hall_i2c_squares_prepare_scan();
  hall_i2c_squares_publish_scan(squares_state);
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
  for (int i = 0; i < HALL_POC_THRESHOLD_COUNT; i++) {
    if (energy >= s_poc_thresholds[i].min_energy &&
        energy <= s_poc_thresholds[i].max_energy) {
      return s_poc_thresholds[i].type;
    }
  }
  return HALL_PIECE_TYPE_NONE;
}

static void hall_dbg_log_settled(unsigned seg, unsigned field, uint8_t row,
                                 uint8_t col) {
  int32_t energy = s_dbg_peak_e[seg][field];
  int s0 = s_dbg_peak_s0[seg][field];
  int s1 = s_dbg_peak_s1[seg][field];
  int d0 = (s0 < 0) ? -s0 : s0;
  int d1 = (s1 < 0) ? -s1 : s1;
  hall_piece_type_t piece = hall_dbg_piece_type(energy);
  if (piece == HALL_PIECE_TYPE_ROOK || piece == HALL_PIECE_TYPE_QUEEN) {
    piece = hall_name_rook_or_royal(s0, s1);
  }
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

void hall_i2c_squares_prepare_scan(void) {}

void hall_i2c_squares_publish_scan(uint8_t squares_state[64]) {
  (void)squares_state;
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

bool hall_i2c_squares_segments_ok(void) { return true; }

void hall_i2c_squares_set_logical_empty(const uint8_t empty_squares[64],
                                        const uint8_t piece_types[64]) {
  (void)empty_squares;
  (void)piece_types;
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

bool hall_i2c_squares_identity_settled(uint8_t square) {
  if (square >= 64) {
    return false;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return s_poc_state[square].identity_settled;
#else
  return false;
#endif
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

bool hall_i2c_squares_get_reading(uint8_t square, hall_square_reading_t *out) {
  if (square >= 64 || out == NULL) {
    return false;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  const hall_poc_square_state_t *st = &s_poc_state[square];
  out->energy = st->live_energy;
  out->dev0 = st->live_dev0;
  out->dev1 = st->live_dev1;
  out->type = st->current_piece;
  out->color = st->current_color;
  out->occupied = st->occupied ? 1u : 0u;
  out->valid = st->live_valid ? 1u : 0u;
  return true;
#else
  (void)out;
  return false;
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

void hall_i2c_squares_note_confirmed(uint8_t square, hall_piece_type_t type,
                                    hall_piece_color_t color) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  hall_gain_note(square, type, color);
#else
  (void)square;
  (void)type;
  (void)color;
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
  printf("%ld %ld %ld\n", (long)st->live_energy, (long)st->live_dev0,
         (long)st->live_dev1);
  fflush(stdout);
#endif
}
