#!/usr/bin/python
#

import random
import os
import sys
import subprocess, md5, time

# =====================================================
# Main function
# =====================================================
def translateIncrementalSlugsFileToSequenceOfMonolithicSlugsFiles(slugsFile):
    filePrefix = slugsFile[0:slugsFile.rindex(".")]
    incrementalFile = open(slugsFile,"r")
    incrementalLines = incrementalFile.readlines()
    incrementalFile.close()
    numberInSequence = 0

    categories = {"INPUT":[],"OUTPUT":[],"ENV_INIT":{},"ENV_LIVENESS":{},"ENV_TRANS":{},"SYS_INIT":{},"SYS_LIVENESS":{},"SYS_TRANS":{}}
    for line in incrementalLines:
        line = line.strip()
        if line.startswith("#"):
            pass # Comment
        elif len(line)==0:
            pass # Nothing to do
        elif line.startswith("EXITONERROR"):
            pass # Again, nothing to do
        elif line.startswith("INPUT "):
            apName = line[6:].strip()
            categories["INPUT"].append(apName)
        elif line.startswith("OUTPUT "):
            apName = line[6:].strip()
            categories["OUTPUT"].append(apName)
        elif line.startswith("ALA "):
            line = line[4:]
            name = line[0:line.find(" ")].strip()
            chain = line[line.find(" "):].strip()
            categories["ENV_LIVENESS"][name]=chain
        elif line.startswith("ASA "):
            line = line[4:]
            name = line[0:line.find(" ")].strip()
            chain = line[line.find(" "):].strip()
            categories["ENV_TRANS"][name]=chain
        elif line.startswith("AIA "):
            line = line[4:]
            name = line[0:line.find(" ")].strip()
            chain = line[line.find(" "):].strip()
            categories["ENV_INIT"][name]=chain
        elif line.startswith("ALG "):
            line = line[4:]
            name = line[0:line.find(" ")].strip()
            chain = line[line.find(" "):].strip()
            categories["SYS_LIVENESS"][name]=chain
        elif line.startswith("ASG "):
            line = line[4:]
            name = line[0:line.find(" ")].strip()
            chain = line[line.find(" "):].strip()
            categories["SYS_TRANS"][name]=chain
        elif line.startswith("AIG "):
            line = line[4:]
            name = line[0:line.find(" ")].strip()
            chain = line[line.find(" "):].strip()
            categories["SYS_INIT"][name]=chain
        elif line=="CR":
            numberInSequence += 1
            outFile = open(filePrefix+"_"+str(numberInSequence)+".slugsin","w")
            for c in ["INPUT","OUTPUT"]:
                outFile.write("["+c+"]\n")
                for a in categories[c]:
                    outFile.write(a+"\n")
                outFile.write("\n")
            for c in ["ENV_INIT","ENV_LIVENESS","ENV_TRANS","SYS_INIT","SYS_LIVENESS","SYS_TRANS"]:
                outFile.write("["+c+"]\n")
                for (a,b) in categories[c].iteritems():
                    outFile.write(b+"\n")
                outFile.write("\n")
            outFile.close()
        else:
            print >>sys.stderr,"Error: Cannot interpret line in the incremental synthesis benchmark: "+line
            raise

    


# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)>1:
        slugsFile = sys.argv[1]
    else:
        print >>sys.stderr,"Error: Expected incremental slugs file name as input."
        sys.exit(1)



    translateIncrementalSlugsFileToSequenceOfMonolithicSlugsFiles(slugsFile)

