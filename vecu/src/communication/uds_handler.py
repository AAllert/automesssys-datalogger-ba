"""
Unified UDS request handler.

Transport- and framing-agnostic: receives a stripped UDS payload (ISO-TP PCI
already removed) and returns the response bytes.
"""

from src.simulation.vehicle_simulator import VehicleSimulator


class UdsHandler:
    """
    Processes raw UDS requests and returns UDS response bytes.
    Replaces the per-transport service dispatch in UdsServer and UartCanServer.
    """

    def __init__(self, simulator: VehicleSimulator) -> None:
        self._simulator = simulator

    def process(self, can_id: int, data: bytes) -> bytes:
        """
        Dispatch a UDS request to the appropriate service handler.

        Args:
            can_id:  Source CAN ID (passed through for logging only).
            data:    Raw UDS payload with ISO-TP PCI already stripped.

        Returns:
            UDS response bytes (positive or negative response).
        """
        if not data:
            print("[UDS] Empty request")
            return bytes([0x7F, 0x00, 0x13])

        sid = data[0]
        print(f"[UDS] SID=0x{sid:02X} from {can_id:03X}")

        if sid == 0x22:
            return self._rdbi(sid, data)
        if sid == 0x10:
            return self._session_control(sid, data)
        if sid == 0x2E:
            return self._wdbi(sid, data)
        if sid == 0x3E:
            return bytes([0x7E, 0x00])

        print(f"[UDS] serviceNotSupported: 0x{sid:02X}")
        return bytes([0x7F, sid, 0x11])

    # ── Service handlers ──────────────────────────────────────────────────────

    def _rdbi(self, sid: int, data: bytes) -> bytes:
        if len(data) < 3:
            print(f"[UDS] RDBI malformed ({len(data)} bytes)")
            return bytes([0x7F, sid, 0x13])
        did = (data[1] << 8) | data[2]
        response = self._simulator.get_value_for_did(did)
        if response and len(response) >= 3 and response[0] == 0x7F:
            print(f"[UDS] NRC=0x{response[2]:02X} for DID 0x{did:04X}")
        return response

    def _session_control(self, sid: int, data: bytes) -> bytes:
        if len(data) < 2:
            return bytes([0x7F, sid, 0x13])
        session_type = data[1]
        print(f"[UDS] DiagnosticSessionControl: 0x{session_type:02X}")
        return bytes([0x50, session_type])

    def _wdbi(self, sid: int, data: bytes) -> bytes:
        if len(data) < 3:
            return bytes([0x7F, sid, 0x13])
        did = (data[1] << 8) | data[2]
        print(f"[UDS] WriteDataByIdentifier: DID=0x{did:04X}")
        return bytes([0x6E, data[1], data[2]])
