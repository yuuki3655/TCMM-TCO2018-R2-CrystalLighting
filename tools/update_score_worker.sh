#!/bin/bash -e

readonly BEGIN=$1
readonly END=$2

make release.o CrystalLightingVis.class

test_case=1
while read test_seed; do
  if [ ${test_case} -ge ${BEGIN} ] && [ ${test_case} -le ${END} ]; then
    echo "Run "${test_case}"/800"
    java CrystalLightingVis -exec "./release.o" -seed ${test_seed} -novis > scores/score_${test_seed}.txt
  fi
  test_case=$((test_case+1))
done <testset.txt
