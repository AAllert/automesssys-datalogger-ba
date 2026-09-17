export class DataloggerApi {
    constructor() {
        this.hostname = "http://automessy.local";
        this.defaultTimeoutMs = 5000;
    }

    /**
     * @param {string} path
     * @param {object} [options] Standard fetch options, plus an optional
     *        `timeoutMs` to override the default request timeout (e.g. for
     *        slow file transfers).
     */
    async request(path, { timeoutMs = this.defaultTimeoutMs, ...options } = {}) {
        const isJsonBody = options.body
            && typeof options.body === "object"
            && !(options.body instanceof Blob)
            && !(options.body instanceof ArrayBuffer)
            && !(options.body instanceof FormData)
            && !ArrayBuffer.isView(options.body);

        if (isJsonBody) {
            options.headers ??= {};
            options.headers["Content-Type"] = "application/json";
            options.body = JSON.stringify(options.body);
        }

        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
        options.signal = controller.signal;

        let response;
        try {
            response = await fetch(this.hostname + path, options);
        } catch {
            // Covers both network errors and the abort triggered by the timeout.
            return {
                status: 0,
                ok: false,
                headers: null,
                data: null
            };
        } finally {
            clearTimeout(timeoutId);
        }

        let data = null;
        const contentType = response.headers.get("Content-Type");
        if (contentType?.includes("application/json")) {
            data = await response.json();
        } else if (response.ok) {
            // binary bodies (file downloads) are handed back as a Blob
            data = await response.blob();
        }

        return {
            status: response.status,
            ok: response.ok,
            headers: response.headers,
            data: data
        };
    }

    // configuration

    async getActiveConfig() {
        return this.request("/active-uds-config", {
            method: "GET"
        });
    }

    async setActiveConfig(configFilename) {
        return this.request("/active-uds-config", {
            method: "PUT",
            body: {
                config_file: configFilename
            }
        });
    }

    async getSystemTime() {
        return this.request("/time", {
            method: "GET"
        });
    }

    async setSystemTime(timestamp) {
        return this.request("/time", {
            method: "PUT",
            body: {
                timestamp: timestamp
            }
        });
    }

    async syncSystemTime() {
        return this.setSystemTime(Math.floor(Date.now() / 1000));
    }

    async getCanTimeout() {
        return this.request("/can-timeout", {
            method: "GET"
        });
    }

    async setCanTimeout(canTimeout) {
        return this.request("/can-timeout", {
            method: "PUT",
            body: {
                can_timeout: canTimeout
            }
        });
    }

    async getUdsTimeout() {
        return this.request("/uds-timeout", {
            method: "GET"
        });
    }

    async setUdsTimeout(udsTimeout) {
        return this.request("/uds-timeout", {
            method: "PUT",
            body: {
                uds_timeout: udsTimeout
            }
        });
    }

    async getDeepsleepTimeout() {
        return this.request("/deepsleep-timeout", {
            method: "GET"
        });
    }

    async setDeepsleepTimeout(deepsleepTimeout) {
        return this.request("/deepsleep-timeout", {
            method: "PUT",
            body: {
                deepsleep_timeout: deepsleepTimeout
            }
        });
    }

    async getTerm15RequestInterval() {
        return this.request("/term15-request-interval", {
            method: "GET"
        });
    }

    async setTerm15RequestInterval(requestInterval) {
        return this.request("/term15-request-interval", {
            method: "PUT",
            body: {
                request_interval: requestInterval
            }
        });
    }

    // datalogger

    async startLogging(outputFile) {
        return this.request("/datalogger/start", {
            method: "POST",
            body: outputFile ? { output_file: outputFile } : undefined
        });
    }

    async stopLogging() {
        return this.request("/datalogger/stop", {
            method: "POST"
        });
    }

    // file

    /**
     * Normalizes user-supplied file paths to exactly one leading slash.
     * @param path The path to normalize
     * @return the normalized path
     */ 
    filePath(path) {
        return "/" + path.replace(/^\/+/, "");
    }

    /**
     * @param {number} [offset] Index of the first entry to return.
     * @param {number} [limit] Max. number of entries to return (server-side capped at 100).
     */
    async listConfigs(offset = 0, limit = 20) {
        return this.request(`/configs?offset=${offset}&limit=${limit}`, {
            method: "GET"
        });
    }

    /**
     * @param {number} [offset] Index of the first entry to return.
     * @param {number} [limit] Max. number of entries to return (server-side capped at 100).
     */
    async listRawLogs(offset = 0, limit = 20) {
        return this.request(`/logs?offset=${offset}&limit=${limit}`, {
            method: "GET"
        });
    }

    /**
     * @param {number} [offset] Index of the first entry to return.
     * @param {number} [limit] Max. number of entries to return (server-side capped at 100).
     */
    async listCsvLogs(offset = 0, limit = 20) {
        return this.request(`/csv-logs?offset=${offset}&limit=${limit}`, {
            method: "GET"
        });
    }

    async downloadFile(filepath) {
        return this.request(this.filePath(filepath), {
            method: "GET",
            timeoutMs: 60000     // file transfers can take longer than typical status requests
        });
    }

    async uploadFile(filepath, file) {
        return this.request(this.filePath(filepath), {
            method: "POST",
            headers: {
                "Content-Type": "application/octet-stream"
            },
            body: file,
            timeoutMs: 60000     // file transfers can take longer than typical status requests
        });
    }

    async deleteFile(filepath) {
        return this.request(this.filePath(filepath), {
            method: "DELETE"
        });
    }

    // ota

    async otaUpdate(firmware) {
        return this.request("/ota", {
            method: "POST",
            headers: {
                "Content-Type": "application/octet-stream"
            },
            body: firmware,
            timeoutMs: 120000     // firmware images are larger still and flashing takes time
        });
    }
}
