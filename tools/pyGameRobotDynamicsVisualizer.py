#!/usr/bin/python
#
# Animates the robot abstraction using continuous robot dynamics
#
#
# REQUIREMENTS FOR PROPER Operation:
# - The slugsin file has the state bits in inverse order. This is necessary to solve the problem that Pessoa's abstract are most-significant-bit first
# - There are multiple required files:
#   - PNG file for the workspace
#   - SLUGSIN file for the specification
#   - BDD file for the robot abstraction
#   - COLORCODING file for the simulator options
#   - ROBOTMDL file with model and parameters
#   The files all need to have the same prefix, with the exception of the BDD file, where all things standing behind dots right of the last path separator character ("/") are stripped. This allows
#   recycling the same .bdd file for many scenarios.

import math
import os
import sys, code
import resource
import subprocess
import signal
import tempfile
import copy
import itertools
import Image
import os, pygame, pygame.locals
import numpy as np

# ==================================
# Settings
# ==================================
MAGNIFY = 64

# ==================================
# Entry point
# ==================================
if len(sys.argv)<2:
    print >>sys.stderr, "Error: Need PNG file as parameter"
    sys.exit(1)
specFile = sys.argv[1]

# ==================================
# Read input image
# ==================================
import os,sys
pngfile = Image.open(specFile)
pngFileBasis = specFile[0:specFile.rfind(".png")]
# print "Size of Workspace:",pngfile.size
xsize = pngfile.size[0]
ysize = pngfile.size[1]
imageData = pngfile.getdata()
palette = pngfile.getpalette()
if (xsize>1023):
    print >>sys.stderr,"Error: Scenario is too large - not supported."
    sys.exit(1)
if (ysize>1023):
    print >>sys.stderr,"Error: Scenario is too large - not supported."
    sys.exit(1)

# Adapt size of display
pygame.init()
displayInfo = pygame.display.Info()
MAGNIFY = min(MAGNIFY,displayInfo.current_w*3/4/xsize)
MAGNIFY = min(MAGNIFY,displayInfo.current_h*3/4/ysize)

# ====================================================================
# Read input image color encoding configuration file
# ====================================================================
colorCodingInformationFile = open(pngFileBasis+".colorcoding","r").readlines()
colorCoding = [(int(a.strip().split(" ")[0]),a.strip().split(" ")[1]) for a in colorCodingInformationFile if a.strip()!=""]
colorCodingMap = {a:b for (a,b) in colorCoding}
print colorCoding

# ===================================================================
# Load the robot motion model paramters from file
# ===================================================================
robotmdlfile = pngFileBasis+".robotmdl"
robotModel = open(robotmdlfile,"r")
paramsState = False
motionStateOutputState = False
motionControlOutputState = False
systemModelState = False
modelParams = []
motionStateVars = []
motionControlVars = []
systemModel = []
for line in robotModel.readlines():
    line = line.strip()
    if line.startswith("["):
        motionStateOutputState = False
        motionControlOutputState = False
        systemModelState = False
    if line=="[MOTION STATE OUTPUT]":
        motionStateOutputState = True
    elif motionStateOutputState and len(line)>0 and not line.startswith("#"):
        motionStateVars.append(line)
    if line=="[MOTION CONTROLLER OUTPUT]":
        motionControlOutputState = True
    elif motionControlOutputState and len(line)>0 and not line.startswith("#"):
        motionControlVars.append(line)
    if line=="[SYSTEM MODEL]":
        systemModelState = True
    elif systemModelState and len(line)>0 and not line.startswith("#"):
        systemModel.append(line)
robotModel = open(robotmdlfile,"r")
motionStateParams = [[]]*len(motionStateVars)
motionControlParams = [[]]*len(motionControlVars)
for line in robotModel.readlines():
    line = line.strip()
    if line.startswith("["):
        paramsState = False
    if line=="[PARAMETERS]":
        paramsState = True
    elif paramsState and len(line)>0 and not line.startswith("#"):
        if line.startswith(('tau','eta','mu')) or line.startswith(tuple(motionStateVars)) or line.startswith(tuple(motionControlVars)):
            modelParams.append(line)
            #TODO: some params saved twice?!?
            for i,varName in enumerate(motionStateVars):
                if line.startswith(varName):
                    if len(motionStateParams[i])==0:
                        tmp = []
                    tmp.append(eval(line[line.rfind("=")+1:]))
                    motionStateParams[i] = tmp
            for i,varName in enumerate(motionControlVars):
                if line.startswith(varName):
                    if len(motionControlParams[i])==0:
                        tmp = []
                    tmp.append(eval(line[line.rfind("=")+1:]))
                    motionControlParams[i] = tmp
        else:
            print >> sys.stderr, "motion state/control variables don't match parameters in the robot model file"

if len(set(['x','y']).intersection(motionStateVars)) < 2:
    print >> sys.stderr, "x and y must be included as motion state variables in the robot model file"

# evaluate the strings in modelParams
for i in xrange(0,len(modelParams)):
    exec(modelParams[i])
for i,varName in enumerate(motionStateVars):
    while systemModel[i].rfind('sin(') >= 0 and systemModel[i][systemModel[i].rfind('sin(')-3:systemModel[i].rfind('sin(')] != 'np.':
        systemModel[i] = systemModel[i][0:systemModel[i].rfind('sin(')]+'np.'+systemModel[i][systemModel[i].rfind('sin('):]
    while systemModel[i].rfind('cos(') >= 0 and systemModel[i][systemModel[i].rfind('cos(')-3:systemModel[i].rfind('cos(')] != 'np.':
        systemModel[i] = systemModel[i][0:systemModel[i].rfind('cos(')]+'np.'+systemModel[i][systemModel[i].rfind('cos('):]
    while systemModel[i].rfind('tan(') >= 0 and systemModel[i][systemModel[i].rfind('tan(')-3:systemModel[i].rfind('tan(')] != 'np.':
        systemModel[i] = systemModel[i][0:systemModel[i].rfind('tan(')]+'np.'+systemModel[i][systemModel[i].rfind('tan('):]
print motionStateParams
tmp = [[motionStateParams[i][0]+eta/2-1e-4,motionStateParams[i][1]-eta/2+1e-4] for i in range(len(motionStateParams))]
minMaxState = map(list,zip(*tmp))  # bounds on the continuous states
minMaxStateCent = map(list,zip(*motionStateParams))  # bounds on the continuous state centroid
tmp = [[motionControlParams[i][0]+mu/2-1e-4,motionControlParams[i][1]-mu/2+1e-4] for i in range(len(motionControlParams))]
minMaxCtrl = map(list,zip(*tmp))  # bounds on the continuous controls
minMaxCtrlCent = map(list,zip(*motionControlParams))  # bounds on the continuous control centroid

# ====================================================================
# Read motion state outputs from the input file
# ====================================================================
slugsinfile = pngFileBasis+".slugsin"
slugsReader = open(slugsinfile,"r")
motionStateOutputState = False
motionControlOutputState = False
xMotionStateVars = []
yMotionStateVars = []
motionStateBitVars = [[]]*len(motionStateVars)
motionControlBitVars = [[]]*len(motionControlVars)
for line in slugsReader.readlines():
    line = line.strip()
    if line.startswith("["):
        motionStateOutputState = False
        motionControlOutputState = False
    if line=="[MOTION STATE OUTPUT]":
        motionStateOutputState = True
    elif motionStateOutputState and len(line)>0 and not line.startswith("#"):
        if not line.startswith(tuple(motionStateVars)):
            print >> sys.stderr, line+" not included among motion state variables in the robot model file"
        else:
            for i,varName in enumerate(motionStateVars):
                if line.startswith(varName):
                    k = i
                    if len(motionStateBitVars[k])==0:
                        tmp = []
        tmp.append(line)
        motionStateBitVars[k] = tmp
    if line=="[MOTION CONTROLLER OUTPUT]":
        motionControlOutputState = True
    elif motionControlOutputState and len(line)>0 and not line.startswith("#"):
        if not line.startswith(tuple(motionControlVars)):
            print >> sys.stderr, line+" not included among motion control variables in the robot model file"
        else:
            for i,varName in enumerate(motionControlVars):
                if line.startswith(varName):
                    k = i
                    if len(motionControlBitVars[k])==0:
                        tmp = []
        tmp.append(line)
        motionControlBitVars[k] = tmp

print("Motion state variables: "+str(motionStateBitVars))
print("Motion control variables: "+str(motionControlBitVars))

# ====================================================================
# Assign keys to doors and deliveries
# ====================================================================
keys = []
for (a,b) in colorCoding:
    if b=="Door":
        keys.append((a,b))
    elif b=="Delivery":
        keys.append((a,b))

# ====================================================================
# Assign robot keys
# ====================================================================
movingObstacles = []
for (a,b) in colorCoding:
    if b.startswith("MovingObstacle:"):
        movingObstacles.append((a,b.split(":")))

# ==================================
# Prepare Slugs Call
# ==================================
bddinfile = specFile
while bddinfile.rfind(".") > bddinfile.rfind(os.sep):
    bddinfile = bddinfile[0:bddinfile.rfind(".")]
bddinfile = bddinfile+".bdd"
print "Using BDD file: "+bddinfile
slugsLink = sys.argv[0][0:sys.argv[0].rfind("pyGameRobotDynamicsVisualizer.py")]+"../src/slugs"
print slugsLink

# ==================================
# Main loop
# ==================================
def actionLoop():
    screen = pygame.display.set_mode(((xsize+2)*MAGNIFY,(ysize+2)*MAGNIFY))
    pygame.display.set_caption('Strategy Visualizer')
    clock = pygame.time.Clock()

    screenBuffer = pygame.Surface(screen.get_size())
    screenBuffer = screenBuffer.convert()
    screenBuffer.fill((64, 64, 64)) # Dark Gray

    # Open Slugs
    slugsProcess = subprocess.Popen(slugsLink+" --interactiveStrategy "+slugsinfile+" "+bddinfile, shell=True, bufsize=1048000, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

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

    # Get output APs (Motion State)
    slugsProcess.stdin.write("XPRINTMOTIONSTATE\n")
    slugsProcess.stdin.flush()
    slugsProcess.stdout.readline() # Skip the prompt
    lastLine = " "
    outputAPs = []
    while (lastLine!=""):
        lastLine = slugsProcess.stdout.readline().strip()
        if lastLine!="":
            outputAPs.append(lastLine)

    # Get output APs (Other output)
    slugsProcess.stdin.write("XPRINTOTHEROUTPUTS\n")
    slugsProcess.stdin.flush()
    slugsProcess.stdout.readline() # Skip the prompt
    lastLine = " "
    while (lastLine!=""):
        lastLine = slugsProcess.stdout.readline().strip()
        if lastLine!="":
            outputAPs.append(lastLine)

    # Get motion controller output APs
    slugsProcess.stdin.write("XPRINTMOTIONCONTROLOUTPUTS\n")
    slugsProcess.stdin.flush()
    slugsProcess.stdout.readline() # Skip the prompt
    lastLine = " "
    while (lastLine!=""):
        lastLine = slugsProcess.stdout.readline().strip()
        if lastLine!="":
            outputAPs.append(lastLine)
    print "output APs:"+str(outputAPs)

    # Get initial state
    slugsProcess.stdin.write("XGETINIT\n")
    slugsProcess.stdin.flush()
    slugsProcess.stdout.readline() # Skip the prompt
    currentState = slugsProcess.stdout.readline().strip()

    motionStateRaw = [0]*len(motionStateBitVars)
    for k in xrange(0,len(motionStateBitVars)):
        for i,ap in enumerate(outputAPs):
            for  j,ap2 in enumerate(motionStateBitVars[k]):
                if ap==ap2:
                    if currentState[i+len(inputAPs)]=="1":
                        motionStateRaw[k] += (1 << j)
                    elif currentState[i+len(inputAPs)]=="0":
                        pass
                    else:
                        raise 123
    motionState = np.array(motionStateRaw)*eta + np.array(minMaxStateCent[1])
    motionState = list(np.minimum(np.maximum(motionState,np.array(minMaxState[1])),np.array(minMaxState[0])))
    for i in xrange(0,len(motionState)):
        exec(motionStateVars[i]+str("=motionState[i]"))

    # Pre-store positions
    doorAndDeliveryInputBitPositions = {}
    for (a,b) in colorCoding:
        if b=="Door" or b=="Delivery":
            for pos,name in enumerate(inputAPs):
                if name=="door"+str(a) or name=="deliveryrequest"+str(a)  :
                    doorAndDeliveryInputBitPositions[a] = pos

    while 1:

        for event in pygame.event.get():
            if event.type == pygame.locals.QUIT or (event.type == pygame.locals.KEYDOWN and event.key == pygame.locals.K_ESCAPE):
                slugsProcess.stdin.write("QUIT\n")
                slugsProcess.stdin.flush()
                return

        # Obtain robot information for drawing
        motionCtrlRaw = [0]*len(motionControlBitVars)
        for k in xrange(0,len(motionControlBitVars)):
            for i,ap in enumerate(outputAPs):
                for  j,ap2 in enumerate(motionControlBitVars[k]):
                    if ap==ap2:
                        if currentState[i+len(inputAPs)]=="1":
                            motionCtrlRaw[k] += (1 << j)
                        elif currentState[i+len(inputAPs)]=="0":
                            pass
                        else:
                            raise 123
        motionCtrl = np.array(motionCtrlRaw)*mu + np.array(minMaxCtrlCent[1])
        motionCtrl = list(np.minimum(np.maximum(motionCtrl,np.array(minMaxCtrl[1])),np.array(minMaxCtrl[0])))
        for i in xrange(0,len(motionCtrl)):
            exec(motionControlVars[i]+str("=motionCtrl[i]"))
        print v, w
        print x, y, theta

        # Draw pickup/drop
        for i,ap in enumerate(outputAPs):
            if ap=="pickup":
                if currentState[i+len(inputAPs)]!="0":
                    fillColor = (255,64,64)
                else:
                    fillColor = (64,64,64)
                pygame.draw.rect(screenBuffer,fillColor,(0,0,MAGNIFY*(xsize+2)/2,MAGNIFY),0)
            if ap=="drop":
                if currentState[i+len(inputAPs)]!="0":
                    fillColor = (64,220,64)
                else:
                    fillColor = (64,64,64)
                pygame.draw.rect(screenBuffer,fillColor,(MAGNIFY*(xsize+2)/2,0,MAGNIFY*(xsize+2)/2,MAGNIFY),0)
            
        # Obtain moving obstacle information for drawing
        movingPos = []
        for (a,b) in movingObstacles:
            posX = 0
            posY = 0
            for i,ap in enumerate(inputAPs):
                if ap in ["mox"+str(a)+"_0","mox"+str(a)+"_1","mox"+str(a)+"_2","mox"+str(a)+"_3","mox"+str(a)+"_4","mox"+str(a)+"_5","mox"+str(a)+"_6","mox"+str(a)+"_7","mox"+str(a)+"_8","mox"+str(a)+"_9"]:
                    if currentState[i]=="1":
                        posX += (1 << int(ap[ap.rfind("_")+1:]))
                    elif currentState[i]=="0":
                        pass
                    else:
                        raise 123
                elif ap in ["moy"+str(a)+"_0","moy"+str(a)+"_1","moy"+str(a)+"_2","moy"+str(a)+"_3","moy"+str(a)+"_4","moy"+str(a)+"_5","moy"+str(a)+"_6","moy"+str(a)+"_7","moy"+str(a)+"_8","moy"+str(a)+"_9"]:
                    if currentState[i]=="1":
                        posY += (1 << int(ap[ap.rfind("_")+1:]))
            movingPos.append((posX,posY))

        # Draw Field
        for xf in xrange(0,xsize):
            for yf in xrange(0,ysize):
                paletteColor = imageData[yf*xsize+xf]
                
                # Translate color to what is to be written
                if paletteColor in colorCodingMap:
                    colorCodeDescription = colorCodingMap[paletteColor]
                    if colorCodeDescription.startswith("MovingObstacle:"):
                        color = (255,255,255)
                    elif colorCodeDescription=="Door" or colorCodeDescription=="Delivery":
                        if currentState[doorAndDeliveryInputBitPositions[paletteColor]]=="0":
                            color = (128+palette[paletteColor*3]/2,128+palette[paletteColor*3+1]/2,128+palette[paletteColor*3+2]/2)
                        else:
                            color = palette[paletteColor*3:paletteColor*3+3]
                    else:
                        color = palette[paletteColor*3:paletteColor*3+3]
                else:
                    color = palette[paletteColor*3:paletteColor*3+3]

                pygame.draw.rect(screenBuffer,color,((xf+1)*MAGNIFY,(yf+1)*MAGNIFY,MAGNIFY,MAGNIFY),0)

        # Draw "Good" Robot
        robotX = int(round(np.true_divide((x - xmin),eta)))
        robotY = int(round(np.true_divide((y - ymin),eta)))
        pygame.draw.circle(screenBuffer, (192,32,32), ((robotX+1)*MAGNIFY+MAGNIFY/2,(robotY+1)*MAGNIFY+MAGNIFY/2) , MAGNIFY/3-2, 0)
        pygame.draw.circle(screenBuffer, (255,255,255), ((robotX+1)*MAGNIFY+MAGNIFY/2,(robotY+1)*MAGNIFY+MAGNIFY/2) , MAGNIFY/3-1, 1)
        pygame.draw.circle(screenBuffer, (0,0,0), ((robotX+1)*MAGNIFY+MAGNIFY/2,(robotY+1)*MAGNIFY+MAGNIFY/2) , MAGNIFY/3, 1)

        # Draw cell frames
        for xc in xrange(0,xsize):
            for yc in xrange(0,ysize):
                pygame.draw.rect(screenBuffer,(0,0,0),((xc+1)*MAGNIFY,(yc+1)*MAGNIFY,MAGNIFY,MAGNIFY),1)
        pygame.draw.rect(screenBuffer,(0,0,0),(MAGNIFY-1,MAGNIFY-1,MAGNIFY*xsize+2,MAGNIFY*ysize+2),1)

        # Draw "Bad" Robot/Moving Obstacle
        for obstacleNo,(a,b) in enumerate(movingObstacles):
            speedMO = int(b[1]) # 0 or 1 - 0 is twice as slow
            xsizeMO = int(b[2])
            ysizeMO = int(b[3])
            (xpos,ypos) = movingPos[obstacleNo]
            pygame.draw.rect(screenBuffer, (32,32,192), ((xpos+1)*MAGNIFY+MAGNIFY/4,(ypos+1)*MAGNIFY+MAGNIFY/4, MAGNIFY*xsizeMO-MAGNIFY/2, MAGNIFY*xsizeMO-MAGNIFY/2),0)
            pygame.draw.rect(screenBuffer, (255,255,255), ((xpos+1)*MAGNIFY+MAGNIFY/4-1,(ypos+1)*MAGNIFY+MAGNIFY/4-1, MAGNIFY*xsizeMO-MAGNIFY/2+2, MAGNIFY*xsizeMO-MAGNIFY/2+2),1)
            pygame.draw.rect(screenBuffer, (0,0,0), ((xpos+1)*MAGNIFY+MAGNIFY/4-2,(ypos+1)*MAGNIFY+MAGNIFY/4-2, MAGNIFY*xsizeMO-MAGNIFY/2+4, MAGNIFY*xsizeMO-MAGNIFY/2+4),1)
        
        # Flip!
        screen.blit(screenBuffer, (0, 0))
        pygame.display.flip()

        # Update Doors and requests
        nextInput = currentState[0:len(inputAPs)]
        for keyNum,(a,b) in enumerate(keys):
            if pygame.key.get_pressed()[pygame.locals.K_1+keyNum]:
                nextInput = nextInput[0:doorAndDeliveryInputBitPositions[a]]+"1"+nextInput[doorAndDeliveryInputBitPositions[a]+1:]
            else:
                nextInput = nextInput[0:doorAndDeliveryInputBitPositions[a]]+"0"+nextInput[doorAndDeliveryInputBitPositions[a]+1:]

        # Update moving obstacles
        for obstacleNo,(a,b) in enumerate(movingObstacles):
            speedMO = int(b[1]) # 0 or 1 - 0 is twice as slow
            (xpos,ypos) = movingPos[obstacleNo]

            mayMove = True
            if speedMO==0:
                for i,ap in enumerate(outputAPs):
                    if ap == "MOSpeederMonitor"+str(a):
                        if currentState[i+len(inputAPs)]!="1":
                            mayMove = False

            if mayMove:
                if pygame.key.get_pressed()[pygame.locals.K_LEFT]:
                    xpos -= 1
                elif pygame.key.get_pressed()[pygame.locals.K_RIGHT]:
                    xpos += 1
                elif pygame.key.get_pressed()[pygame.locals.K_UP]:
                    ypos -= 1
                elif pygame.key.get_pressed()[pygame.locals.K_DOWN]:
                    ypos += 1

            # Encode new position
            for i,ap in enumerate(inputAPs):
                if ap in ["mox"+str(a)+"_0","mox"+str(a)+"_1","mox"+str(a)+"_2","mox"+str(a)+"_3","mox"+str(a)+"_4","mox"+str(a)+"_5","mox"+str(a)+"_6","mox"+str(a)+"_7","mox"+str(a)+"_8","mox"+str(a)+"_9"]:
                    if (xpos & (1 << int(ap[ap.rfind("_")+1:])))>0:
                        nextInput = nextInput[0:i]+"1"+nextInput[i+1:]
                    else:
                        nextInput = nextInput[0:i]+"0"+nextInput[i+1:]
                elif ap in ["moy"+str(a)+"_0","moy"+str(a)+"_1","moy"+str(a)+"_2","moy"+str(a)+"_3","moy"+str(a)+"_4","moy"+str(a)+"_5","moy"+str(a)+"_6","moy"+str(a)+"_7","moy"+str(a)+"_8","moy"+str(a)+"_9"]:
                    if (ypos & (1 << int(ap[ap.rfind("_")+1:])))>0:
                        nextInput = nextInput[0:i]+"1"+nextInput[i+1:]
                    else:
                        nextInput = nextInput[0:i]+"0"+nextInput[i+1:]

        # Execute the continuous transition
        for i,varName in enumerate(motionStateVars):
            exec(systemModel[i])
        preMotionState = []
        for i,varName in enumerate(motionStateVars):
            # one-sample update
            exec(varName+" = "+varName+"_dot*tau + "+varName)
            exec(varName+" = min([max(["+varName+",minMaxState[1][i]]),minMaxState[0][i]])")
            exec(varName+"b = np.true_divide(("+varName+" - "+varName+"min),eta)")
            preMotionState += bin(int(round(eval(varName+'b'))))[2:].zfill(len(motionStateBitVars[i]))
            preMotionState = ''.join(preMotionState)
        print eval(varName+'b')
        print bin(int(round(eval(varName+'b'))))[2:]
        print preMotionState

        # Make the transition
        slugsProcess.stdin.write("XMAKECONTROLTRANS\n"+nextInput+preMotionState)
        slugsProcess.stdin.flush()
        slugsProcess.stdout.readline() # Skip the prompt
        nextLine = slugsProcess.stdout.readline().strip()
        if nextLine.startswith("ERROR"):
            screenBuffer.fill((192, 64, 64)) # Red!
            # Keep the state the same
        else:
            currentState = nextLine
            print >>sys.stderr, currentState
            screenBuffer.fill((64, 64, 64)) # Gray, as usual
        
        # Done
        clock.tick(10)

# ==================================
# Call main program
# ==================================
actionLoop()
