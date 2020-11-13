/*
 ******************************************************************************
 * @file    wake_up.c
 * @author  Sensors Software Solution Team
 * @brief   This file show the simplest way to detect wake_up from sensor.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/*
 * This example was developed using the following STMicroelectronics
 * evaluation boards:
 *
 * - STEVAL_MKI109V3 + STEVAL-MKI206V1
 * - NUCLEO_F411RE + STEVAL-MKI206V1
 * - DISCOVERY_SPC584B + STEVAL-MKI206V1
 *
 * and STM32CubeMX tool with STM32CubeF4 MCU Package
 *
 * Used interfaces:
 *
 * STEVAL_MKI109V3    - Host side:   USB (Virtual COM)
 *                    - Sensor side: SPI(Default) / I2C(supported)
 *
 * NUCLEO_STM32F411RE - Host side: UART(COM) to USB bridge
 *                    - Sensor side: I2C(Default) / SPI(supported)
 *
 * DISCOVERY_SPC584B  - Host side: UART(COM) to USB bridge
 *                    - Sensor side: I2C(Default) / SPI(supported)
 *
 * If you need to run this example on a different hardware platform a
 * modification of the functions: `platform_write`, `platform_read`,
 * `tx_com` and 'platform_init' is required.
 *
 */

/* STMicroelectronics evaluation boards definition
 *
 * Please uncomment ONLY the evaluation boards in use.
 * If a different hardware is used please comment all
 * following target board and redefine yours.
 */

//#define STEVAL_MKI109V3  /* little endian */
#define NUCLEO_F411RE    /* little endian */
//#define SPC584B_DIS      /* big endian */

/* ATTENTION: By default the driver is little endian. If you need switch
 *            to big endian please see "Endianness definitions" in the
 *            header file of the driver (_reg.h).
 */


#if defined(STEVAL_MKI109V3)
/* MKI109V3: Define communication interface */
#define SENSOR_BUS hspi2
/* MKI109V3: Vdd and Vddio power supply values */
#define PWM_3V3 915

#elif defined(NUCLEO_F411RE)
/* NUCLEO_F411RE: Define communication interface */
#define SENSOR_BUS hi2c1

#elif defined(SPC584B_DIS)
/* DISCOVERY_SPC584B: Define communication interface */
#define SENSOR_BUS I2CD1

#endif

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "ais2dw12_reg.h"

#if defined(NUCLEO_F411RE)
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"

#elif defined(STEVAL_MKI109V3)
#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"
#include "gpio.h"
#include "spi.h"

#elif defined(SPC584B_DIS)
#include "components.h"
#endif

/* Private macro -------------------------------------------------------------*/
#define    BOOT_TIME            20 //ms

/* Private variables ---------------------------------------------------------*/
static uint8_t whoamI, rst;
static uint8_t tx_buffer[1000];

/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg,
                              uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);
static void tx_com( uint8_t *tx_buffer, uint16_t len );
static void platform_delay(uint32_t ms);
static void platform_init(void);

/* Main Example --------------------------------------------------------------*/
void ais2dw12_wake_up(void)
{
  /* Initialize mems driver interface */
  stmdev_ctx_t dev_ctx;
  ais2dw12_reg_t int_route;
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.handle = &SENSOR_BUS;
  /* Initialize platform specific hardware */
  platform_init();
  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);
  /* Check device ID */
  ais2dw12_device_id_get(&dev_ctx, &whoamI);

  if (whoamI != AIS2DW12_ID)
    while (1) {
      /* manage here device not found */
    }

  /* Restore default configuration */
  ais2dw12_reset_set(&dev_ctx, PROPERTY_ENABLE);

  do {
    ais2dw12_reset_get(&dev_ctx, &rst);
  } while (rst);

  /* Set full scale
   */
  ais2dw12_full_scale_set(&dev_ctx, AIS2DW12_2g);
  /* Configure power mode */
  ais2dw12_power_mode_set(&dev_ctx, AIS2DW12_PWR_MD_12bit);
  /* Set Output Data Rate */
  ais2dw12_data_rate_set(&dev_ctx, AIS2DW12_XL_ODR_100Hz);
  /* Apply high-pass digital filter on Wake-Up function */
  ais2dw12_filter_path_set(&dev_ctx, AIS2DW12_HIGH_PASS_ON_OUT);
  /*
   * Apply high-pass digital filter on Wake-Up function
   * Duration time is set to zero so Wake-Up interrupt signal
   * is generated for each X,Y,Z filtered data exceeding the
   * configured threshold
   */
  ais2dw12_wkup_dur_set(&dev_ctx, 0);
  /* Set wake-up threshold
   * Set Wake-Up threshold: 1 LSb corresponds to FS_XL/2^6
   */
  ais2dw12_wkup_threshold_set(&dev_ctx, 2);
  /* Enable interrupt generation on Wake-Up INT1 pin */
  ais2dw12_pin_int1_route_get(&dev_ctx, &int_route.ctrl4_int1);
  int_route.ctrl4_int1.int1_wu = PROPERTY_ENABLE;
  ais2dw12_pin_int1_route_set(&dev_ctx, &int_route.ctrl4_int1);

  /* Wait Events */
  while (1) {
    ais2dw12_all_sources_t all_source;
    /* Check Wake-Up events */
    ais2dw12_all_sources_get(&dev_ctx, &all_source);

    if (all_source.wake_up_src.wu_ia) {
      sprintf((char *)tx_buffer, "Wake-Up event on ");

      if (all_source.wake_up_src.x_wu) {
        strcat((char *)tx_buffer, "X");
      }

      if (all_source.wake_up_src.y_wu) {
        strcat((char *)tx_buffer, "Y");
      }

      if (all_source.wake_up_src.z_wu) {
        strcat((char *)tx_buffer, "Z");
      }

      strcat((char *)tx_buffer, " direction\r\n");
      tx_com(tx_buffer, strlen((char const *)tx_buffer));
    }
  }
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to write
 * @param  bufp      pointer to data to write in register reg
 * @param  len       number of consecutive register to write
 *
 */
static int32_t platform_write(void *handle, uint8_t reg,
                              uint8_t *bufp,
                              uint16_t len)
{
#if defined(NUCLEO_F411RE)
  HAL_I2C_Mem_Write(handle, AIS2DW12_I2C_ADD_H, reg,
                    I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
#elif defined(STEVAL_MKI109V3)
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(handle, &reg, 1, 1000);
  HAL_SPI_Transmit(handle, bufp, len, 1000);
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
#elif defined(SPC584B_DIS)
  i2c_lld_write(handle,  AIS2DW12_I2C_ADD_H & 0xFE, reg, bufp, len);
#endif
  return 0;
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
#if defined(NUCLEO_F411RE)
  HAL_I2C_Mem_Read(handle, AIS2DW12_I2C_ADD_H, reg,
                   I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
#elif defined(STEVAL_MKI109V3)
  reg |= 0x80;
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(handle, &reg, 1, 1000);
  HAL_SPI_Receive(handle, bufp, len, 1000);
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
#elif defined(SPC584B_DIS)
  i2c_lld_read(handle, AIS2DW12_I2C_ADD_H & 0xFE, reg, bufp, len);
#endif
  return 0;
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  tx_buffer     buffer to transmit
 * @param  len           number of byte to send
 *
 */
static void tx_com(uint8_t *tx_buffer, uint16_t len)
{
#if defined(NUCLEO_F411RE)
  HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
#elif defined(STEVAL_MKI109V3)
  CDC_Transmit_FS(tx_buffer, len);
#elif defined(SPC584B_DIS)
  sd_lld_write(&SD2, tx_buffer, len);
#endif
}

/*
 * @brief  platform specific delay (platform dependent)
 *
 * @param  ms        delay in ms
 *
 */
static void platform_delay(uint32_t ms)
{
#if defined(NUCLEO_F411RE) | defined(STEVAL_MKI109V3)
  HAL_Delay(ms);
#elif defined(SPC584B_DIS)
  osalThreadDelayMilliseconds(ms);
#endif
}

/*
 * @brief  platform specific initialization (platform dependent)
 */
static void platform_init(void)
{
#if defined(STEVAL_MKI109V3)
  TIM3->CCR1 = PWM_3V3;
  TIM3->CCR2 = PWM_3V3;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_Delay(1000);
#endif
}
