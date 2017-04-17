/**
 * @file    dap_vendor_command.h
 * @brief   CMSIS-DAP vendor specific command implementations
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

#include "stdint.h"
#include "string.h"

#include "RTL.h"
#include "rl_usb.h"
#include "info.h"
#include "DAP_config.h"
#include "uart.h"
#include "DAP.h"
#include "main.h"
#include "target_config.h"
#include "read_uid.h"

void main_identification_led(uint16_t time);

// Process DAP Vendor command and prepare response
// Default function (can be overridden)
//   request:  pointer to request data
//   response: pointer to response data
//   return:   number of bytes in response
// this function is declared as __weak in DAP.c
uint32_t DAP_ProcessVendorCommand(uint8_t *request, uint8_t *response)
{
    // get unique ID command
    if (*request == ID_DAP_Vendor0) {
        const char *id_str = info_get_unique_id();
        uint8_t len = strlen(id_str);
        *response = ID_DAP_Vendor0;
        *(response + 1) = len;
        memcpy(response + 2, id_str, len);
        return (len + 2);
    } else if (*request == ID_DAP_Vendor8) {
        *response = ID_DAP_Vendor8;
        *(response + 1) = 1;
        if (0 == request[1]) {
            main_usb_set_test_mode(false);
        } else if (1 == request[1]) {
            main_usb_set_test_mode(true);
        } else {
            *(response + 1) = 0;
        }
    }
    // get CPU type command
    else if (*request == ID_DAP_Vendor1) {
        uint8_t targetID = swd_init_get_target();

        *response = ID_DAP_Vendor1;
        *(response + 1) = targetID;
        return 2;
    }
    else if (*request == ID_DAP_Vendor2) {
        uint32_t fullUniqueId[4];
        read_full_unique_id(fullUniqueId);

        *response = ID_DAP_Vendor2;
        *(response + 1) = 16;
        memcpy(response + 2, (uint8_t *)fullUniqueId, 16);
        return (16 + 2);
    }    
    else if (*request == ID_DAP_Vendor31) {
        uint16_t time = request[1]  | (request[2] << 8) ;
        main_identification_led(time);
        *response = ID_DAP_Vendor31;        
    }    
    // else return invalid command
    else {
        *response = ID_DAP_Invalid;
    }

    return (1);
}
