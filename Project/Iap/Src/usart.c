#include		"iap.h"
#include		<stdio.h>
#include 		<ctype.h>


#define			USART1_DR_Base  USART1_BASE+4
#define 		RxBufferSize		512
#define 		TxBufferSize		512

#ifndef __DISCO__
_io					*__com1,*__com3;
//______________________________________________________________________________________
int					__getDMA(_buffer *rx) {
int					i=0;
						if(DMA2_Stream2->NDTR)
							rx->_push=&(rx->_buf[rx->len - DMA2_Stream2->NDTR]);
						else
							rx->_push=rx->_buf;
						
						if(_buffer_pull(rx,&i,1))
							return i;
						else
							return EOF;
}
//______________________________________________________________________________________
int					__putDMA(_buffer *tx, int	c) {
static			int n=0;

						if(DMA2_Stream7->NDTR==0)																					// end of buffer reached
							n=0;																														// reset static index
						if(n == TxBufferSize-1)																						// buffer full ?
							return(EOF);	
						
						DMA2_Stream7->CR &= ~0x00000001;																	// stream 7 off
						while(DMA2_Stream7->CR & 0x00000001);															// wait for HW

						DMA2_Stream7->M0AR=(uint32_t)(&tx->_buf[n-DMA2_Stream7->NDTR]);		// set DMA pointer to new character
						DMA2_Stream7->NDTR++;																							// increase DMA counter
						tx->_buf[n++]=c;																									// enter new character
						
						DMA2->HIFCR = 0x0F400000;																					// clear all flags
						DMA2_Stream7->CR |= 0x00000001;																		// stream 7 on
						return(c);
}
//______________________________________________________________________________________
void 				DMA_Configuration(_io *io)
{
						DMA_InitTypeDef DMA_InitStructure;
						DMA_StructInit(&DMA_InitStructure);
						
						RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

						DMA_DeInit(DMA2_Stream7);
						DMA_InitStructure.DMA_Channel = DMA_Channel_4;
						DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)USART1_DR_Base;
						DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)(io->tx->_buf);
						DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
						DMA_InitStructure.DMA_BufferSize = 0;
						DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;

						DMA_InitStructure.DMA_Priority = DMA_Priority_High;
						DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
						DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
						DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
						DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;

						DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
						DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

						DMA_Init(DMA2_Stream7, &DMA_InitStructure);
						DMA_Cmd(DMA2_Stream7, ENABLE);

						DMA_DeInit(DMA2_Stream2);
						DMA_InitStructure.DMA_Channel = DMA_Channel_4;
						DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)USART1_DR_Base;
						DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)(io->rx->_buf);
						DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
						DMA_InitStructure.DMA_BufferSize = RxBufferSize;
						DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;

						DMA_Init(DMA2_Stream2, &DMA_InitStructure);
						DMA_Cmd(DMA2_Stream2, ENABLE);

						USART_DMACmd(USART1, USART_DMAReq_Rx | USART_DMAReq_Tx, ENABLE);
}
//______________________________________________________________________________________
int					__putISR(_buffer *tx, int	c) {
						if(USART_GetFlagStatus(USART3, USART_FLAG_TXE) != RESET)
							USART_SendData(USART3, c);	
						else {
							_buffer_push(tx,&c,1);
							USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
						}
						return c;
}
//______________________________________________________________________________________
_io*	 			Initialize_USART(void) {
						GPIO_InitTypeDef 				GPIO_InitStructure;

						USART_ClockInitTypeDef  USART_ClockInitStructure;
						USART_InitTypeDef 			USART_InitStructure;

						__com1=_io_init(RxBufferSize,TxBufferSize);
						__com1->put= __putDMA;
						__com1->get= __getDMA;	
						RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
						RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);

						GPIO_StructInit(&GPIO_InitStructure);
						GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
						GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
						GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	
						GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
						GPIO_Init(GPIOA, &GPIO_InitStructure);
						GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);		
						GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);		
												
						DMA_Configuration(__com1);
						
						USART_ClockInitStructure.USART_Clock = USART_Clock_Disable;
						USART_ClockInitStructure.USART_CPOL = USART_CPOL_Low;
						USART_ClockInitStructure.USART_CPHA = USART_CPHA_2Edge;
						USART_ClockInitStructure.USART_LastBit = USART_LastBit_Disable;
						USART_ClockInit(USART1, &USART_ClockInitStructure);
						
						USART_InitStructure.USART_BaudRate = 921600;
						USART_InitStructure.USART_WordLength = USART_WordLength_8b;
						USART_InitStructure.USART_StopBits = USART_StopBits_1;
						USART_InitStructure.USART_Parity = USART_Parity_No;
						USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
						USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
						USART_Init(USART1, &USART_InitStructure);
						USART_Cmd(USART1, ENABLE);
						
#if defined (__IOCV1__) || defined (__IOCV2__) 
						__com3=_io_init(RxBufferSize,TxBufferSize);
						__com3->put= __putISR;
						RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);
						RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);
						GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
						GPIO_Init(GPIOC, &GPIO_InitStructure);
						GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);		
						GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);	
						
						USART_ClockInit(USART3, &USART_ClockInitStructure);
						USART_InitStructure.USART_BaudRate = 57600;
						USART_Init(USART3, &USART_InitStructure);
						USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);	
{						
						NVIC_InitTypeDef NVIC_InitStructure;
						NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
						NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
						NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
						NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
						NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
						NVIC_Init(&NVIC_InitStructure);
						USART_Cmd(USART3, ENABLE);
}
#endif

#if defined (__NUCLEO__) 
						__com3=_io_init(RxBufferSize,TxBufferSize);
						__com3->put= __putISR;
						RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);
						RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);
						GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
						GPIO_Init(GPIOD, &GPIO_InitStructure);
						GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);		
						GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);	

						USART_ClockInit(USART3, &USART_ClockInitStructure);
						USART_InitStructure.USART_BaudRate = 115200;
						USART_Init(USART3, &USART_InitStructure);
						USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);	
{						
						NVIC_InitTypeDef NVIC_InitStructure;
						NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
						NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
						NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
						NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
						NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
						NVIC_Init(&NVIC_InitStructure);
						USART_Cmd(USART3, ENABLE);
}
#endif
						return __com1;
}
/*******************************************************************************
* Function Name  : USART1_IRQHandler
* Description    : This function handles USART3  interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void 				USART3_IRQHandler(void) {
int					i;
						if(USART_GetFlagStatus(USART3, USART_FLAG_RXNE) != RESET) {
							i= USART_ReceiveData(USART3);
							USART_ClearITPendingBit(USART3, USART_IT_RXNE);
							_buffer_push(__com3->rx,&i,1);
						}
						if(USART_GetFlagStatus(USART3, USART_FLAG_TXE) != RESET) {
							USART_ClearITPendingBit(USART3, USART_IT_TXE);
							i=0;																																		// tx ready to send
							if(_buffer_pull(__com3->tx,&i,1))																				// if data available
								USART_SendData(USART3, i);																						// send!
							else																																		// else
								USART_ITConfig(USART3, USART_IT_TXE, DISABLE);												// disable interrupt
						}
}
//_________________________________________________________________________________
int 				fputc(int c, FILE *f) {
						if(__com1)
							__com1->put(__com1->tx,c);
						if(__com3)
						__com3->put(__com3->tx,c);
						return c;
}
int 				fgetc(FILE *f) {
						if(__com1) {
							int i=__com1->get(__com1->rx);
							if(i != EOF)
								return i;
						}
						if(__com3)
							return __com3->get(__com3->rx);
						return EOF;
}
#else
/*******************************************************************************
* Function Name  : IO retarget 
* Description    : This function handles USART1  interrupt request.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
volatile int32_t ITM_RxBuffer=ITM_RXBUFFER_EMPTY;
//_________________________________________________________________________________
int 				fgetc(FILE *f) {
						if(ITM_CheckChar())
							return ITM_ReceiveChar();
						else
							return EOF;
}		
int 				fputc(int c, FILE *f) {
							return	ITM_SendChar(c);
}
#endif
