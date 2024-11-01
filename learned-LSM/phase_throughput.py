import matplotlib.pyplot as plt
import numpy as np

def create_throughput_plot(font_size=12):
    # Data
    index_types = ['LearnedKV', 'RocksDB']
    write_phases = ['P0', 'P1', 'P2', 'P3']
    read_phases = ['P1', 'P2', 'P3']  # Omitting P0 for read throughput

    # Read data from CSV
    import pandas as pd
    df = pd.read_csv('out.csv')
    
    # Extract throughput data for each index type
    learned_kv_data = df[df['index_type'] == 'LearnedKV'].iloc[0]
    rocksdb_data = df[df['index_type'] == 'RocksDB'].iloc[0]
    
    # Write throughput data
    write_throughput = [
        [learned_kv_data['P0 throughput'], learned_kv_data['P1 write throughput'], 
         learned_kv_data['P2 write throughput'], learned_kv_data['P3 write throughput']],  # LearnedKV
        [rocksdb_data['P0 throughput'], rocksdb_data['P1 write throughput'],
         rocksdb_data['P2 write throughput'], rocksdb_data['P3 write throughput']]  # RocksDB+
    ]
    
    # Read throughput data 
    read_throughput = [
        [learned_kv_data['P1 read throughput'], learned_kv_data['P2 read throughput'],
         learned_kv_data['P3 read throughput']],  # LearnedKV
        [rocksdb_data['P1 read throughput'], rocksdb_data['P2 read throughput'],
         rocksdb_data['P3 read throughput']]  # RocksDB+
    ]

    # Set up the plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7.5, 7))
    plt.subplots_adjust(top=0.95, bottom=0.1, hspace=0.3)  # Adjust top margin

    # Colors for each index type
    # colors = ['#FF4500', '#4682B4']  # Orange Red, Steel Blue
    # colors = ['#FFB3BA', '#BAFFC9']  # Light Pink, Light Green
    colors = ['#FF6347', '#4682B4']

    # Textures for each index type
    # textures = ['///', '\\\\\\']
    textures = ['///', '...']

    # Function to add value labels on top of each bar
    def add_labels(ax, rects):
        for rect in rects:
            height = rect.get_height()
            ax.annotate(f'{height:.2f}',
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=font_size)

    # Function to plot bars and set y-axis limit
    def plot_bars(ax, data, phases):
        x = np.arange(len(phases))
        width = 0.35
        max_height = 0

        for i, index_type in enumerate(index_types):
            rects = ax.bar(x + i*width, data[i], width, label=index_type, 
                           color=colors[i], hatch=textures[i], edgecolor='black')
            add_labels(ax, rects)
            max_height = max(max_height, max(data[i]))

        # Set y-axis limit with 10% padding
        ax.set_ylim(0, max_height * 1.1)

        ax.set_xticks(x + width / 2)
        ax.set_xticklabels(phases, fontsize=font_size)
        ax.legend(fontsize=font_size)
        ax.tick_params(axis='y', labelsize=font_size)

    # Write Throughput subplot
    plot_bars(ax1, write_throughput, write_phases)
    ax1.set_ylabel('Write Throughput (KOPS)', fontsize=font_size)
    ax1.set_title('Write Throughput by Phase', fontsize=font_size+2)

    # Read Throughput subplot
    plot_bars(ax2, read_throughput, read_phases)
    ax2.set_xlabel('Phases', fontsize=font_size)
    ax2.set_ylabel('Read Throughput (KOPS)', fontsize=font_size)
    ax2.set_title('Read Throughput by Phase', fontsize=font_size+2)

    plt.tight_layout()

    # Save the figure
    # plt.savefig('phase_throughput.pdf', bbox_inches='tight')
    plt.savefig('phase_throughput.png', dpi=300, bbox_inches='tight')

    plt.show()

# Call the function with default font size
create_throughput_plot()

# To adjust font size, call with a different font size, e.g.:
# create_throughput_plot(font_size=12)