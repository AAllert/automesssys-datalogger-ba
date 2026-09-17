from __future__ import annotations

import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from src.config import Config
from src.simulation.signal_adapters import (  # noqa: E402
    Id3SignalAdapter,
    Mirai1SignalAdapter,
    Mirai2SignalAdapter,
)
from src.simulation.telemetry import TelemetrySnapshot  # noqa: E402
from src.state.vehicle_state import VehicleState  # noqa: E402
from src.vehicles import DISCOVERED_VEHICLES  # noqa: E402


DOC_PATH = PROJECT_ROOT / "docs" / "SIGNAL_CALCULATIONS.md"
UNMAPPED_NOTE = (
    "Nicht modelliert. Keine Zuweisung im aktiven Signal-Adapter; der Wert bleibt im "
    "`ParameterRegistry` auf `0.0` und wird bei DID-Anfragen als 0 kodiert."
)


GENERIC_BLOCKS = [
    (
        "G1",
        "`velocity_kmh`",
        "Zielgeschwindigkeit aus Zustand: `CREEPING=5`, `DRIVING=50`, `HIGHWAY=120` km/h. "
        "Darauf kommt eine geglaettete Zufallsvariation von maximal `+-15 %`. "
        "Die Istgeschwindigkeit folgt dem Ziel mit fahrzeugabhaengigem Beschleunigungslimit "
        "und einem festen Bremslimit von `3.0 m/s^2`.",
    ),
    (
        "G2",
        "`motor_rpm`",
        "`(velocity_kmh / 3.6) / (2 * pi * wheel_radius_m) * 60 * gear_ratio`.",
    ),
    (
        "G3",
        "`hv_current_a`",
        "Zustandsabhaengig. `PARKED_OFF=0 A`, `PARKED_IGN_ON=5 A`, "
        "`CHARGING=-(max_ac_charge_power_kw * 1000 * 0.90) / battery_nominal_voltage`. "
        "In Fahrzustaenden: Luftwiderstand `0.5 * rho * cd * A * v^2`, Rollwiderstand "
        "`c_r * m * g`, Beschleunigungskraft `m * a`, daraus mechanische Leistung "
        "`(F_drag + F_roll + F_accel) * v`. Positive Leistung wird durch "
        "`inverter_efficiency * motor_efficiency` geteilt, Rekuperation mit demselben "
        "Wirkungsgrad und zusaetzlichem Faktor `0.85` multipliziert. Danach werden `2500 W` "
        "Nebenverbraucher addiert und durch `battery_nominal_voltage` geteilt. Reku ist auf "
        "`-150 A` begrenzt.",
    ),
    (
        "G4",
        "`hv_voltage_v`",
        "Basisspannung `battery_nominal_voltage * (0.9 + 0.1 * soc / 100)`. "
        "Beim Entladen wird `abs(current) * battery_internal_resistance` abgezogen, "
        "beim Laden addiert.",
    ),
    (
        "G5",
        "`battery_temp_avg_c`",
        "Thermisches Modell aus `I^2 * R`-Verlusten, zusaetzlicher entropischer Erwärmung "
        "und Zusatzverlusten bei starker Rekuperation. Dazu kommen passive Kuehlung, "
        "aktive Kuehlung ab ca. `28 C` und Heizung unterhalb der Mindesttemperatur.",
    ),
    (
        "G6",
        "`hv_soc_pct`",
        "Coulomb Counting: `soc -= ((current * delta_time) / 3600) / battery_capacity_ah * 100`. "
        "Der Startwert ist `80 %` und der Wert wird auf `0..100 %` begrenzt.",
    ),
    (
        "G7",
        "`odometer_total_km`, `trip_distance_km`",
        "Nur bei positiver Geschwindigkeit: `distance_km = velocity_kmh / 3600 * delta_time`, "
        "danach Aufsummierung auf Odometer und Trip.",
    ),
    (
        "G8",
        "`accelerator_pedal_pct`",
        "`clamp((target_velocity_kmh / top_speed_kmh) * 70 + max(acceleration_mps2, 0) * 12, 0, 100)`. "
        "In `CREEPING` wird der Wert mit `0.6` multipliziert; in `PARKED_IGN_ON`, "
        "`PARKED_OFF` und `CHARGING` ist er `0`.",
    ),
    (
        "G9",
        "`terminal_15`, `_ignition_on`",
        "`terminal_15 = 1`, solange der Zustand nicht `PARKED_OFF` ist. "
        "`_ignition_on = 1`, solange der Zustand weder `PARKED_OFF` noch `CHARGING` ist.",
    ),
]


BEV_BLOCKS = [
    (
        "B1",
        "Batterietemperatur-Min/Max",
        "`battery_temp_span = min(6.0, abs(hv_current_a) * 0.01 + 1.0)`, dann "
        "`min = avg - span/2`, `max = avg + span/2`.",
    ),
    (
        "B2",
        "Zellspannungen",
        "`cell_base_v = hv_voltage_v / 96`, `cell_delta_v = min(0.04, abs(hv_current_a) * 0.00008 + 0.006)`, "
        "`uCellMin = max(2.5, cell_base_v - cell_delta_v)`, "
        "`uCellMax = min(4.3, cell_base_v + cell_delta_v)`, "
        "`uSumCell = cell_base_v * 96`.",
    ),
    (
        "B3",
        "Kuehlbedarf, Luefter und Kompressor",
        "`thermal_load = max(0, battery_temp_avg_c - ambient_temp_c)`, "
        "`cooling_demand = clamp(thermal_load * 4.0 + abs(hv_current_a) * 0.06, 0, 100)`, "
        "`fan_current = clamp(cooling_demand * 0.12, 0, 18)`, "
        "`compressor_speed = clamp(cooling_demand * 55, 0, 6000)`, "
        "`compressor_torque = clamp(cooling_demand * 0.16, 0, 40)`.",
    ),
    (
        "B4",
        "Kabinen- und Kuehlmitteltemperatur",
        "`cabin_temp = ambient_temp_c + (22 - ambient_temp_c) * min(1, delta_time * 0.15 + 0.35)`, "
        "`coolant_temp = ambient_temp_c + 6 + thermal_load * 0.25 + fan_current * 0.2`.",
    ),
    (
        "B5",
        "Dynamische Stromgrenzen",
        "`charge_current_limit = clamp(250 - max(0, soc - 60) * 2.5 - max(0, battery_temp_max_c - 35) * 4, 20, 250)`, "
        "`discharge_current_limit = clamp(320 - max(0, 15 - battery_temp_avg_c) * 6 - max(0, soc - 90) * 4, 40, 320)`.",
    ),
]


FCEV_BLOCKS = [
    (
        "F1",
        "`traction_load`",
        "In `PARKED_OFF` ist die Last `0`, in `PARKED_IGN_ON` ist sie `6`. "
        "Sonst: `clamp((velocity_kmh / top_speed_kmh) * 60 + abs(acceleration_mps2) * 12 + accelerator_pedal_pct * 0.4, 0, 100)`.",
    ),
    (
        "F2",
        "Fuel-Cell-Stack",
        "`stack_current = clamp(base_stack_current + traction_load * 1.8, 0, 220)`, "
        "wobei `base_stack_current` je nach Zustand `0`, `8` oder `20 A` ist. "
        "`stack_voltage = clamp(battery_nominal_voltage + 40 - stack_current * 0.35, 220, 420)`.",
    ),
    (
        "F3",
        "H2-Druck und Temperaturen",
        "`target_pressure = clamp(730 - traction_load * 2.2, 420, 730)`, danach Tiefpass "
        "auf `hydrogen_pressure_high`. "
        "`target_coolant_temp = ambient + 10 + traction_load * 0.12`, "
        "`target_tank_temp = ambient + 4 + traction_load * 0.05`, beide ebenfalls geglaettet.",
    ),
    (
        "F4",
        "Pumpen-/Kompressordrehzahl",
        "`pump_or_compressor_speed_rpm = clamp(2200 + traction_load * 55, 1500, 9000)`.",
    ),
]


MIRAI2_BLOCKS = [
    (
        "M2-1",
        "Mirai2-Ambientdruck und Boost",
        "`ambient_pressure = 101.3 - velocity_kmh * 0.006`, "
        "`boost_ratio = clamp(100 + accelerator_pedal_pct * 0.45, 0, 200)`.",
    ),
    (
        "M2-2",
        "Mirai2-Generator",
        "`generator_rpm = pump_or_compressor_speed_rpm * 0.92`, "
        "`fGen = generator_rpm / 60`, `tqGenReq = accelerator_pedal_pct * 1.5`.",
    ),
    (
        "M2-3",
        "Mirai2-Zeitwerte",
        "`datetime.now()` liefert Tag, Stunde, Minute, Monat, Sekunde und Jahr (`YY`).",
    ),
]


MODELED_SIGNALS = {
    "vw_id3_pro_s": {
        "title": "VW ID.3 (`bev`)",
        "rows": [
            ("vVeh1", "`snapshot.velocity_kmh`", "G1"),
            ("vVeh2", "`snapshot.velocity_kmh`", "G1"),
            ("vVeh3", "`snapshot.velocity_kmh`", "G1"),
            ("vVeh4", "`snapshot.velocity_kmh`", "G1"),
            ("vVeh5", "`snapshot.velocity_kmh`", "G1"),
            ("nEm1", "`snapshot.motor_rpm`", "G2"),
            ("nEm2", "`snapshot.motor_rpm`", "G2"),
            ("nEmTrac", "`snapshot.motor_rpm`", "G2"),
            ("curHvBat1", "`snapshot.hv_current_a`", "G3"),
            ("cur2HvBat", "`snapshot.hv_current_a`", "G3"),
            ("curDcLink", "`snapshot.hv_current_a`", "G3"),
            ("uHvBat", "`snapshot.hv_voltage_v`", "G4"),
            ("uHv", "`snapshot.hv_voltage_v`", "G4"),
            ("uDcLink", "`snapshot.hv_voltage_v`", "G4"),
            ("tHvBat", "`snapshot.battery_temp_avg_c`", "G5"),
            ("rSocBat", "`snapshot.hv_soc_pct`", "G6"),
            ("rSocBat2", "`snapshot.hv_soc_pct`", "G6"),
            ("aVeh", "`snapshot.acceleration_mps2 * 1000`", "G1, Ausgabe in `mg` statt `m/s^2`."),
            ("sMileage", "`snapshot.odometer_total_km`", "G7"),
            ("stTerm15", "`snapshot.terminal_15`", "G9"),
            ("rApp", "`snapshot.accelerator_pedal_pct`", "G8"),
            ("sVehTrip", "`snapshot.trip_distance_km`", "G7"),
            ("tAmb", "`snapshot.ambient_temp_c`", "Direkt aus `VehicleConstants.ambient_temp_celsius`; kommt aus CLI-Option `--ambient-temp`."),
            ("tCab", "`snapshot.cabin_temp_c`", "B4"),
            ("curFan", "`snapshot.fan_current_a`", "B3"),
            ("rFanDes", "`snapshot.fan_command_pct`", "B3"),
            ("nCmpr", "`snapshot.compressor_speed_rpm`", "B3"),
            ("tqCmpr", "`snapshot.compressor_torque_nm`", "B3"),
            ("uSumCell", "`snapshot.cell_voltage_sum_v`", "B2"),
            ("uCellMin1", "`snapshot.cell_voltage_min_v`", "B2"),
            ("uCellMax1", "`snapshot.cell_voltage_max_v`", "B2"),
            ("tMinHvBat", "`snapshot.battery_temp_min_c`", "B1"),
            ("tMaxHvBat", "`snapshot.battery_temp_max_c`", "B1"),
            ("curChrgLimDyn", "`snapshot.charge_current_limit_a`", "B5"),
            ("curLimDynDchrg", "`snapshot.discharge_current_limit_a`", "B5"),
        ],
    },
    "toyota_mirai1": {
        "title": "Toyota Mirai 1 (`fcev`)",
        "rows": [
            ("uBat", "`snapshot.hv_voltage_v`", "G4"),
            ("distTotalTraveled", "`snapshot.odometer_total_km`", "G7"),
            ("EVSysWakeUpSig", "`snapshot.ignition_on`", "G9"),
            ("FCMode", "Zustandscode", "`PARKED_OFF=0`, `PARKED_IGN_ON=1`, `CREEPING/DRIVING/HIGHWAY=2`, `CHARGING=3`."),
            ("uFCRelatedPartsDrive", "`12.0 + terminal_15 * 2.0`", "Hilfsspannung, direkt aus `snapshot.terminal_15`."),
            ("curFC2", "`snapshot.stack_current_a`", "F1 + F2"),
            ("uFCTotal", "`snapshot.stack_voltage_v`", "F2"),
            ("uFCBB", "`snapshot.hv_voltage_v`", "G4"),
            ("pH2High", "`snapshot.hydrogen_pressure_high`", "F3"),
            ("pH2FillingSysHigh", "`snapshot.hydrogen_pressure_high * 0.97`", "Abgeleitet aus F3."),
            ("pH2Low", "`snapshot.hydrogen_pressure_high * 0.35`", "Abgeleitet aus F3."),
            ("pH2HiSmooth", "`snapshot.hydrogen_pressure_high * 0.98`", "Abgeleitet aus F3."),
            ("pH2LowSmooth", "`snapshot.hydrogen_pressure_high * 0.35 * 0.98`", "Abgeleitet aus F3."),
            ("tH2PumpMot", "`snapshot.coolant_temp_c + 3.0`", "Abgeleitet aus F3."),
            ("nH2Pump", "`snapshot.pump_or_compressor_speed_rpm`", "F4"),
            ("nH2PumpDes", "`snapshot.pump_or_compressor_speed_rpm * 1.05`", "Abgeleitet aus F4."),
            ("tH2Tank1Smoth", "`snapshot.battery_temp_min_c`", "F3; im FCEV-Modell auf `tank_temp_c - 1.0` gesetzt."),
            ("tH2Tank2Smooth", "`snapshot.battery_temp_max_c`", "F3; im FCEV-Modell auf `tank_temp_c + 1.5` gesetzt."),
            ("mH2Remain", "`max(0.5, hydrogen_pressure_high * 0.0075)`", "Abgeleitet aus F3."),
            ("mH2RemainFilling", "`mH2Remain * 1.02`", "Abgeleitet aus `mH2Remain`."),
            ("tFCStaCoolantStaOut", "`snapshot.coolant_temp_c`", "F3"),
            ("tFCStaCoolantRadOut", "`snapshot.coolant_temp_c - 1.5`", "Abgeleitet aus F3."),
            ("uTankShutValve1", "`12.0 * terminal_15`", "Direkt aus `snapshot.terminal_15`."),
            ("uTankShutValve2", "`12.0 * terminal_15`", "Direkt aus `snapshot.terminal_15`."),
            ("stTerm15", "`snapshot.terminal_15`", "G9"),
        ],
    },
    "toyota_mirai2": {
        "title": "Toyota Mirai 2 (`fcev`)",
        "rows": [
            ("numTiDay", "Aktueller Kalendertag", "M2-3"),
            ("numTiHr", "Aktuelle Stunde", "M2-3"),
            ("numTiMin", "Aktuelle Minute", "M2-3"),
            ("numTiMth", "Aktueller Monat", "M2-3"),
            ("numTiSec", "Aktuelle Sekunde", "M2-3"),
            ("numTiYr", "Aktuelles Jahr (`YY`)", "M2-3"),
            ("rApp", "`snapshot.accelerator_pedal_pct`", "G8"),
            ("tAmb", "`snapshot.ambient_temp_c`", "Direkt aus `VehicleConstants.ambient_temp_celsius`; kommt aus CLI-Option `--ambient-temp`."),
            ("pAmbFilt", "Gefilterter Umgebungsdruck", "M2-1"),
            ("pAmb", "Umgebungsdruck", "`pAmbFilt + 0.4`."),
            ("rBstRat", "Boost-Verhaeltnis", "M2-1"),
            ("uFcConvOut", "`snapshot.stack_voltage_v`", "F2"),
            ("uFcConvOutFlit", "`snapshot.stack_voltage_v * 0.985`", "Abgeleitet aus F2."),
            ("curFc", "`snapshot.stack_current_a`", "F1 + F2"),
            ("curFcFilt", "`snapshot.stack_current_a * 0.97`", "Abgeleitet aus F2."),
            ("uFc", "`snapshot.stack_voltage_v`", "F2"),
            ("uFcFilt", "`snapshot.stack_voltage_v * 0.99`", "Abgeleitet aus F2."),
            ("tqGenReq", "Generator-Drehmomentanforderung", "M2-2"),
            ("fGen", "Generatorfrequenz", "M2-2"),
            ("nGen", "Generatordrehzahl", "M2-2"),
            ("pH2HighRng", "`snapshot.hydrogen_pressure_high`", "F3"),
            ("curHvCtrl", "`snapshot.hv_current_a`", "G3"),
            ("nInvPmp", "`snapshot.pump_or_compressor_speed_rpm`", "F4"),
            ("stTerm15", "`snapshot.terminal_15`", "G9"),
            ("uAirVlvSply", "`12.0 + terminal_15 * 1.5`", "Direkt aus `snapshot.terminal_15`."),
        ],
    },
}


ADAPTERS = {
    "vw_id3_pro_s": Id3SignalAdapter(),
    "toyota_mirai1": Mirai1SignalAdapter(),
    "toyota_mirai2": Mirai2SignalAdapter(),
}


def append_table(lines: list[str], headers: list[str], rows: list[tuple[str, ...]]) -> None:
    lines.append("| " + " | ".join(headers) + " |")
    lines.append("| " + " | ".join("---" for _ in headers) + " |")
    for row in rows:
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")


def get_unmapped_labels(vehicle_id: str) -> list[str]:
    config = Config(DISCOVERED_VEHICLES[vehicle_id].CONFIG_PATH)
    config_labels = set(config.get_labels())
    snapshot = TelemetrySnapshot(state=VehicleState.PARKED_OFF)
    adapter_labels = set(ADAPTERS[vehicle_id].build_updates(snapshot))
    return sorted(config_labels - adapter_labels)


def build_document() -> str:
    lines: list[str] = []
    lines.append("# Signalberechnung der aktuellen Simulation")
    lines.append("")
    lines.append("Diese Referenz beschreibt den **aktuellen Runtime-Pfad** der Simulation.")
    lines.append(
        "Sie basiert auf den aktiven Implementierungen in `src/simulation/vehicle_simulator.py`, "
        "`src/physics/physics_calculator.py`, `src/physics/derived_values_calculator.py`, "
        "`src/simulation/signal_adapters.py` und `src/simulation/parameter_registry.py`."
    )
    lines.append("")
    lines.append("## Geltungsbereich")
    lines.append("")
    lines.append(
        "- Die Tabellen erfassen **jedes CSV-Signal**, das aktuell ueber die drei Fahrzeugdefinitionen erreichbar ist."
    )
    lines.append(
        "- Signale mit aktiver Adapter-Zuweisung werden mit ihrer aktuellen Berechnung beschrieben."
    )
    lines.append(
        "- Alle uebrigen CSV-Signale sind explizit als **nicht modelliert** markiert. Diese Signale bleiben derzeit auf `0.0`."
    )
    lines.append(
        "- `state_config.py` ist Teil des Repositories, wird aber im aktuellen Laufzeitpfad **nicht verwendet**."
    )
    lines.append("")
    lines.append("## Runtime-Pipeline")
    lines.append("")
    lines.append("1. Ein YAML-Szenario liefert nur Zustand und Zustandsdauer.")
    lines.append("2. `StateManagerImpl` schaltet die Zustaende, unter Beruecksichtigung von Stop-Bedingungen.")
    lines.append("3. `VehicleSimulatorImpl` berechnet Sollgeschwindigkeit, Istgeschwindigkeit, Beschleunigung und Odometer.")
    lines.append("4. `PhysicsCalculatorImpl` berechnet Motor-RPM, HV-Strom, HV-Spannung, Batterietemperatur und SOC.")
    lines.append("5. `DerivedValuesCalculator` ergaenzt abgeleitete Werte (Zellspannungen, Temperaturen, Stromgrenzen).")
    lines.append("6. Der jeweilige `SignalAdapter` mappt den Snapshot auf die CSV-Labels.")
    lines.append("7. `ParameterRegistryImpl` codiert die physikalischen Werte bei DID-Anfragen gemaess CSV-Faktor und Offset.")
    lines.append("")
    lines.append("## Rechenbausteine")
    lines.append("")
    append_table(lines, ["ID", "Interner Wert", "Berechnung"], GENERIC_BLOCKS)
    lines.append("### BEV-spezifische Rechenbausteine")
    lines.append("")
    append_table(lines, ["ID", "Bereich", "Berechnung"], BEV_BLOCKS)
    lines.append("### FCEV-spezifische Rechenbausteine")
    lines.append("")
    append_table(lines, ["ID", "Bereich", "Berechnung"], FCEV_BLOCKS)
    lines.append("### Mirai2-spezifische Rechenbausteine")
    lines.append("")
    append_table(lines, ["ID", "Bereich", "Berechnung"], MIRAI2_BLOCKS)

    for vehicle_id in ("vw_id3_pro_s", "toyota_mirai1", "toyota_mirai2"):
        section = MODELED_SIGNALS[vehicle_id]
        unmapped_labels = get_unmapped_labels(vehicle_id)

        lines.append(f"## {section['title']}")
        lines.append("")
        lines.append(
            f"Aktuell modelliert: **{len(section['rows'])}** Signale. "
            f"Nicht modelliert: **{len(unmapped_labels)}** Signale."
        )
        lines.append("")
        lines.append("### Modellierte Signale")
        lines.append("")
        append_table(lines, ["Signal", "Interner Wert", "Berechnung"], section["rows"])

        lines.append("### Nicht modellierte CSV-Signale")
        lines.append("")
        unmapped_rows = [(f"`{label}`", "-", UNMAPPED_NOTE) for label in unmapped_labels]
        append_table(lines, ["Signal", "Interner Wert", "Berechnung"], unmapped_rows)

    lines.append("## Spezialfaelle ausserhalb der Signaltabellen")
    lines.append("")
    append_table(
        lines,
        ["DID", "Verhalten", "Berechnung"],
        [
            (
                "`0x02B2`",
                "Zuendungs-Spezialfall",
                "Kein CSV-Signal. Antwort ist `0xAA`, wenn `_ignition_on > 0`, sonst `0x00`.",
            ),
            (
                "`701`",
                "Umgebungsdaten",
                "Kein einzelnes Signal. Die Response kombiniert `sMileage` mit aktuellem Zeitstempel aus `time.localtime()`.",
            ),
        ],
    )
    return "\n".join(lines) + "\n"


def main() -> None:
    DOC_PATH.write_text(build_document(), encoding="utf-8")
    print(f"Wrote {DOC_PATH}")


if __name__ == "__main__":
    main()
