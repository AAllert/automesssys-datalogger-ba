from dataclasses import replace
import random

from src.config.config import Config
from src.physics import PhysicsCalculator, DerivedValuesCalculator
from src.simulation import SignalName
from src.simulation.vehicle_simulator import VehicleSimulator
from src.simulation.uds_encoder import UdsEncoder, DID_IGNITION
from src.state.vehicle_state import VehicleState
from src.vehicles import get_vehicle, list_vehicles


EXPECTED_VEHICLES = ["vw_id3", "mirai1", "mirai2"]
DEFAULT_VEHICLE_ID = "vw_id3"
EXPECTED_ROW_COUNT = 157
EXPECTED_ISOTP_PAIRS = 5


class StaticStateManager:
    """Test double: holds a fixed VehicleState and ignores time-based transitions."""

    def __init__(self, state: VehicleState) -> None:
        self._state = state

    def get_current_state(self) -> VehicleState:
        return self._state

    def update(self, delta_time: float, current_velocity_kmh: float = 0.0) -> None:
        pass

    def is_waiting_for_stop(self) -> bool:
        return False


def _build_simulator(
    vehicle_id: str = DEFAULT_VEHICLE_ID,
    state: VehicleState = VehicleState.DRIVING,
) -> tuple[VehicleSimulator, Config]:
    module = get_vehicle(vehicle_id)
    config = Config(module.config_path)
    constants = replace(module.physical_constants, ambient_temp_celsius=21.0)
    physics = PhysicsCalculator(constants)
    encoder = UdsEncoder(config)
    sim = VehicleSimulator(
        StaticStateManager(state),
        physics,
        encoder,
        DerivedValuesCalculator(constants),
        constants,
        random.seed(42)
    )
    return sim, config


def _did_for_label(config: Config, label: str) -> int:
    for row in config.get_rows():
        if row.label == label:
            return row.did
    raise KeyError(label)


def test_vehicle_definitions_are_registered():
    assert list_vehicles() == EXPECTED_VEHICLES


def test_config_parsing_for_vw_id3():
    module = get_vehicle(DEFAULT_VEHICLE_ID)
    config = Config(module.config_path)
    assert len(config) == EXPECTED_ROW_COUNT
    assert len(config.get_unique_isotp_addresses()) == EXPECTED_ISOTP_PAIRS


def test_simulator_populates_derived_signals():
    sim, config = _build_simulator()
    for _ in range(5):
        sim.update(1.0)

    config_labels = config.get_labels()
    for label in [
        SignalName.R_APP,
        SignalName.S_VEH_TRIP,
        SignalName.T_AMB,
        SignalName.U_SUM_CELL,
        SignalName.T_MIN_HV_BAT,
        SignalName.CUR_CHRG_LIM_DYN,
    ]:
        assert label in config_labels, f"Signal '{label}' missing from config"

    snap = sim.last_snapshot
    assert snap.accelerator_pedal_pct != 0.0
    assert snap.trip_distance_km != 0.0
    assert snap.ambient_temp_c != 0.0
    assert snap.cell_voltage_sum_v != 0.0
    assert snap.battery_temp_min_c != 0.0
    assert snap.charge_current_limit_a != 0.0


def test_uds_ignition_encoding_on():
    sim, _ = _build_simulator(state=VehicleState.DRIVING)
    for _ in range(5):
        sim.update(1.0)

    assert sim.get_value_for_did(DID_IGNITION) == bytes([0x62, 0x02, 0xB2, 0xAA])


def test_uds_ignition_encoding_off():
    sim, _ = _build_simulator(state=VehicleState.PARKED_OFF)
    sim.update(1.0)

    assert sim.get_value_for_did(DID_IGNITION) == bytes([0x62, 0x02, 0xB2, 0x00])


def test_uds_standard_signal_response_format():
    sim, config = _build_simulator()
    for _ in range(5):
        sim.update(1.0)

    # uCellMin1: 4-byte payload → total 7 bytes (SID + DID[2] + data[4])
    response = sim.get_value_for_did(_did_for_label(config, SignalName.U_CELL_MIN1))
    assert response[0] == 0x62
    assert len(response) == 7
