import pandas as pd
import matplotlib.pyplot as plt

# Load the data
data = pd.read_csv('scan.csv')

# Add a new column to differentiate the sets for "with update"
data['update_set'] = data.apply(lambda row: f'Update Set {row["nums_kv"]}', axis=1)
data['update_set'] = data.apply(lambda row: 'Without Update' if row['scan_after_update'] == 0 else row['update_set'], axis=1)

# Mapping for clearer xticks
update_set_mapping = {
    'Without Update': 'Scan without Update',
    'Update Set 544921': 'Scan with Update Set 1',
    'Update Set 544620': 'Scan with Update Set 2'
}

# Summarize data by condition and index type
data_summary = data.groupby(['update_set', 'index_type'])['scan_throughput'].mean().unstack()

# Creating the plot
fig, ax = plt.subplots(figsize=(14, 8))

# Bar width
bar_width = 0.2

# Get unique update sets to determine positions
unique_update_sets = data['update_set'].unique()
positions = []
for i in range(len(unique_update_sets)):
    positions.append(i)
    positions.append(i + bar_width)

# Plotting the bars
bars = []
for i, update_set in enumerate(unique_update_sets):
    bars.append(ax.bar(positions[2*i], data_summary.loc[update_set, 'LearnedKV'], color='b', width=bar_width, edgecolor='grey', label='LearnedKV' if i == 0 else ""))
    bars.append(ax.bar(positions[2*i + 1], data_summary.loc[update_set, 'RocksDB'], color='r', width=bar_width, edgecolor='grey', label='RocksDB' if i == 0 else ""))

# Adding labels with enlarged text
ax.set_ylabel('Scan Throughput (KOPS)', fontweight='bold', fontsize=14)
ax.set_title('Scan Throughput Comparison', fontsize=16)
ax.set_xticks([p + bar_width / 2 for p in range(len(unique_update_sets))])
ax.set_xticklabels([update_set_mapping[us] for us in unique_update_sets], fontsize=14)
ax.legend(loc='upper left', bbox_to_anchor=(0.78, 0.94), fontsize=14)

# Adding the accurate numbers on the bars
for bar_group in bars:
    for bar in bar_group:
        height = bar.get_height()
        ax.annotate(f'{height:.2f}',
                    xy=(bar.get_x() + bar.get_width() / 2, height),
                    xytext=(0, 3),  # 3 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=12)

# Saving the figure
plt.savefig('scan.png')
plt.savefig('scan.pdf')
plt.show()
