"""
ISO-TP (ISO 15765-2) codec.

Assembles multi-frame UDS requests from incoming CAN frames and fragments
outgoing UDS responses back into CAN frames.  Transport-agnostic: works with
any Transport implementation (NativeCanTransport, UartTransport, StdinTransport).
"""

import time
from dataclasses import dataclass
from typing import Optional

from src.communication.transport import CanFrame, Transport


@dataclass
class UdsRequest:
    can_id: int
    payload: bytes


class IsoTpCodec:
    """
    ISO-TP framing layer.

    process_frame() — feed one incoming CAN frame; returns a UdsRequest (stripped
    of ISO-TP PCI bytes) when a complete UDS payload has been assembled.  For
    multi-frame requests this method blocks internally while collecting consecutive
    frames.

    send_response() — fragment a UDS response payload and write the resulting CAN
    frames to the transport.  Waits for a Flow Control frame when the response
    requires multiple frames.
    """

    def __init__(self, transport: Transport, config) -> None:
        self._transport = transport
        self._config = config

    # ── Incoming ─────────────────────────────────────────────────────────────

    def process_frame(self, frame: CanFrame) -> Optional[UdsRequest]:
        """
        Process one incoming CAN frame.
        Returns a UdsRequest with a stripped UDS payload, or None if not complete.
        """
        if not frame.data:
            return None

        pci_nibble = frame.data[0] >> 4

        if pci_nibble == 0x0:
            return self._handle_single_frame(frame)
        if pci_nibble == 0x1:
            return self._handle_first_frame(frame)
        # Consecutive frames (0x2) and Flow Control (0x3) are handled inline
        return None

    def _handle_single_frame(self, frame: CanFrame) -> Optional[UdsRequest]:
        sf_len = frame.data[0] & 0x0F
        if sf_len == 0 or sf_len > len(frame.data) - 1:
            print(f"[ISO-TP] Invalid SF length: {sf_len}")
            return None
        payload = frame.data[1:1 + sf_len]
        print(f"[ISO-TP SF] RX {frame.can_id:03X}: {payload.hex(' ').upper()} ({sf_len} bytes)")
        return UdsRequest(frame.can_id, payload)

    def _handle_first_frame(self, frame: CanFrame) -> Optional[UdsRequest]:
        total_len = ((frame.data[0] & 0x0F) << 8) | frame.data[1]
        payload = bytearray(frame.data[2:])
        print(f"[ISO-TP FF] RX {frame.can_id:03X}: total_len={total_len}, got {len(payload)} bytes so far")

        # Send Flow Control: ContinueToSend, block_size=0, ST=0
        fc_id = self._response_id(frame.can_id)
        self._transport.write_frame(
            CanFrame(fc_id, bytes([0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]))
        )
        print(f"[ISO-TP FC] Sent ContinueToSend to {fc_id:03X}")

        # Collect consecutive frames until full payload received or deadline
        deadline = time.time() + 5.0
        while len(payload) < total_len and time.time() < deadline:
            cf = self._transport.read_frame_blocking(timeout_s=1.0)
            if cf is None:
                continue
            if cf.data and (cf.data[0] & 0xF0) == 0x20:
                sn = cf.data[0] & 0x0F
                needed = total_len - len(payload)
                payload.extend(cf.data[1 : 1 + needed])
                print(f"[ISO-TP CF#{sn}] +{needed} bytes ({len(payload)}/{total_len})")

        if len(payload) < total_len:
            print(f"[ISO-TP MF] Timeout: received {len(payload)}/{total_len} bytes — discarding incomplete frame")
            return None
        assembled = bytes(payload[:total_len])
        print(f"[ISO-TP MF] Assembled {len(assembled)}/{total_len} bytes: {assembled.hex(' ').upper()}")
        return UdsRequest(frame.can_id, assembled)

    # ── Outgoing ─────────────────────────────────────────────────────────────

    def send_response(self, request_can_id: int, payload: bytes) -> None:
        """Fragment a UDS response payload and write it to the transport."""
        response_id = self._response_id(request_can_id)

        if len(payload) <= 7:
            self._send_single_frame(response_id, payload)
        else:
            self._send_multi_frame(response_id, payload)

    def _send_single_frame(self, response_id: int, payload: bytes) -> None:
        sf_data = bytes([len(payload)]) + payload
        sf_data = sf_data + bytes(8 - len(sf_data))
        print(f"[ISO-TP SF] TX {response_id:03X}: {sf_data.hex(' ').upper()}")
        self._transport.write_frame(CanFrame(response_id, sf_data))

    def _send_multi_frame(self, response_id: int, payload: bytes) -> None:
        total_len = len(payload)
        print(f"[ISO-TP MF] TX {response_id:03X}: {total_len} bytes")

        # First Frame: 2 PCI bytes + first 6 payload bytes
        ff_data = bytes([0x10 | ((total_len >> 8) & 0x0F), total_len & 0xFF]) + payload[:6]
        self._transport.write_frame(CanFrame(response_id, ff_data))
        print(f"[ISO-TP FF] TX First Frame: {ff_data.hex(' ').upper()}")

        # Wait for Flow Control
        print("[ISO-TP] Waiting for Flow Control...")
        fc = self._transport.read_frame_blocking(timeout_s=2.0)
        if fc and fc.data and (fc.data[0] & 0xF0) == 0x30:
            block_size = fc.data[1] if len(fc.data) > 1 else 0
            print(f"[ISO-TP FC] Received ContinueToSend, block_size={block_size}")
        else:
            print("[ISO-TP FC] WARNING: No valid FC received — sending CFs anyway")

        # Consecutive Frames: 1 PCI byte + up to 7 payload bytes, padded to 8
        remaining = payload[6:]
        sn = 1
        while remaining:
            cf_payload = remaining[:7]
            remaining = remaining[7:]
            cf_data = bytes([0x20 | (sn & 0x0F)]) + cf_payload
            cf_data = cf_data + bytes(8 - len(cf_data))
            self._transport.write_frame(CanFrame(response_id, cf_data))
            print(f"[ISO-TP CF#{sn}] TX: {cf_data.hex(' ').upper()}")
            sn = (sn + 1) & 0x0F

        print("[ISO-TP MF] Transmission complete")

    # ── Helpers ──────────────────────────────────────────────────────────────

    def _response_id(self, request_id: int) -> int:
        gw_id = self._config.get_gateway_id(request_id)
        if gw_id is not None:
            return gw_id
        return _calculate_response_id(request_id)


def _calculate_response_id(request_id: int) -> int:
    """
    Derive the response CAN ID from the request ID using standard UDS conventions.
    Config-based gateway IDs take precedence over this formula (see IsoTpCodec._response_id).
    """
    if request_id == 0x7DF:
        return 0x7E8
    if 0x7E0 <= request_id <= 0x7E7:
        return request_id + 0x08
    if request_id > 0x7FF:
        # 29-bit extended: swap source and target bytes
        target = (request_id >> 8) & 0xFF
        source = request_id & 0xFF
        if target == 0x00:
            # Functional (broadcast): ECU at address 0x40 responds
            return (request_id & 0xFFFF0000) | (0x40 << 8) | source
        return (request_id & 0xFFFF0000) | (source << 8) | target
    return request_id + 0x08
