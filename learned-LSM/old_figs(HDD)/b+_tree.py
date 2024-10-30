import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read the data
data = pd.read_csv('b+_tree.csv')

# Set up the plot
plt.rcParams.update({'font.size': 14})  # Set global font size
fig, ax = plt.subplots(figsize=(12, 6))

# Set the width of each bar and the positions of the bars
width = 0.35
x = np.arange(2)  # Two positions: one for read, one for write

# Create the bars
learnedkv_bars = ax.bar(x - width/2, data.loc[0, ['read_throughput', 'write_throughput']], width, label='LearnedKV', color='skyblue')
btree_bars = ax.bar(x + width/2, data.loc[1, ['read_throughput', 'write_throughput']], width, label='B+-tree', color='lightgreen')

# Customize the plot
ax.set_ylabel('Throughput (KOPS)', fontsize=14)
ax.set_title('Performance Comparison: LearnedKV vs B+-tree', fontsize=14)
ax.set_xticks(x)
ax.set_xticklabels(['Read Throughput', 'Write Throughput'], fontsize=14)
ax.legend(fontsize=14)

# Add value labels on top of each bar
for bars in [learnedkv_bars, btree_bars]:
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2, height,
                f'{height:.2f}', ha='center', va='bottom', fontsize=14)

# Adjust layout and display the plot
plt.tight_layout()
plt.show()

# Optionally, save the figure
# plt.savefig('index_performance_comparison.png', dpi=300, bbox_inches='tight')

plt.savefig('b+_tree.png')
plt.savefig('b+_tree.pdf')

