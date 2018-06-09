#!/bin/bash -e

make main.o CrystalLightingVis.class

readonly SEED=$1
shift
java CrystalLightingVis -exec "./main.o" -seed ${SEED} "$@"
