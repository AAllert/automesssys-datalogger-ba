"""
Main entry point to start the vECU simulator.
Initilizes all components and starts the simulation.
"""

import sys

# ── stdout redirect ───────────────────────────────────────────────────────────
# Capture the real stdout before redirecting so StdinTransport can write CAN
# frame responses there.
sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding="utf-8")
_can_out = sys.stdout
sys.stdout = sys.stderr
# ─────────────────────────────────────────────────────────────────────────────

import argparse
import time
from dataclasses import replace

from src.communication import IsoTpCodec, UdsHandler
from src.communication import Transport, StdinTransport, NativeCanTransport, UartTransport
from src.config import Config
from src.physics import DerivedValuesCalculator
from src.physics import PhysicsCalculator
from src.simulation import UdsEncoder
from src.simulation.vehicle_simulator import VehicleSimulator
from src.state.scenario_loader import YamlScenarioLoader
from src.state.state_manager import StateManager
from src.vehicles import get_vehicle, get_default_vehicle

def parse_args():
    parser = argparse.ArgumentParser(description="vECU stdin/stdout mode for HIL testing")
    parser.add_argument(
        "--transport",
        default="stdin",
        help="Tranport Backend, which should be used: can, usb or stdin. (default: stdin)"
    )
    parser.add_argument(
        "--vehicle",
        default=get_default_vehicle().vehicle_id,
        help=f"Vehicle profile to simulate (default: {get_default_vehicle().vehicle_id})"
    )
    parser.add_argument(
        "--scenario",
        default="scenarios/city_drive.yaml",
        help="Path to scenario YAML file (default: scenarios/city_drive.yaml)"
    )
    parser.add_argument(
        "--ambient-temp",
        type=float,
        default=20.0,
        help="Ambient temperature in °C (default: 20.0)"
    )
    parser.add_argument(
        "--update-rate",
        type=int,
        default=50,
        help="Simulation update rate in Hz (default: 50)"
    )
    parser.add_argument(
        "--port",
        type=str,
        help="Serial port of the datalogger (e.g. COM6, /dev/ttyUSB0)"
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="UART baud rate (default: 115200)"
    )

    return parser.parse_args()

def main():
    args = parse_args()

    if args.transport == "usb" and not args.port:
        print("Parser Error: --port is required when --transport=usb")
        return

    print("=" * 60)
    print("Loading vECU - Virtual ECU Simulator for HIL Testing")
    print("=" * 60)

    print("\n[1/6] Loading vehicle profile...")
    vehicle_module = get_vehicle(args.vehicle)
    vehicle_constants = replace(vehicle_module.physical_constants, ambient_temp_celsius=args.ambient_temp)
    print(f"[OK] Vehicle: {vehicle_module.vehicle_id}")

    print("\n[2/6] Loading scenario...")
    loader = YamlScenarioLoader(args.scenario)
    steps = loader.get_steps()
    print(f"[OK] Loaded '{loader.get_name()}' with {len(steps)} steps")

    print("\n[3/6] Initializing state manager...")
    state_manager = StateManager()
    state_manager.load_scenario(steps)
    state_manager.set_loop(loader.should_loop())
    print("[OK] State manager ready")

    print("\n[4/6] Initializing physics calculator...")
    physics_calc = PhysicsCalculator(vehicle_constants)
    print("[OK] Physics calculator ready")

    print("\n[5/6] Loading UDS configuration...")
    config = Config(vehicle_module.config_path)
    uds_encoder = UdsEncoder(config)
    print(f"[OK] UDS encoder ready with {len(config)} signals")

    print("\n[6/6] Initializing vehicle simulator...")
    derived_values_calculator = DerivedValuesCalculator(vehicle_constants)
    vehicle_sim = VehicleSimulator(
        state_manager,
        physics_calc,
        uds_encoder,
        derived_values_calculator,
        vehicle_constants,
    )
    print("[OK] Vehicle simulator ready")

    transport: Transport = None

    def handle_non_can_line(line: str) -> None:
        print(f"[LOG] {line}")

    if args.transport == "can":
        transport = NativeCanTransport(config)
    elif args.transport == "usb":
        transport = UartTransport(port=args.port, baud_rate=args.baud)
        transport.on_unknown_line = handle_non_can_line
    else:
        transport = StdinTransport(can_out=_can_out)
        transport.on_unknown_line = handle_non_can_line

    isotp_codec = IsoTpCodec(transport, config)
    uds_handler = UdsHandler(vehicle_sim)

    # Signal to the test runner that we are ready to process CAN frames
    print("SIMULATION RUNNING", flush=True)

    dt               = 1.0 / args.update_rate
    last_status_time = time.time()

    try:
        while True:
            loop_start = time.time()

            vehicle_sim.update(dt)

            # Drain all available CAN frames from stdin before sleeping
            while transport.in_waiting > 0:
                frame = transport.read_frame()
                if frame:
                    print(f"[RX] CAN {frame.can_id:X} {frame.data.hex(' ').upper()}", flush=True)
                    request = isotp_codec.process_frame(frame)
                    if request:
                        response = uds_handler.process(request.can_id, request.payload)
                        #time.sleep(0.7)
                        isotp_codec.send_response(request.can_id, response)

            now = time.time()
            if now - last_status_time >= 1.0:
                state = vehicle_sim.get_current_state()
                velocity = vehicle_sim.velocity_kmh
                soc = physics_calc.get_soc()
                temp = physics_calc.battery_temp
                rpm = vehicle_sim.last_snapshot.motor_rpm
                term15 = vehicle_sim.last_snapshot.terminal_15
                print(
                    f"State: {state.name:15s} | "
                    f"Time: {state_manager.get_time_in_state():5.1f}s | "
                    f"Progress: {state_manager.get_progress()*100:5.1f}% | "
                    f"Speed: {velocity:5.1f} km/h | "
                    f"SOC: {soc:5.1f}% | "
                    f"T_bat: {temp:5.1f}°C | "
                    f"term15: {term15} | ",
                    #f"RPM: {rpm:5.0f}",
                    flush=True,
                )
                last_status_time = now

            elapsed    = time.time() - loop_start
            time.sleep(max(0.0, dt - elapsed))

    except KeyboardInterrupt:
        print("\n[STOP] Simulation gestoppt", flush=True)
    finally:
        transport.close()
        print("[OK] Shutdown complete", flush=True)


if __name__ == "__main__":
    main()
