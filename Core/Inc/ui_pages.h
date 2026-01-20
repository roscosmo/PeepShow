#ifndef UI_PAGES_H
#define UI_PAGES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_EVT_NONE = 0,
  UI_EVT_TICK = 1,
  UI_EVT_NAV_UP = 2,
  UI_EVT_NAV_DOWN = 3,
  UI_EVT_NAV_LEFT = 4,
  UI_EVT_NAV_RIGHT = 5,
  UI_EVT_SELECT = 6,
  UI_EVT_BACK = 7,
  UI_EVT_DEC = 8,
  UI_EVT_INC = 9
} ui_evt_t;

#define UI_PAGE_EVENT_NONE   (0U)
#define UI_PAGE_EVENT_RENDER (1U << 0U)
#define UI_PAGE_EVENT_BACK   (1U << 1U)
#define UI_PAGE_EVENT_HANDLED (1U << 2U)

#define UI_PAGE_FLAG_JOY_MENU    (1U << 0U)
#define UI_PAGE_FLAG_JOY_MONITOR (1U << 1U)
#define UI_PAGE_FLAG_POWER_STATS (1U << 2U)
#define UI_PAGE_FLAG_LIS2        (1U << 3U)
#define UI_PAGE_FLAG_LIS2_STEP   (1U << 4U)

typedef uint32_t (*ui_page_event_fn)(ui_evt_t evt);

typedef struct
{
  const char *name;
  void (*enter)(void);
  ui_page_event_fn event;
  void (*render)(void);
  void (*exit)(void);
  uint16_t tick_ms;
  uint16_t flags;
} ui_page_t;

extern const ui_page_t PAGE_MENU;
extern const ui_page_t PAGE_HOME;
extern const ui_page_t PAGE_JOY_CAL;
extern const ui_page_t PAGE_JOY_TARGET;
extern const ui_page_t PAGE_JOY_CURSOR;
extern const ui_page_t PAGE_SOUND;
extern const ui_page_t PAGE_MENU_INPUT;
extern const ui_page_t PAGE_BATT_STATS;
extern const ui_page_t PAGE_STORAGE_INFO;
extern const ui_page_t PAGE_STORAGE_AUDIO;
extern const ui_page_t PAGE_SEED_AUDIO;
extern const ui_page_t PAGE_SLEEP;
extern const ui_page_t PAGE_RTC_SET;
extern const ui_page_t PAGE_LIS2_IMU;
extern const ui_page_t PAGE_LIS2_STEPS;

#ifdef __cplusplus
}
#endif

#endif /* UI_PAGES_H */
