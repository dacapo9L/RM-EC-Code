from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Dict, Any


@dataclass
class CameraConfig:
    camera_index: int = 0
    exposure_time_us: float = 7000.0
    gain_db: float = 8.0
    timeout_ms: int = 100


@dataclass
class DetectorConfig:
    clahe_clip_limit: float = 2.0
    clahe_tile_size: int = 8
    blur_kernel: int = 3

    high_threshold: int = 210
    low_threshold: int = 170
    close_kernel: int = 3
    open_kernel: int = 3

    min_light_area: float = 30.0
    max_light_area: float = 10000.0
    min_light_ratio: float = 2.0
    max_tilt_from_vertical_deg: float = 30.0

    max_pair_tilt_diff_deg: float = 18.0
    max_height_ratio: float = 1.6
    max_center_y_ratio: float = 0.55
    min_spacing_ratio: float = 0.5
    max_spacing_ratio: float = 5.0
    min_armor_aspect_ratio: float = 1.0
    max_armor_aspect_ratio: float = 6.0
    max_brightness_diff_ratio: float = 0.55
    max_inner_to_light_ratio: float = 0.86
    min_light_fill_ratio: float = 0.42

    center_weight: float = 0.8
    geometry_weight: float = 1.0
    brightness_weight: float = 0.5
    track_weight: float = 0.7
    track_max_dist_ratio: float = 0.35

    ema_alpha: float = 0.42
    max_lost_hold_frames: int = 4


@dataclass
class RuntimeConfig:
    show_window_name: str = "Armor Detector"
    resize_scale: float = 1.0
    print_interval_s: float = 1.0
    save_dir: str = "runs"
    flip_horizontal: bool = True
    flip_vertical: bool = True


@dataclass
class SenderConfig:
    enable_uart: bool = True
    uart_port: str = "/dev/ttyUSB0"
    uart_baud: int = 115200
    send_hz: float = 50.0
    default_digit_when_target: int = 1


@dataclass
class EvalConfig:
    save_metrics: bool = True
    expected_eval_minutes: float = 10.0


@dataclass
class AppConfig:
    camera: CameraConfig = field(default_factory=CameraConfig)
    detector: DetectorConfig = field(default_factory=DetectorConfig)
    runtime: RuntimeConfig = field(default_factory=RuntimeConfig)
    sender: SenderConfig = field(default_factory=SenderConfig)
    evaluation: EvalConfig = field(default_factory=EvalConfig)

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)

    def ensure_save_dir(self, base_dir: Path) -> Path:
        path = base_dir / self.runtime.save_dir
        path.mkdir(parents=True, exist_ok=True)
        return path
