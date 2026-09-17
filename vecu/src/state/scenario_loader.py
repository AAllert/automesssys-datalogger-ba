import yaml
from dataclasses import dataclass
from .vehicle_state import VehicleState, is_valid_transition


class InvalidTransitionError(Exception):
    """Raised when a scenario contains an invalid state transition."""
    pass


@dataclass
class ScenarioStep:
    state: VehicleState
    duration: float


class YamlScenarioLoader:
    def __init__(self, yaml_path: str, validate_transitions: bool = True):
        """Load a scenario from a YAML file.
        
        Args:
            yaml_path: Path to the YAML scenario file
            validate_transitions: If True, validate all state transitions on load
            
        Raises:
            InvalidTransitionError: If validate_transitions is True and an invalid
                                   transition is found in the scenario
        """
        with open(yaml_path, 'r') as f:
            data = yaml.safe_load(f)
        
        self.name = data.get('name', 'Unnamed')
        self.loop = data.get('loop', False)
        self.steps = []
        
        for step in data.get('sequence', []):
            state = VehicleState[step['state']]  # String -> Enum
            duration = float(step['duration'])
            self.steps.append(ScenarioStep(state, duration))
        
        if validate_transitions:
            self._validate_transitions()
    
    def _validate_transitions(self) -> None:
        """Validate all state transitions in the scenario.
        
        Raises:
            InvalidTransitionError: If an invalid transition is found
        """
        if len(self.steps) < 2:
            return
        
        for i in range(len(self.steps) - 1):
            from_state = self.steps[i].state
            to_state = self.steps[i + 1].state
            
            if not is_valid_transition(from_state, to_state):
                raise InvalidTransitionError(
                    f"Invalid transition in scenario '{self.name}' at step {i + 1}: "
                    f"{from_state.name} -> {to_state.name} is not allowed. "
                    f"Check the state transition rules in vehicle_state.py."
                )
        
        # If looping, also validate transition from last to first state
        if self.loop and len(self.steps) >= 1:
            from_state = self.steps[-1].state
            to_state = self.steps[0].state
            
            if not is_valid_transition(from_state, to_state):
                raise InvalidTransitionError(
                    f"Invalid loop transition in scenario '{self.name}': "
                    f"{from_state.name} -> {to_state.name} is not allowed when looping. "
                    f"Check the state transition rules in vehicle_state.py."
                )
    
    def get_steps(self) -> list[ScenarioStep]:
        return self.steps
    
    def get_name(self) -> str:
        return self.name
    
    def should_loop(self) -> bool:
        return self.loop