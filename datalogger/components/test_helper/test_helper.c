#include "test_helper.h"

#include <stdio.h>

char *build_err_msg(const char *name, esp_err_t result, esp_err_t expected)
{
    static char msg[128];
    snprintf(msg, sizeof(msg), "%s failed with err: %s, expected: %s", name, esp_err_to_name(result), esp_err_to_name(expected));
    return msg;
}
