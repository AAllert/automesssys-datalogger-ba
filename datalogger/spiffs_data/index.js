import { DataloggerApi } from "./datalogger-api.js";
import { UIPresenter } from "./presenter.js";
import { FileTable } from "./tables.js";

const api = new DataloggerApi();
const presenter = new UIPresenter();

/**
 * Registry of selectable file types for the file table dropdown.
 *
 * - kind: path prefix used for download/delete/upload requests ("configs" | "logs").
 * - list: fetches the entries for this file type.
 * - columns: table columns shown for this file type, in order.
 * - showUpload: whether the round upload button is shown for this file type.
 */
const FILE_TYPES = {
    configs: {
        label: "Konfigurationsdateien",
        kind: "configs",
        list: (offset, limit) => api.listConfigs(offset, limit),
        columns: [
            { key: "filename", label: "Dateiname" },
            { key: "size", label: "Größe (KB)" }
            // { key: "vehicle", label: "Fahrzeug" },
            // { key: "values", label: "Werte" }
        ],
        showUpload: true
    },
    "logs-log": {
        label: "Log-Dateien (.log)",
        kind: "logs",
        list: (offset, limit) => api.listRawLogs(offset, limit),
        columns: [
            { key: "filename", label: "Dateiname" },
            { key: "size", label: "Größe (KB)" }
            // { key: "vehicle", label: "Fahrzeug" },
            // { key: "cycles", label: "Zyklen" },
            // { key: "values", label: "Werte" }
        ],
        showUpload: false
    },
    "logs-csv": {
        label: "Log-Dateien (.csv)",
        kind: "logs",
        list: (offset, limit) => api.listCsvLogs(offset, limit),
        columns: [
            { key: "filename", label: "Dateiname" },
            { key: "size", label: "Größe (KB)" }
        ],
        showUpload: false
    }
};

const fileTypeSelect = document.getElementById("file-type-select");
Object.entries(FILE_TYPES).forEach(([key, type]) => {
    const option = document.createElement("option");
    option.value = key;
    option.textContent = type.label;
    fileTypeSelect.appendChild(option);
});

let currentFileTypeKey = fileTypeSelect.value;

const fileTable = new FileTable({
    headRowId: "file-table-head-row",
    bodyId: "file-table-body",
    paginationId: "file-table-pagination",
    pageSize: 20,
    columns: FILE_TYPES[currentFileTypeKey].columns,
    onDownload: (entry) => downloadEntry(entry),
    onDelete: (entry) => deleteEntry(entry)
});

fileTypeSelect.onchange = () => {
    currentFileTypeKey = fileTypeSelect.value;
    refreshFileTable();
};

presenter.showLoggerState("Nicht verbunden!", "on", "on", "on");

// ---------------------------------
//     Aktive UDS-Konfiguration
// ---------------------------------

const activeConfigCard = document.getElementById("active-config-card");
const activeConfigDialog = document.getElementById("active-config-dialog");
const activeConfigForm = document.getElementById("active-config-form");
const activeConfigSelect = document.getElementById("active-config-select");
const activeConfigCancel = document.getElementById("active-config-cancel");

/**
 * Fetches the current active configuration and updates the card with it.
 */
async function refreshActiveConfig() {
    const result = await api.getActiveConfig();
    if (!result.ok || !result.data?.active_config) {
        return;
    }
    presenter.showActiveConfig(result.data.active_config, result.data.values);
}

/**
 * Loads the list of available configuration files into the dialog's dropdown,
 * pre-selects the currently active one if known, then opens the dialog.
 */
async function openActiveConfigDialog() {
    const result = await api.listConfigs();
    if (!result.ok || !result.data?.entries) {
        alert("Konfigurationsdateien konnten nicht geladen werden.");
        return;
    }

    activeConfigSelect.innerHTML = "";
    result.data.entries.forEach((entry) => {
        const option = document.createElement("option");
        option.value = entry.filename;
        option.textContent = entry.filename;
        activeConfigSelect.appendChild(option);
    });

    if (activeConfigSelect.options.length === 0) {
        alert("Es sind keine Konfigurationsdateien vorhanden.");
        return;
    }

    activeConfigDialog.showModal();

    const activeResult = await api.getActiveConfig();
    if (activeConfigDialog.open && activeResult.ok && activeResult.data?.active_config) {
        activeConfigSelect.value = activeResult.data.active_config;
    }
}

activeConfigCard.addEventListener("click", openActiveConfigDialog);
activeConfigCancel.addEventListener("click", () => activeConfigDialog.close());

// Clicking the backdrop (outside the dialog box) also closes it.
activeConfigDialog.addEventListener("click", (e) => {
    if (e.target === activeConfigDialog) {
        activeConfigDialog.close();
    }
});

activeConfigForm.addEventListener("submit", async (e) => {
    e.preventDefault();

    const selectedFilename = activeConfigSelect.value;
    if (!selectedFilename) {
        return;
    }

    const result = await api.setActiveConfig(selectedFilename);
    switch (result.status) {
        case 200:
            presenter.showActiveConfig(selectedFilename);
            activeConfigDialog.close();
            break;
        case 404:
            alert("Die ausgewählte Konfigurationsdatei wurde nicht gefunden.");
            break;
        case 408:
            alert("Zeitüberschreitung beim Setzen der aktiven Konfiguration.");
            break;
        default:
            alert("Aktive Konfiguration konnte nicht gesetzt werden.");
    }
});

refreshActiveConfig();

// ---------------------------------
//     Start Button
// ---------------------------------

const startButton = document.getElementById("start-button");
var loggingRunning = false;
startButton.onclick = async (e) => {
    if (loggingRunning) {
        const result = await api.stopLogging();
        switch (result.status) {
            case 200:
                loggingRunning = false;
                startButton.innerText = "Start Logging";
                break;
            default:
                alert("Stop Logging failed");
        }
    } else {
        const result = await api.startLogging();
        switch (result.status) {
            case 200:
                loggingRunning = true;
                startButton.innerText = "Stop Logging";
                break;
            case 400:
                alert("Missing config or output file");
            case 408: 
                alert("Request timouted");
        }
    }
}

/**
 * Converts the unix timestamp to a displayable string
 * @param {*} unixSeconds The unix timestamp to be converted
 * @returns Returns the unix timestamp formatted as 'dd.mm.yyyy, hh:mm'
 */
function formatSystemTime(unixSeconds) {
    const date = new Date(unixSeconds * 1000);
    return date.toLocaleString("de-DE", {
        day: "2-digit",
        month: "2-digit",
        year: "numeric",
        hour: "2-digit",
        minute: "2-digit"
    });
}

const systemTimeCard = document.getElementById("system-time-card");
systemTimeCard.addEventListener("click", async () => {
    if (!confirm("Systemzeit mit der Uhr dieses Geräts synchronisieren?")) {
        return;
    }
    const result = await api.syncSystemTime();
    if (!result.ok) {
        alert("Synchronisierung fehlgeschlagen.");
        return;
    }

    const response = await api.getSystemTime();
    if (response.ok) {
        presenter.showSystemTime(formatSystemTime(response.data.timestamp));
    }
});

// ---------------------------------
//        Logs and configurations
// ---------------------------------

/**
 * Reloads the file table with currently selected settings
 */
async function refreshFileTable() {
    const type = FILE_TYPES[currentFileTypeKey];
    fileTable.setColumns(type.columns);
    addConfigButton.hidden = !type.showUpload;

    await fileTable.setSource((offset, limit) => type.list(offset, limit));
}

/**
 * Downloads a file from the file table.
 * @param {*} entry The table row's entry, must contain a "filename".
 */
async function downloadEntry(entry) {
    const kind = FILE_TYPES[currentFileTypeKey].kind;
    const result = await api.downloadFile(`${kind}/${entry.filename}`);
    if (!result.ok) {
        alert("Download fehlgeschlagen.");
        return;
    }

    const url = URL.createObjectURL(result.data);
    const link = document.createElement("a");
    link.href = url;
    link.download = entry.filename;
    link.click();
    URL.revokeObjectURL(url);
}

/**
 * Deletes a file from the table and reloads it.
 * @param {*} entry The table row's entry, must contain a "filename".
 */
async function deleteEntry(entry) {
    if (!confirm(`${entry.filename} wirklich löschen?`)) {
        return;
    }

    const kind = FILE_TYPES[currentFileTypeKey].kind;
    const result = await api.deleteFile(`${kind}/${entry.filename}`);
    if (!result.ok) {
        alert("Löschen fehlgeschlagen.");
        return;
    }

    refreshFileTable();
}

const addConfigButton = document.getElementById("add-config-button");
const configFileInput = document.getElementById("config-file-input");

addConfigButton.onclick = () => configFileInput.click();

configFileInput.onchange = async () => {
    const file = configFileInput.files[0];
    configFileInput.value = "";
    if (!file) {
        return;
    }

    const result = await api.uploadFile(`configs/${file.name}`, file);
    switch (result.status) {
        case 200:
            refreshFileTable();
            break;
        case 400:
            alert("Diese Konfigurationsdatei existiert bereits.");
            break;
        case 403:
            alert("Hochladen an diesem Pfad ist nicht erlaubt.");
            break;
        case 404:
            alert("Zielverzeichnis nicht gefunden.");
            break;
        case 507:
            alert("Nicht genug Speicherplatz auf dem Logger.");
            break;
        default:
            alert("Hochladen fehlgeschlagen.");
    }
};

refreshFileTable();

// ---------------------------------
//       Parametereinstellungen
// ---------------------------------

/**
 * Registry of the editable datalogger parameters shown as cards. 
 * Each entry describes how to fetch/persist its value and how to render/validate it.
 *
 * - valueKey: key of the value in the GET/PUT JSON bodies.
 * - get/set: API calls to read and persist the value.
 */
const PARAM_SETTINGS = [
    {
        id: "can-timeout",
        label: "CAN-Timeout",
        hint: "",
        unit: "ms",
        min: 10,
        max: 4294967295,
        valueKey: "can_timeout",
        get: () => api.getCanTimeout(),
        set: (value) => api.setCanTimeout(value)
    },
    {
        id: "uds-timeout",
        label: "UDS-Timeout",
        hint: "",
        unit: "ms",
        min: 10,
        max: 4294967295,
        valueKey: "uds_timeout",
        get: () => api.getUdsTimeout(),
        set: (value) => api.setUdsTimeout(value)
    },
    {
        id: "deepsleep-timeout",
        label: "Deep-Sleep-Timeout",
        hint: "WiFi & PIR & Term15 sind aus",
        unit: "s",
        min: 10,
        max: 600,
        valueKey: "deepsleep_timeout",
        get: () => api.getDeepsleepTimeout(),
        set: (value) => api.setDeepsleepTimeout(value)
    },
    {
        id: "term15-request-interval",
        label: "Zündungs-Abfrageintervall",
        hint: "",
        unit: "ms",
        min: 100,
        max: 10000,
        valueKey: "request_interval",
        get: () => api.getTerm15RequestInterval(),
        set: (value) => api.setTerm15RequestInterval(value)
    }
];

const paramCardContainer = document.getElementById("param-card-container");
const paramDialog = document.getElementById("param-dialog");
const paramForm = document.getElementById("param-form");
const paramDialogTitle = document.getElementById("param-dialog-title");
const paramFormLabel = document.getElementById("param-form-label");
const paramInput = document.getElementById("param-input");
const paramCancel = document.getElementById("param-cancel");

let activeParam = null;

/**
 * Fetches a parameter's current value and updates its card.
 * @param {*} param Entry from PARAM_SETTINGS.
 */
async function refreshParamCard(param) {
    const result = await param.get();
    const valueField = document.getElementById(`${param.id}-value`);
    if (!result.ok) {
        return;
    }
    valueField.innerText = `${result.data[param.valueKey]} ${param.unit}`;
}

/**
 * Loads the parameter's current value into the dialog and opens it.
 * @param {*} param Entry from PARAM_SETTINGS.
 */
async function openParamDialog(param) {
    activeParam = param;
    paramDialogTitle.innerText = param.label;
    paramFormLabel.innerText = `Neuer Wert (${param.unit}, ${param.min}–${param.max})`;
    paramInput.min = param.min;
    paramInput.max = param.max;

    const result = await param.get();
    paramInput.value = result.ok ? result.data[param.valueKey] : "";

    paramDialog.showModal();
}

PARAM_SETTINGS.forEach((param) => {
    const card = document.createElement("button");
    card.id = `${param.id}-card`;
    card.className = "card card-hover";
    card.innerHTML = `
        <h3>${param.label}</h3>
        <p class="hint">${param.hint}</p>
        <div class="card-content">
            <p id="${param.id}-value"></p>
        </div>
        <img src="edit-icon.svg" alt="✎" class="card-icon">
    `;
    card.addEventListener("click", () => openParamDialog(param));
    paramCardContainer.appendChild(card);

    refreshParamCard(param);
});

paramCancel.addEventListener("click", () => paramDialog.close());

paramDialog.addEventListener("click", (e) => {
    if (e.target === paramDialog) {
        paramDialog.close();
    }
});

paramForm.addEventListener("submit", async (e) => {
    e.preventDefault();

    const value = Number(paramInput.value);
    const result = await activeParam.set(value);
    switch (result.status) {
        case 200:
            paramDialog.close();
            refreshParamCard(activeParam);
            break;
        case 400:
            alert(`Ungültiger Wert. Erlaubter Bereich: ${activeParam.min}–${activeParam.max} ${activeParam.unit}.`);
            break;
        default:
            alert("Wert konnte nicht gesetzt werden.");
    }
});

// ---------------------------------
//           OTA Update
// ---------------------------------

const otaSelectButton = document.getElementById("ota-select-button");
const otaStartButton = document.getElementById("ota-start-button");
const otaFileInput = document.getElementById("ota-file-input");
const otaFirmwareName = document.getElementById("ota-firmware-name");

let selectedFirmware = null;

otaSelectButton.onclick = () => otaFileInput.click();

otaFileInput.onchange = () => {
    const file = otaFileInput.files[0];
    otaFileInput.value = "";
    if (!file) {
        return;
    }

    selectedFirmware = file;
    otaFirmwareName.innerText = file.name;
    otaStartButton.disabled = false;
};

otaStartButton.onclick = async () => {
    if (!selectedFirmware) {
        return;
    }
    if (!confirm(`Firmware-Update mit "${selectedFirmware.name}" wirklich starten? Das Gerät startet danach neu.`)) {
        return;
    }

    otaSelectButton.disabled = true;
    otaStartButton.disabled = true;
    otaStartButton.innerText = "Update läuft…";

    const result = await api.otaUpdate(selectedFirmware);
    switch (result.status) {
        case 200:
            alert("Update erfolgreich. Das Gerät startet neu.");
            selectedFirmware = null;
            otaFirmwareName.innerText = "Keine Firmware-Datei ausgewählt";
            break;
        case 400:
            alert("Ungültige Firmware-Datei.");
            break;
        case 500:
            alert("OTA-Update ist im aktuellen Zustand nicht möglich (z.B. während einer laufenden Messung).");
            break;
        case 0:
            // The device restarts right after sending its response, so the connection can drop before the browser reads it even on success.
            alert("Verbindung zum Gerät unterbrochen. Das Update wurde eventuell trotzdem übernommen, das Gerät startet in diesem Fall neu.");
            selectedFirmware = null;
            otaFirmwareName.innerText = "Keine Firmware-Datei ausgewählt";
            break;
        default:
            alert("OTA-Update fehlgeschlagen.");
    }

    otaSelectButton.disabled = false;
    otaStartButton.disabled = !selectedFirmware;
    otaStartButton.innerText = "Update starten";
};

// ---------------------------------
//           Websockets
// ---------------------------------

function handleSocketMessage(msg) {
    switch (msg.type) {
        case "status":
            presenter.showLoggerState(msg.state, msg.leds.red, msg.leds.yellow, msg.leds.green);
            if (msg.state == "logging_active" || msg.state == "wait_term15" || msg.state == "import_config") {
                loggingRunning = true;
                startButton.innerText = "Stop Logging";
            } else {
                loggingRunning = false;
                startButton.innerText = "Start Logging";
            }
            break;
        case "time":
            presenter.showSystemTime(formatSystemTime(msg.timestamp));
            break;
        case "cycles":
            presenter.showCycles(msg.cycles);
            break;
        case "timeouts":
            presenter.showTimeouts(msg.timeouts);
            break;
        case "storage":
            presenter.showStorage(msg.used, msg.total);
            break;
        default:
            console.warn("Unbekannter Websocket-Nachrichtentyp:", msg.type);
    }
}

function connectStatusSocket() {
    const ws = new WebSocket(`ws://${location.host}/ws`);

    ws.onmessage = (evt) => {
        handleSocketMessage(JSON.parse(evt.data));
    };

    ws.onclose = () => {
        setTimeout(connectStatusSocket, 1000);
    };

    ws.onerror = () => ws.close();
}

connectStatusSocket();
