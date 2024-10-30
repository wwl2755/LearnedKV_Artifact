import pandas as pd
import matplotlib.pyplot as plt

# Read the CSV file into a DataFrame
data = pd.read_csv('bloom_filter.csv')

# Extract relevant data
read_throughputs = data['P3 read throughput'].values
write_throughputs = data['P3 write throughput'].values

# Positions for main groups and labels
group_positions = [1, 2]
group_labels = ['Read Throughput', 'Write Throughput']

# Adjusting the width of the bars
width = 0.2  # New width for the bars

# Adjusting font sizes
title_fontsize = 12
axis_title_fontsize = 12
tick_label_fontsize = 13
legend_fontsize = 10

# Calculate the ratios for read and write throughput
read_ratio = (read_throughputs[0] - read_throughputs[1]) / read_throughputs[1] * 100
write_ratio = (write_throughputs[0] - write_throughputs[1]) / write_throughputs[1] * 100
ratios = [read_ratio, write_ratio]

# Calculate the maximum value among all bars to adjust the y-axis limit
max_value = max(max(read_throughputs), max(write_throughputs))
y_limit = max_value + 0.1 * max_value  # Adding 10% of the maximum value as extra space


# Creating the bar chart with the adjusted width
plt.figure(figsize=(7.5, 4))

# Bars
bars_with_bf = plt.bar([group_positions[0] - width/2, group_positions[1] - width/2], [read_throughputs[0], write_throughputs[0]], width=width, label='With Bloom Filter', color='#FF6347', hatch='///', align='center',edgecolor='black')
bars_without_bf = plt.bar([group_positions[0] + width/2, group_positions[1] + width/2], [read_throughputs[1], write_throughputs[1]], width=width, label='Without Bloom Filter', color='#4682B4', hatch='...', align='center',edgecolor='black')

# Annotate bars with calculated ratios
for i, bar in enumerate(bars_with_bf):
    height = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2, height + 0.03 * max_value, f"{ratios[i]:.2f}%", ha='center', fontsize=12)  # Adjusted text position with 3% of max value


# Labeling with adjusted font sizes
plt.ylabel('Throughput (KOPS)', fontsize=axis_title_fontsize)
# plt.title('Comparison of Throughput With and Without Bloom Filter', fontsize=title_fontsize)
plt.xticks(group_positions, group_labels, fontsize=tick_label_fontsize)
plt.yticks(fontsize=tick_label_fontsize)
plt.ylim(0, y_limit)  # Setting the adjusted y-axis limit
plt.legend(fontsize=legend_fontsize, loc='upper right')
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()

# Display the plot
plt.show()


plt.savefig('bloom_filter.pdf')
plt.savefig('bloom_filter.png')