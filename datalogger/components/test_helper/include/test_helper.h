#pragma once

#include "unity.h"
#include "esp_err.h"
#include "nvs.h"

#define TEST_ASSERT_ESP_ERR(expected, expr)                           \
    do {                                                              \
        esp_err_t err = (expr);                                       \
        char msg[128];                                                \
        snprintf(msg, sizeof(msg),                                    \
                 "%s failed with err: %s",                            \
                 #expr,                                               \
                 esp_err_to_name(err));                               \
        TEST_ASSERT_EQUAL_MESSAGE(expected, err, msg);                \
    } while (0)

char *build_err_msg(const char *name, esp_err_t result, esp_err_t expected);
