#!/bin/bash -e

make release.o CrystalLightingVis.class

rm -rf scores
mkdir scores

./tools/update_score_worker.sh 1 200 &
./tools/update_score_worker.sh 201 400 &
./tools/update_score_worker.sh 401 600 &
./tools/update_score_worker.sh 601 800 &
wait

./score_diff.sh | tee score.txt
