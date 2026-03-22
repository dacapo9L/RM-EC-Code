from __future__ import annotations

import argparse
import json
import os
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path

import cv2

from armor_detector import ArmorDetector, draw_detection
from config import AppConfig
from hikrobot_camera import HikCamera
from vision_sender import VisionSender


@dataclass
class EvalStats:
    frames_total: int = 0
    target_frames: int = 0
    target_switch_count: int = 0
    lost_event_count: int = 0
    reacquire_event_count: int = 0
    last_has_target: int = 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Hikrobot armor detector (visualization only, no serial output)."
    )
    parser.add_argument("--camera-index", type=int, default=None, help="Camera index.")
    parser.add_argument("--exposure-us", type=float, default=None, help="Exposure time in us.")
    parser.add_argument("--gain-db", type=float, default=None, help="Gain in dB.")
    parser.add_argument("--scale", type=float, default=None, help="Display scale.")
    parser.add_argument(
        "--save-dir",
        type=str,
        default=None,
        help="Directory to save logs/screenshots (relative to Vision).",
    )
    parser.add_argument(
        "--no-gui",
        action="store_true",
        help="Run without cv2.imshow (for SSH/headless environment).",
    )
    parser.add_argument("--flip-h", action="store_true", help="Flip frame horizontally.")
    parser.add_argument("--flip-v", action="store_true", help="Flip frame vertically.")
    parser.add_argument(
        "--rotate-180",
        action="store_true",
        help="Rotate frame by 180 degrees (equivalent to flip-h + flip-v).",
    )
    parser.add_argument(
        "--no-uart",
        action="store_true",
        help="Disable UART sender and run detector only.",
    )
    parser.add_argument(
        "--uart-port",
        type=str,
        default=None,
        help="UART device path, e.g. /dev/ttyTHS1 or /dev/ttyUSB0.",
    )
    parser.add_argument("--uart-baud", type=int, default=None, help="UART baudrate.")
    parser.add_argument("--send-hz", type=float, default=None, help="UART send frequency.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    cfg = AppConfig()
    if args.camera_index is not None:
        cfg.camera.camera_index = args.camera_index
    if args.exposure_us is not None:
        cfg.camera.exposure_time_us = args.exposure_us
    if args.gain_db is not None:
        cfg.camera.gain_db = args.gain_db
    if args.scale is not None:
        cfg.runtime.resize_scale = args.scale
    if args.save_dir is not None:
        cfg.runtime.save_dir = args.save_dir
    if args.flip_h:
        cfg.runtime.flip_horizontal = True
    if args.flip_v:
        cfg.runtime.flip_vertical = True
    if args.rotate_180:
        cfg.runtime.flip_horizontal = True
        cfg.runtime.flip_vertical = True
    if args.no_uart:
        cfg.sender.enable_uart = False
    if args.uart_port is not None:
        cfg.sender.uart_port = args.uart_port
    elif os.environ.get("VISION_UART_PORT"):
        cfg.sender.uart_port = os.environ["VISION_UART_PORT"]
    if args.uart_baud is not None:
        cfg.sender.uart_baud = args.uart_baud
    if args.send_hz is not None:
        cfg.sender.send_hz = args.send_hz

    vision_dir = Path(__file__).resolve().parent
    save_dir = cfg.ensure_save_dir(vision_dir)
    start_tag = datetime.now().strftime("%Y%m%d_%H%M%S")
    config_path = save_dir / f"detector_config_{start_tag}.json"
    with config_path.open("w", encoding="utf-8") as f:
        json.dump(cfg.to_dict(), f, ensure_ascii=False, indent=2)
    print(f"[INFO] Config saved: {config_path}")

    detector = ArmorDetector(cfg.detector)
    frame_count = 0
    t0 = time.perf_counter()
    t_last_print = t0
    fps = 0.0
    eval_stats = EvalStats()
    headless = args.no_gui or not bool(os.environ.get("DISPLAY"))
    if headless:
        print("[INFO] Headless mode enabled (no GUI window).")
    print(
        "[INFO] Frame transform: "
        f"flip_h={int(cfg.runtime.flip_horizontal)} "
        f"flip_v={int(cfg.runtime.flip_vertical)}"
    )
    sender = None
    if cfg.sender.enable_uart:
        sender = VisionSender(
            port=cfg.sender.uart_port,
            baud=cfg.sender.uart_baud,
            send_hz=cfg.sender.send_hz,
        )
        if sender.is_ready():
            print(
                f"[INFO] UART sender enabled: {cfg.sender.uart_port} "
                f"{cfg.sender.uart_baud}bps @{cfg.sender.send_hz:.1f}Hz"
            )
        else:
            print(f"[WARN] UART sender disabled due to error: {sender.last_error()}")
            sender = None
    else:
        print("[INFO] UART sender disabled by flag/config.")

    try:
        with HikCamera(
            index=cfg.camera.camera_index,
            exposure_time_us=cfg.camera.exposure_time_us,
            gain_db=cfg.camera.gain_db,
        ) as cam:
            print(f"[INFO] Camera opened: {cam.width}x{cam.height}")
            print("[INFO] Controls: q=quit, p=snapshot, Ctrl+C=quit")

            while True:
                frame = cam.read(timeout_ms=cfg.camera.timeout_ms)
                if frame is None:
                    continue
                if cfg.runtime.flip_horizontal:
                    frame = cv2.flip(frame, 1)
                if cfg.runtime.flip_vertical:
                    frame = cv2.flip(frame, 0)

                det = detector.detect(frame)
                frame_count += 1
                eval_stats.frames_total += 1
                if det.has_target:
                    eval_stats.target_frames += 1
                current_target = int(det.has_target)
                if current_target != eval_stats.last_has_target:
                    eval_stats.target_switch_count += 1
                    if current_target == 0:
                        eval_stats.lost_event_count += 1
                    else:
                        eval_stats.reacquire_event_count += 1
                    eval_stats.last_has_target = current_target

                now = time.perf_counter()
                elapsed = now - t0
                if elapsed > 0:
                    fps = frame_count / elapsed

                if sender is not None and sender.should_send(now):
                    if det.has_target:
                        tx_x, tx_y = det.center
                    else:
                        tx_x = frame.shape[1] // 2
                        tx_y = frame.shape[0] // 2
                    sender.send(
                        now_ts=now,
                        has_target=det.has_target,
                        x=tx_x,
                        y=tx_y,
                        digit_when_target=cfg.sender.default_digit_when_target,
                    )

                vis = draw_detection(frame, det, fps)
                if cfg.runtime.resize_scale and cfg.runtime.resize_scale != 1.0:
                    vis = cv2.resize(
                        vis,
                        None,
                        fx=cfg.runtime.resize_scale,
                        fy=cfg.runtime.resize_scale,
                        interpolation=cv2.INTER_LINEAR,
                    )
                if not headless:
                    cv2.imshow(cfg.runtime.show_window_name, vis)

                if now - t_last_print >= cfg.runtime.print_interval_s:
                    t_last_print = now
                    target_ratio = (
                        eval_stats.target_frames / max(eval_stats.frames_total, 1) * 100.0
                    )
                    tx_info = ""
                    if sender is not None:
                        tx_info = (
                            f" tx_ok={sender.stats.tx_ok} tx_drop={sender.stats.tx_drop}"
                            f" tx_reopen={sender.stats.tx_reopen}"
                        )
                    print(
                        f"[STAT] fps={fps:.1f} target={int(det.has_target)} "
                        f"center={det.center} score={det.score:.2f} source={det.source} "
                        f"lost={det.lost_count} target_ratio={target_ratio:.1f}%{tx_info}"
                    )

                if not headless:
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord("q"):
                        break
                    if key == ord("p"):
                        ts = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
                        img_path = save_dir / f"snapshot_{ts}.png"
                        meta_path = save_dir / f"snapshot_{ts}.json"
                        cv2.imwrite(str(img_path), vis)
                        with meta_path.open("w", encoding="utf-8") as f:
                            json.dump(
                                {
                                    "center": det.center,
                                    "bbox_xywh": det.bbox_xywh,
                                    "score": det.score,
                                    "source": det.source,
                                    "lost_count": det.lost_count,
                                    "has_target": det.has_target,
                                    "is_predicted": det.is_predicted,
                                    "fps": fps,
                                },
                                f,
                                ensure_ascii=False,
                                indent=2,
                            )
                        print(f"[INFO] Snapshot saved: {img_path.name}")
    except KeyboardInterrupt:
        print("\n[INFO] Interrupted by user.")
    finally:
        if sender is not None:
            sender.close()
        if cfg.evaluation.save_metrics:
            end_ts = time.perf_counter()
            duration_s = max(0.0, end_ts - t0)
            target_ratio = eval_stats.target_frames / max(eval_stats.frames_total, 1)
            metrics = {
                "duration_s": duration_s,
                "expected_eval_minutes": cfg.evaluation.expected_eval_minutes,
                "frames_total": eval_stats.frames_total,
                "target_frames": eval_stats.target_frames,
                "target_ratio": target_ratio,
                "target_switch_count": eval_stats.target_switch_count,
                "lost_event_count": eval_stats.lost_event_count,
                "reacquire_event_count": eval_stats.reacquire_event_count,
                "avg_fps": (frame_count / duration_s) if duration_s > 1e-6 else 0.0,
                "uart": asdict(sender.stats) if sender is not None else None,
            }
            metrics_path = save_dir / f"eval_metrics_{start_tag}.json"
            with metrics_path.open("w", encoding="utf-8") as f:
                json.dump(metrics, f, ensure_ascii=False, indent=2)
            print(f"[INFO] Eval metrics saved: {metrics_path}")


if __name__ == "__main__":
    main()
