import struct
import time

from src.simulation.signal_catalog import SIGNAL_CATALOG
from src.simulation.telemetry import TelemetrySnapshot

# DID 0x02B2: Ignition status — queried first by the datalogger; wrong response causes immediate exit.
DID_IGNITION = 0x02B2

# DID 701: Environment data — complex 12-byte response with odometer + timestamp.
DID_ENVIRONMENT = 701

_RESPONSE_SID = 0x62


class UdsEncoder:
    """Encodes physical signal values from a TelemetrySnapshot into UDS response bytes."""

    def __init__(self, config):
        self.config = config

    def encode(self, did: int, snapshot: TelemetrySnapshot) -> bytes:
        print(f"[ENCODER] encode: DID=0x{did:04X} ({did})")
        did_bytes = did.to_bytes(2, 'big')

        if did == DID_IGNITION:
            return self._encode_ignition(did_bytes, snapshot)

        if did == DID_ENVIRONMENT:
            return self._encode_environment(did_bytes, snapshot)

        return self._encode_standard(did, did_bytes, snapshot)

    def _encode_ignition(self, did_bytes: bytes, snapshot: TelemetrySnapshot) -> bytes:
        # 0xAA signals "ignition on" to the datalogger
        payload = bytes([0xAA]) if snapshot.ignition_on > 0 else bytes([0x00])
        result = bytes([_RESPONSE_SID]) + did_bytes + payload
        print(f"[ENCODER] Ignition DID 0x{DID_IGNITION:04X}: ignition_on={snapshot.ignition_on} -> {result.hex(' ').upper()}")
        return result

    def _encode_environment(self, did_bytes: bytes, snapshot: TelemetrySnapshot) -> bytes:
        """
        DID 701: 12-byte response with odometer and timestamp.

        Bit layout:
        - Bits 8-31  (24 bit): odometer in km
        - Bits 40-71 (32 bit): timestamp — sec(6)|min(6)|hour(5)|day(5)|month(4)|year(6)
        """
        print(f"[ENCODER] Environment DID {DID_ENVIRONMENT}: encoding timestamp + odometer")
        now = time.localtime()
        mileage_km = max(0, min(int(snapshot.odometer_total_km), (1 << 24) - 1))

        time_value = (
            (now.tm_sec & 0x3F) |
            ((now.tm_min & 0x3F) << 6) |
            ((now.tm_hour & 0x1F) << 12) |
            ((now.tm_mday & 0x1F) << 17) |
            ((now.tm_mon & 0x0F) << 22) |
            (((now.tm_year - 2000) & 0x3F) << 26)
        )

        payload_canvas = (mileage_km << 8) | (time_value << 40)
        payload = payload_canvas.to_bytes(12, 'little')
        return bytes([_RESPONSE_SID]) + did_bytes + payload

    def _encode_standard(self, did: int, did_bytes: bytes, snapshot: TelemetrySnapshot) -> bytes:
        config_rows = [row for row in self.config.get_rows() if row.did == did]

        if not config_rows:
            print(f"[ENCODER] ERROR: DID=0x{did:04X} ({did}) not in config -> NRC 0x31 (requestOutOfRange)")
            return bytes([0x7F, 0x22, 0x31])

        config_row = config_rows[0]
        data_size = config_row.data_size_in_bytes
        print(f"[ENCODER] Found {len(config_rows)} config row(s) for DID=0x{did:04X}: label='{config_row.label}' data_size={data_size}B")

        payload_canvas = 0

        for row in config_rows:
            catalog_entry = SIGNAL_CATALOG.get(row.label)
            physical_value = catalog_entry.extractor(snapshot) if catalog_entry else 0.0
            print(f"[ENCODER]   Signal '{row.label}': physical={physical_value} factor={row.factor} offset={row.offset} bits={row.length_in_bit} start_bit={row.start_bit} type={row.data_type}")

            raw_value = int((physical_value - row.offset) / row.factor)

            if row.data_type == "uint":
                max_val = (1 << row.length_in_bit) - 1
                raw_value = max(0, min(raw_value, max_val))
            elif row.data_type == "int":
                min_val = -(1 << (row.length_in_bit - 1))
                max_val = (1 << (row.length_in_bit - 1)) - 1
                raw_value = max(min_val, min(raw_value, max_val))
                if raw_value < 0:
                    raw_value += 1 << row.length_in_bit
            elif row.data_type == "float32":
                packed = struct.pack('!f', physical_value)
                raw_value = int.from_bytes(packed, byteorder='big')

            print(f"[ENCODER]   -> raw_value={raw_value} (0x{raw_value:X})")
            payload_canvas |= (raw_value << row.start_bit)

        payload = payload_canvas.to_bytes(data_size, 'little')
        result = bytes([_RESPONSE_SID]) + did_bytes + payload
        print(f"[ENCODER] Encoded response: {result.hex(' ').upper()} ({len(result)} bytes)")
        return result
