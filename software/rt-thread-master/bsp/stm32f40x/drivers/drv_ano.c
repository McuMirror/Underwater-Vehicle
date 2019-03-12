/*
 * File      : drv_ano.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2019 JMU Electronic Technology Association Team 
 *
 * Change Logs:
 * Date         Author          Notes
 * 2019.3				zengwangfa      ��������վ(ANO_TC)ͨ��Э����ֲv1.0
 * 
 * Attention: ����ֻ��Ҫ���� -> ANO_DT_Data_Receive_Prepare(u8 data);
 *��ͳһ�ӿڡ�����ֻ��Ҫ���� -> ANO_SEND_StateMachine(void);
 *            ������������ -> void Save_Or_Reset_PID_Parameter(void);
 */
#include "drv_ano.h"
#include "sys.h"
#include "PID.h"
#include "led.h"
#include "flash.h"
#include "gyroscope.h"
#include <rtdevice.h>

/*---------------------- Constant / Macro Definitions -----------------------*/		

#define BYTE0(dwTemp)       ( *( (char *)(&dwTemp) + 0) )
#define BYTE1(dwTemp)       ( *( (char *)(&dwTemp) + 1) )
#define BYTE2(dwTemp)       ( *( (char *)(&dwTemp) + 2) )
#define BYTE3(dwTemp)       ( *( (char *)(&dwTemp) + 3) )
	


#define HardwareType 	0.00  //Ӳ������  00Ϊ����Ӳ���汾
#define HardwareVER 	2.00  //Ӳ���汾
#define SoftwareVER 	3.10  //�����汾  3��10��
#define ProtocolVER 	1  		//Э��汾
#define BootloaderVER 1  		//Bootloader�汾
/*----------------------- Variable Declarations. -----------------------------*/

extern rt_device_t debug_uart_device;	
extern float  volatge;

u8 data_to_send[50];//ANO����վ�������ݻ���
u8 ANO_Send_PID_Flag[6]={0};//PID���ͱ�־λ
u16 Percontroller_Databuf[8]={0};//�Զ���ң�������ݻ���

Vector3f_pid PID_Parameter[PID_USE_NUM]={0};
u8 Sort_PID_Cnt=0;
u8 Sort_PID_Flag=0; //PID״̬��־λ��1��FLASH  2��λԭʼ����


//---------------�����Ƿָ�����������������������<����Ϊ���� ����>�������������������������Ƿָ���------------------//
/*******************************************
* �� �� ����ANO_DT_Data_Receive_Prepare
* ��    �ܣ�ANO����վ���ݽ���
* �������������ѭ�������һ������data
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_DT_Data_Receive_Prepare(u8 data)//ANO����վ���ݽ���
{
		static u8 RxBuffer[50];
		static u8 _data_len = 0,_data_cnt = 0;
		static u8 state = 0;
		if(state==0&&data==0xAA)//֡ͷ1
		{
				state=1;
				RxBuffer[0]=data;
		}
		else if(state==1&&data==0xAF)//֡ͷ2
		{
				state=2;
				RxBuffer[1]=data;
		}
		else if(state==2&&data<0XF1)//�����ֽ�
		{
				state=3;
				RxBuffer[2]=data;
		}
		else if(state==3&&data<50)//��Ч���ݳ���
		{
				state = 4;
				RxBuffer[3]=data;
				_data_len = data;
				_data_cnt = 0;
		}
		else if(state==4&&_data_len>0)//���ݽ���
		{
				_data_len--;
				RxBuffer[4+_data_cnt++]=data;
				if(_data_len==0)
					state = 5;
		}
		else if(state==5)//У���
		{
				state = 0;
				RxBuffer[4+_data_cnt]=data;
				ANO_DT_Data_Receive_Anl(RxBuffer,_data_cnt+5);//���ݽ���
		}
		else state = 0;
}



/*******************************************
* �� �� ����ANO_DT_Send_Check
* ��    �ܣ�ANO����վ��У��
* ���������head ������
						check_num У���
* �� �� ֵ��none
* ע    �⣺none
********************************************/
static void ANO_DT_Send_Check(u8 head, u8 check_sum)
{
		u8 sum = 0,i=0;
		data_to_send[0]=0xAA;
		data_to_send[1]=0xAA;
		data_to_send[2]=0xEF;
		data_to_send[3]=2;
		data_to_send[4]=head;
		data_to_send[5]=check_sum;
		for(i=0;i<6;i++){
				sum += data_to_send[i];
		}
		data_to_send[6]=sum;
		rt_device_write(debug_uart_device, 0,data_to_send, 7);    //���ͺ�����

}

/*******************************************
* �� �� ����ANO_DT_Data_Receive_Anl
* ��    �ܣ�ANO���ݽ���
* ���������data_buf: ���ݰ�
						num: ���ݰ���С
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_DT_Data_Receive_Anl(u8 *data_buf,u8 num)
{
		u8 sum = 0,i=0;
		for(i=0;i<(num-1);i++)  sum += *(data_buf+i);
		if(!(sum==*(data_buf+num-1)))    return; //�ж�sum
		if(!(*(data_buf)==0xAA && *(data_buf+1)==0xAF))     return;//�ж�֡ͷ
		if(*(data_buf+2)==0x01)
		{
				if(*(data_buf+4)==0x01) {;}
				if(*(data_buf+4)==0x02) {;}
				if(*(data_buf+4)==0x03) {;}
		}
		
		if(*(data_buf+2)==0x02)
		{
				if(*(data_buf+4)==0x01)		//��ȡ��ǰPID����
				{
						ANO_Send_PID_Flag[0]=1;
						ANO_Send_PID_Flag[1]=1;
						ANO_Send_PID_Flag[2]=1;
						ANO_Send_PID_Flag[3]=1;
						ANO_Send_PID_Flag[4]=1;
						ANO_Send_PID_Flag[5]=1;
				
						Bling_Set(&Light_2,300,50,0.5,0,0,69,0);
						Bling_Set(&Light_3,300,100,0.5,0,0,70,0);
				}
				if(*(data_buf+4)==0x02)//��ȡ����ģʽ��������
				{
						;
				}
				if(*(data_buf+4)==0xA0)//��ȡ��λ���汾��Ϣ
				{
						;
				}
				if(*(data_buf+4)==0xA1)//�ָ�Ĭ�ϲ���
				{
						Sort_PID_Flag = 2;	
						Bling_Set(&Light_1,300,100,0.5,0,0,68,0);
						Bling_Set(&Light_2,300,50,0.5,0,0,69,0);
				}
		}
		
		if(*(data_buf+2)==0x10)                             //����PID1
		{
				Total_Controller.Roll_Gyro_Control.Kp  = 0.001*( (vs16)(*(data_buf+4)<<8)|*(data_buf+5) );
				Total_Controller.Roll_Gyro_Control.Ki  = 0.001*( (vs16)(*(data_buf+6)<<8)|*(data_buf+7) );
				Total_Controller.Roll_Gyro_Control.Kd  = 0.01 *( (vs16)(*(data_buf+8)<<8)|*(data_buf+9) );
				Total_Controller.Pitch_Gyro_Control.Kp = 0.001*( (vs16)(*(data_buf+10)<<8)|*(data_buf+11) );
				Total_Controller.Pitch_Gyro_Control.Ki = 0.001*( (vs16)(*(data_buf+12)<<8)|*(data_buf+13) );
				Total_Controller.Pitch_Gyro_Control.Kd = 0.01 *( (vs16)(*(data_buf+14)<<8)|*(data_buf+15) );
				Total_Controller.Yaw_Gyro_Control.Kp   = 0.001*( (vs16)(*(data_buf+16)<<8)|*(data_buf+17) );
				Total_Controller.Yaw_Gyro_Control.Ki   = 0.001*( (vs16)(*(data_buf+18)<<8)|*(data_buf+19) );
				Total_Controller.Yaw_Gyro_Control.Kd   = 0.01 *( (vs16)(*(data_buf+20)<<8)|*(data_buf+21) );
				ANO_DT_Send_Check(*(data_buf+2),sum);
		}
		if(*(data_buf+2)==0x11)                             //����PID2
		{
				Total_Controller.Roll_Angle_Control.Kp  = 0.001*( (vs16)(*(data_buf+4)<<8)|*(data_buf+5) );
				Total_Controller.Roll_Angle_Control.Ki  = 0.001*( (vs16)(*(data_buf+6)<<8)|*(data_buf+7) );
				Total_Controller.Roll_Angle_Control.Kd  = 0.01 *( (vs16)(*(data_buf+8)<<8)|*(data_buf+9) );
				Total_Controller.Pitch_Angle_Control.Kp = 0.001*( (vs16)(*(data_buf+10)<<8)|*(data_buf+11) );
				Total_Controller.Pitch_Angle_Control.Ki = 0.001*( (vs16)(*(data_buf+12)<<8)|*(data_buf+13) );
				Total_Controller.Pitch_Angle_Control.Kd = 0.01 *( (vs16)(*(data_buf+14)<<8)|*(data_buf+15) );
				Total_Controller.Yaw_Angle_Control.Kp   = 0.001*( (vs16)(*(data_buf+16)<<8)|*(data_buf+17) );
				Total_Controller.Yaw_Angle_Control.Ki   = 0.001*( (vs16)(*(data_buf+18)<<8)|*(data_buf+19) );
				Total_Controller.Yaw_Angle_Control.Kd   = 0.01 *( (vs16)(*(data_buf+20)<<8)|*(data_buf+21) );
				ANO_DT_Send_Check(*(data_buf+2),sum); 
		}
		if(*(data_buf+2)==0x12)                             //����PID3
		{
				Total_Controller.High_Speed_Control.Kp    = 0.001*( (vs16)(*(data_buf+4)<<8)|*(data_buf+5) );
				Total_Controller.High_Speed_Control.Ki    = 0.001*( (vs16)(*(data_buf+6)<<8)|*(data_buf+7) );
				Total_Controller.High_Speed_Control.Kd    = 0.01 *( (vs16)(*(data_buf+8)<<8)|*(data_buf+9) );
				Total_Controller.High_Position_Control.Kp = 0.001*( (vs16)(*(data_buf+10)<<8)|*(data_buf+11) );
				Total_Controller.High_Position_Control.Ki = 0.001*( (vs16)(*(data_buf+12)<<8)|*(data_buf+13) );
				Total_Controller.High_Position_Control.Kd = 0.01 *( (vs16)(*(data_buf+14)<<8)|*(data_buf+15) );
				ANO_DT_Send_Check(*(data_buf+2),sum); 
		}
		if(*(data_buf+2)==0x13)                             //����PID4
		{
				ANO_DT_Send_Check(*(data_buf+2),sum);
		}
		if(*(data_buf+2)==0x14)                             //����PID5
		{
	 
				ANO_DT_Send_Check(*(data_buf+2),sum);
		}
		if(*(data_buf+2)==0x15)                             //����PID6
		{
				ANO_DT_Send_Check(*(data_buf+2),sum);
				Sort_PID_Cnt++;
				Sort_PID_Flag=1;
				Bling_Set(&Light_1,300,50,0.5,0,0,68,0);
				Bling_Set(&Light_3,300,100,0.5,0,0,70,0);
		}
}

//---------------�����Ƿָ�����������������������<����Ϊ���� �Ӻ���>�������������������������Ƿָ���------------------//
/*******************************************
* �� �� ����ANO_Data_Send_Version
* ��    �ܣ����ͻ����汾��Ϣ��Ӳ�����ࡢӲ����������Э�顢Bootloader�汾������һ�顿
* ���������none
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_Data_Send_Version(void)//���ͻ�����Ϣ����̬������״̬��
{
		u8 _cnt=0;
		vs16 _temp;
		vs32 _temp2;
		u8 sum = 0;
		u8 i = 0;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0x00;
		data_to_send[_cnt++]=0;
		
		_temp = HardwareType;
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int)(HardwareVER*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int)(SoftwareVER*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		
		_temp2 = (int)(ProtocolVER*100);//��λcm
		data_to_send[_cnt++]=BYTE1(_temp2);
		data_to_send[_cnt++]=BYTE0(_temp2);
		
		_temp2 = (int)(BootloaderVER*100);//��λcm
		data_to_send[_cnt++]=BYTE1(_temp2);
		data_to_send[_cnt++]=BYTE0(_temp2);

		
		data_to_send[3] = _cnt-4;
		sum = 0;
		for(i=0;i<_cnt;i++){
				sum += data_to_send[i];
		}
		data_to_send[_cnt++]=sum;
		rt_device_write(debug_uart_device, 0,data_to_send, _cnt);    //���ͺ�����
}


/*******************************************
* �� �� ����ANO_Data_Send_Status
* ��    �ܣ����ͻ�����Ϣ��ŷ�����ǡ��߶ȡ�����״̬������һ�顿
* ���������none
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_Data_Send_Status(void)//���ͻ�����Ϣ����̬������״̬��
{
		u8 _cnt=0;
		vs16 _temp;
		vs32 _temp2;
		u8 sum = 0;
		u8 i;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0x01;
		data_to_send[_cnt++]=0;
		
		_temp = (int)(JY901.Euler.Roll*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int)(JY901.Euler.Pitch*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int)(-JY901.Euler.Yaw*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		
		_temp2 = 0;//��λcm
		data_to_send[_cnt++]=BYTE3(_temp2);
		data_to_send[_cnt++]=BYTE2(_temp2);
		data_to_send[_cnt++]=BYTE1(_temp2);
		data_to_send[_cnt++]=BYTE0(_temp2);
		
		data_to_send[_cnt++]=0x01;//����ģʽ
		data_to_send[_cnt++]=1;//����0������1
		
		data_to_send[3] = _cnt-4;
		sum = 0;
		for(i=0;i<_cnt;i++){
				sum += data_to_send[i];
		}
		data_to_send[_cnt++]=sum;
		rt_device_write(debug_uart_device, 0,data_to_send, _cnt);    //���ͺ�����
}

/*******************************************
* �� �� ����ANO_DT_Send_Senser
* ��    �ܣ����ʹ�����ԭʼ������ (���ٶȡ����ٶȡ��ų�)  �������顿
* ���������3������x,y,z��
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_DT_Send_Senser(float a_x,float a_y,float a_z,float g_x,float g_y,float g_z,float m_x,float m_y,float m_z)
{
		u8 _cnt=0;
		vs16 _temp;
		u8 sum = 0;
		u8 i = 0;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0x02;
		data_to_send[_cnt++]=0;
		
		_temp = (int)(a_x*100);   
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int)(a_y*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int)(a_z*100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		
		_temp = g_x;
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = g_y;
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = g_z;
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		
		_temp = m_x;
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = m_y;
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = m_z;
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		
		data_to_send[3] = _cnt-4;
		
		sum = 0;
		for(i=0;i<_cnt;i++){
				sum += data_to_send[i];
		}
		data_to_send[_cnt++] = sum;
		
		rt_device_write(debug_uart_device, 0,data_to_send, _cnt);    //���ͺ�����
}

/*******************************************
* �� �� ����ANO_DT_Send_PID
* ��    �ܣ�����PID����
* ���������group:�ڼ���PID����
						3��PID����p,i,d
* �� �� ֵ��none
* ע    �⣺��һ��PID���ݣ�group=1;
********************************************/
void ANO_DT_Send_PID(u8 group,float p1_p,float p1_i,float p1_d,float p2_p,float p2_i,float p2_d,float p3_p,float p3_i,float p3_d)
{
		u8 _cnt=0;
		u8 sum = 0,i=0;
		int16_t _temp;
		
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0x10+group-1;
		data_to_send[_cnt++]=0;
			
		_temp = (int16_t)(p1_p * 1000);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p1_i  * 1000);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p1_d  * 100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p2_p  * 1000);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p2_i  * 1000);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p2_d * 100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p3_p  * 1000);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p3_i  * 1000);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		_temp = (int16_t)(p3_d * 100);
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
		
		data_to_send[3] = _cnt-4;
		
		for(i=0;i<_cnt;i++){
			sum += data_to_send[i];
		}
		
		data_to_send[_cnt++]=sum;
		rt_device_write(debug_uart_device, 0,data_to_send, _cnt);    //���ͺ�����
}

/*******************************************
* �� �� ����ANO_DT_Send_RCData
* ��    �ܣ�����PID����
* ���������group:�ڼ���PID����
						3��PID����p,i,d
* �� �� ֵ��none
* ע    �⣺��һ��PID���ݣ�group=1;
********************************************/
void ANO_DT_Send_RCData(u16 thr,u16 yaw,u16 rol,u16 pit,u16 aux1,u16 aux2,u16 aux3,u16 aux4,u16 aux5,u16 aux6)//����ң����ͨ������
{
		u8 _cnt=0;
		u8 i=0;
		u8 sum = 0;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0x03;
		data_to_send[_cnt++]=0;
		data_to_send[_cnt++]=BYTE1(thr);
		data_to_send[_cnt++]=BYTE0(thr);
		data_to_send[_cnt++]=BYTE1(yaw);
		data_to_send[_cnt++]=BYTE0(yaw);
		data_to_send[_cnt++]=BYTE1(rol);
		data_to_send[_cnt++]=BYTE0(rol);
		data_to_send[_cnt++]=BYTE1(pit);
		data_to_send[_cnt++]=BYTE0(pit);
		data_to_send[_cnt++]=BYTE1(aux1);
		data_to_send[_cnt++]=BYTE0(aux1);
		data_to_send[_cnt++]=BYTE1(aux2);
		data_to_send[_cnt++]=BYTE0(aux2);
		data_to_send[_cnt++]=BYTE1(aux3);
		data_to_send[_cnt++]=BYTE0(aux3);
		data_to_send[_cnt++]=BYTE1(aux4);
		data_to_send[_cnt++]=BYTE0(aux4);
		data_to_send[_cnt++]=BYTE1(aux5);
		data_to_send[_cnt++]=BYTE0(aux5);
		data_to_send[_cnt++]=BYTE1(aux6);
		data_to_send[_cnt++]=BYTE0(aux6);
		
		data_to_send[3] = _cnt-4;	
		sum = 0;
		for(i=0;i<_cnt;i++){
				sum += data_to_send[i];
		}
		data_to_send[_cnt++]=sum;
		rt_device_write(debug_uart_device, 0,data_to_send, _cnt);    //���ͺ�����
}

/*******************************************
* �� �� ����ANO_Data_Send_Voltage_Current
* ��    �ܣ����͵�ѹ����������5�顿
* ���������none
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_Data_Send_Voltage_Current(void)
{
		u8 _cnt=0;
		vs16 _temp;
		vs32 _temp2;
		u8 sum = 0;
		u8 i = 0;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0xAA;
		data_to_send[_cnt++]=0x05;
		data_to_send[_cnt++]=0;
		
		_temp = (int)(volatge*100);      //��ѹ
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
	
		_temp = (int)(0.5*100); //����
		data_to_send[_cnt++]=BYTE1(_temp);
		data_to_send[_cnt++]=BYTE0(_temp);
			
		data_to_send[3] = _cnt-4;
		sum = 0;
		for(i=0;i<_cnt;i++){
				sum += data_to_send[i];
		}
		data_to_send[_cnt++]=sum;
		rt_device_write(debug_uart_device, 0,data_to_send, _cnt);    //���ͺ�����
}




//---------------�����Ƿָ�����������������������<����Ϊ���� �Ӻ���>�������������������������Ƿָ���------------------//
/*******************************************
* �� �� ����ANO_SEND_StateMachine
* ��    �ܣ���������ѭ������
* ���������none
* �� �� ֵ��none
* ע    �⣺none
********************************************/
void ANO_SEND_StateMachine(void)
{
		static u16 ANO_Cnt = 0;


		if(ANO_Cnt == 0){
				ANO_Data_Send_Version();
		}
		ANO_Cnt++;
		
		if(ANO_Cnt == 1){		
				ANO_Data_Send_Status();
		}
	
		else if(ANO_Cnt == 2){
				ANO_DT_Send_Senser(JY901.Acc.x,JY901.Acc.y,JY901.Acc.z,
													 JY901.Gyro.x,JY901.Gyro.y,JY901.Gyro.z,
													 JY901.Mag.x,JY901.Mag.y,JY901.Mag.z);
		}
		
		else if(ANO_Cnt==3)
		{

				ANO_DT_Send_RCData(Percontroller_Databuf[2],Percontroller_Databuf[3],
													 Percontroller_Databuf[0],Percontroller_Databuf[1],
													 Percontroller_Databuf[4],Percontroller_Databuf[5],
													 Percontroller_Databuf[6],Percontroller_Databuf[7],0,0);

		}
		else if(ANO_Cnt == 4){
				ANO_Data_Send_Voltage_Current();
		}
		
		else if(ANO_Cnt == 5
          &&ANO_Send_PID_Flag[0] == 0
            &&ANO_Send_PID_Flag[1] == 0
              &&ANO_Send_PID_Flag[2] == 0
                &&ANO_Send_PID_Flag[3] == 0
                  &&ANO_Send_PID_Flag[4] == 0
                    &&ANO_Send_PID_Flag[5] == 0)//��ǰ��ֹ���Ͷ���
		{
				ANO_Cnt=0;
		}
	

		else if(ANO_Cnt == 6 && ANO_Send_PID_Flag[0] == 1){//��1֡ PID
				ANO_DT_Send_PID(1,Total_Controller.Roll_Gyro_Control.Kp, 
												Total_Controller.Roll_Gyro_Control.Ki,
												Total_Controller.Roll_Gyro_Control.Kd,
												Total_Controller.Pitch_Gyro_Control.Kp,
												Total_Controller.Pitch_Gyro_Control.Ki,
												Total_Controller.Pitch_Gyro_Control.Kd,
												Total_Controller.Yaw_Gyro_Control.Kp,
												Total_Controller.Yaw_Gyro_Control.Ki,
												Total_Controller.Yaw_Gyro_Control.Kd);
				ANO_Send_PID_Flag[0]=0;
		}
		
		else if(ANO_Cnt == 7 && ANO_Send_PID_Flag[1] == 1)//��2֡ PID
		{
				ANO_DT_Send_PID(2,Total_Controller.Roll_Angle_Control.Kp,
												Total_Controller.Roll_Angle_Control.Ki,
												Total_Controller.Roll_Angle_Control.Kd,
												Total_Controller.Pitch_Angle_Control.Kp,
												Total_Controller.Pitch_Angle_Control.Ki,
												Total_Controller.Pitch_Angle_Control.Kd,
												Total_Controller.Yaw_Angle_Control.Kp,
												Total_Controller.Yaw_Angle_Control.Ki,
												Total_Controller.Yaw_Angle_Control.Kd);
				ANO_Send_PID_Flag[1]=0;
		}
		
		else if(ANO_Cnt == 8 && ANO_Send_PID_Flag[2] == 1)//��3֡ PID
		{
				ANO_DT_Send_PID(3,Total_Controller.High_Speed_Control.Kp,
												Total_Controller.High_Speed_Control.Ki,
												Total_Controller.High_Speed_Control.Kd,
												Total_Controller.High_Position_Control.Kp,
												Total_Controller.High_Position_Control.Ki,
												Total_Controller.High_Position_Control.Kd,
												0,
												0,
												0);
				ANO_Send_PID_Flag[2]=0;
		}
		
		else if(ANO_Cnt == 9 && ANO_Send_PID_Flag[3] == 1)//��4֡ PID
		{
				ANO_DT_Send_PID(4,
												0,
												0,
												0,
												0,
												0,
												0,
												0,
												0,
												0);
				ANO_Send_PID_Flag[3]=0;
		}
		else if(ANO_Cnt == 10 && ANO_Send_PID_Flag[4] == 1)//��5֡ PID
		{
				ANO_DT_Send_PID(5,
												0,
												0,
												0,
												0,		
												0,
												0,				
												0,					
												0,						
												0);
				ANO_Send_PID_Flag[4]=0;
		}
		else if(ANO_Cnt == 11 && ANO_Send_PID_Flag[5] == 1)//��6֡ PID
		{
				ANO_DT_Send_PID(6,  
												0,
												0,
												0,
												0,
												0,
												0,
												0,
												0,
												0);
				ANO_Send_PID_Flag[5]=0;
				ANO_Cnt=0;
		}					
}





//---------------�����Ƿָ�����������������������<����Ϊ���� ����>�������������������������Ƿָ���------------------//

void Save_Or_Reset_PID_Parameter(void) 
{
		if(Sort_PID_Flag == 1)//������վ����PID����д��Flash
		{
				PID_Parameter[0].p=Total_Controller.Roll_Gyro_Control.Kp;
				PID_Parameter[0].i=Total_Controller.Roll_Gyro_Control.Ki;
				PID_Parameter[0].d=Total_Controller.Roll_Gyro_Control.Kd;
			
				PID_Parameter[1].p=Total_Controller.Pitch_Gyro_Control.Kp;
				PID_Parameter[1].i=Total_Controller.Pitch_Gyro_Control.Ki;
				PID_Parameter[1].d=Total_Controller.Pitch_Gyro_Control.Kd;
				
				PID_Parameter[2].p=Total_Controller.Yaw_Gyro_Control.Kp;
				PID_Parameter[2].i=Total_Controller.Yaw_Gyro_Control.Ki;
				PID_Parameter[2].d=Total_Controller.Yaw_Gyro_Control.Kd;
				
				PID_Parameter[3].p=Total_Controller.Roll_Angle_Control.Kp;
				PID_Parameter[3].i=Total_Controller.Roll_Angle_Control.Ki;
				PID_Parameter[3].d=Total_Controller.Roll_Angle_Control.Kd;
			
				PID_Parameter[4].p=Total_Controller.Pitch_Angle_Control.Kp;
				PID_Parameter[4].i=Total_Controller.Pitch_Angle_Control.Ki;
				PID_Parameter[4].d=Total_Controller.Pitch_Angle_Control.Kd;
				
				PID_Parameter[5].p=Total_Controller.Yaw_Angle_Control.Kp;
				PID_Parameter[5].i=Total_Controller.Yaw_Angle_Control.Ki;
				PID_Parameter[5].d=Total_Controller.Yaw_Angle_Control.Kd;
				
				PID_Parameter[6].p=Total_Controller.High_Speed_Control.Kp;
				PID_Parameter[6].i=Total_Controller.High_Speed_Control.Ki;
				PID_Parameter[6].d=Total_Controller.High_Speed_Control.Kd;
				
				PID_Parameter[7].p=Total_Controller.High_Position_Control.Kp;
				PID_Parameter[7].i=Total_Controller.High_Position_Control.Ki;
				PID_Parameter[7].d=Total_Controller.High_Position_Control.Kd;			

				log_v("PID_Save_Flash - > Success!");
				Save_PID_Parameter();
				Sort_PID_Flag=0;
		}
		else if(Sort_PID_Flag==2)//����λPID��������д��Flash
		{
			
				Total_PID_Init();//��PID��������Ϊ����Control_Unit���������
						
				PID_Parameter[0].p=Total_Controller.Roll_Gyro_Control.Kp;
				PID_Parameter[0].i=Total_Controller.Roll_Gyro_Control.Ki;
				PID_Parameter[0].d=Total_Controller.Roll_Gyro_Control.Kd;
			
				PID_Parameter[1].p=Total_Controller.Pitch_Gyro_Control.Kp;
				PID_Parameter[1].i=Total_Controller.Pitch_Gyro_Control.Ki;
				PID_Parameter[1].d=Total_Controller.Pitch_Gyro_Control.Kd;
				
				PID_Parameter[2].p=Total_Controller.Yaw_Gyro_Control.Kp;
				PID_Parameter[2].i=Total_Controller.Yaw_Gyro_Control.Ki;
				PID_Parameter[2].d=Total_Controller.Yaw_Gyro_Control.Kd;
				
				PID_Parameter[3].p=Total_Controller.Roll_Angle_Control.Kp;
				PID_Parameter[3].i=Total_Controller.Roll_Angle_Control.Ki;
				PID_Parameter[3].d=Total_Controller.Roll_Angle_Control.Kd;
			
				PID_Parameter[4].p=Total_Controller.Pitch_Angle_Control.Kp;
				PID_Parameter[4].i=Total_Controller.Pitch_Angle_Control.Ki;
				PID_Parameter[4].d=Total_Controller.Pitch_Angle_Control.Kd;
				
				PID_Parameter[5].p=Total_Controller.Yaw_Angle_Control.Kp;
				PID_Parameter[5].i=Total_Controller.Yaw_Angle_Control.Ki;
				PID_Parameter[5].d=Total_Controller.Yaw_Angle_Control.Kd;
				
				PID_Parameter[6].p=Total_Controller.High_Speed_Control.Kp;
				PID_Parameter[6].i=Total_Controller.High_Speed_Control.Ki;
				PID_Parameter[6].d=Total_Controller.High_Speed_Control.Kd;
				
				PID_Parameter[7].p=Total_Controller.High_Position_Control.Kp;
				PID_Parameter[7].i=Total_Controller.High_Position_Control.Ki;
				PID_Parameter[7].d=Total_Controller.High_Position_Control.Kd;
				
				Save_PID_Parameter();
				
				Sort_PID_Flag=0;
				log_v("PID_Reset_Flash - > Success!");
				ANO_Send_PID_Flag[0]=1;//�ظ�Ĭ�ϲ����󣬽����µ����ݷ����õ���վ		
				ANO_Send_PID_Flag[1]=1;
				ANO_Send_PID_Flag[2]=1;
				ANO_Send_PID_Flag[3]=1;
				ANO_Send_PID_Flag[4]=1;
				ANO_Send_PID_Flag[5]=1;
		}
	
		else {
				return;
		}
}

