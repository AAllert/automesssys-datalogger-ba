import math
from dataclasses import replace

from src.vehicles import VehicleConstants
from src.simulation.telemetry import TelemetrySnapshot
from src.state.vehicle_state import VehicleState


class DerivedValuesCalculator:

    def __init__(self, vehicle_constants: VehicleConstants):
        self.constants = vehicle_constants

    def calc_derived_values(self, snapshot: TelemetrySnapshot, dt: float) -> TelemetrySnapshot:
        """Enriches a snapshot with derived signals (thermal, cell voltages, current limits)."""
        c = self.constants

        battery_temp_span = min(c.temp_spread_max_c, abs(snapshot.hv_current_a) * c.temp_spread_current_coeff + c.temp_spread_base_c)
        battery_temp_min = snapshot.battery_temp_avg_c - battery_temp_span / 2.0
        battery_temp_max = snapshot.battery_temp_avg_c + battery_temp_span / 2.0

        cell_base_v = snapshot.hv_voltage_v / c.cell_count if c.cell_count else 0.0
        cell_delta_v = min(c.cell_spread_max_v, abs(snapshot.hv_current_a) * c.cell_spread_current_coeff + c.cell_spread_base_v)
        cell_voltage_min = max(c.cell_min_voltage_v, cell_base_v - cell_delta_v)
        cell_voltage_max = min(c.cell_max_voltage_v, cell_base_v + cell_delta_v)
        cell_voltage_sum = cell_base_v * c.cell_count

        thermal_load = max(0.0, snapshot.battery_temp_avg_c - snapshot.ambient_temp_c)
        cooling_demand = self._clamp(thermal_load * c.cooling_thermal_load_coeff + abs(snapshot.hv_current_a) * c.cooling_current_coeff, 0.0, 100.0)
        fan_current = self._clamp(cooling_demand * c.fan_current_per_demand_pct, 0.0, c.fan_max_current_a)
        compressor_speed = self._clamp(cooling_demand * c.compressor_rpm_per_demand_pct, 0.0, c.compressor_max_rpm)
        compressor_torque = self._clamp(cooling_demand * c.compressor_torque_per_demand_pct, 0.0, c.compressor_max_torque_nm)
        cabin_temp = snapshot.cabin_temp_c + (c.cabin_target_temp_c - snapshot.cabin_temp_c) * (1.0 - math.exp(-dt / c.cabin_heating_tau_s))
        coolant_temp = snapshot.ambient_temp_c + c.coolant_base_offset_c + thermal_load * c.coolant_thermal_load_coeff + fan_current * c.coolant_fan_coeff

        if snapshot.state == VehicleState.CHARGING:
            charger_current = abs(snapshot.hv_current_a)
            charger_voltage = snapshot.hv_voltage_v
        else:
            charger_current = 0.0
            charger_voltage = 0.0

        charge_limit = self._clamp(
            c.charge_current_max_a
            - max(0.0, snapshot.hv_soc_pct - c.charge_soc_derating_threshold_pct) * c.charge_soc_derating_rate
            - max(0.0, battery_temp_max - c.charge_temp_derating_threshold_c) * c.charge_temp_derating_rate,
            c.charge_current_min_a,
            c.charge_current_max_a,
        )
        discharge_limit = self._clamp(
            c.discharge_current_max_a
            - max(0.0, c.discharge_cold_threshold_c - snapshot.battery_temp_avg_c) * c.discharge_cold_derating_rate
            - max(0.0, snapshot.hv_soc_pct - c.discharge_soc_threshold_pct) * c.discharge_soc_derating_rate,
            c.discharge_current_min_a,
            c.discharge_current_max_a,
        )

        return replace(
            snapshot,
            cabin_temp_c=cabin_temp,
            coolant_temp_c=coolant_temp,
            battery_temp_min_c=battery_temp_min,
            battery_temp_max_c=battery_temp_max,
            cell_voltage_sum_v=cell_voltage_sum,
            cell_voltage_min_v=cell_voltage_min,
            cell_voltage_max_v=cell_voltage_max,
            charge_current_limit_a=charge_limit,
            discharge_current_limit_a=discharge_limit,
            charger_output_voltage_v=charger_voltage,
            charger_output_current_a=charger_current,
            fan_current_a=fan_current,
            fan_command_pct=cooling_demand,
            compressor_speed_rpm=compressor_speed,
            compressor_torque_nm=compressor_torque,
        )

    @staticmethod
    def _clamp(value: float, minimum: float, maximum: float) -> float:
        return max(minimum, min(value, maximum))
