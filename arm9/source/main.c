#include <nds.h>
#include <nds/fifomessages.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>

u32 getCP15Ctrl();
void disableMPU();
void enableMPU();

PrintConsole* console;
u8* workBuffer;
u8 biosKeys[0x4000];


u32 NAND_Init()
{
	fifoSendValue32(FIFO_SDMMC, SDMMC_NAND_START);
	fifoWaitValue32(FIFO_SDMMC);
	u32 ret = fifoGetValue32(FIFO_SDMMC);
	return ret;
}

u32 NAND_GetSize()
{
	fifoSendValue32(FIFO_SDMMC, SDMMC_NAND_SIZE);
	fifoWaitValue32(FIFO_SDMMC);
	u32 ret = fifoGetValue32(FIFO_SDMMC);
	return ret;
}

u32 NAND_ReadSectors(u32 start, u32 num, void* buf)
{
	FifoMessage msg;

	msg.type = SDMMC_NAND_READ_SECTORS;
	msg.sdParams.startsector = start;
	msg.sdParams.numsectors = num;
	msg.sdParams.buffer = buf;

	fifoSendDatamsg(FIFO_SDMMC, sizeof(msg), (u8*)&msg);
	fifoWaitValue32(FIFO_SDMMC);
	
	DC_InvalidateRange(buf, num*0x200);

	u32 ret = fifoGetValue32(FIFO_SDMMC);
	return ret;
}

u32 NAND_GetNocashFooter(void* buf)
{
	fifoSendAddress(FIFO_USER_02, buf);
	
	fifoSendValue32(FIFO_USER_01, 5);
	fifoWaitValue32(FIFO_USER_01);
	
	DC_InvalidateRange(buf, 0x40);
	
	u32 ret = fifoGetValue32(FIFO_USER_01);
	return ret;
}


u32 SDMMC_EnableDMA()
{
	fifoSendValue32(FIFO_USER_01, 4);
	fifoWaitValue32(FIFO_USER_01);
	u32 ret = fifoGetValue32(FIFO_USER_01);
	return ret;
}


u32 FW_Read(void* buf)
{
	fifoSendAddress(FIFO_USER_02, buf);
	fifoSendValue32(FIFO_USER_01, 6);
	fifoWaitValue32(FIFO_USER_01);
	
	DC_InvalidateRange(buf, 128*1024);
	
	u32 ret = fifoGetValue32(FIFO_USER_01);
	return ret;
}


void DrawMenu()
{
	consoleClear();
	
	printf("DSi dumper - by Arisotura\n");
	printf("--------------------------------");
	printf("A: dump all\n");
	printf("B: dump NAND\n");
	printf("X: dump DS-mode firmware\n");
	printf("Y: dump BIOS\n");
	printf("SELECT: quit\n");
	printf("--------------------------------");
}


int DumpNAND()
{
	mkdir("dsidump", 0);
	
	FILE* f = fopen("dsidump/nand.bin", "wb");
	if (!f)
	{
		printf("Error: failed to create NAND file\n");
		return 0;
	}
	
	printf("Dumping NAND...\n");
	
	u32 nandsize = NAND_GetSize();
	const u32 chunksize = (8*1024*1024) / 512;
	
	for (u32 pos = 0; pos < nandsize; pos += chunksize)
	{
		u32 len = chunksize;
		if ((pos + len) > nandsize)
			len = nandsize - pos;
			
		console->cursorX = 0;
		u32 percent = (pos * 100) / nandsize;
		printf("%d / %d (%d%%)       ", pos, nandsize, percent);
			
		if (NAND_ReadSectors(pos, len, workBuffer) != 0)
		{
			console->cursorX = 0;
			printf("Error: NAND read failed @ %08X\n", pos);
			fclose(f);
			return 0;
		}
		
		if (fwrite(workBuffer, len*512, 1, f) != 1)
		{
			console->cursorX = 0;
			printf("Error: NAND file write failed @ %08X\n", pos);
			fclose(f);
			return 0;
		}
	}
	
	memset(workBuffer, 0, 0x40);
	NAND_GetNocashFooter(workBuffer);
	fseek(f, 0xFF800, SEEK_SET);
	fwrite(workBuffer, 0x40, 1, f);
	
	console->cursorX = 0;
	printf("NAND dump complete!        \n");
	fclose(f);
	return 1;
}

int DumpFirmware()
{
	mkdir("dsidump", 0);
	
	FILE* f = fopen("dsidump/dsfirmware.bin", "wb");
	if (!f)
	{
		printf("Error: failed to create firmware file\n");
		return 0;
	}
	
	printf("Dumping DS-mode firmware...\n");
	
	memset(workBuffer, 0, 128*1024);
	FW_Read(workBuffer);
	
	if (fwrite(workBuffer, 128*1024, 1, f) != 1)
	{
		console->cursorX = 0;
		printf("Error: firmware file write failed\n");
		fclose(f);
		return 0;
	}
	
	console->cursorX = 0;
	printf("DS-mode firmware dump complete! ");
	fclose(f);
	return 1;
}

int DumpBIOS()
{
	FILE* f;
	mkdir("dsidump", 0);
	
	// 'augmented', NO$GBA style BIOS dump
	// FFFF0000-FFFF8000 can be dumped normally
	// FFFF87F4: 0x400 bytes at 01FFC400
	// FFFF9920: 0x80 bytes at 01FFC800
	// FFFF99A0: 0x1048 bytes at 01FFC894
	// FFFFA9E8: 0x1048 bytes at 01FFD8DC
	
	memset(workBuffer, 0, 0x10000);
	f = fopen("dsidump/bios9i.bin", "wb");
	if (!f)
	{
		printf("Error: failed to create ARM9 BIOS file\n");
		return 0;
	}
	
	fwrite((const void*)0xFFFF0000, 0x8000, 1, f);
	fwrite(workBuffer, 0x7F4, 1, f);
	fwrite(&biosKeys[0x0400], 0x400, 1, f);
	fwrite(workBuffer, 0xD2C, 1, f);
	fwrite(&biosKeys[0x0800], 0x80, 1, f);
	fwrite(&biosKeys[0x0894], 0x1048, 1, f);
	fwrite(&biosKeys[0x18DC], 0x1048, 1, f);
	fwrite(workBuffer, 0x45D0, 1, f);
	
	fclose(f);
	printf("ARM9 BIOS dump complete!\n");
	
	
	f = fopen("dsidump/bios7i.bin", "wb");
	if (!f)
	{
		printf("Error: failed to create ARM7 BIOS file\n");
		return 0;
	}
	
	fifoSendAddress(FIFO_USER_02, workBuffer);
	fifoSendValue32(FIFO_USER_01, 1);
	fifoWaitValue32(FIFO_USER_01);
	fifoGetValue32(FIFO_USER_01);
	
	DC_InvalidateRange(workBuffer, 0x10000);
	fwrite(workBuffer, 0x10000, 1, f);
	
	fclose(f);
	printf("ARM7 BIOS dump complete!\n");
	
	
	// switch to DS BIOS
	fifoSendValue32(FIFO_USER_01, 2);
	fifoWaitValue32(FIFO_USER_01);
	fifoGetValue32(FIFO_USER_01);
	
	
	f = fopen("dsidump/bios9.bin", "wb");
	if (!f)
	{
		printf("Error: failed to create DS-mode ARM9 BIOS file\n");
		return 0;
	}
	
	DC_InvalidateRange((void*)0xFFFF0000, 0x1000);
	fwrite((const void*)0xFFFF0000, 0x1000, 1, f);
	
	fclose(f);
	printf("DS-mode ARM9 BIOS dump complete!");
	
	
	f = fopen("dsidump/bios7.bin", "wb");
	if (!f)
	{
		printf("Error: failed to create DS-mode ARM7 BIOS file\n");
		return 0;
	}
	
	fifoSendAddress(FIFO_USER_02, workBuffer);
	fifoSendValue32(FIFO_USER_01, 3);
	fifoWaitValue32(FIFO_USER_01);
	fifoGetValue32(FIFO_USER_01);
	
	DC_InvalidateRange(workBuffer, 0x4000);
	fwrite(workBuffer, 0x4000, 1, f);
	
	fclose(f);
	printf("DS-mode ARM7 BIOS dump complete!");
	return 1;
}


void BIOS_VerifyKeys()
{
	u16 crc0 = swiCRC16(0xFFFF, &biosKeys[0x0400], 0x400);
	u16 crc1 = swiCRC16(0xFFFF, &biosKeys[0x0800], 0x80);
	u16 crc2 = swiCRC16(0xFFFF, &biosKeys[0x0894], 0x1048);
	u16 crc3 = swiCRC16(0xFFFF, &biosKeys[0x18DC], 0x1048);
	
	if (crc0 != 0xEB51 ||
		crc1 != 0xE88D ||
		crc2 != 0xC6F4 ||
		crc3 != 0x3A6E)
		printf("/!\\ ARM9 BIOS dump may be bad   (ITCM was altered after boot)\n");
	
	fifoSendValue32(FIFO_USER_01, 7);
	fifoWaitValue32(FIFO_USER_01);
	u32 v0 = fifoGetValue32(FIFO_USER_01);
	fifoWaitValue32(FIFO_USER_01);
	u32 v1 = fifoGetValue32(FIFO_USER_01);
	
	crc0 = v0 & 0xFFFF;
	crc1 = v0 >> 16;
	crc2 = v1 & 0xFFFF;
	crc3 = v1 >> 16;
	
	if (crc0 != 0xCB75 ||
		crc1 != 0x8340 ||
		crc2 != 0x53AB ||
		crc3 != 0x4F71)
		printf("/!\\ ARM7 BIOS dump may be bad   (WRAM was altered after boot)\n");
}

int halt()
{
	printf("Press SELECT to quit\n");
	for (;;)
	{
		scanKeys();
		u32 input = keysDown();
		if (input & KEY_SELECT) return 0;
		swiWaitForVBlank();
	}
}

int main(void) 
{
	DC_FlushAll();
	disableMPU();
	memcpy(biosKeys, (const void*)0x01FFC000, 0x4000);
	enableMPU();

	console = consoleDemoInit();
	
	u32 scfg = *(vu32*)0x04004000;
	if (!(scfg & 0x1))
	{
		printf("Error: not running in DSi mode\n");
		return halt();
	}
	
	if (!fatInitDefault())
	{
		printf("Error: FAT init failed\n");
		return halt();
	}
	
	u32 res = NAND_Init();
	if (res != 0)
	{
		printf("Error: NAND init failed\n");
		return halt();
	}
	
	SDMMC_EnableDMA();
	
	workBuffer = (u8*)malloc(8*1024*1024);
	if (!workBuffer)
	{
		printf("Error: memory allocation failed\n");
		return halt();
	}
	
	DrawMenu();
	
	BIOS_VerifyKeys();
	
	for (;;)
	{
		scanKeys();
		u32 input = keysDown();
		if (input & KEY_SELECT)
		{
			free(workBuffer);
			return 0;
		}
		
		if (input & KEY_A)
		{
			DrawMenu();
			if (DumpNAND())
			{
				if (DumpFirmware())
				{
					DumpBIOS();
				}
			}
			
			break;
		}
		else if (input & KEY_B)
		{
			DrawMenu();
			DumpNAND();
		}
		else if (input & KEY_X)
		{
			DrawMenu();
			DumpFirmware();
		}
		else if (input & KEY_Y)
		{
			DrawMenu();
			DumpBIOS();
			
			break;
		}
		
		swiWaitForVBlank();
	}
	
	free(workBuffer);
	
	console->cursorX = 0; console->cursorY = 2;
	printf("Press SELECT to exit            ");
	printf("                                ");
	printf("                                ");
	printf("                                ");
	printf("                                ");

	for (;;)
	{
		scanKeys();
		u32 input = keysDown();
		if (input & KEY_SELECT) break;
		swiWaitForVBlank();
	}

	return 0;
}
