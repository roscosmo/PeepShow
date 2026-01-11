#include "audio_task.h"

#include "app_freertos.h"
#include "cmsis_os2.h"
#include "main.h"

#include <math.h>
#include <stdbool.h>

extern SAI_HandleTypeDef hsai_BlockA1;

typedef enum
{
  AUDIO_STATE_IDLE = 0,
  AUDIO_STATE_PLAYING = 1
} audio_state_t;

static const uint32_t kAudioFlagHalf = (1UL << 0U);
static const uint32_t kAudioFlagFull = (1UL << 1U);
static const uint32_t kAudioFlagError = (1UL << 2U);

static const uint32_t kAudioSampleRate = 16000U;
static const float kAudioToneHz = 440.0f;
static const float kAudioClickHz = 2000.0f;
static const uint32_t kAudioClickMs = 25U;

static int16_t s_audio_buf[2048];
static uint32_t s_phase_q16 = 0U;
static uint32_t s_phase_step_q16 = 0U;
static audio_state_t s_audio_state = AUDIO_STATE_IDLE;
static uint32_t s_click_stop_ms = 0U;
static uint8_t s_click_active = 0U;

static void audio_fill(int16_t *dst, uint32_t count)
{
  if ((dst == NULL) || (count == 0U))
  {
    return;
  }

  const float two_pi = 6.28318530718f;
  uint32_t frames = count / 2U;
  for (uint32_t i = 0U; i < frames; ++i)
  {
    float phase = (float)(s_phase_q16 & 0xFFFFU) / 65536.0f;
    float sample = sinf(two_pi * phase);
    int16_t pcm = (int16_t)(sample * 20000.0f);
    dst[i * 2U] = pcm;
    dst[i * 2U + 1U] = pcm;
    s_phase_q16 += s_phase_step_q16;
  }
}

static void audio_start(void)
{
  if (s_audio_state == AUDIO_STATE_PLAYING)
  {
    return;
  }

  s_phase_q16 = 0U;
  s_phase_step_q16 = (uint32_t)((kAudioToneHz * 65536.0f) / (float)kAudioSampleRate);
  s_click_active = 0U;
  audio_fill(s_audio_buf, (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));

  (void)osThreadFlagsClear(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                           (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]))) == HAL_OK)
  {
    s_audio_state = AUDIO_STATE_PLAYING;
  }
  else
  {
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  }
}

static void audio_stop(void)
{
  if (s_audio_state == AUDIO_STATE_IDLE)
  {
    return;
  }

  (void)HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  s_audio_state = AUDIO_STATE_IDLE;
  s_click_active = 0U;
  s_click_stop_ms = 0U;
}

static void audio_click_start(void)
{
  if (s_audio_state == AUDIO_STATE_PLAYING)
  {
    return;
  }

  s_phase_q16 = 0U;
  s_phase_step_q16 = (uint32_t)((kAudioClickHz * 65536.0f) / (float)kAudioSampleRate);
  s_click_active = 1U;
  s_click_stop_ms = osKernelGetTickCount() + kAudioClickMs;
  audio_fill(s_audio_buf, (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));

  (void)osThreadFlagsClear(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                           (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]))) == HAL_OK)
  {
    s_audio_state = AUDIO_STATE_PLAYING;
  }
  else
  {
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
    s_click_active = 0U;
  }
}

void audio_task_run(void)
{
  app_audio_cmd_t cmd = 0U;

  for (;;)
  {
    if (s_audio_state == AUDIO_STATE_PLAYING)
    {
      if (s_click_active != 0U)
      {
        uint32_t now_ms = osKernelGetTickCount();
        if ((int32_t)(now_ms - s_click_stop_ms) >= 0)
        {
          audio_stop();
          continue;
        }
      }

      int32_t flags = (int32_t)osThreadFlagsWait(kAudioFlagHalf | kAudioFlagFull | kAudioFlagError,
                                                 osFlagsWaitAny, 20U);
      if (flags >= 0)
      {
        if (((uint32_t)flags & kAudioFlagHalf) != 0U)
        {
          audio_fill(&s_audio_buf[0], (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U));
        }
        if (((uint32_t)flags & kAudioFlagFull) != 0U)
        {
          audio_fill(&s_audio_buf[sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U],
                     (uint32_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0]) / 2U));
          (void)HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                                     (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));
        }
        if (((uint32_t)flags & kAudioFlagError) != 0U)
        {
          audio_stop();
        }
      }
      else if (hsai_BlockA1.State == HAL_SAI_STATE_READY)
      {
        (void)HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)s_audio_buf,
                                   (uint16_t)(sizeof(s_audio_buf) / sizeof(s_audio_buf[0])));
      }
      if (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, 0U) != osOK)
      {
        continue;
      }
    }
    else
    {
      if (osMessageQueueGet(qAudioCmdHandle, &cmd, NULL, osWaitForever) != osOK)
      {
        continue;
      }
    }

    switch (cmd)
    {
      case APP_AUDIO_CMD_TOGGLE_TONE:
        if (s_audio_state == AUDIO_STATE_PLAYING)
        {
          audio_stop();
        }
        else
        {
          audio_start();
        }
        break;
      case APP_AUDIO_CMD_STOP:
        audio_stop();
        break;
      case APP_AUDIO_CMD_KEYCLICK:
        audio_click_start();
        break;
      default:
        break;
    }
  }
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == &hsai_BlockA1) && (tskAudioHandle != NULL))
  {
    (void)osThreadFlagsSet(tskAudioHandle, kAudioFlagHalf);
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == &hsai_BlockA1) && (tskAudioHandle != NULL))
  {
    (void)osThreadFlagsSet(tskAudioHandle, kAudioFlagFull);
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == &hsai_BlockA1) && (tskAudioHandle != NULL))
  {
    (void)osThreadFlagsSet(tskAudioHandle, kAudioFlagError);
  }
}
