#!/bin/bash
for k in 1 2 3 4 5; do
echo "== 第 $k 轮 =="
sudo /tmp/race_test.sh 10 100 | grep -E 'plain|atomic|mutex|spin'
done
