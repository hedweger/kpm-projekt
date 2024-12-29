#!/bin/bash

echo "running simulation..."
for direction in UL # DL
do
  for mode in UE_COVERAGE # BEAM_SHAPE COVERAGE_AREA 
  do
    for power in 35 # 20 50
    do
      echo "running $direction $mode $power"
      ./ns3 run "scratch/haca-kpm.cc --direction=$direction --mode=$mode --power=$power"
    done
  done
done

echo "simulations done"
