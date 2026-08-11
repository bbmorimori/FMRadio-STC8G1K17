#ifndef __FM_RRD102_H__
#define __FM_RRD102_H__

#include "soft_i2c.h"

/**
 * @brief FMモジュール初期化
 */
void FM_RRD102_Init(void);

/**
 * @brief 周波数設定（100kHz単位）
 */
void FM_RRD102_SetFrequency(unsigned int freq_100khz);

/**
 * @brief 現在周波数取得
 */
unsigned int FM_RRD102_GetFrequency(void);

/**
 * @brief 音量設定
 */
void FM_RRD102_SetVolume(unsigned char volume);

/**
 * @brief ミュート設定
 */
void FM_RRD102_Mute(unsigned char mute);

/**
 * @brief SEEK開始
 */
void FM_RRD102_Seek(unsigned char seek_up);

/**
 * @brief SEEK完了確認
 */
unsigned char FM_RRD102_IsSeekComplete(void);

/**
 * @brief SEEK失敗確認
 */
unsigned char FM_RRD102_IsSeekFailed(void);

/**
 * @brief RSSI取得
 */
unsigned char FM_RRD102_GetRSSI(void);

/**
 * @brief ステレオ受信確認
 */
unsigned char FM_RRD102_IsStereo(void);

/**
 * @brief 重低音ブースト設定
 */
void FM_RRD102_SetBass(unsigned char enable);

/**
 * @brief ステレオ/モノラル設定
 */
void FM_RRD102_SetStereo(unsigned char mono);

/**
 * @brief デエンファシス設定
 */
void FM_RRD102_SetDeEmphasis(unsigned char de_75us);

/**
 * @brief ソフトミュート設定
 */
void FM_RRD102_SetSoftMute(unsigned char enable);

/**
 * @brief SEEK感度閾値設定
 */
void FM_RRD102_SetSeekThreshold(unsigned char threshold);

#endif /* __FM_RRD102_H__ */
