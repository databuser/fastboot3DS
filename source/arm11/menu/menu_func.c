/*
 *   This file is part of fastboot 3DS
 *   Copyright (C) 2017 derrek, profi200, d0k3
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

 
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "fs.h"
#include "fsutils.h"
#include "arm11/menu/menu_color.h"
#include "arm11/menu/menu_fsel.h"
#include "arm11/menu/menu_func.h"
#include "arm11/menu/menu_util.h"
#include "arm11/hardware/hid.h"
#include "arm11/hardware/i2c.h"
#include "arm11/console.h"
#include "arm11/config.h"
#include "arm11/debug.h"
#include "arm11/fmt.h"
#include "arm11/firm.h"
#include "arm11/main.h"



#define PRESET_SLOT_CONFIG_FUNC(x) \
u32 menuPresetSlotConfig##x(void) \
{ \
	return menuPresetSlotConfig((x-1)); \
}

u32 menuPresetNandTools(void)
{
	u32 res = 0xFF;
	
	if (!configDataExist(KDevMode) || !(*(bool*) configGetData(KDevMode)))
		res &= ~((1 << 2) | (1 << 3));
	
	return res;
}

u32 menuPresetBootMenu(void)
{
	u32 res = 0xFF;
	
	for (u32 i = 0; i < 3; i++)
	{
		if (!configDataExist(KBootOption1 + i))
		{
			res &= ~(1 << i);
		}
	}
	
	if (!configDataExist(KDevMode) || !(*(bool*) configGetData(KDevMode)))
		res &= ~(1 << 3);
	
	return res;
}

u32 menuPresetBootConfig(void)
{
	u32 res = 0;
	
	for (u32 i = 0; i < 3; i++)
	{
		if (configDataExist(KBootOption1 + i))
		{
			res |= 1 << i;
		}
	}
	
	if (configDataExist(KBootMode))
		res |= 1 << 3;
	
	return res;
}

u32 menuPresetSlotConfig(u32 slot)
{
	u32 res = 0;
	
	if (configDataExist(KBootOption1 + slot))
	{
		res |= (1 << 0);
		res |= (configDataExist(KBootOption1Buttons + slot)) ? (1 << 1) : (1 << 2);
	}
	else
	{
		res |= (1 << 3);
	}
	
	return res;
}
PRESET_SLOT_CONFIG_FUNC(1)
PRESET_SLOT_CONFIG_FUNC(2)
PRESET_SLOT_CONFIG_FUNC(3)

u32 menuPresetBootMode(void)
{
	if (configDataExist(KBootMode))
	{
		return (1 << (*(u32*) configGetData(KBootMode)));
	}
		
	return 0;
}


u32 menuSetBootMode(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) term_con;
	(void) menu_con;
	u32 res = (configSetKeyData(KBootMode, &param)) ? 0 : 1;
	
	return res;
}

u32 menuSetupBootSlot(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) term_con;
	
	const u32 slot = param & 0xF;
	char* res_path = NULL;
	char* start = NULL;
	
	// if bit4 of param is set, reset slot and return
	if (param & 0x10)
	{
		configDeleteKey(KBootOption1Buttons + slot);
		configDeleteKey(KBootOption1 + slot);
		return 0;
	}
	
	if (configDataExist(KBootOption1 + slot))
		start = (char*) configGetData(KBootOption1 + slot);
	
	res_path = (char*) malloc(256);
	if (!res_path) panicMsg("Out of memory");
	
	ee_printf_screen_center("Select a firmware file for slot #%lu.\nPress [HOME] to cancel.", slot + 1);
	updateScreens();
	
	u32 res = 0;
	if (menuFileSelector(res_path, menu_con, start, "*.firm", true))
		res = (configSetKeyData(KBootOption1 + slot, res_path)) ? 0 : 1;
	
	free(res_path);
	return res;
}

u32 menuSetupBootKeys(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) menu_con;
	
	const u32 y_center = 7;
	const u32 y_instr = 21;
	const u32 slot = param & 0xF;
	
	// don't allow setting this up if firm is not set
	if (!configDataExist(KBootOption1 + slot))
		return 0;
	
	// if bit4 of param is set, delete boot keys and return
	if (param & 0x10)
	{
		configDeleteKey(KBootOption1Buttons + slot);
		return 0;
	}
	
	hidScanInput();
	u32 kHeld = hidKeysHeld();
	
	while (true)
	{
		// build button string
		char button_str[80];
		keysToString(kHeld, button_str);
		
		// clear console
		consoleSelect(term_con);
		consoleClear();
		
		// draw input block
		term_con->cursorY = y_center;
		ee_printf(ESC_SCHEME_WEAK);
		ee_printf_line_center("Hold button(s) to setup.");
		ee_printf_line_center("Currently held buttons:");
		ee_printf(ESC_SCHEME_STD);
		ee_printf_line_center(button_str);
		ee_printf(ESC_RESET);
		
		// draw instructions
		term_con->cursorY = y_instr;
		ee_printf(ESC_SCHEME_WEAK);
		if (configDataExist(KBootOption1Buttons + slot))
		{
			char* currentSetting =
				(char*) configCopyText(KBootOption1Buttons + slot);
			if (!currentSetting) panicMsg("Config error");
			ee_printf_line_center("Current: %s", currentSetting);
			free(currentSetting);
		}
		ee_printf_line_center("[HOME] to cancel");
		ee_printf(ESC_RESET);
		
		// update screens
		updateScreens();
		
		// check for buttons until held for ~1.5sec
		u32 kHeldNew = 0;
		do
		{
			// check hold duration
			u32 vBlanks = 0;
			do
			{
				GFX_waitForEvent(GFX_EVENT_PDC0, true);
				if(hidGetPowerButton(false)) return 1;
				
				hidScanInput();
				kHeldNew = hidKeysHeld();
				if(hidKeysDown() & KEY_SHELL) sleepmode();
			}
			while ((kHeld == kHeldNew) && (++vBlanks < 100));
			
			// check HOME key
			if (kHeldNew & KEY_HOME) return 1;
		}
		while (!((kHeld|kHeldNew) & 0xfff));
		// repeat checks until actual buttons are held
		
		if (kHeld == kHeldNew) break;
		kHeld = kHeldNew;
	}
	
	// if we arrive here, we have a button combo
	u32 res = (configSetKeyData(KBootOption1Buttons + slot, &kHeld)) ? 0 : 1;
	
	return res;
}

u32 menuLaunchFirm(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	char path_store[256];
	char* path;
	
	// select & clear console
	consoleSelect(term_con);
	consoleClear();
		
	if (param < 3) // loading from bootslot
	{
		// check if bootslot exists
		ee_printf("Checking bootslot...\n");
		if (!configDataExist(KBootOption1 + param))
		{
			ee_printf("Bootslot does not exist!\n");
			goto fail;
		}
		path = (char*) configGetData(KBootOption1 + param);
	}
	else if (param == 0xFE) // loading from FIRM1
	{
		path = "firm1:";
	}
	else if (param == 0xFF) // user decision
	{
		ee_printf_screen_center("Select a firmware file to boot.\nPress [HOME] to cancel.");
		updateScreens();
		
		path = path_store;
		if (!menuFileSelector(path, menu_con, NULL, "*.firm", true))
			return 1;
		
		// back to terminal console
		consoleSelect(term_con);
		consoleClear();
	}
	
	// try load and verify
	ee_printf("\nLoading %s...\n", path);
	s32 res = loadVerifyFirm(path, false);
	if (res != 0)
	{
		ee_printf("Firm %s error code %li!\n", (res > -8) ? "load" : "verify", res);
		goto fail;
	}
	
	ee_printf("\nFirm load success, launching firm..."); // <-- you will never see this
	g_startFirmLaunch = true;
	
	return 0;
	
	fail:
	
	ee_printf("\nFirm launcher failed.\n\nPress B or HOME to return.");
	updateScreens();
	outputEndWait();
	
	return 1;
}

u32 menuContinueBoot(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	// all the relevant stuff handled outside
	(void) term_con;
	(void) menu_con;
	(void) param;
	g_continueBootloader = true;
	return 0;
}

u32 menuBackupNand(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) menu_con;
	(void) param;
	u32 result = 1;
	
	// select & clear console
	consoleSelect(term_con);
	consoleClear();
	
	
	// ensure SD mounted
	if (!fsEnsureMounted("sdmc:"))
	{
		ee_printf("SD not inserted or corrupt!\n");
		goto fail;
	}
	
	
	// console serial number
	char serial[0x10] = { 0 }; // serial from SecureInfo_?
	if (!fsQuickRead("nand:/rw/sys/SecureInfo_A", serial, 0xF, 0x102) && 
		!fsQuickRead("nand:/rw/sys/SecureInfo_B", serial, 0xF, 0x102))
		ee_snprintf(serial, 0x10, "UNKNOWN");
	
	// current state of the RTC
	u8 rtc[8] = { 0 };
	I2C_readRegBuf(I2C_DEV_MCU, 0x30, rtc, 8); // ignore the return value
	
	// create NAND backup filename
	char fpath[64];
	ee_snprintf(fpath, 64, NAND_BACKUP_PATH "/%02X%02X%02X%02X%02X%02X_%s_nand.bin",
		rtc[6], rtc[5], rtc[4], rtc[2], rtc[1], rtc[0], serial);
	
	ee_printf("Creating NAND backup:\n%s\n\nPreparing NAND backup...\n", fpath);
	updateScreens();
	
	
	// open file handle
	s32 fHandle;
	if (!fsCreateFileWithPath(fpath) ||
		((fHandle = fOpen(fpath, FS_OPEN_EXISTING | FS_OPEN_WRITE)) < 0))
	{
		ee_printf("Cannot create file!\n");
		goto fail;
	}
	
	// reserve space for NAND backup / needs work (!!!)
    // !MISSING! Actually use the actual size of the NAND
	ee_printf("Reserving space...\n");
	updateScreens();
	if ((fLseek(fHandle, NAND_BACKUP_SIZE) != 0) || (fTell(fHandle) != NAND_BACKUP_SIZE))
	{
		fClose(fHandle);
		fUnlink(fpath);
		ee_printf("Not enough space!\n");
		goto fail;
	}
	
	
	// setup device read
	s32 devHandle = fPrepareRawAccess(FS_DEVICE_NAND);
	if (devHandle < 0)
	{
		fClose(fHandle);
		fUnlink(fpath);
		ee_printf("Cannot open NAND device!\n");
		goto fail;
	}
	
	// setup device buffer
	s32 dbufHandle = fCreateDeviceBuffer(DEVICE_BUFSIZE);
	if (dbufHandle < 0)
		panicMsg("Out of memory");
	
	
	// all done, ready to do the NAND backup
	ee_printf("\n");
	for (s64 p = 0; p < NAND_BACKUP_SIZE; p += DEVICE_BUFSIZE)
	{
		s64 readBytes = (NAND_BACKUP_SIZE - p > DEVICE_BUFSIZE) ? DEVICE_BUFSIZE : NAND_BACKUP_SIZE - p;
		s32 errcode = 0;
		ee_printf_progress("NAND backup", PROGRESS_WIDTH, p, NAND_BACKUP_SIZE);
		updateScreens();
		
		if ((errcode = fReadToDeviceBuffer(devHandle, p, readBytes, dbufHandle)) != 0)
		{
			ee_printf("\nError: Cannot read from NAND (%li)!\n", errcode);
			goto fail_close_handles;
		}
		
		if ((errcode = fsWriteFromDeviceBuffer(fHandle, p, readBytes, dbufHandle)) != 0)
		{
			ee_printf("\nError: Cannot write to file (%li)!\n", errcode);
			goto fail_close_handles;
		}
		
		// check for user cancel request
		if (userCancelHandler(true))
		{
			fClose(fHandle);
			fFinalizeRawAccess(devHandle);
			fFreeDeviceBuffer(dbufHandle);
			fUnlink(fpath);
			return 1;
		}
	}
	
	// NAND access finalized
	ee_printf_progress("NAND backup", PROGRESS_WIDTH, NAND_BACKUP_SIZE, NAND_BACKUP_SIZE);
	ee_printf("\n\nNAND backup finished.\n");
	result = 0;
	
	
	fail_close_handles:
	
	fClose(fHandle);
	fFinalizeRawAccess(devHandle);
	fFreeDeviceBuffer(dbufHandle);
	
	
	fail:
	
	ee_printf("\nPress B or HOME to return.");
	updateScreens();
	outputEndWait();

	
	if (result != 0) fUnlink(fpath);
	return result;
}

u32 menuRestoreNand(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	u32 result = 1;
    
	
	// select & clear console
	consoleSelect(term_con);
	consoleClear();
    
	// ensure SD mounted
	if (!fsEnsureMounted("sdmc:"))
	{
		ee_printf("SD not inserted or corrupt!\n");
		goto fail;
	}
    
    
	ee_printf_screen_center("Select a NAND backup for restore.\nPress [HOME] to cancel.");
	updateScreens();
	
	char fpath[256];
	if (!menuFileSelector(fpath, menu_con, NAND_BACKUP_PATH, "*nand*.bin", false))
		return 1; // canceled by user
    
    // may be only of interest when not doing forced restore
    // *MISSING* check NAND backup validity (header, file size, crypto)
    // *MISSING* ensure the NAND backup does not change the partitioning
    // *MISSING* protect sector 0x00, sector 0x96 and the FIRM partitions for safe restore
    
    
    // select & clear console
	consoleSelect(term_con);
	consoleClear();
    
    ee_printf("Restoring NAND backup:\n%s\n\nPreparing NAND restore...\n", fpath);
	updateScreens();
    
    
    // open file handle
	s32 fHandle;
	if ((fHandle = fOpen(fpath, FS_OPEN_EXISTING | FS_OPEN_READ)) < 0)
	{
		ee_printf("Cannot open file!\n");
		goto fail;
	}
    
    // setup device read
	s32 devHandle = fPrepareRawAccess(FS_DEVICE_NAND);
	if (devHandle < 0)
	{
		fClose(fHandle);
		fUnlink(fpath);
		ee_printf("Cannot open NAND device!\n");
		goto fail;
	}
	
	// setup device buffer
	s32 dbufHandle = fCreateDeviceBuffer(DEVICE_BUFSIZE);
	if (dbufHandle < 0)
		panicMsg("Out of memory");
    
    
    // check file size
    ee_printf("File size: %lu MiB\n", fSize(fHandle) / 0x100000);
    if (fSize(fHandle) > NAND_BACKUP_SIZE)
    {
        ee_printf("Size exceeds available space!\n");
        goto fail_close_handles;
    }
    
    
    // all done, ready to do the NAND backup
	ee_printf("\n");
	for (s64 p = 0; p < NAND_BACKUP_SIZE; p += DEVICE_BUFSIZE)
	{
		s64 readBytes = (NAND_BACKUP_SIZE - p > DEVICE_BUFSIZE) ? DEVICE_BUFSIZE : NAND_BACKUP_SIZE - p;
		s32 errcode = 0;
		ee_printf_progress("NAND restore", PROGRESS_WIDTH, p, NAND_BACKUP_SIZE);
		updateScreens();
		
		if ((errcode = fReadToDeviceBuffer(fHandle, p, readBytes, dbufHandle)) != 0)
		{
			ee_printf("\nError: Cannot read from file (%li)!\n", errcode);
			goto fail_close_handles;
		}
		
		if ((errcode = fsWriteFromDeviceBuffer(devHandle, p, readBytes, dbufHandle)) != 0)
		{
			ee_printf("\nError: Cannot write to NAND (%li)!\n", errcode);
			goto fail_close_handles;
		}
		
		// check for user cancel request
		if (userCancelHandler(true))
		{
			fClose(fHandle);
			fFinalizeRawAccess(devHandle);
			fFreeDeviceBuffer(dbufHandle);
			fUnlink(fpath);
			return 1;
		}
	}
	
	// NAND access finalized
	ee_printf_progress("NAND restore", PROGRESS_WIDTH, NAND_BACKUP_SIZE, NAND_BACKUP_SIZE);
	ee_printf("\n\nNAND restore finished.\n");
	result = 0;
	
	
	fail_close_handles:
	
	fClose(fHandle);
	fFinalizeRawAccess(devHandle);
	fFreeDeviceBuffer(dbufHandle);
	
	
	fail:
	
	ee_printf("\nPress B or HOME to return.");
	updateScreens();
	outputEndWait();

	
	return result;
}

u32 menuShowCredits(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) menu_con;
	(void) param;
	
	// clear console
	consoleSelect(term_con);
	consoleClear();
	
	// credits
	term_con->cursorY = 4;
	ee_printf(ESC_SCHEME_ACCENT0);
	ee_printf_line_center("Fastboot3DS Credits");
	ee_printf_line_center("===================");
	ee_printf_line_center("");
	ee_printf(ESC_SCHEME_STD);
	ee_printf_line_center("Main developers:");
	ee_printf(ESC_SCHEME_WEAK);
	ee_printf_line_center("derrek");
	ee_printf_line_center("profi200");
	ee_printf_line_center("");
	ee_printf(ESC_SCHEME_STD);
	ee_printf_line_center("Thanks to:");
	ee_printf(ESC_SCHEME_WEAK);
	ee_printf_line_center("yellows8");
	ee_printf_line_center("plutoo");
	ee_printf_line_center("smea");
	ee_printf_line_center("Normmatt (for sdmmc code)");
	ee_printf_line_center("WinterMute (for console code)");
	ee_printf_line_center("d0k3 (for menu code)");
	ee_printf_line_center("");
	ee_printf_line_center("... everyone who contributed to 3dbrew.org");
	updateScreens();

	
	// Konami code
	const u32 konami_code[] = {
		KEY_DUP, KEY_DUP, KEY_DDOWN, KEY_DDOWN, KEY_DLEFT, KEY_DRIGHT, KEY_DLEFT, KEY_DRIGHT, KEY_B, KEY_A };
	const u32 konami = sizeof(konami_code) / sizeof(u32);
	u32 k = 0;
	
	// handle user input
	u32 kDown = 0;
	do
	{
		GFX_waitForEvent(GFX_EVENT_PDC0, true);
		
		if(hidGetPowerButton(false)) // handle power button
			break;
		
		hidScanInput();
		kDown = hidKeysDown();
		
		if (kDown) k = (kDown & konami_code[k]) ? k + 1 : 0;
		if (!k && (kDown & KEY_B)) break;
		if (kDown & KEY_SHELL) sleepmode();
	}
	while (!(kDown & KEY_HOME) && (k < konami));
	
	
	// Konami code entered?
	if (k == konami)
	{
		const bool enabled = true;
		configSetKeyData(KDevMode, &enabled);
		
		consoleClear();
		term_con->cursorY = 10;
		ee_printf(ESC_SCHEME_ACCENT1);
		ee_printf_line_center("You are now a developer!");
		ee_printf(ESC_RESET);
		ee_printf_line_center("");
		ee_printf_line_center("Access to developer-only features granted.");
		updateScreens();
		
		outputEndWait();
	}
	
	
	return 0;
}

u32 menuDummyFunc(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) menu_con;
	
	// clear console
	consoleSelect(term_con);
	consoleClear();
	
	// print something
	ee_printf("This is not implemented yet.\nMy parameter was %lu.\nGo look elsewhere, nothing to see here.\n\nPress B or HOME to return.", param);
	updateScreens();
	outputEndWait();

	return 0;
}

u32 debugSettingsView(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) menu_con;
	(void) param;
	
	// clear console
	consoleSelect(term_con);
	consoleClear();
	
	ee_printf("Write config: %s\n", writeConfigFile() ? "success" : "failed");
	ee_printf("Load config: %s\n", loadConfigFile() ? "success" : "failed");
	
	// show settings
	for (int key = 0; key < KLast; key++)
	{
		const char* kText = configGetKeyText(key);
		const bool kExist = configDataExist(key);
		ee_printf("%02i %s: %s\n", key, kText, kExist ? "exists" : "not found");
		if (configDataExist(key))
		{
			char* text = (char*) configCopyText(key);
			ee_printf("text: %s / u32: %lu\n", text, *(u32*) configGetData(key));
			free(text);
		}
	}
	updateScreens();
	
	// wait for B / HOME button
	do
	{
		GFX_waitForEvent(GFX_EVENT_PDC0, true);
		if(hidGetPowerButton(false)) // handle power button
			return 0;
		
		hidScanInput();
	}
	while (!(hidKeysDown() & (KEY_B|KEY_HOME)));
	
	return 0;
}

u32 debugEscapeTest(PrintConsole* term_con, PrintConsole* menu_con, u32 param)
{
	(void) menu_con;
	(void) param;
	
	// clear console
	consoleSelect(term_con);
	consoleClear();
	
	ee_printf("\x1b[1mbold\n\x1b[0m");
	ee_printf("\x1b[2mfaint\n\x1b[0m");
	ee_printf("\x1b[3mitalic\n\x1b[0m");
	ee_printf("\x1b[4munderline\n\x1b[0m");
	ee_printf("\x1b[5mblink slow\n\x1b[0m");
	ee_printf("\x1b[6mblink fast\n\x1b[0m");
	ee_printf("\x1b[7mreverse\n\x1b[0m");
	ee_printf("\x1b[8mconceal\n\x1b[0m");
	ee_printf("\x1b[9mcrossed-out\n\x1b[0m");
	ee_printf("\n");
	
	for (u32 i = 0; i < 8; i++)
	{
		char c[8];
		ee_snprintf(c, 8, "\x1b[%lu", 30 + i);
		ee_printf("color #%lu:  %smnormal\x1b[0m %s;2mfaint\x1b[0m %s;4munderline\x1b[0m %s;7mreverse\x1b[0m %s;9mcrossed-out\x1b[0m\n", i, c, c, c, c, c);
	}
	
	// wait for B / HOME button
	do
	{
		updateScreens();
		if(hidGetPowerButton(false)) // handle power button
			return 0;
		
		hidScanInput();
	}
	while (!(hidKeysDown() & (KEY_B|KEY_HOME)));
	
	return 0;
}