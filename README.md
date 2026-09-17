# E-Mobility-Datalogger-BA
- bachelor thesis by Arwin Allert
- based on the master thesis by Markus Huber and the project study from WiSe 25/26

## Installation

### Datalogger
- Install the ESP-IDF Framework: https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32c6/get-started/index.html (Version 5.4.3 used for this project)

### Virtual Electronic Control Unit (vECU)
- Python must be installed
```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv venv
uv sync
uv run python -m src.main
```

### Testrunner
- The vECU must be installed
```bash
pip install matplotlib  # for the generation of performance diagrams
pip install gcovr       # for the code coverage report
```

### Recommended VS-Code extensions
The recommended IDE is Visual Studio Code, which provides all extensions you will need for all parts of the project. 
- espressif.esp-idf-extension
- ms-vscode.cpptools-extension-pack
- ecmel.vscode-html-css
- ritwickdey.liveserver
- 42crunch.vscode-openapi
- arjun.swagger-viewer
- ms-python.python
- mechatroner.rainbow-csv

## Usage

### Build & Flash Firmware on Datalogger
```bash
# Run the ESP-IDF Script first or use the ESP-IDF Terminal
cd <REPO_ROOT>/datalogger
idf.py set-target esp32-c6
idf.py build
idf.py flash
```

If you have problems with flashing the ESP, try to set the ESP in Bootloader Mode using the following procedure:

1. Press and hold BOOT
2. Press and release RESET 
3. Wait 1-2 seconds, then release BOOT

In this mode the ESP does not automatically execute the main function after connection, so firmware issues will not be a problem.
After that you should be able to flash the ESP again.

To exit Bootloader Mode, press RESET again. 

### Connect to the web interface hosted by the Datalogger
- Connect to the wifi access point ```AutoMessy``` of the datalogger, the password is ```automessy123```
- Open ```automessy.local``` in your Browser or use the IP ```192.168.4.1```

### Start vECU
```bash
cd <REPO_ROOT>/vecu
python -u -m src.main
```
#### vECU Parameters: 
All parameters are optional, their default values are indicated in angle brackets. 
```bash
--transport <stdin>
--vehicle <vw_id3>
--scenario <scenarios/city_drive.yaml>
--ambient-temp <20.0>
--update-rate <50>
--port <COM6>           # for USB-Serial transport only
--baud <115200>         # for USB-Serial transport only
```

### Start Datalogger Tests

```bash
# Run the ESP-IDF Script first or use the ESP-IDF Terminal
cd <REPO_ROOT>/testrunner
python run_hil_tests.py
```

#### Testrunner Parameters: 
All parameters are optional; their default values are indicated in angle brackets. 

```bash
--tags <all registered tags>    # defines which tests should run
--vecu                          # starts the vECU for hil tests
--force-flash                   # flashes the ESP new with the current build
--rebuild                       # rebuilds the firmware regardless of code changes
--port <auto detect>
--baud <115200>
--coverage                      # activates a code coverage analyse
```


These parameters only take into account if the vecu is started. 
```bash
--scenario <city_drive>
--ambient-temp <20.0>
--update-rate <50>
```

#### Code Coverage Report
The Testrunner Python scripts will print a summary of the report in the Terminal. The full report can be seen in the directory 

- /\<repo_root\>/datalogger/test/build/coverage_report

### Add new Test Suite to a component

1. create subcomponent "test" with own CMakeLists.txt
2. create testfile "test\_\<komponentenname>.c" in this subcomponent
3. append the name of the component to the REQUIREMENTS list of the main component of the "test"- subproject
4. append the name of the component to the list of components to test to the CMakeLists.txt of the "test"- subproject
5. append the tag of the suite to the tag-list in the testrunner (optional)
6. delete the CMakeCache.txt in the build-folder (automated in the testrunner) and make a idf.py fullclean
