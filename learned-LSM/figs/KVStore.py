import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def create_kvstore_throughput_plot(font_size=12):
    # Read data from CSV
    df = pd.read_csv('KVStore.csv')

    # Data preparation
    index_types = df['index_type']
    write_throughput = df['write_throughput']
    read_throughput = df['read_throughput']

    # Set up the plot
    fig, ax = plt.subplots(figsize=(14, 6))  # Increased width to accommodate horizontal labels

    # Set width of bars and positions
    bar_width = 0.35
    index = np.arange(len(index_types))
    r1 = index - bar_width/2
    r2 = index + bar_width/2

    # Colors for write and read
    colors = {
        'write': '#FF6347',  # Tomato
        'read': '#4682B4'    # Steel Blue
    }

    # Textures for write and read
    textures = ['///', '...']

    # Create bars
    plt.bar(r1, write_throughput, color=colors['write'], hatch=textures[0], 
            width=bar_width, edgecolor='black', label='Write Throughput')
    plt.bar(r2, read_throughput, color=colors['read'], hatch=textures[1], 
            width=bar_width, edgecolor='black', label='Read Throughput')

    # Add labels and title
    plt.xlabel('KV Store Type', fontsize=font_size)
    plt.ylabel('Throughput (KOPS)', fontsize=font_size)
    plt.title('KV Store Write and Read Throughput Comparison', fontsize=font_size+2)
    plt.xticks(index, index_types, fontsize=font_size)  # Removed rotation
    plt.legend(fontsize=font_size-1)

    # Add value labels on top of each bar
    def add_labels(rects):
        for rect in rects:
            height = rect.get_height()
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
    plt.savefig('kvstore_throughput.pdf', bbox_inches='tight')
    plt.savefig('kvstore_throughput.png', dpi=300, bbox_inches='tight')

    plt.show()

# Call the function with default font size
create_kvstore_throughput_plot()

# To adjust font size, call with a different font size, e.g.:
# create_kvstore_throughput_plot(font_size=12)