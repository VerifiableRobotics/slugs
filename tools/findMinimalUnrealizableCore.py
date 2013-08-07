#!/usr/bin/python
#

import random
import os
import sys
import subprocess, md5, time

# =====================================================
# Option configuration
# =====================================================
slugsExecutableAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("tools/findMinimalUnrealizableCore.py")]+"src/slugs"
slugsReturnFile = "/tmp/check_"+str(os.getpid())+".slugsreturn"


# =====================================================
# Main function
# =====================================================
def isRealizable():
    # =====================================================
    # Slugs
    # =====================================================
    command = slugsExecutableAndBasicOptions + " --onlyRealizability /tmp/core.slugsin 2> "+slugsReturnFile
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Fuzzing Aborted")

    realizable = None
    slugsReturn = open(slugsReturnFile,"r")
    for line in slugsReturn.readlines():
        if line.strip() == "RESULT: Specification is unrealizable.":
            realizable = False
        elif line.strip() == "RESULT: Specification is realizable.":
            realizable = True
    slugsReturn.close()

    if realizable == None:
        print >>sys.stderr, "Slugs did not tell us if the specification is realizable or not!"
        raise Exception("Fuzzing Aborted")
    return realizable



def findMinimalCore(slugsFile):
    filePrefix = slugsFile[0:slugsFile.rindex(".")]
    monolithicFile = open(slugsFile,"r")
    monolithicLines = monolithicFile.readlines()
    monolithicFile.close()
    
    # Take care of input and output
    categories = {"[INPUT]":[],"[OUTPUT]":[],"[ENV_INIT]":[],"[ENV_LIVENESS]":[],"[ENV_TRANS]":[],"[SYS_INIT]":[],"[SYS_LIVENESS]":[],"[SYS_TRANS]":[]}
    currentCategory = ""
    lastCR = False
    propNr = 1
    for line in monolithicLines:
        line = line.strip()
        if line.startswith("#"):
            pass
        elif line.startswith("["):
            currentCategory = line
        elif len(line)>0:
            categories[currentCategory].append(line)


    lineNr = 0
    while lineNr < len(categories["[SYS_LIVENESS]"]):
        print "Live: ",lineNr,"of",len(categories["[SYS_LIVENESS]"])
        coreFile = open("/tmp/core.slugsin","w")
        for a in ["[INPUT]","[OUTPUT]","[ENV_INIT]","[ENV_LIVENESS]","[ENV_TRANS]","[SYS_INIT]","[SYS_TRANS]"]:
            coreFile.write(a+"\n")
            for line in categories[a]:
                coreFile.write(line+"\n")
        coreFile.write("[SYS_LIVENESS]\n")
        for i in xrange(0,len(categories["[SYS_LIVENESS]"])):
            if i!=lineNr:
                coreFile.write(categories["[SYS_LIVENESS]"][i]+"\n")
        coreFile.close()
        if not isRealizable():
            categories["[SYS_LIVENESS]"] = categories["[SYS_LIVENESS]"][0:lineNr] + categories["[SYS_LIVENESS]"][lineNr+1:]
        else:
            lineNr += 1

    lineNr = 0
    while lineNr < len(categories["[SYS_TRANS]"]):
        print "Safe: ",lineNr,"of",len(categories["[SYS_TRANS]"])
        coreFile = open("/tmp/core.slugsin","w")
        for a in ["[INPUT]","[OUTPUT]","[ENV_INIT]","[ENV_LIVENESS]","[ENV_TRANS]","[SYS_INIT]","[SYS_LIVENESS]"]:
            coreFile.write(a+"\n")
            for line in categories[a]:
                coreFile.write(line+"\n")
        coreFile.write("[SYS_TRANS]\n")
        for i in xrange(0,len(categories["[SYS_TRANS]"])):
            if i!=lineNr:
                coreFile.write(categories["[SYS_TRANS]"][i]+"\n")
        coreFile.close()
        if not isRealizable():
            categories["[SYS_TRANS]"] = categories["[SYS_TRANS]"][0:lineNr] + categories["[SYS_TRANS]"][lineNr+1:]
        else:
            lineNr += 1

    # Done!
    coreFile = open("/tmp/core.slugsin","w")
    for a in ["[INPUT]","[OUTPUT]","[ENV_INIT]","[ENV_LIVENESS]","[ENV_TRANS]","[SYS_INIT]","[SYS_LIVENESS]","[SYS_TRANS]"]:
        coreFile.write(a+"\n")
        for line in categories[a]:
            coreFile.write(line+"\n")
        coreFile.write("\n")
    coreFile.close()
            


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



    findMinimalCore(slugsFile)

