#!/bin/bash -e

win=0
lose=0
tie=0
prev_total=0
curr_total=0
while read test_seed; do
  prev=$(git show HEAD:scores/score_${test_seed}.txt | awk '{printf "%.1f", $3}')
  curr=$(awk '{printf "%.1f", $3}' scores/score_${test_seed}.txt)
  if (( $(echo ${prev}' < '${curr} | bc -l) )); then
    win=$((win+1))
  elif (( $(echo ${prev}' > '${curr} | bc -l) )); then
    lose=$((lose+1))
  else
    tie=$((tie+1))
  fi
  prev_total=$(echo ${prev_total} ${prev} | awk '{printf "%.1f", $1 + $2}')
  curr_total=$(echo ${curr_total} ${curr} | awk '{printf "%.1f", $1 + $2}')
done <testset.txt

echo "win = "${win}
echo "lose = "${lose}
echo "tie = "${tie}
echo "prev_total = "${prev_total}
echo "curr_total = "${curr_total}
