#!/usr/bin/python
#
# Creates some benchmarks for an assembly line in a factory.

import math
import os
import sys
import resource
import subprocess
import signal
import tempfile
import copy


# ==================================
# Configurations, consisting of:
# 1. Length of Assembly line
# 2. Number of tasks to perform on each element
# 3. Number of steps for which every such number of steps, the belt moves on
# 4. Error counter: How often it is allowed that not a single task has already been performed on new elements put onto the belt
#
# Fixed are:
# - the number of robot arms is 2
# - for every incoming new element on the belt, one task has already been performed
# ==================================
configurations = [
                  (5,3,1,0),(5,4,1,0),(5,5,2,0),(5,6,2,0),
                  (3,3,1,1),(4,3,1,1),
                                      (5,5,2,1),
                  (5,3,1,4),          (5,5,2,10),
                  (5,3,1,5),          (5,5,2,11),
                  (7,3,1,0),
                  (7,5,2,0),
                  (7,5,2,10),(7,5,2,11),
]



# ==================================
# Check if run as intended
# ==================================
if __name__ != "__main__":
    raise Exception("This script is not to be used as a module!")
startingDir = os.path.dirname(sys.argv[0])

# ==================================
# Declare executables
# ==================================
translaterExecutable = startingDir+"/../../tools/translateSafetySlugsSpecToSyntCompAIGFormat.py"
structuredSlugsParserExecutable = startingDir+"/../../tools/StructuredSlugsParser/compiler.py"
aisyExecutableAndBasicOptions = "$AISY"
aigtoaigExecutable = "$AIGTOAIG"
abcExecutable = "$ABC"

# ==================================
# Read parameters
# ==================================
makeAAGBenchmarks = False
if len(sys.argv)>1:
    if sys.argv[1]=="--makeAAGBenchmarks":
        makeAAGBenchmarks = True
    else:
        print >>sys.stderr, "Error: the only parameter supported by this script is '--makeAAGBenchmarks'!"
        sys.exit(1)

# ==================================
# Make a benchmark file
# ==================================
def makeBenchmarkFile(configuration):

    filenamePrefix = "factory_assembly_"+str(configuration[0])+"x"+str(configuration[1])+"_"+str(configuration[2])+"_"+str(configuration[3])+"errors"
    outFile = open(filenamePrefix+".structuredslugs","w")

    maxErrorCount = configuration[3]

    text = """[INPUT]
movingcounter:0...%(MaxSteps)d
%(PositionAPs)s
errorCount:0...%(ErrorCount)d

[OUTPUT]
arm1:0...%(MaxX)d
arm2:0...%(MaxX)d
arm1op:0...%(NofTasksMinusOne)d
arm2op:0...%(NofTasksMinusOne)d

[ENV_TRANS]
movingcounter'=0 -> ((%(SomeNextTaskAlreadyDoneAtTheFirstElement)s) | errorCount' > errorCount)
errorCount' >= errorCount


%(LinesFirstElementsUpdated)s

%(LinesNonFirstElementsUpdated)s

# The moving counter counts module
(movingcounter<%(MaxSteps)d) -> (movingcounter' = movingcounter+1)
(movingcounter=%(MaxSteps)d) -> (movingcounter' = 0)

[SYS_TRANS]
arm1' < arm2'
arm1' <= arm1+1
arm1 <= arm1'+1
arm2' <= arm2+1
arm2 <= arm2'+1

# Everything must be done one step *before* the items are moved away.
%(SafetyGuarantees)s

[ENV_INIT]
movingcounter = 0
%(PositionAPs)s

[SYS_INIT]
arm1 = 0
arm2 = 2
arm1op = 0
arm2op = 0
""" 
    text = text % {
      'MaxSteps': (configuration[2]-1),
      'MaxX' : (configuration[0]-1),
      'NofTasksMinusOne' : configuration[1]-1,
      'PositionAPs': "\n".join(["p"+str(i)+"_"+str(j) for i in xrange(0,configuration[0]) for j in xrange(0,configuration[1])]),
      'SomeNextTaskAlreadyDoneAtTheFirstElement' : " | ".join(["p0_"+str(j)+"'" for j in xrange(0,configuration[1])]),
      'LinesFirstElementsUpdated' : "\n".join(["movingcounter'!=0 -> (p0_"+str(j)+"' <-> p0_"+str(j)+" & (movingcounter'!=0) | arm1=0 & arm1op="+str(j)+" | arm2=0 & arm2op="+str(j)+")" for j in xrange(0,configuration[1])]),
      'LinesNonFirstElementsUpdated': "\n".join(["p%(i)d_%(j)d' <-> (p%(i)d_%(j)d & (movingcounter'!=0) | arm1=%(i)d & arm1op=%(j)d | arm2=%(i)d & arm2op=%(j)d | (p%(im1)d_%(j)d & movingcounter'=0))" % {'i':i, 'j':j,'im1':i-1 } for  i in xrange(1,configuration[0]) for j in xrange(0,configuration[1])]),
      'SafetyGuarantees': "\n".join(["!(!p%(i)d_%(j)d & movingcounter'=0)" % {'i': configuration[0]-1, 'j':j} for j in xrange(0,configuration[1])]),
      'ErrorCount': maxErrorCount
    }
    
    outFile.write(text)
    outFile.close()

    # =====================================================
    # Translate to regular slugsin
    # =====================================================
    retValue = os.system(structuredSlugsParserExecutable + " " + filenamePrefix+".structuredslugs > "+filenamePrefix+".slugsin")
    if (retValue!=0):
        print >>sys.stderr, "Structured to Non-structured Slugs Translation failed!"
        raise Exception("Translation Aborted")

    if makeAAGBenchmarks:
        # =====================================================
        # Translate
        # =====================================================
        retValue = os.system(translaterExecutable + " " + filenamePrefix+".slugsin > "+filenamePrefix+".aag")
        if (retValue!=0):
            print >>sys.stderr, "Translation failed!"
            raise Exception("Translation Aborted")

        # =====================================================
        # to Binary AIG
        # =====================================================
        retValue = os.system(aigtoaigExecutable + " " +filenamePrefix+".aag"+ " " +filenamePrefix+".aig")
        if (retValue!=0):
            print >>sys.stderr, "Translation Part 2 failed!"
            raise Exception("Fuzzing Aborted")

        # =====================================================
        # Optimize
        # =====================================================
        retValue = os.system(abcExecutable + " -c \"read_aiger "+filenamePrefix+".aig; rewrite; write_aiger -s "+filenamePrefix+".aig\"")
        if (retValue!=0):
            print >>sys.stderr, "Translation Part 3 failed!"
            raise Exception("Translation Aborted")

        # =====================================================
        # to ASCII AIG
        # =====================================================
        retValue = os.system(aigtoaigExecutable + " " +filenamePrefix+".aig"+ " " +filenamePrefix+".aag")
        if (retValue!=0):
            print >>sys.stderr, "Translation Part 4 failed!"
            raise Exception("Translation Aborted")



# ==================================
# Make files
# ==================================
for a in configurations:
    makeBenchmarkFile(a)


