/**
 * @file    serial.c
 * @brief   implementation of serial.h
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

#include "RTL.h"
#include "serial.h"

extern OS_ID serial_mailbox;

UART_Configuration uart_config;

int32_t serial_initialize(void)
{
    os_mbx_send(&serial_mailbox, (void *)SERIAL_INITIALIZE, 0);
    return 1;
}

int32_t serial_uninitialize(void)
{
    os_mbx_send(&serial_mailbox, (void *)SERIAL_UNINITIALIZE, 0);
    return 1;
}

int32_t serial_reset(void)
{
    os_mbx_send(&serial_mailbox, (void *)SERIAL_RESET, 0);
    return 1;
}

int32_t serial_set_configuration(UART_Configuration *config)
{
    int32_t  needupdate = 0;
    //check config
    if(uart_config.Baudrate != config->Baudrate){
        uart_config.Baudrate = config->Baudrate;
        needupdate = 1;
    }
    if(uart_config.DataBits != config->DataBits){
        uart_config.DataBits = config->DataBits;
        needupdate = 1;
    }
    if(uart_config.Parity != config->Parity){
        uart_config.Parity = config->Parity;
        needupdate = 1;
    }
    if(uart_config.StopBits != config->StopBits){
        uart_config.StopBits = config->StopBits;
        needupdate = 1;
    }
    
    if(uart_config.FlowControl != config->FlowControl){
        uart_config.FlowControl = config->FlowControl;
        needupdate = 1;
    }

    if(needupdate == 1){
        os_mbx_send(&serial_mailbox, (void*)SERIAL_SET_CONFIGURATION, 0);
    }
    return 1;    
}

int32_t serial_get_configuration(UART_Configuration *config)
{
    *config = uart_config;
    return 1;
}

