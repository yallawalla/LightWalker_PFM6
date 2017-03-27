#include	"pfm.h"
/**
  ******************************************************************************
  * @file    appmisc.c
  * @author  Fotona d.d.
  * @version V1
  * @date    30-Sept-2013
  * @brief	 Application support 
  *
  */
/** @addtogroup PFM6_Application
* @{
*/
//___________________________________________________________________________
void	Wait(int t,void (*f)(void)) {
int		to=__time__+t;
//			_DEBUG_MSG("waiting %d ms",t);
			while(to > __time__) {
				if(f)
					f();
			}
//			_DEBUG_MSG("... continue");
}
/*******************************************************************************
* Function Name : ScopeDumpBinary
* Description   :
* Input         :
* Output        :
* Return        :
*******************************************************************************/
int		ScopeDumpBinary(_ADCDMA *buf, int count) {

static _ADCDMA	*p=NULL;
static _io			*io=NULL;
static int			m=0;

int		i,j;
union	{
			_ADCDMA adc;
			char 		c[sizeof(_ADCDMA)];
			}	_ADC={0,0};

			if(buf) {														// call from parse !!!
				p=buf;
				m=count;
				io=__stdin.handle.io;
				return 0;
			} else if(io && p && m) {
					io=_stdio(io);
					printf("-! %d,%d\r\n",m,4*pfm->ADCRate);
					io=_stdio(io);
					for(i=0;i<m;++i) {
						_ADC.adc.U += p[i].U;
						_ADC.adc.I += p[i].I;
						if((i%4) == 3) {
							_ADC.adc.U /=4;
							_ADC.adc.I /=4;
							io=_stdio(io);
							for(j=0; j<sizeof(_ADCDMA); ++j)
								printf("%c",_ADC.c[j]);
								io=_stdio(io);
								_ADC.adc.U =0;
								_ADC.adc.I =0;
							}
					}
				}
			return -1;
}
/*******************************************************************************
* Function Name : str2hex
* Description   : pretvorba stringa v hex stevilo
* Input         : **p, pointer na string array (char *), n stevilo znakov
* Output        : char * se poveca za stevilo znakov
* Return        : hex integer
*******************************************************************************/
int		str2hex(char **p,int n) {
char	q[16];
			strncpy(q,*p,n)[n]='\0';
			*p += n;
			return(strtoul(q,NULL,16));
			}
/*******************************************************************************/
/**
  * @brief  temperature interpolation from 	ADC data
  * @param t: ADC readout
  * @retval : temperature (10x deg.C)
  */
const int Ttab[]={											// tabela kontrolnih toèk
			2500,								  						// za interpolacijo tempreature
			5000,								  						// zaradi prec izracuna x 100
			8000,
			12500
			};

const	int Rtab[]={
			(4096.0*_Rdiv(47000.0,22000.0)),	// tabela vhodnih toèk za
			(4096.0*_Rdiv(18045.0,22000.0)),	// interpolacijo (readout iz ADC !!!)
			(4096.0*_Rdiv(6685.6,22000.0)),
			(4096.0*_Rdiv(1936.6,22000.0))
			};
/*******************************************************************************
* Function Name : LW_SpecOps
* Description   : checks compatibility and overrides the inconsistent
*								: input parameters; also recognizes parameter set for
* Input         : special waveform modes (ASP, SWEEPS, SHAPING)
* Output        : 
* Return        :
*******************************************************************************/
int 	LW_SpecOps(PFM *p, burst *mirr) {
	
			p->Burst.Erpt = 0;
// _________________________________________________________________________________________________________
			if(_MODE(p,__ASP__) && mirr->Time == 50 && mirr->Length==1 && mirr->N == 5 && mirr->Ptype == _SHPMOD_MAIN) {
				p->Burst.Ptype |= (_SHPMOD_ASP | _SHPMOD_CAL);	
				p->Burst.Length=mirr->Length*1000;
				p->Burst.Time=mirr->Time;
				p->Burst.N=mirr->N;
				p->Burst.Repeat=mirr->Repeat;
				return -1;
			}
// _________________________________________________________________________________________________________
///			if(_MODE(p,__SWEEPS__) && mirr->Time == 50 && mirr->N == 2 && mirr->Ptype == _SHPMOD_MAIN) {
			if(mirr->Time == 50 && mirr->N == 2 && mirr->Ptype == _SHPMOD_MAIN) {
				p->Burst.Ptype |= _SHPMOD_SWEEPS;
				p->Burst.Length=mirr->Length*10;
				p->Burst.Time=mirr->Time;
				p->Burst.N=mirr->N;
				p->Burst.Repeat=mirr->Repeat;
				return -1;
			}	
// _________________________________________________________________________________________________________
			if(mirr->N==0)
				p->Burst.N=1;
			if(mirr->Length==0)
				p->Burst.Length=_MAX_BURST/_uS;
			else if(mirr->Length > _MAX_BURST/_mS) {
				p->Burst.Repeat = (mirr->Length + (mirr->N)/2) / mirr->N;
				p->Burst.Erpt = mirr->N-1;
				p->Burst.Length=_MAX_BURST/_uS;
				p->Burst.N=1;
			}	else	
				p->Burst.Length=mirr->Length*1000;
// _________________________________________________________________________________________________________
			return -1;
}
// _________________________________________________________________________________________________________
_QSHAPE	shape[8];
extern short user_shape[];			
#define	_K1											(_STATUS(p, PFM_STAT_SIMM1)/1)
#define	_K2											(_STATUS(p, PFM_STAT_SIMM2)/2)
#define	_minmax(x,x1,x2,y1,y2) 	__min(__max(((y2-y1)*(x-x1))/(x2-x1)+y1,y1),y2)
int 		ksweeps=80, nsweeps=0; 																	// sweeps coeff.
/*******************************************************************************
* Function Name : SetPwmTab
* Description   : set the pwm sequence
* Input         : *p, PFM object pointer
* Return        :
*******************************************************************************/
void	SetPwmTab(PFM *p) {
_TIM18DMA	*t=TIM18_buf;

int		i,j,n;
int		to=p->Burst.Time,too=100;
int		Uo=p->Burst.Pmax;
//-------wait for prev to finish ---
			while(_MODE(p,_PULSE_INPROC))
				Wait(2,App_Loop);
//-------user shape check-----------										// 3. koren iz razmerja energij, ajde :)
			if(*(int *)user_shape) {													
				double k=(6.0/7.0)*pow(pow((7.0/6.0)*p->Burst.Pmax,3)/400000.0*p->Burst.Time*p->Burst.N/(*(int *)user_shape),1.0/3.0);		
				for(i=2; user_shape[i]; ++t,++i,++i) {
					t->n=2*user_shape[i]/10-1;
					t->T1=t->T2=_K1*(user_shape[i+1]*k + p->Burst.Pdelay);
					t->T3=t->T4=_K2*(user_shape[i+1]*k + p->Burst.Pdelay);
				}
				p->Burst.Ptype=_SHPMOD_OFF;																// da prepreci default pulze
				p->Burst.Delay=0;																// brez delaya
				p->Burst.Time=100;															// da prepreci regulacijo 
				p->Burst.Length=100;
				p->Burst.N=1;
				to=too=0;
			} 
//-------DELAY----------------------
			for(n=2*((p->Burst.Delay*_uS)/_PWM_RATE_HI)-1; n>0; n -= 255, ++t) {
				t->T1=t->T2=_K1*p->Burst.Pdelay;
				t->T3=t->T4=_K2*p->Burst.Pdelay;
				if(n > 255)
					t->n=255;
				else
					t->n=n;
			};
//
//
//
//-------preludij-------------------
			if(p->Burst.Ptype & 0x02) {
				int	du=0,u=0;

				for(i=0; i<sizeof(shape)/sizeof(_QSHAPE); ++i)
					if(p->Burst.Time==shape[i].qref && shape[i].q0) {
						to=shape[i].q0;
						Uo=(int)(pow((pow(p->Burst.Pmax,3)*p->Burst.N*shape[i].qref/to),1.0/3.0)+0.5);
						if(p->Burst.Ptype & _SHPMOD_MAIN) {
							if(Uo > shape[i].q1)
								Uo = shape[i].q1;
						} else
							shape[i].q1=Uo;
// prePULSE + delay
						for(n=((to*_uS)/_PWM_RATE_HI); n>0; n--,++t) 	{
							du+=(2*Uo-u-2*du)*70/shape[i].q0;
							u+=du*70/shape[i].q0;						
							t[0].T1= t[0].T2=_K1*(p->Burst.Pdelay + du + u*shape[i].q2/100);
							t[0].T3= t[0].T4=_K2*(p->Burst.Pdelay + du + u*shape[i].q2/100);
							t->n=1;
						}
// if Uo < q1 or calibrating prePULSE finish prePULSE & return
						if(Uo < shape[i].q1 || !(p->Burst.Ptype & _SHPMOD_MAIN)) {
							while(du > p->Burst.Pdelay) 	{
								du+=(0-u-2*du)*70/shape[i].q0; 
								u+=du*70/shape[i].q0;
								t[0].T1= t[0].T2=_K1*(p->Burst.Pdelay + du + u*shape[i].q2/100);
								t[0].T3= t[0].T4=_K2*(p->Burst.Pdelay + du + u*shape[i].q2/100);
								t->n=1;
								++t;
								++to;
							}

							for(n=2*((p->Burst.Length*_uS/p->Burst.N-to*_uS)/_PWM_RATE_HI)-1;n>0;n -= 255,++t)	{
								t->T1=t->T2=_K1*p->Burst.Pdelay;
								t->T3=t->T4=_K2*p->Burst.Pdelay;
								if(n > 255)
									t->n=255;
								else
									t->n=n;
							}
//-------end of sequence------------						
							t->T1=t->T2=_K1*p->Burst.Psimm[0];
							t->T3=t->T4=_K2*p->Burst.Psimm[1];
							t->n=1;
							++t;
							t->T1=t->T2=_K1*p->Burst.Psimm[0];
							t->T3=t->T4=_K2*p->Burst.Psimm[1];
							t->n=0;
							return;
						}
// else change parameters & continue to 1.pulse q1=214  345V=295 >> 221           340=291 >> 218									133,109
						to=shape[i].q3;
//					Uo=(int)(pow((pow(p->Burst.Pmax,3)*p->Burst.N*shape[i].qref - pow(shape[i].q1,3)*shape[i].q0)/shape[i].q3  /p->Burst.N,1.0/3.0)+0.5);
						Uo=(int)(pow((pow(p->Burst.Pmax,3)*p->Burst.N*shape[i].qref - pow(shape[i].q1,3)*shape[i].q0)/shape[i].qref/p->Burst.N,1.0/3.0)+0.5);
//
//
//	ASP dodatek, 30.5.2016
						if(p->Burst.Ptype & _SHPMOD_ASP) {
							too=_minmax(Uo,260,550,100,100);
							shape[i].q0=250;
							shape[i].q1=250;
							shape[i].q2=36;
							shape[i].q3=50;
							shape[i].qref=50;
						}
						else
							too=_minmax(Uo,260,550,20,100);
					}						
				}	else
						too=p->Burst.Length/p->Burst.N - to;							// dodatek ups....
			if(p->Burst.Ptype & _SHPMOD_MAIN) {
//-------PULSE----------------------					
				for(j=0; j<p->Burst.N; ++j) {

//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
// SWEEPS ...........................................................
// mode recognized by mode req, pulse time = 50u, burst length = 1ms, no. of pulses = 5, regular call ...
//

					if(p->Burst.Ptype & _SHPMOD_SWEEPS) {
// set distance after 1 pulse
						if(j==0) {
							if(p->Burst.Length < 300 || p->Burst.Length > 600)
								too=10*abs((p->Burst.Count % 60)-30)+250;					
							else
								too=p->Burst.Length - p->Burst.Time;	
						}								
// break the seq. if alternate setup mode and odd pulse; else, compute voltage correction on delta t 
						if(j==1) {
							if(p->Burst.Count > 0 && p->Burst.Count % 30 == 0)
								break;
// nad 600V ni spreminjanja 2 pulza, da ne tresci v 650 plafon !
							if(p->Burst.Pmax < (int)(_PWM_RATE_HI*0.85))		
								Uo += Uo*(too - 550)*ksweeps/300/1000+Uo*nsweeps/1000;
						}
// unconditional break on second pulse 
						if(j==2)
							break;
					}					
//..end  sweeps........................................
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
					for(n=2*((to*_uS + _PWM_RATE_HI/2)/_PWM_RATE_HI)-1; n>0; n -= 255, ++t) {					
						t->T1=t->T2=_K1*(Uo+p->Burst.Pdelay);
						t->T3=t->T4=_K2*(Uo+p->Burst.Pdelay);
						if(n > 255)
							t->n=255;
						else
							t->n=n;
						}
//-------PAUSE----------------------			
					for(n=2*((too*_uS)/_PWM_RATE_HI)-1;n>0;n -= 255,++t)	{
						t->T1=t->T2=_K1*p->Burst.Pdelay;
						t->T3=t->T4=_K2*p->Burst.Pdelay;
						if(n > 255)
							t->n=255;
						else
							t->n=n;
					}
				}
			}
//------- fill seq. till end--------		
			for(n=2*((p->Burst.Length*_uS - p->Burst.N*(to+too)*_uS)/_PWM_RATE_HI)-1;n>0;n -= 255,++t)	{
				t->T1=t->T2=_K1*p->Burst.Pdelay;
				t->T3=t->T4=_K2*p->Burst.Pdelay;
				if(n > 255)
					t->n=255;
				else
					t->n=n;
			}
//-------end of sequence------------						
			t->T1=t->T2=_K1*p->Burst.Psimm[0];
			t->T3=t->T4=_K2*p->Burst.Psimm[1];
			t->n=1;
			++t;
			t->T1=t->T2=_K1*p->Burst.Psimm[0];
			t->T3=t->T4=_K2*p->Burst.Psimm[1];
			t->n=0;
	}
/*______________________________________________________________________________
* Function Name : SetSimmerPw
* Description   : simmer pulse width
* Input         : None
* Output        : None
* Return        : None
*/
void		SetSimmerPw(PFM *p) {
_TIM18DMA	*t;

			if(_MODE(p,_XLAP_SINGLE)) {
				if(_STATUS(p, PFM_STAT_SIMM1))
					TIM8->CCR2=TIM8->CCR1=TIM1->CCR2=TIM1->CCR1=p->Burst.Psimm[0];
				else
					TIM8->CCR2=TIM8->CCR1=TIM1->CCR2=TIM1->CCR1=0;
				if(_STATUS(p, PFM_STAT_SIMM2))
					TIM8->CCR4=TIM8->CCR3=TIM1->CCR4=TIM1->CCR3=p->Burst.Psimm[1];
				else
					TIM8->CCR4=TIM8->CCR3=TIM1->CCR4=TIM1->CCR3=0;
				for(t=TIM18_buf;t->n;++t) {
					t->T2=t->T1;
					t->T4=t->T3;
				};
			} else {
				if(_STATUS(p, PFM_STAT_SIMM1))  {
					TIM8->CCR1=TIM1->CCR1=p->Burst.Psimm[0];
					TIM8->CCR2=TIM1->CCR2=TIM1->ARR-p->Burst.Psimm[0];
				} else {
					TIM8->CCR1=TIM1->CCR1=0;
					TIM8->CCR2=TIM1->CCR2=TIM1->ARR;
				}
				if(_STATUS(p, PFM_STAT_SIMM2))  {
					TIM8->CCR3=TIM1->CCR3=p->Burst.Psimm[1];
					TIM8->CCR4=TIM1->CCR4=TIM1->ARR-p->Burst.Psimm[1];
				} else {
					TIM8->CCR3=TIM1->CCR3=0;
					TIM8->CCR4=TIM1->CCR4=TIM1->ARR;
				}
			for(t=TIM18_buf;t->n;++t) {
				t->T2=TIM1->ARR-t->T1;
				t->T4=TIM1->ARR-t->T3;
			}
	}
}
/*______________________________________________________________________________
*/
void	IncrementSimmerRate(int rate) {
static
int		r=0,flag;
			if(flag == (TIM1->CR1 & TIM_CR1_DIR))
				return;
			flag = (TIM1->CR1 & TIM_CR1_DIR);
			if(!r && !rate)
				return;
			if(rate)
				r=rate;
			else {
				if(TIM1->ARR < r) {
					++TIM1->ARR;
					++TIM8->ARR;
					if(!_MODE(pfm,_XLAP_SINGLE)) {
						++TIM1->CCR2;
						++TIM1->CCR4;
						++TIM8->CCR2;
						++TIM8->CCR4;
					}
				}
			}
}
/*______________________________________________________________________________
* Function Name : SetSimmerRate
* Description   : simmer pulse width
* Input         : None
* Output        : None
* Return        : None
*/
void	SetSimmerRate(PFM *p, int simmrate) {

			_DEBUG_MSG("simmer rate %3d kHz", _mS/simmrate);

			while(!(TIM1->CR1 & TIM_CR1_DIR)) Watchdog();
			while((TIM1->CR1 & TIM_CR1_DIR)) Watchdog();
	
			TIM_CtrlPWMOutputs(TIM1, DISABLE);
			TIM_CtrlPWMOutputs(TIM8, DISABLE);

			TIM_SetCounter(TIM1,0);
			TIM_SetCounter(TIM8,0);

			TIM_Cmd(TIM1,DISABLE);
			TIM_Cmd(TIM8,DISABLE);			
			
			TIM_SetCounter(TIM1,0);
			TIM_SetCounter(TIM8,0);

			TIM_SetCounter(TIM1,simmrate/4);
			if(_MODE(p,_XLAP_QUAD))
				TIM_SetCounter(TIM8,3*simmrate/4);
			else
				TIM_SetCounter(TIM8,simmrate/4);

			TIM_SetAutoreload(TIM1,simmrate);
			TIM_SetAutoreload(TIM8,simmrate);
			SetSimmerPw(p);

			if(_MODE(p,_XLAP_SINGLE)) {
				TIM_OC2PolarityConfig(TIM1, TIM_OCPolarity_High);
				TIM_OC4PolarityConfig(TIM1, TIM_OCPolarity_High);
				TIM_OC2PolarityConfig(TIM8, TIM_OCPolarity_High);
				TIM_OC4PolarityConfig(TIM8, TIM_OCPolarity_High);
			} else {
				TIM_OC2PolarityConfig(TIM1, TIM_OCPolarity_Low);
				TIM_OC4PolarityConfig(TIM1, TIM_OCPolarity_Low);
				TIM_OC2PolarityConfig(TIM8, TIM_OCPolarity_Low);
				TIM_OC4PolarityConfig(TIM8, TIM_OCPolarity_Low);
			}
			
			if(_MODE(p,_PULSE_INPROC)) {
				TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
				TIM_ITConfig(TIM1,TIM_IT_Update,ENABLE);
				TriggerADC(p);
			} else {
				TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
				TIM_ITConfig(TIM1,TIM_IT_Update,DISABLE);
			}			
			
			TIM_CtrlPWMOutputs(TIM1, ENABLE);
			TIM_CtrlPWMOutputs(TIM8, ENABLE);
			TIM_Cmd(TIM1,ENABLE);

//_____________________________________________________________	
// 		if(!(p->Error  & _CRITICAL_ERR_MASK))
// 			EnableIgbt();
}
/*******************************************************************************/
void	EnableIgbt(void) {
			TIM_CtrlPWMOutputs(TIM1, ENABLE);
			TIM_CtrlPWMOutputs(TIM8, ENABLE);		
}
/*******************************************************************************/
void	DisableIgbt(void) {
			TIM_CtrlPWMOutputs(TIM1, DISABLE);
			TIM_CtrlPWMOutputs(TIM8, DISABLE);		
}
/*******************************************************************************/
int		fanPmin=20;
int		fanPmax=95;
int		fanTL=3000;
int		fanTH=4000;
/*******************************************************************************/
int		IgbtTemp(void) {
int		cc,t=__max( __fit(ADC3_buf[0].IgbtT1,Rtab,Ttab),
									__fit(ADC3_buf[0].IgbtT2,Rtab,Ttab));

			if(t<fanTL)
				cc=(_FAN_PWM_RATE*fanPmin)/200;
			else {
				if (t>fanTH)
					cc=(_FAN_PWM_RATE*fanPmax)/200;
				else
					cc=(_FAN_PWM_RATE*(((t-fanTL)*(fanPmax-fanPmin))/(fanTH-fanTL)+fanPmin	))/200;
			}
			cc=__min(_FAN_PWM_RATE/2-5,__max(5,cc));
			if(TIM_GetCapture1(TIM3) < cc)
				TIM_SetCompare1(TIM3,TIM_GetCapture1(TIM3)+1);
			else
				TIM_SetCompare1(TIM3,TIM_GetCapture1(TIM3)-1);
			return(t/100);
}
/*******************************************************************************/
//	ADP1047 linear to float converter_____________________________________________
float	__lin2f(short i) {
		return((i&0x7ff)*pow(2,i>>11));
}
//	ADP1047 float to linear converter_____________________________________________
short	__f2lin(float u, short exp) {
		return ((((int)(u/pow(2,exp>>11)))&0x7ff)  | (exp & 0xf800));
}
/*******************************************************************************/
/**
  * @brief	: fit 2 reda, Bronštajn str. 670
  * @param t:
  * @retval :
  */
int  	__fit(int to, const int t[], const int ft[]) {
int		f3=(ft[3]*(t[0]-to)-ft[0]*(t[3]-to)) / (t[0]-t[3]);
int		f2=(ft[2]*(t[0]-to)-ft[0]*(t[2]-to)) / (t[0]-t[2]);
int		f1=(ft[1]*(t[0]-to)-ft[0]*(t[1]-to)) / (t[0]-t[1]);
			f3=(f3*(t[1]-to) - f1*(t[3]-to)) / (t[1]-t[3]);
			f2=(f2*(t[1]-to)-f1*(t[2]-to)) / (t[1]-t[2]);
			return(f3*(t[2]-to)-f2*(t[3]-to)) / (t[2]-t[3]);
}
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
//_______________________________________________________________________________________________________________________________
__inline int signum(double val) {
			  return((0 < val) - (val < 0));
}
//_____________________________________________________________________________________________
/**
  * @brief  - SWEEPS, adapt coeff.m set new pwm tab, counter increment && timeout setup
	*					- triggered on ENM message if _SHPMOD_SWEEPS flag set and __SWEEPS__ active in cfg.ini
	*					- modifies 1st-2nd pulse ratio, according to distance, energy and burst rate 
  * @param  main PFM object *p, int emj=ENM message energy, 10*mJ
  * @retval NONE
  */
void		Sweep(PFM *p,int emj) {
static	int	offref=0,kref=0;
int			n=abs(p->Burst.Count % 60 - 30);
				if(p->Burst.Ptype & _SHPMOD_SWEEPS) {																		// check mode
					if(p->Burst.Count >= 30) {																						// ignore the first sweep
						if(n % 30 == 0) {																										// on the extremes, do the following...
							if(p->Burst.Length < 300 || p->Burst.Length > 600) {							// skip the distance compensation if fix mode
								if(n==0)																												// otherwise, on the left 
									ksweeps = __max(__min(ksweeps + signum(kref-emj),110),70);		// 		increment K
								else																														// on the right
									ksweeps = __max(__min(ksweeps - signum(kref-emj),110),70);		//		decrement K
							}
							offref=emj;																												// take the offset reference					
							if(_DBG(p,25)) {																									// just debug stuff...
								_io *io=_stdio(__dbug);
								printf(":%04d, %dV,%3.1f, %d, %d\r\n>",n,pfm->Burst.Pmax*_AD2HV(pfm->Burst.HVo)/_PWM_RATE_HI, ((float)offref)/10.0,ksweeps,nsweeps);
								_stdio(io);
							}
						} else {																														// in between
							nsweeps = __max(__min(nsweeps + signum(offref-emj/2),100),-100);	// 		adjust 2nd's offset
						}
					}
					kref=emj/2;																														// take the K reference
					p->Burst.Count++;																											// increment pulse counter
					p->Burst.Timeout = __time__ + 5*p->Burst.Repeat;											// set next burst timeout
					SetPwmTab(p);																													// update pwm table...
				}
}
/**
* @}
*/
