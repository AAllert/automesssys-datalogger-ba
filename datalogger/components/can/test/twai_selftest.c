/* TWAI Self Test Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 * The following example demonstrates the self testing capabilities of the TWAI
 * peripheral by utilizing the No Acknowledgment Mode and Self Reception Request
 * capabilities. This example can be used to verify that the TWAI peripheral and
 * its connections to the external transceiver operates without issue. The example
 * will execute multiple iterations, each iteration will do the following:
 * 1) Start the TWAI driver
 * 2) Transmit and receive 100 messages using self reception request
 * 3) Stop the TWAI driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "twai_selftest.h"
/* --------------------- Definitions and static variables ------------------ */

//Example Configurations
#define NO_OF_MSGS              30
#define NO_OF_ITERS             2
#define TX_GPIO_NUM             16
#define RX_GPIO_NUM             17
#define TX_TASK_PRIO            8       //Sending task priority
#define RX_TASK_PRIO            9       //Receiving task priority
#define CTRL_TSK_PRIO           10      //Control task priority
#define MSG_ID                  0x555   //11 bit standard format ID
#define EXAMPLE_TAG             "TWAI Self Test"

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
//Filter all other IDs except MSG_ID
static const twai_filter_config_t f_config = {.acceptance_code = (MSG_ID << 21),
                                             .acceptance_mask = ~(TWAI_STD_ID_MASK << 21),
                                             .single_filter = true};
//Set to NO_ACK mode due to self testing with single module
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NO_ACK);

static SemaphoreHandle_t tx_sem;
static SemaphoreHandle_t rx_sem;
static SemaphoreHandle_t ctrl_sem;
static SemaphoreHandle_t done_sem;
#define portMAX_DELAY_custom 300

//Shared first-error state, set by whichever task/call fails first
static volatile esp_err_t s_selftest_err;

static void record_error(esp_err_t err, const char *what)
{
    ESP_LOGE(EXAMPLE_TAG, "%s failed: %s", what, esp_err_to_name(err));
    if (s_selftest_err == ESP_OK) {
        s_selftest_err = err;
    }
}
/* --------------------------- Tasks and Functions -------------------------- */

static void twai_transmit_task(void *arg)
{
    twai_message_t tx_msg = {.data_length_code = 1, .identifier = MSG_ID, .self = 1};
    for (int iter = 0; iter < NO_OF_ITERS; iter++) {
        xSemaphoreTake(tx_sem, portMAX_DELAY_custom);
        bool failed = false;
        for (int i = 0; i < NO_OF_MSGS; i++) {
            //Transmit messages using self reception request
            tx_msg.data[0] = i;
            esp_err_t err = twai_transmit(&tx_msg, portMAX_DELAY_custom);
            if (err != ESP_OK) {
                record_error(err, "twai_transmit");
                failed = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (failed) {
            break;
        }
    }
    vTaskDelete(NULL);
}

static void twai_receive_task(void *arg)
{
    twai_message_t rx_message;

    for (int iter = 0; iter < NO_OF_ITERS; iter++) {
        xSemaphoreTake(rx_sem, portMAX_DELAY_custom);
        bool failed = false;
        for (int i = 0; i < NO_OF_MSGS; i++) {
            //Receive message and print message data
            esp_err_t err = twai_receive(&rx_message, portMAX_DELAY_custom);
            if (err != ESP_OK) {
                record_error(err, "twai_receive");
                failed = true;
                break;
            }
        }
        //Indicate to control task this iteration is done (success or failure)
        if (!failed) {
            ESP_LOGI(EXAMPLE_TAG, "All messages received for iteration %d", iter);
        }
        xSemaphoreGive(ctrl_sem);
        if (failed) {
            break;
        }
    }
    vTaskDelete(NULL);
}

static void twai_control_task(void *arg)
{
    xSemaphoreTake(ctrl_sem, portMAX_DELAY_custom);
    for (int iter = 0; iter < NO_OF_ITERS; iter++) {
        //Start TWAI Driver for this iteration
        esp_err_t err = twai_start();
        if (err != ESP_OK) {
            record_error(err, "twai_start");
            //Release TX/RX tasks so they don't block for a full timeout, they will fail fast since the driver never started.
            xSemaphoreGive(rx_sem);
            xSemaphoreGive(tx_sem);
            xSemaphoreTake(ctrl_sem, portMAX_DELAY_custom);    //Sync with RX task's exit
            break;
        }
        ESP_LOGI(EXAMPLE_TAG, "Driver started");

        //Trigger TX and RX tasks to start transmitting/receiving
        xSemaphoreGive(rx_sem);
        xSemaphoreGive(tx_sem);
        xSemaphoreTake(ctrl_sem, portMAX_DELAY_custom);    //Wait for TX and RX tasks to finish iteration

        err = twai_stop();                          //Stop the TWAI Driver
        if (err != ESP_OK) {
            record_error(err, "twai_stop");
            break;
        }
        ESP_LOGI(EXAMPLE_TAG, "Driver stopped");
        vTaskDelay(pdMS_TO_TICKS(100));             //Delay then start next iteration
    }
    xSemaphoreGive(done_sem);
    vTaskDelete(NULL);
}

esp_err_t twai_selftest(void)
{
    s_selftest_err = ESP_OK;
    bool driver_installed = false;

    //Create tasks and synchronization primitives
    tx_sem = xSemaphoreCreateBinary();
    rx_sem = xSemaphoreCreateBinary();
    ctrl_sem = xSemaphoreCreateBinary();
    done_sem = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(twai_control_task, "TWAI_ctrl", 4096, NULL, CTRL_TSK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 4096, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(twai_transmit_task, "TWAI_tx", 4096, NULL, TX_TASK_PRIO, NULL, tskNO_AFFINITY);

    //Install TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        record_error(err, "twai_driver_install");
    } else {
        driver_installed = true;
        ESP_LOGI(EXAMPLE_TAG, "Driver installed");
    }

    //Start control task (it will unwind gracefully even if install failed above)
    xSemaphoreGive(ctrl_sem);
    //Wait for all iterations and tasks to complete running
    xSemaphoreTake(done_sem, portMAX_DELAY_custom);

    //Uninstall TWAI driver, only if it was actually installed
    if (driver_installed) {
        err = twai_driver_uninstall();
        if (err != ESP_OK) {
            record_error(err, "twai_driver_uninstall");
        } else {
            ESP_LOGI(EXAMPLE_TAG, "Driver uninstalled");
        }
    }

    //Cleanup
    vSemaphoreDelete(tx_sem);
    vSemaphoreDelete(rx_sem);
    vSemaphoreDelete(ctrl_sem);
    vQueueDelete(done_sem);
    return (s_selftest_err == ESP_OK) ? ESP_OK : ESP_FAIL;
}

