#!/bin/bash

LDLIBS="-lpthread -latomic";

DEFINITIONS=(SYNCH_NUMA_SUPPORT SYNCH_TRACK_CPU_COUNTERS);
LIBS=("-lnuma" "-lpapi");

for i in ${!DEFINITIONS[@]}; do
    if grep -xq "\s*#define\s\+${DEFINITIONS[i]}\(\s*\|\s.*\)" libconcurrent/includes/config.h
    then
        LDLIBS="$LDLIBS ${LIBS[i]}";
    fi
done

echo $LDLIBS;