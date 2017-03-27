/**
  ******************************************************************************
  * @file    com.c
  * @author  Fotona d.d.
  * @version V1
  * @date    30-Sept-2013
  * @brief	 COM port parsing related functionality
  *
  */

/** @addtogroup PFM6_Application
* @{
*/
#include	"pfm.h"

#include	<ctype.h>
#include	<math.h>
#include	<string.h>
#include	<ff.h>
#include	"limits.h"


// override vektorjev za STM32F401-Discovery.lib

//void DMA1_Stream3_IRQHandler(void) {
//void SPIx_DMA_RX_IRQ(void);
//		 SPIx_DMA_RX_IRQ();
//}
//void DMA1_Stream4_IRQHandler(void) {
//void SPIx_DMA_TX_IRQ(void);
//		 SPIx_DMA_TX_IRQ();
//}

short		user_shape[2048];
//___________________________________________________________________________
void		Cfg(_fsdrive n, char *filename) {
int			i,j;
FATFS		fs;
FIL			f;
_io*		io;
				if(f_chdrive(n)==FR_OK)
					if(f_mount(n,&fs)==FR_OK)
						if(f_open(&f,filename,FA_READ)==FR_OK) {
							while(!f_eof(&f)) {
								io=_stdio(__com0);
								if(f_read(&f,&i,1,(UINT *)&j)==FR_OK && j==1)
									ungetch(i);
								_stdio(io);
								App_Loop();
							}
							f_close(&f);
							f_mount(n,NULL);
						}
}
//___________________________________________________________________________
void		deInitialize_usb(void);
void		Initialize_host_msc(void);
void		Initialize_device_msc(void);
void		Initialize_device_vcp(void);
//___________________________________________________________________________
int			DecodeMinus(char *c) {
				char		*cc[8];
				int			m,n,*p;
				switch(*c) {
//__________________________________________________usb host/file/serial_____
				case 'u':
				if(strscan(c,cc,' ')==2) {
					deInitialize_usb();
					Wait(200,App_Loop);
					switch(*cc[1]) {
						case 'h':
#ifdef __DISCO__																		// USB power on
							GPIO_ResetBits(GPIOC,GPIO_Pin_0);
#endif
							USBH_App=USBH_Iap;
							Initialize_host_msc();
							break;
						case 'f':
							Initialize_device_msc();
							break;
						case 's':
							Initialize_device_vcp();
							break;
					}
					break;
				}
				return _PARSE_ERR_SYNTAX;
//__________________________________________________i2c disable _______
				case 'i':
					__charger6=NULL;
					break;
//__________________________________________________can loop/net_____________
				case 'c':
				if(strscan(c,cc,' ')==2)
					__can=Initialize_CAN(*cc[1]-'n');
				break;
//__________________________________________________defragment ______________
				case 'P':
				{
					int i;
					i=Defragment(0);
					printf("\r\n>Packing sectors... %d%c",i,'%');
					if(i>10) {
						Watchdog_init(4000);
						Defragment(-1);
					}
					printf(" Done");
					Watchdog_init(300);
				}
				break;
//__________________________________________________formatting flash drive___
				case 'F':
					if(strscan(c,cc,' ')<2)
						return _PARSE_ERR_MISSING;
					switch(*cc[1]) {
FATFS				fs_usb;	
						case 'u':
							printf("\rFormat usb ...[y/n]");
							do {
								Wait(5,App_Loop);
								m=fgetc(&__stdin);
							} while(m==-1);
							if(m == 'y') {
								f_mount(FSDRIVE_USB,&fs_usb);
								f_mkfs(FSDRIVE_USB,1,4096);
								f_mount(FSDRIVE_USB,NULL);
							}
							break;

						case 'f':
							printf("\rFormat flash ...[y/n]");
							Watchdog_init(4000);
							do {
								Wait(5,App_Loop);
								m=fgetc(&__stdin);
							} while(m==-1);
							if(m == 'y') {
FATFS						fs_cpu;				
								printf(" erasing ");
								for(n=0; n<6; ++n)
									if(FLASH_Erase(FATFS_SECTOR+n*FLASH_Sector_1)==FLASH_COMPLETE)
										printf(".");
									else
										printf("?");
								f_mount(FSDRIVE_CPU,&fs_cpu);
								f_mkfs(FSDRIVE_CPU,1,4096);
								f_mount(FSDRIVE_CPU,NULL);
							}
							break;
							
						default:
							return _PARSE_ERR_ILLEGAL;
						}
						break;
				case 'l':
						Initialize_LED(cc,strscan(++c,cc,','));
						break;
//__________________________________________________querying flash sector_______________
				case 'q':
					n=strscan(++c,cc,',');
					if(n) {
						for(p=(int *)FATFS_ADDRESS; p[512/4] != -1; p=&p[512/4+1]) {
							if(p[512/4] == getHEX(cc[0],EOF)) {
								for(m=0; m<512; m+=16) {
									printf("\r\n");
									for(n=0,c=(char *)p; n<16; ++n)
										printf("%02X ",(~(c[m+n]))&0xff);
									for(n=0; n<16; ++n) {
										if((((~(c[m+n]))&0xff)>0x20) && (((~(c[m+n]))&0xff)<0x7f))
											printf("%c",(~(c[m+n]))&0xff);
										else
											printf(".");
									}
								}
								printf("\r\n----------------------------------------------------------------");
								for(m=-1;m==-1;m=fgetc(&__stdin))
									App_Loop();
								if(m==0x1b)
									break;
							}
						}
						break;
					} else
					SectorQuery();
				break;
//______________________________________________________________________________________
				case '?':
					n=strscan(++c,cc,',');

					if(n==1 && *cc[0]=='1') {
						ScopeDumpBinary(ADC1_buf,_MAX_BURST/_MAX_ADC_RATE);
						break;
					}
					if(n==1 && *cc[0]=='2') {
						ScopeDumpBinary(ADC2_buf,_MAX_BURST/_MAX_ADC_RATE);
						break;
					}
					if(n==2) {
						ScopeDumpBinary((_ADCDMA *)atoi(cc[0]),atoi(cc[1]));
						break;
					}
					return _PARSE_ERR_SYNTAX;
//__________________________________________________ mode setup _____________
				case 'm':
					n=strscan(++c,cc,',');
					while(n--)
						_CLEAR_MODE(pfm,atoi(cc[n]));	
					break;
//__________________________________________________ error setup _____________
				case 'E':
					n=strscan(++c,cc,',');
					while(n--)
						_CLEAR_ERROR(pfm,getHEX(cc[n],-1));
					break;
//__________________________________________________ debug setup _____________
				case 'D':
					__dbug=__stdin.handle.io;
					n=strscan(++c,cc,',');
					while(n--)
						_CLEAR_DBG(pfm,strtol(cc[n],NULL,0));
					break;
//__________________________________________________ delay execution ________
				case 'd':
					if(strscan(c,cc,' ')==2) {
						Wait(atoi(cc[1]),App_Loop);
					}
					break;
//______________________________________________________________________________________
				default:
					return _PARSE_ERR_SYNTAX;
				}
				return _PARSE_OK;
}
//______________________________________________________________________________________
int			DecodePlus(char *c) {

				char		*cc[8];
				int			n;
				switch(*c) {
//__________________________________________________ mode setup _____________
				case 'm':
					n=strscan(++c,cc,',');
					while(n--)
						_SET_MODE(pfm,atoi(cc[n]));
					break;
//__________________________________________________ events setup _____________
				case 'e':
					n=strscan(++c,cc,',');
					while(n--)
						_SET_EVENT(pfm,atoi(cc[n]));
					break;
//__________________________________________________ watchdog setup _____________
				case 'w':
					if(strscan(++c,cc,','))
						Watchdog_init(atoi(cc[0]));
					else
						Watchdog_init(300);
					break;
//__________________________________________________ error setup _____________
				case 'E':
					n=strscan(++c,cc,',');
					while(n--)
						_SET_ERROR(pfm,getHEX(cc[n],-1));
					break;
//__________________________________________________ debug setup _____________
				case 'D':
					__dbug=__stdin.handle.io;
					n=strscan(++c,cc,',');
					while(n--)
						_SET_DBG(pfm,atoi(cc[n]));
					break;
//__________________________________________________ I2C setup _____________
				case 'i':
					switch(strscan(++c,cc,',')) {
						case 0:
							__charger6=Initialize_I2C(0x58,50000);
						break;
						case 1:
							__charger6=Initialize_I2C(getHEX(cc[0],2),0);
						break;
						case 2:
							__charger6=Initialize_I2C(getHEX(cc[0],2),atoi(cc[1]));
						break;
					}
					break;
//______________________________________________________________________________________
				default:
					return _PARSE_ERR_SYNTAX;
				}
				return _PARSE_OK;
}
//______________________________________________________________________________________
int			EnterFile(char *c) {
static
FIL			*f=NULL;																// file object pointer												
				if(!f) {																// first alloc
					f=calloc(1,sizeof(FIL));	
					if(!f)																// mem error, exit
						return _PARSE_ERR_MEM;
					if(f_open(f,c,FA_READ|FA_WRITE|FA_OPEN_ALWAYS)==FR_OK &&
						f_lseek(f,f_size(f))==FR_OK) {			// open & pointer to eof
							fprintf((FILE *)f,"\r");					// init. needed kwdf???
							__stdin.handle.io->parse=EnterFile;
							return _PARSE_OK;									// parser redirect
						} else {
							free(f);													// free & eroor exit othw
							return _PARSE_ERR_OPENFILE;
						}	
				}
				if(!c)																	// eol parser entry...
					printf("\r\n");	
				else
//______________________________________________________________________________________
				switch(*c) {
				case 0x04:
					f_sync(f);														// buffer flush, close file and memfree ...
					f_close(f);
					free(f);
					f=NULL;																// null pointer
					__stdin.handle.io->parse=DecodeFs;		// parser redirect
					break;
//______________________________________________________________________________________
				default:
					fprintf((FILE *)f,"%s\r\n",c);
				}
			return _PARSE_OK;
}
//______________________________________________________________________________________
int		DecodeFs(char *c) {
			char	*sc[8];

static DIR		dir;
static FATFS	fs;

			TCHAR lfn[_MAX_LFN + 1];				// long filename support
			FILINFO	fno;
			FIL			f;

			fno.lfname = lfn;
			fno.lfsize = sizeof lfn;

			if(!c) {
				printf("\r\n");
				if(f_getcwd(lfn,_MAX_LFN)==FR_OK && f_opendir(&dir,lfn)==FR_OK)
					printf("%s>",lfn);
				else
					printf("?:>"); 		
			} else
//______________________________________________________________________________________
			switch(*c) {
			case ' ':
				return DecodeFs(++c);

			case '0':
			case '1':
				if(f_chdrive(*c - '0')==FR_OK && 
					f_mount(*c - '0',&fs)==FR_OK &&
						f_getcwd(lfn,_MAX_LFN)==FR_OK) {
							f_opendir(&dir,lfn);
							if(*++c)
								return DecodeFs(c);
						}
				__stdin.handle.io->parse=DecodeFs;
				break;
//__rename file_________________________________________________________________________
			case 'r':
				if(strscan(strchr(c,' '),sc,',') != 2)
					return _PARSE_ERR_SYNTAX;
				if(f_rename(sc[0],sc[1]) != FR_OK)
					return _PARSE_ERR_SYNTAX;
				break;
//__type file_________________________________________________________________________
			case 't':
				if(strscan(c,sc,' ') != 2)
					return _PARSE_ERR_SYNTAX;
				if(f_open(&f,sc[1],FA_READ)==FR_OK) {
					printf("\r\n");
					while(f_gets(c,sizeof(c),&f))
						printf("%s",c);
					f_close(&f);
				}
				break;
//__make directory_____________________________________________________________________
			case 'm':
				if(strscan(c,sc,' ') != 2)
					return _PARSE_ERR_SYNTAX;
				if(f_mkdir(sc[1]) != FR_OK)
					return _PARSE_ERR_SYNTAX;
				break;
//__change directory_____________________________________________________________________
			case 'c':
				if(strscan(c,sc,' ') != 2)
					return _PARSE_ERR_SYNTAX;
				if(f_chdir(sc[1]) != FR_OK )
					return _PARSE_ERR_SYNTAX;
				break;
//__entering new file__________________________________________________________________
			case 'f':
				if(strscan(c,sc,' ') != 2)
					return _PARSE_ERR_SYNTAX;
				return(EnterFile(sc[1]));
//__delete || dir _____________________________________________________________________
			case 'd':
				return(DecodeFs(++c));
//__delete file________________________________________________________________________
			case 'e':
				if(strscan(c,sc,' ') != 2)
					return _PARSE_ERR_SYNTAX;
				if(f_unlink(sc[1]) != FR_OK)
					return _PARSE_ERR_SYNTAX;
				break;
//__list directory______________________________________________________________________
			case 'i':
{
				f_readdir(&dir,NULL);
				while(1) {
					f_readdir(&dir,&fno);
					if(!dir.sect)
						break;
					if(dir.lfn_idx != (WORD)-1) {
						printf("\r\n%-16s",fno.lfname);
					} else
						printf("\r\n%-16s",fno.fname);
					if (fno.fattrib & AM_DIR)
						printf("/");
					else
						printf("%d",(int)fno.fsize);
				}
}
			break;
//______________________________________________________________________________________
			case '>':
				__stdin.handle.io->parse=DecodeCom;
				return(DecodeCom(NULL));
//______________________________________________________________________________________
			default:
					return _PARSE_ERR_SYNTAX;
		}
		return _PARSE_OK;
}
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//int		DecodeSpi(char *c) {
//void	*InitializeSpi(void);
//int		ExchgSpi(int, int);		
//int		i,j,k;
//static void
//			*spi=NULL;
//			if(!spi)
//				spi=InitializeSpi();
//			if(!spi) {
//				printf(" not initialized ");
//				return _PARSE_ERR_NORESP;
//			}
//			switch(*c) {
//				case '\x0':
//					break;
//				case ' ':
//					return DecodeSpi(++c);
////________read register_________________________________________________________________
//				case 'r':
//					sscanf(++c,"%X",&i);
//					i=((i<<16) | i) | 0x00a000a0;
//					ExchgSpi(i, 4);
//					Wait(2,App_Loop);
//					i=ExchgSpi(0x00000000, 4);
//					printf(" >> %02X,%02X",i&0xff,(i>>16)&0xff);
//					break;
////________reset register________________________________________________________________
//				case '!':
//					sscanf(++c,"%X",&i);
//					i=((i<<16) | i) | 0x00c000c0;
//					ExchgSpi(i,4);
//					break;
////________write register________________________________________________________________
//				case 'w':
//					if(sscanf(++c,"%X,%X,%X",&i,&j,&k)==3) {
//						i=((i<<16) | i) | 0x00800080;
//						j=((k<<16) | j);
//					} else {
//						i=((i<<16) | i) | 0x00800080;
//						j=((j<<16) | j);
//					}
//					ExchgSpi(i,4);
//					ExchgSpi(j,4);
//					break;
////______command___________________2w______________________________________________________
//				default:
//						sscanf(c,"%X",&i);
//						i=((i<<16) | i);
//						ExchgSpi(i,4);
//						break;
//			}
//			return _PARSE_OK;
//}
//___________________________________________________________________________
int	DecodeCom(char *c) {
		char 		*cc[8];
		int		 	m,n,k;
		if(!c)
			printf("\r\n>");
		else

		switch(*c) {
//__________________________________________________SW version query____________________
				case 'v':
				case 'V':
					RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
					CRC_ResetDR();
					printf(" %d.%02d %s, <%08X>",
						SW_version/100,SW_version%100,
						__DATE__,
							CRC_CalcBlockCRC(__Vectors, (FATFS_ADDRESS-(int)__Vectors)/sizeof(int)));			//crc od vektorjev do zacetka FS
					break;
//__________________________________________________single interger read/write__________
				case 'B':
					n=strscan(++c,cc,',');
					if(n) {
						if(n>1)
							*(char *)strtol(cc[0],NULL,0)=(char)strtol(cc[1],NULL,0);
						else
							printf(",%02X",*(unsigned char *)strtol(cc[0],NULL,0));
						break;
					}
					return _PARSE_ERR_SYNTAX;
//__________________________________________________single interger read/write__________
				case 'W':
					n=strscan(++c,cc,',');
					if(n) {
						if(n>1)
							*(short *)strtol(cc[0],NULL,0)=(short)strtol(cc[1],NULL,0);
						else
							printf(",%04X",*(unsigned short *)strtol(cc[0],NULL,0));
						break;
					}
					return _PARSE_ERR_SYNTAX;
//__________________________________________________single interger read/write__________
				case 'L':
					n=strscan(++c,cc,',');
					if(n) {
						if(n>1)
							*(int *)strtol(cc[0],NULL,0)=(int)strtol(cc[1],NULL,0);
						else
							printf(",%08X",*(unsigned int *)strtol(cc[0],NULL,0));
						break;
					}
					return _PARSE_ERR_SYNTAX;
//__________________________________________________S-records read_____________________
				case 'S':
					return(sLoad(c));
//__________________________________________________S-records/trace dump________________
				case 'D':
					n=strscan(++c,cc,',');
					if(n<2)
						return _PARSE_ERR_SYNTAX;
					if(n==3 && *cc[2]=='i') {
						n = (int)getHEX(cc[1],EOF);
						iDump((int *)getHEX(cc[0],EOF),n);
					}
					else {
						n = (int)getHEX(cc[1],EOF);
						sDump((char *)getHEX(cc[0],EOF),n);
					}
					break;
//__________________________________________________submit CAN message(PFM from_)
				case '<': {
CanTxMsg	tx={0,0,CAN_ID_STD,CAN_RTR_DATA,0,0,0,0,0,0,0,0,0};
int				n;
					tx.StdId=getHEX(++c,2);
					++c;++c;
					for(n=0; n<strlen(c);++n,++n)
						tx.Data[n/2]=getHEX(&c[n],2);
					tx.DLC=n/2;
					CAN_ITConfig(__CAN__, CAN_IT_FMP0, DISABLE);
					_buffer_push(__can->rx,&tx,sizeof(CanTxMsg));
					CAN_ITConfig(__CAN__, CAN_IT_FMP0, ENABLE);
					break;
				}				
//__________________________________________________submit CAN message(PFM to __)
				case '>': {
CanTxMsg	tx={0,0,CAN_ID_STD,CAN_RTR_DATA,0,0,0,0,0,0,0,0,0};
int				i;
					tx.StdId=getHEX(++c,2);
					++c;++c;
					for(i=0; i<strlen(c);++i,++i)
						tx.Data[i/2]=getHEX(&c[i],2);
					tx.DLC=i/2;
//					CAN_ITConfig(__CAN__, CAN_IT_TME, DISABLE);	
					i=_buffer_push(__can->tx,&tx,sizeof(CanTxMsg));
//					CAN_ITConfig(__CAN__, CAN_IT_TME, ENABLE);							
					break;
				}
//__________________________________________________submit CAN message(SYS to PFM)______
				case '.': {
CanTxMsg	tx={_ID_SYS2PFM,0,CAN_ID_STD,CAN_RTR_DATA,0,0,0,0,0,0,0,0,0};
int				n;
					for(++c,n=0; n<strlen(c);++n,++n)
						tx.Data[n/2]=getHEX(&c[n],2);
					tx.DLC=n/2;
					CAN_ITConfig(__CAN__, CAN_IT_FMP0, DISABLE);
					_buffer_push(__can->rx,&tx,sizeof(CanTxMsg));
					CAN_ITConfig(__CAN__, CAN_IT_FMP0, ENABLE);
					CanReply(NULL);	
					break;
				}
//__________________________________________________i2c read_________________
				case 'r':
					n=strscan(++c,cc,',');
					if(n) {
						if(n>1)
							m=atoi(cc[1]);
						else
							m=1;
						n=getHEX(cc[0],EOF);
						if(n>255)
							n=((n>>8)&0x00ff) | ((n<<8)&0xff00);
						if(!readI2C(__charger6,(char *)(&n),m))
						{
							printf(" ...I2C error(%02X)",_PARSE_ERR_NORESP);
						} else {
							printf(",");
							printf("%0*X",2*m,n & ((1<<(m*8))-1));
						}
						break;
					}
					return _PARSE_ERR_SYNTAX;
//__________________________________________________i2c write_________________
				case 'w':
					n=strscan(++c,cc,',');
					if(n==2) {
						n=getHEX(cc[0],EOF);
						if(n>255) {
							n=((n>>8)&0x00ff) | ((n<<8)&0xff00);
							n|=getHEX(cc[1],EOF)<<16;
						}
						else
							n |= (getHEX(cc[1],EOF)<<8);
						if(!writeI2C(__charger6,(char *)(&n),strlen(cc[0])/2+strlen(cc[1])/2))
							printf(" ...I2C error(%02X)",_PARSE_ERR_NORESP);	
					} else
						return _PARSE_ERR_SYNTAX;
					break;
//______________________________________________________________________________________
				case '#':
					n=strscan(++c,cc,',');
					if(!n) {
						for(n=0;n<sizeof(user_shape)/sizeof(short);++n)
							user_shape[n]=0;	
						break;
					}
					if(n==2) {
						for(n=2;n<sizeof(user_shape)/sizeof(short);++n,++n)
							if(!user_shape[n])
								break;
						*(int *)user_shape += (pow(atoi(cc[1]),3)*atof(cc[0]))/400000.0 + 0.5;			// mJ !!!
						user_shape[n+0]=atoi(cc[0]);
						user_shape[n+1]=atoi(cc[1]);
						break;
					}
					return _PARSE_ERR_SYNTAX;
//______________________________________________________________________________________
				case 'p':
					n=strscan(++c,cc,',');

					if(!n) {
						printf("\r>p(ulse)  T,U           ... %dus,%dV",pfm->Burst.Time,pfm->Burst.Pmax*_AD2HV(pfm->Burst.HVo)/_PWM_RATE_HI);
						if(pfm->qdelay || pfm->qwidth)
							printf("\r\n>q(swch)  delay,width   ... %dus,%dus",pfm->qdelay,pfm->qwidth);
						break;
						}

					if(n>0)
						pfm->Burst.Time=atoi(cc[0]);

					if(n>1) { 
						if(atof(cc[1]) >= 1.0)
							pfm->Burst.Pmax=atof(cc[1])*_PWM_RATE_HI/(float)_AD2HV(pfm->Burst.HVo);
						else
							pfm->Burst.Pmax=_PWM_RATE_HI*atof(cc[1]);
					}

					if(n==3)
						pfm->Burst.Ptype = (ptype)atoi(cc[2]);
					else
						pfm->Burst.Ptype = _SHPMOD_MAIN;
					SetPwmTab(pfm);

					if(n==4) {																		// dodatek za vnos pockelsa 
						pfm->qdelay=atoi(cc[2]);										// ndc673476iopj
						pfm->qwidth=atoi(cc[3]);
						PFM_pockels(pfm);
					}
					
					break;
//______________________________________________________________________________________
				case 'd':
					n=strscan(++c,cc,',');

					if(!n) {
						printf("\r>d(elay)  T,PW          ... %dus,%.2f",pfm->Burst.Delay,(float)pfm->Burst.Pdelay/_PWM_RATE_HI);
						break;
						}
					if(n>0)
						pfm->Burst.Delay=atoi(cc[0]);
					if(n>1)
						pfm->Burst.Pdelay=_PWM_RATE_HI*atof(cc[1]);
					SetPwmTab(pfm);
					break;
//______________________________________________________________________________________
				case 'b':
					n=strscan(++c,cc,',');
					if(!n) {
						printf("\r>b(urst)  N,len,per     ... %d,%dus,%dms",pfm->Burst.N, pfm->Burst.Length,pfm->Burst.Repeat);
						break;
						}
					if(n>0)
						pfm->Burst.N=atoi(cc[0]);
					if(n>1)
						pfm->Burst.Length=atoi(cc[1]);
					if(n>2)
						pfm->Burst.Repeat=atoi(cc[2]);
					else
						pfm->Burst.Repeat=0;
					SetPwmTab(pfm);
					break;
//______________________________________________________________________________________
				case 'q':
				{
					n=strscan(++c,cc,',');
					if(!n) {
							printf("\r>q(shape) w,U,td,to,tref ...");
							for(k=0; shape[k].qref; ++k)
								printf("\r\n%d,%d,%d,%d,%d",				shape[k].q0,
																										(shape[k].q1*_AD2HV(pfm->Burst.HVo) + _PWM_RATE_HI/2)/_PWM_RATE_HI,
																										shape[k].q2,
																										shape[k].q3,
																										shape[k].qref);		
					break;
					} 
					if(n==5) {
						for(k=0; shape[k].qref && shape[k].qref != atoi(cc[4]); ++k);
						shape[k].q0=atoi(cc[0]);
						shape[k].q1=(_PWM_RATE_HI*atoi(cc[1]))/_AD2HV(pfm->Burst.HVo);
						shape[k].q2=atoi(cc[2]);
						shape[k].q3=atoi(cc[3]);
						shape[k].qref=atoi(cc[4]);	
					}
					else
						return _PARSE_ERR_SYNTAX; 
				}
					break;
//______________________________________________________________________________________
				case 's':
					switch(strscan(++c,cc,',')) {
						case 0:
							printf("\r>s(immer) n // t1,t2,tp ... %dns,%dns,%dus",(int)(1000*pfm->Burst.Psimm[0])/_uS,(int)(1000*pfm->Burst.Psimm[1])/_uS,_PWM_RATE_LO/_uS);
							break;
						case 1:
							PFM_command(pfm,atoi(cc[0]) & 0x3);
							break;
						case 2:
							pfm->Burst.Psimm[0]=atoi(cc[0])*_uS/1000;
							pfm->Burst.Psimm[1]=atoi(cc[1])*_uS/1000;
							SetSimmerPw(pfm);
							break;
						case 3:
							pfm->Burst.Psimm[0]=atoi(cc[0])*_uS/1000;
							pfm->Burst.Psimm[1]=atoi(cc[1])*_uS/1000;
							if(atoi(cc[2])<10 || atoi(cc[2])>100)
								return _PARSE_ERR_ILLEGAL;
							_PWM_RATE_LO=atoi(cc[2])*_uS;
//						SetSimmerPw(pfm);
							SetSimmerRate(pfm,_PWM_RATE_LO);		
							break;
						default:
							return _PARSE_ERR_SYNTAX;
					}
					break;
//______________________________________________________________________________________
				case 'a':
					switch(strscan(++c,cc,',')) {
extern int	_U1off,_U2off,_U1ref,_U2ref,_I1off,_I2off;
						case 0:
							printf("  \r>a(dc)    U1,I1,U2,I2   ... %dV,%dA,%dV,%dA",_AD2HV(ADC3_AVG*ADC1_simmer.U),_AD2I(ADC1_simmer.I),_AD2HV(ADC3_AVG*ADC2_simmer.U),_AD2I(ADC2_simmer.I));
							printf("\n\r>a(dc)    idle          ... %dV,%dA,%dV,%dA",_AD2HV(ADC3_AVG*_U1off),_AD2I(_I1off),_AD2HV(ADC3_AVG*_U2off),_AD2I(_I2off));
							printf("\n\r>a(dc)    Isimm,I,Uh,Ul ... %dA,%dA,%dV,%dV",_AD2I(pfm->Burst.Isimm),_AD2I(pfm->Burst.Imax),ADC3_AVG*_AD2HV(ADC3->HTR),ADC3_AVG*_AD2HV(ADC3->LTR));
							break;
						case 2:
							pfm->Burst.Isimm=_I2AD(atoi(cc[0]));
							pfm->Burst.Imax=_I2AD(atoi(cc[1]));
														
							ADC_AnalogWatchdogThresholdsConfig(ADC1,pfm->Burst.Isimm,0);
							ADC_AnalogWatchdogThresholdsConfig(ADC2,pfm->Burst.Isimm,0);			

							ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
							ADC_ClearITPendingBit(ADC2, ADC_IT_AWD);
							ADC_ITConfig(ADC1,ADC_IT_AWD,ENABLE);
							ADC_ITConfig(ADC2,ADC_IT_AWD,ENABLE);
							break;
						default:
							return _PARSE_ERR_SYNTAX;
					}
					break;	
//______________________________________________________________________________________
				case 'i':							
					switch(strscan(++c,cc,',')) {
						case 0:
							printf("\r>i(DAC)   i1,i2         ... %d%c,%d%c",(DAC_GetDataOutputValue(DAC_Channel_1)*100+0x7ff)/0xfff,'%',(DAC_GetDataOutputValue(DAC_Channel_2)*100+0x7ff)/0xfff,'%');
							break;
						case 2:
							DAC_SetDualChannelData(DAC_Align_12b_R,(atoi(cc[1])*0xfff+50)/100,(atoi(cc[0])*0xfff+50)/100);
							DAC_DualSoftwareTriggerCmd(ENABLE);		
							break;
						default:
							return _PARSE_ERR_SYNTAX;
					}
				break;
//______________________________________________________________________________________
				case 'x':
					switch(atoi(++c)) {
					case 1:
						_SET_MODE(pfm,_XLAP_SINGLE);
						_CLEAR_MODE(pfm,_XLAP_DOUBLE);
						_CLEAR_MODE(pfm,_XLAP_QUAD);
						break;
					case 2:
						_SET_MODE(pfm,_XLAP_DOUBLE);
						_CLEAR_MODE(pfm,_XLAP_SINGLE);
						_CLEAR_MODE(pfm,_XLAP_QUAD);
						break;
					case 4:
						_SET_MODE(pfm,_XLAP_QUAD);
						_CLEAR_MODE(pfm,_XLAP_SINGLE);
						_CLEAR_MODE(pfm,_XLAP_DOUBLE);
						break;
					default:
						return _PARSE_ERR_SYNTAX;
					}
					SetSimmerRate(pfm,_PWM_RATE_LO);
					break;
//______________________________________________________________________________________
				case 't':
					for(n=m=0; TIM18_buf[n].n; ++n) {
						printf("\r\n%d,%d,%d",m,TIM18_buf[n].T1,TIM18_buf[n].T3);
						m+= 10*(1+TIM18_buf[n].n)/2;
						printf("\r\n%d,%d,%d",m,TIM18_buf[n].T1,TIM18_buf[n].T3);
					}
					m+= 10*(1+TIM18_buf[n].n)/2;
					printf("\r\n%d,%d,%d\r\n>",m,TIM18_buf[n-1].T1,TIM18_buf[n-1].T3);
					break;
//______________________________________________________________________________________
				case 'e':
				{		
					for(pfm->Burst.Pmax=0; pfm->Burst.Pmax<600; ++pfm->Burst.Pmax) {
						SetPwmTab(pfm);
						for(k=m=n=0; TIM18_buf[k].n; ++k) {
							m+= pow((float)TIM18_buf[k].T1*7.0/6.0,3)/400.0*10.0*(float)(1+TIM18_buf[k].n)/2+0.5;
							n+= pow((float)TIM18_buf[k].T3*7.0/6.0,3)/400.0*10.0*(float)(1+TIM18_buf[k].n)/2+0.5;
						}
						Watchdog();
						printf("\r\n%d,%d,%d",pfm->Burst.Pmax*7/6,m/1000,n/1000);
					}
					printf("\r\n>");
				}
					break;
//______________________________________________________________________________________
				case 'f':
					n=strscan(++c,cc,',');
					if(!n) {
						printf("\r>f(an)    Tl,Th,min,max,T.. %d,%d,%d%c,%d%c,%d",fanTL/100,fanTH/100,fanPmin,'%',fanPmax,'%',pfm->Temp);
						break;
					} else {
						if(n==4) {
							fanTL=100*atoi(cc[0]);
							fanTH=100*atoi(cc[1]);
							fanPmin=atoi(cc[2]);
							fanPmax=atoi(cc[3]);
							break;
						}
					}
					return _PARSE_ERR_SYNTAX;
//______________________________________________________________________________________
				case 'u':
{
struct	{signed int e:3;} e3;
int			u=0,umax=0,umin=0;
					switch(strscan(++c,cc,',')) {
						case 0:
							printf("\r>u(bank)  Uc,Uc/2,20,-5 ... %dV,%dV,%.1fV,%.1fV",_AD2HV(pfm->HV),_AD2HV(pfm->HV2)/2,_AD2p20V(pfm->Up20),_AD2m5V(pfm->Um5));
							return _PARSE_OK;
						case 3:
							umax=atoi(cc[2]);
						case 2:
							umin=atoi(cc[1]);
						case 1:
							u=atoi(cc[0]);
							break;
						default:
							return _PARSE_ERR_ILLEGAL;
					}
					if(!umax)
						umax=u+u/10;
					if(!umin)
						umin=u-2*u/3;
					if(u>800 || u<0 || umin>=u || umax<=u)										
						return _PARSE_ERR_ILLEGAL;
					pfm->Burst.HVo=_HV2AD(u);
						
					ADC_ITConfig(ADC3,ADC_IT_AWD,DISABLE);
					ADC_AnalogWatchdogThresholdsConfig(ADC3,_HV2AD(umax)/ADC3_AVG,_HV2AD(umin)/ADC3_AVG);

					m=_VOUT_MODE;
					if(readI2C(__charger6,(char *)&m,1))	{
						e3.e=m;
						m = _VOUT+((u<<(8-e3.e))&~0xff);
						if(writeI2C(__charger6,(char *)&m,3))
							break;
					}
					return _PARSE_ERR_NORESP;
}
//______________________________________________________________________________________
				case 'U':
					m=_READ_VIN;
					if(readI2C(__charger6,(char *)&m,2))	{
						printf("\r>Vin=%.1f, ",__lin2f(m));
						m=_READ_VOUT;
						if(readI2C(__charger6,(char *)&m,2)) {
							printf("Vout=%.1f, ",__lin2f(m));
							m=_READ_PIN;
							if(readI2C(__charger6,(char *)&m,2)) {
								printf("Pout=%.1f",__lin2f(m));
								break;
							}
						}
					}
					return _PARSE_ERR_NORESP;
//______________________________________________________________________________________
				case '?':
					ungets("u\ri\rs\rd\rp\rb\rf\r");
				break;
//______________________________________________________________________________________
				case '-':
					return DecodeMinus(++c);
//______________________________________________________________________________________
				case '+':
					return DecodePlus(++c);
//______________________________________________________________________________________
				case ':':
					return DecodeFs(++c);
//______________________________________________________________________________________
//				case '*':
//					return DecodeSpi(++c);
//______________________________________________________________________________________
				default:
					return _PARSE_ERR_SYNTAX;
				}
			return _PARSE_OK;
}
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
//______________________________________________________________________________________
int					USBH_Iap(int call) {	
FATFS				fs0,fs1;
DIR					dir;
FIL					f0,f1;
BYTE 				buffer[1024];   																									//	file copy buffer */
FRESULT 		fr;          																											//	FatFs function common result code	*/
UINT 				br, bw;         																									//	File read/write count */
FILINFO			fno;
TCHAR				*t;
static
int					state=0;

#if _USE_LFN
static char lfn[_MAX_LFN + 1];   /* Buffer to store the LFN */
fno.lfname = lfn;
fno.lfsize = sizeof lfn;
#endif

						if(state==0 && call==0) {
							++state;
							Watchdog_init(4000);
							fno.lfname = lfn;																								//	set long filename buffer 
							fno.lfsize = sizeof lfn;
							_RED1(0);_GREEN1(0);_BLUE1(0);_YELLOW1(0);
							if(f_mount(FSDRIVE_USB,&fs0)==FR_OK)														// mount usb 
								if(f_mount(FSDRIVE_CPU,&fs1)==FR_OK)													// mount flash
									if(f_chdrive(FSDRIVE_USB)== FR_OK)													// go to usb drive
										if(f_chdir("/sync")==FR_OK)																// goto /sync directory
											if(f_opendir(&dir,lfn)==FR_OK) {												// & open it !
												while(f_readdir(&dir,&fno)==FR_OK && dir.sect) {			// scan the files, if end, exit
													if (fno.fattrib & AM_DIR)														// skip if it is a subdirectory 
														continue;
													t = *fno.lfname ? fno.lfname : fno.fname;						// check for long filenames
													
													if(f_chdrive(FSDRIVE_USB)== FR_OK && f_open(&f0,t,FA_OPEN_EXISTING | FA_READ)!=FR_OK) continue;
													if(f_chdrive(FSDRIVE_CPU)== FR_OK && f_open(&f1,t,FA_CREATE_ALWAYS | FA_WRITE)!=FR_OK) continue;

													for (;;) {
														if((__time__ / 100) % 2)
															_YELLOW1(1000);
														else
															_YELLOW1(0);															
														fr = f_read(&f0, buffer, sizeof buffer, &br);			/* Read a chunk of source file */
														if (fr || br == 0) break; 												/* error or eof */
														fr = f_write(&f1, buffer, br, &bw);								/* Write it to the destination file */
														if (fr || bw < br) break;													/* error or disk full */
														Watchdog();
													}
													++state;
												f_close(&f0);																					// close both files
												f_close(&f1);	
												}
											f_mount(FSDRIVE_USB,NULL);															// dismount both drives
											f_mount(FSDRIVE_CPU,&fs1);
											}
											if(state>1)
												_GREEN1(3000);
											else
												_RED1(3000);
						}
						
						if(call==EOF) {
							if(state>1)
								WWDG_init();
							state=0;
						}
						return(0);
					}
/**
* @}
*/
