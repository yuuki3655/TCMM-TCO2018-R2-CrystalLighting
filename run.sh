#!/bin/bash -e

make main.o CrystalLightingVis.class
java CrystalLightingVis -exec "./main.o" -seed $1
