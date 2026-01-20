#include "ui_pages.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "sound_manager.h"
#include "storage_task.h"

#include <stdio.h>
#include <string.h>

static storage_status_t s_last;
static uint8_t s_has_last = 0U;
static uint8_t s_audio_index = 0U;
static uint8_t s_audio_scroll = 0U;
static uint32_t s_audio_seq_last = 0U;

static uint16_t ui_text_width(const char *text)
{
  if (text == NULL)
  {
    return 0U;
  }

  size_t len = strlen(text);
  if (len == 0U)
  {
    return 0U;
  }

  return (uint16_t)((len * (FONT8X8_WIDTH + 1U)) - 1U);
}

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
    case STORAGE_OP_AUDIO_LIST:
      return "ALIST";
    default:
      return "NONE";
  }
}

static void format_kb(char *dst, size_t dst_len, uint32_t bytes)
{
  uint32_t kb = (bytes + 512U) / 1024U;
  (void)snprintf(dst, dst_len, "%luKB", (unsigned long)kb);
}

static void storage_audio_list_adjust_scroll(uint32_t count, uint16_t rows)
{
  if (count == 0U)
  {
    s_audio_index = 0U;
    s_audio_scroll = 0U;
    return;
  }

  if (s_audio_index >= count)
  {
    s_audio_index = (uint8_t)(count - 1U);
  }

  if (rows == 0U)
  {
    rows = 1U;
  }

  if (s_audio_scroll > s_audio_index)
  {
    s_audio_scroll = s_audio_index;
  }
  if (s_audio_index >= (uint32_t)s_audio_scroll + rows)
  {
    s_audio_scroll = (uint8_t)(s_audio_index - rows + 1U);
  }
}

static void page_storage_info_enter(void)
{
  s_has_last = 0U;
}

static uint32_t page_storage_info_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if (evt == UI_EVT_SELECT)
  {
    (void)storage_request_remount();
    return UI_PAGE_EVENT_NONE;
  }

  if (evt == UI_EVT_DEC)
  {
    (void)storage_request_test();
    return UI_PAGE_EVENT_NONE;
  }

  if (evt == UI_EVT_INC)
  {
    (void)storage_request_list("/");
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

static void page_storage_audio_enter(void)
{
  s_audio_index = 0U;
  s_audio_scroll = 0U;
  s_audio_seq_last = storage_audio_list_seq();
  (void)storage_request_audio_list();
}

static uint32_t page_storage_audio_event(ui_evt_t evt)
{
  if (evt == UI_EVT_BACK)
  {
    return UI_PAGE_EVENT_BACK;
  }

  if ((evt == UI_EVT_NAV_UP) || (evt == UI_EVT_NAV_DOWN))
  {
    uint32_t count = storage_audio_list_count();
    if (count > 0U)
    {
      if (evt == UI_EVT_NAV_UP)
      {
        if (s_audio_index == 0U)
        {
          s_audio_index = (uint8_t)(count - 1U);
        }
        else
        {
          s_audio_index--;
        }
      }
      else
      {
        s_audio_index = (uint8_t)((s_audio_index + 1U) % count);
      }
      return UI_PAGE_EVENT_RENDER;
    }
  }
  else if (evt == UI_EVT_SELECT)
  {
    storage_audio_entry_t entry;
    if (storage_audio_list_get(s_audio_index, &entry) != 0U)
    {
      char path[STORAGE_PATH_MAX];
      (void)snprintf(path, sizeof(path), "/audio/%s", entry.name);
      const sound_registry_entry_t *sound = sound_registry_get_by_path(path);
      if (sound != NULL)
      {
        sound_play(sound->id);
      }
    }
    return UI_PAGE_EVENT_NONE;
  }
  else if ((evt == UI_EVT_DEC) || (evt == UI_EVT_INC))
  {
    (void)storage_request_audio_list();
    return UI_PAGE_EVENT_NONE;
  }

  if (evt == UI_EVT_TICK)
  {
    uint32_t seq = storage_audio_list_seq();
    if (seq != s_audio_seq_last)
    {
      s_audio_seq_last = seq;
      uint32_t count = storage_audio_list_count();
      if (count == 0U)
      {
        s_audio_index = 0U;
        s_audio_scroll = 0U;
      }
      else if (s_audio_index >= count)
      {
        s_audio_index = (uint8_t)(count - 1U);
      }
      return UI_PAGE_EVENT_RENDER;
    }
  }

  return UI_PAGE_EVENT_NONE;
}

static void page_storage_render_info(void)
{
  storage_status_t status;
  storage_get_status(&status);

  renderFill(false);
  renderDrawText(4U, 4U, "STORAGE INFO", RENDER_LAYER_UI, RENDER_STATE_BLACK);

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

  uint16_t height = renderGetHeight();
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 4U));
    renderDrawText(4U, hint_y, "L: TEST  R: LIST", RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(4U, (uint16_t)(hint_y + FONT8X8_HEIGHT + 2U),
                   "A: REMOUNT  B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

static void page_storage_render_audio(void)
{
  renderFill(false);
  renderDrawText(4U, 4U, "AUDIO FILES", RENDER_LAYER_UI, RENDER_STATE_BLACK);

  uint16_t top = (uint16_t)(FONT8X8_HEIGHT + 8U);
  uint16_t line_step = (uint16_t)(FONT8X8_HEIGHT + 2U);
  uint16_t height = renderGetHeight();
  uint16_t bottom_reserved = 0U;
  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    bottom_reserved = (uint16_t)(FONT8X8_HEIGHT * 2U + 4U);
  }

  uint16_t max_rows = 1U;
  if (height > (uint16_t)(top + bottom_reserved))
  {
    max_rows = (uint16_t)((height - top - bottom_reserved) / line_step);
    if (max_rows == 0U)
    {
      max_rows = 1U;
    }
  }

  uint32_t count = storage_audio_list_count();
  storage_audio_list_adjust_scroll(count, max_rows);

  if (count == 0U)
  {
    renderDrawText(4U, top, "NO AUDIO", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
  else
  {
    for (uint16_t row = 0U; row < max_rows; ++row)
    {
      uint32_t idx = (uint32_t)s_audio_scroll + row;
      if (idx >= count)
      {
        break;
      }

      storage_audio_entry_t entry;
      if (storage_audio_list_get(idx, &entry) == 0U)
      {
        continue;
      }

      char line[32];
      char size_buf[16];
      format_kb(size_buf, sizeof(size_buf), entry.size);
      (void)snprintf(line, sizeof(line), "%s %s", entry.name, size_buf);

      char path[STORAGE_PATH_MAX];
      (void)snprintf(path, sizeof(path), "/audio/%s", entry.name);
      const sound_registry_entry_t *sound = sound_registry_get_by_path(path);
      if (sound == NULL)
      {
        size_t len = strlen(line);
        if ((len + 3U) < sizeof(line))
        {
          (void)snprintf(&line[len], sizeof(line) - len, " NR");
        }
      }

      uint16_t y = (uint16_t)(top + (row * line_step));
      bool selected = (idx == s_audio_index);
      if (selected)
      {
        uint16_t text_w = ui_text_width(line);
        uint16_t box_w = (uint16_t)(text_w + 4U);
        if (box_w < 10U)
        {
          box_w = 10U;
        }
        renderFillRect(2U, (uint16_t)(y - 1U), box_w,
                       (uint16_t)(FONT8X8_HEIGHT + 2U),
                       RENDER_LAYER_UI, RENDER_STATE_BLACK);
        renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_WHITE);
      }
      else
      {
        renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_STATE_BLACK);
      }
    }
  }

  if (height > (uint16_t)(FONT8X8_HEIGHT + 6U))
  {
    uint16_t hint_y = (uint16_t)(height - (FONT8X8_HEIGHT * 2U + 4U));
    renderDrawText(4U, hint_y, "UP/DN: SELECT  A: PLAY",
                   RENDER_LAYER_UI, RENDER_STATE_BLACK);
    renderDrawText(4U, (uint16_t)(hint_y + FONT8X8_HEIGHT + 2U),
                   "L/R: REFRESH  B: BACK", RENDER_LAYER_UI, RENDER_STATE_BLACK);
  }
}

const ui_page_t PAGE_STORAGE_INFO =
{
  .name = "Storage Info",
  .enter = page_storage_info_enter,
  .event = page_storage_info_event,
  .render = page_storage_render_info,
  .exit = NULL,
  .tick_ms = 250U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};

const ui_page_t PAGE_STORAGE_AUDIO =
{
  .name = "Storage Audio",
  .enter = page_storage_audio_enter,
  .event = page_storage_audio_event,
  .render = page_storage_render_audio,
  .exit = NULL,
  .tick_ms = 250U,
  .flags = UI_PAGE_FLAG_JOY_MENU
};
