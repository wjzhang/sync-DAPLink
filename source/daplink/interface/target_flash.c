/**
 * @file    target_flash.c
 * @brief   Implementation of target_flash.h
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

#include "string.h"

#include "target_config.h"
#include "target_reset.h"
#include "gpio.h"
#include "validation.h"
#include "target_config.h"
#include "intelhex.h"
#include "swd_host.h"
#include "flash_intf.h"
#include "util.h"
#include "settings.h"

#include "target_ids.h"

static error_t target_flash_init(void);
static error_t target_flash_uninit(void);
static error_t target_flash_program_page(uint32_t adr, const uint8_t *buf, uint32_t size);
static error_t target_flash_erase_sector(uint32_t addr);
static error_t target_flash_erase_chip(void);
static uint32_t target_flash_program_page_min_size(uint32_t addr);
static uint32_t target_flash_erase_sector_size(uint32_t addr);

static const flash_intf_t flash_intf = {
    target_flash_init,
    target_flash_uninit,
    target_flash_program_page,
    target_flash_erase_sector,
    target_flash_erase_chip,
    target_flash_program_page_min_size,
    target_flash_erase_sector_size,
};

const flash_intf_t *const flash_intf_target = &flash_intf;

static uint32_t lastEraseSectorNumber = 0xFFFFFFFF;

static error_t target_flash_init()
{
    if (targetID == Target_UNKNOWN)
        return ERROR_TARGET_UNKNOWN;
    
    const program_target_t *const flash = target_device[targetID].flash_algo;

    lastEraseSectorNumber = 0xFFFFFFFF;
    
    if (0 == target_set_state(RESET_PROGRAM)) {
        return ERROR_RESET;
    }

    // Download flash programming algorithm to target and initialise.
    if (0 == swd_write_memory(flash->algo_start, (uint8_t *)flash->algo_blob, flash->algo_size)) {
        return ERROR_ALGO_DL;
    }

    if (0 == swd_flash_syscall_exec(&flash->sys_call_s, flash->init, target_device[targetID].flash_start, 0, 0, 0)) {
        return ERROR_INIT;
    }

    return ERROR_SUCCESS;
}

static error_t target_flash_uninit(void)
{
    // Resume the target if configured to do so
    if (config_get_auto_rst()) {
        target_set_state(RESET_RUN);
    }

    swd_off();
    return ERROR_SUCCESS;
}

static error_t target_flash_program_page(uint32_t addr, const uint8_t *buf, uint32_t size)
{
    if (targetID == Target_UNKNOWN)
        return ERROR_TARGET_UNKNOWN;
    
    const program_target_t *const flash = target_device[targetID].flash_algo;

    // check if security bits were set
    if (1 == security_bits_set(addr, (uint8_t *)buf, size)) {
        return ERROR_SECURITY_BITS;
    }

    while (size > 0) {
        uint32_t write_size = MIN(size, flash->program_buffer_size);
        uint32_t nextSectorAddress = 0;
        uint32_t currentSectorNumber = target_device[targetID].get_sector_number(addr);
        if (currentSectorNumber != lastEraseSectorNumber) {
            if(ERROR_SUCCESS != target_flash_erase_sector(currentSectorNumber)){
                return ERROR_ERASE_SECTOR;
            }						
            lastEraseSectorNumber = currentSectorNumber;
        }
          //check is cross sectors
        nextSectorAddress = target_device[targetID].get_sector_address(currentSectorNumber) + target_device[targetID].get_sector_length(currentSectorNumber);
        if((addr + write_size)  >  nextSectorAddress){
            write_size = nextSectorAddress - addr;
        }
        
        // Write page to buffer
        if (!swd_write_memory(flash->program_buffer, (uint8_t *)buf, write_size)) {
            return ERROR_ALGO_DATA_SEQ;
        }

        // Run flash programming
        if (!swd_flash_syscall_exec(&flash->sys_call_s,
                                    flash->program_page,
                                    addr,
                                    write_size,
                                    flash->program_buffer,
                                    0)) {
            return ERROR_WRITE;
        }

        if (config_get_automation_allowed()) {
            // Verify data flashed if in automation mode
            while (write_size > 0) {
                uint8_t rb_buf[16];
                uint32_t verify_size = MIN(write_size, sizeof(rb_buf));
                if (!swd_read_memory(addr, rb_buf, verify_size)) {
                    return ERROR_ALGO_DATA_SEQ;
                }
                if (memcmp(buf, rb_buf, verify_size) != 0) {
                    return ERROR_WRITE;
                }
                addr += verify_size;
                buf += verify_size;
                size -= verify_size;
                write_size -= verify_size;
            }
        } else {
            addr += write_size;
            buf += write_size;
            size -= write_size;
        }
    }

    return ERROR_SUCCESS;
}

static error_t target_flash_erase_sector(uint32_t sector)
{
    uint32_t address = 0;

    if (targetID == Target_UNKNOWN)
        return ERROR_TARGET_UNKNOWN;
    
    const program_target_t *const flash = target_device[targetID].flash_algo;

    address =	target_device[targetID].get_sector_address(sector);
    if (0 == swd_flash_syscall_exec(&flash->sys_call_s, flash->erase_sector, address, 0, 0, 0)) {
        return ERROR_ERASE_SECTOR;
    }

    return ERROR_SUCCESS;
}

static error_t target_flash_erase_chip(void)
{
    error_t status = ERROR_SUCCESS;
    const program_target_t *const flash = target_device[targetID].flash_algo;

    if (0 == swd_flash_syscall_exec(&flash->sys_call_s, flash->erase_chip, 0, 0, 0, 0)) {
        return ERROR_ERASE_ALL;
    }

    // Reset and re-initialize the target after the erase if required
    if (target_device[targetID].erase_reset) {
        status = target_flash_init();
    }

    return status;
}

static uint32_t target_flash_program_page_min_size(uint32_t addr)
{
    uint32_t size = 256;
    util_assert(target_device[targetID].sector_size >= size);
    return size;
}

static uint32_t target_flash_erase_sector_size(uint32_t addr)
{
    return target_device[targetID].sector_size;
}
