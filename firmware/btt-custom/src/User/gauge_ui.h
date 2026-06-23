#ifndef _GAUGE_UI_H_
#define _GAUGE_UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define GAUGE_PAGE_COUNT 4

typedef enum {
  GAUGE_PAGE_DASHBOARD = 0,
  GAUGE_PAGE_SENSORS,
  GAUGE_PAGE_DTC,
  GAUGE_PAGE_SETTINGS,
} GaugePage;

void Gauge_Init(void);
void Gauge_Update(void);
void Gauge_SetPage(GaugePage page);
GaugePage Gauge_GetPage(void);
void Gauge_NextPage(void);
void Gauge_PrevPage(void);

#ifdef __cplusplus
}
#endif

#endif
