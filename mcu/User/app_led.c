#include "./SYSTEM/sys/sys.h"
#include "app_led.h"
#include <math.h>
#include <string.h>

TIM_HandleTypeDef htim3;
static float sine_angle;
static uint8_t breathing = 1;
#define TWO_PI 6.28318530718f

void breathing_led_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef channel = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &gpio);
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 480 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(&htim3);
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 0;
    channel.OCPolarity = TIM_OCPOLARITY_LOW;
    channel.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &channel, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

void app_led_set_mode(app_led_mode_t mode)
{
    if (mode == APP_LED_BREATHE) breathing = 1;
    else if (mode == APP_LED_ON) { breathing = 0; TIM3->CCR3 = 1000; }
    else if (mode == APP_LED_OFF) { breathing = 0; TIM3->CCR3 = 0; }
}

app_led_mode_t app_led_get_mode(void)
{
    return breathing ? APP_LED_BREATHE : (TIM3->CCR3 ? APP_LED_ON : APP_LED_OFF);
}

void breathing_led_poll(void)
{
    static uint32_t last_tick;
    uint32_t now = HAL_GetTick();
    if (breathing && (uint32_t)(now - last_tick) >= 5U) {
        last_tick = now;
        TIM3->CCR3 = (uint16_t)((sinf(sine_angle) + 1.0f) * 500.0f);
        sine_angle += 0.05f;
        if (sine_angle >= TWO_PI) sine_angle -= TWO_PI;
    }
}