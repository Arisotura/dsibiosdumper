/*---------------------------------------------------------------------------------

	Simple console print demo
	-- dovoto

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>

u32 getCP15Ctrl();
void disableMPU();

u8 workBuffer[0x10000];

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	DC_FlushAll();
	disableMPU();

	consoleDemoInit();  //setup the sub screen for printing

	printf("DSi BIOS dumper - by Arisotura\n");
	
	u32 scfg = *(vu32*)0x04004000;
	if (!(scfg & 0x1))
	{
		printf("Error: not running in DSi mode\n");
		for (;;) swiWaitForVBlank();
	}
	
	if (!fatInitDefault())
	{
		printf("Error: FAT init failed\n");
		for (;;) swiWaitForVBlank();
	}
	
	printf("Press A to dump BIOS to the SD\ncard\n");
	
	for (;;)
	{
		scanKeys();
		u32 input = keysDown();
		if (input & KEY_START) return 0;
		if (input & KEY_A) break;
		swiWaitForVBlank();
	}
	
	memset(workBuffer, 0, 0x10000);
	FILE* f;
	
	printf("Dumping DSi-mode BIOS\n");
	
	// 'augmented', NO$GBA style BIOS dump
	// FFFF0000-FFFF8000 can be dumped normally
	// FFFF87F4: 0x400 bytes at 01FFC400
	// FFFF9920: 0x80 bytes at 01FFC800
	// FFFF99A0: 0x1048 bytes at 01FFC894
	// FFFFA9E8: 0x1048 bytes at 01FFD8DC
	
	f = fopen("bios9i.bin", "wb");
	
	fwrite((const void*)0xFFFF0000, 0x8000, 1, f);
	fwrite(workBuffer, 0x7F4, 1, f);
	fwrite((const void*)0x01FC400, 0x400, 1, f);
	fwrite(workBuffer, 0xD2C, 1, f);
	fwrite((const void*)0x01FC800, 0x80, 1, f);
	fwrite((const void*)0x01FC894, 0x1048, 1, f);
	fwrite((const void*)0x01FFD8C, 0x1048, 1, f);
	fwrite(workBuffer, 0x45D0, 1, f);
	
	fclose(f);
	printf("ARM9 BIOS: bios9i.bin\n");
	
	
	f = fopen("bios7i.bin", "wb");
	
	fifoSendAddress(FIFO_USER_02, workBuffer);
	fifoSendValue32(FIFO_USER_01, 1);
	
	while (!fifoCheckValue32(FIFO_USER_01));
	fifoGetValue32(FIFO_USER_01);
	
	fwrite(workBuffer, 0x10000, 1, f);
	
	fclose(f);
	printf("ARM7 BIOS: bios7i.bin\n");
	
	
	printf("Dumping DS-mode BIOS\n");
	
	fifoSendValue32(FIFO_USER_01, 2);
	while (!fifoCheckValue32(FIFO_USER_01));
	fifoGetValue32(FIFO_USER_01);
	
	
	f = fopen("bios9.bin", "wb");
	
	fwrite((const void*)0xFFFF0000, 0x1000, 1, f);
	
	fclose(f);
	printf("ARM9 BIOS: bios9.bin\n");
	
	
	f = fopen("bios7.bin", "wb");
	
	fifoSendAddress(FIFO_USER_02, workBuffer);
	fifoSendValue32(FIFO_USER_01, 3);
	
	while (!fifoCheckValue32(FIFO_USER_01));
	fifoGetValue32(FIFO_USER_01);
	
	fwrite(workBuffer, 0x4000, 1, f);
	
	fclose(f);
	printf("ARM7 BIOS: bios7.bin\n");
	
	
	printf("Done, press START to exit\n");

	for (;;)
	{
		scanKeys();
		u32 input = keysDown();
		if (input & KEY_START) break;
		swiWaitForVBlank();
	}

	return 0;
}
