import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from io import StringIO

DATAPATH = "/home/tim/precompute/use-cases/thread-sanitizer/sample_apps/performance-eval/results"
NUM_T_TO_SHOW = 96

colors = [
    "#DDAA33",
    "#BB5566",
    "#004488"
]
mode_order = ["modified", "normal", "without"]
mode_to_color = dict(zip(mode_order, colors))

def get_plot_specomp(df_specomp):

    fig, axs = plt.subplots(1, 3, sharey=False, figsize=(12, 5))

    for ax, (bench, df_group) in zip(axs, df_specomp.groupby("Benchmark")):

        sns.lineplot(data=df_group, x="num_threads", y="time", hue="mode", hue_order=mode_order,
                     palette=mode_to_color, marker="o", ax=ax)
        ax.set_title(f"{bench}")
        ax.set_xlabel("Number of Threads")
        ax.set_ylabel("Time (s)")
        ax.legend(title="Tsan Runtime")

        # Compute offset for label positions
        y_min, y_max = ax.get_ylim()
        y_offset = 0.03 * (y_max - y_min)

        mean_times = df_group.groupby(["num_threads", "mode"])["time"].mean().reset_index()
        pivoted = mean_times.pivot(index="num_threads", columns="mode", values="time")
        percentages = pivoted.div(pivoted["without"], axis=0)

        # Annotate slowdown on plots
        for thread in percentages.index:
            for mode in ["modified", "normal"]:
                time_val = df_group[(df_group["num_threads"] == thread) & (df_group["mode"] == mode)][
                    "time"].median()
                slowdown = percentages.loc[thread, mode]
                color = mode_to_color[mode]
                # Offset: above for normal, below for modified
                if mode == "normal":
                    offset = y_offset
                    va = "bottom"
                else:  # modified
                    offset = -y_offset
                    va = "top"

                if bench == "358.botsalgn":
                    ax.text(thread, time_val + offset, f"{slowdown:.2f}×", ha="center", va='center', fontsize=8,
                            color=color)
                else:
                    ax.text(thread, time_val + offset, f"{slowdown:.0f}×", ha="center", va='center', fontsize=8,
                            color=color)

    fig.suptitle(f"SpecOMP 2012: Overhead vs Number of Threads")
    plt.tight_layout()
    plt.savefig(f"specomp.pdf")


def get_plot(df, name):
    fig, (ax1, ax2) = plt.subplots(1, 2, sharey=True, figsize=(12, 5))

    sns.lineplot(data=df[df["num_threads"] == NUM_T_TO_SHOW], x="size", y="time", hue="mode",
                 hue_order=mode_order, palette=mode_to_color, marker="o", ax=ax1, )
    ax1.set_title(f"{name}: Overhead vs Problem size ({NUM_T_TO_SHOW} threads)")
    ax1.set_xlabel("Problem Size")
    ax1.set_ylabel("Time (s)")
    ax1.legend(title="Tsan Runtime")

    max_size = df["size"].max()
    sns.lineplot(data=df[df["size"] == max_size], x="num_threads", y="time", hue="mode", hue_order=mode_order,
                 palette=mode_to_color, marker="o", ax=ax2)
    ax2.set_title(f"{name}: Overhead vs Number of Threads (size = {max_size})")
    ax2.set_xlabel("Number of Threads")
    ax2.set_ylabel("Time (s)")
    ax2.legend(title="Tsan Runtime")

    # Compute offset for label positions
    y_min, y_max = ax1.get_ylim()
    y_offset = 0.03 * (y_max - y_min)

    mean_times = df.groupby(["size", "mode"])["time"].mean().reset_index()
    pivoted = mean_times.pivot(index="size", columns="mode", values="time")
    percentages = pivoted.div(pivoted["without"], axis=0)

    # Annotate slowdown on plots
    for s in percentages.index:
        for mode in ["modified", "normal"]:
            time_val = df[(df["num_threads"] == NUM_T_TO_SHOW) & (df["size"] == s) & (df["mode"] == mode)][
                "time"].median()
            slowdown = percentages.loc[s, mode]
            color = mode_to_color[mode]
            # Offset: above for normal, below for modified
            if mode == "normal":
                offset = y_offset
                va = "bottom"
            else:  # modified
                offset = -y_offset
                va = "top"

            ax1.text(s, time_val + offset, f"{slowdown:.0f}×", ha="center", va='center', fontsize=8, color=color)

    mean_times = df.groupby(["num_threads", "mode"])["time"].mean().reset_index()
    pivoted = mean_times.pivot(index="num_threads", columns="mode", values="time")
    percentages = pivoted.div(pivoted["without"], axis=0)

    # Annotate slowdown on plots
    for thread in percentages.index:
        for mode in ["modified", "normal"]:
            time_val = df[(df["size"] == max_size) & (df["num_threads"] == thread) & (df["mode"] == mode)][
                "time"].median()
            slowdown = percentages.loc[thread, mode]
            color = mode_to_color[mode]
            # Offset: above for normal, below for modified
            if mode == "normal":
                offset = y_offset
                va = "bottom"
            else:  # modified
                offset = -y_offset
                va = "top"

            ax2.text(thread, time_val + offset, f"{slowdown:.0f}×", ha="center", va='center', fontsize=8, color=color)

    plt.tight_layout()
    plt.savefig(f"{name}.pdf")


def main():
    # hpccg
    df_hpccg = pd.read_csv(
        DATAPATH + "/results_hpccg.csv")  # ,names=["config", "num_threads", "mode", "time"],skiprows=[0])
    # Extract size and iterations using regex
    df_hpccg['size'] = df_hpccg['config'].str.extract(r'(\d+)').astype(int)
    # time is in seconds: to float
    df_hpccg['time'] = df_hpccg['time'].str.replace('s', '').astype(float)
    df_hpccg = df_hpccg[df_hpccg['size'] <= 200]  # timeout on larger measurements

    get_plot(df_hpccg, 'HPCCG')

    # lulesh
    df_lulesh = pd.read_csv(DATAPATH + "/results_lulesh.csv")
    # Extract size and iterations using regex
    df_lulesh['size'] = df_lulesh['config'].str.extract(r'-s\s+(\d+)').astype(int)
    df_lulesh['iterations'] = df_lulesh['config'].str.extract(r'-i\s+(\d+)').astype(int)
    # time is in seconds: to float
    df_lulesh['time'] = df_lulesh['time'].str.replace('s', '').astype(float)
    df_lulesh = df_lulesh[df_lulesh['iterations'] == 20]  # no difference based on iteration count anyway

    get_plot(df_lulesh, 'LULESH')

    # teaLeaf

    # Read the file while skipping unwanted lines
    with open(DATAPATH + "/results_tealeaf.csv") as f:
        # tealeaf tries to verify the solution but fails to read the problems file in my experiment
        lines = [line.strip() for line in f if not line.startswith("Command exited")]

    # Now read the cleaned lines into a DataFrame
    cleaned_data = "\n".join(lines)
    df_tealeaf = pd.read_csv(StringIO(cleaned_data), header=None,
                             names=["size", "iterations", "num_threads", "mode", "time"])

    # Optional: convert time column from "Xs" to float
    df_tealeaf["time"] = df_tealeaf["time"].str.rstrip("s").astype(float)
    df_tealeaf = df_tealeaf[df_tealeaf["iterations"] == 2]  # no difference based on iteration count anyway
    df_tealeaf = df_tealeaf[df_tealeaf["size"] <= 2048]  # timeout on larger measurements

    get_plot(df_tealeaf, 'TeaLeaf')

    # spec omp
    df_specomp = pd.read_csv(DATAPATH + "/specomp2012/spec_train.csv")
    df_specomp = df_specomp.rename(
        columns={"Run Mode": "mode", "Base # Threads": "num_threads", "Est. Base Run Time": "time"})
    get_plot_specomp(df_specomp)


if __name__ == "__main__":
    main()
