/**
 * @file    gpio.c
 * @brief   
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "LPC11Uxx.h"
#include "RTL.h"
#include "gpio.h"
#include "compiler.h"
#include "target_reset.h"
#include "IO_Config.h"
#include "settings.h"
#include "iap.h"

static void busy_wait(uint32_t cycles)
{
    volatile uint32_t i;
    i = cycles;

    while (i > 0) {
        i--;
    }
}

void gpio_init(void)
{
    // enable clock for GPIO port 0
    LPC_SYSCON->SYSAHBCLKCTRL |= (1UL << 6);
    
	//config the CFG 4 pins as input
    PIN_CFG0_IOCON = PIN_CFG0_IOCON_INIT;
    LPC_GPIO->CLR[PIN_CFG0_PORT] = PIN_CFG0;
    LPC_GPIO->DIR[PIN_CFG0_PORT] &= ~PIN_CFG0;
    PIN_CFG1_IOCON = PIN_CFG1_IOCON_INIT;
    LPC_GPIO->CLR[PIN_CFG1_PORT] = PIN_CFG1;
    LPC_GPIO->DIR[PIN_CFG1_PORT] &= ~PIN_CFG1;
    PIN_CFG2_IOCON = PIN_CFG2_IOCON_INIT;
    LPC_GPIO->CLR[PIN_CFG2_PORT] = PIN_CFG2;
    LPC_GPIO->DIR[PIN_CFG2_PORT] &= ~PIN_CFG2;
    PIN_CFG3_IOCON = PIN_CFG3_IOCON_INIT;
    LPC_GPIO->CLR[PIN_CFG3_PORT] = PIN_CFG3;
    LPC_GPIO->DIR[PIN_CFG3_PORT] &= ~PIN_CFG3; 
    
#if defined(TARGET_POWER_HOLD)
    // Target PowerHOLD port
    PIN_PWH_IOCON = PIN_PWH_IOCON_INIT;
    LPC_GPIO->CLR[PIN_PWH_PORT] = PIN_PWH;
    LPC_GPIO->DIR[PIN_PWH_PORT] |= PIN_PWH;
#endif
    // configure GPIO-LED as output
#if defined(CONTROLLED_POWER_LED)
    // Power led (red)
    PIN_POW_LED_IOCON = PIN_POW_LED_IOCON_INIT;
    LPC_GPIO->CLR[PIN_POW_LED_PORT] = PIN_POW_LED;
    LPC_GPIO->DIR[PIN_POW_LED_PORT] |= PIN_POW_LED;
#endif
    // configure GPIO-LED as output
    // DAP led (green)
    PIN_DAP_LED_IOCON = PIN_DAP_LED_IOCON_INIT;
    LPC_GPIO->SET[PIN_DAP_LED_PORT] = PIN_DAP_LED;
    LPC_GPIO->DIR[PIN_DAP_LED_PORT] |= PIN_DAP_LED;
    // Serial LED (blue)
    PIN_CDC_LED_IOCON = PIN_CDC_LED_IOCON_INIT;
    LPC_GPIO->SET[PIN_CDC_LED_PORT] = PIN_CDC_LED;
    LPC_GPIO->DIR[PIN_CDC_LED_PORT] |= PIN_CDC_LED;
    
    // configure Button(s) as input   
    PIN_RESET_IN_FWRD_IOCON = PIN_RESET_IN_FWRD_IOCON_INIT;
    LPC_GPIO->DIR[PIN_RESET_IN_FWRD_PORT] &= ~PIN_RESET_IN_FWRD;
    
#if !defined(PIN_nRESET_FET_DRIVE)
    // open drain logic for reset button
    PIN_nRESET_IOCON = PIN_nRESET_IOCON_INIT;
    LPC_GPIO->CLR[PIN_nRESET_PORT] = PIN_nRESET;
    LPC_GPIO->DIR[PIN_nRESET_PORT] &= ~PIN_nRESET;
#else
    // FET drive logic for reset button
    PIN_nRESET_IOCON = PIN_nRESET_IOCON_INIT;
    LPC_GPIO->CLR[PIN_nRESET_PORT] = PIN_nRESET;
    LPC_GPIO->DIR[PIN_nRESET_PORT] |= PIN_nRESET;
#endif
    /* Enable AHB clock to the FlexInt, GroupedInt domain. */
    LPC_SYSCON->SYSAHBCLKCTRL |= ((1 << 19) | (1 << 23) | (1 << 24));
    // Give the cap on the reset button time to charge
    busy_wait(10000);

    if ((gpio_get_sw_reset() == 0) || config_ram_get_initial_hold_in_bl()) {
        IRQn_Type irq;
        // Disable SYSTICK timer and interrupt before calling into ISP
        SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);

        // Disable all nvic interrupts
        for (irq = (IRQn_Type)0; irq < (IRQn_Type)32; irq++) {
            NVIC_DisableIRQ(irq);
            NVIC_ClearPendingIRQ(irq);
        }

        // If switching to "bootloader" mode then setup the watchdog
        // so it will exit CRP mode after ~30 seconds
        if (config_ram_get_initial_hold_in_bl()) {
            LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 15); // Enable watchdog module
            LPC_SYSCON->PDRUNCFG &= ~(1 << 6);      // Enable watchdog clock (WDOSC)
            LPC_SYSCON->WDTOSCCTRL = (0xF << 5);    // Set max frequency - 2.3MHz
            LPC_WWDT->CLKSEL = (1 << 0);            // Select watchdog clock
            LPC_WWDT->TC = 0x00FFFFFF;              // Set time to reset to ~29s
            LPC_WWDT->MOD = (1 << 0) | (1 << 1);    // Enable watchdog and set reset
            LPC_WWDT->FEED = 0xAA;                  // Enable watchdog
            LPC_WWDT->FEED = 0x55;
        }

        iap_reinvoke();
    }
}

void gpio_set_hid_led(gpio_led_state_t state)
{
    if (state) {
        LPC_GPIO->CLR[PIN_DAP_LED_PORT] = PIN_DAP_LED;
    } else {
        LPC_GPIO->SET[PIN_DAP_LED_PORT] = PIN_DAP_LED;
    }
}

void gpio_set_cdc_led(gpio_led_state_t state)
{
    if (state) {
        LPC_GPIO->CLR[PIN_CDC_LED_PORT] = PIN_CDC_LED;
    } else {
        LPC_GPIO->SET[PIN_CDC_LED_PORT] = PIN_CDC_LED;
    }
}

static uint32_t gpio_get_config0(void)
{
	return ((LPC_GPIO->PIN[PIN_CFG0_PORT] & PIN_CFG0) != 0x00)? PIN_HIGH:PIN_LOW;
}

static uint32_t gpio_get_config1(void)
{
	return ((LPC_GPIO->PIN[PIN_CFG1_PORT] & PIN_CFG1) != 0x00)? PIN_HIGH:PIN_LOW;
}

static uint32_t gpio_get_config2(void)
{
	return ((LPC_GPIO->PIN[PIN_CFG2_PORT] & PIN_CFG2) != 0x00)? PIN_HIGH:PIN_LOW;
}

static uint32_t gpio_get_config3(void)
{
	return ((LPC_GPIO->PIN[PIN_CFG3_PORT] & PIN_CFG3) != 0x00)? PIN_HIGH:PIN_LOW;
}

uint8_t gpio_get_config(uint8_t cfgid)
{
    uint8_t rc = 0;
	switch (cfgid)
	{
        case PIN_CONFIG_DT01:
            rc = gpio_get_config0();
            break;

		case PIN_CONFIG_1:
			rc = gpio_get_config1();
			break;

		case PIN_CONFIG_2:
			rc = gpio_get_config2();
            break;

		case PIN_CONFIG_3:
			rc = gpio_get_config3();
			break;
    }

	return rc;
}

uint8_t gpio_get_sw_reset(void)
{
    static uint8_t last_reset_forward_pressed = 0;
    uint8_t reset_forward_pressed;
    uint8_t reset_pressed;
    reset_forward_pressed = LPC_GPIO->PIN[PIN_RESET_IN_FWRD_PORT] & PIN_RESET_IN_FWRD ? 0 : 1;

    // Forward reset if the state of the button has changed
    //    This must be done on button changes so it does not interfere
    //    with other reset sources such as programming or CDC Break
    if (last_reset_forward_pressed != reset_forward_pressed) {
        if (reset_forward_pressed) {
            target_set_state(RESET_HOLD);
        } else {
            target_set_state(RESET_RUN);
        }

        last_reset_forward_pressed = reset_forward_pressed;
    }

    reset_pressed = reset_forward_pressed ;
    return !reset_pressed;
}

void target_forward_reset(bool assert_reset)
{
    // Do nothing - reset is forwarded in gpio_get_sw_reset
}
