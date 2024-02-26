#!/usr/bin/python
#
# Creates some benchmarks for an obstacle and a robot in a maze, used for testing the environmental-friendly synthesis extension
# This version: 
# - robot and obstacle can move one step at a time to non-occupied cells
# - there is only a wall in the middle of the maze leaving a circular path around this wall
# - this example renders the heuristic realizable

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
# ==================================
#configurations = [(3,2),(9,2),(15,2),(25,2),(35,2),(49,2),(63,2),(81,2),(99,2),(3,2),(3,10),(3,16),(3,20),(3,26),(3,30),(3,36),(3,40),(3,50)]
configurations = [(49,2),(63,2),(81,2),(99,2)]

# ==================================
# Check if run as intended
# ==================================
if __name__ != "__main__":
    raise Exception("This script is not to be used as a module!")
startingDir = os.path.dirname(sys.argv[0])

# ==================================
# Declare executables
# ==================================
structuredSlugsParserExecutable = startingDir+"../../../tools/StructuredSlugsParser/compiler.py"

# ==================================
# Make a benchmark file
# ==================================
def makeBenchmarkFile(configuration):

    filenamePrefix = "maze_loop_"+str(configuration[0])+"x"+str(configuration[1])
    outFile = open(filenamePrefix+".structuredslugs","w")
    
    linesbytwo=configuration[1]/2
    columnsbyfour=configuration[0]/4
    
    

    text = """
[INPUT]
obsx: 0...%(maxX)d
obsy: 0...%(maxY)d


[OUTPUT]
robx: 0...%(maxX)d
roby: 0...%(maxY)d

[SYS_INIT]
robx = 0 & roby = 0


[SYS_TRANS]
(robx' <= robx+1) & (roby' <= roby+1) & (robx <= robx'+1) & (roby <= roby'+1) 
(robx < robx' | robx' < robx) -> (roby' = roby)
(roby < roby' | roby' < roby) -> (robx' = robx)
(obsx' = robx') ->  ((obsy' < roby') | (roby'< obsy'))
(obsy' = roby') ->  ((obsx' < robx') | (robx'< obsx'))
((robx < %(maxX)d) & (0 < robx ))-> (roby' = roby)


[SYS_LIVENESS]
%(sysLive)s

[ENV_INIT]
obsx=1 & obsy=0

[ENV_TRANS]
(obsx' <= obsx+1) & (obsy' <= obsy+1) & (obsx <= obsx'+1) & (obsy <= obsy'+1) 
(obsx < obsx' | obsx' < obsx) -> (obsy' = obsy)
(obsy < obsy' | obsy' < obsy) -> (obsx' = obsx)
(robx = obsx') -> ( (roby < obsy') | (obsy'< roby) )
(roby = obsy') -> ( (robx < obsx') | (obsx'< robx) )
(robx = obsx) -> ( (roby < obsy) | (obsy< roby) )
(roby = obsy) -> ( (robx < obsx) | (obsx< robx) )
((robx < %(maxX)d) & (0 < robx )) -> (obsy' = obsy)


[ENV_LIVENESS]
%(envLive)s

""" 
    text = text % {
        'maxX': configuration[0]-1, 'maxY': configuration[1]-1,
        'cbytwo': ((configuration[0]-1)/2)-1,
        'dbytwo': ((configuration[0]-1)/2)+1,
        'sysLive': "\n".join(["\n".join([ "robx = "+str(4*j)+" & roby = 0" for j in xrange(1,columnsbyfour)]),
                              "\n".join([ "robx = "+str(configuration[0]-1-4*j)+" & roby = 1 " for j in xrange(0,columnsbyfour)]),
                              "robx = 0 & roby = 0"
                              ]),
        'envLive': "\n".join([
            "\n".join([ "obsx = "+str(4*j)+"& obsy = 0" for j in xrange(0,columnsbyfour)]),
            "\n".join([ "obsx = "+str(configuration[0]-1-4*j)+"& obsy = 1 " for j in xrange(0,columnsbyfour)])]),
        }
    
    outFile.write(text)
    outFile.close()

    # =====================================================
    # Translate to slugsin
    # =====================================================
    retValue = os.system(structuredSlugsParserExecutable + " " + filenamePrefix+".structuredslugs >  "+filenamePrefix+".slugsin")
    if (retValue!=0):
        print >>sys.stderr, "Structured to Non-structured Slugs Translation failed!"
        raise Exception("Translation Aborted")

    retValue = os.system("( time ../../../src/slugs --explicitStrategy --jsonOutput " + filenamePrefix+".slugsin ) > " +filenamePrefix+"_data_3FP.txt 2>&1")
    if (retValue!=0):
        print >>sys.stderr, "Strategy Extraction 3FP Failed"
        raise Exception("Translation Aborted") 

    retValue = os.system("( time ../../../src/slugs --environmentFriendlyStrategy --jsonOutput " + filenamePrefix+".slugsin ) > " +filenamePrefix+"_data_4FP.txt 2>&1")
    if (retValue!=0):
        print >>sys.stderr, "Strategy Extraction 4FP Failed"
        raise Exception("Extraction Aborted") 

    retValue = os.system("( time ../../../src/slugs --environmentFriendlyStrategy --jsonOutput --useHeuristic " + filenamePrefix+".slugsin ) > " +filenamePrefix+"_data_4FP_Heuristic.txt 2>&1")
    if (retValue!=0):
        print >>sys.stderr, "Strategy Extraction 4FP_Heuristic Failed"
        raise Exception("Extraction Aborted")

# ==================================
# Make files
# ==================================
for a in configurations:
    makeBenchmarkFile(a)


