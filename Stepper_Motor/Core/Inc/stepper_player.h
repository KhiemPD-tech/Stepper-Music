/**
  ******************************************************************************
  * @file    stepper_player.h
  * @brief   Phát nhạc bằng 3 động cơ bước (STEP/DIR) trên STM32F103C8T6.
  *
  *  Nguyên lý: TIM2 ngắt đều đặn PLAYER_TICK_HZ lần/giây. Mỗi motor có một bộ
  *  tích pha 32 bit (DDS). Mỗi lần tràn pha = 1 xung STEP, nên tần số xung STEP
  *  chính là tần số (cao độ) của nốt nhạc, chính xác đến ~7 ppm.
  *  Lịch phát (nốt nào, lúc nào) lấy từ song_data.h, kiểm tra mỗi 1 ms.
  *
  *  Chân (khai báo trong main.h / CubeMX, tất cả trên GPIOA):
  *     Motor 0: STEP=PA0  DIR=PA1
  *     Motor 1: STEP=PA2  DIR=PA3
  *     Motor 2: STEP=PA4  DIR=PA5
  ******************************************************************************
  */
#ifndef STEPPER_PLAYER_H
#define STEPPER_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ---------------------------- Cấu hình ------------------------------------ */

/* Tần số ngắt TIM2 (Hz). Càng cao thì cao độ càng "sạch" (ít rung pha),
 * nhưng tốn CPU hơn. 100000 => độ phân giải 10 us, tốn ~15% CPU.
 * Chỉ còn 50000 nếu CPU quá tải. Độ rộng xung STEP = 1 tick (10 us). */
#define PLAYER_TICK_HZ          100000UL

/* 1: phát lặp lại bài, nghỉ PLAYER_LOOP_GAP_MS giữa các lượt; 0: phát 1 lần */
#define PLAYER_LOOP             1
#define PLAYER_LOOP_GAP_MS      2000UL

/* Đảo chiều (DIR) sau mỗi N xung STEP để trục motor dao động tại chỗ thay vì
 * quay mãi một hướng. 0 = tắt (không đảo chiều). Ví dụ driver full-step,
 * motor 1.8 độ: 200 xung = 1 vòng. */
#define PLAYER_DIR_FLIP_STEPS   200U

/* ------------------------------- API -------------------------------------- */

void    StepperPlayer_Init(void);       /* cấu hình lại TIM2 theo PLAYER_TICK_HZ */
void    StepperPlayer_Start(void);      /* phát từ đầu bài */
void    StepperPlayer_Stop(void);       /* dừng, hạ các chân STEP xuống LOW */
uint8_t StepperPlayer_IsPlaying(void);  /* 1 nếu đang phát */

/* Gọi trong TIM2_IRQHandler (xem stm32f1xx_it.c). Tự kiểm tra và xóa cờ ngắt. */
void    StepperPlayer_TimerISR(void);

#ifdef __cplusplus
}
#endif

#endif /* STEPPER_PLAYER_H */
