#ifndef MCU_TEMP_H
#define MCU_TEMP_H
#include "stm32h7xx_hal.h"
/* ADC3 is reserved by this module. Units: degrees C and millivolts. */
HAL_StatusTypeDef mcu_temp_read(int32_t *temperature_c, uint32_t *vdda_mv);
typedef struct {
    const char *stage;
    const char *channel;
    uint32_t kernel_hz, vref_raw, temp_raw, vdda_mv;
    uint32_t cal_vref, cal_t1, cal_t2, revision, adc_error;
    int32_t temperature_c;
} mcu_temp_diag_t;
const mcu_temp_diag_t *mcu_temp_diagnostics(void);
#endif
