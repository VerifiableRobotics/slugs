#!/usr/bin/python
#
# Creates the simple maze example with only four cells and one wall between the lower and upper left used for debugging
# This version: 
# - robot and obstacle can move one step at a time to non-occupied cells


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
# the simple example has only this one configuration!
configurations = [(2,2)]

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

    filenamePrefix = "maze_simple_"+str(configuration[0])+"x"+str(configuration[1])
    outFile = open(filenamePrefix+".structuredslugs","w")
    
    linesbytwo=configuration[1]/2
    
    

    text = """
[INPUT]
obsx: 0...%(maxX)d
obsy: 0...%(maxY)d


[OUTPUT]
robx: 0...%(maxX)d
roby: 0...%(maxY)d

[SYS_INIT]
robx = %(maxX)d & roby = 0


[SYS_TRANS]
(robx' <= robx+1) & (roby' <= roby+1) & (robx <= robx'+1) & (roby <= roby'+1) 
(robx < robx' | robx' < robx) -> (roby' = roby)
(roby < roby' | roby' < roby) -> (robx' = robx)
(obsx' = robx') ->  ((obsy' < roby') | (roby'< obsy'))
(obsy' = roby') ->  ((obsx' < robx') | (robx'< obsx'))
(robx = 0 )-> (roby' = roby)


[SYS_LIVENESS]
robx=0 & roby=1
robx=1 & roby=0

[ENV_INIT]
obsx=0 & obsy=0


[ENV_TRANS]
(obsx' <= obsx+1) & (obsy' <= obsy+1) & (obsx <= obsx'+1) & (obsy <= obsy'+1) 
(obsx < obsx' | obsx' < obsx) -> (obsy' = obsy)
(obsy < obsy' | obsy' < obsy) -> (obsx' = obsx)
(robx = obsx') -> ( (roby < obsy') | (obsy'< roby) )
(roby = obsy') -> ( (robx < obsx') | (obsx'< robx) )
(robx = obsx) -> ( (roby < obsy) | (obsy< roby) )
(roby = obsy) -> ( (robx < obsx) | (obsx< robx) )
(obsx = 0 ) -> (obsy' = obsy)


[ENV_LIVENESS]
obsx=0 & obsy=0
obsx=1 & obsy=1


""" 
    text = text % {
        'maxX': configuration[0]-1, 'maxY': configuration[1]-1,
        }
    
    outFile.write(text)
    outFile.close()

    # =====================================================
    # Translate to slugsin
    # =====================================================
    retValue = os.system(structuredSlugsParserExecutable + " " + filenamePrefix+".structuredslugs > "+filenamePrefix+".slugsin")
    if (retValue!=0):
        print >>sys.stderr, "Structured to Non-structured Slugs Translation failed!"
        raise Exception("Translation Aborted")




# ==================================
# Make files
# ==================================
for a in configurations:
    makeBenchmarkFile(a)


