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
HEADERS += BFAbstractionLibrary/BF.h BFAbstractionLibrary/BFCudd.h gr1context.hpp variableTypes.hpp variableManager.hpp extensionExtractExplicitStrategy.hpp extensionRoboticsSemantics.hpp \
    extensionWeakenSafetyAssumptions.hpp extensionBiasForAction.hpp \
    extensionComputeCNFFormOfTheSpecification.hpp extensionCounterstrategy.hpp extensionExtractExplicitCounterstrategy.hpp \
    extensionIncrementalSynthesis.hpp extensionFixedPointRecycling.hpp extensionInteractiveStrategy.hpp extensionIROSfastslow.hpp \
    extensionAnalyzeInitialPositions.hpp extensionAnalyzeAssumptions.hpp \
    BFAbstractionLibrary/BFCuddMintermEnumerator.h \
    extensionComputeInterestingRunOfTheSystem.hpp \
    extensionAnalyzeSafetyLivenessInteraction.hpp \
    extensionAbstractWinningTraceGenerator.hpp \
    extensionInterleave.hpp \
    extensionPermissiveExplicitStrategy.hpp \
    extensionIncompleteInformationEstimatorSynthesis.hpp \
    extensionNondeterministicMotion.hpp \
    extensionExtractSymbolicStrategy.hpp \
    extensionTwoDimensionalCost.hpp

SOURCES += main.cpp BFAbstractionLibrary/bddDump.cpp BFAbstractionLibrary/BFCuddVarVector.cpp BFAbstractionLibrary/BFCudd.cpp BFAbstractionLibrary/BFCuddManager.cpp \
    BFAbstractionLibrary/BFCuddVarCube.cpp tools.cpp synthesisAlgorithm.cpp synthesisContextBasics.cpp variableManager.cpp \
    BFAbstractionLibrary/BFCuddMintermEnumerator.cpp

TARGET = slugs
INCLUDEPATH = ../lib/cudd-2.5.0/include BFAbstractionLibrary

LIBS += -L../lib/cudd-2.5.0/cudd -L../lib/cudd-2.5.0/util -L../lib/cudd-2.5.0/mtr -L../lib/cudd-2.5.0/st -L../lib/cudd-2.5.0/dddmp -L../lib/cudd-2.5.0/epd -lcudd -ldddmp -lmtr -lepd -lst -lutil

PKGCONFIG += 
QT -= gui \
    core
