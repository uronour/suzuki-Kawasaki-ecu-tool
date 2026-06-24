#ifndef _GAUGE_UI_H_
#define _GAUGE_UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define GAUGE_PAGE_COUNT 5

typedef enum {
  GAUGE_PAGE_DASHBOARD = 0,
  GAUGE_PAGE_SENSORS,
  GAUGE_PAGE_DTC,
  GAUGE_PAGE_SETTINGS,
  GAUGE_PAGE_ABOUT,
} GaugePage;

typedef enum {
  DASH_STYLE_OEM = 0,
  DASH_STYLE_RACE,
  DASH_STYLE_FLAT,
  DASH_STYLE_NEO_RETRO,
  DASH_STYLE_COUNT
} DashStyle;

void Gauge_Init(void);
void Gauge_SetDashStyle(DashStyle style);
DashStyle Gauge_GetDashStyle(void);
void Gauge_Update(void);
void Gauge_SetPage(GaugePage page);
GaugePage Gauge_GetPage(void);
void Gauge_NextPage(void);
void Gauge_PrevPage(void);
void Gauge_Press(void);

#ifdef __cplusplus
}
#endif

#endif
