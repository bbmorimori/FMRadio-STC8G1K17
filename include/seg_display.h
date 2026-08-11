
#ifndef __SEG_DISPLAY_H__
#define __SEG_DISPLAY_H__

#define LONG_PRESS_COUNT 200
#define REPEAT_INTERVAL 20

#define SW2_BIT 0x01
#define SW1_BIT 0x02
#define SW4_BIT 0x04
#define SW3_BIT 0x08

extern volatile unsigned char seg_data[4];
extern volatile unsigned char sw_status;

extern volatile unsigned char sw_push_flag;
extern volatile unsigned char sw_release_flag;
extern volatile unsigned char sw_click_flag;
extern volatile unsigned char sw_long_flag;
extern volatile unsigned char sw_repeat_flag;

extern volatile unsigned int wait_counter;
extern volatile unsigned long time_count;

/**
 * @brief 7SEG初期化
 */
void Seg_Init(void);

/**
 * @brief Timer0 ISR（1ms）
 */
void Timer0_ISR(void) __interrupt(1);

/**
 * @brief ms待機
 */
void wait_ms(unsigned int ms);

/**
 * @brief SWイベントクリア
 */
void SW_ClearAll(void);

#endif
