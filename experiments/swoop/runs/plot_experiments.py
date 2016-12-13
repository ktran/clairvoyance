#!/usr/bin/env python
import matplotlib
matplotlib.use('Agg')

import jobs, plotting_utils
import glob, getopt, sys, os, re, csv
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib import gridspec
import seaborn as sns

plt.style.use('grayscale')
sns.set_style("whitegrid")
sns.despine(left=True)

# the execute time (DVFS)
EXECUTE_TIME='execute_time'

# the unroll count
UNROLL_COUNT='unroll'

# the cycles (perf)
CYCLES='cycles'

# the benchmark name
BENCHMARK='benchmark'

# the type (swoop, unroll, original)
TYPE='type'

# the hoist type (consv, spec, ..)
HOIST_TYPE='hoist'

# the number of indirections, if applicable
INDIRECTION='indirection'

# the number of indirections, if applicable
INPUT='input'

# the total runtime (perf)
TOTAL_PERF_TIME='total'

# the frequency that was used
FREQUENCY='frequency'

SWOOP_TYPE='swoop'
UNROLL_TYPE='unroll'
ORIGINAL_TYPE='original'
SCHEDULER_TYPE='sched'

NORMALIZED_PREFIX='norm_'

def usage():
    print 'Usage:', sys.argv[0], '-i (results.csv)', '[--plot-version-comparison, --plot-best, --print-best]','[--inputset (input version name, e.g. test1/train1/ref1/, default ref1)]'

def refine_plot(plot, xlabel, ylabel, title, x_minmax, y_minmax, legend_title):
    # set title and x, y labels
    fontsize_label = 13
    fontsize_ticks = 10

    plot.set_xlabel(xlabel, fontsize = fontsize_label)
    plot.set_ylabel(ylabel, fontsize = fontsize_label)
    plot.set_title(title, fontsize = fontsize_label, y=1.08)

    # determine the min/max values for x,y
    if (x_minmax[0] != x_minmax[1]):
        plt.xlim(x_minmax[0], x_minmax[1])

    if (y_minmax[0] != y_minmax[1]):
        plt.ylim(y_minmax[0], y_minmax[1])

    plot.spines["top"].set_visible(False)
    plot.spines["bottom"].set_visible(False)
    plot.spines["right"].set_visible(False)
    plot.spines["left"].set_visible(False)

    # Remove right unnecessary tick axices
    plot.get_xaxis().tick_bottom()
    plot.get_yaxis().tick_left()

    # Adjust font sizes
    plt.yticks(fontsize=fontsize_ticks)
    plt.xticks(fontsize=fontsize_ticks)

    # Align tick labels
    labels = [label.get_text() for label in plot.xaxis.get_majorticklabels()]
    plot.set_xticklabels(labels, ha='right')

    # Put legend outside of graph
    plot.legend(loc='center left', bbox_to_anchor=(1, 0.5), title=legend_title)

    return plot

def normalize(reference, to_normalize, col_to_normalize, on=[BENCHMARK]):
    merged = pd.merge(to_normalize, reference, how='left', on=on, suffixes=('', '_ref'))
    for col in col_to_normalize:
        merged[col] =  merged[col] / merged[col + '_ref']

    normalized = merged[list(to_normalize.keys())]
    return normalized

def plot(data, type_to_plot, column_to_plot, frequency_to_plot, title, xlabel, ylabel, outfile, x_minmax = [0, 0], y_minmax = [0, 0]):
    relevant_data = data[[column_to_plot, UNROLL_COUNT, BENCHMARK, TYPE, HOIST_TYPE, INDIRECTION, FREQUENCY]]
    relevant_data = relevant_data[relevant_data[FREQUENCY] == frequency_to_plot]
    reference_data = relevant_data[(relevant_data[TYPE] == UNROLL_TYPE) & (relevant_data[UNROLL_COUNT] == 1)]
    if type_to_plot in jobs.swoop_hoist_types:
        swoop_data = relevant_data[(relevant_data[TYPE] == SWOOP_TYPE) & (relevant_data[HOIST_TYPE] == type_to_plot)]
    elif type_to_plot == UNROLL_TYPE:
        swoop_data = relevant_data[(relevant_data[TYPE] == UNROLL_TYPE)]
    else:
        print 'Type not recognized: ', type_to_plot
        sys.exit(-1)

    columns_to_normalize = [column_to_plot]
    norm = normalize(reference_data, swoop_data, columns_to_normalize)
    norm = norm[[INDIRECTION, BENCHMARK, UNROLL_COUNT, column_to_plot]]

    # for each unroll count, find the indirection that was fastest (min):
    plot_data = norm.loc[norm.groupby([BENCHMARK, UNROLL_COUNT])[column_to_plot].idxmin()]

    # drop indirections for plotting
    plot_data.drop(INDIRECTION, axis=1, inplace=True)

    # set benchmark name to be the index, and make each unroll count a column with the runtime as the content
    plot = plot_data.pivot(index=BENCHMARK, columns=UNROLL_COUNT, values=column_to_plot).plot(kind='bar', rot=30)

    # some general refinement of the plot, to make it prettier
    plot = refine_plot(plot, xlabel, ylabel, title, x_minmax, y_minmax, 'Unroll Count')

    fig = plot.get_figure()

    # use every free whitespace available
    fig.tight_layout(pad=0.1)

    fig.savefig(outfile, bbox_inches="tight")

def plot_comparison(data, types_to_compare, input_name, column_to_plot, frequency, title, xlabel, ylabel, outfile, x_minmax = [0, 0], y_minmax = [0, 0]):
    relevant_data = data[[column_to_plot, INPUT, UNROLL_COUNT, BENCHMARK, TYPE, HOIST_TYPE, INDIRECTION, FREQUENCY, SCHEDULER_TYPE]]
    relevant_data = relevant_data[(relevant_data[FREQUENCY] == frequency) & (relevant_data[INPUT] == input_name)]
    reference_data = relevant_data[(relevant_data[TYPE] == ORIGINAL_TYPE) & (relevant_data[SCHEDULER_TYPE] == 'default')]

    swoop_data = relevant_data[(relevant_data[TYPE] == SWOOP_TYPE) &
                               (relevant_data[HOIST_TYPE].isin(types_to_compare)) |
                               (relevant_data[TYPE].isin(types_to_compare))]

    columns_to_normalize = [column_to_plot]
    norm = normalize(reference_data, swoop_data, columns_to_normalize)
    norm = norm[[INDIRECTION, BENCHMARK, UNROLL_COUNT, TYPE, HOIST_TYPE, column_to_plot]]

    plot_data = norm.loc[norm.groupby([BENCHMARK, TYPE, HOIST_TYPE])[column_to_plot].idxmin()]
    
    plotting_utils.print_full(plot_data)
    
    # drop indirections for plotting
    plot_data.drop([INDIRECTION, UNROLL_COUNT, TYPE], axis=1, inplace=True)

    # set benchmark name to be the index, and make each unroll count a column with the runtime as the content
    reordered = plot_data.pivot(index=BENCHMARK, columns=HOIST_TYPE, values=column_to_plot)
    common_elements = [x for x in types_to_compare if x in reordered.columns.values]
    reordered = reordered[common_elements]

    plotting_utils.replace_swoop_names(reordered)
    if reordered.empty:
        print 'No data to plot'
        sys.exit(1)

    plot = reordered.plot(kind='bar', rot=30)
    print plot

    # some general refinement of the plot, to make it prettier
    plot = refine_plot(plot, xlabel, ylabel, title, x_minmax, y_minmax, '')

    plot.legend(loc='upper center', bbox_to_anchor=(0.5, 1.25),
              ncol=6, fancybox=True)


    fig = plot.get_figure()
    fig.set_size_inches(8.5,2.5)

    # use every free whitespace available
    fig.tight_layout(pad=0.1)

    fig.savefig(outfile, bbox_inches="tight")

def print_best_combination(data, input_name):
    types_to_compare = ['spec','multispec','consv','specsafe','multispecsafe']
    frequency = 'default'
    column_to_plot = TOTAL_PERF_TIME

    relevant_data = data[[column_to_plot, INPUT, UNROLL_COUNT, BENCHMARK, TYPE, HOIST_TYPE, INDIRECTION, FREQUENCY, SCHEDULER_TYPE]]
    relevant_data = relevant_data[(relevant_data[FREQUENCY] == frequency) & (relevant_data[INPUT] == input_name)]
    reference_data = relevant_data[(relevant_data[TYPE] == ORIGINAL_TYPE) & (relevant_data[SCHEDULER_TYPE] == 'default')]

    swoop_data = relevant_data[(relevant_data[TYPE] == SWOOP_TYPE) &
                               (relevant_data[HOIST_TYPE].isin(types_to_compare)) |
                               (relevant_data[TYPE].isin(types_to_compare)) &
                               ((relevant_data[TYPE] != ORIGINAL_TYPE) | (relevant_data[SCHEDULER_TYPE] != 'default'))]

    columns_to_normalize = [column_to_plot]
    norm = normalize(reference_data, swoop_data, columns_to_normalize)
    norm = norm[[INDIRECTION, BENCHMARK, UNROLL_COUNT, TYPE, HOIST_TYPE, column_to_plot]]


    plot_data = norm.loc[norm.groupby([BENCHMARK])[column_to_plot].idxmin()]

    if plot_data.empty:
        print 'No data to analyze.'
        sys.exit(1)

    print plot_data

def plot_runtime_comparison(data, input_name):
    types_to_compare = ['consv','specsafe','spec','multispecsafe','multispec']
    frequency = 'default'

    plot_data = TOTAL_PERF_TIME
    outfile = '-'.join(['_'.join(types_to_compare), plot_data, str(frequency)]) + '.pdf'
    plot_comparison(data, types_to_compare, input_name, plot_data, frequency, '', '', 'Normalized Runtime', outfile)

if __name__ == '__main__':
    in_file = None
    option = -1
    print_best = False
    plot_best = False
    plot_version_comparison = False
    counter = None
    input_version = 'ref1'
    energy_file = None

    try:
        opts, args = getopt.getopt(sys.argv[1:], "i:", ['input=','show-best','inputset=','print-best','plot-best','plot-version-comparison'])
    except getopt.GetoptError, e:
        print e
        usage()
        sys.exit()
    for o, a in opts:
        print 'opts', o, a
        if o == '-h' or o == '--help':
            usage()
            sys.exit()
        if o == '--input' or o == '-i':
            in_file = a
        if o == '--plot' or o == '-p':
            option = int(a)
        if o == '--print-best':
            print_best = True
        if o == '--plot-best':
            plot_best = True
        if o == '--plot-version-comparison':
            plot_version_comparison = True
        if o == '--inputset':
            input_version = a + '1' # for artifact evaluation

    if args:
        usage()
        sys.exit(-1)

    if not in_file:
        usage()
        sys.exit(-1) 

    if not plot_best and not plot_version_comparison and not print_best:
        usage()
        sys.exit(-1)

    data = pd.read_csv(in_file, header=0);

    if print_best:
        print_best_combination(data, input_version)
        sys.exit(0)
    if plot_version_comparison:
        plot_runtime_comparison(data, input_version)
    elif plot_best:
        fontsize_label = 13
        fontsize_ticks = 10

        title = ''
        xlabel = 'Benchmark'
        ylabel = 'Normalized Runtime'
        outfile = 'runtime-plot.pdf'
        column_to_plot = TOTAL_PERF_TIME
        frequency_to_plot = 'default'
        types_to_compare = ('multispec','spec','multispecsafe','consv','specsafe','dae','original')

        relevant_data = data[[column_to_plot, INPUT, UNROLL_COUNT, BENCHMARK, TYPE, HOIST_TYPE, INDIRECTION, FREQUENCY, SCHEDULER_TYPE]]
        relevant_data = relevant_data[(relevant_data[FREQUENCY] == frequency_to_plot) & (relevant_data[INPUT] == input_version)]
        reference_data = relevant_data[(relevant_data[TYPE] == ORIGINAL_TYPE) & (relevant_data[SCHEDULER_TYPE] == 'default')]

        swoop_data = relevant_data[(relevant_data[TYPE] == SWOOP_TYPE) &
                                   (relevant_data[HOIST_TYPE].isin(types_to_compare)) |
                                   (relevant_data[TYPE].isin(types_to_compare)) &
                                   ((relevant_data[TYPE] != ORIGINAL_TYPE) | (relevant_data[SCHEDULER_TYPE] != 'default'))]

        columns_to_normalize = [column_to_plot]
        norm = normalize(reference_data, swoop_data, columns_to_normalize)
        norm = norm[[INDIRECTION, BENCHMARK, UNROLL_COUNT, TYPE, column_to_plot]]

        plot_data = norm.loc[norm.groupby([BENCHMARK, TYPE])[column_to_plot].idxmin()]

        # drop indirections for plotting
        plot_data.drop([INDIRECTION, UNROLL_COUNT], axis=1, inplace=True)

        # set benchmark name to be the index, and make each unroll count a column with the runtime as the content
        fig = plt.figure(figsize=(8.5, 2))
        gs = gridspec.GridSpec(1, 4, width_ratios=[7, 1, 8, 1])

        ax = plt.subplot(gs[0])
        ax2 = plt.subplot(gs[1])
        ax3 = plt.subplot(gs[2])
        ax4 = plt.subplot(gs[3])

        ylim = 1.2

        memory_bound = ('CG','429.mcf','470.lbm','471.omnetpp','433.milc','450.soplex','473.astar','462.libquantum')
        plot_data_mem = plot_data[plot_data[BENCHMARK].isin(memory_bound)]
        plot_data_com = plot_data[plot_data[BENCHMARK].isin(memory_bound) == 0]

        if plot_data_mem.empty and plot_data_com.empty:
            print 'No data to plot.'
            sys.exit(1)

        # replace names
        rename_dict = {'original':'LLVM-SCHED','dae':'DAE','swoop':'CLAIRVOYANCE'}
        reordered_mem = plot_data_mem.pivot(index=BENCHMARK, columns=TYPE, values=column_to_plot)
        reordered_mem.rename(columns = rename_dict, inplace = True)

        reordered_com = plot_data_com.pivot(index=BENCHMARK, columns=TYPE, values=column_to_plot)
        reordered_com.rename(columns = rename_dict, inplace = True)

        if not reordered_mem.empty:
            plot = reordered_mem.plot(kind='bar', rot=30, ax=ax)
            plotting_utils.align_labels(plot)
            ax.set_xlabel('')
            ax.set_ylabel(ylabel, fontsize=fontsize_label)
            ax.tick_params(labelsize=fontsize_ticks,bottom='off')
            ax.legend(fontsize=fontsize_ticks)
            ax.set_ylim(0, ylim)
            ax.legend_.remove()
            handles, labels = ax.get_legend_handles_labels()

            if 'DAE' in reordered_mem.index:
                # mark DAE outliers with number
                outliers = reordered_mem[reordered_mem['DAE'] > ylim]

                for index, row in outliers.iterrows():
                    ax.annotate("{0:.2f}".format(row['DAE']), xy=(1, 1.2), xytext=(3, 1.08), fontsize=fontsize_ticks)

            print reordered_mem

            # some general refinement of the plot, to make it prettier
            bars = reordered_mem.apply(plotting_utils.geomean, axis=0, raw=True).plot(kind='bar', rot=30, ax=ax2, color=matplotlib.rcParams['axes.color_cycle'])
            print reordered_mem.apply(plotting_utils.geomean, axis=0, raw=True)
            plotting_utils.align_labels(bars)
            ax2.tick_params(labelsize=fontsize_ticks,labelbottom='off')
            ax2.set_xlabel('GeoMean', fontsize=fontsize_label)

            fig.legend(handles, labels, loc = 'upper center',bbox_to_anchor=(0.5, 1.25), ncol=16 )

        if not reordered_com.empty:
            # compute-bond
            plot = reordered_com.plot(kind='bar', rot=30, ax=ax3)
            plotting_utils.align_labels(plot)
            ax3.set_xlabel('')
            ax3.tick_params(labelsize=fontsize_ticks,bottom='off')
            ax3.set_ylim(0, ylim)

            handles, labels = ax3.get_legend_handles_labels()
            ax3.legend_.remove()

            print reordered_com
            if 'DAE' in reordered_com.index:
                outliers = reordered_com[reordered_com['DAE'] > ylim]

                for index, row in outliers.iterrows():
                    bm_array = reordered_com.index.values
                    x = np.where(bm_array == index)[0][0]
                    ax3.annotate("{0:.2f}".format(row['DAE']), xy=(0, 0), xytext=(x-1.13, 1.08), fontsize=fontsize_ticks)

            # some general refinement of the plot, to make it prettier
            bars = reordered_com.apply(plotting_utils.geomean, axis=0, raw=True).plot(kind='bar', rot=30, ax=ax4, color=mpl.rcParams['axes.color_cycle'])
            print reordered_com.apply(plotting_utils.geomean, axis=0, raw=True)
            plotting_utils.align_labels(bars)
            ax4.tick_params(labelsize=fontsize_ticks,labelbottom='off')
            ax4.set_xlabel('GeoMean', fontsize=fontsize_label)

            #refine_plot(plot, xlabel, ylabel, title, [0, 0], [0, 1.2], 'Version')
            # use every free whitespace available
            fig.tight_layout(pad=0.1)
            if reordered_mem.empty:
                fig.legend(handles, labels, loc = 'upper center',bbox_to_anchor=(0.5, 1.25), ncol=16 )

        fig.savefig(outfile, bbox_inches="tight")
