import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def create_time_breakdown_plot(font_size=10):
    # Read data from CSV
    df = pd.read_csv('time_break_down.csv')

    # Data preparation
    index_types = df['index_type'].tolist()
    rocksdb_times = df['time for rocksdb'].tolist()
    learned_index_times = df['time for learned index'].tolist()
    loading_times = df['time for loading'].tolist()

    # Calculate total times for each bar
    total_times = [sum(x) for x in zip(rocksdb_times, learned_index_times, loading_times)]

    # Set up the plot with adjusted figure size
    fig, ax = plt.subplots(figsize=(6.5, 4))  # Width: 6.5 inches, Height: 4 inches

    # Set width of bars and positions
    bar_width = 0.5
    index = np.arange(len(index_types))

    # Colors for each operation
    colors = ['#FF6347', '#4682B4', '#32CD32']  # Tomato, Steel Blue, Lime Green
    patterns = ['///', '...', 'xxx']

    # Create stacked bars with reversed order
    p3 = ax.bar(index, loading_times, bar_width, color=colors[2], edgecolor='black', hatch=patterns[2])
    p2 = ax.bar(index, learned_index_times, bar_width, bottom=loading_times, color=colors[1], edgecolor='black', hatch=patterns[1])
    p1 = ax.bar(index, rocksdb_times, bar_width, bottom=[sum(x) for x in zip(loading_times, learned_index_times)], color=colors[0], edgecolor='black', hatch=patterns[0])
    
    # Add labels and title
    ax.set_ylabel('Time (microseconds)', fontsize=font_size)
    # ax.set_title('Time Breakdown for Learned Index and RocksDB+', fontsize=font_size+2)
    ax.set_xticks(index)
    
    # Change x-axis labels
    new_labels = ["LearnedKV_key_in_LI", "LearnedKV_key_in_R", "RocksDB+"]
    ax.set_xticklabels(new_labels, fontsize=font_size)  # Horizontal labels
    
    ax.legend((p1[0], p2[0], p3[0]), ('query_R', 'query_LI', 'load_KV'), fontsize=font_size-1, loc='upper left', bbox_to_anchor=(1, 1))

    # Add value labels on top of each bar segment
    def add_labels(rects):
        for rect in rects:
            height = rect.get_height()
            if height > 0:
                ax.annotate(f'{height:.2f}',
                            xy=(rect.get_x() + rect.get_width() / 2, rect.get_y() + height / 2),
                            xytext=(0, 0),
                            textcoords="offset points",
                            ha='center', va='center', fontsize=font_size-2)

    # add_labels(p1)
    # add_labels(p2)
    # add_labels(p3)

    # Add sum labels on top of each stacked bar
    for i, total in enumerate(total_times):
        ax.annotate(f'{total:.2f}',
                    xy=(index[i], total),
                    xytext=(0, 5),  # 5 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=font_size)

    # Adjust y-axis limits to prevent overlapping
    y_max = max(total_times)
    ax.set_ylim(0, y_max * 1.1)  # Set the upper limit to 110% of the maximum total time

    # Adjust layout and display
    plt.tight_layout()

    # Save the figure
    plt.savefig('time_breakdown.pdf', bbox_inches='tight')
    plt.savefig('time_breakdown.png', dpi=300, bbox_inches='tight')

    plt.show()

# Call the function with default font size
create_time_breakdown_plot()

# To adjust font size, call with a different font size, e.g.:
# create_time_breakdown_plot(font_size=10)