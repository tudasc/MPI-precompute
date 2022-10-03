import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.ticker as mticker
import os
import yaml
import math
import numpy as np
import argparse
import pandas as pd

import seaborn as sns

DATA_DIR = "/work/scratch/tj75qeje/mpi-comp-match/output/"


CACHE_FILE = "cache.csv"
PLTSIZE = (12, 8)
PLTSIZE_BAR_PLT = (12, 4)
PLTSIZE_VIOLIN_PLT = (24, 8)

NORMAL = 1
EAGER = 2
RENDEVOUZ1 = 3
RENDEVOUZ2 = 4

names = {NORMAL: "Normal", EAGER: "Eager", RENDEVOUZ1: "Rendezvous 1", RENDEVOUZ2: "Rendezvous 2"}
# color sceme by Paul Tol https://personal.sron.nl/~pault/
colors = {NORMAL: "#4477AA", EAGER: "#EE6677", RENDEVOUZ1: "#AA3377", RENDEVOUZ2: "#228833"}
colors_list=["#4477AA","#EE6677","#AA3377","#228833"]

buffer_sizes = [4, 8, 32, 512, 1024, 4906, 16384, 65536, 1048576, 4194304, 16777216]
# buffer_sizes = [4, 8, 32, 512, 1024, 16384, 1048576, 4194304, 16777216]
# buffer_sizes = [1048576,4194304,16777216]
buffer_sizes_scaling = [65536, 1048576, 4194304, 16777216]
nrows = 1

comptime_for_barplots = 10000
cycles_to_use = 64
warmup_to_use = 8
process_count_to_use=2

# limited set for faster measurement
# buffer_sizes = [8,1024,16384,65536,262144,1048576,4194304,16777216]
# buffer_sizes = [8,32,512,1024,16384]
# nrows = 2

upper = 100
lower = 0


def mean_percentile_range(array, upper, lower):
    # from:
    # https://stackoverflow.com/questions/61391138/numpy-mean-percentile-range-eg-mean-25th-to-50th-percentile
    # find the indexes of the element below 25th and 50th percentile
    idx_under_25 = np.argwhere(array < np.percentile(array, lower))
    idx_under_50 = np.argwhere(array <= np.percentile(array, upper))
    # the values for 25 and 50 are both included

    # find the number of the elements in between 25th and 50th percentile
    diff_num = len(idx_under_50) - len(idx_under_25)

    # find the sum difference
    diff_sum = np.sum(np.take(array, idx_under_50)) - np.sum(np.take(array, idx_under_25))

    # get the mean
    mean = diff_sum / diff_num
    return mean

#TODO this can be done all in pandas, which may be quicker
def get_data_bufsize(data, key, buf_size, calctime):
    select_df = data[(data['nprocs'] == process_count_to_use) &(data['cycles'] == cycles_to_use) & (data['warmup'] == warmup_to_use) & (data['mode'] == key) & (
                data['calctime'] == calctime) & (data['buflen'] == buf_size)]

    if len(select_df.index) == 0:
        # empty
        return 0, 0, 0, 0, [0]
    y = select_df['overhead'].to_numpy()
    y_min = np.percentile(y, lower)
    y_max = np.percentile(y, upper)
    y_avg = mean_percentile_range(y, upper, lower)

    y_median = np.median(y)

    return y_min, y_max, y_avg, y_median, y

def add_violin(ax, x, data, key, buf_size, comp_time, show_in_legend=True):
    _, max, _, _, y = get_data_bufsize(data, key, buf_size, comp_time)

    violin_parts = ax.violinplot([y], [x * 2], widths=[1.5], quantiles=[lower / 100, upper / 100], showmeans=True,
                                 showmedians=True, showextrema=False)

    swarm=simple_beeswarm(y,nbins=6)
    swarm = swarm + x*2
    ax.plot(swarm,y, 'o',color=colors[key])
    for pc in violin_parts['bodies']:
        pc.set_color(colors[key])

    return (mpatches.Patch(color=colors[key]), names[key]), max


#from
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
            b = i[j+1::2]
            x[a] = (0.5 + j / 3 + np.arange(len(b))) * dx
            x[b] = (0.5 + j / 3 + np.arange(len(b))) * -dx

    return x

def get_violin_plot(data, buffer_sizes, comp_time, plot_name, scaling=True, fill=False, normal=True, eager=True,
                    rendevouz1=True, rendevouz2=True, scaling_key=NORMAL, split_point=5):
    ftsize = 16
    plt.rcParams.update({'font.size': ftsize})
    # plt.rcParams.update({'font.size': 18, 'hatch.linewidth': 0.0075})
    figsz = PLTSIZE_VIOLIN_PLT

    num_violins = 1  # the space in between two different buffer length
    if normal:
        num_violins += 1
    if eager:
        num_violins += 1
    if rendevouz1:
        num_violins += 1
    if rendevouz2:
        num_violins += 1

    y_pos = range(num_violins * 2 * len(buffer_sizes))
    y_scale = 0

    fig, axs = plt.subplots(nrows=1, ncols=2, figsize=figsz, sharex=False, sharey=False)
    local_split_point = split_point

    current_bar = 0
    show_in_legend = True
    i = 0
    for ax in axs:
        legend_labels = []
        x_tics_labels = []
        x_tics = []
        # reshape axis to have 1d-array we can iterate over
        while i < len(buffer_sizes):
            if current_bar >= local_split_point * num_violins:
                local_split_point = len(buffer_sizes)
                show_in_legend = True
                break

            buf_size = buffer_sizes[i]
            i += 1
            x_tics.append(num_violins / 2 + current_bar * 2)
            if eager:
                label, max_y = add_violin(ax, current_bar, data, EAGER, buf_size, comp_time, show_in_legend)
                current_bar += 1
                if show_in_legend:
                    legend_labels.append(label)
                if scaling_key == EAGER:
                    y_scale = max(max_y, y_scale)
            if rendevouz1:
                label, max_y = add_violin(ax, current_bar, data, RENDEVOUZ1, buf_size, comp_time, show_in_legend)
                current_bar += 1
                if show_in_legend:
                    legend_labels.append(label)
                if scaling_key == RENDEVOUZ1:
                    y_scale = max(max_y, y_scale)
            if rendevouz2:
                label, max_y = add_violin(ax, current_bar, data, RENDEVOUZ2, buf_size, comp_time, show_in_legend)
                current_bar += 1
                if show_in_legend:
                    legend_labels.append(label)
                if scaling_key == RENDEVOUZ2:
                    y_scale = max(max_y, y_scale)
            if normal:
                label, max_y = add_violin(ax, current_bar, data, NORMAL, buf_size, comp_time, show_in_legend)
                current_bar += 1
                if show_in_legend:
                    legend_labels.append(label)
                if scaling_key == NORMAL:
                    y_scale = max(max_y, y_scale)
            current_bar += 1
            show_in_legend = False

            if buf_size < 1024:
                x_tics_labels.append("%d\nB" % buf_size)
            elif buf_size < 1048576:
                x_tics_labels.append("%d\nKiB" % (buf_size / 1024))
            else:
                x_tics_labels.append("%d\nMiB" % (buf_size / 1048576))

        ax.set_xlabel("Buffer Size")
        if ax == axs[0]:
            ax.set_ylabel("communication overhead in $\mu$s")
        else:
            ax.yaxis.tick_right()

        # locator = plt.MaxNLocator(nbins=7)
        # ax.xaxis.set_major_locator(locator)
        # ax.locator_params(axis='x', tight=True, nbins=4)
        if ax == axs[1]:
            ax.legend(*zip(*legend_labels), loc='upper left')

        if scaling:
            ax.set_ylim(0, y_scale * 1.02)
            if ax == axs[0]:
                ax.set_ylim(0, y_scale * 0.25)
            
        # scale to microseconds
        scale_y=1e6
        ticks_y =  mticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x * scale_y))
        ax.yaxis.set_major_formatter(ticks_y)

        # convert to seconds easy comparision with y axis

        ax.set_xticks(x_tics)
        ax.set_xticklabels(x_tics_labels)

    plt.tight_layout()
    output_format = "pdf"
    plt.savefig(plot_name + "." + output_format, bbox_inches='tight')

def get_data_process_count(data, key, buf_size, calctime):
    select_df = data[ (data['cycles'] == cycles_to_use) & (
                data['warmup'] == warmup_to_use) & (data['mode'] == key) & (
                             data['calctime'] == calctime) & (data['buflen'] == buf_size)]

    return select_df.groupby('nprocs')['overhead'].median()


def get_scaling_plot(data, buffer_sizes, comp_time,plot_name, scaling=True, fill=False, normal=True, eager=True,
                       rendevouz1=True, rendevouz2=True):

    ftsize = 16
    plt.rcParams.update({'font.size': ftsize})
    # plt.rcParams.update({'font.size': 18, 'hatch.linewidth': 0.0075})
    figsz = PLTSIZE

    ncols = math.ceil(len(buffer_sizes) * 1.0 / nrows)

    fig = plt.figure(figsize=figsz)
    ax= plt.gca()

    
    for buf_size,color in zip(buffer_sizes,colors_list):
        size_str=""
        if buf_size < 1024:
            size_str=("%dB" % buf_size)
        elif buf_size < 1048576:
            size_str=("%dKiB" % (buf_size / 1024))
        else:
            size_str=("%dMiB" % (buf_size / 1048576))


    # empty lableed entity for legend
        plt.plot([], marker="", ls="",label=size_str)
        
        # get the data
        if normal:
            dat= get_data_process_count(data,NORMAL,buf_size,comp_time)
            ax.plot(dat,linestyle='-',label= "Normal",color=color)
        if eager:
            dat = get_data_process_count(data, EAGER, buf_size, comp_time)
            ax.plot(dat, linestyle='--', label= "Eager",color=color)
        if rendevouz1:
            dat = get_data_process_count(data, RENDEVOUZ1, buf_size, comp_time)
            ax.plot(dat, linestyle='-.', label=  "Rendezvous 1",color=color)
        if rendevouz2:
            dat = get_data_process_count(data, RENDEVOUZ2, buf_size, comp_time)
            ax.plot(dat, linestyle=':', label= "Rendezvous 2",color=color)

    #ax.title("Overhead with different number")
    ax.set_xlabel("number of processes")



    ax.legend(loc='upper right',ncol=4)

    if scaling:
        ax.set_ylim(0, max_y * 1.05)

    ax.set_ylabel("communication overhead in $\mu$s")
    # scale to microseconds
    scale_y = 1e6
    ticks_y = mticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x * scale_y))
    ax.yaxis.set_major_formatter(ticks_y)
    
    xtics = [1,8,16, 32,64]
    #labels = [0, 0.005, 0.01]
    ax.set_xticks(xtics)
    #ax.set_xticklabels(labels)
    plt.tight_layout()
    output_format = "pdf"
    plt.savefig(plot_name + "." + output_format, bbox_inches='tight')

def read_data():
    df = pd.DataFrame(columns=['nprocs','mode', 'calctime', 'warmup', 'cycles', 'buflen', 'overhead'])
    print("Read Data ...")

    i = 0
    # read input data
    with os.scandir(DATA_DIR) as dirBase:
        for entryDir in dirBase:
            if entryDir.is_dir:
                nprocs=int(entryDir.name)
            with os.scandir(entryDir) as dir:
                for entry in dir:
                    if entry.is_file():
        
                        try:
                            # original or modified run
                            mode = -1
                            if "normal" in entry.name:
                                mode = NORMAL
                            elif "eager" in entry.name:
                                mode = EAGER
                            elif "rendevouz1" in entry.name:
                                mode = RENDEVOUZ1
                            elif "rendevouz2" in entry.name:
                                mode = RENDEVOUZ2
                            else:
                                print("Error readig file:")
                                print(entry.name)
                                exit(-1)
        
                            # get calctime and cycles:
                            splitted = entry.name.split("_")
                            calctime = int(splitted[2])
                            # assert("calctime" in splitted[1])
                            assert ("cycles" in splitted[3])
                            cycles = int(splitted[4])
                            assert ("warmup" in splitted[5])
                            warmup = int(splitted[6].split(".")[0])
                            # print(calctime)
        
                            # read data
                            run_data = {}
                            with open(DATA_DIR + "/" +entryDir.name+"/"+ entry.name, "r") as stream:
                                try:
                                    run_data = yaml.safe_load(stream)
                                except yaml.YAMLError as exc:
                                    print(exc)
        
                            # only care about the overhead
                            # if there is data available
                            if run_data is not None:
                                run_data = run_data["async_persistentpt2pt"]["over_full"]
                                for key, val in run_data.items():
                                    # key is bufsize, val is overhead measured in s
                                    df.loc[i] = [nprocs,mode, calctime, warmup, cycles, key, val]
                                    i += 1
                        except ValueError:
                            print("could not read %s" % entry.name)
    return df


def print_stat(data, key, buf_size, base_val):
    # only print stats for maximum comp time
    select_df = data[(data['nprocs'] == process_count_to_use) &(data['cycles'] == cycles_to_use) & (data['warmup'] == warmup_to_use) & (data['mode'] == key) & (
                data['calctime'] == data['calctime'].max()) & (data['buflen'] == buf_size)]
    #print(select_df.head)

    avg = select_df['overhead'].median()
    measurement_count = len(select_df.index)
    if measurement_count > 0:

        if base_val == -1:
            print("%s: %d measurements, 0%% improvement (median)" % (names[key], measurement_count))
        else:
            improvement = 100 * (base_val - avg) / base_val
            print("%s: %d measurements, %f%% improvement (median)" % (names[key], measurement_count, improvement))
    else:
        print("%s: 0 measurements" % names[key])

    return measurement_count, avg


def print_statistics(data):
    for buf_size in buffer_sizes:
        print("")
        print(buf_size)
        measurement_count, avg = print_stat(data, NORMAL, buf_size, -1)
        measurement_count, _ = print_stat(data, EAGER, buf_size, avg)
        measurement_count, _ = print_stat(data, RENDEVOUZ1, buf_size, avg)
        measurement_count, _ = print_stat(data, RENDEVOUZ2, buf_size, avg)


def main():
    parser = argparse.ArgumentParser(description='Generates Visualization')
    parser.add_argument('--cache',
                        help='use the content of the cache file named %s. do NOT set this argument, if you want to re-write the cache' % CACHE_FILE)
    args = parser.parse_args()
    if args.cache:
        data = pd.read_csv(CACHE_FILE, index_col=0)
    else:
        data = read_data()
        data.to_csv(CACHE_FILE)
    print_statistics(data)

    print("generating plots ...")

    get_scaling_plot(data,buffer_sizes_scaling,comptime_for_barplots,"overhead_scaling",scaling=False)
    get_violin_plot(data, buffer_sizes, comptime_for_barplots, "overhead_violins",scaling=False)
    get_violin_plot(data, buffer_sizes, comptime_for_barplots, "overhead_violins_scaled", scaling=True)

    print("done")


if __name__ == "__main__":
    main()
