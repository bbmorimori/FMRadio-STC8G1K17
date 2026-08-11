#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__


// ==========================================
// ピン配置の設定（STC8G1K17）
// SDA: P5.4, SCL: P5.5
// ==========================================
#define I2C_SDA     P54
#define I2C_SCL     P55

// SDA/SCL は「準双方向」前提
// SDA=1 → 入力（プルアップ）
// SDA=0 → 出力Low
// SCL も同様に扱う

// SDA を入力状態に（ラインを開放）
#define I2C_SDA_IN()   do { I2C_SDA = 1; } while (0)
// SDA を出力状態に（値は直接 I2C_SDA に書く）
#define I2C_SDA_OUT()  do { /* 準双方向なのでモード変更不要 */ } while (0)

// SCL を入力状態に（ラインを開放）
#define I2C_SCL_IN()   do { I2C_SCL = 1; } while (0)
// SCL を出力状態に（値は直接 I2C_SCL に書く）
#define I2C_SCL_OUT()  do { /* 準双方向なのでモード変更不要 */ } while (0)

// ==========================================
// ソフトウェアI2C 制御関数
// ==========================================

/**
 * @brief ソフトウェアI2Cの初期化（GPIOピン設定）
 *        P5.4(SDA), P5.5(SCL) を準双方向に設定し、両方HIGHにする
 */
void Soft_I2C_Init(void);

/**
 * @brief I2C スタートコンディションの発行
 */
void Soft_I2C_Start(void);

/**
 * @brief I2C ストップコンディションの発行
 */
void Soft_I2C_Stop(void);

/**
 * @brief ACK / NACK 信号の送信
 * @param ack 0: ACK (応答あり), 1: NACK (応答なし/読み込み終了)
 */
void Soft_I2C_SendAck(unsigned char ack);

/**
 * @brief ACK 信号の受信
 * @return unsigned char 0: ACK受信成功, 1: NACK (応答なし)
 */
unsigned char Soft_I2C_RecvAck(void);

/**
 * @brief 1バイト データの送信
 * @param dat 送信する8ビットデータ
 */
void Soft_I2C_WriteByte(unsigned char dat);

/**
 * @brief 1バイト データの受信
 * @return unsigned char 受信した8ビットデータ
 */
unsigned char Soft_I2C_ReadByte(void);

#endif /* __SOFT_I2C_H__ */
