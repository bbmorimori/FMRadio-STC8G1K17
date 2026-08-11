#include "fw_sys.h"
#include "soft_i2c.h"
#include "seg_display.h"
#include "fm_rrd102.h"
#include "app_config.h"

#define _nop_() __asm__("nop")

volatile unsigned char seg_data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
volatile unsigned char seek_busy = 0;

/**
 * @brief ボタンビット定義
 */
#define SW1_BITF (1 << 1)
#define SW2_BITF (1 << 0)
#define SW3_BITF (1 << 3)
#define SW4_BITF (1 << 2)

#define SW_FREQ_UP SW4_BITF
#define SW_FREQ_DOWN SW3_BITF
#define SW_VOL_UP SW1_BITF
#define SW_VOL_DOWN SW2_BITF

/**
 * @brief 7SEGフォント定義
 */
static const unsigned char __code seg_font[] =
    {
        0xC0, 0xF9, 0xA4, 0xB0, 0x99,
        0x92, 0x82, 0xF8, 0x80, 0x90,
        0xFF, 0xBF, 0x8C, 0x8E, 0x86};

enum
{
    SEG_0 = 0,
    SEG_1,
    SEG_2,
    SEG_3,
    SEG_4,
    SEG_5,
    SEG_6,
    SEG_7,
    SEG_8,
    SEG_9,
    SEG_SPC,
    SEG_DSH,
    SEG_P,
    SEG_F,
    SEG_E
};

#define MODE_DISPLAY_TIME 1000

/**
 * @brief 音量表示設定
 *
 * @param[in] volume 音量値
 */
void set_volume_seg(unsigned int volume)
{
    seg_data[3] = seg_font[SEG_SPC];
    seg_data[2] = seg_font[(volume / 10) % 10];
    seg_data[1] = seg_font[volume % 10];
    seg_data[0] = seg_font[SEG_SPC];
}

/**
 * @brief 周波数表示設定
 *
 * @param[in] freqkhz 周波数(kHz)
 */
void set_freq_seg(unsigned int freqkhz)
{
    if (freqkhz >= 1000)
        seg_data[3] = seg_font[(freqkhz / 1000) % 10];
    else
        seg_data[3] = seg_font[SEG_SPC];

    seg_data[2] = seg_font[(freqkhz / 100) % 10];
    seg_data[1] = seg_font[(freqkhz / 10) % 10] & 0x7F;
    seg_data[0] = seg_font[freqkhz % 10];
}

/**
 * @brief 表示クリア
 */
void set_clear_seg(void)
{
    seg_data[3] = seg_font[SEG_SPC];
    seg_data[2] = seg_font[SEG_SPC];
    seg_data[1] = seg_font[SEG_SPC];
    seg_data[0] = seg_font[SEG_SPC];
}

/**
 * @brief プリセット番号表示
 *
 * @param[in] preset_index プリセット番号
 */
void set_preset_seg(unsigned char preset_index)
{
    seg_data[3] = seg_font[SEG_P];
    if (preset_index >= 10)
        seg_data[2] = seg_font[preset_index / 10];
    else
        seg_data[2] = seg_font[SEG_SPC];

    seg_data[1] = seg_font[preset_index % 10];
    seg_data[0] = seg_font[SEG_SPC];
}

/**
 * @brief プリセット設定表示
 *
 * @param[in] preset_index プリセット番号
 */
void set_preset_set_seg(unsigned char preset_index)
{
    seg_data[3] = seg_font[SEG_E];

    if (preset_index >= 10)
        seg_data[2] = seg_font[preset_index / 10];
    else
        seg_data[2] = seg_font[SEG_SPC];

    seg_data[1] = seg_font[preset_index % 10];
    seg_data[0] = seg_font[SEG_SPC];
}

/**
 * @brief 表示モード切替表示
 *
 * @param[in] disp_index 表示モード番号
 */
void set_disp_seg(unsigned char disp_index)
{
    switch (disp_index)
    {
    case 1:
        seg_data[3] = seg_font[SEG_P];
        seg_data[2] = seg_font[SEG_SPC];
        seg_data[1] = seg_font[SEG_SPC];
        seg_data[0] = seg_font[SEG_SPC];
        break;
    default:
        seg_data[3] = seg_font[SEG_F];
        seg_data[2] = seg_font[SEG_SPC];
        seg_data[1] = seg_font[SEG_SPC];
        seg_data[0] = seg_font[SEG_SPC];
        break;
    }
}

/**
 * @brief WKT割り込みハンドラ
 */
void WKT_ISR(void) __interrupt(19)
{
    WKTCH &= ~0x40;
}

/**
 * @brief 約500msスリープへ移行
 */
void enter_sleep_500ms(void)
{
    P3M1 = 0x00;
    P3M0 = 0x00;
    P3 = 0xFF;

    WKTCH = 0x00;
    WKTCL = 0xE7;
    WKTCH = 0x80 | 0x03;

    EX0 = 0;
    EX1 = 0;
    IE2 |= 0x08;
    EA = 1;

    _nop_();
    _nop_();
    PCON |= 0x02;

    _nop_();
    _nop_();
    _nop_();
    _nop_();

    WKTCH &= ~0x80;

    wait_ms(5);
}

/**
 * @brief スイッチが押されるまで500msスリープを繰り返す
 */
void enter_sleep(void)
{
    wait_ms(10);

    while (1)
    {
        enter_sleep_500ms();

        unsigned char i;
        for (i = 0; i < 5; i++)
        {
            wait_ms(1);
            if ((sw_status & 0x0F) != 0x0F)
                break;
        }

        if ((sw_status & 0x0F) != 0x0F)
            break;
    }
}

/**
 * @brief ソフトSEEK処理
 *
 * @param[in] dir       1:UP, 0:DOWN
 * @param[in] threshold RSSI閾値
 * @return 見つかった周波数
 */
unsigned int FM_RRD102_SoftSeek(unsigned char dir, unsigned char threshold)
{
    unsigned int start_freq = FM_RRD102_GetFrequency();
    unsigned int freq = start_freq;
    unsigned char rssi;
    unsigned int step_count = 0;

    while (1)
    {
        if (dir)
        {
            freq++;
            if (freq > 1080)
                freq = 760;
        }
        else
        {
            if (freq <= 760)
                freq = 1080;
            else
                freq--;
        }

        FM_RRD102_SetFrequency(freq);
        set_freq_seg(freq);

        wait_ms(150);

        unsigned char r1 = FM_RRD102_GetRSSI();
        wait_ms(20);
        unsigned char r2 = FM_RRD102_GetRSSI();
        rssi = (r1 + r2) >> 1;

        if (rssi >= threshold)
            break;

        step_count++;
        if (step_count >= 321)
        {
            FM_RRD102_SetFrequency(start_freq);
            set_freq_seg(start_freq);
            return start_freq;
        }

        if ((sw_status & 0x0F) != 0x0F)
            break;
    }

    return freq;
}

/**
 * @brief プリセット周波数設定
 *
 * @param[in] preset_no プリセット番号
 * @param[in] init_freq 初期周波数
 */
void Preset_SetFrequency(uint8_t preset_no, uint16_t init_freq)
{
    uint16_t freqkhz = init_freq;

    set_preset_set_seg(preset_no + 1);
    FM_RRD102_SetFrequency(freqkhz);
    wait_ms(MODE_DISPLAY_TIME);
    set_freq_seg(freqkhz);
    SW_ClearAll();

    while (1)
    {
        if (sw_click_flag & SW_FREQ_DOWN)
        {
            sw_click_flag &= ~SW_FREQ_DOWN;

            if (freqkhz > 760)
                freqkhz--;

            FM_RRD102_SetFrequency(freqkhz);
            set_freq_seg(freqkhz);
        }

        if (sw_click_flag & SW_FREQ_UP)
        {
            sw_click_flag &= ~SW_FREQ_UP;

            if (freqkhz < 1080)
                freqkhz++;

            FM_RRD102_SetFrequency(freqkhz);
            set_freq_seg(freqkhz);
        }

        if ((sw_long_flag & SW_FREQ_DOWN) ||
            (sw_long_flag & SW_FREQ_UP))
        {
            sys_cfg.presets[preset_no] = freqkhz;
            Config_Save();

            set_preset_seg(preset_no + 1);
            wait_ms(MODE_DISPLAY_TIME);
            set_freq_seg(freqkhz);
            SW_ClearAll();
            return;
        }

        wait_ms(100);
    }
}

/**
 * @brief メイン処理
 */
void main(void)
{
    volatile unsigned int freqkhz = 778;
    volatile unsigned char volume = 5;
    volatile unsigned char preset_mode = 0;
    volatile unsigned char preset_index = 1;

    Soft_I2C_Init();
    Seg_Init();

    Config_InitAndLoad();

    freqkhz = sys_cfg.current_freq;
    volume = sys_cfg.current_vol;
    preset_mode = sys_cfg.preset_mode;
    preset_index = sys_cfg.preset_index;

    FM_RRD102_Init();
    FM_RRD102_Mute(1);
    FM_RRD102_SetBass(1);
    FM_RRD102_SetVolume(volume);
    FM_RRD102_SetFrequency(freqkhz);
    FM_RRD102_Mute(0);

    set_freq_seg(freqkhz);
    time_count = 0;

    while (1)
    {
        unsigned char sw_action = 0;

        if (seek_busy && FM_RRD102_IsSeekComplete())
        {
            seek_busy = 0;
            freqkhz = FM_RRD102_GetFrequency();

            set_freq_seg(freqkhz);
            sw_action = 1;
        }

        if ((sw_long_flag & SW_FREQ_DOWN) && (sw_long_flag & SW_FREQ_UP))
        {
            sw_long_flag &= ~(SW_FREQ_DOWN | SW_FREQ_UP);

            if (!preset_mode)
            {
                preset_mode = 1;
                preset_index = 1;

                set_disp_seg(1);
                wait_ms(MODE_DISPLAY_TIME);
                SW_ClearAll();
                set_preset_seg(preset_index);

                freqkhz = sys_cfg.presets[0];
                sys_cfg.current_freq = freqkhz;
                FM_RRD102_SetFrequency(freqkhz);
                sw_action = 1;
            }
            else
            {
                preset_mode = 0;

                set_disp_seg(0);
                wait_ms(MODE_DISPLAY_TIME);
                SW_ClearAll();
                set_freq_seg(freqkhz);
                sw_action = 1;
            }

            if (sw_action)
            {
                time_count = 0;
                sys_cfg.current_freq = freqkhz;
                sys_cfg.current_vol = volume;
                sys_cfg.preset_mode = preset_mode;

                Config_Save();
            }
            continue;
        }

        if (preset_mode)
        {
            if (sw_click_flag & SW_FREQ_DOWN)
            {
                sw_click_flag &= ~SW_FREQ_DOWN;

                if (preset_index == 1)
                    preset_index = sys_cfg.preset_count;
                else
                    preset_index--;

                set_preset_seg(preset_index);

                freqkhz = sys_cfg.presets[preset_index - 1];
                sys_cfg.current_freq = freqkhz;
                FM_RRD102_SetFrequency(freqkhz);
                sw_action = 1;
            }

            if (sw_click_flag & SW_FREQ_UP)
            {
                sw_click_flag &= ~SW_FREQ_UP;

                if (preset_index == sys_cfg.preset_count)
                    preset_index = 1;
                else
                    preset_index++;

                set_preset_seg(preset_index);

                freqkhz = sys_cfg.presets[preset_index - 1];
                sys_cfg.current_freq = freqkhz;
                FM_RRD102_SetFrequency(freqkhz);
                sw_action = 1;
            }

            if (sw_repeat_flag & SW_FREQ_UP || sw_repeat_flag & SW_FREQ_DOWN)
            {
                sw_repeat_flag &= ~(SW_FREQ_UP | SW_FREQ_DOWN);
                Preset_SetFrequency(preset_index - 1, freqkhz);
                sw_action = 1;
            }

            if (sw_click_flag & SW_VOL_UP)
            {
                sw_click_flag &= ~SW_VOL_UP;
                if (volume < 15)
                    volume++;
                FM_RRD102_Mute(0);
                FM_RRD102_SetVolume(volume);
                set_volume_seg(volume);
                sw_action = 1;
            }

            if (sw_repeat_flag & SW_VOL_UP)
            {
                sw_repeat_flag &= ~SW_VOL_UP;
                if (volume < 15)
                    volume++;
                FM_RRD102_SetVolume(volume);
                set_volume_seg(volume);
                sw_action = 1;
            }

            if (sw_click_flag & SW_VOL_DOWN)
            {
                sw_click_flag &= ~SW_VOL_DOWN;
                if (volume > 0)
                    volume--;
                FM_RRD102_Mute(0);
                FM_RRD102_SetVolume(volume);
                set_volume_seg(volume);
                sw_action = 1;
            }

            if (sw_repeat_flag & SW_VOL_DOWN)
            {
                sw_repeat_flag &= ~SW_VOL_DOWN;
                FM_RRD102_Mute(1);
                sw_action = 1;
            }

            if (sw_action)
            {
                time_count = 0;
                sys_cfg.current_freq = freqkhz;
                sys_cfg.current_vol = volume;
                sys_cfg.preset_index = preset_index;
                Config_Save();
            }

            if (time_count >= 3000)
            {
                set_freq_seg(freqkhz);
            }

            if (time_count >= 10000)
            {
                set_clear_seg();
                enter_sleep();
                set_preset_seg(preset_index);
                wait_ms(MODE_DISPLAY_TIME);
                SW_ClearAll();
                time_count = 0;
            }

            wait_ms(100);
            continue;
        }

        if (sw_click_flag & SW_FREQ_UP)
        {
            sw_click_flag &= ~SW_FREQ_UP;
            freqkhz++;
            if (freqkhz > 1080)
                freqkhz = 760;
            FM_RRD102_SetFrequency(freqkhz);
            set_freq_seg(freqkhz);
            sw_action = 1;
        }

        if (sw_click_flag & SW_FREQ_DOWN)
        {
            sw_click_flag &= ~SW_FREQ_DOWN;
            freqkhz--;
            if (freqkhz < 760)
                freqkhz = 1080;
            FM_RRD102_SetFrequency(freqkhz);
            set_freq_seg(freqkhz);
            sw_action = 1;
        }

        if (sw_click_flag & SW_VOL_UP)
        {
            sw_click_flag &= ~SW_VOL_UP;
            if (volume < 15)
                volume++;
            FM_RRD102_Mute(0);
            FM_RRD102_SetVolume(volume);
            set_volume_seg(volume);
            sw_action = 1;
        }

        if (sw_repeat_flag & SW_VOL_UP)
        {
            sw_repeat_flag &= ~SW_VOL_UP;
            if (volume < 15)
                volume++;
            FM_RRD102_SetVolume(volume);
            set_volume_seg(volume);
            sw_action = 1;
        }

        if (sw_click_flag & SW_VOL_DOWN)
        {
            sw_click_flag &= ~SW_VOL_DOWN;
            if (volume > 0)
                volume--;
            FM_RRD102_Mute(0);
            FM_RRD102_SetVolume(volume);
            set_volume_seg(volume);
            sw_action = 1;
        }

        if (sw_repeat_flag & SW_VOL_DOWN)
        {
            sw_repeat_flag &= ~SW_VOL_DOWN;
            FM_RRD102_Mute(1);
            sw_action = 1;
        }

        if (sw_repeat_flag & SW_FREQ_UP)
        {
            sw_repeat_flag &= ~SW_FREQ_UP;
            freqkhz = FM_RRD102_SoftSeek(1, 23);
            set_freq_seg(freqkhz);
            sw_action = 1;
        }

        if (sw_repeat_flag & SW_FREQ_DOWN)
        {
            sw_repeat_flag &= ~SW_FREQ_DOWN;
            freqkhz = FM_RRD102_SoftSeek(0, 23);
            set_freq_seg(freqkhz);
            sw_action = 1;
        }

        if (sw_action)
        {
            time_count = 0;
            sys_cfg.current_freq = freqkhz;
            sys_cfg.current_vol = volume;
            Config_Save();
        }

        if (time_count >= 3000)
        {
            set_freq_seg(freqkhz);
        }

        if (time_count >= 10000)
        {
            set_clear_seg();
            enter_sleep();

            set_freq_seg(freqkhz);
            wait_ms(MODE_DISPLAY_TIME);
            SW_ClearAll();

            time_count = 0;
        }

        wait_ms(100);
    }
}
