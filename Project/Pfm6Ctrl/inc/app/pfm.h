/* Includes ------------------------------------------------------------------*/
#ifndef WIN32
#include				"stm32f2xx.h"
#endif
#include				<string.h>
#include				<stdio.h>
#include				<math.h>
#include				"io.h"
#include				"ff.h"
#include				"CAN_MAP.h"

#include				"usbh_core.h"
#include				"usbh_msc_usr.h"
#include				"usbd_usr.h"
#include				"usbd_desc.h"
#include				"usbh_msc_core.h"
#include				"usbd_msc_core.h"
#include				"usbd_cdc_core.h"

#include				"usb_conf.h"
#include				"usbh_core.h"
			          
#define 				SW_version		114						
			          
#ifdef __DISCO__
#define					_uS						42
#define					_mS						(1000*_uS)
#define					_MAX_BURST		(1*_mS)
#else
#define					_uS						60
#define					_mS						(1000*_uS)
#define					_MAX_BURST		(3*_mS)
#endif

#define					_PWM_RATE_HI	(10*_uS)
#define					_MAX_ADC_RATE	(1*_uS)
#define					_FAN_PWM_RATE	(50*_uS)

#if  						defined (__PFM6__)
#define					__CAN__				CAN2
#define					__FILT_BASE__	14
#elif  					defined (__DISCO__)
#define					__CAN__				CAN1
#define					__FILT_BASE__	0		
#elif  					defined (WIN32)
#include				<stdint.h>
#else			
	#### 					error, no HW defined
#endif			 

#define					_UREF					3.3																				// 2.5 stari HW?
#define					_Rdiv(a,b)		((a)/(a+b))
#define					_Rpar(a,b)		((a)*(b)/(a+b))
			          
#define					_R1	7.5e3																								// 5.6e3 stari HW?
#define					_R2	2e6

#define					_AD2HV(a)			((int)(((a)*_UREF)/4096.0/ADC3_AVG/_Rdiv(_R1,_R2)+0.5))
#define					_HV2AD(a)			((int)(((a)*4096.0*ADC3_AVG*_Rdiv(_R1,_R2))/_UREF+0.5))
#define					_I2AD(a)			(((a)*4096)/(int)(3.3/2.9999/0.001+0.5))
#define					_AD2I(a)			(((a)*(int)(3.3/2.9999/0.001+0.5))/4096)

#define					_m5V2AD(a)		((int)(4096+((a)-_UREF)*(_Rdiv(12.0,24.0)*4096.0/_UREF))*8)
#define					_p20V2AD(a)		((int)((a)*8*(_Rdiv(12.0,68.0)*4096.0/_UREF)))

#define					_AD2p20V(a)		((float)(((a/8)*_UREF)/4096.0/_Rdiv(12.0,68.0)))
#define					_AD2m5V(a)		((float)(((a/8-4096)*_UREF)/4096.0/_Rdiv(12.0,24.0)+_UREF))
																
#define					__charger6		__i2c1

void						_led(int, int),
								_lightshow(void);

#define					_RED1(a)			_led(0,a)
#define					_GREEN1(a)		_led(1,a)
#define					_YELLOW1(a)		_led(2,a)
#define					_BLUE1(a)			_led(3,a)
#define					_ORANGE1(a)		_led(4,a)
#define					_RED2(a)			_led(5,a)
#define					_GREEN2(a)		_led(6,a)
#define					_YELLOW2(a)		_led(7,a)
#define					_BLUE2(a)			_led(8,a)
#define					_ORANGE2(a)		_led(9,a)
                 
//________________________________________________________________________
typedef					 enum
{								_TRIGGER,
								_PULSE_ENABLED,
								_PULSE_FINISHED,
								_ADC_FINISHED,
								_FAN_TACHO
} 							_event;

#define					_DBG(p,a)				(p->debug & (1<<(a)))
#define					_SET_DBG(p,a)		 p->debug |= (1<<(a))
#define					_CLEAR_DBG(p,a)	 p->debug &= ~(1<<(a))

typedef					 enum
{								_DBG_CAN_TX,
								_DBG_CAN_RX,
								_DBG_ERR_MSG,
								_DBG_SYS_MSG,
								_DBG_ENRG_SYS=20,
								_DBG_SYS_ENRG,
} 							_debug;

typedef					enum
{								_XLAP_SINGLE,							//0
								_XLAP_DOUBLE,							//1
								_XLAP_QUAD,								//2
								__DUMMY1,									//3
								_SIMULATOR,								//4
								_PULSE_INPROC,						//5
								_LONG_INTERVAL,						//6
								_TRIGGER_PERIODIC,				//7
								__DUMMY2,									//8
								_U_LOOP,									//9
								_P_LOOP,									//10
								_CHANNEL1_DISABLE,				//11
								_CHANNEL2_DISABLE,				//12
								_CHANNEL1_SINGLE_TRIGGER,	//13
								_CHANNEL2_SINGLE_TRIGGER,	//14
								__SWEEPS__=25,
								__ASP__=26,
} 							_mode;


#define					_EVENT(p,a)					(p->events & (1<<(a)))
#define					_SET_EVENT(p,a)			 p->events |= (1<<(a))
#define					_CLEAR_EVENT(p,a)		 p->events &= ~(1<<(a))

#define					_ERROR(p,a)					(p->Error & (a))

#define					_CLEAR_ERROR(p,a)	do {																												\
									if(_DBG(p,_DBG_ERR_MSG) && (p->Error & (a))) {															\
_io 								*io=_stdio(__dbug);																												\
										printf(":%04d error %04X,%04X, clear\r\n>",__time__ % 10000,p->Error,a);	\
										_stdio(io);																																\
									}																																						\
									p->Error &= ~(a);																														\
									} while(0)

#define					_SET_ERROR(p,a)	do {																													\
									if(a & _CRITICAL_ERR_MASK) {																								\
										TIM_CtrlPWMOutputs(TIM1, DISABLE);																				\
										TIM_CtrlPWMOutputs(TIM8, DISABLE);																				\
									}																																						\
									if(_DBG(p,_DBG_ERR_MSG) && !(p->Error & (a))) {															\
_io 								*io=_stdio(__dbug);																												\
										printf(":%04d error %04X,%04X, set\r\n>",__time__ % 10000,p->Error,a);		\
										printf(":%04d stats %04X\r\n>",__time__ % 10000,p->Status);		\
										_stdio(io);																																\
									}																																						\
									p->Error |= (a);																														\
								} while(0)

#define					_STATUS(p,a)				(p->Status & (a))
#define					_SET_STATUS(p,a)		(p->Status |= (a))
#define					_CLEAR_STATUS(p,a)	(p->Status &= ~(a))

#define					_MODE(p,a)					(p->mode & (1<<(a)))
#define					_SET_MODE(p,a)			p->mode |= (1<<(a))
#define					_CLEAR_MODE(p,a)		p->mode &= ~(1<<(a))
//________________________________________________________________________
#define 				ADC3_AVG					4
extern 
int							_ADCRates[];
typedef struct	{	unsigned short	U,I;									} _ADCDMA;
typedef struct	{	unsigned short	IgbtT1,IgbtT2,HV2,HV,
									Up20,Um5;															} _ADC3DMA;
typedef struct	{	unsigned short	n,T1,T2,T3,T4; 				} _TIM18DMA;
typedef struct	{	unsigned short	DAC2,DAC1;						} _DACDMA;
typedef struct	{	unsigned short	addr,speed,ntx,nrx;
									unsigned char		txbuf[4],rxbuf[4];		}_i2c;
typedef struct	{					 short	q0,q1,q2,q3,qref;			}	_QSHAPE;
extern					_QSHAPE shape[8];			
//________________________________________________________________________
int 						readI2C(_i2c *,char *, int),
								writeI2C(_i2c *,char *, int),
								str2hex(char **,int n);
//________________________________________________________________________
#define					_SIGN_PAGE				FLASH_Sector_1
#define					_FLASH_TOP				0x08008000
#define					_BOOT_SECTOR			0x08000000

#define 				_FW_START					((int *)(_FLASH_TOP-16))
#define 				_FW_CRC						((int *)(_FLASH_TOP-20))
#define 				_FW_SIZE					((int *)(_FLASH_TOP-24))
#define 				_SIGN_CRC					((int *)(_FLASH_TOP-28))
#define					_FLASH_BLANK			((int)-1)
//________________________________________________________________________
#ifdef __DISCO__
#define					FATFS_SECTOR			FLASH_Sector_5
#define					FATFS_ADDRESS			0x8020000
#endif

#ifdef __PFM6__
#define					FATFS_SECTOR			FLASH_Sector_6
#define					FATFS_ADDRESS			0x8040000
#endif
//________________________________________________________________________            
extern int			(*USBH_App)(int);
int							USBH_Iap(int);
//________________________________________________________________________
#define					_ID_SYS2PFM				0x20
#define					_ID_SYS2EC				0x23
#define					_ID_PFM2SYS				0x40
#define					_ID_EC2SYS				0x43

#define					_ID_SYS_TRIGG			0x1A
#define					_ID_SYS2PFMcom		0xB0
#define					_ID_PFMcom2SYS		0xB1

#define					_ID_IAP_GO				0xA0
#define					_ID_IAP_ERASE			0xA1
#define					_ID_IAP_ADDRESS		0xA2
#define					_ID_IAP_DWORD			0xA3
#define					_ID_IAP_ACK				0xA4
#define					_ID_IAP_SIGN			0xA5
#define					_ID_IAP_STRING		0xA6
#define					_ID_IAP_PING			0xA7

#define					_ID_SYS2ENRG			0x1f
#define					_ID_ENRG2SYS			0x3f
//________________________________________________________________________
#define					_PFM_status_req		0x00
#define					_PFM_command			0x02
#define					_PFM_set					0x03
#define					_PFM_reset				0x04
#define					_PFM_IgbtTemp_req	0x05
#define					_PFM_simmer_set		0x06
				        
#define					_PFM_IgbtTemp_ack	0x03
#define					_PFM_U_req				0x08
#define					_PFM_E_ack				0x07
				        
#define					_PFM_RevNum_req		0x0A
#define					_PFM_Ping					0x0C
#define					_PFM_Iap					0xAA
//________________________________________________________________________
// add.	Pfm6
#define					_PFM_CurrentLimit	0x10
#define					_PFM_SetPwm				0x11
#define					_PFM_HV_req				0x12
#define					_PFM_SetLoop			0x13

#define 				_PFM_TRIGG				0x71
#define					_PFM_SetHVmode		0x72
#define 				_PFM_POCKELS			0x73
//________________________________________________________________________
#define					_EC_status_req		0x00
#define					_EC_command				0x02
#define					_EC_RevNum_req		0x0A
#define					_EC_Ping					0x0C
//________________________________________________________________________
#define					_CtrlY						0x19
#define					_CtrlZ						0x1A
#define					_Esc							0x1B
#define					_Eof							-1

typedef	enum {
								_SHPMOD_OFF=			0x00,
								_SHPMOD_MAIN=			0x01,
								_SHPMOD_CAL=			0x02,
								_SHPMOD_QSWCH=		0x04,
	
								_SHPMOD_SWEEPS=		0x40,
								_SHPMOD_ASP=			0x80
} ptype;
//________________________________________________________________________
typedef 				struct {
short						Repeat,						//	_PFM_reset command parameters
								N,									
								Length,
								E,							
								U,								//	_PFM_set command parameters
								Time,
								Pmax,	
								Psimm[2],					//	simmer pwm, izracunan iz _PFM_simmer_set
								Pdelay,						//	burst interval	pwm
								Delay,						//	-"-	delay
								Imax,							// not used 
								Isimm,						// not used 
								Idelay,						// not used 
								HVo,							// op. voltage, ADC value x ADC3_AVG	
								Erpt;
int							Timeout,					// sweeps pulse counter timeout
								Count;						// sweeps pulse counter
ptype						Ptype;
} burst;
//________________________________________________________________________
typedef 				struct {
burst						Burst;
int							Error;						
short						Status,	
								HV,								// Cap1+Cap2	ADC value x ADC3_AVG
								HV2,							// Cap1			ADC value x ADC3_AVG								
								Temp,							// Igbt temp,	degrees
								Up20,
								Um5,
								ADCRate;
_event					events;
int							mode,
								debug;	
short						qdelay,qwidth;
} PFM;				  
//________________________________________________________________________
extern					PFM							*pfm;
				        
extern 					_TIM18DMA				TIM18_buf[];
extern 					_ADCDMA					ADC1_buf[], ADC2_buf[],ADC1_simmer,ADC2_simmer;
extern 					_ADC3DMA				ADC3_buf[];
				        
void						App_Init(void),
				        
								SetSimmerRate(PFM *, int),
								SetPwmTab(PFM *),
								SetSimmerPw(PFM *),
								EnableIgbt(void),
								DisableIgbt(void),
								Trigger(PFM *),
								TriggerADC(PFM *);
				        
int							IgbtTemp(void),
								CanReply(char *, ...),
								Eack(PFM *),
								PFM_command(PFM *, int),
								PFM_pockels(PFM *),
								PFM_status_send(PFM *, int),
								LW_SpecOps(PFM *,burst *);
				        
void 						USBD_Storage_Init(void);
				        
#define					_VOUT_MODE			0x20
#define					_VOUT						0x21
#define					_READ_VIN				0x88
#define					_READ_IIN				0x89
#define					_READ_VOUT			0x8B
#define					_READ_PIN				0x97
#define					_VIN_ON					0x35
#define					_VIN_OFF				0x36
#define					_PFC_ON					0x8401
#define					_PFC_OFF				0x0401
#define					_READ_RATE			0xFE3A
#define					_STATUS_WORD 		0x79

typedef enum		{
								FSDRIVE_CPU=0,
								FSDRIVE_USB=1
} _fsdrive;

int							FLASH_Program(uint32_t, uint32_t); 
int							FLASH_Erase(uint32_t);
	
void						ProcessingEvents(PFM *),
								ProcessingCharger(PFM *),
								ProcessingStatus(PFM *);
				        
void						SysTick_init(void),
								Watchdog_init(int),
								Watchdog(void),
								WWDG_init(void),
										
								USBD_Vcp_Init(void),
								Initialize_ADC(void),
								Initialize_DAC(void),
								Reset_I2C(_i2c *),
								Initialize_LED(char *[], int),
								Initialize_TIM(void),
								Initialize_NVIC(void),
								Cfg(_fsdrive, char *);
										
_io 						*Initialize_USART(int),
								*Initialize_CAN(int);
_i2c*						Initialize_I2C(int, int);
				
extern int			fanPmin,fanPmax,fanTL,fanTH;
extern void			(*App_Loop)(void),__App_Loop(void);
void						Wait(int,void (*)(void));

extern _io			*__com0,
								*__com1,
								*__dbug,
								*__can;
								
extern _i2c			*__i2c1,
								*__i2c2;
								
				
extern volatile int		__time__;
			
char						*cgets(int, int);
int							DecodeCom(char *),
								DecodeFs(char *);


void 						ParseCan(PFM *),
								ParseCom(_io *);
								
int							ScopeDumpBinary(_ADCDMA *, int);
							
void						USBHost(void);

int							getHEX(char *, int);
void						putHEX(unsigned int,int);
int							hex2asc(int);
void						putHEX(unsigned int,int);
int							strscan(char *,char *[],int);
int							hex2asc(int);
void						putHEX(unsigned int,int);
int							sLoad(char *);
int							iDump(int *,int);
int							sDump(char *,int);
int							hex2asc(int);
int							asc2hex(int);

#ifndef	__max				
#define __max(a,b)  (((a) > (b)) ? (a) : (b))	
#endif
#ifndef	__min				
#define __min(a,b)  (((a) < (b)) ? (a) : (b))	
#endif

int							__fit(int,const int[],const int[]);
float						__lin2f(short);
short						__f2lin(float, short);
				        
extern					uint32_t	__Vectors[];
extern					int			_PWM_RATE_LO;
extern 					int				Pref1,Pref2;

void						SectorQuery(void);
int 						Defragment(int);
				        
#define 				_TRIGGER1			(!GPIO_ReadOutputDataBit(GPIOD,GPIO_Pin_12))				        
#define 				_TRIGGER2			(!GPIO_ReadOutputDataBit(GPIOD,GPIO_Pin_13))			        
#define 				_TRIGGER1_ON	do {															\
											if(!_TRIGGER1)														\
												_DEBUG_MSG("trigger 1 enabled");				\
												GPIO_ResetBits(GPIOD,GPIO_Pin_12);			\
											} while(0)
#define 				_TRIGGER1_OFF	do {															\
											if(_TRIGGER1)															\
												_DEBUG_MSG("trigger 1 disabled");				\
												GPIO_SetBits(GPIOD,GPIO_Pin_12);	  		\
											} while(0)
#define 				_TRIGGER2_ON	do {															\
											if(!_TRIGGER2)														\
												_DEBUG_MSG("trigger 2 enabled");				\
												GPIO_ResetBits(GPIOD,GPIO_Pin_13);			\
											} while(0)
#define 				_TRIGGER2_OFF	do {															\
											if(_TRIGGER2)															\
												_DEBUG_MSG("trigger 2 disabled");				\
												GPIO_SetBits(GPIOD,GPIO_Pin_13);		  	\
											} while(0)
				        
#define					_PFM_CWBAR_SENSE	(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_14)== Bit_RESET)
#define					_PFM_FAULT_SENSE	(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_8) == Bit_SET)

#define					_CRITICAL_ERR_MASK		(PFM_ERR_DRVERR | PFM_ERR_PULSEENABLE | _PFM_ADCWDG_ERR | PFM_ERR_PSRDYN | PFM_STAT_UBHIGH | _PFM_HV2_ERR)

#define					_PFM_ADCWDG_ERR		0x1000
#define					_PFM_FAN_ERR			0x2000
#define					_PFM_HV2_ERR			0x4000
#define					_PFM_I2C_ERR			0x8000
				        
#define					_PFM_CWBAR_STAT		PFM_ERR_PULSEENABLE
				        
enum	err_parse	{
								_PARSE_OK=0,
								_PARSE_ERR_SYNTAX,
								_PARSE_ERR_ILLEGAL,
								_PARSE_ERR_MISSING,
								_PARSE_ERR_NORESP,
								_PARSE_ERR_OPENFILE,
								_PARSE_ERR_MEM
								};

__inline void dbg1(char *s) {
			if(_DBG(pfm,_DBG_SYS_MSG)) {
				_io *io=_stdio(__dbug);
				printf(":%04d %s\r\n>",__time__ % 10000, s);
				_stdio(io);
			}
}

__inline void dbg2(char *s, int arg1) {
			if(_DBG(pfm,_DBG_SYS_MSG)) {
				_io *io=_stdio(__dbug);
				printf(":%04d ",__time__ % 10000);
				printf(s,arg1);
				printf("\r\n>");
				_stdio(io);
			}
}

__inline void dbg3(char *s, int arg1, int arg2) {
			if(_DBG(pfm,_DBG_SYS_MSG)) {
				_io *io=_stdio(__dbug);
				printf(":%04d ",__time__ % 10000);
				printf(s,arg1,arg2);
				printf("\r\n>");
				_stdio(io);
			}
}


#define	GET_MACRO(_1,_2,_3,NAME,...) NAME
#define	_DEBUG_MSG(...) GET_MACRO(__VA_ARGS__, dbg3, dbg2, dbg1)(__VA_ARGS__)

#define	_k_Er	(20.0*20.0)
#define	_k_Nd	(28.5 * 28.5)
// __________________________________________________________________________________________________________
//											Pref1=_I2AD(p->Burst.U * p->Burst.U) * _HV2AD(p->Burst.U) / (ADC3_AVG *_k_Er * 1000 * 4096);
//											Pref2=_I2AD(p->Burst.U * p->Burst.U) * _HV2AD(p->Burst.U) / (ADC3_AVG *_k_Nd * 1000 * 4096);
