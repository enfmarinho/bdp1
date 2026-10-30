"""
Animate geolocated tweets over time and save the animation as a GIF.

Install dependencies:
    pip install pandas matplotlib pillow

Expected CSV columns:
    Longitude, Latitude, Timestamp, Timezone, Class

Timestamp format:
    YYYYMMDDHHMMSS, e.g. 20130112000000

Timezone codes:
    1 = Eastern, 2 = Central, 3 = Mountain, 4 = Pacific

Assumption:
    Timestamp is local wall-clock time in the timezone identified by Timezone.
    corrected_timestamp is converted to UTC.
"""

from zoneinfo import ZoneInfo

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.colors import to_hex

import argparse
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate an animated GIF from a tweet CSV file."
    )

    parser.add_argument(
        "input",
        type=Path,
        help="Path to the input CSV file.",
    )

    parser.add_argument(
        "--rotulos", 
        type=Path, 
        required=True, 
        help="Path to the binary labels (.bin)."
    )

    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Path to the output GIF file (optional).",
    )

    args = parser.parse_args()

    if args.output is None:
        args.output = args.input.with_name(
            f"{args.input.stem}_animation.gif"
        )

    return args


# INPUT_CSV = "test_tweets_5000.csv"
# OUTPUT_GIF = "tweets_over_time.gif"

# Change these if your dataset uses different timezone conventions.
TIMEZONE_MAP = {
    1: "America/New_York",
    2: "America/Chicago",
    3: "America/Denver",
    4: "America/Los_Angeles",
}

# Animation settings
# FPS = 5
# FRAME_INTERVAL_MS = 200
MAX_FRAMES = 300  # Set to None to use every distinct timestamp.
TARGET_FPS = 30
ANIMATION_DURATION_SECONDS = 5
MAX_FPS = 30

POINT_SIZE = 12
POINT_ALPHA = 0.75


def parse_local_timestamp(row):
    """Interpret the timestamp as local time in the row's stated timezone."""
    timezone_code = int(row["Timezone"])
    if timezone_code not in TIMEZONE_MAP:
        raise ValueError(f"Unknown Timezone code: {timezone_code}")

    # Parse YYYYMMDDHHMMSS as a naive datetime.
    local_time = pd.to_datetime(
        str(int(row["Timestamp"])),
        format="%Y%m%d%H%M%S",
        errors="raise",
    )

    # Localize to the relevant US time zone, then convert to UTC.
    # Ambiguous/nonexistent local times around DST transitions are surfaced
    # as errors rather than silently guessed.
    return (
        local_time
        .tz_localize(
            ZoneInfo(TIMEZONE_MAP[timezone_code]),
            ambiguous="raise",
            nonexistent="raise",
        )
        .tz_convert("UTC")
    )


def main():
    args = parse_args()
    df = pd.read_csv(args.input)

    rotulos = np.fromfile(args.rotulos, dtype=np.int32)

    if len(df) != len(rotulos):
        raise ValueError(f"Tamanho incompatível: CSV ({len(df)}) vs Rótulos ({len(rotulos)})")

    df["Class"] = rotulos
    required = {"Longitude", "Latitude", "Timestamp", "Timezone", "Class"}
    missing = required - set(df.columns)
    if missing:
        raise ValueError(f"CSV is missing required columns: {sorted(missing)}")

    # Remove rows without coordinates or class labels.
    df = df.dropna(subset=["Longitude", "Latitude", "Timestamp", "Timezone", "Class"]).copy()

    df["Longitude"] = pd.to_numeric(df["Longitude"], errors="raise")
    df["Latitude"] = pd.to_numeric(df["Latitude"], errors="raise")
    df["Timezone"] = pd.to_numeric(df["Timezone"], errors="raise").astype(int)

    # Convert timestamps into a common timeline (UTC).
    df["corrected_timestamp"] = df.apply(parse_local_timestamp, axis=1)
    df = df.sort_values("corrected_timestamp").reset_index(drop=True)

    if df.empty:
        raise ValueError("No valid rows found in the CSV.")

    classes = sorted(df["Class"].astype(str).unique())
    df["Class"] = df["Class"].astype(str)

    # Give every class a distinct color.
    cmap = plt.get_cmap("tab20", max(len(classes), 1))
    colors = {label: to_hex(cmap(i)) for i, label in enumerate(classes)}

    # One frame per distinct timestamp, or sample timestamps if there are too many.
    timestamps = df["corrected_timestamp"].drop_duplicates().sort_values().tolist()
    NUM_FRAMES = len(timestamps)

    FPS = min(
        MAX_FPS,
        NUM_FRAMES / ANIMATION_DURATION_SECONDS
    )

    NUM_FRAMES = ANIMATION_DURATION_SECONDS * TARGET_FPS
    
    FRAME_INTERVAL_MS = 1000 / TARGET_FPS


    if MAX_FRAMES is not None and len(timestamps) > MAX_FRAMES:
        indices = [
            round(i * (len(timestamps) - 1) / (MAX_FRAMES - 1))
            for i in range(MAX_FRAMES)
        ] if MAX_FRAMES > 1 else [0]
        timestamps = [timestamps[i] for i in indices]

    # Compute consistent plot bounds.
    lon_min, lon_max = df["Longitude"].min(), df["Longitude"].max()
    lat_min, lat_max = df["Latitude"].min(), df["Latitude"].max()
    lon_pad = max((lon_max - lon_min) * 0.03, 0.1)
    lat_pad = max((lat_max - lat_min) * 0.03, 0.1)

    fig, ax = plt.subplots(figsize=(11, 7))
    fig.subplots_adjust(right=0.80)

    # Create one scatter artist per class, so the legend and colors remain stable.
    scatters = {}
    for label in classes:
        scatters[label] = ax.scatter(
            [], [],
            s=POINT_SIZE,
            alpha=POINT_ALPHA,
            color=colors[label],
            label=label,
            edgecolors="none",
        )

    ax.set_xlim(lon_min - lon_pad, lon_max + lon_pad)
    ax.set_ylim(lat_min - lat_pad, lat_max + lat_pad)
    ax.set_xlabel("Longitude")
    ax.set_ylabel("Latitude")
    ax.set_title("Tweets over time")
    ax.grid(True, alpha=0.25)
    ax.legend(
        title="Class",
        loc="upper left",
        bbox_to_anchor=(1.01, 1.0),
        borderaxespad=0,
    )

    timestamp_text = ax.text(
        0.02, 0.98, "",
        transform=ax.transAxes,
        ha="left",
        va="top",
        bbox={"facecolor": "white", "alpha": 0.8, "edgecolor": "none"},
    )

    frame_indices = np.linspace(
        0,
        len(timestamps) - 1,
        NUM_FRAMES,
        dtype=int,
    )

    def update(frame_index):
        timestamp_index = frame_indices[frame_index]
        current_time = timestamps[timestamp_index]
        visible = df[df["corrected_timestamp"] <= current_time]

        for label, artist in scatters.items():
            subset = visible[visible["Class"] == label]
            artist.set_offsets(subset[["Longitude", "Latitude"]].to_numpy())

        timestamp_text.set_text(
            f"UTC: {current_time.strftime('%Y-%m-%d %H:%M:%S')}\n"
            f"Tweets shown: {len(visible):,} / {len(df):,}"
        )
        return [*scatters.values(), timestamp_text]

    # NUM_FRAMES = len(timestamps)

    # FRAME_INTERVAL_MS = (
    #     ANIMATION_DURATION_SECONDS * 1000 / NUM_FRAMES
    # )

    # animation = FuncAnimation(
    #     fig,
    #     update,
    #     frames=NUM_FRAMES,
    #     interval=FRAME_INTERVAL_MS,
    #     blit=False,
    #     repeat=False,
    # )

    animation = FuncAnimation(
        fig,
        update,
        frames=NUM_FRAMES,
        interval=FRAME_INTERVAL_MS,
        blit=False,
        repeat=False,
    )

    animation.save(args.output, writer=PillowWriter(fps=FPS))
    plt.close(fig)

    # Save the converted data too, so corrected_timestamp is available for analysis.
    df.to_csv(args.input.with_name(f"{args.input.stem}_with_corrected_timestamp.csv"), index=False)

    

    print(f"Saved animation: {args.output}")
    print("Saved converted data: tweets_with_corrected_timestamp.csv")
    print(f"Rows: {len(df):,}; classes: {len(classes)}; frames: {len(timestamps)}")


if __name__ == "__main__":
    main()
