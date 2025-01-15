CLANG ?= clang
OPT ?= opt
DIS ?= llvm-dis
WIDTH ?= 65536
SRCS := $(wildcard src/*.c)
BINS := $(SRCS:src/%.c=%)

all: ${BINS}

%: src/%.c ../cmake-build-debug/src/libPIMVectorizationPass.so
	# Compile the program with the forced vector width
	${CLANG} -emit-llvm -g -O3 -fno-unroll-loops -mllvm -force-vector-width=${WIDTH} -I../include -o "debug/$@.bc" -c "$<"
	${DIS} "debug/$@.bc" -o "debug/$@.ll"

	# Run the pass
	${OPT} -load ../cmake-build-debug/src/libPIMVectorizationPass.so -pim < "debug/$@.bc" > "debug/$@.pim.bc"

	# Compile the result of the pass
	${DIS} "debug/$@.pim.bc" -o "debug/$@.pim.ll"
	${CLANG} -O0 -g -o "$@" "debug/$@.pim.bc"

clean:
	rm -f ${BINS} debug/*
