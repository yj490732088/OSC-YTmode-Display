#include "stm32f10x.h"
#include "ad_da.h"
#include "timer.h"
#include "string.h"
#include "usart.h"


extern const u8 Table_A[];


//输出缓存
u8 OutputData[CutIndex][DA_BUF_LEN][DAFq] = {0};

//图像缓存
u8 GRAM[(150*200)/8];
u8 FreshFlag = 0;

//中间处理值
u16 Buffer_Lin0[CutIndex][Img_H][2];

//线填充buff
void Lin_Buf(u8 offset){
	u16 i,j,k;
	for(k=0;k<CutIndex;k++)
		for(i=0;i<Img_H;i++)
			for(j=0;j<DAFq;j++)
				OutputData[k][2+offset+i][j] = Buffer_Lin0[k][i][j%2]*0xFF/150;	//抖动
}

//图像转换为两条线
void Img_Lin(u8* pData){
	u16 i,j,k;
	
	for(k=0;k<CutIndex;k++){
		
		if(k<(CutIndex/2))j = Img_V;
		else j = 0;
		for(i=0;i<Img_H;i++){
			Buffer_Lin0[k][i][0] = j;//(5-ch)*25+12;
			Buffer_Lin0[k][i][1] = j;//(5-ch)*25+12;
		}
		
		for(j=0;j<Img_H;j++){
			for(i=0;i<CutLen;i++){	//纵向10
				if((pData[(j/8)*Img_V+k*CutLen+i]>>(7-(j%8)))&1){
					Buffer_Lin0[k][j][0] = (CutIndex-1-k)*CutLen+CutLen-i-1;
					break;
				}
			}
			if(i<CutLen){
				for(i=0;i<CutLen;i++){
					if((pData[(j/8)*Img_V+k*CutLen+CutLen-1-i]>>(7-(j%8)))&1){
						Buffer_Lin0[k][j][1] = (CutIndex-1-k)*CutLen+i;
						break;
					}
				}
			}
		}
		
	}
	
	Lin_Buf(0);
}


void Delay(u32 i){
	while(i--);
}

int main(void){
	u32 i;
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  
 	TIM3_Int_Init(10000-1,72-1);
	uart_init(1500000);
	
	//DAC-DMA
	Dac1_Init();
 	Wave_DMA_Config();
 	Wave_TIM_Config(32);
	
	//Trig
	for(i=0;i<CutIndex;i++)
		memset(OutputData[i],0xFF,DAFq);
	
	Img_Lin((u8*)Table_A);
	
	
	while(1){
		if(FreshFlag){
			FreshFlag = 0;
			Img_Lin(GRAM);
		}
		
	}
	
}



u8 DispStep;

void DMA2_Channel3_IRQHandler(void){	//DMA结束
	if(DMA_GetITStatus(DMA2_IT_TC3)){
		DMA_ClearITPendingBit(DMA2_IT_GL3);
		GPIO_SetBits(GPIOA,GPIO_Pin_8);//PA8 可以作为低端示波器的触发源
		DMA_Cmd(DMA2_Channel3,DISABLE);
		
		if(DispStep>=CutIndex)DispStep = 0;
		DMA2_Channel3->CMAR = (uint32_t)OutputData[DispStep];
		DispStep++;
		
		DMA2_Channel3->CNDTR = DA_BUF_LEN*DAFq;
		DMA_Cmd(DMA2_Channel3,ENABLE);
		GPIO_ResetBits(GPIOA,GPIO_Pin_8);
	}
}


void TIM3_IRQHandler(void){ 
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) == SET){
		TIM_ClearITPendingBit(TIM3,TIM_IT_Update);
		FUCK_STC;
	}
}



//To send AA
void USART1_IRQHandler(void){                
	u8 Res;
	static u32 Addr = 0;
	static u8 Flag = 0;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET){
		Res = USART_ReceiveData(USART1);
		if(Flag){
			GRAM[Addr++] = Res;
			if(Addr>=sizeof(GRAM)){
				FreshFlag = 1;
				Flag = 0;
			}
		}
		if(Flag == 0 && Res == 0xAA){
			Flag = 1;
			Addr = 0;
		}
	}
}

