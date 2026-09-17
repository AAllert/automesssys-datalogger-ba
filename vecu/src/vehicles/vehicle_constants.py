from dataclasses import dataclass

@dataclass
class VehicleConstants:
    """Complete vehicle specification used by all simulation components."""

    # === Motor/Drivetrain ===
    motor_power_kw: float = 150.0
    motor_torque_nm: float = 310.0
    motor_max_rpm: float = 16000.0
    gear_ratio: float = 9.73

    # === Kinematics ===
    wheel_radius_m: float = 0.35

    # === Battery Pack — Pack Level ===
    battery_capacity_ah: float = 205.0
    battery_capacity_kwh: float = 82.0
    battery_nominal_voltage: float = 400.0
    battery_min_voltage: float = 320.0
    battery_max_voltage: float = 460.0
    battery_internal_resistance: float = 0.08

    # === Battery Pack — Cell Level ===
    cell_count: int = 96
    cell_min_voltage_v: float = 2.5
    cell_max_voltage_v: float = 4.3
    cell_spread_current_coeff: float = 0.00008
    cell_spread_base_v: float = 0.006
    cell_spread_max_v: float = 0.04

    # === Thermal — Battery ===
    battery_thermal_mass_kg: float = 150.0
    battery_specific_heat: float = 900.0
    cooling_coefficient: float = 0.05
    ambient_temp_celsius: float = 20.0
    max_battery_temp: float = 45.0
    min_battery_temp: float = 15.0
    temp_spread_current_coeff: float = 0.01
    temp_spread_base_c: float = 1.0
    temp_spread_max_c: float = 6.0

    # === Thermal — Cabin & Coolant ===
    cabin_target_temp_c: float = 22.0
    cabin_heating_tau_s: float = 300.0
    coolant_base_offset_c: float = 6.0
    coolant_thermal_load_coeff: float = 0.25
    coolant_fan_coeff: float = 0.2

    # === Cooling System ===
    cooling_thermal_load_coeff: float = 4.0
    cooling_current_coeff: float = 0.06
    fan_max_current_a: float = 18.0
    fan_current_per_demand_pct: float = 0.12
    compressor_max_rpm: float = 6000.0
    compressor_rpm_per_demand_pct: float = 55.0
    compressor_max_torque_nm: float = 40.0
    compressor_torque_per_demand_pct: float = 0.16

    # === Current Limits ===
    charge_current_max_a: float = 250.0
    charge_current_min_a: float = 20.0
    charge_soc_derating_threshold_pct: float = 60.0
    charge_soc_derating_rate: float = 2.5
    charge_temp_derating_threshold_c: float = 35.0
    charge_temp_derating_rate: float = 4.0
    discharge_current_max_a: float = 320.0
    discharge_current_min_a: float = 40.0
    discharge_cold_threshold_c: float = 15.0
    discharge_cold_derating_rate: float = 6.0
    discharge_soc_threshold_pct: float = 90.0
    discharge_soc_derating_rate: float = 4.0

    # === Efficiency ===
    inverter_efficiency: float = 0.95
    motor_efficiency: float = 0.94
    regen_efficiency: float = 0.85

    # === Charging ===
    max_ac_charge_power_kw: float = 11.0
    max_dc_charge_power_kw: float = 170.0

    # === Aerodynamics ===
    drag_coefficient: float = 0.267
    frontal_area_m2: float = 2.36
    air_density: float = 1.2
    rolling_resistance: float = 0.01
    vehicle_mass_kg: float = 2100.0

    # === Performance ===
    top_speed_kmh: float = 160.0
    acceleration_0_100_s: float = 7.3

    # === Current ===
    standby_current: float = 5.0
    idle_current: float = 8.0
    aux_power_w: float = 2500.0 # Auxilary loads, e.g HVAC, lights, infotainment, 12V System
    max_regen_current: int = -150 # Max regen ~60 kW

    @property
    def drivetrain_efficiency(self) -> float:
        """Drivetrain efficiency: battery -> inverter -> motor -> wheels"""
        return self.inverter_efficiency * self.motor_efficiency

    @property
    def max_motor_force_n(self) -> float:
        return (self.motor_torque_nm * self.gear_ratio) / self.wheel_radius_m

    @property
    def max_acceleration_mps2(self) -> float:
        return self.max_motor_force_n / self.vehicle_mass_kg
