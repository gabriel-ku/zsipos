set -e
echo "buildling bbl ..."

cd "$ZTOP/kernel"

SRCDIR="`pwd`/riscv-pk"
WRKDIR="`pwd`/build_$BITS/bbl"

if [ "$1" == "clean" ]
then
	rm -rf "$WRKDIR"
	exit
fi

mkdir -p "$WRKDIR"

cd "$WRKDIR"

if [ ! -f config.status ]
then
	"$SRCDIR/configure" \
		--host="$ZTC_PREFIX" \
		--with-arch=rv${BITS}imac \
		--with-payload=../linux/vmlinux \
		--with-mem-start=0x80000000 
fi

make



