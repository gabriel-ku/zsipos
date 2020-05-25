if [ "$1" == "--nobuild" ]
then
  nobuild="true"
  shift
fi
if [ "$nobuild" == "true" ]
then
	nobuild="--no-compile-gateware --no-compile-software"
fi
outdir=build_$BOARD
python3 $BOARD.py \
	--cpu-type=$CPU_TYPE --cpu-variant=$CPU_VARIANT \
	--l2-size=0 \
	--integrated-sram-size=16384 \
	--output-dir $outdir \
	--dts-file $outdir/software/include/generated/devicetree.dts \
 	--csr-json $outdir/$BOARD.json \
	$nobuild $*

