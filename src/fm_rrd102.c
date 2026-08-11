#include "fm_rrd102.h"
#include "seg_display.h"

// RDA5807M I2C アドレス
#define RDA5807M_I2C_SEQ_WRITE_ADDR 0x20  // シーケンシャル書き込み (Reg 0x02〜)
#define RDA5807M_I2C_RAND_WRITE_ADDR 0x22 // ランダム書き込み (レジスタアドレス指定)
#define RDA5807M_I2C_READ_ADDR 0x21       // 読み出し用

// レジスタ初期設定値バッファ (Reg 0x02 〜 Reg 0x07)
static unsigned char rda_regs[12] = {
    0xC0, 0x09, // Reg 0x02: DHIZ=1, DMUTE=1, NEW_METHOD=1, ENABLE=1
    0x00, 0x08, // Reg 0x03: CHAN=0, TUNE=0, BAND=10 (76-108MHz JAPAN WIDE BAND)
    0x0A, 0x00, // Reg 0x04: DE=1 (50us), SOFTMUTE_EN=1
    0x88, 0x0F, // Reg 0x05: INT_MODE=1, SEEKTH=1000, VOLUME=15
    0x00, 0x00, // Reg 0x06: 予約
    0x42, 0x2A  // Reg 0x07: 推奨値
};

/**
 * @brief 指定レジスタへ 16bit データを書き込む（ランダム書き込み）
 *
 * @param[in] reg_addr  レジスタ番号
 * @param[in] high_byte 上位バイト
 * @param[in] low_byte  下位バイト
 */
static void FM_Write_Register(unsigned char reg_addr, unsigned char high_byte, unsigned char low_byte)
{
    Soft_I2C_Start();
    Soft_I2C_WriteByte(RDA5807M_I2C_RAND_WRITE_ADDR); // 0x22
    Soft_I2C_RecvAck();

    Soft_I2C_WriteByte(reg_addr); // レジスタ番号
    Soft_I2C_RecvAck();

    Soft_I2C_WriteByte(high_byte); // 上位バイト
    Soft_I2C_RecvAck();

    Soft_I2C_WriteByte(low_byte); // 下位バイト
    Soft_I2C_RecvAck();

    Soft_I2C_Stop();
    wait_ms(10);
}

/**
 * @brief RRD102 の初期化処理
 *
 * @return なし
 */
void FM_RRD102_Init(void)
{
    FM_Write_Register(0x03, 0x00, 0x00); // Reg 0x03 クリア
    FM_Write_Register(0x02, 0x00, 0x01); // SOFT_RESET=1
    wait_ms(30);

    FM_Write_Register(0x03, 0x00, 0x08); // BAND設定

    rda_regs[0] = 0xC0; // DHIZ=1, DMUTE=1
    rda_regs[1] = 0x09; // NEW_METHOD=1, ENABLE=1
    FM_Write_Register(0x02, rda_regs[0], rda_regs[1]);

    rda_regs[6] = 0x90;
    rda_regs[7] = 0x85; // VOLUME=5
    FM_Write_Register(0x05, rda_regs[6], rda_regs[7]);
}

/**
 * @brief 周波数を直接設定する（例: 77.8MHz → 778）
 *
 * @param[in] freq_100khz  周波数(100kHz単位)
 * @return なし
 */
void FM_RRD102_SetFrequency(unsigned int freq_100khz)
{
    unsigned int chan;
    unsigned char low_bits;

    if (freq_100khz < 760U)
    {
        freq_100khz = 760U;
    }
    else if (freq_100khz > 1080U)
    {
        freq_100khz = 1080U;
    }

    chan = freq_100khz - 760U;

    rda_regs[2] = (unsigned char)(chan >> 2);     // CHAN[9:2]
    low_bits = (unsigned char)(chan & 0x03) << 6; // CHAN[1:0]
    rda_regs[3] = low_bits | 0x18;                // TUNE + BAND

    FM_Write_Register(0x03, rda_regs[2], rda_regs[3]);

    rda_regs[3] &= ~0x10; // TUNEビットリセット
}

/**
 * @brief 音量設定（0〜15）
 *
 * @param[in] volume 音量値
 * @return なし
 */
void FM_RRD102_SetVolume(unsigned char volume)
{
    if (volume > 15U)
    {
        volume = 15U;
    }

    rda_regs[7] = (rda_regs[7] & 0xF0) | volume;
    FM_Write_Register(0x05, rda_regs[6], rda_regs[7]);
}

/**
 * @brief ミュート設定（1: 消音, 0: 解除）
 *
 * @param[in] mute ミュートフラグ
 * @return なし
 */
void FM_RRD102_Mute(unsigned char mute)
{
    if (mute)
    {
        rda_regs[0] &= ~(1 << 6); // DMUTE=0
    }
    else
    {
        rda_regs[0] |= (1 << 6); // DMUTE=1
    }

    FM_Write_Register(0x02, rda_regs[0], rda_regs[1]);
}

/**
 * @brief SEEK（オートスキャン）開始
 *
 * @param[in] seek_up 1: 周波数UP方向, 0: DOWN方向
 * @return なし
 */
void FM_RRD102_Seek(unsigned char seek_up)
{
    rda_regs[0] |= (1 << 6); // DMUTE=1

    if (seek_up)
    {
        rda_regs[0] |= (1 << 1); // SEEKUP=1
    }
    else
    {
        rda_regs[0] &= ~(1 << 1); // SEEKUP=0
    }

    rda_regs[0] |= (1 << 0); // SEEK=1

    FM_Write_Register(0x02, rda_regs[0], rda_regs[1]);

    rda_regs[0] &= ~(1 << 0); // 次回用にSEEKビットを落とす
}

/**
 * @brief SEEK完了確認
 *
 * @return 1: 完了, 0: 選局中
 */
unsigned char FM_RRD102_IsSeekComplete(void)
{
    unsigned char reg0a_high;

    Soft_I2C_Start();
    Soft_I2C_WriteByte(RDA5807M_I2C_READ_ADDR);
    Soft_I2C_RecvAck();

    reg0a_high = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(1);

    Soft_I2C_Stop();

    return (reg0a_high & 0x40) ? 1 : 0;
}

/**
 * @brief SEEK失敗確認
 *
 * @return 1: 失敗, 0: 成功または実行中
 */
unsigned char FM_RRD102_IsSeekFailed(void)
{
    unsigned char reg0a_high;

    Soft_I2C_Start();
    Soft_I2C_WriteByte(RDA5807M_I2C_READ_ADDR);
    Soft_I2C_RecvAck();

    reg0a_high = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(1);

    Soft_I2C_Stop();

    return (reg0a_high & 0x20) ? 1 : 0;
}

/**
 * @brief 重低音ブースト設定
 *
 * @param[in] enable 1: ON, 0: OFF
 * @return なし
 */
void FM_RRD102_SetBass(unsigned char enable)
{
    if (enable)
    {
        rda_regs[0] |= (1 << 4);
    }
    else
    {
        rda_regs[0] &= ~(1 << 4);
    }

    FM_Write_Register(0x02, rda_regs[0], rda_regs[1]);
}

/**
 * @brief ステレオ/モノラル設定
 *
 * @param[in] mono 1: モノラル固定, 0: ステレオ
 * @return なし
 */
void FM_RRD102_SetStereo(unsigned char mono)
{
    if (mono)
    {
        rda_regs[0] |= (1 << 5);
    }
    else
    {
        rda_regs[0] &= ~(1 << 5);
    }

    FM_Write_Register(0x02, rda_regs[0], rda_regs[1]);
}

/**
 * @brief デエンファシス設定
 *
 * @param[in] de_75us 1: 75us, 0: 50us
 * @return なし
 */
void FM_RRD102_SetDeEmphasis(unsigned char de_75us)
{
    if (de_75us)
    {
        rda_regs[4] &= ~(1 << 3);
    }
    else
    {
        rda_regs[4] |= (1 << 3);
    }

    FM_Write_Register(0x04, rda_regs[4], rda_regs[5]);
}

/**
 * @brief ソフトミュート設定
 *
 * @param[in] enable 1: 有効, 0: 無効
 * @return なし
 */
void FM_RRD102_SetSoftMute(unsigned char enable)
{
    if (enable)
    {
        rda_regs[4] |= (1 << 1);
    }
    else
    {
        rda_regs[4] &= ~(1 << 1);
    }

    FM_Write_Register(0x04, rda_regs[4], rda_regs[5]);
}

/**
 * @brief SEEK感度閾値設定（0〜15）
 *
 * @param[in] threshold 閾値
 * @return なし
 */
void FM_RRD102_SetSeekThreshold(unsigned char threshold)
{
    if (threshold > 15U)
    {
        threshold = 15U;
    }

    rda_regs[6] = (rda_regs[6] & 0xF0) | (threshold & 0x0F);
    FM_Write_Register(0x05, rda_regs[6], rda_regs[7]);
}

/**
 * @brief 現在の周波数を取得
 *
 * @return 周波数(100kHz単位)
 */
unsigned int FM_RRD102_GetFrequency(void)
{
    unsigned char reg0a_high, reg0a_low;
    unsigned int readchan;

    Soft_I2C_Start();
    Soft_I2C_WriteByte(RDA5807M_I2C_READ_ADDR);
    Soft_I2C_RecvAck();

    reg0a_high = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(0);
    reg0a_low = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(1);

    Soft_I2C_Stop();

    readchan = (((unsigned int)(reg0a_high & 0x03)) << 8) | reg0a_low;
    return readchan + 760U;
}

/**
 * @brief RSSI（受信信号強度）取得
 *
 * @return RSSI値（0〜127）
 */
unsigned char FM_RRD102_GetRSSI(void)
{
    unsigned char reg0a_high, reg0a_low, reg0b_high;

    Soft_I2C_Start();
    Soft_I2C_WriteByte(RDA5807M_I2C_READ_ADDR);
    Soft_I2C_RecvAck();

    reg0a_high = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(0);
    reg0a_low = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(0);

    reg0b_high = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(1);

    Soft_I2C_Stop();

    return (reg0b_high >> 1);
}

/**
 * @brief ステレオ受信確認
 *
 * @return 1: ステレオ, 0: モノラル
 */
unsigned char FM_RRD102_IsStereo(void)
{
    unsigned char reg0a_high;

    Soft_I2C_Start();
    Soft_I2C_WriteByte(RDA5807M_I2C_READ_ADDR);
    Soft_I2C_RecvAck();

    reg0a_high = Soft_I2C_ReadByte();
    Soft_I2C_SendAck(1);

    Soft_I2C_Stop();

    return (reg0a_high & 0x04) ? 1 : 0;
}
