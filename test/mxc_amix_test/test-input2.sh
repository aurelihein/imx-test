#!/bin/bash

cd $(dirname $0)
script_name=$(basename $0)
sample_name=${script_name%.*}

if [ "$#" != "1" ]; then
	echo "Please provide WAV file as parameter."
	exit -1
fi

if [ -r amixenv.sh ]
then
	source amixenv.sh
else
	echo "amixenv.sh is missing!"
	exit -1
fi

procs=(
	"arecord -q -D hw:${devices[0]} -f S32_LE -c 2 -t wav $sample_name.wav"
	"aplay -q -D hw:${devices[1]} $1"
)
pids=()

# Use TDM2 as mixer clock
prepare_tdm TDM2

echo " =============================="
echo " AMIX Test Play on hw:${devices[1]}"
echo " =============================="
echo "    Play $1 on hw:${devices[1]}"
echo "    Record $sample_name.wav on hw:${devices[0]}"
echo " =============================="

for i in "${procs[@]}"
do
	cmd="$i"
	$cmd &
	pids+=($!)
done
####################################
# Test starts here
####################################
sleep 0.01
amixer -q -c $card cset name="Output Source" TDM2

####################################
for pid in "${pids[@]}"
do
	wait $pid
done

echo " =============================="
echo " AMIX Test finished, please check $sample_name.wav"
exit 0

