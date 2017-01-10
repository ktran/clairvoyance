# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
import matplotlib.pyplot as plt
import warnings
import plot_experiments as swoopplot
import pandas as pd


geomean = lambda n: reduce(lambda x,y: x*y, n) ** (1.0 / len(n))

def replace_swoop_names(df):
        replace_dict = {'consv':'consv', 'spec':'spec','specsafe':'spec-safe','multispecsafe':'multi-spec-safe','multispec':'multi-spec'}
        df.rename(columns = replace_dict, inplace = True)


def align_xaxis(ax2, ax1, x1, x2):
        "maps xlim of ax2 to x1 and x2 in ax1"
        (x1, _), (x2, _) = ax2.transData.inverted().transform(ax1.transData.transform([[x1, 0], [x2, 0]]))
        xs, xe = ax2.get_xlim()
        k, b = np.polyfit([x1, x2], [xs, xe], 1)
        ax2.set_xlim(xs*k+b, xe*k+b)


def print_full(x):
        pd.set_option('display.max_rows', len(x))
        print(x)
        pd.reset_option('display.max_rows')

def align_labels(plot):
        # Align tick labels
        labels = [label.get_text() for label in plot.xaxis.get_majorticklabels()]
        plot.set_xticklabels(labels, ha='right')


def refine_plot(plot, xlabel, ylabel, title, x_minmax, y_minmax):
    # set title and x, y labels

    fontsize_label = 23
    fontsize_ticks = 20

    plot.set_xlabel(xlabel, fontsize=fontsize_label)
    plot.set_ylabel(ylabel, fontsize=fontsize_label)
    plot.set_title(title, fontsize=fontsize_label)

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

    plot.tick_params(labelsize=fontsize_ticks)
    plot.legend(loc='upper center', bbox_to_anchor=(0.5, 1.15),
                ncol=3, fancybox=True, fontsize=fontsize_ticks)


    plt.figure(figsize=(12, 3))

    align_labels(plot)


def get_axes(gs, row, column, num_rows, num_column):
    if num_rows == 1 or num_column == 1:
        return gs[row + column]
    return gs[row, column]

def plot_all_per_version(benchmark, data, data_column_label, data_row_label, column_values, row_values, val_to_plot, title, xlabel, ylabel, outfile):
    num_versions = len(row_values)
    num_row_values = len(row_values)
    num_column_values = len(column_values)
    f, axes = plt.subplots(nrows=num_row_values, ncols=num_column_values, sharex=True)

    f.text(0.5, 0.04, xlabel, ha='center')
    f.text(0.04, 0.5, ylabel, va='center', rotation='vertical')

    for col_index in range(len(column_values)):
        if len(column_values) > 1:
            col_element = column_values[col_index]
            col_element_data = data[data[data_column_label] == col_element]
        else:
            col_element_data = data
        for row_index in range(num_row_values):
            swoop_type = row_values[row_index]
            ax = get_axes(axes, row_index, col_index, num_versions, num_column_values)
            type_data = col_element_data[col_element_data[data_row_label] == swoop_type]

            if type_data.empty:
                warnings.warn('Dataframe is empty for ' + benchmark + ' (' + swoop_type + '). Ignoring.')
                continue

            print type_data.pivot(index=swoopplot.UNROLL_COUNT, columns=swoopplot.INDIRECTION, values=val_to_plot)
            plot = type_data.pivot(index=swoopplot.UNROLL_COUNT, columns=swoopplot.INDIRECTION, values=val_to_plot).plot(kind='bar', ax=ax, rot=0)
            refine_plot(plot, '', '', swoop_type, [0,0], [0,0])
            f.add_subplot(ax)

            # keep only last plot's legend (one is enough)
            if row_index == num_versions - 1:
                ax.legend(title='Indirection Count', loc='upper center', bbox_to_anchor=(0.5, -0.4),
                          fancybox=True, shadow=True, ncol=5)

            else:
                ax.legend_.remove()


    # use every free whitespace available
    f.suptitle(title, fontsize=18)
    f.savefig(outfile, bbox_inches="tight")
