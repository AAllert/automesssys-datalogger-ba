import random

from src.physics.physics_calculator import PhysicsCalculator
from src.physics.derived_values_calculator import DerivedValuesCalculator
from src.vehicles import VehicleConstants
from src.simulation.uds_encoder import UdsEncoder
from src.simulation.telemetry import TelemetrySnapshot
from src.state.state_manager import StateManager
from src.state.vehicle_state import VehicleState


def _clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(value, maximum))


class VehicleSimulator:
    """
    Main vehicle simulator - orchestrates state management, physics, and parameters.
    """

    BASE_VELOCITIES = {
        VehicleState.CREEPING: 5.0,
        VehicleState.DRIVING: 50.0,
        VehicleState.HIGHWAY: 120.0,
    }

    VELOCITY_VARIATION_PERCENT = 0.15  # ±15% maximum variation

    def __init__(
        self,
        state_manager: StateManager,
        physics_calc: PhysicsCalculator,
        uds_encoder: UdsEncoder,
        derived_values_calculator: DerivedValuesCalculator,
        vehicle_constants: VehicleConstants,
        seed: int | None = None,
    ):
        self.state_manager = state_manager
        self.physics = physics_calc
        self.uds_encoder = uds_encoder
        self.constants = vehicle_constants
        self.derived_values_calculator = derived_values_calculator
        self._rng = random.Random(seed)

        self.velocity_kmh = 0.0
        self.target_velocity_kmh = 0.0
        self.acceleration_mps2 = 0.0
        self.odometer_km = 0.0
        self.trip_distance_km = 0.0
        self.last_snapshot = TelemetrySnapshot(
            state=VehicleState.PARKED_OFF,
            cabin_temp_c=vehicle_constants.ambient_temp_celsius,
            ambient_temp_c=vehicle_constants.ambient_temp_celsius,
        )

        self._current_target_variation = 0.0
        self._variation_value = 0.0
        self._time_at_current_target = 0.0
        self._hold_duration = 0.0

    def update(self, delta_time: float) -> None:
        """Updates the complete simulation for one time step."""
        self.state_manager.update(delta_time, self.velocity_kmh)
        current_state = self.state_manager.get_current_state()

        self._update_target_velocity(current_state, delta_time)
        self._update_velocity(delta_time, current_state)

        motor_rpm = self.physics.calculate_motor_rpm(self.velocity_kmh)
        battery_current = self.physics.calculate_battery_current(
            self.velocity_kmh, current_state, self.acceleration_mps2
        )
        battery_voltage = self.physics.calculate_battery_voltage(
            battery_current, self.physics.get_soc()
        )
        battery_temp = self.physics.update_battery_temp(delta_time, battery_current)
        soc = self.physics.update_soc(delta_time, battery_current)

        if self.velocity_kmh > 0:
            distance_km = (self.velocity_kmh / 3600.0) * delta_time
            self.odometer_km += distance_km
            self.trip_distance_km += distance_km

        battery_power_kw = (battery_voltage * battery_current) / 1000.0
        ignition_on = 1.0 if current_state not in [VehicleState.PARKED_OFF, VehicleState.CHARGING] else 0.0
        terminal_15 = 1.0 if current_state != VehicleState.PARKED_OFF else 0.0

        snapshot = TelemetrySnapshot(
            state=current_state,
            velocity_kmh=self.velocity_kmh,
            target_velocity_kmh=self.target_velocity_kmh,
            acceleration_mps2=self.acceleration_mps2,
            motor_rpm=motor_rpm,
            terminal_15=terminal_15,
            ignition_on=ignition_on,
            odometer_total_km=self.odometer_km,
            trip_distance_km=self.trip_distance_km,
            accelerator_pedal_pct=self._calculate_accelerator_pedal_pct(current_state),
            ambient_temp_c=self.constants.ambient_temp_celsius,
            cabin_temp_c=self.last_snapshot.cabin_temp_c,
            hv_voltage_v=battery_voltage,
            hv_current_a=battery_current,
            hv_power_kw=battery_power_kw,
            hv_soc_pct=soc,
            battery_temp_avg_c=battery_temp,
        )

        self.last_snapshot = self.derived_values_calculator.calc_derived_values(snapshot, delta_time)

    def _calculate_accelerator_pedal_pct(self, state: VehicleState) -> float:
        if state in {VehicleState.PARKED_OFF, VehicleState.CHARGING}:
            return 0.0

        target_ratio = 0.0
        if self.constants.top_speed_kmh > 0:
            target_ratio = self.target_velocity_kmh / self.constants.top_speed_kmh

        demand = (target_ratio * 70.0) + max(0.0, self.acceleration_mps2) * 12.0
        if state == VehicleState.CREEPING:
            demand *= 0.6
        elif state == VehicleState.PARKED_IGN_ON:
            demand = 0.0
        return _clamp(demand, 0.0, 100.0)

    def _update_target_velocity(self, state: VehicleState, delta_time: float) -> None:
        """Sets target velocity based on current state with realistic variations."""
        if state in [VehicleState.PARKED_OFF, VehicleState.PARKED_IGN_ON, VehicleState.CHARGING]:
            self.target_velocity_kmh = 0.0
            self._variation_value = 0.0
            self._current_target_variation = 0.0
            self._time_at_current_target = 0.0
            return

        if self.state_manager.is_waiting_for_stop():
            self.target_velocity_kmh = 0.0
            return

        base_velocity = self.BASE_VELOCITIES.get(state, 0.0)

        if base_velocity > 0:
            self._update_velocity_variation(delta_time)
            variation_factor = 1.0 + (self._variation_value * self.VELOCITY_VARIATION_PERCENT)
            self.target_velocity_kmh = base_velocity * variation_factor
        else:
            self.target_velocity_kmh = 0.0

    def _update_velocity_variation(self, delta_time: float) -> None:
        """Updates velocity variation with realistic hold-and-transition pattern."""
        self._time_at_current_target += delta_time

        if self._time_at_current_target >= self._hold_duration:
            change_magnitude = self._rng.random()
            if change_magnitude < 0.6:
                new_target = self._rng.uniform(-0.33, 0.33)
            elif change_magnitude < 0.9:
                new_target = self._rng.uniform(-0.67, 0.67)
            else:
                new_target = self._rng.uniform(-1.0, 1.0)

            max_change = 0.5
            change = new_target - self._current_target_variation
            if abs(change) > max_change:
                new_target = self._current_target_variation + (max_change if change > 0 else -max_change)

            self._current_target_variation = new_target
            self._time_at_current_target = 0.0
            self._hold_duration = self._rng.uniform(3.0, 8.0)

        transition_rate = 0.01
        diff = self._current_target_variation - self._variation_value
        if abs(diff) > 0.001:
            max_step = transition_rate
            if abs(diff) <= max_step:
                self._variation_value = self._current_target_variation
            else:
                self._variation_value += max_step if diff > 0 else -max_step

    def _update_velocity(self, delta_time: float, state: VehicleState) -> None:
        """Updates velocity with realistic acceleration/deceleration based on vehicle specs."""
        if state in [VehicleState.PARKED_OFF, VehicleState.CHARGING]:
            self.velocity_kmh = 0.0
            self.acceleration_mps2 = 0.0
            return

        velocity_diff = self.target_velocity_kmh - self.velocity_kmh
        velocity_ms = self.velocity_kmh / 3.6

        if velocity_diff > 0:
            max_accel_mps2 = self._calculate_max_acceleration(velocity_ms)
            max_accel_kmh_per_s = max_accel_mps2 * 3.6
            velocity_change = min(velocity_diff, max_accel_kmh_per_s * delta_time)
        else:
            max_decel_mps2 = 3.0
            max_decel_kmh_per_s = max_decel_mps2 * 3.6
            velocity_change = max(velocity_diff, -max_decel_kmh_per_s * delta_time)

        self.velocity_kmh += velocity_change
        self.velocity_kmh = max(0.0, min(self.velocity_kmh, self.constants.top_speed_kmh))
        self.acceleration_mps2 = (velocity_change / 3.6) / delta_time if delta_time > 0 else 0.0

    def _calculate_max_acceleration(self, velocity_ms: float) -> float:
        """Calculate maximum acceleration based on motor characteristics."""
        c = self.constants
        max_wheel_torque = c.motor_torque_nm * c.gear_ratio
        max_force_torque = max_wheel_torque / c.wheel_radius_m
        accel_torque_limited = max_force_torque / c.vehicle_mass_kg

        if velocity_ms > 1.0:
            max_power_w = c.motor_power_kw * 1000 * c.drivetrain_efficiency
            accel_power_limited = max_power_w / (c.vehicle_mass_kg * velocity_ms)
        else:
            accel_power_limited = float("inf")

        max_accel = min(accel_torque_limited, accel_power_limited)
        return max_accel * 0.4

    def get_value_for_did(self, did: int) -> bytes:
        """Returns encoded UDS response for a DID."""
        return self.uds_encoder.encode(did, self.last_snapshot)

    def get_current_state(self) -> VehicleState:
        """Returns current vehicle state."""
        return self.state_manager.get_current_state()
