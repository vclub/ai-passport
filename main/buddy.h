#pragma once

#include "bsp_button.h"

#ifdef __cplusplus
extern "C" {
#endif

void buddy_start(void);
void buddy_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);

#ifdef __cplusplus
}
#endif
