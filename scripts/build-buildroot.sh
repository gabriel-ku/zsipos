set -e
echo "building buildroot ..."

cd "$ZTOP/system/buildroot/buildroot"

if [ "$1"x == "clean"x ]
then
	rm -rf output
	exit
fi

if [ ! -f .config ]
then
	cp ../config .config
fi

if [ "$1"x == "config"x ]
then
	make menuconfig
	cp .config ../config
else
	make
fi

