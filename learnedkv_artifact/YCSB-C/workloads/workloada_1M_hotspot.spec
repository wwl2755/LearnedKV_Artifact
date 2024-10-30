# Yahoo! Cloud System Benchmark
# Workload A: Update heavy workload
#   Application example: Session store recording recent actions
#                        
#   Read/update ratio: 50/50
#   Default data size: 1 KB records (10 fields, 100 bytes each, plus key)
#   Request distribution: zipfian

recordcount=1000000
operationcount=3000000
workload=com.yahoo.ycsb.workloads.CoreWorkload

readallfields=true

readproportion=0
updateproportion=1
scanproportion=0
insertproportion=0

requestdistribution=latest

# Percentage of data items that constitute the hot set
hotspotdatafraction=0.2

# Percentage of operations that access the hot set
hotspotopnfraction=0.8

