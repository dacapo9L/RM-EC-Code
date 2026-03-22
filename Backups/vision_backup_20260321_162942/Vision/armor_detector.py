from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional, Tuple

import cv2
import numpy as np

from config import DetectorConfig


@dataclass
class LightBar:
    center: Tuple[float, float]
    width: float
    height: float
    area: float
    tilt_from_vertical_deg: float
    mean_intensity: float
    box_points: np.ndarray


@dataclass
class Detection:
    has_target: bool
    center: Tuple[int, int]
    bbox_xywh: Tuple[int, int, int, int]
    score: float
    source: str
    lost_count: int
    is_predicted: bool = False


class ArmorDetector:
    def __init__(self, cfg: DetectorConfig):
        self.cfg = cfg
        self._clahe = cv2.createCLAHE(
            clipLimit=cfg.clahe_clip_limit,
            tileGridSize=(cfg.clahe_tile_size, cfg.clahe_tile_size),
        )
        self._last_center: Optional[np.ndarray] = None
        self._last_bbox: Optional[Tuple[int, int, int, int]] = None
        self._last_score: float = 0.0
        self._lost_count: int = 0

    def detect(self, frame: np.ndarray) -> Detection:
        gray = self._to_gray(frame)
        proc = self._preprocess(gray)

        high_mask = self._binary_mask(proc, self.cfg.high_threshold)
        high_detection = self._detect_from_mask(proc, high_mask, "high")
        if high_detection is None:
            low_mask = self._binary_mask(proc, self.cfg.low_threshold)
            det = self._detect_from_mask(proc, low_mask, "low")
        else:
            det = high_detection

        if det is not None:
            return self._update_tracking(det)
        return self._predict_or_drop()

    def _to_gray(self, frame: np.ndarray) -> np.ndarray:
        if frame.ndim == 2:
            return frame
        return cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    def _preprocess(self, gray: np.ndarray) -> np.ndarray:
        enhanced = self._clahe.apply(gray)
        k = self.cfg.blur_kernel
        if k % 2 == 0:
            k += 1
        return cv2.GaussianBlur(enhanced, (k, k), 0)

    def _binary_mask(self, img: np.ndarray, threshold_val: int) -> np.ndarray:
        _, bin_img = cv2.threshold(img, threshold_val, 255, cv2.THRESH_BINARY)
        close_k = np.ones((self.cfg.close_kernel, self.cfg.close_kernel), np.uint8)
        open_k = np.ones((self.cfg.open_kernel, self.cfg.open_kernel), np.uint8)
        closed = cv2.morphologyEx(bin_img, cv2.MORPH_CLOSE, close_k)
        opened = cv2.morphologyEx(closed, cv2.MORPH_OPEN, open_k)
        return opened

    def _extract_light_bars(self, gray: np.ndarray, mask: np.ndarray) -> List[LightBar]:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        bars: List[LightBar] = []

        for c in contours:
            area = float(cv2.contourArea(c))
            if area < self.cfg.min_light_area or area > self.cfg.max_light_area:
                continue
            if len(c) < 5:
                continue

            rect = cv2.minAreaRect(c)
            (cx, cy), (w, h), _ = rect
            long_side = max(w, h)
            short_side = min(w, h)
            if short_side < 1e-6:
                continue
            ratio = long_side / short_side
            if ratio < self.cfg.min_light_ratio:
                continue

            vx, vy, _, _ = cv2.fitLine(c, cv2.DIST_L2, 0, 0.01, 0.01)
            angle_h = float(np.degrees(np.arctan2(float(vy), float(vx))))
            tilt_from_vertical = abs(90.0 - abs(angle_h))
            if tilt_from_vertical > self.cfg.max_tilt_from_vertical_deg:
                continue

            contour_mask = np.zeros_like(gray, dtype=np.uint8)
            cv2.drawContours(contour_mask, [c], -1, 255, thickness=-1)
            mean_intensity = float(cv2.mean(gray, mask=contour_mask)[0])
            box_pts = cv2.boxPoints(rect).astype(np.int32)

            bars.append(
                LightBar(
                    center=(float(cx), float(cy)),
                    width=float(w),
                    height=float(h),
                    area=area,
                    tilt_from_vertical_deg=tilt_from_vertical,
                    mean_intensity=mean_intensity,
                    box_points=box_pts,
                )
            )
        return bars

    def _pair_score(
        self,
        left: LightBar,
        right: LightBar,
        image_center: Tuple[float, float],
        image_diag: float,
    ) -> Optional[Tuple[float, Tuple[int, int, int, int], Tuple[int, int]]]:
        h1 = max(left.width, left.height)
        h2 = max(right.width, right.height)
        if min(h1, h2) < 1e-6:
            return None

        height_ratio = max(h1, h2) / min(h1, h2)
        if height_ratio > self.cfg.max_height_ratio:
            return None

        pair_tilt_diff = abs(left.tilt_from_vertical_deg - right.tilt_from_vertical_deg)
        if pair_tilt_diff > self.cfg.max_pair_tilt_diff_deg:
            return None

        cy_diff = abs(left.center[1] - right.center[1])
        avg_h = 0.5 * (h1 + h2)
        if cy_diff / avg_h > self.cfg.max_center_y_ratio:
            return None

        spacing = abs(left.center[0] - right.center[0])
        spacing_ratio = spacing / avg_h
        if spacing_ratio < self.cfg.min_spacing_ratio or spacing_ratio > self.cfg.max_spacing_ratio:
            return None

        bright_diff_ratio = abs(left.mean_intensity - right.mean_intensity) / max(
            max(left.mean_intensity, right.mean_intensity), 1.0
        )
        if bright_diff_ratio > self.cfg.max_brightness_diff_ratio:
            return None

        all_pts = np.vstack((left.box_points, right.box_points))
        x, y, w, h = cv2.boundingRect(all_pts)
        cx = int(x + w * 0.5)
        cy = int(y + h * 0.5)

        center_dist = np.hypot(cx - image_center[0], cy - image_center[1]) / max(image_diag, 1.0)
        geom_score = (
            (1.0 - min(1.0, (height_ratio - 1.0) / max(self.cfg.max_height_ratio - 1.0, 1e-6)))
            + (1.0 - min(1.0, pair_tilt_diff / max(self.cfg.max_pair_tilt_diff_deg, 1e-6)))
            + (1.0 - min(1.0, (cy_diff / avg_h) / max(self.cfg.max_center_y_ratio, 1e-6)))
        ) / 3.0
        bright_score = 1.0 - min(1.0, bright_diff_ratio / max(self.cfg.max_brightness_diff_ratio, 1e-6))
        center_score = 1.0 - min(1.0, center_dist)

        score = (
            self.cfg.geometry_weight * geom_score
            + self.cfg.brightness_weight * bright_score
            + self.cfg.center_weight * center_score
        )

        return score, (x, y, w, h), (cx, cy)

    def _detect_from_mask(
        self, gray: np.ndarray, mask: np.ndarray, source: str
    ) -> Optional[Detection]:
        bars = self._extract_light_bars(gray, mask)
        if len(bars) < 2:
            return None

        bars.sort(key=lambda b: b.center[0])
        h, w = gray.shape[:2]
        image_center = (w * 0.5, h * 0.5)
        image_diag = float(np.hypot(w, h))

        best = None
        for i in range(len(bars)):
            for j in range(i + 1, len(bars)):
                candidate = self._pair_score(bars[i], bars[j], image_center, image_diag)
                if candidate is None:
                    continue
                if best is None or candidate[0] > best[0]:
                    best = candidate

        if best is None:
            return None

        score, bbox, center = best
        return Detection(
            has_target=True,
            center=center,
            bbox_xywh=bbox,
            score=float(score),
            source=source,
            lost_count=self._lost_count,
            is_predicted=False,
        )

    def _update_tracking(self, det: Detection) -> Detection:
        center = np.array(det.center, dtype=np.float32)
        if self._last_center is None:
            smooth = center
        else:
            a = np.clip(self.cfg.ema_alpha, 0.0, 1.0)
            smooth = a * center + (1.0 - a) * self._last_center

        smooth_center = (int(round(float(smooth[0]))), int(round(float(smooth[1]))))
        self._last_center = smooth
        self._last_bbox = det.bbox_xywh
        self._last_score = det.score
        self._lost_count = 0

        det.center = smooth_center
        det.lost_count = 0
        return det

    def _predict_or_drop(self) -> Detection:
        self._lost_count += 1
        if self._last_center is not None and self._last_bbox is not None:
            if self._lost_count <= self.cfg.max_lost_hold_frames:
                held_score = max(0.0, self._last_score * (1.0 - 0.2 * self._lost_count))
                return Detection(
                    has_target=True,
                    center=(int(round(float(self._last_center[0]))), int(round(float(self._last_center[1])))),
                    bbox_xywh=self._last_bbox,
                    score=held_score,
                    source="hold",
                    lost_count=self._lost_count,
                    is_predicted=True,
                )

        self._last_center = None
        self._last_bbox = None
        self._last_score = 0.0
        return Detection(
            has_target=False,
            center=(-1, -1),
            bbox_xywh=(0, 0, 0, 0),
            score=0.0,
            source="none",
            lost_count=self._lost_count,
            is_predicted=False,
        )


def draw_detection(frame: np.ndarray, det: Detection, fps: float) -> np.ndarray:
    out = frame.copy()
    if out.ndim == 2:
        out = cv2.cvtColor(out, cv2.COLOR_GRAY2BGR)

    h, w = out.shape[:2]
    cv2.line(out, (w // 2 - 8, h // 2), (w // 2 + 8, h // 2), (100, 180, 100), 1)
    cv2.line(out, (w // 2, h // 2 - 8), (w // 2, h // 2 + 8), (100, 180, 100), 1)

    if det.has_target:
        x, y, bw, bh = det.bbox_xywh
        color = (0, 255, 255) if det.is_predicted else (0, 255, 0)
        cv2.rectangle(out, (x, y), (x + bw, y + bh), color, 2)
        cv2.circle(out, det.center, 4, color, -1)

    lines = [
        f"target: {int(det.has_target)}  source: {det.source}",
        f"center: ({det.center[0]}, {det.center[1]})  score: {det.score:.2f}",
        f"lost_count: {det.lost_count}  fps: {fps:.1f}",
    ]
    y0 = 24
    for i, txt in enumerate(lines):
        cv2.putText(
            out,
            txt,
            (10, y0 + i * 26),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (20, 240, 20),
            2,
            cv2.LINE_AA,
        )
    return out
