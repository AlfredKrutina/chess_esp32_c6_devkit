#include "main.h"
#include "stm32c0xx_it.h"

void NMI_Handler(void) {}

void HardFault_Handler(void) {
  while (1) {
  }
}

void SVC_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) {}

static void hall_select_tx_buffer(void) {
  if (g_hall_pointer_reg == HALL_I2C_REG_POINTER_PROTO_VER) {
    g_hall_active_tx = g_hall_payload_ver;
  } else {
    g_hall_active_tx = g_hall_payload_raw;
  }
}

void I2C1_IRQHandler(void) {
  if (LL_I2C_IsActiveFlag_ADDR(I2C1)) {
    if (LL_I2C_GetAddressMatchCode(I2C1) != HALL_I2C_ADDR_MATCH) {
      LL_I2C_ClearFlag_ADDR(I2C1);
      return;
    }

    if (LL_I2C_GetTransferDirection(I2C1) == LL_I2C_DIRECTION_WRITE) {
      LL_I2C_ClearFlag_ADDR(I2C1);
      LL_I2C_EnableIT_RX(I2C1);
    } else {
      hall_select_tx_buffer();
      g_hall_tx_idx = 0;
      LL_I2C_ClearFlag_ADDR(I2C1);
      LL_I2C_EnableIT_TX(I2C1);
    }
    return;
  }

  if (LL_I2C_IsActiveFlag_RXNE(I2C1)) {
    g_hall_pointer_reg = LL_I2C_ReceiveData8(I2C1);
    LL_I2C_DisableIT_RX(I2C1);
    return;
  }

  if (LL_I2C_IsActiveFlag_NACK(I2C1)) {
    LL_I2C_ClearFlag_NACK(I2C1);
    return;
  }

  if (LL_I2C_IsActiveFlag_TXIS(I2C1)) {
    uint8_t b = 0;
    if (g_hall_active_tx != 0 && g_hall_tx_idx < HALL_I2C_PAYLOAD_BYTES) {
      b = g_hall_active_tx[g_hall_tx_idx++];
    }
    LL_I2C_TransmitData8(I2C1, b);
    return;
  }

  if (LL_I2C_IsActiveFlag_STOP(I2C1)) {
    LL_I2C_ClearFlag_STOP(I2C1);
    if (!LL_I2C_IsActiveFlag_TXE(I2C1)) {
      LL_I2C_ClearFlag_TXE(I2C1);
    }
    LL_I2C_DisableIT_TX(I2C1);
    g_hall_tx_idx = 0;
    return;
  }

  if (LL_I2C_IsActiveFlag_ARLO(I2C1)) {
    LL_I2C_ClearFlag_ARLO(I2C1);
    return;
  }
  if (LL_I2C_IsActiveFlag_BERR(I2C1)) {
    LL_I2C_ClearFlag_BERR(I2C1);
    return;
  }
  if (LL_I2C_IsActiveFlag_OVR(I2C1)) {
    LL_I2C_ClearFlag_OVR(I2C1);
    return;
  }

  if (!LL_I2C_IsActiveFlag_TXE(I2C1)) {
    return;
  }
}
