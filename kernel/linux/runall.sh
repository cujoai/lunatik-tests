testfiles=" \
	big.lua \
	closure.lua \
	db.lua \
	events.lua \
	locals.lua \
	pm.lua \
	vararg.lua \
	calls.lua \
	constructs.lua \
	errors.lua \
	gc.lua \
	literals.lua \
	nextvar.lua \
	preload.lua \
	coroutine.lua \
	sort.lua \
	strings.lua \
	tpack.lua \
	goto.lua \
	"

for f in $testfiles
do
	echo "running $f"
	lua runtest.lua $f
	dmesg | tail -5
	echo "-----"
done

