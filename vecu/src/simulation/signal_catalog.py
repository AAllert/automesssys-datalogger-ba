from dataclasses import dataclass
from typing import Callable

from src.simulation.signal_names import SignalName
from src.simulation.telemetry import TelemetrySnapshot


@dataclass
class CatalogEntry:
    """Display metadata and value extractor for one computed signal label."""

    label: SignalName
    title: str
    unit: str
    decimals: int
    extractor: Callable[[TelemetrySnapshot], float]
    value_map: dict[int, str] | None = None

    def format_value(self, value: float) -> str:
        if self.value_map is not None:
            mapped = self.value_map.get(int(round(value)))
            if mapped is not None:
                return mapped
        if self.decimals <= 0:
            return f"{value:.0f}"
        return f"{value:.{self.decimals}f}"


def _entry(
    label: SignalName,
    title: str,
    unit: str = "",
    decimals: int = 2,
    extractor: Callable[[TelemetrySnapshot], float] = lambda s: 0.0,
    value_map: dict[int, str] | None = None,
) -> CatalogEntry:
    return CatalogEntry(
        label=label,
        title=title,
        unit=unit,
        decimals=decimals,
        extractor=extractor,
        value_map=value_map,
    )


SIGNAL_CATALOG: dict[SignalName, CatalogEntry] = {
    e.label: e
    for e in (
        _entry(SignalName.V_VEH1, "Vehicle Speed ECU1", "km/h", 2, lambda s: s.velocity_kmh),
        _entry(SignalName.V_VEH2, "Vehicle Speed ECU2", "km/h", 2, lambda s: s.velocity_kmh),
        _entry(SignalName.V_VEH3, "Vehicle Speed ECU3", "km/h", 2, lambda s: s.velocity_kmh),
        _entry(SignalName.V_VEH4, "Vehicle Speed ECU4", "km/h", 2, lambda s: s.velocity_kmh),
        _entry(SignalName.V_VEH5, "Vehicle Speed ECU5", "km/h", 2, lambda s: s.velocity_kmh),
        _entry(SignalName.N_EM1, "Motor Speed 1", "rpm", 0, lambda s: s.motor_rpm),
        _entry(SignalName.N_EM2, "Motor Speed 2", "rpm", 0, lambda s: s.motor_rpm),
        _entry(SignalName.N_EM_TRAC, "Traction Motor Speed", "rpm", 0, lambda s: s.motor_rpm),
        _entry(SignalName.CUR_HV_BAT1, "HV Battery Current", "A", 2, lambda s: s.hv_current_a),
        _entry(SignalName.CUR2_HV_BAT, "HV Battery Current 2", "A", 2, lambda s: s.hv_current_a),
        _entry(SignalName.CUR_DC_LINK, "DC Link Current", "A", 2, lambda s: s.hv_current_a),
        _entry(SignalName.U_HV_BAT, "HV Battery Voltage", "V", 2, lambda s: s.hv_voltage_v),
        _entry(SignalName.U_HV, "HV System Voltage", "V", 2, lambda s: s.hv_voltage_v),
        _entry(SignalName.U_DC_LINK, "DC Link Voltage", "V", 2, lambda s: s.hv_voltage_v),
        _entry(SignalName.T_HV_BAT, "Battery Average Temperature", "C", 2, lambda s: s.battery_temp_avg_c),
        _entry(SignalName.R_SOC_BAT, "Battery State of Charge", "%", 2, lambda s: s.hv_soc_pct),
        _entry(SignalName.R_SOC_BAT2, "Battery State of Charge 2", "%", 2, lambda s: s.hv_soc_pct),
        _entry(SignalName.A_VEH, "Vehicle Acceleration", "mg", 2, lambda s: s.acceleration_mps2 * 1000.0),
        _entry(SignalName.S_MILEAGE, "Total Odometer", "km", 3, lambda s: s.odometer_total_km),
        _entry(
            SignalName.ST_TERM15, "Terminal 15", "", 0,
            lambda s: s.terminal_15, {0: "Off", 1: "On"},
        ),
        _entry(
            SignalName.IGNITION_ON, "Ignition", "", 0,
            lambda s: s.ignition_on, {0: "Off", 1: "On"},
        ),
        _entry(SignalName.R_APP, "Accelerator Pedal", "%", 2, lambda s: s.accelerator_pedal_pct),
        _entry(SignalName.S_VEH_TRIP, "Trip Distance", "km", 3, lambda s: s.trip_distance_km),
        _entry(SignalName.T_AMB, "Ambient Temperature", "C", 2, lambda s: s.ambient_temp_c),
        _entry(SignalName.T_CAB, "Cabin Temperature", "C", 2, lambda s: s.cabin_temp_c),
        _entry(SignalName.CUR_FAN, "Cooling Fan Current", "A", 2, lambda s: s.fan_current_a),
        _entry(SignalName.R_FAN_DES, "Cooling Fan Command", "%", 2, lambda s: s.fan_command_pct),
        _entry(SignalName.N_CMPR, "Compressor Speed", "rpm", 0, lambda s: s.compressor_speed_rpm),
        _entry(SignalName.TQ_CMPR, "Compressor Torque", "Nm", 2, lambda s: s.compressor_torque_nm),
        _entry(SignalName.U_SUM_CELL, "Cell Voltage Sum", "V", 2, lambda s: s.cell_voltage_sum_v),
        _entry(SignalName.U_CELL_MIN1, "Minimum Cell Voltage", "V", 3, lambda s: s.cell_voltage_min_v),
        _entry(SignalName.U_CELL_MAX1, "Maximum Cell Voltage", "V", 3, lambda s: s.cell_voltage_max_v),
        _entry(SignalName.T_MIN_HV_BAT, "Minimum Battery Temperature", "C", 2, lambda s: s.battery_temp_min_c),
        _entry(SignalName.T_MAX_HV_BAT, "Maximum Battery Temperature", "C", 2, lambda s: s.battery_temp_max_c),
        _entry(SignalName.CUR_CHRG_LIM_DYN, "Dynamic Charge Current Limit", "A", 2, lambda s: s.charge_current_limit_a),
        _entry(SignalName.CUR_LIM_DYN_DCHRG, "Dynamic Discharge Current Limit", "A", 2, lambda s: s.discharge_current_limit_a),
    )
}
