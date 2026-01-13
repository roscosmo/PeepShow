#include "ui_pages.h"

#include "app_freertos.h"
#include "display_renderer.h"
#include "font8x8_basic.h"
#include "storage_task.h"

#include <stdio.h>
#include <string.h>

static storage_status_t s_last;
static uint8_t s_has_last = 0U;

static const char *storage_mount_label(storage_mount_state_t state)
{
  switch (state)
  {
    case STORAGE_MOUNT_MOUNTED:
      return "OK";
    case STORAGE_MOUNT_ERROR:
      return "ERR";
    default:
      return "NO";
  }
}

static const char *storage_op_label(storage_op_t op)
{
  switch (op)
  {
    case STORAGE_OP_MOUNT:
      return "MOUNT";
    case STORAGE_OP_REMOUNT:
      return "REMOUNT";
    case STORAGE_OP_WRITE:
      return "WRITE";
    case STORAGE_OP_READ:
      return "READ";
    case STORAGE_OP_LIST:
      return "LIST";
    case STORAGE_OP_DELETE:
      return "DELETE";
    case STORAGE_OP_EXISTS:
      return "EXISTS";
    case STORAGE_OP_TEST:
      return "TEST";
    case STORAGE_OP_DPD_ENTER:
      return "DPD ON";
    case STORAGE_OP_DPD_EXIT:
      return "DPD OFF";
    case STORAGE_OP_STREAM_READ:
      return "STREAM";
    case STORAGE_OP_STREAM_TEST:
      return "STREAM";
    case STORAGE_OP_STREAM_OPEN:
      return "SOPEN";
    case STORAGE_OP_STREAM_CLOSE:
      return "SCLOSE";
    default:
      return "NONE";
  }
}

static void format_kb(char *dst, size_t dst_len, uint32_t bytes)
{
  uint32_t kb = (bytes + 512U) / 1024U;
  (void)snprintf(dst, dst_len, "%luKB", (unsigned long)kb);
}

static void page_storage_enter(void)
{
  s_has_last = 0U;
}

static uint32_t page_storage_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_SELECT)
  {
    app_audio_cmd_t audio_cmd = APP_AUDIO_CMD_FLASH_TOGGLE;
    (void)osMessageQueuePut(qAudioCmdHandle, &audio_cmd, 0U, 0U);
    return UI_PAGE_EVENT_NONE;
  }

  if (evt == UI_EVT_DEC)
  {
    (void)storage_request_delete("/music.wav");
    return UI_PAGE_EVENT_NONE;
  }

  if (evt == UI_EVT_INC)
  {
    (void)storage_request_remount();
    return UI_PAGE_EVENT_NONE;
  }

  if (evt == UI_EVT_TICK)
  {
    storage_status_t status;
    storage_get_status(&status);
    if ((s_has_last == 0U) ||
        (memcmp(&status, &s_last, sizeof(status)) != 0))
    {
      s_last = status;
      s_has_last = 1U;
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_storage_render(void)
{
  storage_status_t status;
  storage_get_status(&status);

  renderFill(false);
  renderDrawText(4U, 4U, "STORAGE", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t y = (uint16_t)(FONT8X8_HEIGHT + 8U);
  uint16_t step = (uint16_t)(FONT8X8_HEIGHT + 2U);

  char line[32];
  char size_buf[16];
  (void)snprintf(line, sizeof(line), "Mount: %s", storage_mount_label(status.mount_state));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  format_kb(size_buf, sizeof(size_buf), status.flash_size);
  (void)snprintf(line, sizeof(line), "Total: %s", size_buf);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  if (status.stats_valid != 0U)
  {
    format_kb(size_buf, sizeof(size_buf), status.flash_used);
    (void)snprintf(line, sizeof(line), "Used: %s", size_buf);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "Used: N/A");
  }
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  if (status.stats_valid != 0U)
  {
    format_kb(size_buf, sizeof(size_buf), status.flash_free);
    (void)snprintf(line, sizeof(line), "Free: %s", size_buf);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "Free: N/A");
  }
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  if (status.stats_valid == 0U)
  {
    (void)snprintf(line, sizeof(line), "Music: N/A");
  }
  else if (status.music_present != 0U)
  {
    format_kb(size_buf, sizeof(size_buf), status.music_size);
    (void)snprintf(line, sizeof(line), "Music: %s", size_buf);
  }
  else
  {
    (void)snprintf(line, sizeof(line), "Music: none");
  }
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "Last: %s", storage_op_label(status.last_op));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "Err: %ld", (long)status.last_err);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  (void)snprintf(line, sizeof(line), "Val: %lu", (unsigned long)status.last_value);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
  y = (uint16_t)(y + step);

  uint16_t height = renderGetHeight();
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT + 2U));
    renderDrawText(2U, hint_y, "A:PLAY L:DEL", RENDER_LAYER_UI, RENDER_STATE_BLACK);
    if (hint_y > (uint16_t)(FONT8X8_HEIGHT + 2U))
    {
      uint16_t hint_y2 = (uint16_t)(hint_y - (FONT8X8_HEIGHT + 2U));
      renderDrawText(2U, hint_y2, "R:REMOUNT", RENDER_LAYER_UI, RENDER_STATE_BLACK);
    }
  }
}

const ui_page_t PAGE_STORAGE =
{
  .name = "Storage",
  .enter = page_storage_enter,
  .event = page_storage_event,
  .render = page_storage_render,
  .exit = NULL,
  .tick_ms = 250U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
