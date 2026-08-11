#include "soft_i2c.h"
#include "fw_sys.h"
#define _nop_() __asm__("nop")

/**
 * @brief I2C遅延（速度調整）
 *
 * @return なし
 */
static void I2C_Delay(void)
{
    volatile uint8_t i = 4;
    while (i--)
    {
        _nop_();
    }
}

/**
 * @brief ソフトI2C初期化（P5.4=SDA, P5.5=SCL を準双方向に設定）
 *
 * @return なし
 */
void Soft_I2C_Init(void)
{
    P5M1 &= ~((1 << 4) | (1 << 5));
    P5M0 &= ~((1 << 4) | (1 << 5));

    I2C_SDA = 1;
    I2C_SCL = 1;
}

/**
 * @brief I2C STARTコンディション生成
 *
 * @return なし
 */
void Soft_I2C_Start(void)
{
    I2C_SDA = 1;
    I2C_SCL = 1;
    I2C_Delay();

    I2C_SDA = 0;
    I2C_Delay();

    I2C_SCL = 0;
}

/**
 * @brief I2C STOPコンディション生成
 *
 * @return なし
 */
void Soft_I2C_Stop(void)
{
    I2C_SCL = 0;
    I2C_SDA = 0;
    I2C_Delay();

    I2C_SCL = 1;
    I2C_Delay();

    I2C_SDA = 1;
    I2C_Delay();
}

/**
 * @brief 1バイト送信（MSB→LSB）
 *
 * @param[in] dat 送信データ
 * @return なし
 */
void Soft_I2C_WriteByte(unsigned char dat)
{
    unsigned char i;

    for (i = 0; i < 8; i++)
    {
        I2C_SDA = (dat & 0x80) ? 1 : 0;
        dat <<= 1;

        I2C_Delay();
        I2C_SCL = 1;
        I2C_Delay();
        I2C_SCL = 0;
    }
}

/**
 * @brief 1バイト受信（MSB→LSB）
 *
 * @return 受信データ
 */
unsigned char Soft_I2C_ReadByte(void)
{
    unsigned char i, dat = 0;

    I2C_SDA = 1;

    for (i = 0; i < 8; i++)
    {
        dat <<= 1;

        I2C_SCL = 1;
        I2C_Delay();

        if (I2C_SDA)
            dat |= 0x01;

        I2C_SCL = 0;
        I2C_Delay();
    }

    return dat;
}

/**
 * @brief ACK/NACK送信
 *
 * @param[in] ack 0=ACK, 1=NACK
 * @return なし
 */
void Soft_I2C_SendAck(unsigned char ack)
{
    I2C_SDA = ack ? 1 : 0;

    I2C_Delay();
    I2C_SCL = 1;
    I2C_Delay();
    I2C_SCL = 0;

    I2C_SDA = 1;
}

/**
 * @brief ACK受信
 *
 * @return 0=ACK, 1=NACK
 */
unsigned char Soft_I2C_RecvAck(void)
{
    unsigned char ack;

    I2C_SDA = 1;
    I2C_Delay();

    I2C_SCL = 1;
    I2C_Delay();

    ack = I2C_SDA;

    I2C_SCL = 0;
    I2C_Delay();

    return ack;
}
