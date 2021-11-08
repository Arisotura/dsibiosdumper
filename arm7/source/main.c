#include <nds.h>
#include <dswifi7.h>
#include <maxmod7.h>
#include <string.h>


void VblankHandler(void) 
{
	Wifi_Update();
}

void VcountHandler() 
{
	inputGetAndSend();
}

volatile bool exitflag = false;

void powerButtonCB() 
{
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


extern void sdmmc_send_command(struct mmcdevice *ctx, uint32_t cmd, uint32_t args);

void sdmmc_send_command_DMA(struct mmcdevice *ctx, uint32_t cmd, uint32_t args) 
{
	int i;
    bool getSDRESP = (cmd << 15) >> 31;
    uint16_t flags = (cmd << 15) >> 31;
    const bool readdata = cmd & 0x20000;
    const bool writedata = cmd & 0x40000;
	
    if(readdata || writedata)
    {
        flags |= TMIO_STAT0_DATAEND;
    }
	
	uint32_t size = ctx->size;
    uint16_t *dataPtr = (uint16_t*)ctx->data;
    uint32_t *dataPtr32 = (uint32_t*)ctx->data;

    bool useBuf = ( NULL != dataPtr );
    bool useBuf32 = (useBuf && (0 == (3 & ((uint32_t)dataPtr))));
	
	const int dmano = 0;
	vu32* dmareg = (vu32*)(0x04004104 + (dmano * 0x1C));
	
	bool dma = false;
	if (readdata && useBuf32)
	{
		dmareg[0] = 0x0400490C;
		dmareg[1] = (u32)dataPtr32;
		dmareg[2] = ctx->size >> 2;
		dmareg[3] = 0x80;
		dmareg[4] = 0;
		dmareg[6] = (0<<10) | (2<<13) | (7<<16) | (0x08<<24) | (0<<29) | (1<<30) | (1<<31);
		dma = true;
	}
	else if (writedata && useBuf32)
	{
		dmareg[0] = (u32)dataPtr32;
		dmareg[1] = 0x0400490C;
		dmareg[2] = ctx->size >> 2;
		dmareg[3] = 0x80;
		dmareg[4] = 0;
		dmareg[6] = (2<<10) | (0<<13) | (7<<16) | (0x08<<24) | (0<<29) | (1<<30) | (1<<31);
		dma = true;
	}

    ctx->error = 0;
    while((sdmmc_read16(REG_SDSTATUS1) & TMIO_STAT1_CMD_BUSY)); //mmc working?
    sdmmc_write16(REG_SDIRMASK0,0);
    sdmmc_write16(REG_SDIRMASK1,0);
    sdmmc_write16(REG_SDSTATUS0,0);
    sdmmc_write16(REG_SDSTATUS1,0);
	
	u32 oldime = REG_IME;
	u32 oldie = REG_IE;
	REG_IE = ((oldime == 0) ? 0 : oldie) | (1<<28);
	REG_IME = 1;

    sdmmc_write16(REG_SDCMDARG0,args &0xFFFF);
    sdmmc_write16(REG_SDCMDARG1,args >> 16);
    sdmmc_write16(REG_SDCMD,cmd &0xFFFF);
	
	if (dma)
	{
		vu32* irqflags = (vu32*)0x0380FFF8;
		for (;;)
		{
			swiHalt();
			if (*irqflags & (1<<28))
			{
				*irqflags &= ~(1<<28);
				break;
			}
		}
	}

    uint16_t status0 = 0;

    while(1) 
	{
        volatile uint16_t status1 = sdmmc_read16(REG_SDSTATUS1);
        volatile uint16_t ctl32 = sdmmc_read16(REG_SDDATACTL32);
		if (!dma)
		{
			if((ctl32 & 0x100))
			{
				if(readdata) 
				{
					if(useBuf) 
					{
						sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_RXRDY, 0);
						if(size > 0x1FF) 
						{
							if(useBuf32) 
							{
								for(i = 0; i<0x200; i+=4) 
								{
									*dataPtr32++ = sdmmc_read32(REG_SDFIFO32);
								}
							} 
							else 
							{
								for(i = 0; i<0x200; i+=2) 
								{
									*dataPtr++ = sdmmc_read16(REG_SDFIFO);
								}
							}

							size -= 0x200;
						}
					}

					sdmmc_mask16(REG_SDDATACTL32, 0x800, 0);
				}
			}

			if(!(ctl32 & 0x200))
			{
				if(writedata) 
				{
					if(useBuf) 
					{
						sdmmc_mask16(REG_SDSTATUS1, TMIO_STAT1_TXRQ, 0);
						if(size > 0x1FF) 
						{
							for(i = 0; i<0x200; i+=4) 
							{
								sdmmc_write32(REG_SDFIFO32,*dataPtr32++);
							}
							size -= 0x200;
						}
					}

					sdmmc_mask16(REG_SDDATACTL32, 0x1000, 0);
				}
			}
		}
		
        if(status1 & TMIO_MASK_GW) 
		{
            ctx->error |= 4;
            break;
        }

        if(!(status1 & TMIO_STAT1_CMD_BUSY)) 
		{
            status0 = sdmmc_read16(REG_SDSTATUS0);
            if(sdmmc_read16(REG_SDSTATUS0) & TMIO_STAT0_CMDRESPEND) 
			{
                ctx->error |= 0x1;
            }
            if(status0 & TMIO_STAT0_DATAEND) 
			{
                ctx->error |= 0x2;
            }

            if((status0 & flags) == flags)
                break;
        }
    }
    ctx->stat0 = sdmmc_read16(REG_SDSTATUS0);
    ctx->stat1 = sdmmc_read16(REG_SDSTATUS1);
    sdmmc_write16(REG_SDSTATUS0,0);
    sdmmc_write16(REG_SDSTATUS1,0);

    if(getSDRESP != 0) 
	{
        ctx->ret[0] = sdmmc_read16(REG_SDRESP0) | (sdmmc_read16(REG_SDRESP1) << 16);
        ctx->ret[1] = sdmmc_read16(REG_SDRESP2) | (sdmmc_read16(REG_SDRESP3) << 16);
        ctx->ret[2] = sdmmc_read16(REG_SDRESP4) | (sdmmc_read16(REG_SDRESP5) << 16);
        ctx->ret[3] = sdmmc_read16(REG_SDRESP6) | (sdmmc_read16(REG_SDRESP7) << 16);
    }
	
	dmareg[6] = 0;
	
	REG_IME = oldime;
	REG_IE = oldie;
}


extern void hijackFunc(void* oldfunc, void* newfunc);

u8 biosKeys[0x4000];

int main() 
{
	memcpy(biosKeys, (const void*)0x03FFC000, 0x4000);
	
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
					memcpy(&workBuffer[0x8188], &biosKeys[0x0400], 0x200);
					memcpy(&workBuffer[0xB5D8], &biosKeys[0x0600], 0x40);
					memcpy(&workBuffer[0xC6D0], &biosKeys[0x0654], 0x1048);
					memcpy(&workBuffer[0xD718], &biosKeys[0x169C], 0x1048);
					
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
				
			case 4:
				{
					*(vu32*)0x04004100 = 0;
					irqEnable(BIT(28)); // NDMA 0
					
					fifoSendValue32(FIFO_USER_03, (u32)&sdmmc_send_command);
					fifoSendValue32(FIFO_USER_03, (u32)&sdmmc_send_command_DMA);
					fifoSendValue32(FIFO_USER_03, 0x12345678);
					
					u32 ime = enterCriticalSection();
					hijackFunc((void*)sdmmc_send_command, (void*)sdmmc_send_command_DMA);
					leaveCriticalSection(ime);
					
					fifoSendValue32(FIFO_USER_01, 1312);
				}
				break;
				
			case 5:
				{
					u8* buf = (u8*)fifoGetAddress(FIFO_USER_02);
					
					const char* footerid = "DSi eMMC CID/CPU";
					strncpy(&buf[0x00], footerid, 16);
					
					sdmmc_get_cid(1, (u32*)&buf[0x10]);
					
					*(u32*)&buf[0x20] = *(vu32*)0x04004D00;
					*(u32*)&buf[0x24] = *(vu32*)0x04004D04;
					
					fifoSendValue32(FIFO_USER_01, 1312);
				}
				break;
				
			case 6:
				{
					u8* buf = (u8*)fifoGetAddress(FIFO_USER_02);
					
					readFirmware(0, buf, 128*1024);
					
					fifoSendValue32(FIFO_USER_01, 1312);
				}
				break;
				
			case 7:
				{
					u16 crc0 = swiCRC16(0xFFFF, &biosKeys[0x0400], 0x200);
					u16 crc1 = swiCRC16(0xFFFF, &biosKeys[0x0600], 0x40);
					u16 crc2 = swiCRC16(0xFFFF, &biosKeys[0x0654], 0x1048);
					u16 crc3 = swiCRC16(0xFFFF, &biosKeys[0x169C], 0x1048);
					
					fifoSendValue32(FIFO_USER_01, crc0 | (crc1 << 16));
					fifoSendValue32(FIFO_USER_01, crc2 | (crc3 << 16));
				}
				break;
			}
		}
		
		swiWaitForVBlank();
	}
	return 0;
}
