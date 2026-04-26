import argparse
import pandas as pd
import sys
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# ... (Styling constants remain the same) ...
BG_COLOR      = "#0a0a0a"   
PANEL_COLOR   = "#111111"   
GRID_COLOR    = "#2a2a2a"   
RAW_COLOR     = "#e0e0e0"   
FILT_COLOR    = "#00bfff"   
THRESH_COLOR  = "#ff4444"   # Red for the threshold line
TITLE_COLOR   = "#7ec8e3"   
TICK_COLOR    = "#888888"
AXIS_COLOR    = "#444444"

def style_axes(ax, title: str, ylabel: str = "Value"):
    ax.set_facecolor(PANEL_COLOR)
    ax.tick_params(colors=TICK_COLOR, which="both", labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(AXIS_COLOR)
    ax.grid(True, color=GRID_COLOR, linewidth=0.5, linestyle="--", alpha=0.7)
    ax.set_title(title, color=TITLE_COLOR, fontsize=11, fontweight="bold", loc="left")
    ax.set_xlabel("Pixel Index", color=TICK_COLOR, fontsize=8)
    ax.set_ylabel(ylabel, color=TICK_COLOR, fontsize=8)

def plot_pipeline_results(df, csv_path, tv_value, save_path=None):
    n = len(df)
    x = df['index']
    
    fig = plt.figure(figsize=(14, 10), facecolor=BG_COLOR)
    fig.suptitle("CynLr Scanner Pipeline - Hardware Verification", 
                 color=TITLE_COLOR, fontsize=14, fontweight="bold", y=0.97)

    gs = GridSpec(3, 1, figure=fig, hspace=0.4, top=0.92, bottom=0.08, left=0.08, right=0.96)

    # ── Panel 1: Raw Input ────────────────────────────────────────────
    ax1 = fig.add_subplot(gs[0])
    ax1.plot(x, df['raw_pixel'], color=RAW_COLOR, linewidth=0.8, alpha=0.3, label="Raw Sensor Data")
    style_axes(ax1, "1. Raw Input Stream (Captured by DataGenerationBlock)", "Intensity (0-255)")
    ax1.legend(loc="upper right", fontsize=8, facecolor=PANEL_COLOR, labelcolor=TICK_COLOR)

    # ── Panel 2: Filtered Output + THRESHOLD LINE ──────────────────────
    ax2 = fig.add_subplot(gs[1])
    ax2.plot(x, df['filtered_value'], color=FILT_COLOR, linewidth=1.2, label="Gaussian Smoothed")
    
    # ADDED: Horizontal line representing the threshold value
    ax2.axhline(y=tv_value, color=THRESH_COLOR, linestyle="--", linewidth=1.5, alpha=0.9, 
                label=f"Threshold (TV={tv_value})")
    
    # Optional: Highlight where the signal is above threshold
    ax2.fill_between(x, df['filtered_value'], tv_value, 
                     where=(df['filtered_value'] >= tv_value),
                     color=FILT_COLOR, alpha=0.1, interpolate=True)

    style_axes(ax2, "2. DSP Output (9-tap Gaussian Convolution)", "Filtered Value")
    ax2.legend(loc="upper right", fontsize=8, facecolor=PANEL_COLOR, labelcolor=TICK_COLOR)

    # ── Panel 3: Threshold Result ─────────────────────────────────────
    ax3 = fig.add_subplot(gs[2])
    ax3.step(x, df['threshold_status'], color="#00e676", linewidth=1.2, where="post")
    ax3.fill_between(x, df['threshold_status'], step="post", alpha=0.15, color="#00e676")
    style_axes(ax3, "3. Final Binary Output (Threshold Logic)", "0 / 1")
    ax3.set_yticks([0, 1])
    ax3.set_yticklabels(["OFF", "TRIGGER"], color=TICK_COLOR)

    # ── Stats Footer ──────────────────────────────────────────────────
    total_triggers = df['threshold_status'].sum()
    stats_text = (f"Source: {csv_path}  |  Threshold TV: {tv_value}  |  "
                  f"Total Samples: {n}  |  Triggers Detected: {int(total_triggers)}")
    fig.text(0.5, 0.02, stats_text, ha="center", color=TICK_COLOR, fontsize=9)

    if save_path:
        plt.savefig(save_path, dpi=150, facecolor=BG_COLOR)
        print(f"[OK] Visualisation saved to {save_path}")
    else:
        plt.show()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", default="data/out.csv", help="Path to pipeline out.csv")
    parser.add_argument("--save", help="Path to save image")
    # ADDED: Argument to specify the TV used in the C++ run
    parser.add_argument("--tv", type=float, default=100.0, help="Threshold value (TV) used in C++")
    args = parser.parse_args()

    try:
        df = pd.read_csv(args.csv)
    except Exception as e:
        print(f"Error loading {args.csv}: {e}")
        sys.exit(1)

    print(f"Visualising {len(df)} samples with TV={args.tv} from {args.csv}...")
    plot_pipeline_results(df, args.csv, args.tv, args.save)

if __name__ == "__main__":
    main()