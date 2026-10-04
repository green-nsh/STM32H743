#include "mcu_temp.h"
#include "./SYSTEM/delay/delay.h"

#define TEMP_SAMPLES 16U
static ADC_HandleTypeDef adc3;
static uint8_t initialized;
static mcu_temp_diag_t diagnostic;
const mcu_temp_diag_t *mcu_temp_diagnostics(void) { return &diagnostic; }

static HAL_StatusTypeDef temp_adc_init(void)
{
    uint32_t kernel_hz;
    HAL_StatusTypeDef status;
    if (initialized) return HAL_OK;
    /* sys_stm32_clock_init already runs PLL2 at 220 MHz. ADC uses PLL2 P. */
    diagnostic.stage = "PLL2-not-ready";
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PLL2RDY) == RESET) return HAL_ERROR;
    __HAL_RCC_PLL2CLKOUT_ENABLE(RCC_PLL2_DIVP);
    __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_PLL2);
    kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ADC);
    /* At current clocks: 220 MHz / 16 = 13.75 MHz; sampling takes ~59 us. */
    diagnostic.kernel_hz = kernel_hz;
    diagnostic.stage = "ADC-clock-range";
    if (!kernel_hz || kernel_hz / 16U > 20000000U) return HAL_ERROR;
    __HAL_RCC_ADC3_CLK_ENABLE();
    adc3.Instance = ADC3;
    adc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV16;
    adc3.Init.Resolution = ADC_RESOLUTION_16B;
    adc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
    adc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    adc3.Init.LowPowerAutoWait = DISABLE;
    adc3.Init.ContinuousConvMode = DISABLE;
    adc3.Init.NbrOfConversion = 1;
    adc3.Init.DiscontinuousConvMode = DISABLE;
    adc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    adc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    adc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    adc3.Init.OversamplingMode = DISABLE;
    diagnostic.stage = "ADC-init";
    status = HAL_ADC_Init(&adc3);
    if (status == HAL_OK) {
        diagnostic.stage = "ADC-calibration";
        status = HAL_ADCEx_Calibration_Start(&adc3, ADC_CALIB_OFFSET_LINEARITY,
                                              ADC_SINGLE_ENDED);
    }
    if (status != HAL_OK) {
        diagnostic.adc_error = HAL_ADC_GetError(&adc3);
        HAL_ADC_DeInit(&adc3);
        return status;
    }
    initialized = 1;
    return HAL_OK;
}

static HAL_StatusTypeDef read_channel(uint32_t channel, uint32_t *average)
{
    ADC_ChannelConfTypeDef config = {0};
    HAL_StatusTypeDef status;
    uint32_t i, sum = 0;
    diagnostic.channel = channel == ADC_CHANNEL_VREFINT ? "VREFINT" : "TEMPSENSOR";
    config.Channel = channel;
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
    config.SingleDiff = ADC_SINGLE_ENDED;
    config.OffsetNumber = ADC_OFFSET_NONE;
    config.Offset = 0;
    /* ConfigChannel enables TSEN/VREFEN. No external GPIO is used. */
    diagnostic.stage = "channel-config";
    status = HAL_ADC_ConfigChannel(&adc3, &config);
    if (status != HAL_OK) return status;
    delay_us(100); /* Allow internal sensor/reference to stabilize. */
    /* Discard the first conversion after switching channels. */
    for (i = 0; i <= TEMP_SAMPLES; ++i) {
        uint32_t raw;
        diagnostic.stage = "conversion-start";
        status = HAL_ADC_Start(&adc3);
        if (status != HAL_OK) return status;
        diagnostic.stage = "conversion-poll";
        status = HAL_ADC_PollForConversion(&adc3, 10);
        if (status != HAL_OK) { diagnostic.adc_error = HAL_ADC_GetError(&adc3); HAL_ADC_Stop(&adc3); return status; }
        raw = HAL_ADC_GetValue(&adc3);
        diagnostic.stage = "conversion-stop";
        status = HAL_ADC_Stop(&adc3);
        if (status != HAL_OK) return status;
        if (i) sum += raw;
    }
    *average = (sum + TEMP_SAMPLES / 2U) / TEMP_SAMPLES;
    return HAL_OK;
}

HAL_StatusTypeDef mcu_temp_read(int32_t *temperature_c, uint32_t *vdda_mv)
{
    HAL_StatusTypeDef status;
    uint32_t temp_raw, vref_raw, voltage;
    int32_t temperature;
    diagnostic.stage = "arguments";
    diagnostic.channel = "none";
    diagnostic.vref_raw = diagnostic.temp_raw = diagnostic.vdda_mv = 0;
    diagnostic.temperature_c = 0;
    diagnostic.adc_error = 0;
    diagnostic.cal_vref = *VREFINT_CAL_ADDR;
    diagnostic.cal_t1 = *TEMPSENSOR_CAL1_ADDR;
    diagnostic.cal_t2 = *TEMPSENSOR_CAL2_ADDR;
    diagnostic.revision = DBGMCU->IDCODE;
    if (!temperature_c || !vdda_mv) return HAL_ERROR;
    /* Check calibration data before calling macros that divide by it. */
    diagnostic.stage = "factory-calibration-data";
    if (*VREFINT_CAL_ADDR == 0U || *VREFINT_CAL_ADDR == 0xffffU ||
        *TEMPSENSOR_CAL1_ADDR == 0U || *TEMPSENSOR_CAL1_ADDR == 0xffffU ||
        *TEMPSENSOR_CAL2_ADDR == 0U || *TEMPSENSOR_CAL2_ADDR == 0xffffU ||
        *TEMPSENSOR_CAL1_ADDR == *TEMPSENSOR_CAL2_ADDR) return HAL_ERROR;
    status = temp_adc_init();
    if (status != HAL_OK) return status;
    status = read_channel(ADC_CHANNEL_VREFINT, &vref_raw);
    if (status == HAL_OK) diagnostic.vref_raw = vref_raw;
    if (status == HAL_OK) status = read_channel(ADC_CHANNEL_TEMPSENSOR, &temp_raw);
    if (status != HAL_OK) {
        if (!diagnostic.adc_error) diagnostic.adc_error = HAL_ADC_GetError(&adc3);
        HAL_ADC_DeInit(&adc3);
        initialized = 0; /* Retry initialization on the next command. */
        return status;
    }
    diagnostic.temp_raw = temp_raw;
    diagnostic.stage = "raw-data-range";
    if (!vref_raw || vref_raw >= 65535U || !temp_raw || temp_raw >= 65535U)
        return HAL_ERROR;
    voltage = __HAL_ADC_CALC_VREFANALOG_VOLTAGE(vref_raw, ADC_RESOLUTION_16B);
    diagnostic.vdda_mv = voltage;
    diagnostic.stage = "VREF-voltage-range";
    if (voltage < 1800U || voltage > 3600U) return HAL_ERROR;
    /* HAL selects CAL2 = 110 or 130 C according to the silicon revision. */
    temperature = __HAL_ADC_CALC_TEMPERATURE(voltage, temp_raw, ADC_RESOLUTION_16B);
    diagnostic.temperature_c = temperature;
    diagnostic.stage = "temperature-range";
    if (temperature < -40 || temperature > 150) return HAL_ERROR;
    diagnostic.stage = "OK";
    *temperature_c = temperature;
    *vdda_mv = voltage;
    return HAL_OK;
}
