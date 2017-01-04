#!/usr/bin/python
#
# Performs k-resilient GR(1) synthesis
# Takes the input file in Slugs format
#
# Copyright (c) 2014, Ruediger Ehlers
#    All rights reserved.
#
#    Redistribution and use in source and binary forms, with or without
#    modification, are permitted provided that the following conditions are met:
#        * Redistributions of source code must retain the above copyright
#          notice, this list of conditions and the following disclaimer.
#        * Redistributions in binary form must reproduce the above copyright
#          notice, this list of conditions and the following disclaimer in the
#          documentation and/or other materials provided with the distribution.
#        * Neither the name of any university at which development of this tool
#          was performed nor the names of its contributors may be used to endorse
#          or promote products derived from this software without specific prior
#          written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND CONTRIBUTORS BE LIABLE
#    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
import math
import os
import sys, code
import resource
import subprocess
import signal
import tempfile
import copy
import itertools
import time

# ==================================
# Entry point
# ==================================
if len(sys.argv)<2:
    print >>sys.stderr, "Error: Need Specification file as parameter"
    sys.exit(1)
specFile = sys.argv[1]
alsoWriteOptimalFiles = False
if len(sys.argv)>2:
    if sys.argv[2]=="--writeOptimalCaseSpecifications":
        alsoWriteOptimalFiles = True
    else:
        print >>sys.stderr, "Error: The only parameter other than the input file that is supported is '--writeOptimalCaseSpecifications'"
        sys.exit(1)


# Read Spec file -- Treat variable limit lines as strict
specFile = open(specFile,"r")
mode = ""
lines = {"[ENV_TRANS]":[],"[ENV_INIT]":[],"[INPUT]":[],"[OUTPUT]":[],"[SYS_TRANS]":[],"[SYS_INIT]":[],"[ENV_LIVENESS]":[],"[SYS_LIVENESS]":[] }
essentialEnvTrans = []
nextIsVariableLimits = False

for line in specFile.readlines():
    line = line.strip()
    if line == "":
        pass
    elif line.startswith("#"):
        if line.startswith("## Variable limits"):
            nextIsVariableLimits = True
    elif line.startswith("["):
        mode = line
        # if not mode in lines:
        #    lines[mode] = []
    else:
        if nextIsVariableLimits and mode=="[ENV_TRANS]":
            essentialEnvTrans.append(line)
        else:
            lines[mode].append(line)
        nextIsVariableLimits = False
specFile.close()

# ==================================
# Timing
# ==================================
sumTimeSlugs = 0.0
nofSlugsCalls = 0

# ==================================
# Debugging Helper
# ==================================
def DebugKeyboard(banner="Debugger started (CTRL-D to quit)"):

    # use exception trick to pick up the current frame
    try:
        raise None
    except:
        frame = sys.exc_info()[2].tb_frame.f_back

    # evaluate commands in current namespace
    namespace = frame.f_globals.copy()
    namespace.update(frame.f_locals)

    print "START DEBUG"
    code.interact(banner=banner, local=namespace)
    print "END DEBUG"

# ==================================
# Find relative position of the synthesis tool
# ==================================
# Compute path to examples
basepath =  os.path.realpath(__file__)
basepath = basepath[0:basepath.rfind("/")]
basepath = basepath + "/../src/slugs"
print >>sys.stderr,"Base path: "+basepath


# ==================================
# Lattice frontier set functions
# ==================================
def getFrontierSetInLatticeRecurse(oracleFunction,startingPoint,resultStoragePlace,cachedOracleFalseElements,doneStoragePlace,solutionsKnownAlready):
    # Returns True if we found a greater or equal element that the oracleFunction maps to True

    # DebugKeyboard()
    # Been here already?
    if startingPoint in doneStoragePlace:
        for a in resultStoragePlace:
            if startingPoint.isLeqThan(a):
                return True
        return False
    for otherElement in cachedOracleFalseElements:
        if otherElement.isLeqThan(startingPoint):
            return False
    doneStoragePlace.add(startingPoint)

    # Dominated?
    toBeChecked = True
    print >>sys.stderr, "Working on: "+str(startingPoint)
    for a in resultStoragePlace:
        if startingPoint.isLeqThan(a):
            print >>sys.stderr,"Found "+str(startingPoint)+" to be small than "+str(a)
            toBeChecked = False
    # Known already?
    for a in solutionsKnownAlready:
        if startingPoint.isLeqThan(a):
            toBeChecked = False
    # Check if we did not went over the border
    if toBeChecked:
        if not oracleFunction(startingPoint.vec):
            cachedOracleFalseElements.add(startingPoint)
            return False
    # Compute successors and recurse!
    foundGreaterElement = False
    for a in startingPoint.getSuccessors():
        print >>sys.stderr,"Recursing from "+str(startingPoint)+" into "+str(a)
        newElementFound = getFrontierSetInLatticeRecurse(oracleFunction,a,resultStoragePlace,cachedOracleFalseElements,doneStoragePlace,solutionsKnownAlready)
        foundGreaterElement = foundGreaterElement or newElementFound 
    print >>sys.stderr,"Return from recursion: "+str(startingPoint)
    # Store this in if we did not find a greater element and it is not dominated by something that we have already
    if not foundGreaterElement: 
        for a in resultStoragePlace:
            if startingPoint.isLeqThan(a):
                return False
        resultStoragePlace.append(startingPoint)
        return True


def getFrontierSetInLattice(oracleFunction,startingPoint,solutionsKnownAlready = []):
    resultStoragePlace = []
    doneStoragePlace = set([])
    cachedOracleFalseElements = set([])
    getFrontierSetInLatticeRecurse(oracleFunction,startingPoint,resultStoragePlace,cachedOracleFalseElements,doneStoragePlace,solutionsKnownAlready)
    return resultStoragePlace


# ==================================
# Find Infty-robust solutions
# ==================================
inftyRobustSolutions = []

def checkInftyRobustImplementability(whichAssumptionsAreRobustified):
    global sumTimeSlugs
    global nofSlugsCalls
    print >>sys.stderr,"checkInftyRobustImplementability: "+str(whichAssumptionsAreRobustified)
    tempdir = tempfile.mkdtemp("slugsRobust")
    tempfilename = tempdir+"/spec.txt"
    tempfileData = open(tempfilename,"w")
    for copyCategory in ["[INPUT]","[OUTPUT]","[ENV_LIVENESS]","[ENV_INIT]","[SYS_INIT]","[SYS_TRANS]"]:
        tempfileData.write(copyCategory+"\n")
        for a in lines[copyCategory]:
            tempfileData.write(a+"\n")
        tempfileData.write("\n")
    tempfileData.write("[ENV_TRANS]\n")
    for line in essentialEnvTrans:
        tempfileData.write(line+"\n")
    envTrans = lines["[ENV_TRANS]"]
    sysPrefix = ""
    for i in xrange(0,len(envTrans)):
        if (i>=len(whichAssumptionsAreRobustified)) or not whichAssumptionsAreRobustified[i]:
            tempfileData.write(envTrans[i]+"\n")
        else:
            sysPrefix = "| ! "+envTrans[i]+" "+sysPrefix
    sysLive = lines["[SYS_LIVENESS]"]
    tempfileData.write("[SYS_LIVENESS]\n")
    for line in sysLive:
        tempfileData.write(sysPrefix+line+"\n")
    tempfileData.close()

    # if str(whichAssumptionsAreRobustified) == "(True, False, True)":
    #     print >>sys.stderr, "Terminating!"
    #     sys.exit(1)


    # Try to perform synthesis
    startTime = time.time()
    process = subprocess.Popen(basepath+" "+tempfilename, shell=True, bufsize=1000000, stderr=subprocess.PIPE)
    output = process.stderr
    realizable = None
    for line in output:
        print >>sys.stderr,"l: "+line.strip()
        line = line.strip()
        if line == "RESULT: Specification is realizable.":
            realizable = True
        elif line == "RESULT: Specification is unrealizable.":
            realizable = False
    if realizable == None:
        print >>sys.stderr, "Error: Can't find realizability result"
        print >>sys.stderr, "Specification file: "+tempfilename
        sys.exit(1)
    sumTimeSlugs += time.time()-startTime
    nofSlugsCalls += 1
    print >>sys.stderr,"Slugs Computation time: "+str(time.time()-startTime)

    # Clean it up!
    os.unlink(tempfilename)
    os.rmdir(tempdir)
    return realizable

def findInftyRobustSolutions():
    resultStore = []

    class BoolVectorLatticeElement:
        def __init__(self, initVec):
            self.vec = tuple(initVec)
        def __repr__(self):
            return str(self.vec)
        def __key(self):
            return self.vec
        def __eq__(x, y):
            return x.__key() == y.__key()
        def __hash__(self):
            return hash(self.__key())
        def getSuccessors(self):
            results = []
            for a in xrange(0,len(self.vec)):
                if not self.vec[a]:
                    newVec = list(self.vec)
                    newVec[a] = True
                    results.append(BoolVectorLatticeElement(newVec))
            return results
        def isLeqThan(self,other):
            result = True
            for a in xrange(0,len(self.vec)):
                result = result and (other.vec[a] or not self.vec[a])
            return result 
   
    resultStore = getFrontierSetInLattice(checkInftyRobustImplementability,BoolVectorLatticeElement([False for line in lines["[ENV_TRANS]"]]))
    return resultStore



# ==================================
# Find Optimal robustness cases. Optimize with respect to
# - Robustness stage: (0) unrobust (1) robust with respect to the resilience number (2) infty-resilient
# - Resilience number
# ==================================
def binaryEncode(nofBits,value):
    result = []
    while len(result)<nofBits:
        if (value & 1)>0:
            result.append(True)
        else:
            result.append(False)
        value = value >> 1
    return result

def writeMixedRobustSpecification(whichAssumptionsAreRobustified,filename):

    tempfileData = open(filename,"w")

    # Add outbits for the counter bits
    tempfileData.write("[OUTPUT]\n")
    for a in lines["[OUTPUT]"]:
        tempfileData.write(a+"\n")
    maxCounterValue = whichAssumptionsAreRobustified[len(whichAssumptionsAreRobustified)-1]
    nofCounterRobustifiedAssumptions = 0
    nofCounterBits = int(math.ceil(math.log(maxCounterValue+1)/math.log(2)))
    for a in xrange(0,nofCounterBits):
        tempfileData.write("_ResilienceCounter_"+str(a)+"\n")
    tempfileData.write("\n")

    # Unaltered Slugs input file categories
    for copyCategory in ["[INPUT]","[ENV_INIT]"]:
        tempfileData.write(copyCategory+"\n")
        for a in lines[copyCategory]:
            tempfileData.write(a+"\n")
        tempfileData.write("\n")

    # Liveness assumptions
    tempfileData.write("[ENV_LIVENESS]\n")
    for a in lines["[ENV_LIVENESS]"]:
        tempfileData.write("| "*(nofCounterBits)+" ".join(["_ResilienceCounter_"+str(i) for i in xrange(0,nofCounterBits)])+" "+a+"\n")
    tempfileData.write("\n")
    
    # SysInit copy & add Counter information
    tempfileData.write("[SYS_INIT]\n")
    for a in lines["[SYS_INIT]"]:
        tempfileData.write(a+"\n")
    for a in xrange(0,nofCounterBits):
        tempfileData.write("! _ResilienceCounter_"+str(a)+"\n")

    # ENV_TRANS (basic cases & preparing data structures)
    tempfileData.write("\n[ENV_TRANS]\n")
    for line in essentialEnvTrans:
        tempfileData.write(line+"\n")
    envTrans = lines["[ENV_TRANS]"]
    sysPrefix = ""
    casesMasked = []
    for i in xrange(0,len(envTrans)):
        if whichAssumptionsAreRobustified[i]==0:
            tempfileData.write(envTrans[i]+"\n")
        elif whichAssumptionsAreRobustified[i]==1:
            casesMasked.append(envTrans[i])
            sysPrefix = "| ! "+envTrans[i]+" "+sysPrefix
        elif whichAssumptionsAreRobustified[i]==2:
            sysPrefix = "| ! "+envTrans[i]+" "+sysPrefix
        else:
            raise "Internal error"

    # ENV_Trans (we shall not exceed the counter value of maxCounterValue)
    for currentCounterValue in xrange(maxCounterValue-len(casesMasked)+1,maxCounterValue+1):

        # Prepare counter-prefix
        binaryEncodingOfCounterValue = binaryEncode(nofCounterBits,currentCounterValue)
        selectorPrefix = "| ! "+("& "*(nofCounterBits))+" ".join([("" if binaryEncodingOfCounterValue[i] else "! ")+"_ResilienceCounter_"+str(i) for i in xrange(0,nofCounterBits)]+["1"])

        # Combinations of failures that exceed the prefix shall not occur
        disallowedCombinations = itertools.combinations(casesMasked, maxCounterValue-currentCounterValue+1)
        for combination in disallowedCombinations:
            suffix = "| "*len(combination)+" ".join([a for a in combination]+["0"])
            tempfileData.write(selectorPrefix+" "+suffix+"\n")
    
    # SYS_TRANS (basic part)
    tempfileData.write("\n[SYS_TRANS]\n")
    for a in lines["[SYS_TRANS]"]:
        tempfileData.write(a+"\n")
        
    # SYS_TRANS (accumulate the error)
    for fromValue in xrange(0,maxCounterValue+1):
        for toValue in xrange(fromValue,min(maxCounterValue+1,fromValue+len(casesMasked)+1)):

            # Encode pre counter value
            binaryEncodingOfPreCounterValue = binaryEncode(nofCounterBits, fromValue)
            selectorPrefix = "! "+("& "*(nofCounterBits))+" ".join([("" if binaryEncodingOfPreCounterValue[i] else "! ")+"_ResilienceCounter_"+str(i) for i in xrange(0,nofCounterBits)]+["1"])

            # Encode that the post counter value is <= toValue
            def recurseSmaller(reference,pos):
                # print "Recursing Smaller: "+str(reference)+","+str(pos)
                if pos<0:
                    return "1"
                if (reference & (1 << pos))>0:
                    return "| ! _ResilienceCounter_"+str(pos)+"' "+recurseSmaller(reference,pos-1)
                else:
                    return "& ! _ResilienceCounter_"+str(pos)+"' "+recurseSmaller(reference,pos-1)
            selectorPrefix = "| "+selectorPrefix+" "+recurseSmaller(toValue,nofCounterBits-1)

            # Traverse over the list of toValue-fromValue faults that could make this possible. One of them needs to hold if we
            # want to increase the counter value by this far.
            for selectedConstraints in itertools.combinations(casesMasked,len(casesMasked)-toValue+fromValue):
                # print >>sys.stderr, "From "+str(fromValue)+" to "+str(toValue)+" selectedConstraints Size: "+str(len(selectedConstraints))
                thisCase = "| "*len(selectedConstraints)+"0 "+" ".join(["! "+c for c in selectedConstraints])
                tempfileData.write("| "+selectorPrefix+" "+thisCase+"\n")

    # SYS_LIVENESS - Normal lines
    sysLive = lines["[SYS_LIVENESS]"]
    tempfileData.write("\n[SYS_LIVENESS]\n")
    for line in sysLive:
        tempfileData.write(sysPrefix+line+"\n")

    # SYS_LIVENESS - Must go to 0 eventually if errors stop popping up
    tempfileData.write(sysPrefix+("& "*(nofCounterBits))+" ".join(["! _ResilienceCounter_"+str(i)+"'" for i in xrange(0,nofCounterBits)]+["1"])+"\n")
    tempfileData.close()


def checkMixedRobustImplementability(whichAssumptionsAreRobustified):
    global sumTimeSlugs
    global nofSlugsCalls
    print >>sys.stderr,"Oracle Call: " + str(whichAssumptionsAreRobustified)
    tempdir = tempfile.mkdtemp("slugsRobust")
    tempfilename = tempdir+"/spec.txt"

    writeMixedRobustSpecification(whichAssumptionsAreRobustified,tempfilename)



    # Try to perform synthesis
    startTime = time.time()
    process = subprocess.Popen(basepath+" "+tempfilename, shell=True, bufsize=1000000, stderr=subprocess.PIPE)
    output = process.stderr
    realizable = None
    for line in output:
        print >>sys.stderr,"l: "+line.strip()
        line = line.strip()
        if line == "RESULT: Specification is realizable.":
            realizable = True
        elif line == "RESULT: Specification is unrealizable.":
            realizable = False
    sumTimeSlugs += time.time()-startTime
    nofSlugsCalls += 1
    print >>sys.stderr,"Slugs Computation time: "+str(time.time()-startTime)

    # Debug check
    # if str(whichAssumptionsAreRobustified)=="(0, 0, 1, 3)":
    #     print "ABORTING!!!!!!!!"
    #     sys.exit(1)

    if realizable == None:
        print >>sys.stderr, "Error: Can't find realizability result"
        print >>sys.stderr, "Specification file: "+tempfilename
        sys.exit(1)

    # Clean it up!
    os.unlink(tempfilename)
    os.rmdir(tempdir)
    return realizable

def findMixedRobustSolutions(inftyRobustSolutions):
    resultStore = []
    class MixedLatticeElement:
        def __init__(self, setOfInftyResilientSolutions,vec):
            self.setOfInftyResilientSolutions = setOfInftyResilientSolutions
            self.vec = tuple(vec)
        def __repr__(self):
            return str(self.vec)
        def __key(self):
            return self.vec
        def __eq__(x, y):
            return x.__key() == y.__key()
        def __hash__(self):
            return hash(self.__key())
        def getSuccessors(self):
            results = []
            # Assumption mask increasement
            for a in xrange(0,len(self.vec)-1):
                if self.vec[a]<2:
                    newVec = list(self.vec)
                    newVec[a] += 1
                    results.append(MixedLatticeElement(self.setOfInftyResilientSolutions,newVec))
            # Resilience number incresement
            isMoreAmbitionThanInftyRobustSolutions = True
            for reference in self.setOfInftyResilientSolutions:
                isLeq = True
                isLess = False
                for i in xrange(0,len(reference.vec)):
                    isLeq = isLeq and ((self.vec[i]==0) or reference.vec[i])
                    isLess = isLess or ((self.vec[i]>0) and not reference.vec[i])
                isMoreAmbitionThanInftyRobustSolutions = isMoreAmbitionThanInftyRobustSolutions and (isLess or not isLeq)
            if isMoreAmbitionThanInftyRobustSolutions:
                newVec = list(self.vec)
                newVec[len(newVec)-1] += 1
                results.append(MixedLatticeElement(self.setOfInftyResilientSolutions,newVec))
            print >>sys.stderr,"Super-Elements of "+str(self.vec)+": "+str(results)
            return results
        def isLeqThan(self,other):
            result = True
            for a in xrange(0,len(self.vec)):
                result = result and (other.vec[a] >= self.vec[a])
            return result 
   
    startingSolution = [0 for a in inftyRobustSolutions[0].vec]+[1]
    oldSolutions = [MixedLatticeElement(inftyRobustSolutions,[2 if a else 0 for a in k.vec]+[1]) for k in inftyRobustSolutions]
    resultStore = getFrontierSetInLattice(checkMixedRobustImplementability,MixedLatticeElement(inftyRobustSolutions,startingSolution),oldSolutions)
    return resultStore





#======================================
# Main part of the program
#======================================
print >>sys.stderr,"========================================="
print >>sys.stderr,"Assumptions considered"
print >>sys.stderr,"========================================="
for a in lines["[ENV_TRANS]"]:
    print >>sys.stderr," - "+a[0:50]

inftyRobustSolutions = findInftyRobustSolutions()
if len(inftyRobustSolutions)==0:
    print "Result: Our specification is absolutely unrealizable, so the script stops here."
    sys.exit(1)
print >>sys.stderr,"========================================="
print >>sys.stderr,"Result: "+str( inftyRobustSolutions )
print >>sys.stderr,"========================================="
print >>sys.stderr,"Analysing cases for Mixed resilience.\n"
print >>sys.stderr,"========================================="
mixedRobustSolutions = findMixedRobustSolutions(inftyRobustSolutions)
print >>sys.stderr,"========================================="
print "Error-resilience levels achievable:"
for a in mixedRobustSolutions:
    print "-",
    for i,x in enumerate(a.vec):
        if i==len(a.vec)-1:
            print "k="+str(x)
        else:
            if x==0:
                print "none,",
            elif x==1:
                print "some,",
            elif x==2:
                print "any, ",
            else:
                print "(ERROR: Cannot interpret resilience level)"

print >>sys.stderr,"# Slugs calls: "+str(nofSlugsCalls)
print "\nTotal time spent with the Slugs GR(1) synthesis tool (seconds): "+str(sumTimeSlugs)
print >>sys.stderr,"Average Slugs time (seconds): "+str(sumTimeSlugs/nofSlugsCalls)

#======================================
# Write optimal specs
#======================================
if alsoWriteOptimalFiles:
    specFile = sys.argv[1]
    fileBaseName = specFile[0:specFile.rfind(".slugsin")]
    print fileBaseName
    for solution in mixedRobustSolutions:
        writeMixedRobustSpecification(solution.vec,fileBaseName+str(solution)+".slugsin")

