#ifndef APP_LED_H
#define APP_LED_H
typedef enum {
    APP_LED_OFF,
    APP_LED_ON,
    APP_LED_BREATHE
} app_led_mode_t;

void breathing_led_init(void);
void breathing_led_poll(void);
void app_led_set_mode(app_led_mode_t mode);
app_led_mode_t app_led_get_mode(void);
#endif
