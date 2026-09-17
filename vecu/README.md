# vECU - Generic Vehicle Simulator

A generic vehicle ECU simulator for Hardware-in-the-Loop (HIL) testing. It simulates one generic electric vehicle over CAN or through the UART/CAN bridge used by the data logger.

The current runtime is intentionally generic:
- some baseline parameters are inspired by publicly available EV reference data
- many detail signals are heuristic and tuned for plausible HIL behavior
- the simulator is not a validated digital twin of a VW ID.3, Toyota Mirai, or any other production vehicle

## Quick Start

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv venv
uv sync
uv run python -m src.main
```

## Usage

```bash
# Generic CAN mode
uv run python -m src.main

# Explicit generic profile
uv run python -m src.main --vehicle generic_vehicle --scenario scenarios/city_drive.yaml

# Generic UART mode
uv run python -m src.main_uart --port /dev/ttyUSB0 --vehicle generic_vehicle
```

## Runtime Model

Each simulator tick follows this pipeline:

1. The scenario/state manager determines the current vehicle state.
2. The vehicle simulator computes shared telemetry such as speed, acceleration, odometer, SOC, current, voltage, and temperatures.
3. The generic powertrain model enriches that telemetry with battery and thermal side signals.
4. The generic signal adapter maps shared telemetry to the active UDS labels.
5. The parameter registry encodes the current values into UDS responses when a DID is requested.

## Scope and Data Quality

The project now makes a strict distinction between:
- **reference-backed baseline values**, such as power, torque, net battery size, top speed, and drag coefficient
- **heuristic model values**, such as cooling coefficients, auxiliary loads, and thermal tuning constants

See:
- [Project Documentation](/Users/eniangashi/Desktop/emob-hil/vecu/docs/DOCUMENTATION.md)
- [Parameter Rationale](/Users/eniangashi/Desktop/emob-hil/vecu/docs/PARAMETER_RATIONALE.md)
- [Architecture Decisions](/Users/eniangashi/Desktop/emob-hil/vecu/docs/DECISIONS.md)

## Future Work

A future group should replace the generic profile with validated vehicle-specific models. That requires:
- real vehicle reference traces
- DID-by-DID decoding validation
- parameter calibration against measurements
- explicit validation criteria for state, thermal, and energy behavior

## Tests

```bash
cd /Users/eniangashi/Desktop/emob-hil/vecu
python -m pytest
```
