import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def create_scan_throughput_plot(font_size=12):
    # Read data from CSV
    df = pd.read_csv('scan.csv')

    # Data preparation
    workloads = df['workload'].unique()
    index_types = df['index_type'].unique()

    # Set up the plot
    fig, ax = plt.subplots(figsize=(7.5, 4))

    # Set width of bars and positions
    bar_width = 0.25
    index = np.arange(len(workloads))
    r1 = index - bar_width/2
    r2 = index + bar_width/2

    # Colors for each index type
    colors = {
        'LearnedKV': '#FF6347',  # Tomato
        'RocksDB': '#4682B4'     # Steel Blue
    }

    # Textures for each index type
    textures = ['///', '...']

    # Create bars
    for i, index_type in enumerate(index_types):
        data = df[df['index_type'] == index_type]['scan_throughput']
        plt.bar(r1 if i == 0 else r2, data, color=colors[index_type], hatch=textures[i], 
                width=bar_width, edgecolor='black', label=index_type)

    # Add labels and title
    # plt.xlabel('Workload', fontsize=font_size)
    plt.ylabel('Scan Throughput (KOPS)', fontsize=font_size)
    # plt.title('Scan Throughput Comparison Across Different Workloads', fontsize=font_size+2)
    plt.xticks(index, [w.replace('_', ' ').title() for w in workloads], fontsize=font_size)
    plt.legend(fontsize=font_size-1)

    # Add value labels on top of each bar
    def add_labels(rects):
        for rect in rects:
            height = rect.get_height()
            ax.annotate(f'{height:.2f}',
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=font_size-2)

    for rect in ax.patches:
        add_labels([rect])

    # Adjust layout and display
    plt.tight_layout()

    # Save the figure
    plt.savefig('scan_throughput.pdf', bbox_inches='tight')
    plt.savefig('scan_throughput.png', dpi=300, bbox_inches='tight')

    plt.show()

# Call the function with default font size
create_scan_throughput_plot()

# To adjust font size, call with a different font size, e.g.:
# create_scan_throughput_plot(font_size=12)