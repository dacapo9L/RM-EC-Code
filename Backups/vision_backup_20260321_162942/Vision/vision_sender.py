from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from typing import Optional


VISION_UART_HEADER = ord("s")
VISION_UART_TAIL = ord("e")
VISION_UART_PAYLOAD_LEN = 8
VISION_UART_FRAME_LEN = 12
VISION_FLAG_HAS_TARGET = 1 << 0
VISION_FLAG_HAS_DISTANCE = 1 << 1


def _crc8(data: bytes) -> int:
    crc = 0x00
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x31) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


@dataclass
class SenderStats:
    tx_ok: int = 0
    tx_drop: int = 0
    tx_reopen: int = 0


class VisionSender:
    """Send packets compatible with UserFiles/Subsystem/5_communication/vision_uart.c."""

    def __init__(self, port: str, baud: int, send_hz: float):
        self.port = port
        self.baud = int(baud)
        self.send_hz = float(send_hz)
        self.period_s = 1.0 / self.send_hz if self.send_hz > 1e-6 else 0.02
        self.next_send_ts = time.perf_counter()
        self.stats = SenderStats()
        self._serial = None
        self._serial_err: Optional[str] = None
        self._open()

    def _open(self) -> None:
        try:
            import serial  # type: ignore
        except Exception as exc:  # pragma: no cover
            self._serial_err = (
                f"pyserial not available: {exc}. Install with `pip install pyserial`."
            )
            self._serial = None
            return

        try:
            self._serial = serial.Serial(
                self.port,
                self.baud,
                timeout=0,
                write_timeout=0,
            )
            self._serial_err = None
        except Exception as exc:
            self._serial = None
            self._serial_err = f"open failed: {exc}"

    def is_ready(self) -> bool:
        return self._serial is not None

    def last_error(self) -> Optional[str]:
        return self._serial_err

    def should_send(self, now_ts: float) -> bool:
        return now_ts >= self.next_send_ts

    def _build_frame(
        self,
        has_target: bool,
        x: int,
        y: int,
        digit: int,
        has_distance: bool = False,
        distance_mm: int = 0,
    ) -> bytes:
        x = max(0, min(65535, int(x)))
        y = max(0, min(65535, int(y)))
        distance_mm = max(0, min(65535, int(distance_mm)))
        flags = 0
        if has_target:
            flags |= VISION_FLAG_HAS_TARGET
        if has_distance:
            flags |= VISION_FLAG_HAS_DISTANCE
        payload = struct.pack(
            "<BBHHH",
            int(digit) & 0xFF,
            flags & 0xFF,
            x,
            y,
            distance_mm,
        )
        assert len(payload) == VISION_UART_PAYLOAD_LEN

        frame = bytearray(VISION_UART_FRAME_LEN)
        frame[0] = VISION_UART_HEADER
        frame[1] = VISION_UART_PAYLOAD_LEN
        frame[2:10] = payload
        frame[10] = _crc8(payload)
        frame[11] = VISION_UART_TAIL
        return bytes(frame)

    def send(
        self,
        now_ts: float,
        has_target: bool,
        x: int,
        y: int,
        digit_when_target: int,
    ) -> bool:
        if self._serial is None:
            self.stats.tx_drop += 1
            return False

        digit = digit_when_target if has_target else 0xFF
        frame = self._build_frame(has_target=has_target, x=x, y=y, digit=digit)
        ok = False
        try:
            self._serial.write(frame)
            ok = True
            self.stats.tx_ok += 1
        except Exception as exc:
            self.stats.tx_drop += 1
            self._serial_err = f"write failed: {exc}"
            try:
                self._serial.close()
            except Exception:
                pass
            self._serial = None
            self._open()
            self.stats.tx_reopen += 1

        while self.next_send_ts <= now_ts:
            self.next_send_ts += self.period_s
        return ok

    def close(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                pass
            self._serial = None
