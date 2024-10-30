import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Load the data
data = pd.read_csv('phase_throughput.csv')

# Extracting throughput columns
read_throughput_cols = [None, 'P1 read throughput', 'P2 read throughput', 'P3 read throughput']
write_throughput_cols = ['P0 throughput', 'P1 write throughput', 'P2 write throughput', 'P3 write throughput']

rocksdb_read = data[data['index_type'] == 'RocksDB'][read_throughput_cols[1:]].values.flatten()
learnedkv_read = data[data['index_type'] == 'LearnedKV'][read_throughput_cols[1:]].values.flatten()
rocksdb_write = data[data['index_type'] == 'RocksDB'][write_throughput_cols].values.flatten()
learnedkv_write = data[data['index_type'] == 'LearnedKV'][write_throughput_cols].values.flatten()

# Adding a zero for P0 write throughput
rocksdb_read = np.insert(rocksdb_read, 0, 0)
learnedkv_read = np.insert(learnedkv_read, 0, 0)

# Adjusting bar width and spacing
bar_width = 0.35
spacing = 0.8
group_width = 4 * bar_width + spacing

# Adjusting indices for individual bars for each operation and system with increased spacing
phases = ['P0', 'P1', 'P2', 'P3']
index_rocksdb_read = np.arange(len(phases)) * group_width - bar_width*1.5
index_learnedkv_read = np.arange(len(phases)) * group_width - bar_width/2
index_rocksdb_write = np.arange(len(phases)) * group_width + bar_width/2
index_learnedkv_write = np.arange(len(phases)) * group_width + bar_width*1.5

# Plotting
fig, ax = plt.subplots(figsize=(16, 8))
ax.bar(index_rocksdb_read, rocksdb_read, bar_width, label='RocksDB Read', color='b', alpha=0.6)
ax.bar(index_learnedkv_read, learnedkv_read, bar_width, label='LearnedKV Read', color='c', alpha=0.6)
ax.bar(index_rocksdb_write, rocksdb_write, bar_width, label='RocksDB Write', color='r', alpha=0.6)
ax.bar(index_learnedkv_write, learnedkv_write, bar_width, label='LearnedKV Write', color='m', alpha=0.6)

# Adjusting font sizes
title_fontsize = 28
axis_title_fontsize = 26
tick_label_fontsize = 24
legend_fontsize = 24


ax.set_xlabel('Phases', fontsize=axis_title_fontsize)
ax.set_ylabel('Throughput (KOPS)', fontsize=axis_title_fontsize)
ax.set_title('Throughput Comparison between LearnedKV and RocksDB', fontsize=title_fontsize)
ax.set_xticks(np.arange(len(phases)) * group_width)
ax.set_xticklabels(phases, fontsize=tick_label_fontsize)
ax.tick_params(axis='y', labelsize=tick_label_fontsize)
ax.legend(fontsize=legend_fontsize)

plt.tight_layout()
plt.grid(True, which="both", linestyle="--", linewidth=0.5, axis='y')
plt.show()

plt.savefig('phase_throughput.pdf')
plt.savefig('phase_throughput.png')
