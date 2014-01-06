#!/usr/bin/python
#
# Performs \infty-resilient GR(1) synthesis
# Takes the input file in Slugs format

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

# ==================================
# Entry point
# ==================================
if len(sys.argv)<2:
    print >>sys.stderr, "Error: Need Specification file as parameter"
    sys.exit(1)
specFile = sys.argv[1]
options = sys.argv[2:]
outputFileBasis = specFile[0:specFile.find(".")]
print outputFileBasis


# ==================================
# Read input image
# ==================================
import os,sys
pngfile = Image.open(specFile)
print "Size of Workspace:",pngfile.size
xsize = pngfile.size[0]
ysize = pngfile.size[1]
imageData = pngfile.getdata()


# ====================================================================
# Read input image color encoding configuration file
# ====================================================================
colorCodingInformationFile = open(outputFileBasis+".colorcoding","r").readlines()
colorCoding = [(int(a.strip().split(" ")[0]),a.strip().split(" ")[1]) for a in colorCodingInformationFile if a.strip()!=""]

# =============================================================
# Locate "inactive reagions" (Doors, Pick-up zones, drop zones, etc.)
# =============================================================
nonFeatureColors = set([0,1])
assigned = [] # Which parts of the workspace have been assigned some "meaning" already?
for y in xrange(0,ysize):
    assigned.append([])
    thisLine = assigned[len(assigned)-1]
    for x in xrange(0,xsize):
        if imageData[y*xsize+x] in nonFeatureColors:
            thisLine.append(True)
        else:
            thisLine.append(False)

# =============================================================
# Locate "active reagions" (Doors, Pick-up zones, drop zones, etc.)
# =============================================================
actionZones = []

for y in xrange(0,ysize):
    thisLine = assigned[y]
    for x in xrange(0,xsize):
        if thisLine[x]==False:
            def floodFill(x,y,result,color):
                if (imageData[y*xsize+x]==color) and not assigned[y][x]:
                    result.add((x,y))
                    assigned[y][x]=True
                    if x>0:
                        floodFill(x-1,y,result,color)
                    if x<xsize-1:
                        floodFill(x+1,y,result,color)
                    if y>0:
                        floodFill(x,y-1,result,color)
                    if y<ysize-1:
                        floodFill(x,y+1,result,color)

            elements = set([(x,y)])
            floodFill(x,y,elements,imageData[y*xsize+x])
            actionZones.append((imageData[y*xsize+x],elements))


# Merge zones with the same number >= 2 so that they appear as one
groupedZones = {}
for (a,b) in actionZones:
    groupedZones[a] = []
for (a,b) in actionZones:
    groupedZones[a] += b
actionZones = [(a,b) for (a,b) in actionZones if a<=1]+[(a,groupedZones[a]) for a in groupedZones.keys() if a>=2]

print actionZones

# =============================================================
# Make the motion part of the specification
# =============================================================
nofXCoordinateBits = 1
while ((1 << nofXCoordinateBits) < xsize):
    nofXCoordinateBits += 1
nofYCoordinateBits = 1
while ((1 << nofYCoordinateBits) < ysize):
    nofYCoordinateBits += 1

def encodeXValue(x):
    parts = ["&"]*(nofXCoordinateBits-1)
    for i in xrange(0,nofXCoordinateBits):
        if (x & (1 << i))==0:
            parts.append("!")
        parts.append("x"+str(i))
    return " ".join(parts)

def encodeYValue(x):
    parts = ["&"]*(nofYCoordinateBits-1)
    for i in xrange(0,nofYCoordinateBits):
        if (x & (1 << i))==0:
            parts.append("!")
        parts.append("y"+str(i))
    return " ".join(parts)

def encodeXPrimeValue(x):
    parts = ["&"]*(nofXCoordinateBits-1)
    for i in xrange(0,nofXCoordinateBits):
        if (x & (1 << i))==0:
            parts.append("!")
        parts.append("x"+str(i)+"'")
    return " ".join(parts)

def encodeYPrimeValue(x):
    parts = ["&"]*(nofYCoordinateBits-1)
    for i in xrange(0,nofYCoordinateBits):
        if (x & (1 << i))==0:
            parts.append("!")
        parts.append("y"+str(i)+"'")
    return " ".join(parts)

# Input and Output
slugsFile = open(outputFileBasis+".slugsin","w")
slugsFile.write("[OUTPUT]\nleft\nright\nup\ndown\npickup\ndrop\n")

slugsFile.write("\n[INPUT]\n")
for i in xrange(0,nofXCoordinateBits):
    slugsFile.write("x"+str(i)+"\n")
for i in xrange(0,nofYCoordinateBits):
    slugsFile.write("y"+str(i)+"\n")

slugsFile.write("\n[ENV_INIT]\n")
for i in xrange(0,nofXCoordinateBits):
    slugsFile.write("! x"+str(i)+"\n")
for i in xrange(0,nofYCoordinateBits):
    slugsFile.write("! y"+str(i)+"\n")

slugsFile.write("\n[SYS_INIT]\n! left\n! right\n! up\n! down\n! pickup\n! drop\n")

# General motion radius by 1 cell into directions now being steered to and 2 cells otherwise
slugsFile.write("\n[ENV_TRANS]\n# We can't jump far\n")

for i in xrange(0,3*xsize+3*ysize):
    slugsFile.write("& ")

for i in xrange(0,xsize):
    slugsFile.write("| | ! left ! "+encodeXValue(i)+" ")
    parts = []
    for k in xrange(max(0,i-2),min(xsize,i+2)-1):
        parts.append("|")
    for k in xrange(max(0,i-2),min(xsize,i+2)):
        parts.append(encodeXPrimeValue(k))
    slugsFile.write(" ".join(parts))
    slugsFile.write(" ")

for i in xrange(0,xsize):
    slugsFile.write("| | ! right ! "+encodeXValue(i)+" ")
    parts = []
    for k in xrange(max(0,i-1),min(xsize,i+3)-1):
        parts.append("|")
    for k in xrange(max(0,i-1),min(xsize,i+3)):
        parts.append(encodeXPrimeValue(k))
    slugsFile.write(" ".join(parts))
    slugsFile.write(" ")

for i in xrange(0,xsize):
    slugsFile.write("| | | right left ! "+encodeXValue(i)+" ")
    parts = []
    for k in xrange(max(0,i-1),min(xsize,i+2)-1):
        parts.append("|")
    for k in xrange(max(0,i-1),min(xsize,i+2)):
        parts.append(encodeXPrimeValue(k))
    slugsFile.write(" ".join(parts))
    slugsFile.write(" ")

for i in xrange(0,ysize):
    slugsFile.write("| | ! up ! "+encodeYValue(i)+" ")
    parts = []
    for k in xrange(max(0,i-2),min(ysize,i+2)-1):
        parts.append("|")
    for k in xrange(max(0,i-2),min(ysize,i+2)):
        parts.append(encodeYPrimeValue(k))
    slugsFile.write(" ".join(parts))
    slugsFile.write(" ")

for i in xrange(0,ysize):
    slugsFile.write("| | ! down ! "+encodeYValue(i)+" ")
    parts = []
    for k in xrange(max(0,i-1),min(ysize,i+3)-1):
        parts.append("|")
    for k in xrange(max(0,i-1),min(ysize,i+3)):
        parts.append(encodeYPrimeValue(k))
    slugsFile.write(" ".join(parts))
    slugsFile.write(" ")

for i in xrange(0,ysize):
    slugsFile.write("| | | down up ! "+encodeYValue(i)+" ")
    parts = []
    for k in xrange(max(0,i-1),min(ysize,i+2)-1):
        parts.append("|")
    for k in xrange(max(0,i-1),min(ysize,i+2)):
        parts.append(encodeYPrimeValue(k))
    slugsFile.write(" ".join(parts))
    slugsFile.write(" ")

slugsFile.write("1\n")


# Moving where we should be moving (X and Y separate)
slugsFile.write("# We go exactly where we want to go.\n")
for i in xrange(0,3*xsize):
    slugsFile.write("& ")

for i in xrange(0,xsize):
    slugsFile.write("| | ! left ! "+encodeXValue(i)+" ")
    slugsFile.write(encodeXPrimeValue(max(i-1,0)))
    slugsFile.write(" ")

for i in xrange(0,xsize):
    slugsFile.write("| | ! right ! "+encodeXValue(i)+" ")
    slugsFile.write(encodeXPrimeValue(min(i+1,xsize-1)))
    slugsFile.write(" ")

for i in xrange(0,xsize):
    slugsFile.write("| | | right left ! "+encodeXValue(i)+" ")
    slugsFile.write(encodeXPrimeValue(i))
    slugsFile.write(" ")

slugsFile.write("1\n")

for i in xrange(0,3*ysize):
    slugsFile.write("& ")

for i in xrange(0,ysize):
    slugsFile.write("| | ! up ! "+encodeYValue(i)+" ")
    slugsFile.write(encodeYPrimeValue(max(i-1,0)))
    slugsFile.write(" ")

for i in xrange(0,ysize):
    slugsFile.write("| | ! down ! "+encodeYValue(i)+" ")
    slugsFile.write(encodeYPrimeValue(min(i+1,ysize-1)))
    slugsFile.write(" ")

for i in xrange(0,ysize):
    slugsFile.write("| | | up down ! "+encodeYValue(i)+" ")
    slugsFile.write(encodeYPrimeValue(i))
    slugsFile.write(" ")

slugsFile.write("1\n")

# =============================================================
# Make the collision avoidance part of the specification
# =============================================================
slugsFile.write("\n[SYS_TRANS]\n")
for y in xrange(0,ysize):
    for x in xrange(0,xsize):
        if imageData[y*xsize+x]==1:
            # Forbidden zone!!!!!!
            slugsFile.write("| ! "+encodeXPrimeValue(x)+" ! "+encodeYPrimeValue(y)+"\n")
            

# =============================================================
# We can only go left or right and up or down
# =============================================================
slugsFile.write("\n[SYS_TRANS]\n| ! left' ! right'\n| ! up' ! down'\n")


# =============================================================
# Work on the properties under concern
# =============================================================
for activeColor in xrange(2,256,2):


    colorInfo = None
    for (a,b) in colorCoding:
        if a==activeColor:
            colorInfo = b

    if (colorInfo!=None):
        print "Working on color: "+str(activeColor)
        slugsFile.write("\n#INCREMENTAL: Color"+str(activeColor)+"\n\n")

    if colorInfo=="Door":
        slugsFile.write("\n[INPUT]\n")
        slugsFile.write("door"+str(activeColor)+"\n")

        # =============================================================
        # Always eventually do not obstruct a door - for every door individually
        # =============================================================
        allDoors = []
        for (color,d) in actionZones:
            if (color==activeColor):
                allDoors += d
        slugsFile.write("\n[SYS_LIVENESS]\n")
        for i in xrange(0,len(allDoors)-1):
            slugsFile.write("& ")
        for i in xrange(0,len(allDoors)):
            (a,b) = allDoors[i]
            if i>0:
                slugsFile.write(" ")
            slugsFile.write("| ! "+encodeXValue(a)+" ! "+encodeYValue(b))
        slugsFile.write("\n\n")

        # =============================================================
        # Never walk towards a close door
        # =============================================================
        slugsFile.write("\n[SYS_TRANS]\n")
        for doorTuple in actionZones:
            if doorTuple[0]==activeColor:   
                for (a,b) in doorTuple[1]:
                    door = doorTuple[1]
                    candidates = []
                    if a>0 and not (a-1,b) in door:
                        candidates.append((a-1,b,"right'"))
                    if a<(xsize-1) and not (a+1,b) in door:
                        candidates.append((a+1,b,"left'"))
                    if b>0 and not (a,b-1) in door:
                        candidates.append((a,b-1,"down'"))
                    if b<(ysize-1) and not (a,b+1) in door:
                        candidates.append((a,b+1,"up'"))
                    if a>0 and b>0 and not (a-1,b-1) in door:
                        candidates.append((a-1,b-1,"& right' down'"))
                    if a>0 and b<(ysize-1) and not (a-1,b+1) in door:
                        candidates.append((a-1,b+1,"& right' up'"))
                    if a<(xsize-1) and b>0 and not (a+1,b-1) in door:
                        candidates.append((a+1,b-1,"& left' down'"))
                    if a<(xsize-1) and b<(ysize-1) and not (a+1,b+1) in door:
                        candidates.append((a+1,b+1,"& left' up'"))
                    for (a,b,cond) in candidates:
                        slugsFile.write("| | | ! door" + str(activeColor)+ "' ! "+cond+" ! "+encodeXPrimeValue(a)+" ! "+encodeYPrimeValue(b)+"\n")
        slugsFile.write("\n\n")

        slugsFile.write("\n[ENV_LIVENESS]\n")
        slugsFile.write("! door" + str(activeColor)+"'\n")
        
    elif colorInfo=="Delivery":
        slugsFile.write("\n[OUTPUT]\n")
        slugsFile.write("pendingdelivery"+str(activeColor)+"\n")
        slugsFile.write("deliverypickedup"+str(activeColor)+"\n")
        slugsFile.write("\n[INPUT]\n")
        slugsFile.write("deliveryrequest"+str(activeColor)+"\n")

        # =============================================================
        # Bring stuff from A to B whenever requested to do so!
        # =============================================================
        for (a,pickupZones) in actionZones:
            if a==activeColor:
                slugsFile.write("\n[SYS_INIT]\n! pendingdelivery"+str(activeColor)+"\n! deliverypickedup"+str(activeColor)+"\n\n[SYS_TRANS]\n")

                deliveryZoneNumber = a+1
                deliveryZones = None
                for (a,b) in actionZones:
                    if a==deliveryZoneNumber:
                        if deliveryZones!=None:
                            print >>sys.stderr, "Error: Non-continuous delivery zone: "+str(a)
                            sys.exit(1)
                        deliveryZones = b

                print "PI: "+str(pickupZones)
                print "DE: "+str(deliveryZones)

                # PendingDelivery is updated correctly (deliveries only get counted after they have been registered for simplicity
                slugsFile.write("| | pendingdelivery"+str(activeColor)+" ! pendingdelivery"+str(activeColor)+"' deliveryrequest"+str(activeColor)+"\n")
                slugsFile.write("| | pendingdelivery"+str(activeColor)+" pendingdelivery"+str(activeColor)+"' ! deliveryrequest"+str(activeColor)+"\n")
                slugsFile.write("| | ! pendingdelivery"+str(activeColor)+" pendingdelivery"+str(activeColor)+"' drop'\n")
                slugsFile.write("| | ! pendingdelivery"+str(activeColor)+" pendingdelivery"+str(activeColor)+"' deliverypickedup"+str(activeColor)+"\n")
                slugsFile.write("| | ! pendingdelivery"+str(activeColor)+" pendingdelivery"+str(activeColor)+" ")
                slugsFile.write("| "*len(deliveryZones))
                for (a,b) in deliveryZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")
                slugsFile.write("| | ! pendingdelivery"+str(activeColor)+" ! pendingdelivery"+str(activeColor)+"' | ! drop' | ! deliverypickedup"+str(activeColor)+" ! ")
                slugsFile.write("| "*len(deliveryZones))
                for (a,b) in deliveryZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")

                # Picked up is updated correctly
                slugsFile.write("| | deliverypickedup"+str(activeColor)+" deliverypickedup"+str(activeColor)+"' | ! pickup' ! ")
                slugsFile.write("| "*len(pickupZones))
                for (a,b) in pickupZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")
                slugsFile.write("| | deliverypickedup"+str(activeColor)+" ! deliverypickedup"+str(activeColor)+"' & pickup' ")
                slugsFile.write("| "*len(pickupZones))
                for (a,b) in pickupZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")
                slugsFile.write("| | ! deliverypickedup"+str(activeColor)+" deliverypickedup"+str(activeColor)+"' drop'\n")
                slugsFile.write("| | ! deliverypickedup"+str(activeColor)+" ! deliverypickedup"+str(activeColor)+"' ! drop'\n")
                
                # Drop is only allowed at the target position
                slugsFile.write("| | ! deliverypickedup"+str(activeColor)+" ! drop' ")
                slugsFile.write("| "*len(deliveryZones))
                for (a,b) in deliveryZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")

                # No pickup until dropped
                slugsFile.write("| ! deliverypickedup"+str(activeColor)+" ! pickup'\n")

                # Requests are always satisfied -- Crying for more immediately may be ignored.
                slugsFile.write("\n[SYS_LIVENESS]\n! pendingdelivery"+str(activeColor)+"'\n")


    elif (colorInfo!=None) and colorInfo.startswith("MovingObstacle:"):
        # =============================================================
        # Moving Obstacle
        # =============================================================
        movingObstacleInfo = colorInfo.split(":")
        speedMO = int(movingObstacleInfo[1]) # 0 or 1 - 0 is twice as slow
        xsizeMO = int(movingObstacleInfo[2])
        ysizeMO = int(movingObstacleInfo[3])
        blockades = [int(a) for a in movingObstacleInfo[4].split(",")]

        # Assign position bits
        slugsFile.write("\n[INPUT]\n")
        for i in xrange(0,nofXCoordinateBits):
            slugsFile.write("mox"+str(activeColor)+"_"+str(i)+"\n")
        for i in xrange(0,nofYCoordinateBits):
            slugsFile.write("moy"+str(activeColor)+"_"+str(i)+"\n")

        # Define position encoding helpers
        def encodeMOXValue(x):
            parts = ["&"]*(nofXCoordinateBits-1)
            for i in xrange(0,nofXCoordinateBits):
                if (x & (1 << i))==0:
                    parts.append("!")
                parts.append("mox"+str(activeColor)+"_"+str(i))
            return " ".join(parts)

        def encodeMOYValue(x):
            parts = ["&"]*(nofYCoordinateBits-1)
            for i in xrange(0,nofYCoordinateBits):
                if (x & (1 << i))==0:
                    parts.append("!")
                parts.append("moy"+str(activeColor)+"_"+str(i))
            return " ".join(parts)

        def encodeMOXPrimeValue(x):
            parts = ["&"]*(nofXCoordinateBits-1)
            for i in xrange(0,nofXCoordinateBits):
                if (x & (1 << i))==0:
                    parts.append("!")
                parts.append("mox"+str(activeColor)+"_"+str(i)+"'")
            return " ".join(parts)

        def encodeMOYPrimeValue(x):
            parts = ["&"]*(nofYCoordinateBits-1)
            for i in xrange(0,nofYCoordinateBits):
                if (x & (1 << i))==0:
                    parts.append("!")
                parts.append("moy"+str(activeColor)+"_"+str(i)+"'")
            return " ".join(parts)

        # Find the initial pixel
        initial = None
        for x in xrange(0,xsize):
            for y in xrange(0,ysize):
                if imageData[y*xsize+x]==activeColor:
                    if initial!=None:
                        print >>sys.stderr,"Error with moving obstacle with color "+activeColor+": more than one starting position found."
                        sys.exit(1)
                    initial = (x,y)
        if initial==None:
            print >>sys.stderr,"Error with moving obstacle with color "+str(activeColor)+": no starting position found."
            sys.exit(1)
        slugsFile.write("\n[ENV_INIT]\n& "+encodeMOXValue(initial[0])+" "+encodeMOYValue(initial[1])+"\n")
        
        # Speed restricter
        if speedMO==0:
            # May only move every second computation cycle - Prepare monitor
            slugsFile.write("\n[OUTPUT]\n")
            slugsFile.write("MOSpeederMonitor"+str(activeColor)+"\n")
            slugsFile.write("[SYS_INIT]\n")
            slugsFile.write("! MOSpeederMonitor"+str(activeColor)+"\n")
            slugsFile.write("[SYS_TRANS]\n")
            slugsFile.write("| ! MOSpeederMonitor"+str(activeColor)+" ! MOSpeederMonitor"+str(activeColor)+"'\n")
            slugsFile.write("| MOSpeederMonitor"+str(activeColor)+" MOSpeederMonitor"+str(activeColor)+"'\n")
            slugsFile.write("[ENV_TRANS]\n")
            # Prepare constraints
            slugsFile.write("& "*(2*nofXCoordinateBits+2*nofYCoordinateBits)+"1")
            for i in xrange(0,nofXCoordinateBits):
                slugsFile.write(" | | MOSpeederMonitor"+str(activeColor)+" ! mox"+str(activeColor)+"_"+str(i)+" mox"+str(activeColor)+"_"+str(i)+"'")
                slugsFile.write(" | | MOSpeederMonitor"+str(activeColor)+" mox"+str(activeColor)+"_"+str(i)+" ! mox"+str(activeColor)+"_"+str(i)+"'")
            for i in xrange(0,nofYCoordinateBits):
                slugsFile.write(" | | MOSpeederMonitor"+str(activeColor)+" ! moy"+str(activeColor)+"_"+str(i)+" moy"+str(activeColor)+"_"+str(i)+"'")
                slugsFile.write(" | | MOSpeederMonitor"+str(activeColor)+" moy"+str(activeColor)+"_"+str(i)+" ! moy"+str(activeColor)+"_"+str(i)+"'")
            slugsFile.write("\n")

        # Correct motion by the moving obstacle!
        slugsFile.write("\n[ENV_TRANS]\n")
        fromConstraints = ["1"]
        for x in xrange(0,xsize):
            for y in xrange(0,ysize):
                fromCase = "| ! "+encodeMOXValue(x)+" ! "+encodeMOYValue(y)
                toCases = ["0"]
                for (xmove,ymove) in [(-1,0),(1,0),(0,0),(0,-1),(0,1)]:
                    newX = x + xmove
                    newY = y + ymove
                    if (newX <= (xsize-xsizeMO)) and (newY <= (ysize-ysizeMO)):
                        conflict = False
                        for i in xrange(newX,newX+xsizeMO):
                            for j in xrange(newY,newY+ysizeMO):
                                conflict = conflict or (imageData[j*xsize+i] in blockades)
                                # if (imageData[j*xsize+i] in blockades):
                                #     print "Conflict at (",i,",",j,")"
                                #     print blockades
                        if not conflict:
                            toCases.append("& "+encodeMOXPrimeValue(newX)+" "+encodeMOYPrimeValue(newY))
                fromConstraints.append("| "+fromCase+" "+"| "*(len(toCases)-1)+" ".join(toCases))
        slugsFile.write("& "*(len(fromConstraints)-1)+" ".join(fromConstraints)+"\n")
                       
        # No collision
        slugsFile.write("\n[SYS_TRANS]\n")
        slugsFile.write("# No collision")
        clashConstraints = ["1"]
        for x in xrange(0,xsize):
            for y in xrange(0,ysize):
                for i in xrange(x,x+xsizeMO):
                    for j in xrange(y,y+ysizeMO):
                        clashConstraints.append("| | | ! "+encodeMOXPrimeValue(x)+" ! "+encodeMOYPrimeValue(y)+" ! "+encodeXPrimeValue(i)+" ! "+encodeYPrimeValue(j))
        slugsFile.write(""*(len(clashConstraints)-1)+"\n".join(clashConstraints)+"\n")
        

    elif colorInfo!=None:
        print >>sys.stderr,"Error: Don't know what to do with color type: "+colorInfo
        sys.exit(1)

slugsFile.close()
        
