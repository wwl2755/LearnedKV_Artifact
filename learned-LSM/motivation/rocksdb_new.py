import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read the CSV file
df = pd.read_csv('lsm_performance_results.csv')

# Create a figure with two side-by-side subplots
# Adjust figure size to fit a single column in a double-columned technical report
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7, 3.5))
fig.suptitle('LSM Size Performance Comparison', fontsize=10)

# Define patterns for bar textures
patterns = ['x','x','x']

# Function to plot bars and lines
def plot_bars_and_lines(ax, data, title, ylabel):
    bars = ax.bar(range(len(data)), data, align='center', alpha=0.8, 
                  color='lightblue', edgecolor='black', linewidth=1)
    
    # Add textures to bars
    for bar, pattern in zip(bars, patterns):
        bar.set_hatch(pattern)
    
    # Add value labels on top of bars
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{int(height):,}', ha='center', va='bottom', fontsize=8)
    
    # Add line connecting midpoints of bar tops
    midpoints = [bar.get_height() for bar in bars]
    ax.plot(range(len(data)), midpoints, color='red', marker='o', linestyle='-', linewidth=2, markersize=4)
    
    ax.set_xlabel('Initial Data Size', fontsize=8)
    ax.set_ylabel('Time Cost (ms)', fontsize=8)
    ax.set_title(title, fontsize=9)
    ax.grid(True, axis='y', linestyle='--', alpha=0.7)
    ax.set_xticks(range(len(data)))
    ax.set_xticklabels([f'{int(x):,}' for x in df['Initial Keys']], ha='center', fontsize=8)
    ax.tick_params(axis='y', labelsize=8)

# Plot Read Time
plot_bars_and_lines(ax1, df['Read Time (ms)'], 'Read Time Cost', 'Read Time (ms)')

# Plot Write Time
plot_bars_and_lines(ax2, df['Write Time (ms)'], 'Write Time Cost', 'Write Time (ms)')

# Adjust layout and save the plot
plt.tight_layout()
plt.savefig('lsm_performance_runtime_plot.png', dpi=300, bbox_inches='tight')
plt.savefig('lsm_performance_runtime_plot.pdf')
plt.show()

# Print the data as a table
print("LSM Size Performance Comparison:")
print("--------------------------------")
print(df.to_string(index=False))