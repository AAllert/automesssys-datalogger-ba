from dataclasses import dataclass

from src.state.vehicle_state import VehicleState


@dataclass
class TelemetrySnapshot:
    """Normalized runtime telemetry used by car-specific signal adapters."""

    state: VehicleState
    velocity_kmh: float = 0.0
    target_velocity_kmh: float = 0.0
    acceleration_mps2: float = 0.0
    motor_rpm: float = 0.0

    terminal_15: float = 0.0
    ignition_on: float = 0.0
    odometer_total_km: float = 0.0
    trip_distance_km: float = 0.0
    accelerator_pedal_pct: float = 0.0

    ambient_temp_c: float = 0.0
    cabin_temp_c: float = 0.0
    coolant_temp_c: float = 0.0

    hv_voltage_v: float = 0.0
    hv_current_a: float = 0.0
    hv_power_kw: float = 0.0
    hv_soc_pct: float = 0.0

    battery_temp_avg_c: float = 0.0
    battery_temp_min_c: float = 0.0
    battery_temp_max_c: float = 0.0
    cell_voltage_sum_v: float = 0.0
    cell_voltage_min_v: float = 0.0
    cell_voltage_max_v: float = 0.0

    charge_current_limit_a: float = 0.0
    discharge_current_limit_a: float = 0.0
    charger_output_voltage_v: float = 0.0
    charger_output_current_a: float = 0.0

    # Mirai Hydrogen fields
    stack_voltage_v: float = 0.0
    stack_current_a: float = 0.0
    hydrogen_pressure_high: float = 0.0
    pump_or_compressor_speed_rpm: float = 0.0

    fan_current_a: float = 0.0
    fan_command_pct: float = 0.0
    compressor_speed_rpm: float = 0.0
    compressor_torque_nm: float = 0.0
