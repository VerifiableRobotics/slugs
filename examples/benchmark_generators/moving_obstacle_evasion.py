#!/usr/bin/python
#
# Creates some benchmarks for a package sorter

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
# 1. X Size of the Field
# 2. Y Size of the Field
# 3. How many glitches to allow
# ==================================
configurations = [(8,8,0),(8,8,1),(16,16,3),(16,16,4),(24,24,7),(24,24,8),(32,32,11),(32,32,12),(48,48,19),(48,48,20),(64,64,27),(64,64,28),(96,96,43),(96,96,44),(128,128,59),(128,128,60)]



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

    filenamePrefix = "moving_obstacle_"+str(configuration[0])+"x"+str(configuration[1])+"_"+str(configuration[2])+"glitches"
    outFile = open(filenamePrefix+".structuredslugs","w")

    text = """[INPUT]
obsx: 0...%(maxXM1)d
obsy: 0...%(maxYM1)d
robx: 0...%(maxX)d
roby: 0...%(maxY)d
glitches: 0...%(maxGlitch)d

[OUTPUT]
movx: 0...2
movy: 0...2
obsmove

[SYS_INIT]
! obsmove


[SYS_TRANS]
(robx < obsx) | (robx >= obsx+2) | (roby < obsy) | (roby >= obsy+2)
(obsmove' -> ( (obsx=obsx') & (obsy=obsy') ) ) 
((! obsmove') -> ! ( (obsx=obsx') & (obsy=obsy') ) )


[ENV_INIT]
robx = 0
roby = 0
obsx = %(maxXM1)d
obsy = %(maxYM1)d
glitches = 0

[ENV_TRANS]
(obsx' <= obsx+1) & (obsy' <= obsy+1) & (obsx <= obsx'+1) & (obsy <= obsy'+1) & (robx' <= robx+1) & (roby' <= roby+1) & (robx <= robx'+1) & (roby <= roby'+1) 

(robx=0 & robx'=0 & movx<=1 | robx'+1 <= robx+movx) & (robx=%(maxX)d & robx'=%(maxX)d & movx >= 1 | robx'+1 >= robx+movx) & (roby=0 & roby'=0 & movy <= 1 | roby'+1 <= roby+movy) & (roby=%(maxY)d & roby'=%(maxY)d & movy>=1 | roby'+1 >= roby+movy) 

glitches' >= glitches
(obsmove | (obsx=obsx')) & (obsmove | (obsy=obsy')) | (glitches' >= glitches+1)

""" 
    text = text % {'maxX': configuration[0]-1, 'maxY': configuration[1]-1, 'maxXM1': configuration[0]-2, 'maxYM1': configuration[1]-2,'maxGlitch': configuration[2] }
    
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


