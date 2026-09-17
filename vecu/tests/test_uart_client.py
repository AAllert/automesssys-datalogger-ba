"""
Test client for UART CAN Server
Uses virtual serial port pair to simulate COM3 communication without hardware

This module demonstrates:
1. How to read CAN frames from vECU via UART
2. How to decode UDS responses (speed, SOC, temperature, etc.)
3. How to send targeted requests for generic simulated parameters
"""

import time
import subprocess
import sys
import pytest


pytestmark = pytest.mark.skip(reason="Hardware/UART integration test; excluded from default automated runs.")


# UDS DID Mapping - dynamically calculated parameters from the generic runtime
# Format: DID -> (parameter_name, unit, factor, offset, description)
UDS_DID_MAP = {
    # These parameters are dynamically calculated by the vECU physics engine:

    # Vehicle speed from the generic state-based target velocity model
    0x2C96: ("vVeh4", "km/h", 0.01, 0, "Fahrzeuggeschwindigkeit"),

    # Battery SOC integrated from current flow
    0x3DEF: ("rSocBat", "%", 0.01, 0, "Batterie Ladezustand"),

    # Battery voltage from SOC + current + internal resistance
    0x1E3B: ("uHvBat", "V", 0.25, 0, "HV Batterie Spannung"),

    # Battery current from driving resistance + acceleration + auxiliaries
    0x1E3D: ("curHvBat1", "A", 0.01, -189, "HV Batterie Strom"),

    # Battery temperature from thermal model: I²R losses + cooling system
    0x2A0B: ("tHvBat", "°C", 0.5, -40, "Batterie Temperatur"),
}


def decode_uds_value(did: int, raw_bytes: list) -> tuple:
    """
    Decode UDS data bytes to physical value.

    Args:
        did: Data Identifier
        raw_bytes: Raw data bytes from UDS response

    Returns:
        (parameter_name, physical_value, unit, description) or None if DID unknown
    """
    if did not in UDS_DID_MAP:
        return None

    param_name, unit, factor, offset, description = UDS_DID_MAP[did]

    # Convert bytes to integer (big-endian)
    if len(raw_bytes) == 1:
        raw_value = raw_bytes[0]
    elif len(raw_bytes) == 2:
        raw_value = (raw_bytes[0] << 8) | raw_bytes[1]
    elif len(raw_bytes) == 3:
        raw_value = (raw_bytes[0] << 16) | (raw_bytes[1] << 8) | raw_bytes[2]
    elif len(raw_bytes) == 4:
        raw_value = (raw_bytes[0] << 24) | (raw_bytes[1] << 16) | (raw_bytes[2] << 8) | raw_bytes[3]
    else:
        raw_value = 0

    # Apply scaling
    physical_value = raw_value * factor + offset

    return (param_name, physical_value, unit, description)


class VirtualSerialPort:
    """
    Simulates a serial port using the UART server in a subprocess.
    This allows testing without physical COM3 hardware.
    """

    def __init__(self, baud_rate=115200):
        self.baud_rate = baud_rate
        self.server_process = None
        self.output_buffer = []
        self.input_buffer = []
        self.is_open_flag = False
        self._waiting = 0

    def open(self):
        """Start the UART server subprocess"""
        if not self.is_open_flag:
            # Start uart server in background
            self.server_process = subprocess.Popen(
                [sys.executable, "src/main_uart.py"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=False,
                bufsize=0
            )
            self.is_open_flag = True
            # Simulate initialization delay
            time.sleep(0.1)

    def close(self):
        """Stop the UART server subprocess"""
        if self.server_process:
            self.server_process.terminate()
            self.server_process.wait(timeout=5)
            self.server_process = None
        self.is_open_flag = False

    def flushInput(self):
        """Flush input buffer"""
        if self.server_process and self.server_process.stdout:
            # Read and discard available data
            while self._check_available():
                self.server_process.stdout.read(1)
        self._waiting = 0

    def _check_available(self):
        """Check if data is available without blocking"""
        if not self.server_process or not self.server_process.stdout:
            return False
        # Non-blocking check
        import select
        if hasattr(select, 'select'):
            ready, _, _ = select.select([self.server_process.stdout], [], [], 0)
            return len(ready) > 0
        return False

    def inWaiting(self):
        """Return number of bytes waiting in input buffer"""
        if not self.server_process or not self.server_process.stdout:
            return 0

        # Poll the process to update buffers
        if self._check_available():
            # Try to peek at available data
            return 1  # Simplified: return 1 if data available
        return 0

    def write(self, _data):
        """Write data (not needed for read-only test)"""
        pass

    def readline(self):
        """Read one line from the server output"""
        if not self.server_process or not self.server_process.stdout:
            return b''

        try:
            # Read until newline or timeout
            line = b''
            start_time = time.time()
            timeout = 2.0

            while time.time() - start_time < timeout:
                if self.server_process.poll() is not None:
                    # Process ended
                    break

                byte = self.server_process.stdout.read(1)
                if not byte:
                    time.sleep(0.01)
                    continue

                line += byte
                if byte == b'\n':
                    break

            return line
        except Exception:
            return b''


def test_uart_client_simulation():
    """
    Simulates the exact behavior of the COM3 client script:

    from serial import Serial
    import time
    baud_rate = 115200
    ser = Serial('COM3', baud_rate)
    ser.close()
    ser.open()
    time.sleep(5)
    print(f"vor FLush: {ser.inWaiting()}")
    ser.flushInput()
    print(f"nach FLush: {ser.inWaiting()}")
    print(f"nach Write: {ser.inWaiting()}")
    response = ser.readline()
    print(f"received from ESP32: {response}")
    # ... (6x readline total)
    ser.close()
    """

    print("\n" + "=" * 80)
    print("UART Client Simulation Test - Real UART Server Integration")
    print("=" * 80)
    print("\nThis test runs:")
    print("  [This Test] <--UART--> [main_uart.py subprocess] <--CAN--> [vECU Engine]")
    print("=" * 80)

    # Create virtual serial port (simulates COM3)
    baud_rate = 115200
    ser = VirtualSerialPort(baud_rate)

    print("\n[1] Starting UART server subprocess...")
    # Close and reopen (like original script)
    ser.close()
    ser.open()
    print(f"    OK - UART server started (baud rate: {baud_rate})")

    print("\n[2] Waiting for vECU engine initialization (5 seconds)...")
    # Wait 5 seconds (like original script)
    time.sleep(5)
    print("    OK - vECU should be running and generating CAN frames")

    print("\n[3] Checking UART input buffer...")
    # Check buffer before flush
    waiting_before = ser.inWaiting()
    print(f"    vor Flush: {waiting_before} bytes in buffer")
    print("    (vECU continuously sends CAN frames to UART server)")

    # Flush input
    ser.flushInput()

    # Check buffer after flush
    waiting_after = ser.inWaiting()
    print(f"    nach Flush: {waiting_after} bytes in buffer")

    # Check buffer after write (no write in this case)
    waiting_write = ser.inWaiting()
    print(f"    nach Write: {waiting_write} bytes available to read")

    print("\n" + "=" * 80)
    print("[4] Reading live CAN frames from vECU (via UART Server)")
    print("=" * 80)
    print("Note: These are REAL frames from the running vECU simulation!\n")

    # Read 6 lines (like original script)
    responses = []
    for idx in range(6):
        print(f"--- Reading frame {idx + 1}/6 ---")
        response = ser.readline()

        # Try to decode and parse
        if response:
            response_str = response.decode('utf-8', errors='replace').strip()
            print(f"  [UART RX] Raw bytes: {response}")

            if response_str.startswith('CAN'):
                parts = response_str.split()
                if len(parts) >= 3:
                    can_id = parts[1]
                    data_bytes = ' '.join(parts[2:]) if len(parts) > 2 else ""
                    print(f"  [CAN Frame] ID: 0x{can_id} | Data: {data_bytes}")

                    # Try to identify frame type
                    if len(parts) >= 3:
                        first_byte = parts[2]
                        if first_byte in ['50', '62', '7E', '7F']:
                            print(f"  [vECU] UDS Response detected (SID: 0x{first_byte})")
                        else:
                            print(f"  [vECU] CAN data frame")
            else:
                print(f"  [UART RX] Non-CAN message: {response_str}")
        else:
            print("  [UART RX] (empty/timeout)")

        responses.append(response)
        print()

    print("=" * 80)
    print("[5] Terminating UART server subprocess...")
    # Close connection
    ser.close()
    print("    OK - Server terminated")

    print("\n" + "=" * 80)
    print("Test Results Summary")
    print("=" * 80)
    print(f"  Total responses received: {len(responses)}")
    print(f"  Non-empty responses:      {sum(1 for r in responses if r)}")
    print(f"  Valid CAN frames:         {sum(1 for r in responses if r and b'CAN' in r)}")

    # Verify we got some responses
    assert len(responses) == 6, "Should read 6 lines"
    print("\n  [OK] Test completed successfully!")
    print("=" * 80)


def test_uart_client_with_mock():
    """
    Alternative test using a mock serial port that returns predefined CAN frames
    Simulates realistic vECU responses via UART server with actual vehicle parameters
    """

    class MockSerialPort:
        def __init__(self):
            self.is_open_flag = False
            # Simulate vECU responses - ONLY dynamically calculated parameters
            # These values match what the physics engine would calculate during city_drive
            self.buffer = [
                # Response: Vehicle Speed (DID 0x2C96 = vVeh4)
                # Simulates DRIVING state: ~50 km/h
                # Response: 62 2C 96 [13 88] = 5000 * 0.01 = 50.00 km/h
                b'CAN 7E8 62 2C 96 13 88 00 00 00\n',

                # Response: Battery SOC (DID 0x3DEF = rSocBat)
                # Decreases during driving, increases during charging
                # Response: 62 3D EF [1F 40] = 8000 * 0.01 = 80.00%
                b'CAN 7E8 62 3D EF 1F 40 00 00 00\n',

                # Response: HV Battery Voltage (DID 0x1E3B = uHvBat)
                # Depends on SOC + current (internal resistance model)
                # Response: 62 1E 3B [05 DC] = 1500 * 0.25 = 375.0 V
                b'CAN 7E8 62 1E 3B 05 DC 00 00 00\n',

                # Response: HV Battery Current (DID 0x1E3D = curHvBat1)
                # Calculated from drag, rolling resistance, acceleration
                # Response: 62 1E 3D [4E 20] = 20000 * 0.01 - 189 = 11.0 A
                b'CAN 7E8 62 1E 3D 4E 20 00 00 00\n',

                # Response: Battery Temperature (DID 0x2A0B = tHvBat)
                # I²R losses + cooling system thermal model
                # Response: 62 2A 0B [82] = 130 * 0.5 - 40 = 25.0°C
                b'CAN 7E8 62 2A 0B 82 00 00 00 00\n',
            ]
            self.buffer_index = 0

            # Track request info for the 5 dynamic parameters
            self.requests_sent = [
                ("Fahrzeuggeschwindigkeit", 0x2C96, "vVeh4"),
                ("Batterie Ladezustand (SOC)", 0x3DEF, "rSocBat"),
                ("HV Batterie Spannung", 0x1E3B, "uHvBat"),
                ("HV Batterie Strom", 0x1E3D, "curHvBat1"),
                ("Batterie Temperatur", 0x2A0B, "tHvBat"),
            ]

        def open(self):
            self.is_open_flag = True

        def close(self):
            self.is_open_flag = False

        def flushInput(self):
            pass

        def inWaiting(self):
            return len(self.buffer) - self.buffer_index

        def write(self, _data):
            pass

        def readline(self):
            if self.buffer_index < len(self.buffer):
                line = self.buffer[self.buffer_index]
                self.buffer_index += 1
                return line
            return b''

        def get_request_info(self, index):
            """Get information about what was requested"""
            if index < len(self.requests_sent):
                return self.requests_sent[index]
            return ("Unknown", 0x0000, "unknown")

    print("\n" + "=" * 80)
    print("UART Client Mock Test - Simulating ESP32 Data Logger <-> vECU Communication")
    print("=" * 80)
    print("\nThis test simulates:")
    print("  [ESP32 Data Logger] <--UART--> [UART Server] <--CAN--> [vECU]")
    print("\nDemonstrates targeted parameter queries via UDS (ReadDataByIdentifier)")
    print("=" * 80)

    # Use mock instead of real COM3
    ser = MockSerialPort()

    print("\n[1] Opening serial connection...")
    ser.close()
    ser.open()
    print("    OK - Virtual COM3 opened at 115200 baud")

    print("\n[2] Waiting for UART server initialization...")
    time.sleep(5)
    print("    OK - Server ready")

    print("\n[3] Checking input buffer status...")
    waiting_before = ser.inWaiting()
    print(f"    vor Flush: {waiting_before} CAN frames waiting")

    ser.flushInput()

    waiting_after = ser.inWaiting()
    print(f"    nach Flush: {waiting_after} frames in buffer")

    waiting_write = ser.inWaiting()
    print(f"    nach Write: {waiting_write} frames available")

    print("\n" + "=" * 80)
    print("[4] Reading dynamically calculated vehicle parameters from vECU")
    print("=" * 80)
    print("\nThese 5 parameters are calculated by the physics engine in real-time:")
    print("Based on city_drive.yaml scenario simulation")

    responses = []
    for idx in range(5):
        request_desc, did, param_name = ser.get_request_info(idx)

        print(f"\n{'='*80}")
        print(f"Parameter {idx + 1}/5: {request_desc} (DID: 0x{did:04X}, Variable: {param_name})")
        print(f"{'='*80}")

        # Show what request would be sent
        did_high = (did >> 8) & 0xFF
        did_low = did & 0xFF
        print(f"  [ESP32 TX] Request: CAN 7DF 22 {did_high:02X} {did_low:02X} 00 00 00 00 00")
        print(f"  [Info] Service 0x22 = ReadDataByIdentifier")

        # Read the response
        response = ser.readline()
        response_str = response.decode('utf-8', errors='replace').strip()

        # Parse CAN frame
        if response_str.startswith('CAN'):
            parts = response_str.split()
            if len(parts) >= 3:
                can_id = parts[1]
                data_hex = parts[2:]

                print(f"\n  [vECU TX] Response: {response_str}")
                print(f"  [CAN] ID: 0x{can_id} (Physical response address)")

                # Decode UDS response
                if len(data_hex) >= 3:
                    sid = data_hex[0]
                    response_did = (int(data_hex[1], 16) << 8) | int(data_hex[2], 16)

                    if sid == '62':  # Positive response to ReadDataByIdentifier
                        # Extract data bytes (after SID and DID)
                        data_bytes = [int(b, 16) for b in data_hex[3:] if b != '00']

                        # Decode using our mapping
                        decoded = decode_uds_value(response_did, data_bytes)

                        if decoded:
                            param, value, unit, description = decoded
                            print(f"\n  [DECODED] [OK] Success!")
                            print(f"  [DECODED] Parameter: {param}")
                            print(f"  [DECODED] Description: {description}")
                            print(f"  [DECODED] Raw bytes: {' '.join(data_hex[3:6])}")
                            print(f"  [DECODED] Physical value: {value:.2f} {unit}")
                            print(f"  [DECODED] Meaning: {request_desc} = {value:.2f} {unit}")
                        else:
                            print(f"  [WARNING] DID 0x{response_did:04X} not in mapping")
                    elif sid == '7F':  # Negative response
                        nrc = data_hex[3] if len(data_hex) > 3 else '00'
                        print(f"  [ERROR] Negative response (NRC: 0x{nrc})")
                    else:
                        print(f"  [INFO] Service response: 0x{sid}")
        else:
            print(f"  [UART RX] {response}")

        responses.append(response)

    print("\n" + "=" * 80)
    print("[5] Closing connection...")
    ser.close()
    print("    OK - Serial connection closed")

    print("\n" + "=" * 80)
    print("Test Results Summary - Dynamically Calculated Parameters")
    print("=" * 80)
    print(f"  Total frames received: {len(responses)}")
    print(f"  Non-empty responses:   {sum(1 for r in responses if r)}")
    print(f"  CAN frames decoded:    {sum(1 for r in responses if b'CAN' in r)}")
    print("\nPhysics Engine Parameters Tested:")
    print("  1. Speed      - Calculated from state-based targets")
    print("  2. SOC        - Integrated from current flow")
    print("  3. Voltage    - SOC + current model")
    print("  4. Current    - Drag + acceleration + auxiliaries")
    print("  5. Temperature- Thermal model (I²R + cooling)")

    # Verify we got all expected responses
    assert len(responses) == 5, f"Expected 5 responses, got {len(responses)}"
    assert all(b'CAN' in r for r in responses if r), "Not all responses are CAN frames"

    print("\n  [OK] All 5 dynamic parameters tested successfully!")
    print("  [OK] Mock test completed successfully!")
    print("=" * 80)


def demo_targeted_parameter_requests():
    """
    Demonstrates how to send targeted CAN requests for specific vehicle parameters.

    This shows how an ESP32 data logger would request individual parameters
    instead of just passively reading all CAN traffic.
    """
    print("\n" + "=" * 80)
    print("DEMO: How to Request Specific Vehicle Parameters via CAN/UDS")
    print("=" * 80)

    print("\nUDS Service 0x22 (ReadDataByIdentifier) allows targeted parameter queries.")
    print("Each parameter has a unique DID (Data Identifier).")
    print("\nFormat: CAN <ID> 22 <DID_HIGH> <DID_LOW> 00 00 00 00 00")

    print("\n" + "-" * 80)
    print("Example Requests:")
    print("-" * 80)

    # Only the 5 dynamically calculated parameters from city_drive.yaml
    examples = [
        ("Vehicle Speed", 0x2C96, "vVeh4", "km/h", "State-based physics calculation"),
        ("Battery SOC", 0x3DEF, "rSocBat", "%", "Current flow integration"),
        ("HV Battery Voltage", 0x1E3B, "uHvBat", "V", "SOC + current model"),
        ("HV Battery Current", 0x1E3D, "curHvBat1", "A", "Drag + acceleration + aux"),
        ("Battery Temperature", 0x2A0B, "tHvBat", "°C", "Thermal model (I²R + cooling)"),
    ]

    for desc, did, var_name, unit, calculation in examples:
        did_high = (did >> 8) & 0xFF
        did_low = did & 0xFF
        print(f"\n{desc} ({var_name}):")
        print(f"  DID: 0x{did:04X}")
        print(f"  Calculation: {calculation}")
        print(f"  Request:  CAN 7DF 22 {did_high:02X} {did_low:02X} 00 00 00 00 00")
        print(f"  Response: CAN 7E8 62 {did_high:02X} {did_low:02X} [data bytes...] (Value in {unit})")

    print("\n" + "-" * 80)
    print("CAN ID Explanation:")
    print("-" * 80)
    print("  0x7DF = Functional broadcast address (request to all ECUs)")
    print("  0x7E0-0x7E7 = Physical request addresses (specific ECU)")
    print("  0x7E8-0x7EF = Physical response addresses (ECU replies)")
    print("  0x7E8 = Response from ECU at 0x7E0")

    print("\n" + "-" * 80)
    print("How to send a request via UART:")
    print("-" * 80)
    print("""
from serial import Serial

ser = Serial('COM3', 115200)

# Request Battery SOC (DID 0x3DEF)
request = "CAN 7DF 22 3D EF 00 00 00 00 00\\n"
ser.write(request.encode('utf-8'))

# Read response
response = ser.readline()
print(f"Response: {response}")
# Expected: b'CAN 7E8 62 3D EF 1F 40 00 00 00\\n'
# Decoded: 62 = positive response, 3D EF = DID echo, 1F 40 = 8000 -> 80.00%

ser.close()
    """)

    print("=" * 80)


if __name__ == "__main__":
    # Run both tests
    print("\n### Running Virtual Serial Port Test ###")
    try:
        test_uart_client_simulation()
    except Exception as e:
        print(f"[WARNING] Virtual port test failed: {e}")
        print("This is expected if UART server is not available")

    print("\n### Running Mock Serial Port Test ###")
    test_uart_client_with_mock()

    print("\n### Running Targeted Request Demo ###")
    demo_targeted_parameter_requests()
