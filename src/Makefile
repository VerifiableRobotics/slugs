#================================= Slugs main Makefile =====================================

# Definitions
COMPILEROPTIONS = $(shell $(CC) BFAbstractionLibrary/compilerOptionGenerator.c -o /tmp/BFAbstractionCompilerOptionsProducer-$(USER);/tmp/BFAbstractionCompilerOptionsProducer-$(USER))
DEFINES       = -DUSE_CUDD -DNDEBUG
CFLAGS        = -pipe -O3 -g -mtune=native $(COMPILEROPTIONS) -Wall -W -fPIC $(DEFINES)
CXXFLAGS      = -pipe -O3 -g -std=c++11 -mtune=native $(COMPILEROPTIONS) -fPIC $(DEFINES)
INCPATH       = -I. -I../lib/cudd-3.0.0/ -I../lib/cudd-3.0.0/cudd -I../lib/cudd-3.0.0/mtr -I../lib/cudd-3.0.0/epd -I../lib/cudd-3.0.0/st -I../lib/cudd-3.0.0/util -I../lib/cudd-3.0.0/dddmp -IBFAbstractionLibrary 
LINK          = g++
LFLAGS        = 

# Object files
OBJECTS       = main.o \
				synthesisAlgorithm.o synthesisContextBasics.o tools.o variableManager.o \
				BFAbstractionLibrary/bddDump.o BFAbstractionLibrary/BFCuddManager.o \
				BFAbstractionLibrary/BFCuddVarCube.o BFAbstractionLibrary/BFCudd.o \
				BFAbstractionLibrary/BFCuddMintermEnumerator.o  BFAbstractionLibrary/BFCuddVarVector.o \
                $(filter-out ../lib/cudd-3.0.0/cudd/testextra.o,$(filter-out ../lib/cudd-3.0.0/cudd/testcudd.o,$(patsubst %.c,%.o,$(wildcard ../lib/cudd-3.0.0/cudd/*.c)))) \
                $(filter-out ../lib/cudd-3.0.0/mtr/testmtr.o,$(patsubst %.c,%.o,$(wildcard ../lib/cudd-3.0.0/mtr/*.c))) \
                $(patsubst %.c,%.o,$(wildcard ../lib/cudd-3.0.0/epd/*.c)) \
                $(filter-out ../lib/cudd-3.0.0/st/testst.o,$(patsubst %.c,%.o,$(wildcard ../lib/cudd-3.0.0/st/*.c))) \
                $(filter-out ../lib/cudd-3.0.0/util/cpu_stats.o,$(patsubst %.c,%.o,$(wildcard ../lib/cudd-3.0.0/util/*.c))) \
                $(filter-out ../lib/cudd-3.0.0/dddmp/testdddmp.o,$(patsubst %.c,%.o,$(wildcard ../lib/cudd-3.0.0/dddmp/*.c)))

# Headers
HEADERS_BUT_EXTENSIONS = gr1context.hpp variableManager.hpp variableTypes.hpp \
	BFAbstractionLibrary/bddDump.h \
	BFAbstractionLibrary/BFCudd.h \
	BFAbstractionLibrary/BFCuddInlines.h \
	BFAbstractionLibrary/BFCuddManager.h \
	BFAbstractionLibrary/BFCuddMintermEnumerator.h \
	BFAbstractionLibrary/BFCuddVarCube.h \
	BFAbstractionLibrary/BFCuddVarVector.h \
	BFAbstractionLibrary/BF.h

# Executable & Cleaning
slugs: $(OBJECTS)
	$(LINK) $(LFLAGS) -o slugs $(OBJECTS)

clean: 
	rm -f $(OBJECTS) ../lib/cudd-3.0.0/config.h

# Special Transition Rules
../lib/cudd-3.0.0/config.h:
	cd ../lib/cudd-3.0.0; ./configure

main.o : main.cpp ../lib/cudd-3.0.0/config.h extension*.hpp $(HEADERS_BUT_EXTENSIONS)
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

# General Translation rules
%.o : %.cpp ../lib/cudd-3.0.0/config.h $(HEADERS_BUT_EXTENSIONS)
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

%.o : %.c ../lib/cudd-3.0.0/config.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o "$@" "$<"


