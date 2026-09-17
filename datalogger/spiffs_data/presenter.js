export class UIPresenter {
    /**
     * Sets the state of an LED
     * @param {*} id the html id of the LED
     * @param {*} state States can be on off and blink.
     */
    showLed(id, state) {
        const led = document.getElementById(id);
        led.classList.remove("on", "off", "blink");
        led.classList.add(state);
    }

    showLoggerState(text, ledR, ledY, ledG) {
        const textField = document.getElementById("status-text");
        textField.innerText = text;
        this.showLed("led-green", ledG);
        this.showLed("led-yellow", ledY);
        this.showLed("led-red", ledR);
    }

    /**
     * Updated the storage bar with the passed values.
     * @param {*} used The still used storage in MB
     * @param {*} full The total available storage in MB
     */
    showStorage(used, full) {
        const fill = document.getElementById("storage-bar-fill");
        const textBlack = document.getElementById("storage-text");
        const textWhite = document.getElementById("storage-text-white");

        var percent = (used / full) * 100;
        var label = `${used} MB / ${full} MB (${percent.toFixed(1)}%)`;

        if (full == 0) {
            percent = 0
            label = "Keine SD-Karte erkannt"
        }

        fill.style.width = percent + "%";
        textBlack.innerText = label;
        textWhite.innerText = label;

        textWhite.style.clipPath = `inset(0 ${100 - percent}% 0 0)`;
    }

    /**
     * @param {string} name Filename of the currently active configuration.
     * @param {number} [values] Number of specified signals. 
     */
    showActiveConfig(name, values) {
        const configName = document.getElementById("config-name");
        const configValues = document.getElementById("config-values");
        configName.innerText = name;
        configValues.innerText = values !== undefined ? `${values} Werte` : "";
    }

    showMeasureStatus(cycles, timeouts) {
        const statusCycles = document.getElementById("status-cycles");
        const statusTimeouts = document.getElementById("status-timeouts");
        statusCycles.innerText = `${cycles} Zyklen`;
        statusTimeouts.innerText = `${timeouts} Timeouts`;
    }

    showTimeouts(timeouts) {
        const statusTimeouts = document.getElementById("status-timeouts");
        statusTimeouts.innerText = `${timeouts} Timeouts`;
    }

    showCycles(cycles) {
        const statusCycles = document.getElementById("status-cycles");
        statusCycles.innerText = `${cycles} Zyklen`;
    }

    showSystemTime(time) {
        const sysTime = document.getElementById("sys-time");
        sysTime.innerText = time;
    }
}