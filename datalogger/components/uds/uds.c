// implements
#include "uds.h"

// system includes
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// library includes
#include "isotp.h"

// project includes
#include "can_backend.h"
#include "uds_decoder.h"

#define TAG "uds"

#define ISOTP_BUFSIZE 256

static IsoTpLink link;

/* Alloc send and receive buffer statically in RAM */
static uint8_t isotpRecvBuf[ISOTP_BUFSIZE];
static uint8_t isotpSendBuf[ISOTP_BUFSIZE];

esp_err_t send_uds(const uint32_t canId, const uint32_t serviceId, const uint8_t *requestParams, const uint8_t requestParamsSize, 
                    const uint32_t responseCanId, const uint32_t uds_timeout_ms, const uint32_t can_timeout_ms, UdsResponse *response)
{
    if (!requestParams || !response) return ESP_ERR_INVALID_ARG;
    esp_err_t result;

    can_backend_flush();
    isotp_init_link(&link, canId, isotpSendBuf, sizeof(isotpSendBuf), isotpRecvBuf, sizeof(isotpRecvBuf));

    can_frame_t rx_frame;
    uint8_t payload[requestParamsSize + 1];
    payload[0] = serviceId;
    memcpy(&payload[1], requestParams, requestParamsSize);

    int ret = isotp_send(&link, payload, requestParamsSize + 1);
    if (ret == ISOTP_RET_OK) {
        ESP_LOGI(TAG, "isotp_send OK - Frame tranceived for request params [%d,%d]", requestParams[0], requestParams[1]);
    } else {
        switch (ret) {
            case ISOTP_RET_INPROGRESS:
                ESP_LOGE(TAG, "isotp_send FAILED: send already in progress");
                result = ESP_ERR_INVALID_STATE;
                goto done;
            case ISOTP_RET_OVERFLOW:
                ESP_LOGE(TAG, "isotp_send FAILED: buffer overflow");
                result = ESP_ERR_NO_MEM;
                goto done;
            case ISOTP_RET_ERROR:
                ESP_LOGE(TAG, "isotp_send FAILED: general error");
                result = ESP_FAIL;
                goto done;
            default:
                ESP_LOGE(TAG, "isotp_send FAILED: unknown error code %d", ret);
                result = ESP_FAIL;
                goto done;
        }
    }

    TickType_t start = xTaskGetTickCount();
    TickType_t deadline = start + pdMS_TO_TICKS(uds_timeout_ms);

    while (1) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            ESP_LOGW(TAG, "uds request timed out after %"PRIu32" ms (canId=%"PRIu32", responseCanId=%"PRIu32")", uds_timeout_ms, canId, responseCanId);
            result = ESP_ERR_TIMEOUT;
            goto done;
        }
        uint32_t remaining_ms = pdTICKS_TO_MS(deadline - now);
        uint32_t current_can_timeout_ms = (remaining_ms < can_timeout_ms) ? remaining_ms : can_timeout_ms;
        if (current_can_timeout_ms == 0) current_can_timeout_ms = 1; // never poll with a 0ms timeout

        esp_err_t err = can_backend_receive(&rx_frame, current_can_timeout_ms);
        if (err == ESP_ERR_TIMEOUT) {
            // No frame within this slice - loop back and let the deadline check above decide whether the overall uds_timeout_ms budget is exhausted.
            continue;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "can backend receive failed with error %s", esp_err_to_name(err));
            result = err;
            goto done;
        }

        if (link.receive_protocol_result != ISOTP_PROTOCOL_RESULT_OK) {
            result = ESP_FAIL;
            goto done;
        }

        if (rx_frame.id != responseCanId) {
            // filter only for relevant frames, otherwise every frame on the CAN Bus would be processed
            continue;
        }

        isotp_on_can_message(&link, rx_frame.data, rx_frame.dlc);

        // Drain all consecutive frames to avoid a deadlock, where the Datalogger and the ECU wait endless for concecutive frames.
        while (link.send_status == ISOTP_SEND_STATUS_INPROGRESS) {
            isotp_poll(&link);
        }

        if (!isotp_received_all_data(&link)) {
            continue;
        }

        if (link.receive_size > sizeof(response->data)) {
            ESP_LOGE(TAG, "Response too large for buffer: received=%"PRIu16" buffer=%zu", link.receive_size, sizeof(response->data));
            result = ESP_ERR_NO_MEM;
            goto done;
        }

        uint16_t received_size;
        isotp_collect_payload(&link, response->data, sizeof(response->data), &received_size);
        response->size = received_size;

        uint8_t nrc = 0;
        if (uds_is_negative_response(response->data, response->size, &nrc)) {
            if (nrc == UDS_NRC_RESPONSE_PENDING) {
                ESP_LOGI(TAG, "UDS response pending (NRC 0x78) - waiting for final response");
                continue;
            }
            ESP_LOGE(TAG, "UDS NEGATIVE RESPONSE: SID=0x%02X NRC=0x%02X", response->size > 1 ? response->data[1] : 0, nrc);
        } else {
            // Positive response: verify it actually answers this request 
            uint8_t expected_positive_sid = (uint8_t)(serviceId + 0x40);
            bool sid_mismatch = response->data[0] != expected_positive_sid;
            bool did_mismatch = requestParamsSize >= 2 && response->size >= 3 && (response->data[1] != requestParams[0] || response->data[2] != requestParams[1]);
            if (sid_mismatch || did_mismatch) {
                ESP_LOGW(TAG, "uds response does not match request (expected SID=0x%02X, got SID=0x%02X) - discarding stale/foreign frame", expected_positive_sid, response->data[0]);
                result = ESP_FAIL;
                goto done;
            }
        }
        result = ESP_OK;
        goto done;
    }

done:
    response->status = result;
    return result;
}
