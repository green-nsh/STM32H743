#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "shell.h"
#include "app_led.h"

int main(void)
{
    sys_cache_enable();
    HAL_Init();
    sys_stm32_clock_init(192, 5, 2, 4);
    delay_init(480);
    breathing_led_init();
    usart_init(1000000);
    shell_init();
    
    while (1) {
        shell_poll();
        breathing_led_poll();
    }
}
