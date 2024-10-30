import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def create_ycsb_throughput_plot(font_size=10):
    # Read data from CSV
    df = pd.read_csv('YCSB.csv')

    # Data preparation
    index_types = df['index_type'].unique()
    workloads = ['A:write-heavy', 'B:read-heavy', 'C:read-only']

    # Set up the plot
    fig, ax = plt.subplots(figsize=(7.5, 4))  # Adjusted for two-column width
    
    # Set width of bars and positions
    bar_width = 0.13
    r = np.arange(len(workloads))

    # Colors and patterns for different KV stores
    colors = ['#FF6347', '#4682B4', '#32CD32', '#FFD700', '#9370DB', '#20B2AA']
    patterns = ['///', '...', 'xxx', '\\\\\\', '+++', 'ooo']

    # Create grouped bars for each KV store
    for i, index_type in enumerate(index_types):
        index_data = df[df['index_type'] == index_type]
        offset = (i - len(index_types)/2 + 0.5) * bar_width
        bars = ax.bar(r + offset, index_data['throughput(KOPS)'], 
                      width=bar_width, color=colors[i], label=index_type,
                      edgecolor='black', hatch=patterns[i], linewidth=0.8)

        # Add value labels on top of each bar
        for bar in bars:
            height = bar.get_height()
            ax.annotate(f'{height}',
                        xy=(bar.get_x() + bar.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=font_size-2)

    # Add labels and title
    ax.set_xlabel('YCSB Workload', fontsize=font_size+1)
    ax.set_ylabel('Throughput (KOPS)', fontsize=font_size+1)
    # ax.set_title('KV Store Throughput Comparison for YCSB Workloads', fontsize=font_size+2)
    ax.set_xticks(r)
    ax.set_xticklabels(workloads, fontsize=font_size)
    
    # Place legend inside the plot
    ax.legend(fontsize=font_size-1, loc='upper right', bbox_to_anchor=(0.99, 0.99), ncol=2)

    # Add grid lines for better readability
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    # Adjust layout and display
    plt.tight_layout()

    # Save the figure
    plt.savefig('ycsb_throughput.pdf', bbox_inches='tight')
    plt.savefig('ycsb_throughput.png', dpi=300, bbox_inches='tight')

    plt.show()

# Call the function with default font size
create_ycsb_throughput_plot()

# To adjust font size, call with a different font size, e.g.:
# create_ycsb_throughput_plot(font_size=10)