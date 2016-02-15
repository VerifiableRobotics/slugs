#!/usr/bin/python
#

import random
import os
import sys
import subprocess, copy

# =====================================================
# Compute paths
# =====================================================
slugsExecutableAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("analyzeStuckAtConstant.py")]+"../src/slugs"
slugsCompilerAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("analyzeStuckAtConstant.py")]+"StructuredSlugsParser/compiler.py --thorougly"

slugsCompiledFile = "/tmp/check_"+str(os.getpid())+".slugsin"
slugsModifiedFile = "/tmp/check_"+str(os.getpid())+".mod.slugsin"
slugsReturnFile = "/tmp/check_"+str(os.getpid())+".slugsreturn"
slugsErrorFile = "/tmp/check_"+str(os.getpid())+".slugserr"

# =====================================================
# Slugs File reader - keeps everything line-by-line
# =====================================================
def readSlugsFile(slugsFile):
    specFile = open(slugsFile,"r")
    mode = ""
    lines = {"[ENV_TRANS]":[],"[ENV_INIT]":[],"[INPUT]":[],"[OUTPUT]":[],"[SYS_TRANS]":[],"[SYS_INIT]":[],"[ENV_LIVENESS]":[],"[SYS_LIVENESS]":[] }

    for line in specFile.readlines():
        line = line.strip()
        print >>sys.stderr, line
        if line == "":
            pass
        elif line.startswith("["):
            mode = line
            # if not mode in lines:
            #    lines[mode] = []
        else:
            if mode=="" and line.startswith("#"):
                # Initial comments
                pass
            else:
                lines[mode].append(line)
    specFile.close()
    return lines

# =====================================================
# Slugs File Writer - keeps everything line-by-line
# Remove emptylines at the end
# =====================================================
def writeSlugsFile(slugsFile,lines):
    specFile = open(slugsFile,"w")
    for a in ["[INPUT]","[OUTPUT]","[ENV_TRANS]","[ENV_INIT]","[SYS_TRANS]","[SYS_INIT]","[ENV_LIVENESS]","[SYS_LIVENESS]"]:
        specFile.write(a+"\n")
        for c in lines[a]:
            specFile.write(c+"\n")
        specFile.write("\n")
    specFile.close()

# =====================================================
# Custom exception
# =====================================================
class SlugsException(Exception):
    pass


# =====================================================
# Realizability Checker
# =====================================================
def checkRealizability(inputFile):

    # =====================================================
    # Compile to a structured Slugs specification
    # =====================================================
    command = slugsCompilerAndBasicOptions + " "+inputFile+" > "+slugsCompiledFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "================================================"
        print >>sys.stderr, "Slugs compilation failed!"
        print >>sys.stderr, "================================================\n"
        with open(slugsErrorFile,"r") as errorFile:
            for line in errorFile.readlines():
                sys.stderr.write(line)
        raise SlugsException("Could not build report")

    command = slugsExecutableAndBasicOptions + " "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    realizable = None
    with open(slugsErrorFile,"r") as f:
        for line in f.readlines():
            if line.startswith("RESULT: Specification is realizable."):
                realizable = True
            elif line.startswith("RESULT: Specification is unrealizable."):
                realizable = False
    if realizable==None:
        print >>sys.stderr, "Error: slugs was unable to determine the realizability of the specification."
        raise SlugsException("Fatal error")
    return realizable


# =====================================================
# Main function
# =====================================================
def analyzeStuckAtConstant(slugsFile):

    # =====================================================
    # Read the structured input specification
    # =====================================================
    originalSlugsFile = readSlugsFile(slugsFile)    
    
    # =====================================================
    # Check for realizability once
    # =====================================================
    isRealizable = checkRealizability(slugsFile)
    
    print "Starting point is",
    if isRealizable:
        print "a realizable",
        (categoryA,categoryB,categoryC,text) = ("[OUTPUT]","[SYS_TRANS]","[SYS_INIT]","output signal")
    else:
        print "an unrealizable",
        (categoryA,categoryB,categoryC,text) = ("[INPUT]","[ENV_TRANS]","[ENV_INIT]","input signal")
    print "specification"

    # Going through the inputs and outputs
    for line in originalSlugsFile[categoryA]:

        line = line.strip()
        if not line.startswith("#"):

            # See if the fixing the current input or output makes a difference:
            nonDifferenceCausingValues = []

            # Numeric variables
            if ":" in line:
                parts = line.split(":")
                parts = [a.strip() for a in parts]
                if len(parts)!=2:
                    print >>sys.stderr, "Error reading line '"+line+"' in section "+variableType+": Too many ':'s!"
                    raise Exception("Failed to translate file.")
                parts2 = parts[1].split("...")
                if len(parts2)!=2:
                    print >>sys.stderr, "Error reading line '"+line+"' in section "+variableType+": Syntax should be name:from...to, where the latter two are numbers"
                    raise Exception("Failed to translate file.")
                try:
                    minValue = int(parts2[0])
                    maxValue = int(parts2[1])
                except ValueError:
                    print >>sys.stderr, "Error reading line '"+line+"' in section "+variableType+": the minimal and maximal values are not given as numbers"
                    raise Exception("Failed to translate file.")
                if minValue>maxValue:
                    print >>sys.stderr, "Error reading line '"+line+"' in section "+variableType+": the minimal value should be smaller than the maximum one (or at least equal)"
                    raise Exception("Failed to translate file.")
                
                # Fill the dictionaries numberAPLimits, translatedNames with information
                variable = parts[0]
                
                # Go through all values
                for value in xrange(minValue,maxValue+1):
                    thisSpec = copy.deepcopy(originalSlugsFile)
                    thisSpec[categoryB].append(variable+"'="+str(value))
                    thisSpec[categoryC].append(variable+"="+str(value))
    
                    writeSlugsFile(slugsModifiedFile,thisSpec)
                    if (checkRealizability(slugsModifiedFile) == isRealizable):
                        nonDifferenceCausingValues.append(str(value))
            else:
                variable = line.strip()
                for value in [False,True]:
                    thisSpec = copy.deepcopy(originalSlugsFile)
                    if value:
                        thisSpec[categoryB].append(variable+"'")
                        thisSpec[categoryC].append(variable)
                    else:
                        thisSpec[categoryB].append("! "+variable+"'")
                        thisSpec[categoryC].append("! "+variable)
                    writeSlugsFile(slugsModifiedFile,thisSpec)
                    changing = (checkRealizability(slugsModifiedFile) == isRealizable)
                    if changing:
                        nonDifferenceCausingValues.append(str(value))
            
            if len(nonDifferenceCausingValues)==0:
                print "Fixing the value of the "+text+" "+variable+" changes that."
            else:
                print "Fixing the value of the "+text+" "+variable+" to ",
                for i,x in enumerate(nonDifferenceCausingValues):
                    if i>0:
                        if len(nonDifferenceCausingValues)<=2:
                            sys.stdout.write(" ")
                        else:
                            sys.stdout.write(", ")
                    if (i==len(nonDifferenceCausingValues)-1) and i>0:
                        sys.stdout.write("or ")
                    sys.stdout.write(x)
                print " does not change this fact."



# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)>1:
        slugsFile = sys.argv[1]
    else:
        print >>sys.stderr,"Error: Expected non-incremental slugs file name as input."
        sys.exit(1)

    try:
        analyzeStuckAtConstant(slugsFile)
    except SlugsException,e:
        sys.exit(1)

