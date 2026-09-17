from src.vehicles import VehicleConstants
from src.state.vehicle_state import VehicleState
import math

class PhysicsCalculator:
    def __init__(self, vehicle_constants: VehicleConstants):
        self.constants = vehicle_constants or VehicleConstants()
        self.battery_temp = self.constants.ambient_temp_celsius
        self.soc = 80.0  # Initial State of Charge in percentage
        self.last_current = 0.0  # For acceleration current calculation
        self.cooling_active = False  # Battery cooling system state

    def calculate_motor_rpm(self, velocity_kmh: float) -> float:
        """
        Calculates motor RPM from vehicle velocity in km/h.
        
        Formula:
        1. Convert velocity from km/h to m/s.
        2. Wheel speed -> wheel RPM.
        3. Wheel RPM * gear_ratio -> motor RPM.
        """
        if velocity_kmh <= 0:
            return 0.0
        
        velocity_ms = velocity_kmh / 3.6
        wheel_circumference = 2 * math.pi * self.constants.wheel_radius_m
        wheel_rpm = (velocity_ms / wheel_circumference) * 60
        motor_rpm = wheel_rpm * self.constants.gear_ratio

        return motor_rpm
    
    def calculate_battery_current(self, velocity_kmh: float, state: VehicleState, 
                                   acceleration_mps2: float = 0.0) -> float:
        """
        Calculates battery current in Amperes based on vehicle velocity, state, and acceleration.
        
        Includes:
        - Constant speed power (air drag + rolling resistance)
        - Acceleration power (F = m * a)
        - Auxiliary loads (HVAC, electronics)
        - Inverter and motor losses
        
        Positive: Discharging (driving)
        Negative: Charging (regenerative braking or plug-in charging)
        """
        if state == VehicleState.PARKED_OFF:
            return 0.0
        
        elif state == VehicleState.PARKED_IGN_ON:
            # Standby: 12V system, infotainment, BMS
            return self.constants.standby_current  # ~2 kW standby
        
        elif state == VehicleState.CHARGING:
            # AC charging at 11 kW (realistic for home/destination charging)
            charge_power_w = self.constants.max_ac_charge_power_kw * 1000
            # Charging current (negative = charging)
            # Account for charging losses (~90% efficiency)
            charging_efficiency = 0.90
            current = -(charge_power_w * charging_efficiency) / self.constants.battery_nominal_voltage
            return current  # ~-25A at 11kW
        
        elif state in [VehicleState.DRIVING, VehicleState.HIGHWAY, VehicleState.CREEPING]:
            if velocity_kmh <= 0:
                return self.constants.idle_current  # Idle: HVAC, electronics (~3 kW)
            
            velocity_ms = velocity_kmh / 3.6
            
            # === 1. Resistance Forces (constant speed) ===
            # Air resistance: F_drag = 0.5 * ρ * c_d * A * v²
            drag_force = (0.5 * self.constants.air_density * 
                         self.constants.drag_coefficient * 
                         self.constants.frontal_area_m2 * 
                         velocity_ms ** 2)
            
            # Rolling resistance: F_roll = c_r * m * g
            rolling_force = (self.constants.rolling_resistance * 
                           self.constants.vehicle_mass_kg * 
                           9.81)
            
            resistance_force = drag_force + rolling_force
            
            # === 2. Acceleration Force ===
            # F_accel = m * a (positive when accelerating)
            accel_force = self.constants.vehicle_mass_kg * acceleration_mps2
            
            # === 3. Total Mechanical Power ===
            total_force = resistance_force + accel_force
            mechanical_power_w = total_force * velocity_ms
            
            if mechanical_power_w >= 0:
                # Driving/accelerating: divide by efficiency (more electrical power needed)
                electrical_power_w = mechanical_power_w / self.constants.drivetrain_efficiency
            else:
                # Regenerative braking: multiply by efficiency (less power recovered)
                regen_efficiency = self.constants.drivetrain_efficiency * self.constants.regen_efficiency  # Additional regen losses
                electrical_power_w = mechanical_power_w * regen_efficiency
            
            
            
            # === 6. Total Current ===
            total_power_w = electrical_power_w + self.constants.aux_power_w
            current = total_power_w / self.constants.battery_nominal_voltage
            
            # Limit regenerative current (battery protection)
            if current < self.constants.max_regen_current:
                current = self.constants.max_regen_current
            
            self.last_current = current
            return current
        
        return 0.0
    
    def calculate_battery_voltage(self, current: float, soc: float) -> float:
        """
        Calculates battery voltage from current and SOC.
        
        U = U_nominal - (I * R_internal) + SOC_correction
        """
        # Base voltage depends on SOC (simplified linear model)
        soc_factor = soc / 100.0
        base_voltage = self.constants.battery_nominal_voltage * (0.9 + 0.1 * soc_factor)
        
        # Voltage drop due to internal resistance
        voltage_drop = abs(current) * self.constants.battery_internal_resistance
        
        # Adjust based on charging/discharging
        if current < 0:  # Charging
            voltage = base_voltage + voltage_drop
        else:  # Discharging
            voltage = base_voltage - voltage_drop
        
        return voltage
    
    def update_battery_temp(self, delta_time: float, current: float) -> float:
        """
        Updates battery temperature based on current flow with realistic thermal model.

        Heat sources:
        1. Battery internal resistance (I²R losses)
        2. Electrochemical heating (entropy changes during charge/discharge)
        3. Regenerative braking heat (current reversal losses)

        Cooling:
        1. Passive cooling (convection to ambient)
        2. Active liquid cooling (when temp > threshold)

        The generic reference vehicle assumes active liquid cooling to keep the
        pack in a practical operating range.
        """
        # === 1. Heat Generation ===

        # I²R losses (Joule heating) - main heat source
        # Increased internal resistance effect for more realistic heating
        joule_heat_w = (current ** 2) * self.constants.battery_internal_resistance

        # Electrochemical/entropic heating (~15-20% additional heat during high current)
        # More pronounced during fast charging and high-power discharge
        entropic_factor = 0.18 * abs(current) / 100.0  # Scales with current
        entropic_heat_w = joule_heat_w * entropic_factor

        # Additional heating during regenerative braking (current reversal losses)
        regen_heat_w = 0.0
        if current < -50:  # Significant regen braking
            # Additional 10% heat from rectification and battery charging losses
            regen_heat_w = abs(joule_heat_w) * 0.10

        total_heat_w = joule_heat_w + entropic_heat_w + regen_heat_w

        # === 2. Temperature Rise from Heat ===
        # Reduced thermal mass for faster, more realistic temperature response
        effective_thermal_mass = self.constants.battery_thermal_mass_kg * 0.7
        temp_rise = (total_heat_w * delta_time) / (
            effective_thermal_mass * self.constants.battery_specific_heat
        )

        # === 3. Cooling ===

        # Passive cooling (always active) - reduced for more realistic heat buildup
        passive_cooling_coeff = 0.005  # Natural convection (reduced from 0.01)
        passive_cooling = (
            passive_cooling_coeff *
            (self.battery_temp - self.constants.ambient_temp_celsius) *
            delta_time
        )

        # Active liquid cooling (activates when temp exceeds threshold)
        active_cooling = 0.0
        if self.battery_temp > 28.0:  # Cooling activates above 28°C (lowered from 30°C)
            self.cooling_active = True
            # Cooling power proportional to temperature above setpoint
            temp_above_setpoint = self.battery_temp - 25.0  # Target 25°C (more aggressive)
            # Reduced cooling coefficient for more realistic temperature rise
            active_cooling = (
                self.constants.cooling_coefficient * 0.6 *
                temp_above_setpoint *
                delta_time
            )
        elif self.battery_temp < 26.0:
            self.cooling_active = False

        total_cooling = passive_cooling + active_cooling

        # === 4. Battery Heating (cold weather) ===
        heating = 0.0
        if self.battery_temp < self.constants.min_battery_temp:
            # Battery heater activates to warm cells
            heating = 0.02 * (self.constants.min_battery_temp - self.battery_temp) * delta_time

        # === 5. Update Temperature ===
        self.battery_temp += temp_rise - total_cooling + heating

        # Clamp to realistic range
        self.battery_temp = max(-20.0, min(60.0, self.battery_temp))

        return self.battery_temp
    
    def update_soc(self, delta_time: float, current: float) -> float:
        """
        Updates State of Charge based on current flow.
        
        ΔSOC = (I * Δt) / (C * 3600) * 100
        where C is capacity in Ah, time in seconds
        """
        # Current in Ah over this time step
        charge_delta_ah = (current * delta_time) / 3600.0
        
        # SOC change in percent
        soc_delta = (charge_delta_ah / self.constants.battery_capacity_ah) * 100.0
        
        # Update SOC (negative current = charging = increase SOC)
        self.soc -= soc_delta
        
        # Clamp to valid range
        self.soc = max(0.0, min(100.0, self.soc))
        
        return self.soc
    
    def get_soc(self) -> float:
        """Returns current State of Charge"""
        return self.soc
