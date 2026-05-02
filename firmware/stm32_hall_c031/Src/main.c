#include "main.h"

#include "board_matrix_pins.h"
#ifdef BOARD_NUCLEO_C031
#include "board_nucleo_c031.h"
#endif

#include <string.h>

volatile uint8_t g_hall_pointer_reg;
volatile unsigned g_hall_tx_idx;
volatile uint8_t g_hall_payload_raw[HALL_I2C_PAYLOAD_BYTES];
volatile uint8_t g_hall_payload_ver[HALL_I2C_PAYLOAD_BYTES];
volatile const uint8_t *volatile g_hall_active_tx;

static void MX_I2C1_Init(void);
#if !defined(BOARD_NUCLEO_C031) && !defined(BOARD_HALL_I2C_DEMO)
static void MX_MATRIX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void Matrix_ADC_Activate(void);
#elif defined(BOARD_NUCLEO_C031)
static void MX_Nucleo_UserIo_Init(void);
#endif

/* Konzervativní prodleva mezi kalibrací a ENABLE (ST příklad ×32 pro async). */
#define ADC_DELAY_CALIB_ENABLE_CPU_CYCLES                                              \
  ((uint32_t)(LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 32U))

#if !defined(BOARD_NUCLEO_C031) && !defined(BOARD_HALL_I2C_DEMO)

static void Matrix_GPIO_SetMuxAddress(unsigned ch) {
  uint32_t odr = LL_GPIO_ReadOutputPort(MATRIX_ADC_COM1_PORT);
  odr &= ~(uint32_t)MATRIX_MUX_ADDR_PINS;
  if ((ch & 1u) != 0u) {
    odr |= (uint32_t)MATRIX_MUX_S0_PIN;
  }
  if ((ch & 2u) != 0u) {
    odr |= (uint32_t)MATRIX_MUX_S1_PIN;
  }
  if ((ch & 4u) != 0u) {
    odr |= (uint32_t)MATRIX_MUX_S2_PIN;
  }
  if ((ch & 8u) != 0u) {
    odr |= (uint32_t)MATRIX_MUX_S3_PIN;
  }
  LL_GPIO_WriteOutputPort(MATRIX_ADC_COM1_PORT, odr);
}

/** ic 0 = U37 (COM1→PA0), 1 = U38 (COM2→PA1). */
static void Matrix_MuxSelect(unsigned ic, unsigned ch4) {
  Matrix_GPIO_SetMuxAddress(ch4 & 15u);
  /* Nejprve oba muxy vypnuté (E=H). */
  LL_GPIO_SetOutputPin(MATRIX_ADC_COM1_PORT,
                       MATRIX_MUX_E_U37_PIN | MATRIX_MUX_E_U38_PIN);
  for (volatile unsigned w = 0U; w < 80U; w++) {
  }
  if (ic == 0u) {
    LL_GPIO_ResetOutputPin(MATRIX_ADC_COM1_PORT, MATRIX_MUX_E_U37_PIN);
    LL_GPIO_SetOutputPin(MATRIX_ADC_COM1_PORT, MATRIX_MUX_E_U38_PIN);
  } else {
    LL_GPIO_SetOutputPin(MATRIX_ADC_COM1_PORT, MATRIX_MUX_E_U37_PIN);
    LL_GPIO_ResetOutputPin(MATRIX_ADC_COM1_PORT, MATRIX_MUX_E_U38_PIN);
  }
  for (volatile unsigned w = 0U; w < 400U; w++) {
  }
}

static void Matrix_ADC_ConfigureChannel(uint32_t channel_ll_mask) {
  LL_ADC_REG_SetSequencerChannels(ADC1, channel_ll_mask);
  while (LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0) {
  }
  LL_ADC_ClearFlag_CCRDY(ADC1);
}

static uint16_t Matrix_ADC_ReadUint12(void) {
  if ((LL_ADC_IsEnabled(ADC1) != 0U) &&
      (LL_ADC_IsDisableOngoing(ADC1) == 0U) &&
      (LL_ADC_REG_IsConversionOngoing(ADC1) == 0U)) {
    LL_ADC_REG_StartConversion(ADC1);
  }
  while (LL_ADC_IsActiveFlag_EOC(ADC1) == 0) {
  }
  uint32_t d = LL_ADC_REG_ReadConversionData32(ADC1);
  LL_ADC_ClearFlag_EOC(ADC1);
  return (uint16_t)(d & 0xFFFu);
}

static void MX_MATRIX_GPIO_Init(void) {
  LL_GPIO_InitTypeDef g = {0};

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

  g.Pin = MATRIX_ADC_COM1_PIN | MATRIX_ADC_COM2_PIN;
  g.Mode = LL_GPIO_MODE_ANALOG;
  g.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(MATRIX_ADC_COM1_PORT, &g);

  g.Pin = MATRIX_MUX_ADDR_PINS | MATRIX_MUX_E_U37_PIN | MATRIX_MUX_E_U38_PIN;
  g.Mode = LL_GPIO_MODE_OUTPUT;
  g.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  g.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  g.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(MATRIX_ADC_COM1_PORT, &g);

  LL_GPIO_SetOutputPin(MATRIX_ADC_COM1_PORT,
                       MATRIX_MUX_E_U37_PIN | MATRIX_MUX_E_U38_PIN);
}

static void MX_ADC1_Init(void) {
  LL_ADC_InitTypeDef adc = {0};
  LL_ADC_REG_InitTypeDef reg = {0};

  LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC);

  adc.Clock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
  adc.Resolution = LL_ADC_RESOLUTION_12B;
  adc.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  adc.LowPowerMode = LL_ADC_LP_MODE_NONE;
  LL_ADC_Init(ADC1, &adc);

  LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_FIXED);
  reg.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
  reg.SequencerLength = LL_ADC_REG_SEQ_SCAN_DISABLE;
  reg.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  reg.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
  reg.DMATransfer = LL_ADC_REG_DMA_TRANSFER_NONE;
  reg.Overrun = LL_ADC_REG_OVR_DATA_OVERWRITTEN;
  LL_ADC_REG_Init(ADC1, &reg);
  LL_ADC_REG_SetSequencerScanDirection(ADC1, LL_ADC_REG_SEQ_SCAN_DIR_FORWARD);
  LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
  LL_ADC_SetTriggerFrequencyMode(ADC1, LL_ADC_CLOCK_FREQ_MODE_HIGH);

  Matrix_ADC_ConfigureChannel(LL_ADC_CHANNEL_0);
  LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1,
                                       LL_ADC_SAMPLINGTIME_79CYCLES_5);
  LL_ADC_DisableIT_EOC(ADC1);
  LL_ADC_DisableIT_EOS(ADC1);
  LL_ADC_DisableIT_OVR(ADC1);
}

static void Matrix_ADC_Activate(void) {
  uint32_t wait_loop_index;
  uint32_t backup_dma;

  if (LL_ADC_IsEnabled(ADC1) != 0U) {
    return;
  }

  LL_ADC_EnableInternalRegulator(ADC1);
  wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US *
                      (SystemCoreClock / (100000U * 2U))) /
                     10U);
  while (wait_loop_index != 0U) {
    wait_loop_index--;
  }

  backup_dma = LL_ADC_REG_GetDMATransfer(ADC1);
  LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);
  LL_ADC_StartCalibration(ADC1);
  while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0U) {
  }
  LL_ADC_REG_SetDMATransfer(ADC1, backup_dma);

  wait_loop_index = (ADC_DELAY_CALIB_ENABLE_CPU_CYCLES >> 1U);
  while (wait_loop_index != 0U) {
    wait_loop_index--;
  }

  LL_ADC_Enable(ADC1);
  while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0U) {
  }
}

#elif defined(BOARD_NUCLEO_C031)

static void MX_Nucleo_UserIo_Init(void) {
  LL_GPIO_InitTypeDef g = {0};

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);

  g.Pin = BOARD_NUCLEO_LD4_PIN;
  g.Mode = LL_GPIO_MODE_OUTPUT;
  g.Speed = LL_GPIO_SPEED_FREQ_LOW;
  g.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  g.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(BOARD_NUCLEO_LD4_GPIO_PORT, &g);
  LL_GPIO_ResetOutputPin(BOARD_NUCLEO_LD4_GPIO_PORT, BOARD_NUCLEO_LD4_PIN);

  g.Pin = BOARD_NUCLEO_USER_PIN;
  g.Mode = LL_GPIO_MODE_INPUT;
  g.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(BOARD_NUCLEO_USER_GPIO_PORT, &g);
}

#endif /* matrix / Nucleo */

static void MX_I2C1_Init(void) {
  LL_GPIO_InitTypeDef gpio = {0};
  LL_I2C_InitTypeDef i2c = {0};

  LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);

  gpio.Pin = MATRIX_I2C_SCL_PIN | MATRIX_I2C_SDA_PIN;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  gpio.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  gpio.Pull = LL_GPIO_PULL_UP;
  gpio.Alternate = MATRIX_I2C_AF;
  LL_GPIO_Init(MATRIX_I2C_GPIO_PORT, &gpio);

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);

  NVIC_SetPriority(I2C1_IRQn, 0);
  NVIC_EnableIRQ(I2C1_IRQn);

  i2c.PeripheralMode = LL_I2C_MODE_I2C;
  i2c.Timing = 0x0090273DU;
  i2c.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
  i2c.DigitalFilter = 2;
  i2c.OwnAddress1 = HALL_I2C_ADDR_MATCH;
  i2c.TypeAcknowledge = LL_I2C_ACK;
  i2c.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
  LL_I2C_Init(I2C1, &i2c);
  LL_I2C_EnableAutoEndMode(I2C1);
  LL_I2C_SetOwnAddress2(I2C1, 0, LL_I2C_OWNADDRESS2_NOMASK);
  LL_I2C_DisableOwnAddress2(I2C1);
  LL_I2C_DisableGeneralCall(I2C1);
  LL_I2C_EnableClockStretching(I2C1);

  {
    uint32_t timing =
        __LL_I2C_CONVERT_TIMINGS(0x0, 0xC, 0x0, 0x21, 0x6C);
    LL_I2C_SetTiming(I2C1, timing);
  }

  LL_I2C_SetOwnAddress1(I2C1, HALL_I2C_ADDR_MATCH, LL_I2C_OWNADDRESS1_7BIT);
  LL_I2C_EnableOwnAddress1(I2C1);

  LL_I2C_EnableIT_ADDR(I2C1);
  LL_I2C_EnableIT_NACK(I2C1);
  LL_I2C_EnableIT_ERR(I2C1);
  LL_I2C_EnableIT_STOP(I2C1);
}

void SystemClock_Config(void) {
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

  LL_RCC_HSI_Enable();
  while (LL_RCC_HSI_IsReady() != 1) {
  }

  LL_RCC_HSI_SetCalibTrimming(64);
  LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);
  LL_RCC_SetAHBPrescaler(LL_RCC_HCLK_DIV_1);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {
  }

  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_Init1msTick(48000000);
  LL_SetSystemCoreClock(48000000);
}

void Hall_RefreshPayload(void) {
#if defined(BOARD_HALL_I2C_DEMO)
  /*
   * I²C stejné jako produkční Hall slave — bez ADC/muxů.
   * Generuje vzorky tak, aby ESP (CHESS_HALL_PAIR_DIFF_THRESHOLD ~120) viděl
   * měnící se „obsazenost“ polí: |s0−s1| buď 0 nebo 350.
   */
  static uint32_t s_demo_ctr;
  uint8_t packed[HALL_I2C_PAYLOAD_BYTES];

  s_demo_ctr++;
  unsigned phase = (unsigned)(s_demo_ctr >> 16);

  for (unsigned field = 0U; field < HALL_I2C_FIELDS_PER_SEGMENT; field++) {
    uint16_t s0 =
        (uint16_t)(400U + field * 20U + (phase & 0x3FU));
    unsigned occ_patt = ((field + phase) % 3U) != 0U;
    uint16_t s1 = occ_patt ? (uint16_t)(s0 + 350U) : s0;

    hall_i2c_spec_write_le16(&packed[field * 4U + 0U], s0);
    hall_i2c_spec_write_le16(&packed[field * 4U + 2U], s1);
  }

  __disable_irq();
  for (unsigned i = 0U; i < sizeof packed; i++) {
    g_hall_payload_raw[i] = packed[i];
  }
  __enable_irq();
#elif !defined(BOARD_NUCLEO_C031)
  uint16_t samples[HALL_I2C_UINT16_PER_SEGMENT];
  uint8_t packed[HALL_I2C_PAYLOAD_BYTES];

  for (unsigned mux_ic = 0U; mux_ic < 2U; mux_ic++) {
    uint32_t adc_ch = (mux_ic == 0U) ? LL_ADC_CHANNEL_0 : LL_ADC_CHANNEL_1;
    Matrix_ADC_ConfigureChannel(adc_ch);
    for (unsigned ch = 0U; ch < 16U; ch++) {
      Matrix_MuxSelect(mux_ic, ch);
      samples[mux_ic * 16U + ch] = Matrix_ADC_ReadUint12();
    }
  }

  LL_GPIO_SetOutputPin(MATRIX_ADC_COM1_PORT,
                       MATRIX_MUX_E_U37_PIN | MATRIX_MUX_E_U38_PIN);

  for (unsigned si = 0U; si < HALL_I2C_UINT16_PER_SEGMENT; si++) {
    unsigned field = si / 2U;
    unsigned sensor = si % 2U;
    hall_i2c_spec_write_le16(&packed[field * 4U + sensor * 2U], samples[si]);
  }

  __disable_irq();
  for (unsigned i = 0U; i < sizeof packed; i++) {
    g_hall_payload_raw[i] = packed[i];
  }
  __enable_irq();
#else /* BOARD_NUCLEO_C031 */
  uint8_t packed[HALL_I2C_PAYLOAD_BYTES];
  memset(packed, 0, sizeof packed);
  __disable_irq();
  for (unsigned i = 0U; i < sizeof packed; i++) {
    g_hall_payload_raw[i] = packed[i];
  }
  __enable_irq();
#endif
}

int main(void) {
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  SystemClock_Config();

#if defined(BOARD_HALL_I2C_DEMO)
  /* PB6/PB7 I2C — žádný mux/ADC. */
#elif !defined(BOARD_NUCLEO_C031)
  MX_MATRIX_GPIO_Init();
  MX_ADC1_Init();
  Matrix_ADC_Activate();
#else
  MX_Nucleo_UserIo_Init();
#endif
  MX_I2C1_Init();

  memset((void *)g_hall_payload_ver, 0, sizeof(g_hall_payload_ver));
  g_hall_payload_ver[0] = HALL_I2C_PROTOCOL_VERSION;

  g_hall_active_tx = g_hall_payload_raw;
  Hall_RefreshPayload();

  for (;;) {
#ifdef BOARD_NUCLEO_C031
    {
      static uint8_t prev_down;
      uint32_t up = LL_GPIO_IsInputPinSet(BOARD_NUCLEO_USER_GPIO_PORT,
                                          BOARD_NUCLEO_USER_PIN);
      uint32_t down = (up == 0U);
      static uint8_t latched;
      if (down != 0U && prev_down == 0U) {
        latched = 1U;
      }
      prev_down = (uint8_t)down;
      if (latched != 0U) {
        LL_GPIO_SetOutputPin(BOARD_NUCLEO_LD4_GPIO_PORT, BOARD_NUCLEO_LD4_PIN);
      }
    }
#endif
    Hall_RefreshPayload();
  }
}
