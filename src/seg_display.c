#include "seg_display.h"
#define _nop_() __asm__("nop")
#include "fw_sys.h"
#include <stdint.h>

/**
 * @brief STC8G1K17 ピンアサイン
 *
 * @note 桁選択および7セグメント制御ピンの一覧
 */

/* [桁選択]
 *  - Pin 1  : P1.0  -> W1 (第1桁)
 *  - Pin 2  : P1.1  -> W2 (第2桁)
 *  - Pin 3  : P1.6  -> W3 (第3桁)
 *  - Pin 4  : P1.7  -> W4 (第4桁)
 *
 * [7セグメント制御 / SW入力兼用]
 *  - Pin 9  : P3.0  -> Segment A
 *  - Pin 10 : P3.1  -> Segment B
 *  - Pin 11 : P3.2  -> Segment C / S2 スイッチ
 *  - Pin 12 : P3.3  -> Segment D / S1 スイッチ
 *  - Pin 13 : P3.4  -> Segment E / S4 スイッチ
 *  - Pin 14 : P3.5  -> Segment F / S3 スイッチ
 *  - Pin 15 : P3.6  -> Segment G
 *  - Pin 16 : P3.7  -> Segment DP (H)
 */

volatile unsigned int wait_counter = 0;

/**
 * @brief Timer0 初期化（1ms割り込み）
 *
 * @return なし
 */
static void Timer0_Init(void)
{
    SYS_SetClock(); // SDCC使用時は必須
    AUXR &= 0x7F;   // Timer clock 12T mode
    TMOD &= 0xF0;   // Timer mode
    TL0 = 0x00;
    TH0 = 0xFA;
    TF0 = 0;
    TR0 = 1;

    ET0 = 1; // Timer0割り込み許可
    EA = 1;  // 全割り込み許可
    TR0 = 1;
}

volatile unsigned char sw_push_flag = 0;
volatile unsigned char sw_release_flag = 0;
volatile unsigned char sw_long_flag = 0;
volatile unsigned char sw_repeat_flag = 0;
volatile unsigned char sw_click_flag = 0;
volatile unsigned char sw_prev_status = 0x0F;
volatile unsigned int sw_count[4] = {0, 0, 0, 0};
volatile unsigned long time_count = 0;
volatile unsigned char sw_status = 0x0F;

/**
 * @brief 7セグメントおよびスイッチの初期化
 *
 * @return なし
 */
void Seg_Init(void)
{
    P1M0 |= ((1 << 0) | (1 << 1) | (1 << 6) | (1 << 7));
    P1M1 &= ~((1 << 0) | (1 << 1) | (1 << 6) | (1 << 7));

    P1 &= ~((1 << 0) | (1 << 1) | (1 << 6) | (1 << 7));

    P3 = 0xFF;

    P3M1 = 0x00;
    P3M0 = 0xFF;

    Timer0_Init();
}

/**
 * @brief Timer0 割り込み（1ms周期）
 *
 * @note 0〜3: 7SEG表示、4: SWスキャン
 */
void Timer0_ISR(void) __interrupt(1)
{
    unsigned char i;
    unsigned char now;
    unsigned char push;
    unsigned char release;

    static unsigned char repeat_count[4] = {0, 0, 0, 0};
    static unsigned char digit = 0;
    static const char digit_mask[4] =
        {
            0x01,
            0x02,
            0x40,
            0x80};

    time_count++;

    if (wait_counter > 0)
    {
        wait_counter--;
    }

    if (seg_data[0] == 0xFF && seg_data[1] == 0xFF &&
        seg_data[2] == 0xFF && seg_data[3] == 0xFF)
    {
        P1 &= 0x3C;
        digit = 4;
    }

    if (digit < 4)
    {
        P1 &= 0x3C;

        P3 = seg_data[digit];
        P3M1 = 0x00;
        P3M0 = 0xFF;

        P1 |= digit_mask[digit];
    }
    else
    {
        P1 &= 0x3C;

        P3M1 = 0x00;
        P3M0 = 0x00;
        P3 = 0xFF;

        _nop_();
        _nop_();
        _nop_();
        _nop_();

        now = (P3 >> 2) & 0x0F;
        sw_status = now;

        push = (~now) & sw_prev_status;
        release = now & (~sw_prev_status);

        sw_push_flag |= push;
        sw_release_flag |= release;

        for (i = 0; i < 4; i++)
        {
            if (release & (1U << i))
            {
                if (sw_count[i] < LONG_PRESS_COUNT)
                {
                    sw_click_flag |= (1U << i);
                }
            }
        }

        sw_prev_status = now;

        for (i = 0; i < 4; i++)
        {
            if ((now & (1 << i)) == 0)
            {
                if (sw_count[i] < 0xFFFF)
                    sw_count[i]++;

                if (sw_count[i] == LONG_PRESS_COUNT)
                {
                    sw_long_flag |= (1 << i);
                    repeat_count[i] = 0;
                }
                else if (sw_count[i] > LONG_PRESS_COUNT)
                {
                    repeat_count[i]++;

                    if (repeat_count[i] >= REPEAT_INTERVAL)
                    {
                        repeat_count[i] = 0;
                        sw_repeat_flag |= (1 << i);
                    }
                }
            }
            else
            {
                sw_count[i] = 0;
                repeat_count[i] = 0;
            }
        }
    }

    digit++;
    if (digit >= 5)
        digit = 0;
}

/**
 * @brief スイッチ状態を全てクリア
 *
 * @return なし
 */
void SW_ClearAll(void)
{
    EA = 0;

    sw_status = 0x0F;
    sw_prev_status = 0x0F;

    sw_click_flag = 0;
    sw_long_flag = 0;
    sw_repeat_flag = 0;

    EA = 1;
}

/**
 * @brief 指定ミリ秒待機（Timer0割り込みで減算）
 *
 * @param[in] ms 待機時間(ms)
 * @return なし
 */
void wait_ms(unsigned int ms)
{
    wait_counter = ms;
    while (wait_counter > 0)
    {
        ;
    }
}
