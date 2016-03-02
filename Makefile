SRCCOMMON = source/bitvm.cpp
HEADERS = inc/BitVM.h inc/MicroBitTouchDevelop.h

-include Makefile.local

all:
	mkdir -p build
	node scripts/functionTable.js $(SRCCOMMON) $(HEADERS) yotta_modules/microbit-dal/inc/*.h
	yotta build
