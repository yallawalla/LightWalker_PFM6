/**
  ******************************************************************************
  * @file    app.c
  * @author  Fotona d.d.
  * @version V1
  * @date    30-Sept-2013
  * @brief	 Main PFM6 application functionality
  *
  */ 
/** @addtogroup PFM6_Application
* @{
*/
#include	"pfm.h"
#include	<string.h>
#include 	<stdarg.h>
#include 	<stdio.h>

PFM				*pfm;
int				_PWM_RATE_LO=(50*_uS),
					_I1off=0,_I2off=0,
					_U1off=0,_U2off=0,
					_E1ref=0,_E2ref=0;
/*______________________________________________________________________________
* Function Name : App_Init
* Description   : Initialize PFM object
* Input         : None
* Output        : None
* Return        : None
*/
void 			App_Init(void) {
RCC_AHB1PeriphClockCmd(	
					RCC_AHB1Periph_GPIOA |
					RCC_AHB1Periph_GPIOB |
					RCC_AHB1Periph_GPIOC | 
					RCC_AHB1Periph_GPIOD | 
					RCC_AHB1Periph_GPIOE |
					RCC_AHB1Periph_GPIOF |
					RCC_AHB1Periph_GPIOG, ENABLE);

					pfm=calloc(1,sizeof(PFM));
					pfm->Burst.E=0;								
					pfm->Burst.U=0;
					pfm->Burst.HVo=0;
					pfm->Burst.Time=100;
					pfm->Burst.Delay=100;
					pfm->Burst.N=1;
					pfm->Burst.Length=1000;
					pfm->Burst.Repeat=1000;
					pfm->Burst.Isimm=0;
					pfm->Burst.Idelay=0;
					pfm->Burst.Imax=0;
					pfm->Burst.Psimm[0]=pfm->Burst.Psimm[1]=2;
					pfm->Burst.Pdelay=_PWM_RATE_HI*0.02;
					pfm->Burst.Pmax=_PWM_RATE_HI*0.02;
					pfm->Burst.Count=pfm->Burst.Timeout=0;
					pfm->ADCRate=_uS;
					
					pfm->qdelay=0;
					pfm->qwidth=0;
				
					Initialize_NVIC();
					__com0=Initialize_USART(921600);		
					__can=Initialize_CAN(0);	
					Initialize_ADC();
					Initialize_TIM();
		
#if  			defined (__PFM6__)
					__charger6=Initialize_I2C(0x58,50000);
#elif  		defined (__DISCO__)
{
int				i;
					for(i=0; i<ADC3_AVG; ++i) {
						ADC3_buf[i].HV=ADC3_buf[i].HV2=_HV2AD(700)/ADC3_AVG;
						ADC3_buf[i].IgbtT1=ADC3_buf[i].IgbtT2=0x7ff;
						ADC3_buf[i].Um5=_m5V2AD(-4)/8;
						ADC3_buf[i].Up20=_p20V2AD(20)/8;
					}
}
#else
	#### 		error, no HW defined
#endif
					SysTick_init();
					_SET_MODE(pfm,_XLAP_QUAD);
					SetSimmerRate(pfm,_PWM_RATE_LO);
					SetPwmTab(pfm);
					Watchdog_init(300);	
					Initialize_DAC();


					_stdio(__com0);
					if(RCC_GetFlagStatus(RCC_FLAG_SFTRST) == SET)
						printf("\r ... SWR reset\r\n>");
					else if(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET)
						printf("\r ... IWDG reset\r\n>");
					else if(RCC_GetFlagStatus(RCC_FLAG_WWDGRST) == SET)
						printf("\r ... WWDG reset\r\n>");
					else if(RCC_GetFlagStatus(RCC_FLAG_PORRST) == SET)
						printf("\r ... power on reset\r\n>");
					else if(RCC_GetFlagStatus(RCC_FLAG_PINRST) == SET)
					{} else
						{}
					RCC_ClearFlag();   
					_stdio(NULL);
					Cfg(FSDRIVE_CPU, "cfg.ini");
}
/*______________________________________________________________________________
* Function Name : App_Loop
* Description   : Calls/polling of the system tasks w/ main loop
* Input         : None
* Output        : None
* Return        : None
______________________________________________________________________________*/
void			__App_Loop(void) {
					ParseCom(__com0);
					ParseCom(__com1);
					ParseCan(pfm);
					ProcessingEvents(pfm);
					USBHost();
					Watchdog();
}
void			(*App_Loop)(void)= __App_Loop;													// mainloop func. pointer
/*______________________________________________________________________________
  * @brief	ISR events polling, main loop
  * @param	PFM object
  * @retval	: None
  */
void			ProcessingEvents(PFM *p) {
//______________________________________________________________________________
static	
int				t,lastFanTachoEvent=0,trigger_cont=0;
//________1 ms periodic events__________________________________________________
					if(t != __time__) {
						t = __time__;
						_led(-1,-1);																					// led indicator timing process
						ProcessingStatus(p);																	// status & error filters process
						ProcessingCharger(p); 																// I2C comm, Charger6 status processing
//______________________________________________________________________________						
						if(!(t % 100) && !_ERROR(p,_PFM_I2C_ERR))	{						// 100 ms charger6 scan
short					m=_STATUS_WORD;
							if(readI2C(__charger6,(char *)&m,2))
								p->Error = (p->Error & 0xffff) | ((m & 0xff)<<16);
						}
						if(!(t % 3000))																				// clear i2c error every 3 secs
							_CLEAR_ERROR(p,_PFM_I2C_ERR);
//______________________________________________________________________________													
						if(trigger_cont && !(--trigger_cont))									// continuous trigger counter, for testing only
							_SET_EVENT(p,_TRIGGER);		
					}							
//______________________________________________________________________________													
					if(_EVENT(p,_FAN_TACHO)) {															// fan timeout counter reset
						static int n=0;
						_CLEAR_EVENT(p,_FAN_TACHO);
						lastFanTachoEvent = __time__;
						if(++n % 5 == 0)
							_BLUE2(20);
					}
#ifndef __DISCO__
					if((__time__ - lastFanTachoEvent > 200) && (__time__ > 10000))
						_SET_ERROR(p,_PFM_FAN_ERR);		
					else
#endif
						_CLEAR_ERROR(p,_PFM_FAN_ERR);		
//______________________________________________________________________________
					if(_EVENT(p,_TRIGGER)) {																// trigger request
						_CLEAR_EVENT(p,_TRIGGER);						
						if(p->Error & _CRITICAL_ERR_MASK)
							_CLEAR_MODE(p,_TRIGGER_PERIODIC);
						else {
							if(trigger_cont)																		// if continuous, then stop
							trigger_cont=0;
							else {
								Trigger(p);																				// else calling trigger
								if(_MODE(p,_TRIGGER_PERIODIC))											// rearm for continuous										
									trigger_cont=p->Burst.Repeat;						
								}
							}	
					}
//______________________________________________________________________________
					if(_EVENT(p,_PULSE_FINISHED)) {													// end of pulse			
						_CLEAR_EVENT(p,_PULSE_FINISHED);
						SetSimmerRate(p,_PWM_RATE_LO);												// reduce simmer
						if(Eack(p)) {																					// Energ. integrator finished
							Pref1=Pref2=0;
							while(!_EVENT(p,_ADC_FINISHED))											// wait for end of ADC recording
								Wait(2,App_Loop);
							_CLEAR_EVENT(p,_ADC_FINISHED);
							ScopeDumpBinary(NULL,0);														// scope printout, for testing(if enabled ?)
							}
					}
//______________________________________________________________________________
// __SWEEPS__ pulse counter & timeout
//
//
					if(p->Burst.Timeout > 0 && __time__ > p->Burst.Timeout) {
						p->Burst.Count=0;																			// counter & timeout reset
						p->Burst.Timeout=0;
						SetPwmTab(p);																					// final tab setup
					}
//					if(_E1ref || _E2ref) {
//						_DEBUG_MSG("%d,%d\r\n",_E1ref,_E2ref);
//						_E1ref=_E2ref=0;
//					}
//					if(!_MODE(p,_PULSE_INPROC))
//						IncrementSimmerRate(0);	
}
/*______________________________________________________________________________
  * @brief	periodic status/error  polling, main loop call from 1 msec event flag
  * @param  : current PFM object
  * @retval : None
  *
______________________________________________________________________________*/
void			ProcessingStatus(PFM *p) {
int 			i,j,k;
static	
short	 		status_image=0; 
static
int				error_image=0,
					bounce=0;
					
					for(i=j=k=0; i<ADC3_AVG; ++i) {
						j+=ADC3_buf[i].HV;
						k+=ADC3_buf[i].HV2;
					}
					p->HV=j;
					
					p->HV2=k;
					p->Up20 += (8*(ADC3_buf[0].Up20) - p->Up20)/8;
					p->Um5  += (8*(ADC3_buf[0].Um5)  - p->Um5)/8;
//-------------------------------------------------------------------------------			
					p->Temp=IgbtTemp();
					if((p->Temp > (fanTH+fanTH/2)/100) || (p->Temp < -20))
						_SET_ERROR(p,PFM_ERR_TEMP);
					else
						_CLEAR_ERROR(p,PFM_ERR_TEMP);
//-------------------------------------------------------------------------------
					if(p->Up20 < _p20V2AD(18) || p->Up20 > _p20V2AD(22))                      
						_SET_ERROR(p,PFM_ERR_48V);
					else
						_CLEAR_ERROR(p,PFM_ERR_48V);
//-------------------------------------------------------------------------------
					if(p->Um5 > _m5V2AD(-4) || p->Um5 < _m5V2AD(-6))       
						_SET_ERROR(p,PFM_ERR_15V);
					else
						_CLEAR_ERROR(p,PFM_ERR_15V);
//-------------------------------------------------------------------------------
					if(ADC3_buf[0].HV > 100 && abs(ADC3_buf[0].HV-ADC3_buf[0].HV2) > ADC3_buf[0].HV/5)
						_SET_ERROR(p,_PFM_HV2_ERR);
					else
						_CLEAR_ERROR(p,_PFM_HV2_ERR);
//-------------------------------------------------------------------------------
					if(_STATUS(p,PFM_STAT_SIMM1))	{
						if(TIM_GetITStatus(TIM1, TIM_IT_Update)==RESET) { 
							if(abs(ADC3_buf[0].HV - ADC1_simmer.U) < ADC3_buf[0].HV/8)
								_SET_ERROR(p,PFM_ERR_SIMM1);
							else
								_CLEAR_ERROR(p,PFM_ERR_SIMM1);
						}
					}
//-------------------------------------------------------------------------------
					if(_STATUS(p,PFM_STAT_SIMM2)) {
						if(TIM_GetITStatus(TIM1, TIM_IT_Update)==RESET) {
							if(abs(ADC3_buf[0].HV - ADC2_simmer.U) < ADC3_buf[0].HV/8)
								_SET_ERROR(p,PFM_ERR_SIMM2);
							else
								_CLEAR_ERROR(p,PFM_ERR_SIMM2);
						}					
					}
//-------------------------------------------------------------------------------			
					if((status_image != p->Status) || (error_image != p->Error)) {
						error_image = p->Error;	
						status_image = p->Status;
						bounce=25;
					}
//-------------------------------------------------------------------------------
					k=PFM_command(NULL,0);	
					if(bounce && !--bounce) 
						PFM_status_send(p,k);
}		
/*______________________________________________________________________________
  * @brief	Charger6 control procedure; Disables Charger6 if PFM_ERR_DRVERR,PFM_ERR_PULSEENABLE or 
	* _PFM_ADCWDG_ERR are set (from interrupts). Sets the PFM_STAT_PSRDY status bit, 
	* 
  * @param  : current PFM object
  * @retval : None
  *
______________________________________________________________________________*/
void			ProcessingCharger(PFM *p) {
static	
short	 		ton=1500,toff=1000,temg=0;
//-------------------------------------------------------------------------------
					if(p->Error  & _CRITICAL_ERR_MASK) {
						if(!temg--) {
							int i=_PFC_OFF;
							writeI2C(__charger6,(char *)&i,2);	
							ADC_ITConfig(ADC3,ADC_IT_AWD,DISABLE);	
							temg=500;
							ton=300;
							_RED2(100);
							if(PFM_command(NULL,0))												// ugasni simmerje, ce je error
								PFM_command(p,0);
						}
						return;
					}
//-------------------------------------------------------------------------------
					temg=0;
					if(p->Error)
						_RED2(100);
//-------------------------------------------------------------------------------
					if(abs(p->HV - p->Burst.HVo) < _HV2AD(50.0))	{		// +/- 50V !!!
						_SET_STATUS(p,PFM_STAT_PSRDY);
						ADC_ClearITPendingBit(ADC3, ADC_IT_AWD);
						ADC_ITConfig(ADC3,ADC_IT_AWD,ENABLE);					
						_GREEN2(100);
						toff=0;
					} else {
						if(!ton && !toff) {															// sicer timeout
							if(p->HV > p->Burst.HVo) {										// za previsoko
								toff=300;
								ton=3000;
							} else {																			// za prenizko 
								toff=3000;
								ton=3300;
							}
						}
						_CLEAR_STATUS(p,PFM_STAT_PSRDY);
					}	
					if(ton)
						if(!--ton) {
							int i=_PFC_ON;
							writeI2C(__charger6,(char *)&i,2);
						}					
					if(toff)
						if(!--toff) {
							int i=_PFC_OFF;
							writeI2C(__charger6,(char *)&i,2);	
						}			
}
/*______________________________________________________________________________
  * @brief  	__com0 port replacement driver if CAN redirection activated
  * @param p:	transmit buffer or NULL(if redirection signal ...)
	* @param c:	character to output or address of rx message
  * @retval : character written or EOF if full
	*
	*
  */
int				putCAN(_buffer *p, int	c) {
static		
int				(* _replaced)(_buffer *, int)=NULL;
static
CanTxMsg	tx={_ID_PFMcom2SYS,0,CAN_ID_STD,CAN_RTR_DATA,0,0,0,0,0,0,0,0,0};
CanRxMsg* rx=(CanRxMsg *)c;
					if(p) {																						// TX part, if p <> NULL
						if(tx.DLC < 8 && _replaced(p,c) != EOF) {				// if enough place in both buffers,	
							tx.Data[tx.DLC++]=c;													// push character onto output buffer
							if(tx.DLC != 8)
								return c;
						}
						if(tx.DLC == 8)
							return putCAN(NULL,0);
						return EOF;
					}	else {																					// RX part, p==NULL, c= rx message of NULL(flush signal ...)
						if(rx) {																				// rx=message struct
							if(rx->DLC > 0) {															// empty message ...
								if(__com0->put != putCAN) {
										_replaced=__com0->put;									// deactivate redirection
										__com0->put=putCAN;
								}																
								_buffer_LIFO(__com0->rx,rx->Data,rx->DLC);	// submit rx message, last-in-first-out
							} else {	
								if(__com0->put == putCAN)										// xchange drivers ...
									__com0->put=_replaced;
							}	
						} else {
							if(tx.DLC > 0 && CanReply(".",&tx) != EOF) {	// flush CAN if anything in the buffer ....
								tx.DLC=0;																		// reset counter
								return 0;
							} else
								return EOF;	
						}
						return(0);	
					}
}
//______________________________________________________________________________________
void			ParseCom(_io *v) {
char 			*p;
int 			i;
_io				*io;

					if(v) {
						if(!v->parse)																		// first call init
							v->parse=DecodeCom;
						io=_stdio(v);
						if(io != v) {
							i=fgetc(&__stdin);
							switch(i) {
								case _Eof:																	// empty usart
									break;
								case _CtrlZ:																// call watchdog reset
									while(1);
								case _CtrlY:																// call system reset
									NVIC_SystemReset();
								case _Esc:
									_SET_EVENT(pfm,_TRIGGER);									// ... no ja!!
									break;
								default:
									p=cgets(i,EOF);
									if(p) {
										while(*p==' ') ++p;
										i=v->parse(p);
										if(*p && i)
											printf("... WTF(%d)",i);							// error message
										v->parse(NULL);													// call newline
									}
							}
						}
						_stdio(io);
					}
}
/*______________________________________________________________________________
  * @brief  CAN message parser for PFM data as defined in CAN protocol  ICD 
  * @param rx: pointer to CanRxMsg structure
  * @retval : None
	*
	*
  */
void			ParseCan(PFM *p) {	
union			{
					CanRxMsg	rx;
					CanTxMsg	tx;
					} Can;

char			*q=(char *)&Can;
short			n;
static		
int 			inproc=0;	
//________ flushing CAN buffer ____________________________ 
					while((__CAN__->TSR & CAN_TSR_TME) && (!_buffer_empty(__can->tx)) && _buffer_pull(__can->tx,&Can.tx,sizeof(CanTxMsg))) {
							CAN_Transmit(__CAN__,&Can.tx);
//______________________________________________________________________________________
							if(_DBG(pfm,_DBG_CAN_TX) && Can.tx.StdId != _ID_PFMcom2SYS) {
								_io *io=_stdio(__dbug);
								int i;
								printf(":%04d %02X ",__time__ % 10000,Can.tx.StdId);
								for(i=0;i<Can.tx.DLC;++i)
									printf(" %02X",Can.tx.Data[i]);
								printf("\r\n>");
								_stdio(io);
							}
					}
//__________________________________________________________ fill intm. rx buffer, ISR replacement
//					if(CAN_MessagePending(__CAN__, CAN_FIFO0)) {
//						CAN_Receive(__CAN__,CAN_FIFO0, &Can.rx);
//						_buffer_push(__can->rx,&Can.rx,sizeof(CanRxMsg));
//					}
//________ flushing CAN <> COM buffer ______________________ 
					putCAN(NULL,0);
					if(!inproc++) {																		 //kuwf8823hu
//______________________________________________________________________________________					
					if(_buffer_pull(__can->rx,&Can.rx,sizeof(CanRxMsg))) {
						q=(char *)Can.rx.Data;
//______________________________________________________________________________________
						if(_DBG(p,_DBG_CAN_RX) && Can.rx.StdId != _ID_PFMcom2SYS)
						{
							_io *io=_stdio(__dbug);
							printf(":%04d %02X ",__time__ % 10000,Can.rx.StdId);
							for(n=0;n<Can.rx.DLC;++n)
								printf(" %02X",Can.rx.Data[n]);
							printf("\r\n>");
							_stdio(io);
						}
//______________________________________________________________________________________
						switch(Can.rx.StdId) {
							case _ID_SYS_TRIGG:
								_SET_EVENT(p,_TRIGGER);
							break;				
//______________________________________________________________________________________
							case _ID_PFMcom2SYS:
								break;
//______________________________________________________________________________________
							case _ID_SYS2PFMcom:
								putCAN(NULL,(int)&Can.rx);
								break;												
//______________________________________________________________________________________
							case _ID_SYS2PFM:
								switch(*(uint8_t *)q++) {
static burst bmirror;
									case _PFM_status_req:
										PFM_status_send(p,PFM_command(NULL,0));
										break;
									case _PFM_IgbtTemp_req:
										CanReply("ccP",_PFM_IgbtTemp_ack,p->Temp);
										break;
									case _PFM_U_req:
										CanReply("cwP",_PFM_U_req,p->Burst.U);
										break;
									case _PFM_command:
										PFM_command(p,Can.rx.Data[1]);
										Eack(NULL);
										break;
//________________PFM SET ___________________________________________________________________________________
									case _PFM_set:
										p->Burst.U = bmirror.U = *(short *)q++;q++;
										p->Burst.Time = bmirror.Time = *(short *)q++;q++;
										p->Burst.Ptype = bmirror.Ptype = (ptype)(*q++);
										if(LW_SpecOps(p,&bmirror)) {
											p->Burst.Pmax = __max(0,__min(_PWM_RATE_HI, (p->Burst.U *_PWM_RATE_HI)/_AD2HV(10*p->Burst.HVo)));
											if(p->Burst.Pmax > 0 && p->Burst.Pmax < _PWM_RATE_HI) {
												p->Burst.Imax=__min(4095,_I2AD(p->Burst.U/10 + p->Burst.U/2));
												SetPwmTab(p);														
												Eack(NULL);
												break;
											} 
										}
										_SET_ERROR(p,PFM_ERR_PSRDYN);
										break;
//________________PFM RESET _______________________________________smafu za preverjanje LW protokola ________
									case _PFM_reset:
										p->Burst.Repeat = bmirror.Repeat = *(short *)q++;q++;
										p->Burst.N=bmirror.N = *q++;
										p->Burst.Length = bmirror.Length = *q++;
										p->Burst.E = bmirror.E = *(short *)q;
										if(LW_SpecOps(p,&bmirror)) {
											SetPwmTab(p);
											Eack(NULL);
										} else
											_SET_ERROR(p,PFM_ERR_PSRDYN);
										break;
									case _PFM_simmer_set:
										p->Burst.Psimm[0]=*(short *)q/50 + 7;
										++q;++q;
										p->Burst.Psimm[1]=*(short *)q/50 + 7;
										SetSimmerPw(p);					
										break;
									case _PFM_RevNum_req:
										n=*(short *)q;
										CanReply("cwwP",_PFM_RevNum_req,SW_version,n);
										break;
									case _PFM_Ping:
										CanReply("cP",_PFM_Ping);
										break;
									case _PFM_Iap:
										while(1);
//_________________________________________________________________________________
//
// Pfm6 add..
//
//
// ________________________________________________________________________________
									case _PFM_POCKELS: 																					// 0x72, _PFM_POCKELS ghdg78236u
										p->qdelay=*(short *)q++;q++;
										p->qwidth=*(short *)q++;q++;
										PFM_pockels(p);
										break;

									case _PFM_SetHVmode:																				// 0x73, _PFM_SetHVmode 
									{																														// HV & mode configuration modif. by host
										char c[16];
										sprintf(c,"u %d",*(short *)q);
										if(DecodeCom(c) == _PARSE_OK) {
											_CLEAR_ERROR(p,PFM_ERR_UB);
											++q;++q;
											if(*q & 1)
												_CLEAR_MODE(p,_CHANNEL1_DISABLE);
											else
												_SET_MODE(p,_CHANNEL1_DISABLE);
											if(*q & 2)
												_CLEAR_MODE(p,_CHANNEL1_SINGLE_TRIGGER);
											else
												_SET_MODE(p,_CHANNEL1_SINGLE_TRIGGER);
											if(*q & 4)
												_CLEAR_MODE(p,_CHANNEL2_DISABLE);
											else
												_SET_MODE(p,_CHANNEL2_DISABLE);
											if(*q & 8)
												_CLEAR_MODE(p,_CHANNEL2_SINGLE_TRIGGER);
											else
												_SET_MODE(p,_CHANNEL2_SINGLE_TRIGGER);
										} else
											_SET_ERROR(p,PFM_ERR_UB);
									}
									break;

									case _PFM_CurrentLimit:																			// .10
									{
										int dac1,dac2;
										dac1=*(short *)q;
										++q;++q;
										dac2=*(short *)q;
										DAC_SetDualChannelData(DAC_Align_12b_R,dac1,dac2);
										DAC_DualSoftwareTriggerCmd(ENABLE);									
									}
										break;	
									case _PFM_HV_req:																						// .12
										CanReply("cwwwP",_PFM_HV_req,p->HV,p->HV2,p->Temp);
										break;	
									case _PFM_TRIGG:																						// .71
										_SET_EVENT(p,_TRIGGER);
										break;
								}
								break;
//______________________________________________________________________________________
								case _ID_ENRG2SYS: 																						// energometer-2-system message
								{
									void Sweep(PFM *,int);
									union {short w[4];} *e = (void *)q;			
									if(_DBG(p,_DBG_ENRG_SYS)) {
_io 								*io=_stdio(__dbug);										
										printf(":%04d e1=%.1lf,e2=%.1lf\r\n>",__time__ % 10000,
											(double)__max(0,e->w[2])/10,
												(double)__max(0,e->w[3])/10);
									_stdio(io);
									}
									if((p->Burst.Ptype & _SHPMOD_SWEEPS) && (unsigned short)e->w[0]==0xD103)
										Sweep(p,__max(0,e->w[2]));	
								}
								break;
//______________________________________________________________________________________
								case _ID_SYS2ENRG: 																						// system-2-energometer message 
								{
									union {short w[4];} *e = (void *)q;			
									if(_DBG(p,_DBG_SYS_ENRG)) {
_io 								*io=_stdio(__dbug);
										printf(":%04d %04hX %04hX %04hX %04hX\r\n>",__time__ % 10000,e->w[0],e->w[1],e->w[2],e->w[3]);
										_stdio(io);
									}
								}
								break;
//______________________________________________________________________________________
								case _ID_SYS2EC:
								switch(*q++) {
									case _EC_status_req:
										CanReply("cwwE",_EC_status_req,0,0);
										break;
									case _EC_command:
										CanReply("cwwE",_EC_status_req,0,0);
										break;
									case _EC_RevNum_req:
										n=*(short *)q;
										CanReply("cwwE",_EC_RevNum_req,SW_version,n);
										break;
									case _EC_Ping:
										CanReply("cE",_EC_Ping);
										break;
								}
								break;
//______________________________________________________________________________________
							}
						}
					}
					--inproc;
}
/*______________________________________________________________________________
  * @brief  CAN data transmission reply
  * @param format: message format string
  * @param ....  : list of parameters, acc. to format specifier
  * @retval : None
  */
int				CanReply(char *format, ...) {
static 
_io*			io=NULL;
union			{
						void *v;
						int i;
						char c[8];
					} u;
CanTxMsg	tx={_ID_PFM2SYS,0,CAN_ID_STD,CAN_RTR_DATA,0,0,0,0,0,0,0,0,0};
va_list		v;
char*			c;
int				i;				
					if(!format) {
						io=__stdin.handle.io;
						return(0);
					}
					va_start(v, format);
					while(*format) { 
						u.i=va_arg(v, int);
						c=u.c;
						i=0;
						switch(*format++) {
							case '.': memcpy(&tx,u.v,sizeof(tx));
																									break;
							case 'P':	tx.StdId=_ID_PFM2SYS;			break;
							case 'E':	tx.StdId=_ID_EC2SYS;			break;
							case 'c':	i=sizeof(char);						break;
							case 'w':	i=sizeof(short);					break;
							case 'i':	i=sizeof(int);						break;
						}
						while(i--)			
							tx.Data[tx.DLC++]=*c++;
					}
					va_end(v);	
//______________________________________________________________________________________
					if(io) {
						io=_stdio(io);
						printf("%02X.<<.",tx.StdId);
						for(i=0; i<tx.DLC; ++i)
							printf("%c%02X",'.',tx.Data[i]);
						printf("\r\n>");	
						io=_stdio(io);						
					}

					return _buffer_push(__can->tx,&tx,sizeof(CanTxMsg));
}
/*______________________________________________________________________________
  * @brief  Interprets the PFM command message
  * @param 	pfmcmd: PFM command word as defined in CAN protocol  ICD 
  * @retval : None
  *
  */
#define		kVf	(3.3/4096.0*2000.0/7.5)					// 		flash voltage			
#define		kIf	(3.3/4096.0/2.9999/0.001)				// 		flash curr.
#define 	Ts	 1e-6														// 		ADC sample rate
#define 	kmJ	(int)(0.001/kVf/kIf/Ts+0.5) 		//		mJ, fakt. delitve kum. energije < 1 !!!  0.4865351
	
int				Eack(PFM *p) {

static		long long	e1=0,e2=0;
static		int				n=0;

int				i,j,k;

					if(p) {
						j=k=_I2AD(20.0);
						for(i=0; i<_MAX_BURST/_uS; ++i) {
							if(ADC1_buf[i].I > j)
								e1+=(short)(ADC1_buf[i].U) * (short)(ADC1_buf[i].I-_I1off);
							if(ADC2_buf[i].I > k)
								e2+=(short)(ADC2_buf[i].U) * (short)(ADC2_buf[i].I-_I2off);							
						}
						
						if(n++ == p->Burst.Erpt) {
							int e;
							switch(_STATUS(p,PFM_STAT_SIMM1 | PFM_STAT_SIMM2)) {
								case PFM_STAT_SIMM1:
									e=e1*p->ADCRate/kmJ/_uS;
									CanReply("cicP",_PFM_E_ack,e,0);	
									_DEBUG_MSG("E1=%d.%dJ",e/1000,(e%1000)/100);
									break;
								case PFM_STAT_SIMM2:
									e=e2*p->ADCRate/kmJ/_uS;
									CanReply("cicP",_PFM_E_ack,e,0);					
									_DEBUG_MSG("E2=%d.%dJ",e/1000,(e%1000)/100);
									break;
								case PFM_STAT_SIMM1 | PFM_STAT_SIMM2:
									e=(e1+e2)*p->ADCRate/kmJ/_uS;
									CanReply("cicP",_PFM_E_ack,e,0);		
									_DEBUG_MSG("E1+E2=%d.%dJ",e/1000,(e%1000)/100);
									break;
							}
							e1=e2=n=0;
							return(-1);
						}
					} else
						e1=e2=n=0;
					return(0);
}
/*______________________________________________________________________________
  * @brief  Composes & sends the PFM status message
  * @param  : PFM object p, simmer status k
  * @retval : None
  *
  */
int				PFM_status_send(PFM *p, int k) {
						if(p->Error & (PFM_STAT_SIMM1 | PFM_STAT_SIMM2))
							CanReply("cwiP",_PFM_status_req,
								(p->Status & ~(PFM_STAT_SIMM1 | PFM_STAT_SIMM2)) | k,
								(p->Error & ~(PFM_STAT_SIMM1 | PFM_STAT_SIMM2)) | k
								);
						else
							CanReply("cwiP",_PFM_status_req,
								(p->Status & ~(PFM_STAT_SIMM1 | PFM_STAT_SIMM2)) | k,
								p->Error);		
					return k;
					}
/*______________________________________________________________________________
  * @brief  Interprets the PFM command message
  * @param 	pfmcmd: PFM command word as defined in CAN protocol  ICD 
  * @retval : None
  *
  */
int				PFM_command(PFM *p, int n) {
static		int	count=0,no=0;
					int	Uidle;
//________________________________________________________________________________
					if(p) {
						while(_MODE(p,_PULSE_INPROC))																			// no change during pulse
							Wait(2,App_Loop);
//________________________________________________________________________________
						if(no != n) {																											// simmer status changed ???
							_TRIGGER1_OFF;																									// kill both triggers
							_TRIGGER2_OFF;
							_CLEAR_STATUS(p,PFM_STAT_SIMM1 | PFM_STAT_SIMM2);								// clear status
							SetSimmerPw(p);																									// kill both simmers
							no=n & (PFM_STAT_SIMM1 | PFM_STAT_SIMM2);												// mask filter command
							Wait(100,App_Loop);																							// wait 100 msecs
							
							if(!_MODE(p,_CHANNEL1_DISABLE)) {																// if not Erbium  single channel
								if(_MODE(p,_CHANNEL1_SINGLE_TRIGGER))													// single trigger config.. as from V1.11
									Uidle=2*p->HV/7;
								else
									Uidle=p->HV/7;
								_I1off=ADC1_simmer.I;																					// get current sensor offset
								_U1off=ADC1_simmer.U;																					// check idle voltage
#ifndef __DISCO__
								if(abs(Uidle - ADC3_AVG*ADC1_simmer.U) > _HV2AD(30)) {				// HV +/- 30V range ???
									_SET_ERROR(p,PFM_STAT_UBHIGH);															// if not, PFM_STAT_UBHIGH error 
									no=0;
								}
#endif
							}
							if(!_MODE(p,_CHANNEL2_DISABLE)) {																// same for NdYAG channel
								if(_MODE(p,_CHANNEL2_SINGLE_TRIGGER))													// single trigger config.. as from V1.11
									Uidle=2*p->HV/7;
								else
									Uidle=p->HV/7;
								_I2off=ADC2_simmer.I;
								_U2off=ADC2_simmer.U;
#ifndef __DISCO__
								if(abs(Uidle - ADC3_AVG*ADC2_simmer.U) > _HV2AD(30)) {
									_SET_ERROR(p,PFM_STAT_UBHIGH);
									no=0;
								}
#endif
							}
						}
//________________________________________________________________________________
						if(no && _MODE(p,_CHANNEL1_DISABLE))															// if single channel request
							_SET_STATUS(p, PFM_STAT_SIMM2);																	// modify status
						else if(no && _MODE(p,_CHANNEL2_DISABLE))													// same for other channel
							_SET_STATUS(p, PFM_STAT_SIMM1);
						else
							_SET_STATUS(p, no);																							// else set status as requested
//________________________________________________________________________________
#ifndef __DISCO__
						if(!_STATUS(p,_PFM_CWBAR_STAT))																		// crowbar not cleared
							_SET_ERROR(p,PFM_ERR_PULSEENABLE);
#endif
//________________________________________________________________________________
						if(no & PFM_STAT_SIMM1)																						// activate triggers
							_TRIGGER1_ON;
						if(no & PFM_STAT_SIMM2)
							_TRIGGER2_ON;
						
						SetSimmerRate(p,_PWM_RATE_LO);																		// set simmer
						SetPwmTab(p);
						count=1000;																												// set trigger countdown
//________________________________________________________________________________					
					} else {
						if(count && !--count) {
							_TRIGGER1_OFF;																									// kill triggers after count interval
							_TRIGGER2_OFF;
						}
					}						
					return	no;
}
/*______________________________________________________________________________
  * @brief  Pockels cell driver setup
  * @param 	q == can buffer (short delay, short pw, short n-pulses
	* if q==0 >> call from interrupt
  * @retval : None
  *
  */
int				PFM_pockels(PFM *p) {

					TIM_TimeBaseInitTypeDef		TIM_TimeBaseStructure;
					TIM_OCInitTypeDef					TIM_OCInitStructure;
					GPIO_InitTypeDef					GPIO_InitStructure;
					
					TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
					TIM_OCStructInit(&TIM_OCInitStructure);
					GPIO_StructInit(&GPIO_InitStructure);
					TIM_DeInit(TIM4);
					RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
					
					GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
					GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
					GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
					GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
						
					GPIO_PinAFConfig(GPIOD, GPIO_PinSource12, GPIO_AF_TIM4);
					GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
					GPIO_Init(GPIOD, &GPIO_InitStructure);
															
					TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
					TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
					TIM_OCInitStructure.TIM_Pulse=p->qdelay + 1;
					TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
					TIM_OC1Init(TIM4, &TIM_OCInitStructure);		
					
					TIM_TimeBaseStructure.TIM_Period = TIM_GetCapture1(TIM4) + p->qwidth;
					TIM_TimeBaseStructure.TIM_Prescaler = _uS-1;
					TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
					TIM_TimeBaseInit(TIM4,&TIM_TimeBaseStructure);
					
					TIM_Cmd(TIM4, DISABLE);
					return 0;
}
/**
* @}
*/ 
