/*
 *   This file is part of fastboot 3DS
 *   Copyright (C) 2019 derrek, profi200
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "types.h"
#include "mem_map.h"
#include "arm9/hardware/spicard.h"
#include "arm9/hardware/cfg9.h"
#include "arm9/hardware/interrupt.h"
#include "arm9/hardware/timer.h"
#include "arm9/hardware/ndma.h"


#define SPICARD_REGS_BASE   (IO_MEM_ARM9_ONLY + 0xD800)
#define REG_NSPI_CNT        *((vu32*)(SPICARD_REGS_BASE + 0x00))
#define REG_NSPI_CS         *((vu8* )(SPICARD_REGS_BASE + 0x04)) // 32 bit but can be accessed as u8
#define REG_NSPI_BLKLEN     *((vu32*)(SPICARD_REGS_BASE + 0x08))
#define REG_NSPI_FIFO       *((vu32*)(SPICARD_REGS_BASE + 0x0C))
#define REG_NSPI_FIFO_STAT  *((vu8* )(SPICARD_REGS_BASE + 0x10)) // 32 bit but can be accessed as u8
#define REG_NSPI_AUTOPOLL   *((vu32*)(SPICARD_REGS_BASE + 0x14))
#define REG_NSPI_INT_MASK   *((vu32*)(SPICARD_REGS_BASE + 0x18))
#define REG_NSPI_INT_STAT   *((vu32*)(SPICARD_REGS_BASE + 0x1C))



static inline void nspiWaitBusy(void)
{
	while(REG_NSPI_CNT & NSPI_ENABLE);
}

static inline void nspiWaitFifoBusy(void)
{
	while(REG_NSPI_FIFO_STAT & NSPI_FIFO_BUSY);
}

void SPICARD_init(void)
{
	static bool inited = false;
	if(inited) return;
	inited = true;

	// TODO
#define REG_CFG9_CARDCTL      *((vu16*)0x1000000C)
#define REG_CFG9_CARDSTATUS   *((vu8* )0x10000010)
#define REG_CFG9_CARDCYCLES0  *((vu16*)0x10000012)
#define REG_CFG9_CARDCYCLES1  *((vu16*)0x10000014)

#define REG_NTRCARDMCNT       *((vu16*)0x10164000)
#define REG_NTRCARDROMCNT     *((vu32*)0x10164004)

	REG_CFG9_CARDCYCLES0 = 0x1988;
	REG_CFG9_CARDCYCLES1 = 0x264C;
	// boot9 waits here. Unnecessary?

	REG_CFG9_CARDSTATUS = 3u<<2;     // Request power off
	while(REG_CFG9_CARDSTATUS != 0); // Aotomatically changes to 0 (off)
	TIMER_sleep(1);

	REG_CFG9_CARDSTATUS = 1u<<2;     // Prepare power on
	TIMER_sleep(10);

	REG_CFG9_CARDSTATUS = 2u<<2;     // Power on
	TIMER_sleep(27);

	// Switch to NTRCARD controller.
	REG_CFG9_CARDCTL = 0;
	REG_NTRCARDMCNT = 0xC000u;
	REG_NTRCARDROMCNT = 0x20000000;
	TIMER_sleep(120);

	REG_CFG9_CARDCTL |= 1u<<8;

	IRQ_registerHandler(IRQ_CTR_CARD_1, NULL);
	REG_NSPI_INT_MASK = NSPI_INT_TRANSF_END; // Disable interrupt 1
	REG_NSPI_INT_STAT = NSPI_INT_AP_TIMEOUT | NSPI_INT_AP_SUCCESS | NSPI_INT_TRANSF_END; // Aknowledge
}

bool _SPICARD_autoPollBit(u32 params)
{
	REG_NSPI_AUTOPOLL = NSPI_AUTOPOLL_START | params;

	u32 res;
	do
	{
		__wfi();
		res = REG_NSPI_INT_STAT;
	} while(!(res & (NSPI_INT_AP_TIMEOUT | NSPI_INT_AP_SUCCESS)));
	REG_NSPI_INT_STAT = res; // Aknowledge

	return (res & NSPI_INT_AP_TIMEOUT) == 0; // Timeout error
}

void SPICARD_writeRead(NspiClk clk, const u32 *in, u32 *out, u32 inSize, u32 outSize, bool done)
{
	const u32 cntParams = NSPI_ENABLE | NSPI_BUS_1BIT | clk;

	if(in)
	{
		REG_NSPI_BLKLEN = inSize;
		REG_NSPI_CNT = cntParams | NSPI_DIR_WRITE;

		u32 counter = 0;
		do
		{
			if((counter & 31) == 0) nspiWaitFifoBusy();
			REG_NSPI_FIFO = *in++;
			counter += 4;
		} while(counter < inSize);

		nspiWaitBusy();
	}
	if(out)
	{
		REG_NSPI_BLKLEN = outSize;
		REG_NSPI_CNT = cntParams | NSPI_DIR_READ;

		u32 counter = 0;
		do
		{
			if((counter & 31) == 0) nspiWaitFifoBusy();
			*out++ = REG_NSPI_FIFO;
			counter += 4;
		} while(counter < outSize);

		nspiWaitBusy();
	}

	if(done) REG_NSPI_CS = NSPI_DESELECT;
}
