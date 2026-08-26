# SEDS BPHC Avionics Induction 2026-27
# Name: Yashvardhan Singh
# ID: 2025A7PS0112H
"""
Odysseus Navigation Subsystem: Sea Floor Telemetry Processor

This module processes raw bathymetric sonar data for the SEDS Avionics Induction.
It handles data sanitization, outlier removal via statistical filtration, 
continuous bathymetric reconstruction, and high-frequency noise reduction.
Finally, it renders a live animated nautical HUD.

Author: Yashvardhan Singh
Date: August 2026
Version: 1.0.0
"""

import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from typing import Tuple

# =============================================================================
# DATA PIPELINE CONFIGURATION
# =============================================================================

def load_and_clean_data(filename: str = "Depth Data (1).csv") -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Ingests and processes raw sonar telemetry data.

    Executes a multi-stage data cleaning pipeline:
    1. Coerces corrupted string tokens to NaN.
    2. Flags dead sensor dropouts.
    3. Applies a 3-sigma (3σ) rolling median filter to remove acoustic anomalies.
    4. Interpolates missing values to reconstruct continuous topography.
    5. Applies an Exponential Moving Average (EMA) to suppress high-frequency noise.

    Args:
        filename (str): The path to the raw telemetry CSV file.

    Returns:
        Tuple[np.ndarray, np.ndarray, np.ndarray]: 
            - raw_depth: The absolute raw depth array (including NaNs).
            - cleaned: The interpolated and outlier-filtered depth array.
            - smoothed: The final EMA-smoothed depth array.
            
    Raises:
        FileNotFoundError: If the specified CSV file does not exist in the path.
    """
    if not os.path.exists(filename):
        raise FileNotFoundError(f"Source data '{filename}' not found in the current directory.")

    # 1. Ingestion & Coercion
    df = pd.read_csv(filename)
    raw_depth = pd.to_numeric(df['Depth (m)'], errors='coerce')
    cleaned = raw_depth.abs().copy()
    
    # 2. Dropout Filtration
    cleaned[cleaned <= 0] = np.nan
    
    # 3. Statistical Outlier Rejection (3-Sigma Rolling Window)
    window_size = 7
    rolling_med = cleaned.rolling(window=window_size, center=True, min_periods=1).median()
    rolling_std = cleaned.rolling(window=window_size, center=True, min_periods=1).std().fillna(5.0)
    outlier_mask = np.abs(cleaned - rolling_med) > (3.0 * rolling_std)
    cleaned[outlier_mask] = np.nan
    
    # 4. Topographical Reconstruction
    cleaned = cleaned.interpolate(method='linear').bfill().ffill()
    
    # 5. Noise Suppression (EMA)
    smoothed = cleaned.ewm(span=window_size, adjust=False).mean()
    
    return raw_depth.abs().values, cleaned.values, smoothed.values

# Process data and initialize time vectors
raw_data, cleaned_data, filtered_data = load_and_clean_data("Depth Data (1).csv")
total_frames = len(filtered_data)
time_axis = np.arange(1, total_frames + 1)

# =============================================================================
# TELEMETRY VISUALIZATION HUD
# =============================================================================

# Initialize workspace and axis styling
fig, ax = plt.subplots(figsize=(12, 6), dpi=100)
fig.patch.set_facecolor('#0d1117')
ax.set_facecolor('#161b22')

ax.set_title("Odysseus Navigation Subsystem — Live Sea Floor Telemetry", 
             fontsize=14, fontweight='bold', color='#58a6ff', pad=15)
ax.set_xlabel("Mission Elapsed Time (s)", fontsize=11, fontweight='bold', color='#c9d1d9')
ax.set_ylabel("Depth (m)", fontsize=11, fontweight='bold', color='#c9d1d9')

# Configure fixed viewport (inverted Y-axis for depth representation)
ax.set_xlim(0, 305)
ax.set_ylim(460, 0)
ax.grid(True, linestyle='--', alpha=0.3, color='#8b949e')
ax.tick_params(colors='#c9d1d9')

# Instantiate dynamic UI elements
line_raw, = ax.plot([], [], color='#ff7b72', linestyle=':', alpha=0.6, label='Raw Telemetry')
line_filtered, = ax.plot([], [], color='#39d353', linewidth=2.2, label='Filtered Profile')
scatter_ship = ax.scatter([], [], color='#f0883e', s=70, zorder=5, label='Vessel Position')
telemetry_box = ax.text(0.03, 0.92, '', transform=ax.transAxes, color='#f0f6fc',
                        fontsize=10, bbox=dict(facecolor='#21262d', edgecolor='#30363d', boxstyle='round,pad=0.5'))

ax.legend(loc='lower right', facecolor='#21262d', edgecolor='#30363d', labelcolor='#c9d1d9')

def _init_animation():
    """Initializes plot artists to an empty state prior to the first frame."""
    line_raw.set_data([], [])
    line_filtered.set_data([], [])
    scatter_ship.set_offsets(np.empty((0, 2)))
    telemetry_box.set_text('')
    return line_raw, line_filtered, scatter_ship, telemetry_box

def _update_frame(frame: int):
    """
    Calculates and renders a single frame of the animation sequence.

    Args:
        frame (int): The current execution frame index.
    """
    current_t = time_axis[:frame + 1]
    
    line_raw.set_data(current_t, raw_data[:frame + 1])
    line_filtered.set_data(current_t, filtered_data[:frame + 1])

    curr_depth = filtered_data[frame]
    scatter_ship.set_offsets([[current_t[-1], curr_depth]])
    telemetry_box.set_text(f"MET: {current_t[-1]}s / 300s | Depth: {curr_depth:.2f} m")

    return line_raw, line_filtered, scatter_ship, telemetry_box

# Bind animation routine to figure
ani = animation.FuncAnimation(
    fig, _update_frame, frames=total_frames, init_func=_init_animation, 
    blit=False, interval=30, repeat=False
)

if __name__ == "__main__":
    plt.tight_layout()
    plt.show()