#!/usr/bin/python
#

import random
import os
import sys
import subprocess, md5, time

# =====================================================
# Main function
# =====================================================
def translateMonolithicSlugsFileWithCommentsToIncrementalSlugsFile(slugsFile):
    filePrefix = slugsFile[0:slugsFile.rindex(".")]
    monolithicFile = open(slugsFile,"r")
    monolithicLines = monolithicFile.readlines()
    monolithicFile.close()
    incrementalFile = open(filePrefix+".slugsincremental","w")
    
    # Take care of input and output
    categories = {"[INPUT]":"INPUT","[OUTPUT]":"OUTPUT","[ENV_INIT]":"AIA","[ENV_LIVENESS]":"ALA","[ENV_TRANS]":"ASA","[SYS_INIT]":"AIG","[SYS_LIVENESS]":"ALG","[SYS_TRANS]":"ASG"}
    currentCategory = ""
    lastCR = False
    propNr = 1
    for line in monolithicLines:
        line = line.strip()
        if line.startswith("#"):
            incrementalFile.write(line+"\n")
            if line.startswith("#INCREMENTAL:"):
                incrementalFile.write("CR\n")
                lastCR=True
        elif line.startswith("["):
            currentCategory = line
        elif len(line)>0:
            lastCR = False
            if currentCategory in ["[INPUT]","[OUTPUT]"]:
                incrementalFile.write(categories[currentCategory]+" "+line+"\n")
            else:
                incrementalFile.write(categories[currentCategory]+" PROP"+str(propNr)+" "+line+"\n")
                propNr += 1
    if not lastCR:
        incrementalFile.write("CR\n")
    incrementalFile.close()    


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



    translateMonolithicSlugsFileWithCommentsToIncrementalSlugsFile(slugsFile)

