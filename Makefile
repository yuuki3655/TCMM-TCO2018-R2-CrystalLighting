main.o: main.cpp
	g++ -std=gnu++11 -W -Wall -Wno-sign-compare -O2 -pipe -mmmx -msse \
	-msse2 -msse3 -o main.o \
	-DLOCAL_DEBUG_MODE -DLOCAL_ENTRY_POINT_FOR_TESTING \
	main.cpp

release.o: main.cpp
	g++ -std=gnu++11 -W -Wall -Wno-sign-compare -O2 -pipe -mmmx -msse \
	-msse2 -msse3 -o release.o \
	-DLOCAL_ENTRY_POINT_FOR_TESTING \
	main.cpp

CrystalLightingVis.class: CrystalLightingVis.java
	javac CrystalLightingVis.java
