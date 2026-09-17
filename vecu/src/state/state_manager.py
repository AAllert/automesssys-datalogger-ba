from src.state.vehicle_state import VehicleState, is_valid_transition

# Threshold velocity below which the vehicle is considered "stopped"
STOP_VELOCITY_THRESHOLD_KMH = 1.0

# States that require the vehicle to be stopped before entering
STATIONARY_STATES = {
    VehicleState.PARKED_OFF,
    VehicleState.PARKED_IGN_ON,
    VehicleState.CHARGING,
}

class StateManager:
    def __init__(self):
        self.scenario_steps = []
        self.current_step_index = 0
        self.time_in_state = 0.0
        self.loop = False
        self._waiting_for_stop = False  # True when duration expired but waiting for vehicle to stop

    def load_scenario(self, scenario_steps: list) -> None:
        """Loads a sequence of states"""
        self.scenario_steps = scenario_steps
        self.current_step_index = 0
        self.time_in_state = 0.0
        self._waiting_for_stop = False
        if self.scenario_steps:
            self.current_state = self.scenario_steps[0].state

    def set_loop(self, loop: bool) -> None:
        self.loop = loop

    def get_current_state(self) -> VehicleState:
        """Returns the current state of the vehicle."""
        if not self.scenario_steps:
            return VehicleState.PARKED_OFF
        return self.scenario_steps[self.current_step_index].state
    
    def update(self, delta_time: float, current_velocity_kmh: float = 0.0) -> None:
        """Updates the state machine, respecting physical constraints.
        
        State transitions to stationary states (PARKED_OFF, PARKED_IGN_ON, CHARGING)
        only occur when the vehicle velocity is below the stop threshold.
        """
        if not self.scenario_steps:
            return
        
        self.time_in_state += delta_time
        current_step = self.scenario_steps[self.current_step_index]
        
        # Check if duration has expired
        if self.time_in_state >= current_step.duration:
            # Determine the next state
            next_step_index = self._get_next_step_index()
            next_state = self.scenario_steps[next_step_index].state
            
            # Check if transition is allowed
            if self._can_transition_to(next_state, current_velocity_kmh):
                self._transition_to_next_step()
                self._waiting_for_stop = False
            else:
                # Duration expired but can't transition yet - waiting for vehicle to stop
                self._waiting_for_stop = True

    def _get_next_step_index(self) -> int:
        """Returns the index of the next step (handles looping)."""
        next_index = self.current_step_index + 1
        if next_index >= len(self.scenario_steps):
            if self.loop:
                return 0
            else:
                return len(self.scenario_steps) - 1
        return next_index
    
    def _can_transition_to(self, next_state: VehicleState, current_velocity_kmh: float) -> bool:
        """Checks if transition to the next state is allowed.
        
        Validates both:
        1. State machine rules (valid transitions from vehicle_state.py)
        2. Physical constraints (stationary states require vehicle to be stopped)
        """
        current_state = self.get_current_state()
        
        # This should never happen if the scenario was validated on load.
        # If it does, it's a programming error — fail loudly instead of deadlocking.
        if not is_valid_transition(current_state, next_state):
            raise RuntimeError(
                f"Invalid state transition encountered at runtime: "
                f"{current_state.name} -> {next_state.name}. "
                "Scenario must be validated before loading into StateManager."
            )
        
        # Check physical constraints for stationary states
        if next_state in STATIONARY_STATES:
            return current_velocity_kmh < STOP_VELOCITY_THRESHOLD_KMH
        
        return True
    
    def _transition_to_next_step(self) -> None:
        """Performs the actual state transition."""
        self.current_step_index += 1
        self.time_in_state = 0.0
        
        if self.current_step_index >= len(self.scenario_steps):
            if self.loop:
                self.current_step_index = 0
            else:
                self.current_step_index = len(self.scenario_steps) - 1

    def get_time_in_state(self) -> float:
        """Returns the time spent in the current state in seconds."""
        return self.time_in_state
    
    def get_progress(self) -> float:
        """Progress through the entire scenario (0.0 - 1.0)"""
        if not self.scenario_steps:
            return 0.0
        return self.current_step_index / len(self.scenario_steps)
    
    def is_waiting_for_stop(self) -> bool:
        """Returns True if the state machine is waiting for the vehicle to stop."""
        return self._waiting_for_stop