from enum import Enum, auto

class VehicleState(Enum):
    """Possible states of the vehicle simulation."""
    PARKED_OFF = auto()
    CREEPING = auto() 
    PARKED_IGN_ON = auto()
    DRIVING = auto()
    HIGHWAY = auto()
    CHARGING = auto()


# Valid state transitions: from_state -> set of allowed to_states
# This defines the state machine for realistic vehicle behavior
VALID_TRANSITIONS: dict[VehicleState, set[VehicleState]] = {
    # PARKED_OFF: Vehicle is completely off
    # Can turn on ignition or start charging (e.g. scheduled charging)
    VehicleState.PARKED_OFF: {
        VehicleState.PARKED_IGN_ON,
        VehicleState.CHARGING,
    },
    
    # PARKED_IGN_ON: Ignition on, vehicle stationary
    # Can turn off, start creeping, or start charging
    # Must go through CREEPING before DRIVING/HIGHWAY
    VehicleState.PARKED_IGN_ON: {
        VehicleState.PARKED_OFF,
        VehicleState.CREEPING,
        VehicleState.CHARGING,
    },
    
    # CREEPING: Slow movement (parking maneuvers, traffic jam)
    # Can stop (parked) or accelerate to driving
    # Cannot go directly to highway
    VehicleState.CREEPING: {
        VehicleState.PARKED_IGN_ON,
        VehicleState.DRIVING,
    },
    
    # DRIVING: Normal city/road driving
    # Can slow down to creeping or speed up to highway
    # Cannot go directly to parked - must slow down first
    VehicleState.DRIVING: {
        VehicleState.CREEPING,
        VehicleState.HIGHWAY,
    },
    
    # HIGHWAY: High-speed driving
    # Can only slow down to driving
    # Cannot go directly to creeping or parked
    VehicleState.HIGHWAY: {
        VehicleState.DRIVING,
    },
    
    # CHARGING: Vehicle is charging
    # Can stop charging and return to parked (ignition on or off)
    VehicleState.CHARGING: {
        VehicleState.PARKED_IGN_ON,
        VehicleState.PARKED_OFF,
    },
}


def is_valid_transition(from_state: VehicleState, to_state: VehicleState) -> bool:
    """Check if a state transition is valid.
    
    Args:
        from_state: Current state
        to_state: Target state
        
    Returns:
        True if the transition is allowed, False otherwise
    """
    if from_state == to_state:
        return True  # Staying in the same state is always valid
    
    allowed_states = VALID_TRANSITIONS.get(from_state, set())
    return to_state in allowed_states


def get_valid_next_states(current_state: VehicleState) -> set[VehicleState]:
    """Get all valid states that can be transitioned to from the current state.
    
    Args:
        current_state: The current vehicle state
        
    Returns:
        Set of valid next states
    """
    return VALID_TRANSITIONS.get(current_state, set()).copy()