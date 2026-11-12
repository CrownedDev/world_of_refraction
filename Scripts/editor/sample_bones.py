"""Read-only bone sampler for anim sequences (the read part of B-373).

For each anim sequence, writes <out_dir>/<AnimName>.json with every frame's component-space ("world")
position, rotation and speed of the chosen bones, plus the clip's existing notifies and a per-bone summary.
Loads assets only: nothing is modified or saved.

Headless (editor can stay open):  Scripts/ue/sample_bones.sh <out_dir> <anim_path> [<anim_path> ...] [--bones a,b,c]
Inside the editor (Output Log, Cmd):  py "<repo>/Scripts/editor/sample_bones.py" <out_dir> <anim_path> ...
"""

import json
import math
import os
import sys

import unreal

DEFAULT_BONES = ["root", "pelvis", "hand_r", "hand_l", "weapon_r", "foot_l", "foot_r"]
LOG_PREFIX = "[BoneSampler]"
ROUND_DIGITS = 3


def _vec(v):
    return [round(v.x, ROUND_DIGITS), round(v.y, ROUND_DIGITS), round(v.z, ROUND_DIGITS)]


def _rot(r):
    return [round(r.pitch, ROUND_DIGITS), round(r.yaw, ROUND_DIGITS), round(r.roll, ROUND_DIGITS)]


def _distance(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def _field(struct, name):
    try:
        return struct.get_editor_property(name)
    except Exception:  # field not exposed to Python in this engine version
        return None


def _notifies(anim):
    """Existing notifies (name, class, time when exposed, duration). A bonus: never fails the sample."""
    events = []
    try:
        for event in unreal.AnimationLibrary.get_animation_notify_events(anim):
            notify = _field(event, "notify") or _field(event, "notify_state_class")
            time = _field(event, "link_value")
            duration = _field(event, "duration")
            events.append({"name": str(_field(event, "notify_name")),
                           "class": notify.get_class().get_name() if notify else None,
                           "time": round(time, ROUND_DIGITS) if isinstance(time, float) else None,
                           "duration": round(duration, ROUND_DIGITS) if isinstance(duration, float) else None})
    except Exception as error:
        unreal.log_warning("{} notify listing failed: {}".format(LOG_PREFIX, error))
    return events


def sample(anim_path, bones, out_dir):
    anim = unreal.load_asset(anim_path)
    if not isinstance(anim, unreal.AnimSequence):
        unreal.log_error("{} not an AnimSequence: {}".format(LOG_PREFIX, anim_path))
        return None

    length = unreal.AnimationLibrary.get_sequence_length(anim)
    num_frames = unreal.AnimationLibrary.get_num_frames(anim)
    options = unreal.AnimPoseEvaluationOptions()

    first_pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, 0.0, options)
    known = set(str(n) for n in unreal.AnimPoseExtensions.get_bone_names(first_pose))
    present = [b for b in bones if b in known]
    missing = [b for b in bones if b not in known]

    frames = []
    previous = {}
    previous_time = None
    for frame in range(num_frames + 1):
        time = min(unreal.AnimationLibrary.get_time_at_frame(anim, frame), length)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, time, options)
        row = {"frame": frame, "time": round(time, ROUND_DIGITS), "bones": {}}
        for bone in present:
            xform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            pos = _vec(xform.translation)
            dt = (time - previous_time) if previous_time is not None else 0.0
            speed = _distance(pos, previous[bone]) / dt if dt > 0.0 else 0.0
            row["bones"][bone] = {"pos": pos, "rot": _rot(xform.rotation.rotator()), "speed": round(speed, ROUND_DIGITS)}
            previous[bone] = pos
        previous_time = time
        frames.append(row)

    summary = {}
    for bone in present:
        speeds = [f["bones"][bone]["speed"] for f in frames]
        peak = max(range(len(speeds)), key=lambda i: speeds[i])
        summary[bone] = {"peak_speed": speeds[peak], "peak_frame": peak, "peak_time": frames[peak]["time"],
                         "start_to_end_drift": round(_distance(frames[0]["bones"][bone]["pos"],
                                                              frames[-1]["bones"][bone]["pos"]), ROUND_DIGITS)}

    result = {"anim": anim_path, "length": round(length, ROUND_DIGITS), "num_frames": num_frames,
              "fps": round(num_frames / length, ROUND_DIGITS) if length > 0 else 0,
              "space": "component (AnimPoseSpaces.WORLD), cm and degrees [pitch, yaw, roll]; speed cm/s",
              "bones": present, "missing_bones": missing, "notifies": _notifies(anim),
              "summary": summary, "frames": frames}

    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, anim_path.rsplit("/", 1)[-1] + ".json")
    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=1)
    unreal.log("{} {} -> {} ({} frames, missing {})".format(LOG_PREFIX, anim_path, out_path, len(frames), missing))
    return out_path


def main(argv):
    args = list(argv)
    bones = DEFAULT_BONES
    if "--bones" in args:
        index = args.index("--bones")
        bones = [b for b in args[index + 1].split(",") if b]
        del args[index:index + 2]
    if len(args) < 2:
        unreal.log_error("{} usage: sample_bones.py <out_dir> <anim_path> [<anim_path> ...] [--bones a,b,c]".format(LOG_PREFIX))
        return
    out_dir = args[0]
    for anim_path in args[1:]:
        sample(anim_path, bones, out_dir)


if __name__ == "__main__":
    main(sys.argv[1:])
