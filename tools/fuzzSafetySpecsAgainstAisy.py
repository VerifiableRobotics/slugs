#!/usr/bin/python
#
# Does FUZZ-Tests on safety specifications to compare against "Aisy", a
# safety synthesis tool for input specifications in AIG format, as
# used in the reactive synthesis competition (www.syntcomp.org)
#

import random
import os
import sys
import subprocess, md5, time

# =====================================================
# Difficulty Options
# =====================================================
nofAPsPerType = 5
probabilityPerAssumptionProperty = 0.75
probabilityPerGuaranteeProperty = 0.9

# =====================================================
# Execution Options
# =====================================================
slugsFile = "/tmp/fuzz_"+str(os.getpid())+".slugsin"
aagFile = "/tmp/fuzz_"+str(os.getpid())+".aag"
aigFile = "/tmp/fuzz_"+str(os.getpid())+".aig"
slugsResultFile = "/tmp/fuzz_"+str(os.getpid())+".slugsResultFile"
slugsErrorFile = "/tmp/fuzz_"+str(os.getpid())+".slugsErrorFile"
abcErrorFile = "/tmp/fuzz_"+str(os.getpid())+".abcErrorFile"
aisyResultFile = "/tmp/fuzz_"+str(os.getpid())+".aisyResultFile"
aisyErrorFile = "/tmp/fuzz_"+str(os.getpid())+".aisyErrorFile"
slugsExecutableAndBasicOptions = "../src/slugs "
translaterExecutable = "./translateSafetySlugsSpecToSyntCompAIGFormat.py"
aisyExecutableAndBasicOptions = "$AISY"
aigtoaigExecutable = "$AIGTOAIG"
abcExecutable = "$ABC"

# =====================================================
# =====================================================
# Main function for doing one fuzz test.
# =====================================================
# =====================================================
def fuzzOnce():

    # Recursive specification generator
    def makeAProperty(apsUsable):
        ops = ["!","&","|","AP","$","AP","TRUE"]
        op = random.choice(ops)
        while ((op=="next") and (len(apsUsableWithNext)==0)):
            op = random.choice(ops)

        if (op=="!"):
            return "! "+makeAProperty(apsUsable)
        elif (op=="&"):
            return "& "+makeAProperty(apsUsable)+" "+makeAProperty(apsUsable)
        elif (op=="|"):
            return "| "+makeAProperty(apsUsable)+" "+makeAProperty(apsUsable)
        elif (op=="AP"):
            return random.choice(apsUsable)
        elif (op=="$") and not "? 0" in apsUsable:
            nofElements = random.randint(1,5)
            allParts = "$ "+str(nofElements)
            aps = [a for a in apsUsable]
            for i in xrange(0,nofElements):
                allParts = allParts + " "+makeAProperty(aps)
                aps.append("? "+str(i))
            return allParts

        # Nothing else? Then it shall be "TRUE"
        return "1"

    # Create AP lists
    apsInput = ["in"+str(i) for i in xrange(0,nofAPsPerType)]
    apsOutput = ["out"+str(i) for i in xrange(0,nofAPsPerType)]
    apsInputPost = [a + "'" for a in apsInput]
    apsOutputPost = [a + "'" for a in apsOutput]

    # Init assumptions
    properties =  {"[INPUT]":apsInput,"[OUTPUT]":apsOutput,"[ENV_INIT]":[],"[ENV_TRANS]":[],"[SYS_INIT]":[],"[SYS_TRANS]":[]} 
    while random.random() < probabilityPerAssumptionProperty:
        properties["[ENV_TRANS]"].append(makeAProperty(apsInput+apsOutput+apsInputPost))
    while random.random() < probabilityPerAssumptionProperty:
        properties["[ENV_INIT]"].append(makeAProperty(apsInput))
    while random.random() < probabilityPerGuaranteeProperty:
        properties["[SYS_INIT]"].append(makeAProperty(apsOutput))
    while random.random() < probabilityPerGuaranteeProperty:
        properties["[SYS_TRANS]"].append(makeAProperty(apsInput+apsOutput+apsInputPost+apsOutputPost))

    # Write input file
    out = open(slugsFile,"w")
    for a in ["[INPUT]","[OUTPUT]","[ENV_INIT]","[ENV_TRANS]","[SYS_INIT]","[SYS_TRANS]"]:
        out.write(a+"\n")
        for x in properties[a]:
            out.write(x+"\n")
        out.write("\n")
    out.close()

    # =====================================================
    # Run incremental synthesis
    # =====================================================
    retValue = os.system(slugsExecutableAndBasicOptions + " " + slugsFile+" > "+slugsResultFile+" 2> "+slugsErrorFile)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # Examine result
    # =====================================================
    slugsReturn = open(slugsErrorFile,"r")
    monolithicResult = None
    for line in slugsReturn.readlines():
        if line.strip() == "RESULT: Specification is unrealizable.":
            monolithicResult = False
        elif line.strip() == "RESULT: Specification is realizable.":
            monolithicResult = True
    slugsReturn.close()

    if (monolithicResult==None):
        print >>sys.stderr, "Error: Slugs (monolithic) did not return some result"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # Translate
    # =====================================================
    retValue = os.system(translaterExecutable + " " + slugsFile+" > "+aagFile)
    if (retValue!=0):
        print >>sys.stderr, "Translation failed!"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # to Binary AIG
    # =====================================================
    retValue = os.system(aigtoaigExecutable + " " + aagFile +" "+aigFile)
    if (retValue!=0):
        print >>sys.stderr, "Translation Part 2 failed!"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # Optimize
    # =====================================================
    retValue = os.system(abcExecutable + " -c \"read_aiger "+aigFile+"; rewrite; write_aiger -s "+aigFile+"\" > "+abcErrorFile)
    if (retValue!=0):
        print >>sys.stderr, "Translation Part 3 failed!"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # to ASCII AIG
    # =====================================================
    retValue = os.system(aigtoaigExecutable + " " + aigFile +" "+aagFile)
    if (retValue!=0):
        print >>sys.stderr, "Translation Part 2 failed!"
        raise Exception("Fuzzing Aborted")

    # =====================================================
    # Run AISY
    # =====================================================
    command = aisyExecutableAndBasicOptions + " " + aagFile+" > "+aisyResultFile+" 2> "+aisyErrorFile
    retValue = os.system(command)
    EXIT_STATUS_REALIZABLE = 10
    EXIT_STATUS_UNREALIZABLE = 20
    if (retValue/256==EXIT_STATUS_REALIZABLE):
        if monolithicResult==False:
            print >>sys.stderr, "Error: Aisy sais realizable, Slugs disagrees!"
            raise Exception("Fuzzing Aborted")
        else:
            sys.stdout.write("+")

    elif (retValue/256==EXIT_STATUS_UNREALIZABLE):
        if monolithicResult==True:
            print >>sys.stderr, "Error: Aisy sais unrealizable, Slugs disagrees!"
            raise Exception("Fuzzing Aborted")
        else:
            sys.stdout.write("-")
    else:
        print >>sys.stderr, "Aisy failed!"
        print >>sys.stderr, "Error code: "+str(retValue)
        raise Exception("Fuzzing Aborted")


# =====================================================
# Run as main program
# =====================================================
if __name__ == "__main__":

    # Take random seed from the terminal if it exists, otherwise make up one
    if len(sys.argv)>1:
        seed = sys.argv[1]
    else:
        seed = md5.md5(time.ctime()+str(os.getpid())).digest().encode("hex")
    print "Fuzzing Slugs against aisy. Process ID: "+str(os.getpid())+", Seed: "+seed
    random.seed(seed)

    # Start fuzzing    
    while True:
        fuzzOnce()
        sys.stdout.flush()
