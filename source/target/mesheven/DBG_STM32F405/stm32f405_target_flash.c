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

#include "flash_blob.h"
#include "swd_host.h"
#include "target_reset.h"
#include <stdint.h>
#include <debug_cm.h>
#include <RTL.h>


static const uint32_t STM32F405_FLM[] = {
    0xE00ABE00, 0x062D780D, 0x24084068, 0xD3000040, 0x1E644058, 0x1C49D1FA, 0x2A001E52, 0x4770D1F2,		/*0x0020*/
    
    0xf1a14601, 0xf44f6200, 0xf04f3380, 0x429a30ff, 0x0b90d201, 0x4a584770, 0x429a440a, 0x2004d201, 	/*0x0040*/ 
    0x4a564770, 0xf5b14411, 0xd2f92f60, 0xeb002005, 0x47704051, 0x49524853, 0x49536001, 0x20006001, 	/*0x0060*/ 
    0x49504770, 0x310820f1, 0x20006008, 0x20004770, 0x4a4c4770, 0x320cb500, 0x28006810, 0xf7ffda01, 	/*0x0080*/ 
    0x4948ffe9, 0x68083108, 0xd4fc03c0, 0xf4206810, 0xf0407040, 0x60100004, 0xf4406810, 0x60103080, 	/*0x00A0*/ 
    0x03c06808, 0x6810d4fc, 0x0004f020, 0x20006010, 0xb530bd00, 0x46024d3b, 0x6828350c, 0xda012800, 	/*0x00C0*/ 
    0xffc8f7ff, 0x34084c37, 0x03c86821, 0x4610d4fc, 0xffa6f7ff, 0x22026829, 0x715ef421, 0x00c0eb02,  	/*0x00E0*/ 
    0x0001ea40, 0x7000f440, 0x68286028, 0x3080f440, 0x68206028, 0xd4fc03c0, 0xf0206828, 0x60280002, 	/*0x0100*/ 
    0xbd302000, 0x4e27b5f0, 0x360c2400, 0x68304603, 0x2800460d, 0xf7ffda01, 0x4922ff9d, 0x68083108, 	/*0x0120*/ 
    0xd4fc03c0, 0xf4206830, 0x60307040, 0x3701f240, 0x2c01f240, 0x6830e010, 0x000cea40, 0x68106030, 	/*0x0140*/ 
    0x68086018, 0xd4fc03c0, 0xf8d36810, 0x4570e000, 0x1d1bd117, 0x1c641d12, 0x0f95ebb4, 0x07a8d3eb, 	/*0x0160*/ 
    0x6830d014, 0x1401f240, 0x7040f420, 0x60304320, 0x80188810, 0x03c06808, 0x8810d4fc, 0x42888819, 	/*0x0180*/		 
    0x6830d004, 0x603043b8, 0xbdf02001, 0x43b86830, 0x20006030, 0x0000bdf0, 0xf7ff0000, 0xf7fe0000, 	/*0x01A0*/ 
    0x45670123, 0x40023c04, 0xcdef89ab, 0x00000000,
};

const program_target_t stm32f405_flash = {
    0x20000063, // Init
    0x2000006F, // UnInit
    0x20000073, // EraseChip
    0x200000B3, // EraseSector
    0x20000105, // ProgramPage

    // RSB : base adreess is address of Execution Region PrgData in map file
    //       to access global/static data
    // RSP : Initial stack pointer
    {
      0x20000001,             // breakpoint location
      0x20000020+0x000001B0,  // static_base
      0x20002000              // stack_pointer
    },
    0x20001000,               // program_buffer
    0x20000000,               // algo_start
    0x000001B0,               // algo_size
    STM32F405_FLM,            // image
    512,                     // ram_to_flash_bytes_to_be_written
    16384,                       // flash sector size : 16KB/64KB/128KB
    512,                       // auto increment page size
    0x08000000                 // flash base address
};

uint32_t stm32f405_GetSecNum (uint32_t addr) {
	uint32_t rc = 0xFFFFFFFF;
	if (addr >= 0x08000000 && addr < 0x08010000)
	{
		rc = (addr - 0x08000000)/0x4000;
	}
	else if (addr >= 0x08010000 && addr < 0x08020000)
	{
		rc = 4;
	}
	else if (addr >= 0x08020000 && addr < 0x08100000)
	{
		rc = 5 + (addr - 0x08020000)/0x20000;
	}
	return rc;
}

uint32_t stm32f405_GetSecAddress (uint32_t sector) {
	uint32_t rc  = 0x08000000;
	if (sector < 4)
	{
		rc += 0x4000 * sector;
	}
	else if (sector == 4)
	{
		rc += 0x10000;
	}
	else
	{
		rc += (sector-4)*0x20000;
	}
	return rc;
}

uint32_t stm32f405_GetSecLength (uint32_t sector) {
	uint32_t rc  = 0;
	if(sector < 4)
	{
		rc = 0x4000;  //16KB
	}
	else if(sector == 4)
	{
		rc = 0x10000;  //64KB
	}
	else
	{
		rc = 0x20000;  //128KB
	}
	return rc;
}
