from __future__ import annotations

import sys
from ctypes import POINTER, byref, c_ubyte, cast, memset, sizeof
from pathlib import Path
from typing import Optional

import cv2
import numpy as np


THIS_DIR = Path(__file__).resolve().parent
MVIMPORT_DIR = THIS_DIR / "MvImport"
if str(MVIMPORT_DIR) not in sys.path:
    sys.path.insert(0, str(MVIMPORT_DIR))

from MvCameraControl_class import (  # noqa: E402
    MV_E_ACCESS_DENIED,
    MV_ACCESS_Exclusive,
    MV_ACCESS_Control,
    MV_ACCESS_Monitor,
    MV_CC_DEVICE_INFO,
    MV_CC_DEVICE_INFO_LIST,
    MV_CC_PIXEL_CONVERT_PARAM,
    MV_GIGE_DEVICE,
    MV_TRIGGER_MODE_OFF,
    MV_USB_DEVICE,
    MV_FRAME_OUT_INFO_EX,
    MVCC_FLOATVALUE,
    MVCC_INTVALUE,
    MvCamera,
    PixelType_Gvsp_BGR8_Packed,
    PixelType_Gvsp_Mono8,
    PixelType_Gvsp_RGB8_Packed,
)


class HikCamera:
    """Simple Hikrobot MVS wrapper for frame grabbing on Windows/Jetson."""

    def __init__(
        self,
        index: int = 0,
        exposure_time_us: Optional[float] = None,
        gain_db: Optional[float] = None,
    ) -> None:
        self.cam = MvCamera()
        self.width = 0
        self.height = 0
        self.data_buf_size = 0
        self.data_buf = None
        self.st_frame_info = MV_FRAME_OUT_INFO_EX()
        self._opened = False
        self._grabbing = False

        self._open_device(index=index)
        self._set_trigger_off()
        self._set_fixed_exposure_gain(exposure_time_us=exposure_time_us, gain_db=gain_db)
        self._query_resolution()
        self._start_grabbing()
        self._allocate_buffer()

    def _open_device(self, index: int) -> None:
        device_list = MV_CC_DEVICE_INFO_LIST()
        device_type = MV_USB_DEVICE | MV_GIGE_DEVICE
        ret = MvCamera.MV_CC_EnumDevices(device_type, device_list)
        if ret != 0:
            raise RuntimeError(f"MV_CC_EnumDevices failed, ret=0x{ret:x}")
        if device_list.nDeviceNum == 0:
            raise RuntimeError("No Hikrobot camera found.")
        if not 0 <= index < device_list.nDeviceNum:
            raise ValueError(
                f"Camera index out of range: {index}, available={device_list.nDeviceNum}"
            )

        st_device = cast(
            device_list.pDeviceInfo[index], POINTER(MV_CC_DEVICE_INFO)
        ).contents

        ret = self.cam.MV_CC_CreateHandle(st_device)
        if ret != 0:
            raise RuntimeError(f"MV_CC_CreateHandle failed, ret=0x{ret:x}")

        open_modes = [
            ("Exclusive", MV_ACCESS_Exclusive),
            ("Control", MV_ACCESS_Control),
            ("Monitor", MV_ACCESS_Monitor),
        ]
        ret = -1
        selected_mode = None
        for mode_name, mode in open_modes:
            ret = self.cam.MV_CC_OpenDevice(mode, 0)
            if ret == 0:
                selected_mode = mode_name
                break

        if ret != 0:
            self.cam.MV_CC_DestroyHandle()
            if ret == MV_E_ACCESS_DENIED:
                raise RuntimeError(
                    "MV_CC_OpenDevice failed: ACCESS_DENIED (0x80000203). "
                    "Camera is occupied or permission is insufficient."
                )
            raise RuntimeError(f"MV_CC_OpenDevice failed, ret=0x{ret:x}")
        print(f"[INFO] Opened camera with access mode: {selected_mode}")
        self._opened = True

    def _set_trigger_off(self) -> None:
        ret = self.cam.MV_CC_SetEnumValue("TriggerMode", MV_TRIGGER_MODE_OFF)
        if ret != 0:
            print(f"[WARN] Failed to set TriggerMode OFF, ret=0x{ret:x}")

    def _set_enum_off(self, key: str) -> None:
        # For most MVS enum nodes, "Off" maps to 0.
        ret = self.cam.MV_CC_SetEnumValue(key, 0)
        if ret != 0:
            print(f"[WARN] SetEnumValue({key}=Off) failed, ret=0x{ret:x}")

    def _set_float_if_supported(self, key: str, val: Optional[float]) -> None:
        if val is None:
            return
        cap = MVCC_FLOATVALUE()
        ret = self.cam.MV_CC_GetFloatValue(key, cap)
        if ret != 0:
            print(f"[WARN] GetFloatValue({key}) failed, ret=0x{ret:x}")
            return
        clipped = max(cap.fMin, min(cap.fMax, float(val)))
        ret = self.cam.MV_CC_SetFloatValue(key, clipped)
        if ret != 0:
            print(f"[WARN] SetFloatValue({key}) failed, ret=0x{ret:x}")

    def _set_fixed_exposure_gain(
        self, exposure_time_us: Optional[float], gain_db: Optional[float]
    ) -> None:
        # Disable auto exposure/gain for stable detection in competition.
        self._set_enum_off("ExposureAuto")
        self._set_enum_off("GainAuto")
        self._set_float_if_supported("ExposureTime", exposure_time_us)
        self._set_float_if_supported("Gain", gain_db)

    def _query_resolution(self) -> None:
        st_param = MVCC_INTVALUE()
        ret = self.cam.MV_CC_GetIntValue("Width", st_param)
        if ret != 0:
            raise RuntimeError(f"Failed to get Width, ret=0x{ret:x}")
        self.width = int(st_param.nCurValue)

        ret = self.cam.MV_CC_GetIntValue("Height", st_param)
        if ret != 0:
            raise RuntimeError(f"Failed to get Height, ret=0x{ret:x}")
        self.height = int(st_param.nCurValue)

    def _start_grabbing(self) -> None:
        ret = self.cam.MV_CC_StartGrabbing()
        if ret != 0:
            raise RuntimeError(f"MV_CC_StartGrabbing failed, ret=0x{ret:x}")
        self._grabbing = True

    def _allocate_buffer(self) -> None:
        # 4x over-allocation to safely cover packed formats.
        self.data_buf_size = self.width * self.height * 4
        self.data_buf = (c_ubyte * self.data_buf_size)()

    def read(self, timeout_ms: int = 1000) -> Optional[np.ndarray]:
        if self.data_buf is None:
            return None

        ret = self.cam.MV_CC_GetOneFrameTimeout(
            self.data_buf, self.data_buf_size, self.st_frame_info, timeout_ms
        )
        if ret != 0:
            return None

        frame_len = int(self.st_frame_info.nFrameLen)
        h = int(self.st_frame_info.nHeight)
        w = int(self.st_frame_info.nWidth)
        pixel_type = int(self.st_frame_info.enPixelType)
        raw = np.frombuffer(self.data_buf, count=frame_len, dtype=np.uint8)

        if pixel_type == PixelType_Gvsp_Mono8:
            return raw.reshape((h, w))
        if pixel_type == PixelType_Gvsp_BGR8_Packed:
            return raw.reshape((h, w, 3))
        if pixel_type == PixelType_Gvsp_RGB8_Packed:
            img = raw.reshape((h, w, 3))
            return cv2.cvtColor(img, cv2.COLOR_RGB2BGR)

        convert_size = w * h * 3
        convert_buf = (c_ubyte * convert_size)()
        st_convert = MV_CC_PIXEL_CONVERT_PARAM()
        memset(byref(st_convert), 0, sizeof(st_convert))
        st_convert.nWidth = w
        st_convert.nHeight = h
        st_convert.pSrcData = self.data_buf
        st_convert.nSrcDataLen = frame_len
        st_convert.enSrcPixelType = pixel_type
        st_convert.enDstPixelType = PixelType_Gvsp_BGR8_Packed
        st_convert.pDstBuffer = convert_buf
        st_convert.nDstBufferSize = convert_size

        ret = self.cam.MV_CC_ConvertPixelType(st_convert)
        if ret != 0:
            return None

        out = np.frombuffer(convert_buf, count=convert_size, dtype=np.uint8)
        return out.reshape((h, w, 3))

    def close(self) -> None:
        if self._grabbing:
            self.cam.MV_CC_StopGrabbing()
            self._grabbing = False
        if self._opened:
            self.cam.MV_CC_CloseDevice()
            self.cam.MV_CC_DestroyHandle()
            self._opened = False
        cv2.destroyAllWindows()

    def __enter__(self) -> "HikCamera":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()
