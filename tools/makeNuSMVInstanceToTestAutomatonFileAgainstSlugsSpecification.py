#!/usr/bin/python
#
# Takes a Slugs specification file and a finite-state machine for the controller produced by slugs, and
# produces a NuSMV specification file that checks if the automaton is correct.

import math
import os
import sys, re
import resource
import subprocess
import signal
from parser import Parser
from re import match
import StringIO


#=====================
# Check if input files are given
#=====================
if len(sys.argv)<4:
    print >>sys.stderr, "Error: Need SLUGS, AUT, and NuSMV file names as parameters"
    sys.exit(1)

slugsFileName = sys.argv[1]
autFileName = sys.argv[2]
nusmvFileName = sys.argv[3]

#=========================
# Read Slugs Specification
#=========================
slugsFile = open(slugsFileName,"r")
slugsLines = {"[INPUT]":[],"[OUTPUT]":[],"[ENV_LIVENESS]":[],"[ENV_INIT]":[],"[SYS_INIT]":[],"[SYS_TRANS]":[],"[ENV_TRANS]":[],"[SYS_LIVENESS]":[]}
currentCategory = ""
for line in slugsFile.readlines():
    line = line.strip()
    if (not line.startswith("#")) and (len(line)>0):
        if line.startswith("["):
            currentCategory = line
            if not currentCategory in slugsLines:
                print >>sys.stderr, "Error: Illegal Category in slugs line: "+currentCategory
                raise Exception("Slugs input file is invalid")
        else:
            slugsLines[currentCategory].append(line)
slugsFile.close()
          
#=========================
# Read automaton file - for simplified parsing,
# reject everything that is not *precisely* at it is output
# by slugs
#=========================
autFile = open(autFileName,"r")
automatonStates = [] # Tupels of bit valuations and next states
autLines = autFile.readlines()
autLineNumber = 0
firstLineSplitter = re.compile(r" ->|, | |<|:|>")
secondLineSplitter = re.compile(r"  +| : |, ")
while (autLineNumber < len(autLines)):
    currentLine = autLines[autLineNumber].strip()
    if len(currentLine)>0:
        lineparts = firstLineSplitter.split(currentLine)
        apValuation = {}
        if lineparts[0]!="State":
            raise Exception("Automaton file is invalid: expecting 'State'")
        if lineparts[1]!=str(len(automatonStates)):
            raise Exception("Automaton file is invalid: expecting the states to be sorted.")
        if lineparts[2]!="with":
            raise Exception("Automaton file is invalid: expecting 'with'")
        if lineparts[3]!="rank":
            raise Exception("Automaton file is invalid: expecting 'rank'")
        if lineparts[5]!="":
            raise Exception("Automaton file is invalid: expecting symbol")
        if lineparts[6]!="":
            raise Exception("Automaton file is invalid: expecting symbol")
        for i in xrange(7,len(lineparts)-1,2):
            apValuation[lineparts[i]] = int(lineparts[i+1])

        # Reading successors
        lineparts = secondLineSplitter.split(autLines[autLineNumber+1].strip())
        successors = []
        if not lineparts[0].startswith("With successors"):
            raise Exception("Automaton file is invalid: expecting 'With successors'")
        for i in xrange(1,len(lineparts)):
            successors.append(int(lineparts[i]))

        automatonStates.append((apValuation,successors))
        autLineNumber += 2
    else:
        autLineNumber += 1
autFile.close()

# Now make the NuSMV files
nuSMVFile = open(nusmvFileName,"w")
nuSMVFile.write("MODULE main\n IVAR\n")
for var in slugsLines["[INPUT]"]:
    nuSMVFile.write("  v_"+var+" : boolean;\n")
for var in slugsLines["[OUTPUT]"]:
    nuSMVFile.write("  i_"+var+" : boolean;\n")
nuSMVFile.write(" VAR\n")
for var in slugsLines["[OUTPUT]"]:
    nuSMVFile.write("  v_"+var+" : boolean;\n")
nuSMVFile.write("  state : 0.."+str(len(automatonStates))+";\n") # One additional designated initial state (Mealy->Moore semantics)

# Next state
nuSMVFile.write(" ASSIGN\n  init(state) := "+str(len(automatonStates))+";\n")  
nuSMVFile.write("  next(state) :=\n")
nuSMVFile.write("   case\n")
for prestate in xrange(0,len(automatonStates)):
    (valuation,successors) = automatonStates[prestate]
    for succ in successors:
        nuSMVFile.write("    state = "+str(prestate))
        for a in slugsLines["[INPUT]"]:
            value = automatonStates[succ][0][a]
            nuSMVFile.write(" & ")
            if value==0:
                nuSMVFile.write("!")
            nuSMVFile.write("v_"+a)
        nuSMVFile.write(" : " + str(succ)+";\n")

# Initial state transition
for prestate in xrange(0,len(automatonStates)):
    nuSMVFile.write("    state = "+str(len(automatonStates)))
    for a in slugsLines["[INPUT]"]:
        value = automatonStates[prestate][0][a]
        nuSMVFile.write(" & ")
        if value==0:
            nuSMVFile.write("!")
        nuSMVFile.write("v_"+a)
    for a in slugsLines["[OUTPUT]"]:
        value = automatonStates[prestate][0][a]
        nuSMVFile.write(" & ")
        if value==0:
            nuSMVFile.write("!")
        nuSMVFile.write("i_"+a)
    possibleStates = []
    for postState in xrange(0,len(automatonStates)):
        if automatonStates[prestate][0]==automatonStates[postState][0]:
            possibleStates.append(str(postState))
    nuSMVFile.write(" : {" + ",".join(possibleStates)+"};\n")

# Default next state
nuSMVFile.write("    -- If we are in a non-initialization state if and the environment plays some not-allowed next input, then we may move to any state next.\n")
nuSMVFile.write("    state < "+str(len(automatonStates))+" : {")
for i in xrange(0,len(automatonStates)):
    if i>0:
        nuSMVFile.write(", ")
    nuSMVFile.write(str(i));
nuSMVFile.write("};\n")
nuSMVFile.write("    -- If during initization, the environment provides us with some illegal first state, then we must stay in this initial state. Our LTL assumption at the end of this file filter this case out.\n")
nuSMVFile.write("    TRUE : "+str(len(automatonStates))+";\n")

nuSMVFile.write("   esac;\n")

# Next outputs
for outputBit in slugsLines["[OUTPUT]"]:
    nuSMVFile.write("  next(v_"+outputBit+") :=\n")
    nuSMVFile.write("   case\n")
    for prestate in xrange(0,len(automatonStates)):
        (valuation,successors) = automatonStates[prestate]
        for succ in successors:
            nuSMVFile.write("    state = "+str(prestate))
            for a in slugsLines["[INPUT]"]:
                value = automatonStates[succ][0][a]
                nuSMVFile.write(" & ")
                if value==0:
                    nuSMVFile.write("!")
                nuSMVFile.write("v_"+a)
            nuSMVFile.write(" : ")
            if (automatonStates[succ][0][outputBit]==0):
                nuSMVFile.write("FALSE")
            else:
                nuSMVFile.write("TRUE")
            nuSMVFile.write(";\n")

    # Initial state transition
    for prestate in xrange(0,len(automatonStates)):
        nuSMVFile.write("    state = "+str(len(automatonStates)))
        for a in slugsLines["[INPUT]"]:
            value = automatonStates[prestate][0][a]
            nuSMVFile.write(" & ")
            if value==0:
                nuSMVFile.write("!")
            nuSMVFile.write("v_"+a)
        for a in slugsLines["[OUTPUT]"]:
            value = automatonStates[prestate][0][a]
            nuSMVFile.write(" & ")
            if value==0:
                nuSMVFile.write("!")
            nuSMVFile.write("i_"+a)
        nuSMVFile.write(" : ")
        if (automatonStates[prestate][0][outputBit]==0):
            nuSMVFile.write("FALSE")
        else:
            nuSMVFile.write("TRUE")
        nuSMVFile.write(";\n")

    nuSMVFile.write("    TRUE : {FALSE,TRUE};\n")
    nuSMVFile.write("   esac;\n")
nuSMVFile.write("\n")

# ==================================================================
# Prepare specs
# ==================================================================
def slugsToLTLNotYetReversed(tokens):
    tokens.reverse()
    return slugsToLTL(tokens)

def slugsToLTL(reverseTokens):
    op = reverseTokens.pop()
    if (op=="|") or (op=="&"):
        part1 = slugsToLTL(reverseTokens)
        part2 = slugsToLTL(reverseTokens)
        return "(" + part1 + " "+op+" " + part2 + ")"
    if (op=="!"):
        part1 = slugsToLTL(reverseTokens)
        return "! "+part1
    if (op=="1"):
        return "TRUE";
    if (op=="0"):
        return "FALSE";

    # Atomic proposition? Translate from Mealy-type-semantics to Moore-type
    for ap in slugsLines["[INPUT]"]:
        if ap==op:
            return "v_"+ap
        if ap+"'"==op:
            return "X v_"+ap
    for ap in slugsLines["[OUTPUT]"]:
        if ap==op:
            return "X v_"+ap
        if ap+"'"==op:
            return "X X v_"+ap
    raise Exception("slugsToLTL: Could not decipger operator "+op)

def slugsInitToLTLNotYetReversed(tokens):
    tokens.reverse()
    return slugsInitToLTL(tokens)

def slugsInitToLTL(reverseTokens):
    op = reverseTokens.pop()
    if (op=="|") or (op=="&"):
        part1 = slugsInitToLTL(reverseTokens)
        part2 = slugsInitToLTL(reverseTokens)
        return "(" + part1 + " "+op+" " + part2 + ")"
    if (op=="!"):
        part1 = slugsInitToLTL(reverseTokens)
        return "! "+part1
    if (op=="1"):
        return "TRUE";
    if (op=="0"):
        return "FALSE";

    # Atomic proposition? Translate from Mealy-type-semantics to Moore-type
    for ap in slugsLines["[INPUT]"]:
        if ap==op:
            return "v_"+ap
        if ap+"'"==op:
            raise Exception("No 'next's allowed in slugsInitToLTL")
    for ap in slugsLines["[OUTPUT]"]:
        if ap==op:
            return "X v_"+ap
        if ap+"'"==op:
            raise Exception("No 'next's allowed in slugsInitToLTL")
    raise Exception("slugsInitToLTL: Could not decipger operator "+op)

allInitChecker = "("+" & ".join(["TRUE"]+[slugsInitToLTLNotYetReversed(line.split(" ")) for line in slugsLines["[SYS_INIT]"]]+[slugsInitToLTLNotYetReversed(line.split(" ")) for line in slugsLines["[ENV_INIT]"]])+")"
allSafetyAssumptions = "("+" & ".join(["TRUE"]+[slugsToLTLNotYetReversed(line.split(" ")) for line in slugsLines["[ENV_TRANS]"]])+")"
allInitAssumptions = "("+allInitChecker+" & (X state!="+str(len(automatonStates))+"))"

# Write Safety specs
for safetyGuarantee in slugsLines["[SYS_TRANS]"]:
    thisSafetyGuarantee = slugsToLTLNotYetReversed(safetyGuarantee.split(" "))
    nuSMVFile.write("LTLSPEC " + allInitAssumptions+" -> ((!"+allSafetyAssumptions+") V ("+thisSafetyGuarantee+" | ! "+allSafetyAssumptions+"));\n")

# Write Liveness specs
allLivenessAssumptions = "("+" & ".join(["TRUE"]+["G F "+slugsToLTLNotYetReversed(line.split(" ")) for line in slugsLines["[ENV_LIVENESS]"]])+")"
for livenessGuarantee in slugsLines["[SYS_LIVENESS]"]:
    thisLivenessGuarantee = slugsToLTLNotYetReversed(livenessGuarantee.split(" "))
    nuSMVFile.write("LTLSPEC (" + allInitAssumptions+" & G " + allSafetyAssumptions+" & "+allLivenessAssumptions+") -> G F "+thisLivenessGuarantee+";\n")


