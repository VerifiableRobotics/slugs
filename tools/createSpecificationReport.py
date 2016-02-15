#!/usr/bin/python
#

import random
import os
import sys
import subprocess, md5, time, cgi

# =====================================================
# Compute paths
# =====================================================
slugsExecutableAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("createSpecificationReport.py")]+"../src/slugs"
slugsCompilerAndBasicOptions = sys.argv[0][0:sys.argv[0].rfind("createSpecificationReport.py")]+"StructuredSlugsParser/compiler.py --thorougly"
kResilienceChecker = sys.argv[0][0:sys.argv[0].rfind("createSpecificationReport.py")]+"kResilientRealizabilityChecker.py"
uniformBoundedLengthCounterStrategyTool = sys.argv[0][0:sys.argv[0].rfind("createSpecificationReport.py")]+"computeBoundedLengthUniformSafetyCounterStrategy.py"
analyzeStuckAtConstantTool = sys.argv[0][0:sys.argv[0].rfind("createSpecificationReport.py")]+"analyzeStuckAtConstant.py"

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
# Main function
# =====================================================
def createSpecificationReport(slugsFile):

    # =====================================================
    # Compile to a structured Slugs specification
    # =====================================================
    command = slugsCompilerAndBasicOptions + " "+slugsFile+" > "+slugsCompiledFile+" 2> "+slugsErrorFile
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

    # =====================================================
    # Make HTML Barebone
    # =====================================================
    print "<!DOCTYPE html>\n<html>\n<head><title>Specification report for input file "+cgi.escape(slugsFile)+"</title>"
    print "<style media=\"all\" type=\"text/css\">"
    print "body{font-family:\"Verdana\", Verdana, serif;}"
    print "h1{ text-align: center; }"
    print "summary {"
    print "     font-size: 150%;"
    print "}"
    print "pre { white-space: pre-wrap; word-wrap: break-word; }"
    print ".details {"
    print "      margin-left: 30px;"
    print "}"
    print "</style>"
    print "</head>"
    print "<body>"

    # =====================================================
    # Make a page header
    # =====================================================
    print "<H1>Slugs specification analysis report for file <I>"+cgi.escape(slugsFile).replace("/","/<wbr/>")+"</I></H1><HR/>"

    # =====================================================
    # Specification
    # =====================================================
    print "<details>"
    print "<summary>1. Specification</summary>"
    print "<pre class=\"details\">"
    nofEmptyLines = -10000
    with open(slugsFile,"r") as f:
        abortedCopying = False
        for line in f.readlines():
            if not abortedCopying:
                if line.strip()=="":
                    nofEmptyLines+=1
                elif line.strip()=="# --- End of the specification for the report":
                    abortedCopying = True
                else:
                    for i in xrange(0,nofEmptyLines):
                        print ""
                    nofEmptyLines = 0
                    print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================
    # Print Realizability report
    # =====================================================
    print "<details>"
    print "<summary>2. Realizability</summary>"
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
    if realizable:
        print "<p>The specification is <B>realizable</B>.</p>"
        sys.stdout.flush()
        # Also check special robotics semantics        
        command = slugsExecutableAndBasicOptions + " --sysInitRoboticsSemantics "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
        print >>sys.stderr, "Executing: "+command
        retValue = os.system(command)
        if (retValue!=0):
            print >>sys.stderr, "Slugs failed!"
            raise Exception("Could not build report")
        realizableRobotics = None
        with open(slugsErrorFile,"r") as f:
            for line in f.readlines():
                if line.startswith("RESULT: Specification is realizable."):
                    realizableRobotics = True
                elif line.startswith("RESULT: Specification is unrealizable."):
                    realizableRobotics = False
        if realizableRobotics==None:
            print >>sys.stderr, "Error: slugs was unable to determine the special robotics-semantics realizability of the specification."
            raise SlugsException("Fatal error")
        if realizableRobotics:
            print "<p>The specification is also <B>realizable in the robotics semantics</B>.</p>"
        else:
            print "<p>However, the specification is <B>unrealizable in the robotics semantics</B>.</p>"
    else:
        print "<p>The specification is <B>unrealizable</B>!</p>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================
    # Winning positions
    # =====================================================
    print "<details>"
    print "<summary>3. Assumption/Guarantee interaction analysis</summary>"
    print "<pre class=\"details\">"
    command = slugsExecutableAndBasicOptions + " --analyzeSafetyLivenessInteraction "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    lengthOfStrategyToDriveTheOtherPlayerIntoADeadEnd = None
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
            if line.strip().startswith("Note that with only the safety assumptions and guarantees, the environment can falsify the system's specification within "):
                lengthOfStrategyToDriveTheOtherPlayerIntoADeadEnd = int(line.strip().split(" ")[17])
            if line.strip().startswith("Note that with only the safety assumptions and guarantees, the system can falsify the environment's specification within "):
                lengthOfStrategyToDriveTheOtherPlayerIntoADeadEnd = int(line.strip().split(" ")[17])
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================
    # Winning positions
    # =====================================================
    print "<details>"
    print "<summary>4. Winning States/Positions</summary>"
    print "<pre class=\"details\">"
    command = slugsExecutableAndBasicOptions + " --analyzeInitialPositions "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================
    # Winning positions that can falsify the environment
    # =====================================================
    print "<details>"
    print "<summary>5. States/Positions that are winning by environment falsification</summary>"
    print "<pre class=\"details\">"
    slugsLines = readSlugsFile(slugsCompiledFile)
    slugsLines["[SYS_LIVENESS]"] = ["0"]
    writeSlugsFile(slugsModifiedFile,slugsLines)    
    command = slugsExecutableAndBasicOptions + " --analyzeInitialPositions "+slugsModifiedFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================================================
    # Winning positions that can falsify the environment - Restricted to the reachable ones
    # =====================================================================================
    print "<details>"
    print "<summary>5b. States/Positions that are winning by environment falsification (reachable states only)</summary>"
    print "<pre class=\"details\">"
    slugsLines = readSlugsFile(slugsCompiledFile)
    slugsLines["[SYS_LIVENESS]"] = ["0"]
    writeSlugsFile(slugsModifiedFile,slugsLines)    
    command = slugsExecutableAndBasicOptions + " --analyzeInitialPositions --restrictToReachableStates "+slugsModifiedFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()


    # =====================================================
    # Superfluous assumptions
    # =====================================================
    print "<details>"
    print "<summary>6. Which assumptions are actually needed?</summary>"
    print "<pre class=\"details\">"
    command = slugsExecutableAndBasicOptions + " --analyzeAssumptions "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================
    # Example trace of the system
    # =====================================================
    print "<details>"
    print "<summary>7. Example trace of the system</summary>"
    print "<pre class=\"details\">"
    command = slugsExecutableAndBasicOptions + " --computeInterestingRunOfTheSystem "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs failed!"
        raise Exception("Could not build report")
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # =====================================================
    # Computing an abstract trace-like strategy that
    # wins in finite time
    # =====================================================
    if (lengthOfStrategyToDriveTheOtherPlayerIntoADeadEnd!=None):
        print "<details>"
        print "<summary>7b. Quasi-uniform strategy for the winning player to win in finite time</summary>"
        print "<pre class=\"details\">"
        # Note that in the following command, we have to add +1 because the number of states in a trace ist the number of transitions plus 1
        command = slugsExecutableAndBasicOptions + " --computeAbstractWinningTrace "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
        print >>sys.stderr, "Executing: "+command
        retValue = os.system(command)
        if (retValue!=0):
            print >>sys.stderr, uniformBoundedLengthCounterStrategyTool+" failed!"
            raise Exception("Could not build report")
        with open(slugsReturnFile,"r") as f:
            for line in f.readlines():
                print cgi.escape(line),
        print "</pre>"
        print "</details>"
        sys.stdout.flush()

    # =====================================================
    # Error resilience analysis
    # =====================================================
    print "<details>"
    print "<summary>8. The Effect of Stuck-at-0/1 Faults</summary>"
    print "<pre class=\"details\">"
    command = analyzeStuckAtConstantTool+" "+slugsFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
    print >>sys.stderr, "Executing: "+command
    retValue = os.system(command)
    if (retValue!=0):
        print >>sys.stderr, "Slugs/analyzeStuckAtConstantTool failed!"
        raise Exception("Could not build report")
    with open(slugsReturnFile,"r") as f:
        for line in f.readlines():
            print cgi.escape(line),
    print "</pre>"
    print "</details>"
    sys.stdout.flush()

    # ==============================================================================
    # From here onwards: analyses that only make sense for realizable specifications
    # ==============================================================================
    if realizable:

        # =====================================================
        # Input/Output Signal Order analysis
        # =====================================================
        print "<details>"
        print "<summary>9. Input/Output Signal Analysis</summary>"
        print "<pre class=\"details\">"
        command = slugsExecutableAndBasicOptions + " --analyzeInterleaving "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
        print >>sys.stderr, "Executing: "+command
        retValue = os.system(command)
        if (retValue!=0):
            print >>sys.stderr, "Slugs/analyzeStuckAtConstantTool failed!"
            raise Exception("Could not build report")
        with open(slugsReturnFile,"r") as f:
            for line in f.readlines():
                print cgi.escape(line),
        print "</pre>"
        print "</details>"
        sys.stdout.flush()   

        # =====================================================
        # Error resilience analysis
        # =====================================================
        print "<details>"
        print "<summary>10. Achievable Error-resilience levels</summary>"
        print "<pre class=\"details\">"
        command = kResilienceChecker+" "+slugsCompiledFile+" > "+slugsReturnFile+" 2> "+slugsErrorFile
        print >>sys.stderr, "Executing: "+command
        retValue = os.system(command)
        if (retValue!=0):
            print >>sys.stderr, "Slugs/kRelienceChecker failed!"
            raise Exception("Could not build report")
        with open(slugsReturnFile,"r") as f:
            for line in f.readlines():
                print cgi.escape(line),
        print "</pre>"
        print "</details>"
        sys.stdout.flush()

    # =====================================================
    # Close up HTML File
    # =====================================================
    print "</body></html>"
    sys.stdout.flush()
    

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
        createSpecificationReport(slugsFile)
    except SlugsException,e:
        sys.exit(1)

