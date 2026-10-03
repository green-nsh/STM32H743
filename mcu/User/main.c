#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include <math.h>

TIM_HandleTypeDef htim3;
float sine_angle = 0.0f;
#define PI        3.14159265359f
#define TWO_PI   (2.0f * PI)

/* ====== 初始化 PWM ====== */
void breathing_led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin       = GPIO_PIN_0;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 480 - 1;        /* 改大了 */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 1000 - 1;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(&htim3);

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;     /* 改成低电平点亮 */
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}
/* ====== 正弦波呼吸灯主循环 ====== */
void breathing_led_main(void)
{
    float angle_step = 0.05f;  /* 调整呼吸速度 */

    while (1)
    {
        float sine_value = sinf(sine_angle);
        uint16_t brightness = (uint16_t)((sine_value + 1.0f) / 2.0f * 1000);

        TIM3->CCR3 = brightness;  /* PB0 = TIM3_CH3，写 CCR3 */

        sine_angle += angle_step;
        if (sine_angle > TWO_PI) sine_angle -= TWO_PI;

        delay_ms(5);
    }
}

/* ====== main 函数 ====== */
int main(void)
{
    sys_cache_enable();
    HAL_Init();
    sys_stm32_clock_init(192, 5, 2, 4);  /* 480MHz */
    delay_init(480);
	
#ifdef LED_TEST
	int a = 5;
#endif


    breathing_led_init();   /* 初始化 PB0 PWM */
    breathing_led_main();   /* 启动呼吸灯 */

    return 0;
}




