#!/bin/bash -e

make release.o CrystalLightingVis.class

readonly SEED=$1
shift
java CrystalLightingVis -exec "./release.o" -seed ${SEED} "$@"
