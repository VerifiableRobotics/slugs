#!/usr/bin/python
#
# Animates the robot abstraction
#
#
# REQUIREMENTS FOR PROPER Operation:
# - The slugsin file has the state bits in inverse order. This is necessary to solve the problem that Pessoa's abstract are most-significant-bit first
# - There are multiple required files:
#   - PNG file for the workspace
#   - SLUGSIN file for the specification
#   - BDD file for the robot abstraction
#   - COLORCODING file for the simulator options
#   The files all need to have the same prefix, with the exception of the BDD file, where all things standing behind dots right of the last path separator character ("/") are stripped. This allows
#   recycling the same .bdd file for many scenarios.

import math
import os
import sys, code
import resource
import subprocess
import signal
import tempfile
import copy, glob
import itertools
import Image
import os, pygame, pygame.locals

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

# ====================================================================
# Read motion state outputs from the input file
# ====================================================================
slugsinfile = specFile[0:specFile.rfind(".png")]+".slugsin"
slugsReader = open(slugsinfile,"r")
motionStateOutputState = False
xMotionStateVars = []
yMotionStateVars = []
for line in slugsReader.readlines():
    line = line.strip()
    if line.startswith("["):
        motionStateOutputState = False
    if line=="[MOTION STATE OUTPUT]":
        motionStateOutputState = True
    elif motionStateOutputState and len(line)>0 and not line.startswith("#"):
        if line.startswith("x") and not line.upper().startswith("XSPEED"):
            xMotionStateVars.append(line)
        elif line.startswith("y") and not line.upper().startswith("YSPEED"):
            yMotionStateVars.append(line)
        else:
            print >>sys.stderr, "Note: Auxiliary motion state variable "+line+" has been given."


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
bddinfile = bddinfile+"*.bdd"
bddinfile = ":".join(glob.glob(bddinfile))
print "Using BDD file: "+bddinfile
slugsLink = sys.argv[0][0:sys.argv[0].rfind("pyGameNondeterministicMotionVisualizer.py")]+"../src/slugs"
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
    slugsProcess = subprocess.Popen(slugsLink+" --interactiveStrategy --nonDeterministicMotion --nonDeterministicMotionSelfLoopLivenessAssumption "+slugsinfile+" "+bddinfile, shell=True, bufsize=1048000, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

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
    outputAPs = []
    slugsProcess.stdin.write("XPRINTOUTPUTS\n")
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

    # Pre-store positions
    doorAndDeliveryInputBitPositions = {}
    for (a,b) in colorCoding:
        if b=="Door" or b=="Delivery":
            for pos,name in enumerate(inputAPs):
                if name=="door"+str(a) or name=="deliveryrequest"+str(a)  :
                    doorAndDeliveryInputBitPositions[a] = pos

    # Count the loop

    loopNumber = 0
    isPaused = False
    while 1:
        loopNumber += 1

        for event in pygame.event.get():
            if event.type == pygame.locals.QUIT or (event.type == pygame.locals.KEYDOWN and event.key == pygame.locals.K_ESCAPE):
                slugsProcess.stdin.write("QUIT\n")
                slugsProcess.stdin.flush()
                return
            if (event.type == pygame.locals.KEYDOWN and event.key == pygame.locals.K_SPACE):
                isPaused = not isPaused

        # Obtain robot information for drawing
        robotX = 0
        robotY = 0
        for i,ap in enumerate(outputAPs):
            for j,ap2 in enumerate(xMotionStateVars):
                if ap==ap2:
                    if currentState[i+len(inputAPs)]=="1":
                        robotX += (1 << j)
                    elif currentState[i+len(inputAPs)]=="0":
                        pass
                    else:
                        raise 123
        for i,ap in enumerate(outputAPs):
            for  j,ap2 in enumerate(yMotionStateVars):
                if ap==ap2:
                    if currentState[i+len(inputAPs)]=="1":
                        robotY += (1 << j)
                    elif currentState[i+len(inputAPs)]=="0":
                        pass
                    else:
                        raise 123

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
        for x in xrange(0,xsize):
            for y in xrange(0,ysize):
                paletteColor = imageData[y*xsize+x]
                
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

                pygame.draw.rect(screenBuffer,color,((x+1)*MAGNIFY,(y+1)*MAGNIFY,MAGNIFY,MAGNIFY),0)

        # Draw "Good" Robot
        pygame.draw.circle(screenBuffer, (192,32,32), ((robotX+1)*MAGNIFY+MAGNIFY/2,(robotY+1)*MAGNIFY+MAGNIFY/2) , MAGNIFY/3-2, 0)
        pygame.draw.circle(screenBuffer, (255,255,255), ((robotX+1)*MAGNIFY+MAGNIFY/2,(robotY+1)*MAGNIFY+MAGNIFY/2) , MAGNIFY/3-1, 1)
        pygame.draw.circle(screenBuffer, (0,0,0), ((robotX+1)*MAGNIFY+MAGNIFY/2,(robotY+1)*MAGNIFY+MAGNIFY/2) , MAGNIFY/3, 1)

        # Draw cell frames
        for x in xrange(0,xsize):
            for y in xrange(0,ysize):
                pygame.draw.rect(screenBuffer,(0,0,0),((x+1)*MAGNIFY,(y+1)*MAGNIFY,MAGNIFY,MAGNIFY),1)
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

        # Make the transition
        if not isPaused:
            slugsProcess.stdin.write("XMAKETRANSRANDOM\n"+nextInput)
            slugsProcess.stdin.flush()
            slugsProcess.stdout.readline() # Skip the prompt
            nextLine = slugsProcess.stdout.readline().strip()
            if nextLine.startswith("ERROR"):
                screenBuffer.fill((192, 64, 64)) # Red!
                # Keep the state the same
            else:
                currentState = nextLine

                # Print a state header
                if (loopNumber % 20)==1:
                    print "-"*(len(currentState)+2)
                    apNames = inputAPs+outputAPs
                    maxLenAPNames = 0
                    for a in apNames:
                        maxLenAPNames = max(maxLenAPNames,len(a))
                    apNamesEqualized = [(" "*(maxLenAPNames-len(a)))+a for a in apNames]
                    for i in xrange(0,maxLenAPNames):
                        for a in apNamesEqualized:
                            sys.stderr.write(a[i])
                        sys.stderr.write("\n")
                    print "-"*(len(currentState)+2)
                    
                # Print the state
                print >>sys.stderr, currentState
                screenBuffer.fill((64, 64, 64)) # Gray, as usual

                # Tick
                clock.tick(10)
        else:
            # Paused
            screenBuffer.fill((64, 64, 64)) # Gray, as usual
            # Tick
            clock.tick(3)


# ==================================
# Call main program
# ==================================
actionLoop()
