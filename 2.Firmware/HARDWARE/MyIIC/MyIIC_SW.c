#include "MyIIC_SW.h"
#include "delay.h"

void MyIIC_SW_W_SCL(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_8, (BitAction)BitValue);

	delay_us(2);
}

void MyIIC_SW_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_4, (BitAction)BitValue);

	delay_us(2);
}

uint8_t MyIIC_SW_R_SDA(void)
{
	uint8_t BitValue;
	BitValue = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4);

	delay_us(2);
	return BitValue;
}

void MyIIC_SW_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB, ENABLE); // 使能GPIOB时钟

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOA, GPIO_Pin_8);
	GPIO_SetBits(GPIOB, GPIO_Pin_4);
}

void MyIIC_SW_Start(void)
{
	MyIIC_SW_W_SDA(1);
	MyIIC_SW_W_SCL(1);
	MyIIC_SW_W_SDA(0);
	MyIIC_SW_W_SCL(0);
}

void MyIIC_SW_Stop(void)
{
	MyIIC_SW_W_SDA(0);
	MyIIC_SW_W_SCL(1);
	MyIIC_SW_W_SDA(1);
}

void MyIIC_SW_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		MyIIC_SW_W_SDA(Byte & (0x80 >> i));
		MyIIC_SW_W_SCL(1);
		MyIIC_SW_W_SCL(0);
	}
}

// 读1个字节，ack=1时，发送ACK，ack=0，发送nACK
uint8_t MyIIC_SW_ReceiveByte(uint8_t ack)
{
	uint8_t i, Byte = 0x00;
	MyIIC_SW_W_SDA(1);
	for (i = 0; i < 8; i++)
	{
		MyIIC_SW_W_SCL(1);
		if (MyIIC_SW_R_SDA() == 1)
		{
			Byte |= (0x80 >> i);
		}
		MyIIC_SW_W_SCL(0);
	}
	if (!ack)
		MyIIC_SW_SendAck(1); // 发送nACK
	else
		MyIIC_SW_SendAck(0); // 发送ACK
	return Byte;
}

void MyIIC_SW_SendAck(uint8_t AckBit)
{
	MyIIC_SW_W_SDA(AckBit);
	MyIIC_SW_W_SCL(1);
	MyIIC_SW_W_SCL(0);
}

uint8_t MyIIC_SW_ReceiveAck(void)
{
	uint8_t AckBit;
	MyIIC_SW_W_SDA(1);
	MyIIC_SW_W_SCL(1);
	AckBit = MyIIC_SW_R_SDA();
	MyIIC_SW_W_SCL(0);
	return AckBit;
}

uint8_t MyIIC_SW_Write_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address, uint8_t REG_data)
{
	MyIIC_SW_Start();
	MyIIC_SW_SendByte((SlaveAddress << 1) | 0); // 发送器件地址+写命令
	if (MyIIC_SW_ReceiveAck())					 // 等待应答
	{
		MyIIC_SW_Stop();
		return 1;
	}
	MyIIC_SW_SendByte(REG_Address); // 写寄存器地址
	MyIIC_SW_ReceiveAck();			 // 等待应答
	MyIIC_SW_SendByte(REG_data);	 // 发送数据
	if (MyIIC_SW_ReceiveAck())		 // 等待ACK
	{
		MyIIC_SW_Stop();
		return 1;
	}
	MyIIC_SW_Stop();
	return 0;
}

uint8_t MyIIC_SW_Read_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address)
{
	u8 res;
	MyIIC_SW_Start();
	MyIIC_SW_SendByte((SlaveAddress << 1) | 0); // 发送器件地址+写命令
	MyIIC_SW_ReceiveAck();						 // 等待应答
	MyIIC_SW_SendByte(REG_Address);			 // 写寄存器地址
	MyIIC_SW_ReceiveAck();						 // 等待应答
	MyIIC_SW_Start();
	MyIIC_SW_SendByte((SlaveAddress << 1) | 1); // 发送器件地址+读命令
	MyIIC_SW_ReceiveAck();						 // 等待应答
	res = MyIIC_SW_ReceiveByte(0);				 // 读取数据,发送nACK
	MyIIC_SW_Stop();							 // 产生一个停止条件
	return res;
}

uint8_t MyIIC_SW_Write_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
	uint8_t i;
	MyIIC_SW_Start();
	MyIIC_SW_SendByte((DeviceAddr << 1) | 0); // 发送器件地址+写命令
	if (MyIIC_SW_ReceiveAck())				   // 等待应答
	{
		MyIIC_SW_Stop();
		return 1;
	}
	MyIIC_SW_SendByte(REG_Address); // 写寄存器地址
	MyIIC_SW_ReceiveAck();			 // 等待应答
	for (i = 0; i < BytesNum; i++)
	{
		MyIIC_SW_SendByte(buf[i]); // 发送数据
		if (MyIIC_SW_ReceiveAck()) // 等待ACK
		{
			MyIIC_SW_Stop();
			return 1;
		}
	}
	MyIIC_SW_Stop();
	return 0;
}

uint8_t MyIIC_SW_Read_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
	MyIIC_SW_Start();
	MyIIC_SW_SendByte((DeviceAddr << 1) | 0); // 发送器件地址+写命令
	if (MyIIC_SW_ReceiveAck())				   // 等待应答
	{
		MyIIC_SW_Stop();
		return 1;
	}
	MyIIC_SW_SendByte(REG_Address); // 写寄存器地址
	MyIIC_SW_ReceiveAck();			 // 等待应答
	MyIIC_SW_Start();
	MyIIC_SW_SendByte((DeviceAddr << 1) | 1); // 发送器件地址+读命令
	MyIIC_SW_ReceiveAck();					   // 等待应答
	while (BytesNum)
	{
		if (BytesNum == 1)
			*buf = MyIIC_SW_ReceiveByte(0); // 读数据,发送nACK
		else
			*buf = MyIIC_SW_ReceiveByte(1); // 读数据,发送ACK
		BytesNum--;
		buf++;
	}
	MyIIC_SW_Stop(); // 产生一个停止条件
	return 0;
}
