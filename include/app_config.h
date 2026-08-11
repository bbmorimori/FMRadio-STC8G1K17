#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/**
 * @brief 定数・設定値の定義
 */

#define MAX_PRESETS 20

#define MIN_FREQ_100KHZ 760
#define MAX_FREQ_100KHZ 1080
#define DEFAULT_FREQ_100KHZ 778

#define MIN_VOLUME 0
#define MAX_VOLUME 15
#define DEFAULT_VOLUME 5

#define DEFAULT_PRESET_COUNT 10
#define DEFAULT_PRESETS_INIT {778, 807, 825, 838, 929, 937, 800, 800, 800, 800}

/**
 * @brief 設定構造体
 */
typedef struct
{
    unsigned short magic;
    unsigned int current_freq;
    unsigned char current_vol;
    unsigned char current_dispmode;
    unsigned char preset_mode;
    unsigned char preset_index;
    unsigned char preset_count;
    unsigned int presets[MAX_PRESETS];
    unsigned char checksum;
} SystemConfig;

__xdata extern SystemConfig sys_cfg;

/**
 * @brief 設定保存
 */
void Config_Save(void);

/**
 * @brief 設定読み込み（破損時は初期化）
 */
void Config_InitAndLoad(void);

/**
 * @brief 指定チャンネルへ選局
 */
void Config_SetChannel(unsigned char ch_idx);

/**
 * @brief 自動プリセット登録（APS）
 */
unsigned char Config_AutoStorePresets(void);

#endif /* __APP_CONFIG_H__ */