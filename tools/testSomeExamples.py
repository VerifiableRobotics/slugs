#!/usr/bin/env python2
#
# Tests for some examples from the "example" directory if slugs
# computes the expected realizability/unrealizability result.

import os, sys, subprocess,tempfile

realizableBenchmarks = ["networks.slugsin","optimisticRecoveryTest.slugsin","semantics_diference.slugsin","simple_safety_example.slugsin","water_reservoir.structuredslugs","firefighting.slugsin","maximallyPermissiveTestPre.structuredslugs","maximallyPermissiveTest.structuredslugs"]
unrealizableBenchmarks = ["baby_network.slugsin","example_outermost_fixed_point_unrealizability.slugsin"]


def checkRealizability(scriptName,translatorName,parameter):
    '''
    This functions calls a script and looks out for lines stating realizability/unrealizability by Slugs.
    It returns an error message if the subprocess returned with a non-zero errorcode or if multiple results are found.
    Otherwise, the result is True or False.    
    '''
    
    # Call translator script
    deleteFileName = None
    if parameter.endswith(".structuredslugs"):
        (handle,tempfilename) = tempfile.mkstemp()
        outFile = os.fdopen(handle, "w")
        translatorProcess = subprocess.Popen(translatorName+" "+parameter.split(" ")[-1], shell=True, bufsize=1048000, stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        allLines = []
        for line in translatorProcess.stdout.readlines():
            print >>outFile,line
            allLines.append(line)
        errorCode = translatorProcess.wait()
        outFile.close()
        if (errorCode!=0):
            os.unlink(tempfilename)
            errorMessage = "Translator script terminated with a non-zero error code: "+str(errorCode)+", the messages were:"
            for line in allLines:
                errorMessage = errorMessage + "\nM:" + line
            return errorMessage
        parameter = parameter.split(" ")
        parameter = parameter[0:len(parameter)-1]
        parameter.append(tempfilename)
        parameter = " ".join(parameter)
        deleteFileName = tempfilename
    
    slugsProcess = subprocess.Popen(scriptName+" "+str(parameter), shell=True, bufsize=1048000, stdout=subprocess.PIPE,stderr=subprocess.STDOUT)

    nofRealizableLines = 0
    nofUnrealizableLines = 0
    lastLines = []

    for line in slugsProcess.stdout.readlines():
        line = line.strip()
        if len(lastLines)>100:
            lastLines = lastLines[1:]+[line]
        else:
            lastLines.append(line)
        if line=="RESULT: Specification is realizable.":
            nofRealizableLines +=1 
        elif line=="RESULT: Specification is unrealizable.":
            nofUnrealizableLines +=1 
    
    errorCode = slugsProcess.wait()
    if deleteFileName!=None:
        os.unlink(deleteFileName)
    if (errorCode!=0):
        errorMessage = "External tool or script terminated with a non-zero error code: "+str(errorCode)+"\n"+"\n".join(lastLines)
        return errorMessage
    if (nofRealizableLines+nofUnrealizableLines)>1:
        return "Found more than one realizability/unrealizability result!"
    if (nofRealizableLines+nofUnrealizableLines)==0:
        return "Found no realizability/unrealizability result!"
    if nofRealizableLines>0:
        return True
    elif nofUnrealizableLines>0:
        return False
    else:
        raise Exception("Internal error. Should not be able to happen.")


#==========================================
# Main entry point
#==========================================

# Find the folder with the examples
exampleDir = None
for directory in ["..","examples","../examples"]:
    if os.path.isdir(directory):
        exampleDir = directory
if exampleDir==None:
    print >>sys.stderr, "Error: Did not find folder with the example!"
    sys.exit(1)
slugsDir = exampleDir+"/../src/slugs"
translatorScriptDir = exampleDir+"/../tools/StructuredSlugsParser/compiler.py"


for (isRealizable,benchmarks) in [(False,unrealizableBenchmarks),(True,realizableBenchmarks)]:
    for benchmark in benchmarks:
        benchmarkDir = exampleDir+"/"+benchmark
        print >>sys.stderr, "Processing:",benchmark
        realizable = checkRealizability(slugsDir,translatorScriptDir,benchmarkDir)
        if realizable!=isRealizable:
            print >>sys.stderr, "Error: Benchmark ",benchmark," was found to be ",
            if realizable==True:
                print >>sys.stderr, "realizable",
            elif realizable==False:
                print >>sys.stderr, "unrealizable",
            else:
                print >>sys.stderr, "unknown (",realizable,")",
            print >>sys.stderr, "but it should be",
            if isRealizable:
                print >>sys.stderr, "realizable"
            else:
                print >>sys.stderr, "unrealizable"
            sys.exit(1)
                
print >>sys.stderr, "Done!"
