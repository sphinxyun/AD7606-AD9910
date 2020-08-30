/*
****************************************************************************
// Created by TerrorBlade on 2020/7/28.

	*�ļ���ad7606.c
	*���ߣ�xzw
	*�������޸��� �����Ƽ�AD7606 stm32f103����
						1����f103�ĳ�f407����
						2������׼���ΪHal������
						3����ҪFPU֧�֣���keil�����FPU����
						4����������FFT���㺯�������Ӷ����Ҳ�������ֵ��Ƶ�ʼ��㹦�ܡ�
	*����޸�ʱ�䣺2020/8/7

****************************************************************************
*/

#include "stm32f4xx.h"
#include <stdio.h>
#include "ad7606.h"
#include "gpio.h"
#include "tim.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "arm_common_tables.h"

FIFO_T	g_tAD;            //����һ�����ݽ���������


/*������*/
static uint32_t sample_freq;                     //������
static float32_t Freq;                            //����������Ƶ��
static float32_t Amplitude;                      //���Ƶ�ʵķ�ֵ
static float32_t DC_Component;                   //ֱ������

/*����ʹ���е������С�Ͳ�������*/
#define sample_lenth 128                         //��������
static float32_t InPutBuffer[sample_lenth];       //������������ InPutBuffer     ���������ⲿ���� 
static float32_t MidBuffer[sample_lenth];         //�����м�����
static float32_t OutPutBuffer[sample_lenth/2];    //����������� OutPutBuffer    ��������һ����̬����
static float32_t FreqBuffer[(sample_lenth/4)-1];       //�������ֱ������֮���Ƶ������

/*�����˲���ʹ�õ��м�����*/
#define Filter_average_num 4                             //ƽ���˲���������
float32_t Mid_Filter_Freq_Buffer[Filter_average_num];    //Ƶ��ƽ���˲�ʱ���м�����


/*����fft�����еĲ���*/
uint32_t fftSize = 64;                        //FFT�������
uint32_t ifftFlag = 0;                          //����Ϊfft�任   ifftFlag=1ʱΪfft���任
uint32_t doBitReverse = 1;                      //��λȡ��

/*������������*/
float32_t maxvalue;                             //�����Ƶ�������ֵ(����ֱ������)
uint32_t Index;                                 //���ֵ��������λ��(����ֱ������)

float32_t Freq_maxvalue;                        //�����Ƶ���ϵ����ֵ(������ֱ������)
uint32_t Freq_Index;                            //���ֵ��������λ��(������ֱ������)

float32_t Virtual_value;                        //��Чֵ
float32_t res;
/*�������ת���ı�־*/
static int fft_complete_flag;                   //����ת����ɱ�־ 


/*
************************************************************
*������ ad7606_init
*���� ��ʼ��ad7606
*�β� ��
*����ֵ ��
************************************************************
*/
void ad7606_init(void)
{
    ad7606_SetOS(0);    //���ù�����ģʽ

    ad7606_Reset();     //��λad7606

    AD_CONVST_HIGH();
}





/*
************************************************************
*������ ad7606_Reset
*���� ��λad7606
*�β� ��
*����ֵ ��
************************************************************
*/
void ad7606_Reset(void)
{
    /* AD7606�ߵ�ƽ��λ����С����Ҫ��50ns */

    AD_RESET_LOW();

    AD_RESET_HIGH();
    AD_RESET_HIGH();
    AD_RESET_HIGH();
    AD_RESET_HIGH();

    AD_RESET_LOW();
}

/*
************************************************************
*������ ad7606_SetOS
*���� ���ù�����ģʽ
*�β� ucMode: 0-6 0��ʾ�޹����� 1��ʾ2�� 2��ʾ4�� 3��ʾ8�� 4��ʾ16��
 5��ʾ32�� 6��ʾ64��
*����ֵ ��
************************************************************
*/
void ad7606_SetOS(uint8_t _ucMode)
{
    if (_ucMode == 1)
    {
        AD_OS2_0();
        AD_OS1_0();
        AD_OS0_1();
    }
    else if (_ucMode == 2)
    {
        AD_OS2_0();
        AD_OS1_1();
        AD_OS0_0();
    }
    else if (_ucMode == 3)
    {
        AD_OS2_0();
        AD_OS1_1();
        AD_OS0_1();
    }
    else if (_ucMode == 4)
    {
        AD_OS2_1();
        AD_OS1_0();
        AD_OS0_0();
    }
    else if (_ucMode == 5)
    {
        AD_OS2_1();
        AD_OS1_0();
        AD_OS0_1();
    }
    else if (_ucMode == 6)
    {
        AD_OS2_1();
        AD_OS1_1();
        AD_OS0_0();
    }
    else
    {
        AD_OS2_0();
        AD_OS1_0();
        AD_OS0_0();
    }
}

/*
************************************************************
*������ ad7606_StartConv
*���� ����adcת��
*�β� ��
*����ֵ ��
************************************************************
*/
void ad7606_StartConv(void)
{
    /* �����ؿ�ʼת�����͵�ƽ���ٳ���25ns  */
    AD_CONVST_LOW();
    AD_CONVST_LOW();
    AD_CONVST_LOW();	/* ����ִ��2�Σ��͵�ƽ����ʱ���ԼΪ50ns */
    AD_CONVST_LOW();
    AD_CONVST_LOW();
    AD_CONVST_LOW();
    AD_CONVST_LOW();
    AD_CONVST_LOW();
    AD_CONVST_LOW();

    AD_CONVST_HIGH();
}

//spiд����

void SPI_SendData(u16 data)
{
    u8 count=0;
    AD_SCK_LOW();	//???��??��DD��
    for(count=0;count<16;count++)
    {
        if(data&0x8000)
            AD_MISO_LOW();
        else
            AD_MISO_HIGH();
        data<<=1;
        AD_SCK_LOW();
        AD_CSK_HIGH();		//��?��y??��DD��
    }
}

//spi������
u16 SPI_ReceiveData(void)
{
    u8 count=0;
    u16 Num=0;
    AD_CSK_HIGH();
    for(count=0;count<16;count++)//?��3?16??��y?Y
    {
        Num<<=1;
        AD_SCK_LOW();	//???��??��DD��
        if(AD_MISO_IN)Num++;
        AD_CSK_HIGH();
    }
    return(Num);
}

/*
************************************************************
*������ ad7606_ReadBytes
*���� ��ȡADC�������
*�β� ��
*����ֵ usData
************************************************************
*/
uint16_t ad7606_ReadBytes(void)
{
    uint16_t usData = 0;

    usData = SPI_ReceiveData();

    /* Return the shifted data */
    return usData;
}

/*
************************************************************
*������ ad7606_IRQSrc
*���� ��ʱ���ú��������ڶ�ȡADCת������
*�β� ��
*����ֵ ��
************************************************************
*/
void ad7606_IRQSrc(void)
{
    uint8_t i;
    uint16_t usReadValue;
		static int j;

    /* ��ȡ����
    ʾ�������ӣ�CS�͵�ƽ����ʱ�� 35us
    */
    AD_CS_LOW();
    for (i = 0; i < CH_NUM; i++)
    {
        usReadValue = ad7606_ReadBytes();
        if (g_tAD.usWrite < FIFO_SIZE)
        {
            g_tAD.usBuf[g_tAD.usWrite] = usReadValue;
            ++g_tAD.usWrite;
        }
    }

    AD_CS_HIGH();
//	g_tAD.usWrite = 0;
    ad7606_StartConv();
		
		
		
		if( j < fftSize )
		{
			
			InPutBuffer[2*j] = ((float)((short)g_tAD.usBuf[0])/32768/2);         //���ݴ�������������ż��λ
			InPutBuffer[2*j+1] = 0;                                              //����λ����
			g_tAD.usWrite = 0;
			j++;
		}
		else if( j == fftSize)
		{
			j = 0;
			if(fft_complete_flag == 0)
			{
				memcpy(MidBuffer,InPutBuffer,sizeof(InPutBuffer));                
				fft_complete_flag = 1;
			}
			else if(fft_complete_flag == 1)
			{
				fft_complete_flag = 1;
			}
			else 
				printf("error");
		}
				
}

/*
************************************************************
*������ ad7606_StartRecord
*���� ��ʼ�ɼ�
*�β� _ulFreq
*����ֵ ��
************************************************************
*/
void ad7606_StartRecord(uint32_t _ulFreq)
{
    //ad7606_Reset();

	  sample_freq = _ulFreq; 
	
    ad7606_StartConv();				/* ???��2��?����?����?a�̨�1������y?Y��?0��??����a */

    g_tAD.usRead = 0;				/* ��?D??��?a??TIM2???��??0 */
    g_tAD.usWrite = 0;


    MX_TIM4_Init(_ulFreq);                 //���ö�ʱ��4Ƶ��
    HAL_TIM_Base_Start_IT(&htim4);         //ʹ�ܶ�ʱ��4�ж�

}

/*
************************************************************
*������ ad7606_StopRecord
*���� ֹͣ�ɼ�
*�β� ��
*����ֵ ��
************************************************************
*/
void ad7606_StopRecord(void)
{
    HAL_TIM_Base_Stop_IT(&htim4);
}

/*
************************************************************
*������ ad7606_get_signal_average_val
*����	��ȡ��ͨ����AD����ƽ��ֵ
*�β� channal ѡ��ͨ����channal��0-7֮�䣬average_num ��ֵ���ĵ�������
*����ֵ int_singal_sample_val
************************************************************
*/
int32_t ad7606_get_signal_average_val(int8_t channal,int8_t average_num)
{
	int i;
	float val = 0;                                                         	//�����ۼӵ��м����
	int32_t int_singal_sample_val;
	for (i=0; i<average_num;i++)
	{
		val = val + ((float)((short)g_tAD.usBuf[channal])/32768/2);           //�ۼ�
	}
	g_tAD.usWrite = 0;
	int_singal_sample_val = ((int32_t)10000)*(val / average_num);           //�õ�ƽ��ֵ
	return int_singal_sample_val;
}

/*
************************************************************
*������ ad7606_get_fft_data
*����	������ڷ���������,������InPutBuffer����������
*�β� ��
*����ֵ ��
************************************************************
*/
void ad7606_get_fft_data()
{
	int i;
	printf("%d",i);
	for (i=0;i<fftSize;i++)                                                   
	{
		InPutBuffer[2*i] = ((float)((short)g_tAD.usBuf[0])/32768/2);              
		InPutBuffer[2*i+1] = 0;
		g_tAD.usWrite = 0;
		HAL_Delay(1);
	}
}

/*
************************************************************
*������ fft_get_maxvalue
*����	�������������FFT���㡣�������
																1��Ƶ�� Freq
																2����ֵ Amplitude
																3��ֱ������ DC_Component
																4�����ھ�������Чֵ��Virtual_value
*�β� ��
*����ֵ ��
************************************************************
*/
void fft_get_maxvalue()
{
	int k;
	
	if(fft_complete_flag == 1)
	{
		arm_cfft_f32(&arm_cfft_sR_f32_len64,MidBuffer,ifftFlag,doBitReverse);      //�������������FFT�任���任��������������������
	
	  arm_cmplx_mag_f32(MidBuffer,OutPutBuffer,fftSize);                         //�Ծ���FFT�任���������ȡģ���㣬�������������OutPutBuffer������
	
	  arm_max_f32(OutPutBuffer,fftSize,&maxvalue,&Index);                        //���������Ƶ��������ֵ�������������е�λ��

		for(k=0;k<(fftSize/2-1);k++)
		{
			FreqBuffer[k] = OutPutBuffer[k+1];                                       //ȡ��������һ�룬����ȥ��ֱ������
		}
		
		arm_max_f32(FreqBuffer,(fftSize/2-1),&Freq_maxvalue,&Freq_Index);          //ȥ��ֱ�����������������Ƶ��������ֵ�������������е�λ��
		
		Freq = (Freq_Index+1)*((float)sample_freq/(float)fftSize);                 //Ƶ�� = (N-1)*Fs/FFTSize        ��λHz
		
		Amplitude = Freq_maxvalue/((float)fftSize/2)*10000;                        //Ƶ�ʷ��� = value/FFTSize/2*10   ��λV
		
		DC_Component = OutPutBuffer[0]/fftSize*10000;                              //ֱ������ = value/FFTSize
		
		Virtual_value = Amplitude/1.4142135;                                       //��Чֵ
		
		res = ((Virtual_value-8)/43.3)/(4-((Virtual_value-8)/43.3))*2000;
		
		//printf("maxvalue = %f \r\n location = %d  \r\n",maxvalue,Index);
		
	  printf("Fmaxvalue = %f \r\n Amplitude = %f  \r\n  DC_Component = %f  \r\n  Virtual_value = %f  \r\n Res = %f  \r\n  ",Freq,Amplitude,DC_Component,Virtual_value,res);
		
		
		fft_complete_flag = 0;                                                     //��־λ��0����ʾת�����
		
	} 
}

/*
************************************************************
*������ filter_fft
*����	��fft������ֵ�����˲�
*�β� ��
*����ֵ ��
************************************************************
*/
float32_t filter_fft()
{
	uint16_t j;
	float32_t sum=0;
	float32_t result=0;
	
	for(j=0;j<Filter_average_num;j++)
	{
		fft_get_maxvalue();
		
		sum = sum+Amplitude;
	}
	
	result = sum/4;
	
	printf("Amplitude = %f",result);
	
	return result;
}




