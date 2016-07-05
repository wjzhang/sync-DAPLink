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
#ifndef TARGET_IDS_H
#define TARGET_IDS_H

enum Target_IDs
{
    Target_NRF51822  = 0,
    Target_STM32F051 = 1,
    Target_STM32F103 = 2,
    Target_STM32F405 = 3,
    Target_STM32F071 = 4,    
    
    Target_UNKNOWN   = 0xFF
};


#endif  //TARGET_IDS_H
