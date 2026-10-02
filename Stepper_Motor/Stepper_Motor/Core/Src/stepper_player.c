/**
  ******************************************************************************
  * @file    stepper_player.c
  * @brief   Phát nhạc bằng 3 động cơ bước. Xem mô tả trong stepper_player.h
  ******************************************************************************
  */

/* ISR chạy mỗi 10 us nên ép tối ưu O2 cho file này, kể cả khi build Debug (-O0) */
#pragma GCC optimize ("O2")

#include "stepper_player.h"
#include "song_data.h"          /* sinh bởi tools/convert_song.py */

/* ------------------------------ Phần cứng --------------------------------- */

#define N_MOTORS        3U
#define PLAYER_PORT     STEP_1_GPIO_Port        /* STEP và DIR đều nằm trên GPIOA */

#define ALL_STEP_PINS   ((uint32_t)(STEP_1_Pin | STEP_2_Pin | STEP_3_Pin))
#define ALL_DIR_PINS    ((uint32_t)(DIR_1_Pin  | DIR_2_Pin  | DIR_3_Pin))

static const uint16_t step_pin[N_MOTORS] = { STEP_1_Pin, STEP_2_Pin, STEP_3_Pin };
static const uint16_t dir_pin[N_MOTORS]  = { DIR_1_Pin,  DIR_2_Pin,  DIR_3_Pin  };

/* ------------------------------- Bài hát ---------------------------------- */

static const uint32_t * const track[N_MOTORS] = { song_m0, song_m1, song_m2 };
static const uint16_t track_len[N_MOTORS] = {
  (uint16_t)(sizeof(song_m0) / sizeof(song_m0[0])),
  (uint16_t)(sizeof(song_m1) / sizeof(song_m1[0])),
  (uint16_t)(sizeof(song_m2) / sizeof(song_m2[0])),
};

/* ------------------------------ Trạng thái -------------------------------- */

typedef struct {
  uint32_t acc;      /* bộ tích pha                                           */
  uint32_t inc;      /* bước pha mỗi tick = tần số * 2^32 / tick_hz (0 = nghỉ) */
  uint16_t idx;      /* sự kiện kế tiếp trong track                           */
  uint16_t steps;    /* số step kể từ lần đảo chiều gần nhất                  */
} Voice_t;

static Voice_t  voice[N_MOTORS];
static uint32_t now_ms;            /* thời gian phát hiện tại (ms)            */
static uint32_t sub_ticks;         /* đếm tick trong 1 ms                     */
static uint32_t ticks_per_ms;      /* số tick trong 1 ms                      */
static uint32_t phase_k;           /* 2^32 / tick_hz, tính lúc Init           */
static uint32_t pulse_pins;        /* các chân STEP đang ở mức HIGH           */
static uint32_t flip_pending;      /* các chân DIR cần đảo ở tick kế tiếp     */
static uint32_t dir_shadow;        /* trạng thái hiện tại của các chân DIR    */
static volatile uint8_t playing;

/* ------------------------------ Hàm phụ ----------------------------------- */

static uint32_t timer_clock_hz(void)
{
  uint32_t clk = HAL_RCC_GetPCLK1Freq();
  /* Khi prescaler APB1 khác 1, clock của timer APB1 = 2 x PCLK1 */
  if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
  {
    clk *= 2U;
  }
  return clk;
}

static void rewind_song(void)
{
  for (uint32_t i = 0; i < N_MOTORS; i++)
  {
    voice[i].inc = 0U;
    voice[i].acc = 0U;
    voice[i].idx = 0U;
  }
  now_ms = 0U;
}

/* Gọi mỗi 1 ms từ ISR: kích hoạt các sự kiện (đổi nốt) đã đến giờ */
static void schedule_1ms(void)
{
  now_ms++;

  for (uint32_t i = 0; i < N_MOTORS; i++)
  {
    Voice_t *v = &voice[i];
    const uint32_t *tr = track[i];
    uint32_t n = track_len[i];
    uint32_t k = v->idx;

    while (k < n && SONG_TIME_MS(tr[k]) <= now_ms)
    {
      uint32_t inc = SONG_FREQ_HZ(tr[k]) * phase_k;
      v->inc = inc;
      v->acc = 0x80000000U;   /* bắt đầu ở giữa chu kỳ: nốt vang sớm (sau nửa chu kỳ)
                               * và không bao giờ tạo 2 xung STEP liền nhau */
      k++;
    }
    v->idx = (uint16_t)k;
  }

#if PLAYER_LOOP
  if (now_ms >= (SONG_LENGTH_MS + PLAYER_LOOP_GAP_MS))
  {
    rewind_song();
  }
#else
  if (now_ms >= SONG_LENGTH_MS)
  {
    TIM2->CR1  &= ~TIM_CR1_CEN;
    TIM2->DIER &= ~TIM_DIER_UIE;
    PLAYER_PORT->BSRR = ALL_STEP_PINS << 16;
    pulse_pins = 0U;
    playing = 0U;
  }
#endif
}

/* -------------------------------- API ------------------------------------- */

void StepperPlayer_Init(void)
{
  uint32_t tclk = timer_clock_hz();
  uint32_t arr  = tclk / PLAYER_TICK_HZ;      /* số chu kỳ clock cho 1 tick */
  if (arr < 2U) { arr = 2U; }
  uint32_t tick_hz = tclk / arr;

  /* Cấu hình lại TIM2: không chia tần, ARR theo tick mong muốn.
   * (ghi đè giá trị Prescaler/Period mà CubeMX đã sinh trong tim.c) */
  TIM2->CR1  &= ~TIM_CR1_CEN;
  TIM2->PSC   = 0U;
  TIM2->ARR   = arr - 1U;
  TIM2->EGR   = TIM_EGR_UG;                   /* nạp PSC/ARR ngay */
  TIM2->SR    = 0U;

  ticks_per_ms = tick_hz / 1000U;
  phase_k      = (uint32_t)((((uint64_t)1 << 32) + (tick_hz / 2U)) / tick_hz);

  /* Tất cả STEP/DIR về LOW */
  PLAYER_PORT->BSRR = (ALL_STEP_PINS | ALL_DIR_PINS) << 16;
  dir_shadow   = 0U;
  pulse_pins   = 0U;
  flip_pending = 0U;
  playing      = 0U;
  rewind_song();
}

void StepperPlayer_Start(void)
{
  TIM2->CR1  &= ~TIM_CR1_CEN;
  rewind_song();
  sub_ticks    = 0U;
  pulse_pins   = 0U;
  flip_pending = 0U;
  for (uint32_t i = 0; i < N_MOTORS; i++) { voice[i].steps = 0U; }
  PLAYER_PORT->BSRR = ALL_STEP_PINS << 16;

  TIM2->CNT   = 0U;
  TIM2->SR    = 0U;
  TIM2->DIER |= TIM_DIER_UIE;
  playing     = 1U;
  TIM2->CR1  |= TIM_CR1_CEN;
}

void StepperPlayer_Stop(void)
{
  TIM2->CR1  &= ~TIM_CR1_CEN;
  TIM2->DIER &= ~TIM_DIER_UIE;
  playing     = 0U;
  PLAYER_PORT->BSRR = ALL_STEP_PINS << 16;
  pulse_pins  = 0U;
}

uint8_t StepperPlayer_IsPlaying(void)
{
  return playing;
}

/* ----------------------- Chạy mỗi tick (mặc định 10 us) -------------------- */

void StepperPlayer_TimerISR(void)
{
  if ((TIM2->SR & TIM_SR_UIF) == 0U)
  {
    return;
  }
  TIM2->SR = ~TIM_SR_UIF;                     /* xóa cờ (rc_w0) */
  if (!playing)
  {
    return;
  }

  /* 1) Kết thúc xung STEP của tick trước (độ rộng xung = 1 tick).
   *    Chân DIR chỉ đổi lúc này, khi STEP đang LOW, để thỏa thời gian setup
   *    của driver (lần STEP kế tiếp cách ít nhất ~50 tick). */
  uint32_t bsrr = pulse_pins << 16;
  pulse_pins = 0U;
#if PLAYER_DIR_FLIP_STEPS
  if (flip_pending != 0U)
  {
    dir_shadow ^= flip_pending;
    bsrr |= (dir_shadow & flip_pending) | ((~dir_shadow & flip_pending) << 16);
    flip_pending = 0U;
  }
#endif

  /* 2) Bộ tích pha cho từng motor: tràn pha = phát 1 xung STEP */
  for (uint32_t i = 0; i < N_MOTORS; i++)
  {
    Voice_t *v = &voice[i];
    uint32_t inc = v->inc;
    if (inc != 0U)
    {
      uint32_t acc = v->acc + inc;
      if (acc < v->acc)
      {
        bsrr       |= step_pin[i];
        pulse_pins |= step_pin[i];
#if PLAYER_DIR_FLIP_STEPS
        if (++v->steps >= PLAYER_DIR_FLIP_STEPS)
        {
          v->steps = 0U;
          flip_pending |= dir_pin[i];
        }
#endif
      }
      v->acc = acc;
    }
  }

  /* 3) Một lần ghi BSRR duy nhất cho tất cả chân (atomic, không ảnh hưởng PA khác) */
  if (bsrr != 0U)
  {
    PLAYER_PORT->BSRR = bsrr;
  }

  /* 4) Mỗi 1 ms: chuyển nốt theo lịch */
  if (++sub_ticks >= ticks_per_ms)
  {
    sub_ticks = 0U;
    schedule_1ms();
  }
}
