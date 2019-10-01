/*
 * mbed_shield_lcd_defines.h
 *
 *  Created on: 24. 9. 2017
 *      Author: weiss_000
 */

/**
 * Defines required symbols
 * = interface to other program parts
 */
#ifndef MBED_SHIELD_LCD_DEFINES_H_
#define MBED_SHIELD_LCD_DEFINES_H_

/**
 * Header file with Core function - set GPIO mode, set Alternate Function number
 */
#define _MBED_LCD_CORE_FUNCTIONS_H   "stm_core.h"

/**
 * Function for setting GPIO mode
 * Parameteres required: fn(GPIO_TypeDef *gpio, uint32_t bitnum, enum as uint ioMode)
 */
#define _MBED_LCD_NUCLEO_FN_SET_GPIO   Nucleo_SetPinGPIO
/**
 * Function for setting Alternate Funcion number
 * Parameteres required: fn(GPIO_TypeDef *gpio, uint32_t bitnum, uint32_t afValue)
 */
#define _MBED_LCD_NUCLEO_FN_SET_AF   Nucleo_SetAFGPIO
/**
 * Function for writing to GPIO in output mode
 * Parameteres required: fn(GPIO_TypeDef *gpio, uint32_t bitnum, bool state)
 */
#define _MBED_LCD_NUCLEO_FN_GPIO_WR  GPIOWrite
/**
 * Function for reading from GPIO in input mode
 * Parameteres required: fn(GPIO_TypeDef *gpio, uint32_t bitnum)
 * Returned bool
 */
#define _MBED_LCD_NUCLEO_FN_GPIO_RD  GPIORead

/**
 * Constant for ioMode parameter in ..SET_GPIO - mode Output with PushPull
 * Typically represented by enum
 */
#define _MBED_LCD_NUCLEO_ENUM_OUT_PP  ioPortOutputPP
/**
 * Constant for ioMode parameter in ..SET_GPIO - mode ALternate Funcion with PushPull
 * Typically represented by enum
 */
#define _MBED_LCD_NUCLEO_ENUM_AF_PP  ioPortAlternatePP

/**
 * Used SPI channel
 */
#define _MBED_LCD_SPI         SPI1
/**
 * Alternate Funcion number for GPIO AF signals (SCK and MOSI)
 */
#define _MBED_LCD_SPI_AF_NUM  5
/**
 * Definition of GPIO ports for concrete control signals
 */
#define _MBED_LCD_SPI_SCK     GPIOA,5       ///< SPI clock
#define _MBED_LCD_SPI_MOSI    GPIOA,7       ///< SPI MOSI signal
#define _MBED_LCD_PIN_RSTN    GPIOA,6       ///< display RST signal, active in LO
#define _MBED_LCD_PIN_CSN     GPIOB,6       ///< display CS signal, active in LO
#define _MBED_LCD_PIN_A0      GPIOA,8       ///< display A0 signal, LO = commands, HI = data

#endif /* MBED_SHIELD_LCD_DEFINES_H_ */
