#!/bin/bash -e

make release.o CrystalLightingVis.class

rm -rf scores
mkdir scores

./tools/update_score_worker.sh 1 100 &
./tools/update_score_worker.sh 101 200 &
./tools/update_score_worker.sh 201 300 &
./tools/update_score_worker.sh 301 400 &
./tools/update_score_worker.sh 401 500 &
./tools/update_score_worker.sh 501 600 &
./tools/update_score_worker.sh 601 700 &
./tools/update_score_worker.sh 701 800 &
wait

awk '{s+=$3} END {printf "Total = %.1f\n", s}' scores/score_*.txt | tee score.txt
