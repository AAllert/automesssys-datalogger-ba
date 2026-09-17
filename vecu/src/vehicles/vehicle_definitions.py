"""
Generic EV reference profile for the vECU simulation.

Baseline values are partly inspired by public EV reference data, but this model
is not a validated simulation of a specific production vehicle.
"""

from src.vehicles.vehicle_constants import VehicleConstants
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]

DEFAULT_VEHICLE_ID = "vw_id3"


class VehicleModule:
    """This class stores all neccessary Parameters for a specific vehicle. 
    """

    def __init__(self, vehicle_id: str, physical_constants: VehicleConstants, config_path: Path) -> None:
        self.vehicle_id = vehicle_id
        self.physical_constants = physical_constants
        self.config_path = config_path

VEHICLES: list[VehicleModule] = [
    VehicleModule("vw_id3", VehicleConstants(), REPO_ROOT / "configs" / "UdsConfig_v06_ID3.csv"),
    VehicleModule("mirai1", VehicleConstants(), REPO_ROOT / "configs" / "UdsConfig_v06_Mirai1.csv"),
    VehicleModule("mirai2", VehicleConstants(), REPO_ROOT / "configs" / "UdsConfig_v06_Mirai2_alles.csv")
]

def get_vehicle(identifier: str) -> VehicleModule:
    for vehicle in VEHICLES:
        if vehicle.vehicle_id == identifier:
            return vehicle
    raise KeyError(f"Unknown vehicle '{identifier}'. Available: {list_vehicles()}")

def get_default_vehicle() -> VehicleModule:
    return get_vehicle(DEFAULT_VEHICLE_ID)

def list_vehicles() -> list[str]:
    return [vehicle.vehicle_id for vehicle in VEHICLES]