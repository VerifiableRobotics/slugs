#!/usr/bin/python
#
# Interactive Strategy Simulator and Debugger for Slugs
import curses, sys, subprocess, time


# ==================================
# Scenario params
# ==================================
MIN_INFLOW = 3
MAX_INFLOW = 5
MIN_OUTFLOW = 0
MAX_OUTFLOW = 3


# ==================================
# Check parameters
# ==================================
if len(sys.argv)<2:
    print >>sys.stderr, "Error: Need Slugsin file as parameter. Additional parameters are optional"
    sys.exit(1)
specFile = " ".join(sys.argv[1:])


# ==================================
# Start slugs
# ==================================
slugsLink = sys.argv[0][0:sys.argv[0].rfind("watertank.py")]+"../../src/slugs"
slugsProcess = subprocess.Popen(slugsLink+" --interactiveStrategy "+specFile+" 2>/dev/null", shell=True, bufsize=1048000, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

# Get input APs
slugsProcess.stdin.write("XPRINTINPUTS\n")
slugsProcess.stdin.flush()
slugsProcess.stdout.readline() # Skip the prompt
lastLine = " "
inputAPs = []
while (lastLine!=""):
    lastLine = slugsProcess.stdout.readline().strip()
    if lastLine!="":
        inputAPs.append(lastLine)

# Get output APs
slugsProcess.stdin.write("XPRINTOUTPUTS\n")
slugsProcess.stdin.flush()
slugsProcess.stdout.readline() # Skip the prompt
lastLine = " "
outputAPs = []
while (lastLine!=""):
    lastLine = slugsProcess.stdout.readline().strip()
    if lastLine!="":
        outputAPs.append(lastLine)





# ==================================
# Parse input and output bits into structured form
# ==================================
structuredVariables = []
structuredVariablesBitPositions = []
structuredVariablesMin = []
structuredVariablesMax = []
structuredVariablesIsOutput = []
for (isOutput,source,startIndex) in [(False,inputAPs,0),(True,outputAPs,len(inputAPs))]:
    for i,a in enumerate(source):
        if "@" in a:
            # is Structured
            (varName,suffix) = a.split("@")
            if "." in suffix:
                # Is a master variable
                (varNum,minimum,maximum) = suffix.split(".")
                assert varNum=="0"
                structuredVariables.append(varName)    
                structuredVariablesBitPositions.append({0:i+startIndex})
                structuredVariablesMin.append(int(minimum))
                structuredVariablesMax.append(int(maximum))
                structuredVariablesIsOutput.append(isOutput)
            else:
                # Is a slave variable
                indexFound = False
                for j,b in enumerate(structuredVariables):
                    if b==varName:
                        indexFound=j
                if indexFound==None:
                    print >>sys.stderr,"Error in input instance: Master variables have to occur before the slave variables in the input file.\n"
                    sys.exit(1)
                assert structuredVariablesIsOutput[indexFound]==isOutput
                structuredVariablesBitPositions[indexFound][int(suffix)] = i+startIndex
        else:
            # is Unstructured
            structuredVariables.append(a)    
            structuredVariablesBitPositions.append({0:i+startIndex})
            structuredVariablesMin.append(0)
            structuredVariablesMax.append(1)
            structuredVariablesIsOutput.append(isOutput)



# =======================================
# Check that all input symbols make sense
# =======================================
for i in range(0,len(structuredVariables)):
    if not structuredVariablesIsOutput[i]:
        if not structuredVariables[i] in ["waterin","waterout","keeplow"]:
            print "The simulator can't work with the additional input '"+structuredVariables[i]+"'"
            sys.exit(1)



# =======================================
# Printer
# =======================================
print >>sys.stderr,"inputAPs:",inputAPs
print >>sys.stderr,"outputAPs:",outputAPs
print >>sys.stderr,"structuredVariables",structuredVariables
print >>sys.stderr,"structuredVariablesBitPositions",structuredVariablesBitPositions
print >>sys.stderr,"structuredVariablesMin",structuredVariablesMin
print >>sys.stderr,"structuredVariablesMax",structuredVariablesMax
print >>sys.stderr,"structuredVariablesIsOutput",structuredVariablesIsOutput


# =======================================
# Structured variable reverse lister
# =======================================
structuredVariableNameToVariableInStructuredVariableListMapper = {}
for i,a in enumerate(structuredVariables):
    structuredVariableNameToVariableInStructuredVariableListMapper[a] = i


# =======================================
# Encoder function
# =======================================
def encodeInput(waterin,waterout,keepLow):
    outString = ["." for i in range(0,len(inputAPs)+len(outputAPs))]
    for i,varName in enumerate(structuredVariables):
        if varName=="waterin":
            for (b,c) in structuredVariablesBitPositions[i].iteritems():
                if ((waterin >> b) & 1)>0:
                    outString[c] = "1"
                else:
                    outString[c] = "0"
        if varName=="waterout":
            for (b,c) in structuredVariablesBitPositions[i].iteritems():
                if ((waterout >> b) & 1)>0:
                    outString[c] = "1"
                else:
                    outString[c] = "0"
    for i,a in enumerate(inputAPs):
        if a=="keeplow":
            outString[i] = ("1" if keepLow else "0")
    return "".join(outString)


# =======================================
# Decoder function
# =======================================
def getEncodedVariable(stateLine,bitPositions):
    allSum = 0
    for (a,b) in bitPositions.iteritems():
        if stateLine[b]=="1":
            allSum += (1 << a)
    return allSum

# ==================================
# Initialize CURSES
# ==================================
stdscr = curses.initscr()
# curses.start_color()
curses.noecho()
stdscr.keypad(1)
stdscr.refresh()
try:

        
    # =======================================
    # Do simulation
    # =======================================
    doReset = True
    simulationStop = None # No error yet
    abort = False
    while not abort:
    
        # =======================================
        # Reset
        # =======================================
        if doReset:
        
            # Get initial state
            slugsProcess.stdin.write("XGETINIT\n")
            slugsProcess.stdin.flush()
            slugsProcess.stdout.readline() # Skip the prompt
            initLine = slugsProcess.stdout.readline().strip()
            currentState = initLine[0:initLine.find(",")]
            currentLivenessAssumption = 0
            currentLivenessGuarantee = 0

            inFlowValue = MIN_INFLOW
            outFlowValue = MIN_OUTFLOW
            doReset = False
            keepLow = False
            fillLevel = None
            lowmode = None
            simulationStop = None
            specFailures = set([])
            simulationRound = 0
            valveHistory = []
    
    
        # =======================================
        # Do a simulation step
        # =======================================        
        if simulationStop==None:
            
            # Whether valve is open needs to be known before the transition
            for i,a in enumerate(inputAPs+outputAPs):
                if a=="inValve":
                    isValveOpen = currentState[i]!="0"
            valveHistory.append(isValveOpen)
     
            # Set actual inflow
            actualInFlow = inFlowValue if isValveOpen else 0
            
            # Make transition
            dataForStrategyTransition = currentState+encodeInput(actualInFlow,outFlowValue,keepLow)+"\n"+str(currentLivenessAssumption)+"\n"+str(currentLivenessGuarantee)

            slugsProcess.stdin.write("XSTRATEGYTRANSITION\n"+dataForStrategyTransition+"\n")
            print >> sys.stderr, "IPRINT: "+"XSTRATEGYTRANSITION\n"+dataForStrategyTransition+"\n"
            slugsProcess.stdin.flush()
            print >> sys.stderr, "IREAD: "+slugsProcess.stdout.readline() # Skip the prompt
            positionLine = slugsProcess.stdout.readline().strip()
            print >> sys.stderr, "POSILINE: "+positionLine
            if (positionLine=="FAILASSUMPTIONS"):
                simulationStop = "Assumptions have been violated. Some assumptions must have been modelled incorrectly."            
            elif (positionLine=="FAILGUARANTEES") or (positionLine=="FORCEDNONWINNING"):
                simulationStop = "Guarantees have been violated. Should not happen."            
            else:
                currentLivenessAssumption = int(slugsProcess.stdout.readline().strip())
                currentLivenessGuarantee = int(slugsProcess.stdout.readline().strip())
                currentState = "".join(["1" if a in ['A','G','S','1'] else "0" for a in positionLine])
            print >> sys.stderr, "POSILINE: "+positionLine
            simulationRound += 1
    
        
        # =======================================
        # Parse current state
        # =======================================
        oldFillLevel = fillLevel
        oldLowMode = lowmode
        oldIsValveOpen = isValveOpen
        fillLevel = None
        lowmode = None
        for i,a in enumerate(structuredVariables):
            if (a=="waterlevel"):
                fillLevel = getEncodedVariable(currentState,structuredVariablesBitPositions[i]) #+ structuredVariablesMin[i]
            if (a=="lowmode"):
                lowmode = currentState[structuredVariablesBitPositions[i][0]]=="1"
        for i,a in enumerate(inputAPs+outputAPs):
            if a=="inValve":
                isValveOpen = currentState[i]!="0"
    
        # =======================================
        # Check if updates are OK
        # =======================================
        if oldFillLevel!=None:
            if fillLevel != oldFillLevel + actualInFlow - outFlowValue:
                simulationStop = "Tank fill level was not updated correctly."
            if (not lowmode) and oldLowMode and keepLow:
                specFailures.add("Low mode was deactivated by the controller.")
            for i in range(0,len(valveHistory)):
                if valveHistory[i:i+1] == [False,True]:
                    for j in range(1,3):
                        if valveHistory[i+j:i+1+j] == [True,False]:
                            specFailures.add("Valve opened and closed too quickly.")
                if valveHistory[i:i+1] == [True,False]:
                    for j in range(1,3):
                        if valveHistory[i+j:i+1+j] == [False,True]:
                            specFailures.add("Valve closed and opened too quickly.")

        # =======================================
        # Draw current state
        # =======================================        
        stdscr.clear()
        
        stdscr.addstr(0, 1, " ___________    __ ")
        stdscr.addstr(1, 1, "/           \\___\\/__")
        stdscr.addstr(2, 1, "|            _______")
        stdscr.addstr(3, 1, "|           /")
        stdscr.addstr(4, 1, "|           |")
        stdscr.addstr(5, 1, "|           |")
        stdscr.addstr(6, 1, "|           |")
        stdscr.addstr(7, 1, "|           |")
        stdscr.addstr(8, 1, "|           |")
        stdscr.addstr(9, 1, "|           |")
        stdscr.addstr(10,1, "|           |")
        stdscr.addstr(11,1, "\____   ____/")
        stdscr.addstr(12,1, "     \\ /    ")
        stdscr.addstr(13,1, "     | |    ")
        stdscr.addstr(14,1, "     | |    ")
        if not isValveOpen:
            stdscr.addstr(2, 17,"||")
        
        discreteLevel = int(fillLevel/100.0*36)
        for i in range(0,discreteLevel/3):
            stdscr.addstr(11-i,2,"###########")
        if (discreteLevel % 3)==1:
            stdscr.addstr(11-int(discreteLevel/3),2,"___________")
        elif (discreteLevel % 3)==2:
            stdscr.addstr(11-int(discreteLevel/3),2,"===========")
        
        
        
        (ysize,xsize) = stdscr.getmaxyx()
        
        stdscr.addstr(1, 23, "Current fill level: "+str(fillLevel))
        stdscr.addstr(2, 23, "Valve status: "+("Open" if isValveOpen else "Closed"))
        stdscr.addstr(3, 23, "Inflow: "+str(actualInFlow)+" (with valve open: "+str(inFlowValue)+")")
        stdscr.addstr(4, 23, "Outflow: "+str(outFlowValue))
        stdscr.addstr(5, 23, "Round: "+str(simulationRound))
        stdscr.addstr(6, 23, "KeepLow: "+str(keepLow))
        stdscr.addstr(7, 23, "LowMode: "+str(lowmode))
        # stdscr.addstr(8, 30, "State String: "+str(currentState))
        
        if simulationStop!=None:
            stdscr.addstr(9, 23, "Simulation error: ")
            stdscr.addstr(10,23, simulationStop)
        else:
            if len(specFailures)>0:
                stdscr.addstr(9, 23, "Specification failures:")
                for i,a in enumerate(specFailures):
                    stdscr.addstr(10+i, 23, "- "+a)
        
        # Help
        stdscr.addstr(13, 19, "Keys: (Q)uit, (R)eset, (L) KeepLow toggle")
        stdscr.addstr(14, 19, "      Up/Down: Adjust Inflow")
        stdscr.addstr(15, 19, "      Left/Right: Adjust Outflow")

        # =======================================
        # Process keys
        # =======================================
        stdscr.nodelay(True)
        moreKeys = True
        while moreKeys:
        
            c = stdscr.getch()
            if (c==curses.ERR):
                moreKeys = False
            elif c==curses.KEY_DOWN:
                inFlowValue = max(inFlowValue-1,MIN_INFLOW)
            elif c==curses.KEY_UP:
                inFlowValue = min(inFlowValue+1,MAX_INFLOW)
            elif c==curses.KEY_LEFT:
                outFlowValue = max(outFlowValue-1,MIN_OUTFLOW)
            elif c==curses.KEY_RIGHT:
                outFlowValue = min(outFlowValue+1,MAX_OUTFLOW)
            elif c==ord('q'):
                abort = True
            elif c==ord('l'):
                keepLow = not keepLow
            elif c==ord('r'):
                doReset = True
            else:
                stdscr.addstr(10, 0, "C: "+str(c))

        if not abort:
            time.sleep(0.25)


# =====================
# Cleanup Curses
# =====================
finally:
    curses.endwin()
sys.exit(0)


