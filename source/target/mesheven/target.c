/**
 * @file    target.c
 * @brief   Target information for the multi-targets
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

#include "target_config.h"
#include "target_ids.h"

// The file flash_blob.c must only be included in target.c
//#include "flash_blob.c"
extern const program_target_t NRF51_flash;
extern const program_target_t stm32f051_flash;
extern const program_target_t stm32f071_flash;
extern const program_target_t stm32f103_flash;
extern const program_target_t stm32f405_flash;
extern const program_target_t stm32f031_flash;
    
uint32_t nrf51_GetSecNum (uint32_t addr);
uint32_t nrf51_GetSecAddress(uint32_t sector);
uint32_t nrf51_GetSecLength(uint32_t sector);

uint32_t stm32f051_GetSecNum (uint32_t addr);
uint32_t stm32f051_GetSecAddress(uint32_t sector);
uint32_t stm32f051_GetSecLength(uint32_t sector);

uint32_t stm32f071_GetSecNum (uint32_t addr);
uint32_t stm32f071_GetSecAddress(uint32_t sector);
uint32_t stm32f071_GetSecLength(uint32_t sector);

uint32_t stm32f103_GetSecNum (uint32_t addr);
uint32_t stm32f103_GetSecAddress(uint32_t sector);
uint32_t stm32f103_GetSecLength(uint32_t sector);

uint32_t stm32f405_GetSecNum (uint32_t addr);
uint32_t stm32f405_GetSecAddress (uint32_t sector);
uint32_t stm32f405_GetSecLength (uint32_t sector);

uint32_t stm32f031_GetSecNum (uint32_t addr);
uint32_t stm32f031_GetSecAddress(uint32_t sector);
uint32_t stm32f031_GetSecLength(uint32_t sector);

// target information
const target_cfg_t target_device[] = 
{
    //nrf51822
    {
        .sector_size    = 1024,
        .sector_cnt     = 256,
        .flash_start    = 0x00000000,
        .flash_end      = 0x10001100,
        .ram_start      = 0x20000000,
        .ram_end        = 0x20008000,
        .flash_algo     = (program_target_t *) &NRF51_flash,
        .get_sector_number = nrf51_GetSecNum,
        .get_sector_address = nrf51_GetSecAddress,
        .get_sector_length = nrf51_GetSecLength,
    },
    //stm32f051kX
    {
        .sector_size    = 1024,
        .sector_cnt     = 64,
        .flash_start    = 0x08000000,
        .flash_end      = 0x08010000,
        .ram_start      = 0x20000000,
        .ram_end        = 0x20002000,
        .flash_algo     = (program_target_t *) &stm32f051_flash,
        .get_sector_number = stm32f051_GetSecNum,
        .get_sector_address = stm32f051_GetSecAddress,
        .get_sector_length = stm32f051_GetSecLength,        
    },
    //stm32f103rc
    {
        .sector_size    = 2048,
        .sector_cnt     = 128,
        .flash_start    = 0x08000000,
        .flash_end      = 0x08040000,
        .ram_start      = 0x20000000,
        .ram_end        = 0x2000C000,
        .flash_algo     = (program_target_t *) &stm32f103_flash,
        .get_sector_number = stm32f103_GetSecNum,
        .get_sector_address = stm32f103_GetSecAddress,
        .get_sector_length = stm32f103_GetSecLength,        
    },
    //stm32f405
    {
        .sector_size    = 16384,
        .sector_cnt     = 64,
        .flash_start    = 0x08000000,
        .flash_end      = 0x08100000,
        .ram_start      = 0x20000000,
        .ram_end        = 0x20020000,
        .flash_algo     = (program_target_t *) &stm32f405_flash,
        .get_sector_number = stm32f405_GetSecNum,
        .get_sector_address = stm32f405_GetSecAddress,
        .get_sector_length = stm32f405_GetSecLength,        
    },
    //stm32f071
    {
        .sector_size    = 2048,
        .sector_cnt     = 64,
        .flash_start    = 0x08000000,
        .flash_end      = 0x08020000,
        .ram_start      = 0x20000000,
        .ram_end        = 0x20004000,
        .flash_algo     = (program_target_t *) &stm32f071_flash,
        .get_sector_number = stm32f071_GetSecNum,
        .get_sector_address = stm32f071_GetSecAddress,
        .get_sector_length = stm32f071_GetSecLength,        
    },
    //stm32f031
    {
        .sector_size    = 1024,
        .sector_cnt     = 32,
        .flash_start    = 0x08000000,
        .flash_end      = 0x08008000,
        .ram_start      = 0x20000000,
        .ram_end        = 0x20001000,
        .flash_algo     = (program_target_t *) &stm32f031_flash,
        .get_sector_number = stm32f031_GetSecNum,
        .get_sector_address = stm32f031_GetSecAddress,
        .get_sector_length = stm32f031_GetSecLength,        
    }    
    
};
