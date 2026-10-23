/**
 * @file    task_kws.h
 * @brief   KWS 关键词唤醒任务
 *
 * [2026-07-04]: 集成 DS-CNN KWS 模型。
 *   输入:  16kHz PCM 音频 (Mic_Task DMA)
 *   输出:  救命 / 帮帮我 / silence / unknown
 *   触发:  连续 2 帧同一关键词 → safety_feed_shout()
 */

#ifndef TASK_KWS_H
#define TASK_KWS_H

void TaskKWS(void *argument);

#endif /* TASK_KWS_H */
