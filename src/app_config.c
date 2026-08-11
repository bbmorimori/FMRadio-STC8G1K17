#include "app_config.h"
#include "fm_rrd102.h"
#include "fw_sys.h"
#include "seg_display.h"

#define _nop_() __asm__("nop")

#define EEPROM_START_ADDR 0x0000
#define IAP_CMD_READ 1
#define IAP_CMD_WRITE 2
#define IAP_CMD_ERASE 3
#define IAP_ENABLE 0x80

#define MAIN_FOSC_MHZ 18
#define CONFIG_MAGIC 0x5A5A

__xdata SystemConfig sys_cfg;

/**
 * @brief EEPROM IAP機能を無効化
 */
static void EEPROM_Disable(void)
{
    IAP_CONTR = 0;
    IAP_CMD = 0;
    IAP_TRIG = 0;
    IAP_ADDRH = 0xFF;
    IAP_ADDRL = 0xFF;
}

/**
 * @brief EEPROMへ1バイト書き込み
 *
 * @param[in] addr 書き込みアドレス
 * @param[in] dat  書き込むデータ
 */
static void EEPROM_WriteByte(unsigned int addr, unsigned char dat)
{
    unsigned char ea_state = EA;
    EA = 0;

    IAP_TPS = MAIN_FOSC_MHZ;
    IAP_CONTR = IAP_ENABLE;
    IAP_CMD = IAP_CMD_WRITE;
    IAP_ADDRH = (unsigned char)(addr >> 8);
    IAP_ADDRL = (unsigned char)(addr & 0xFF);
    IAP_DATA = dat;

    IAP_TRIG = 0x5A;
    IAP_TRIG = 0xA5;
    _nop_();
    _nop_();

    EEPROM_Disable();
    EA = ea_state;
}

/**
 * @brief EEPROMから1バイト読み出し
 *
 * @param[in] addr 読み出しアドレス
 * @return 読み出したデータ
 */
static unsigned char EEPROM_ReadByte(unsigned int addr)
{
    unsigned char dat;
    unsigned char ea_state = EA;
    EA = 0;

    IAP_TPS = MAIN_FOSC_MHZ;
    IAP_CONTR = IAP_ENABLE;
    IAP_CMD = IAP_CMD_READ;
    IAP_ADDRH = (unsigned char)(addr >> 8);
    IAP_ADDRL = (unsigned char)(addr & 0xFF);

    IAP_TRIG = 0x5A;
    IAP_TRIG = 0xA5;
    _nop_();
    _nop_();

    dat = IAP_DATA;
    EEPROM_Disable();
    EA = ea_state;

    return dat;
}

/**
 * @brief EEPROMセクタ消去
 *
 * @param[in] addr セクタ先頭アドレス
 */
static void EEPROM_EraseSector(unsigned int addr)
{
    unsigned char ea_state = EA;
    EA = 0;

    IAP_TPS = MAIN_FOSC_MHZ;
    IAP_CONTR = IAP_ENABLE;
    IAP_CMD = IAP_CMD_ERASE;
    IAP_ADDRH = (unsigned char)(addr >> 8);
    IAP_ADDRL = (unsigned char)(addr & 0xFF);

    IAP_TRIG = 0x5A;
    IAP_TRIG = 0xA5;
    _nop_();
    _nop_();

    EEPROM_Disable();
    EA = ea_state;
}

/**
 * @brief 設定構造体のチェックサム計算
 *
 * @return チェックサム値
 */
static unsigned char CalculateChecksum(void)
{
    unsigned char *ptr = (unsigned char *)&sys_cfg;
    unsigned char sum = 0;

    for (unsigned int i = 0; i < sizeof(SystemConfig) - 1; i++)
    {
        sum += ptr[i];
    }
    return sum;
}

/**
 * @brief 設定内容をEEPROMへ保存
 *
 * @return なし
 */
void Config_Save(void)
{
    unsigned char *ptr = (unsigned char *)&sys_cfg;
    unsigned int i;

    sys_cfg.checksum = CalculateChecksum();

    EEPROM_EraseSector(EEPROM_START_ADDR);
    for (i = 0; i < sizeof(SystemConfig); i++)
    {
        EEPROM_WriteByte(EEPROM_START_ADDR + i, ptr[i]);
    }
}

/**
 * @brief EEPROMから設定を読み込み、破損時は初期化
 *
 * @return なし
 */
void Config_InitAndLoad(void)
{
    unsigned char *ptr = (unsigned char *)&sys_cfg;
    unsigned int i;

    const unsigned int __code default_presets[DEFAULT_PRESET_COUNT] = DEFAULT_PRESETS_INIT;

    for (i = 0; i < sizeof(SystemConfig); i++)
    {
        ptr[i] = EEPROM_ReadByte(EEPROM_START_ADDR + i);
    }

    if ((sys_cfg.magic != CONFIG_MAGIC) || (sys_cfg.checksum != CalculateChecksum()))
    {
        sys_cfg.magic = CONFIG_MAGIC;
        sys_cfg.current_freq = 800;
        sys_cfg.current_vol = 5;
        sys_cfg.preset_mode = 0;
        sys_cfg.preset_index = 1;

        sys_cfg.preset_count = DEFAULT_PRESET_COUNT;
        for (i = 0; i < DEFAULT_PRESET_COUNT; i++)
        {
            sys_cfg.presets[i] = default_presets[i];
        }

        for (i = DEFAULT_PRESET_COUNT; i < MAX_PRESETS; i++)
        {
            sys_cfg.presets[i] = 0;
        }

        sys_cfg.checksum = CalculateChecksum();
        Config_Save();
    }
}

/**
 * @brief 指定チャンネルへ選局
 *
 * @param[in] ch_idx チャンネル番号
 */
void Config_SetChannel(unsigned char ch_idx)
{
    if (ch_idx < sys_cfg.preset_count)
    {
        sys_cfg.current_freq = sys_cfg.presets[ch_idx];
        FM_RRD102_SetFrequency(sys_cfg.current_freq);
        Config_Save();
    }
}

/**
 * @brief 自動プリセット登録（APS）
 *
 * @return 登録できた局数
 */
unsigned char Config_AutoStorePresets(void)
{
    unsigned int search_freq = 760;
    unsigned int found_freq;
    unsigned char timeout;

    sys_cfg.preset_count = 0;

    FM_RRD102_SetFrequency(search_freq);

    while (search_freq <= 1080 && sys_cfg.preset_count < MAX_PRESETS)
    {
        FM_RRD102_Seek(1);

        timeout = 0;
        while (!FM_RRD102_IsSeekComplete())
        {
            wait_ms(10);
            if (++timeout > 200)
                break;
        }

        if (FM_RRD102_IsSeekFailed())
        {
            break;
        }

        found_freq = FM_RRD102_GetFrequency();

        if (found_freq <= search_freq)
        {
            break;
        }

        sys_cfg.presets[sys_cfg.preset_count] = found_freq;
        sys_cfg.preset_count++;

        search_freq = found_freq;
    }

    if (sys_cfg.preset_count > 0)
    {
        sys_cfg.current_freq = sys_cfg.presets[0];
    }
    else
    {
        sys_cfg.current_freq = 800;
    }

    FM_RRD102_SetFrequency(sys_cfg.current_freq);
    Config_Save();

    return sys_cfg.preset_count;
}
