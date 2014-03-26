#!/usr/bin/python
#
# Creates some benchmarks for the discretized cinderella-stepmother game

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
# 1. Number of Buckets
# 2. Number of adjacent buckets that may be emptied by cinderella in each step
# 3. Bucket capacity
# 4. Number of elements that the stepmother might add in total in each step
# ==================================
configurations = [
                  (5,2,6,4),(5,2,7,4),
                  (5,3,12,9),(5,3,13,9),
                  (6,1,14,5), (6,1,15,5),
                  (6,1,29,10),
                  (6,2,14,6),(6,2,13,6),
                  (6,3,11,6),(6,3,10,6),
                  (7,1,33,10)
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

    filenamePrefix = "cinderella_stepmother_n"+str(configuration[0])+"c"+str(configuration[1])+"_"+str(configuration[2])+"_by_"+str(configuration[3])
    outFile = open(filenamePrefix+".structuredslugs","w")

    nofBuckets = configuration[0]
    cinderellaPower = configuration[1]
    bucketCapacity = configuration[2]
    stepmotherPower = configuration[3]

    text = """[INPUT]
%(inLines)s

[OUTPUT]
%(outLines)s
e:0...%(nofBucketsMinusOne)d

[ENV_TRANS]
%(stepMotherRestrictionLine)s

[ENV_INIT]
%(envInit)s

[SYS_INIT]
%(sysInit)s

[SYS_TRANS]
%(sysTransA)s
%(sysTransB)s

""" 
    text = text % {
      'inLines': "\n".join(["x"+str(i)+":0..."+str(stepmotherPower) for i in xrange(0,nofBuckets)]),
      'outLines': "\n".join(["y"+str(i)+":0..."+str(bucketCapacity) for i in xrange(0,nofBuckets)]),
      'nofBucketsMinusOne': nofBuckets-1,
      'stepMotherRestrictionLine': "+".join(["x"+str(i)+"'" for i in xrange(0,nofBuckets)])+" <= "+str(stepmotherPower),
      'envInit': "\n".join(["x"+str(i)+" = 0" for i in xrange(0,nofBuckets)]),
      'sysInit': "\n".join(["y"+str(i)+" = 0" for i in xrange(0,nofBuckets)]),
      'sysTransA': "\n".join(["!((e<="+str(i)+" & e+"+str(cinderellaPower)+" > "+str(i)+") | (e+"+str(cinderellaPower-1)+" >= "+str(i+nofBuckets)+")) -> y"+str(i)+"' = y"+str(i)+" + x"+str(i)+"'" for i in xrange(0,nofBuckets)]),
      'sysTransB': "\n".join(["((e<="+str(i)+" & e+"+str(cinderellaPower)+" > "+str(i)+") | (e+"+str(cinderellaPower-1)+" >= "+str(i+nofBuckets)+")) -> y"+str(i)+"' = x"+str(i)+"'" for i in xrange(0,nofBuckets)])
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


