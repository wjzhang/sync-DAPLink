/* CMSIS-DAP Interface Firmware
 * Copyright (c) 2009-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "target_reset.h"
#include "swd_host.h"
#include "target_ids.h"

uint8_t targetID = Target_UNKNOWN;

typedef struct{
    void    (*init)(void);
    uint8_t (*unlock)(void);
    uint8_t (*securitybitset)(uint32_t addr, uint8_t *data, uint32_t size);
    uint8_t (*setstate)(TARGET_RESET_STATE);
}Target_Reset;


void common_target_before_init_debug(void)
{
    return;
}

uint8_t common_target_unlock_sequence(void)
{
    return 1;
}

uint8_t common_security_bits_set(uint32_t addr, uint8_t *data, uint32_t size)
{
    return 0;
}

uint8_t common_target_set_state(TARGET_RESET_STATE state)
{
    return swd_set_target_state_hw(state);
}


static const Target_Reset targets[] = {
    {common_target_before_init_debug    , common_target_unlock_sequence    , common_security_bits_set, common_target_set_state    },
    {common_target_before_init_debug    , common_target_unlock_sequence    , common_security_bits_set, common_target_set_state    },
    {common_target_before_init_debug    , common_target_unlock_sequence    , common_security_bits_set, common_target_set_state    },
    {common_target_before_init_debug    , common_target_unlock_sequence    , common_security_bits_set, common_target_set_state    },
    {common_target_before_init_debug    , common_target_unlock_sequence    , common_security_bits_set, common_target_set_state    },
};


void target_before_init_debug(void) {
    if (targetID != Target_UNKNOWN) {    
        targets[targetID].init();
    } else {
        common_target_before_init_debug();
    }
}

uint8_t target_unlock_sequence(void) {
    if (targetID != Target_UNKNOWN) {    
        return  targets[targetID].unlock();
    } else {
        return common_target_unlock_sequence();
    }
}

uint8_t security_bits_set(uint32_t addr, uint8_t *data, uint32_t size)
{
    if (targetID != Target_UNKNOWN) {    
        return  targets[targetID].securitybitset(addr, data, size);
    } else {
        return common_security_bits_set(addr, data, size);
    }
}

uint8_t target_set_state(TARGET_RESET_STATE state) {
    if (targetID != Target_UNKNOWN) {    
        return targets[targetID].setstate(state);
    } else {
        return common_target_set_state(state);
    }
}
