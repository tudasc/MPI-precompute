import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.ticker as mticker
import os
import yaml
import math
import numpy as np
import argparse
import pandas as pd

CACHE_FILE = "cache.csv"
PLTSIZE = (12, 8)

lower = 0
upper = 100

colors_list = ["#4477AA", "#228833"]


def add_violin(ax, x, df, y_selector, color="#4477AA"):
    y = df[y_selector].to_numpy()

    if len(y) > 1:  # if data is present
        violin_parts = ax.violinplot([y], [x], widths=[1.5], quantiles=[lower / 100, upper / 100], showmeans=True,
                                     showmedians=True, showextrema=False)

        swarm = simple_beeswarm(y, nbins=6)
        swarm = swarm + x
        ax.plot(swarm, y, 'o', color=color)
        for pc in violin_parts['bodies']:
            pc.set_color(color)


# from
# https://stackoverflow.com/questions/36153410/how-to-create-a-swarm-plot-with-matplotlib
def simple_beeswarm(y, nbins=None):
    """
    Returns x coordinates for the points in ``y``, so that plotting ``x`` and
    ``y`` results in a bee swarm plot.
    """
    y = np.asarray(y)
    if nbins is None:
        nbins = len(y) // 6

    # Get upper bounds of bins
    x = np.zeros(len(y))
    if len(y) < nbins:
        return x
    ylo = np.min(y)
    yhi = np.max(y)
    dy = (yhi - ylo) / nbins
    ybins = np.linspace(ylo + dy, yhi - dy, nbins - 1)

    # Divide indices into bins
    i = np.arange(len(y))
    ibs = [0] * nbins
    ybs = [0] * nbins
    nmax = 0
    for j, ybin in enumerate(ybins):
        f = y <= ybin
        ibs[j], ybs[j] = i[f], y[f]
        nmax = max(nmax, len(ibs[j]))
        f = ~f
        i, y = i[f], y[f]
    ibs[-1], ybs[-1] = i, y
    nmax = max(nmax, len(ibs[-1]))

    # Assign x indices
    dx = 1 / (nmax // 2)
    for i, y in zip(ibs, ybs):
        if len(i) > 1:
            j = len(i) % 2
            i = i[np.argsort(y)]
            a = i[j::2]
            b = i[j + 1::2]
            x[a] = (0.5 + j / 3 + np.arange(len(b))) * dx
            x[b] = (0.5 + j / 3 + np.arange(len(b))) * -dx

    return x


def get_plot(df, plot_fname, plot_title, x_axis='nprocs', y_axis='runtime'):
    ftsize = 16
    plt.rcParams.update({'font.size': ftsize})
    # plt.rcParams.update({'font.size': 18, 'hatch.linewidth': 0.0075})
    figsz = PLTSIZE

    fig = plt.figure(figsize=figsz)
    ax = plt.gca()

    for x_val, group in df.groupby(x_axis):
        add_violin(ax, x_val - 0.5, group.loc[group['is_altered'] == False], y_axis, colors_list[0])
        add_violin(ax, x_val + 0.5, group.loc[group['is_altered'] == True], y_axis, colors_list[1])

    patches = [mpatches.Patch(color=colors_list[0], label="Original"),
               mpatches.Patch(color=colors_list[1], label="Altered")]
    ax.legend(handles=patches, loc='upper left')
    ax.set_xlabel(x_axis)
    ax.set_ylabel(y_axis)
    plt.title(plot_title)

    plt.tight_layout()
    output_format = "pdf"
    plt.savefig(plot_fname + "." + output_format, bbox_inches='tight')


def get_data_process_count(data, key, buf_size, calctime):
    select_df = data[(data['cycles'] == cycles_to_use) & (
            data['warmup'] == warmup_to_use) & (data['mode'] == key) & (
                             data['calctime'] == calctime) & (data['buflen'] == buf_size)]

    return select_df.groupby('nprocs')['overhead'].median()


def read_file_to_df(filename, is_altered, param_list):
    temp_df = pd.read_csv(filename, names=['nprocs', 'runtime'])
    temp_df['is_altered'] = is_altered
    for i in range(0, len(param_list), 2):
        temp_df[param_list[i][1:]] = param_list[i + 1]
    return temp_df


def read_data(data_dir, param_file):
    param_list = get_param_list(param_file)
    cols = ['nprocs', 'runtime', 'is_altered'] + param_list
    df = pd.DataFrame(columns=cols)
    print("Read Data ...")

    with open(param_file, 'r') as f:
        parameter_combinations = f.readlines()

    for p in parameter_combinations:
        p_list = p.split()

        p_fname = p.replace(' ', '').strip()  # remove trailing newline

        altered_fname = data_dir + "/ALTERED_" + p_fname + ".csv"
        if os.path.isfile(altered_fname):
            df2 = read_file_to_df(altered_fname, True, p_list)
            df = pd.concat([df, df2], axis='index', ignore_index=True)
        else:
            print("Missing Expected file: " + altered_fname)

        orig_fname = data_dir + "/ORIG_" + p_fname + ".csv"
        if os.path.isfile(orig_fname):
            df2 = read_file_to_df(orig_fname, False, p_list)
            df = pd.concat([df, df2], axis='index', ignore_index=True)
        else:
            print("Missing Expected file: " + orig_fname)

    # convert to int / bool
    df['nprocs'] = df['nprocs'].astype(int)
    df['is_altered'] = df['is_altered'].astype(bool)
    # TODO what if not all parameters are integers?
    for p in param_list:
        df[p] = df[p].astype(int)

    return df


def get_param_list(file):
    with open(file, 'r') as f:
        lines = f.readlines()
        first = lines[0].split()
    param_list = []
    for i in range(0, len(first), 2):
        # remove the -
        param = first[i][1:]
        # if param is given with -- remove both of those
        if param[0] == '-':
            param = param[1:]
        param_list.append(param)

    return param_list


def main():
    parser = argparse.ArgumentParser(
        description='Generates Visualization - set the environment variables according to the application you want to analyze')
    parser.add_argument('--cache',
                        help='use the content of the cache file named %s. do NOT set this argument, if you want to re-write the cache' % CACHE_FILE)
    args = parser.parse_args()

    app_name = os.environ['APPLICATION_NAME']
    data_dir = os.environ['EXPERIMENT_DIR']
    param_file = os.environ['VARIABLE_PARAM_FILE']

    if args.cache:
        data = pd.read_csv(CACHE_FILE, index_col=0)
    else:
        data = read_data(data_dir, param_file)
        data.to_csv(CACHE_FILE)

    print("generating plots ...")

    to_plot = data.loc[(data['n'] == 100) & (data['i'] == 1000)]
    get_plot(to_plot, "n100-i1000", app_name)
    to_plot = data.loc[(data['n'] == 1000) & (data['i'] == 1000)]
    get_plot(to_plot, "n1000-i1000", app_name)
    to_plot = data.loc[(data['n'] == 10000) & (data['i'] == 1000)]
    get_plot(to_plot, "n10000-i1000", app_name)
    print("done")


if __name__ == "__main__":
    main()
