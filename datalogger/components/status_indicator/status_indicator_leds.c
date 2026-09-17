// implements
#include "status_indicator.h"

// system includes
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

// project includes
#include "event_loop.h"

#define MS_TO_US(ms) ((ms) * 1000ULL)

#define LED_BLINK_SLOW      1000
#define LED_BLINK_MEDIUM    500
#define LED_BLINK_FAST      200

static const char *TAG = "STATUS_INDICATOR";

static gpio_num_t gpio_red;
static gpio_num_t gpio_green;
static gpio_num_t gpio_yellow;

typedef struct {
    gpio_num_t gpio;
    esp_timer_handle_t timer;
    bool state;
} led_blink_t;

static led_blink_t blink_red;
static led_blink_t blink_green;
static led_blink_t blink_yellow;

static void set_led(led_display_t state, gpio_num_t led_gpio, uint64_t speed);
static void set_led_flashing(gpio_num_t led_gpio, uint64_t speed);
static void set_led_on(gpio_num_t led_gpio);
static void set_led_off(gpio_num_t led_gpio);
static led_blink_t* get_led_struct(gpio_num_t gpio);
static void blink_callback(void* arg);

static void on_state_event(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    status_indicator_display_state((state_id_t) event_id);
}

esp_err_t status_indicator_init(gpio_num_t pin_num_red, gpio_num_t pin_num_yellow, gpio_num_t pin_num_green){
    gpio_red = pin_num_red;
    gpio_yellow = pin_num_yellow;
    gpio_green = pin_num_green;

    blink_red = (led_blink_t){gpio_red, NULL, false};
    blink_green = (led_blink_t){gpio_green, NULL, false};
    blink_yellow = (led_blink_t){gpio_yellow, NULL, false};

    gpio_hold_dis(gpio_red);
    gpio_hold_dis(gpio_green);
    gpio_hold_dis(gpio_yellow);

    gpio_config_t io_conf;

    // config for red led
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << gpio_red);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // config for green led
    io_conf.pin_bit_mask = (1ULL << gpio_green);
    gpio_config(&io_conf);

    // config for yellow led
    io_conf.pin_bit_mask = (1ULL << gpio_yellow);
    gpio_config(&io_conf);

    gpio_set_level(gpio_red, 0);
    gpio_set_level(gpio_green, 0);
    gpio_set_level(gpio_yellow, 0);

    ESP_LOGI(TAG, "Status indicator initialized");
    return esp_event_handler_register_with(app_event_loop, STATE_CHANGED_EVENT, ESP_EVENT_ANY_ID, on_state_event, NULL);;
}

void status_indicator_display_state(state_id_t state) {
    set_led_off(gpio_red);
    set_led_off(gpio_green);
    set_led_off(gpio_yellow);

    led_pattern_t pattern = status_indicator_get_pattern(state);

    set_led(pattern.red, gpio_red, LED_BLINK_MEDIUM);
    set_led(pattern.yellow, gpio_yellow, LED_BLINK_MEDIUM);
    set_led(pattern.green, gpio_green, LED_BLINK_MEDIUM);
}

led_pattern_t status_indicator_get_pattern(state_id_t state) {
    switch (state) {
        case STATE_INITILIZING:
            return (led_pattern_t){LED_DISPLAY_ON, LED_DISPLAY_ON, LED_DISPLAY_ON};
        case STATE_WAIT_TERM15:
            return (led_pattern_t){LED_DISPLAY_ON, LED_DISPLAY_OFF, LED_DISPLAY_OFF};
        case STATE_IMPORT_CONFIG:
            return (led_pattern_t){LED_DISPLAY_OFF, LED_DISPLAY_ON, LED_DISPLAY_OFF};
        case STATE_LOGGING_ACTIVE:
            return (led_pattern_t){LED_DISPLAY_OFF, LED_DISPLAY_OFF, LED_DISPLAY_ON};
        case STATE_LOGGER_IDLE:
            return (led_pattern_t){LED_DISPLAY_OFF, LED_DISPLAY_ON, LED_DISPLAY_ON};
        case STATE_MEMORY_FULL:
            return (led_pattern_t){LED_DISPLAY_BLINK, LED_DISPLAY_OFF, LED_DISPLAY_OFF};
        case STATE_PARSING_CONFIG_ERROR:
            return (led_pattern_t){LED_DISPLAY_OFF, LED_DISPLAY_BLINK, LED_DISPLAY_OFF};
        case STATE_LOG_ERROR:
            return (led_pattern_t){LED_DISPLAY_BLINK, LED_DISPLAY_BLINK, LED_DISPLAY_OFF};
        case STATE_CAN_ERROR:
            return (led_pattern_t){LED_DISPLAY_OFF, LED_DISPLAY_OFF, LED_DISPLAY_BLINK};
        case STATE_UNDEFINED_ERROR:
            return (led_pattern_t){LED_DISPLAY_BLINK, LED_DISPLAY_BLINK, LED_DISPLAY_BLINK};
        default:
            return (led_pattern_t){LED_DISPLAY_OFF, LED_DISPLAY_OFF, LED_DISPLAY_OFF};
    }
}

static led_blink_t* get_led_struct(gpio_num_t gpio)
{
    if (gpio == gpio_red) return &blink_red;
    if (gpio == gpio_green) return &blink_green;
    if (gpio == gpio_yellow) return &blink_yellow;
    return NULL;
}

static void blink_callback(void* arg)
{
    led_blink_t *led = (led_blink_t*) arg;
    led->state = !led->state;
    gpio_set_level(led->gpio, led->state);
}

static void set_led_flashing(gpio_num_t led_gpio, uint64_t speed)
{
    led_blink_t *led = get_led_struct(led_gpio);
    if (!led) return;

    // stop timer if already set
    if (led->timer) {
        esp_timer_stop(led->timer);
        esp_timer_delete(led->timer);
        led->timer = NULL;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &blink_callback,
        .arg = led,
        .name = "led_blink"
    };

    esp_timer_create(&timer_args, &led->timer);
    esp_timer_start_periodic(led->timer, MS_TO_US(speed));
}

static void set_led_on(gpio_num_t led_gpio)
{
    led_blink_t *led = get_led_struct(led_gpio);
    if (led && led->timer) {
        esp_timer_stop(led->timer);
        esp_timer_delete(led->timer);
        led->timer = NULL;
    }
    gpio_set_level(led_gpio, 1);
}

static void set_led_off(gpio_num_t led_gpio)
{
    led_blink_t *led = get_led_struct(led_gpio);
    if (led && led->timer) {
        esp_timer_stop(led->timer);
        esp_timer_delete(led->timer);
        led->timer = NULL;
    }
    gpio_set_level(led_gpio, 0);
}
static void set_led(led_display_t state, gpio_num_t led_gpio, uint64_t speed) {
    switch (state)
    {
    case LED_DISPLAY_ON:
        set_led_on(led_gpio);
        break;
    case LED_DISPLAY_OFF:
        set_led_off(led_gpio);
        break;
    case LED_DISPLAY_BLINK:
        set_led_flashing(led_gpio, speed);
        break;
    }
}

void status_indicator_power_down(void)
{
    set_led_off(gpio_red);
    set_led_off(gpio_green);
    set_led_off(gpio_yellow);

    gpio_hold_en(gpio_red);
    gpio_hold_en(gpio_green);
    gpio_hold_en(gpio_yellow);
}