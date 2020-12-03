/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	Wifi_Update();
}


//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

extern u16 biosRead16(u32 addr);

void biosDump(void* dst, const void* src, u32 len)
{
	u16* _dst = (u16*)dst;
	
	for (u32 i = 0; i < len; i+=2)
	{
		_dst[i>>1] = biosRead16(((u32)src) + i);
	}
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

	readUserSettings();
	ledBlink(0);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	touchInit();

	mmInstall(FIFO_MAXMOD);

	SetYtrigger(80);

	installWifiFIFO();
	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

	setPowerButtonCB(powerButtonCB);

	// Keep the ARM7 mostly idle
	while (!exitflag) 
	{
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) 
		{
			exitflag = true;
			break;
		}
		
		if (fifoCheckValue32(FIFO_USER_01))
		{
			u32 action = fifoGetValue32(FIFO_USER_01);
			switch (action)
			{
			case 1:
				{
					u8* workBuffer = (u8*)fifoGetAddress(FIFO_USER_02);
					
					// 'augmented', NO$GBA-style BIOS dump
					// 00000000: exception vectors, protected
					// 00000020-00008000 can be dumped normally
					// 00008188: 0x200 bytes at 03FFC400
					// 0000B5D8: 0x40 bytes at 03FFC600
					// 0000C6D0: 0x1048 bytes at 03FFC654
					// 0000D718: 0x1048 bytes at 03FFD69C
					
					memset(workBuffer, 0, 0x10000);
					
					*(u32*)&workBuffer[0x0000] = 0xEA000006;
					*(u32*)&workBuffer[0x0004] = 0xEA000006;
					*(u32*)&workBuffer[0x0008] = 0xEA00001F;
					*(u32*)&workBuffer[0x000C] = 0xEA000004;
					*(u32*)&workBuffer[0x0010] = 0xEA000003;
					*(u32*)&workBuffer[0x0014] = 0xEAFFFFFE;
					*(u32*)&workBuffer[0x0018] = 0xEA000013;
					*(u32*)&workBuffer[0x001C] = 0xEA000000;
					
					biosDump(&workBuffer[0x0020], (const void*)0x00000020, 0x7FE0);
					memcpy(&workBuffer[0x8188], (const void*)0x03FFC400, 0x200);
					memcpy(&workBuffer[0xB5D8], (const void*)0x03FFC600, 0x40);
					memcpy(&workBuffer[0xC6D0], (const void*)0x03FFC654, 0x1048);
					memcpy(&workBuffer[0xD718], (const void*)0x03FFC69C, 0x1048);
					
					fifoSendValue32(FIFO_USER_01, 1312);
				}
				break;
				
			case 2:
				{
					// switch to DS BIOS
					*(vu32*)0x04004000 = 0x0202;
					
					fifoSendValue32(FIFO_USER_01, 1312);
				}
				break;
				
			case 3:
				{
					u8* workBuffer = (u8*)fifoGetAddress(FIFO_USER_02);
					
					// dump DS-mode BIOS
					
					memset(workBuffer, 0, 0x10000);
					
					*(u32*)&workBuffer[0x0000] = 0xEA000006;
					*(u32*)&workBuffer[0x0004] = 0xEA000B20;
					*(u32*)&workBuffer[0x0008] = 0xEA000B73;
					*(u32*)&workBuffer[0x000C] = 0xEA000B1E;
					*(u32*)&workBuffer[0x0010] = 0xEA000B1D;
					*(u32*)&workBuffer[0x0014] = 0xEA000B1C;
					*(u32*)&workBuffer[0x0018] = 0xEA000B69;
					*(u32*)&workBuffer[0x001C] = 0xEA000B1A;
					
					biosDump(&workBuffer[0x0020], (const void*)0x00000020, 0x3FE0);
					
					fifoSendValue32(FIFO_USER_01, 1312);
				}
				break;
			}
		}
		
		swiWaitForVBlank();
	}
	return 0;
}
