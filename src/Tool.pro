# QMake Build file
QMAKE_CC = gcc
QMAKE_LINK_C = gcc

QMAKE_CXX = g++
QMAKE_LINK = g++

BDDFLAGS = $$system(gcc BFAbstractionLibrary/compilerOptionGenerator.c -o /tmp/BFAbstractionCompilerOptionsProducer-$$(USER);/tmp/BFAbstractionCompilerOptionsProducer-$$(USER))
DEFINES += USE_CUDD
CFLAGS += -g -fpermissive

QMAKE_LFLAGS_X86_64 = -arch x86_64

QMAKE_CFLAGS_X86_64 = -arch x86_64
QMAKE_CXXFLAGS_X86_64 = -arch x86_64

QMAKE_CFLAGS_RELEASE += -g \
    $$BDDFLAGS
QMAKE_CXXFLAGS_RELEASE += -g -std=gnu++0x \
    $$BDDFLAGS
QMAKE_CFLAGS_DEBUG += -g -Wall -Wextra \
    $$BDDFLAGS
QMAKE_CXXFLAGS_DEBUG += -g -std=gnu++0x -Wall -Wextra \
    $$BDDFLAGS

TEMPLATE = app \
    console
CONFIG += debug
CONFIG -= app_bundle
CONFIG -= qt
HEADERS += BFAbstractionLibrary/BF.h BFAbstractionLibrary/BFCudd.h \
	gr1context.hpp variableTypes.hpp \
	variableManager.hpp \
	extensions/ExtractExplicitStrategy.hpp \
	extensions/RoboticsSemantics.hpp \
    extensions/WeakenSafetyAssumptions.hpp \
    extensionBiasForAction.hpp \
    extensions/ComputeCNFFormOfTheSpecification.hpp \
    extensions/Counterstrategy.hpp \
    extensions/ExtractExplicitCounterstrategy.hpp \
    extensions/IncrementalSynthesis.hpp \
    extensions/FixedPointRecycling.hpp \
    extensions/InteractiveStrategy.hpp \
    extensions/IROSfastslow.hpp \
    extensions/AnalyzeInitialPositions.hpp \
    extensions/AnalyzeAssumptions.hpp \
    BFAbstractionLibrary/BFCuddMintermEnumerator.h \
    extensions/ComputeInterestingRunOfTheSystem.hpp \
    extensions/AnalyzeSafetyLivenessInteraction.hpp \
    extensions/AbstractWinningTraceGenerator.hpp \
    extensions/Interleave.hpp \
    extensions/PermissiveExplicitStrategy.hpp \
    extensions/IncompleteInformationEstimatorSynthesis.hpp \
    extensions/NondeterministicMotion.hpp \
    extensions/ExtractSymbolicStrategy.hpp \
    extensions/TwoDimensionalCost.hpp

SOURCES += main.cpp BFAbstractionLibrary/bddDump.cpp BFAbstractionLibrary/BFCuddVarVector.cpp BFAbstractionLibrary/BFCudd.cpp BFAbstractionLibrary/BFCuddManager.cpp \
    BFAbstractionLibrary/BFCuddVarCube.cpp tools.cpp synthesisAlgorithm.cpp synthesisContextBasics.cpp variableManager.cpp \
    BFAbstractionLibrary/BFCuddMintermEnumerator.cpp

TARGET = slugs
INCLUDEPATH = ../lib/cudd-2.5.0/include BFAbstractionLibrary

LIBS += -L../lib/cudd-2.5.0/cudd -L../lib/cudd-2.5.0/util -L../lib/cudd-2.5.0/mtr -L../lib/cudd-2.5.0/st -L../lib/cudd-2.5.0/dddmp -L../lib/cudd-2.5.0/epd -lcudd -ldddmp -lmtr -lepd -lst -lutil

PKGCONFIG += 
QT -= gui \
    core
