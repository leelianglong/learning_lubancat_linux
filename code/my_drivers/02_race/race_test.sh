#!/bin/bash
DEV=/dev/race_dev
STATS=/sys/class/race_dev/race_dev/stats
RESET=/sys/class/race_dev/race_dev/reset
P=${1:-10}
N=${2:-100}
TOTAL=$((P * N))

echo 1 > $RESET
echo "并发 $P 进程 × $N 次 = $TOTAL 次写入"

start=$(date +%s%N)
for i in $(seq $P); do
( for j in $(seq $N); do echo x > $DEV; done ) &
done
wait
end=$(date +%s%N)

echo "--- stats ---"
cat $STATS
echo "--- 期望：四项都等于 $TOTAL ---"
echo "耗时 $(( (end - start) / 1000000 )) ms"
