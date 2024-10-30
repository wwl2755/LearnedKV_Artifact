import matplotlib.pyplot as plt
import numpy as np

def create_workload_performance_plot(font_size=12):
    # Data
    ratios = ['7:3', '5:5', '3:7', 'Write-only']
    index_types = ['LearnedKV', 'RocksDB']

    write_throughput = {
        'LearnedKV': [46.9861, 60.403, 39.8627, 52.7729],
        'RocksDB': [39.1291, 48.5893, 32.9987, 45.7966]
    }

    read_throughput = {
        'LearnedKV': [91.8553, 73.505, 50.016, 0],
        'RocksDB': [76.9837, 55.4029, 29.3283, 0]
    }

    # Set up the plot
    fig, ax = plt.subplots(figsize=(7.5, 4))

    # Set width of bars and positions
    bar_width = 0.2
    index = np.arange(len(ratios))
    r1 = index - 1.5 * bar_width
    r2 = index - 0.5 * bar_width
    r3 = index + 0.5 * bar_width
    r4 = index + 1.5 * bar_width

    # Colors for each index type (write and read)
    colors = {
        'LearnedKV': {'write': '#FF6347', 'read': '#FF8C69'},  # Tomato and Dark Salmon
        'RocksDB': {'write': '#4682B4', 'read': '#87CEFA'}  # Steel Blue and Light Sky Blue
    }

    # Textures for write and read
    textures = ['///', '...', 'xxx', '\\\\\\']

    # Create bars
    plt.bar(r1, write_throughput['LearnedKV'], color=colors['LearnedKV']['write'], hatch=textures[0], 
            width=bar_width, edgecolor='black', label='LearnedKV Write')
    plt.bar(r2, read_throughput['LearnedKV'], color=colors['LearnedKV']['read'], hatch=textures[1], 
            width=bar_width, edgecolor='black', label='LearnedKV Read')
    plt.bar(r3, write_throughput['RocksDB'], color=colors['RocksDB']['write'], hatch=textures[2], 
            width=bar_width, edgecolor='black', label='RocksDB Write')
    plt.bar(r4, read_throughput['RocksDB'], color=colors['RocksDB']['read'], hatch=textures[3], 
            width=bar_width, edgecolor='black', label='RocksDB Read')

    # Add labels and title
    plt.xlabel('Read:Write Ratio', fontsize=font_size)
    plt.ylabel('Throughput (KOPS)', fontsize=font_size)
    # plt.title('P3 Throughput Comparison Across Different Workloads', fontsize=font_size+2)
    plt.xticks(index, ratios, fontsize=font_size)
    plt.legend(fontsize=font_size-1)

    # Add value labels on top of each bar
    def add_labels(rects):
        for rect in rects:
            height = rect.get_height()
            if height > 0:  # Only add label if the height is greater than 0
                ax.annotate(f'{height:.1f}',
                            xy=(rect.get_x() + rect.get_width() / 2, height),
                            xytext=(0, 3),  # 3 points vertical offset
                            textcoords="offset points",
                            ha='center', va='bottom', fontsize=font_size-2)

    for rect in ax.patches:
        add_labels([rect])

    # Adjust layout and display
    plt.tight_layout()

    # Save the figure
    plt.savefig('readwrite_ratio.pdf', bbox_inches='tight')
    plt.savefig('readwrite_ratio.png', dpi=300, bbox_inches='tight')

    plt.show()

# Call the function with default font size
create_workload_performance_plot()

# To adjust font size, call with a different font size, e.g.:
# create_workload_performance_plot(font_size=12)