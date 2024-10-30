import matplotlib.pyplot as plt
import numpy as np

def parse_data(filename):
    data = {i: {'write': [], 'read': []} for i in range(10)}
    
    with open(filename, 'r') as file:
        for line in file:
            if line.startswith('Stage'):
                parts = line.split()
                stage = int(parts[1])
                time = int(parts[6])
                op_type = 'write' if 'write' in line else 'read'
                data[stage][op_type].append(time)
    
    return data

def calculate_averages(data):
    averages = {
        'write': [np.mean(data[i]['write']) for i in range(10)],
        'read': [np.mean(data[i]['read']) for i in range(10)]
    }
    return averages

def create_plot(averages):
    stages = range(10)
    
    plt.figure(figsize=(12, 6))
    write_line, = plt.plot(stages, averages['write'], 'o-', color='#1f77b4', label='Write Time')
    read_line, = plt.plot(stages, averages['read'], 'o-', color='#ff7f0e', label='Read Time')
    
    plt.title('Read and Write Times Across Different Stages')
    plt.xlabel('Stage')
    plt.ylabel('Time Cost (ms)')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)
    
    # Set x-axis ticks for all stages
    plt.xticks(stages)
    
    # Add data point labels for first and last stages
    for line in [write_line, read_line]:
        y_data = line.get_ydata()
        plt.annotate(f'{y_data[0]:.2f}', (stages[0], y_data[0]), textcoords="offset points", xytext=(0,10), ha='center')
        plt.annotate(f'{y_data[-1]:.2f}', (stages[-1], y_data[-1]), textcoords="offset points", xytext=(0,10), ha='center')
    
    plt.tight_layout()
    plt.savefig('read_write_times.png', dpi=300)
    plt.savefig('read_write_times.pdf')
    plt.show()

# Main execution
filename = 'output.txt'
data = parse_data(filename)
averages = calculate_averages(data)
create_plot(averages)