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


print "ActionZones: "
colorsUsed = set([])
for (a,b) in actionZones:
    colorsUsed.add(a)
    print " - "+str(a)+": "+" ".join([str(c) for c in b])

# =============================================================
# Count doors and delivery request
# =============================================================
deliveries = []
doors = []
for (a,b) in actionZones:
    if ((a%2)==0) and (not (a+1 in colorsUsed)):
        doors.append((a,b))
    elif (a>=2) and ((a%2)==0):
        deliveries.append((a,b))

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
for i in xrange(0,len(deliveries)):
    slugsFile.write("pendingdelivery"+str(i)+"\n")
for i in xrange(0,len(deliveries)):
    slugsFile.write("deliverypickedup"+str(i)+"\n")

slugsFile.write("\n[INPUT]\n")
for i in xrange(0,nofXCoordinateBits):
    slugsFile.write("x"+str(i)+"\n")
for i in xrange(0,nofYCoordinateBits):
    slugsFile.write("y"+str(i)+"\n")
for i in xrange(0,len(deliveries)):
    slugsFile.write("deliveryrequest"+str(i)+"\n")
for i in xrange(0,len(doors)):
    slugsFile.write("door"+str(i)+"\n")

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

    if activeColor in colorsUsed:
        slugsFile.write("\n#INCREMENTAL: Color"+str(activeColor)+"\n\n")

    if activeColor in colorsUsed and not activeColor+1 in colorsUsed:

        # =============================================================
        # Always eventually do not obstruct a door - for every door individually
        # =============================================================
        allDoors = []
        for (color,d) in doors:
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
        for doorNum,doorTuple in enumerate(doors):
            if doorTuple[0]==activeColor:   
                for (a,b) in doorTuple[1]:
                    door = doorTuple[1]
                    candidates = []
                    if a>0 and not (a-1,b) in door:
                        candidates.append((a-1,b,"right"))
                    if a<(xsize-1) and not (a+1,b) in door:
                        candidates.append((a+1,b,"left"))
                    if b>0 and not (a,b-1) in door:
                        candidates.append((a,b-1,"down"))
                    if b<(ysize-1) and not (a,b+1) in door:
                        candidates.append((a,b+1,"up"))
                    if a>0 and b>0 and not (a-1,b-1) in door:
                        candidates.append((a-1,b-1,"& right down"))
                    if a>0 and b<(ysize-1) and not (a-1,b+1) in door:
                        candidates.append((a-1,b-1,"& right up"))
                    if a<(xsize-1) and b>0 and not (a+1,b-1) in door:
                        candidates.append((a-1,b-1,"& left down"))
                    if a<(xsize-1) and b<(ysize-1) and not (a+1,b+1) in door:
                        candidates.append((a-1,b-1,"& left up"))
                    for (a,b,cond) in candidates:
                        slugsFile.write("| | | ! door" + str(doorNum)+ " ! "+cond+" ! "+encodeXValue(a)+" ! "+encodeYValue(b)+"\n")
        slugsFile.write("\n\n")

        slugsFile.write("\n[ENV_LIVENESS]\n")
        for doorNum,doorTuple in enumerate(doors):
            if doorTuple[0]==activeColor:   
                slugsFile.write("! door" + str(doorNum)+"\n")
        
    if activeColor in colorsUsed and activeColor+1 in colorsUsed:

        # =============================================================
        # Bring stuff from A to B whenever requested to do so!
        # =============================================================
        for i,(a,pickupZones) in enumerate(deliveries):
            if a==activeColor:
                slugsFile.write("\n[SYS_INIT]\n! pendingdelivery"+str(i)+"\n! deliverypickedup"+str(i)+"\n\n[SYS_TRANS]\n")

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
                slugsFile.write("| | pendingdelivery"+str(i)+" ! pendingdelivery"+str(i)+"' deliveryrequest"+str(i)+"\n")
                slugsFile.write("| | pendingdelivery"+str(i)+" pendingdelivery"+str(i)+"' ! deliveryrequest"+str(i)+"\n")
                slugsFile.write("| | ! pendingdelivery"+str(i)+" pendingdelivery"+str(i)+"' drop'\n")
                slugsFile.write("| | ! pendingdelivery"+str(i)+" pendingdelivery"+str(i)+"' deliverypickedup"+str(i)+"\n")
                slugsFile.write("| | ! pendingdelivery"+str(i)+" pendingdelivery"+str(i)+" ")
                slugsFile.write("| "*len(deliveryZones))
                for (a,b) in deliveryZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")
                slugsFile.write("| | ! pendingdelivery"+str(i)+" ! pendingdelivery"+str(i)+"' | ! drop' | ! deliverypickedup"+str(i)+" ! ")
                slugsFile.write("| "*len(deliveryZones))
                for (a,b) in deliveryZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")

                # Picked up is updated correctly
                slugsFile.write("| | deliverypickedup"+str(i)+" deliverypickedup"+str(i)+"' | ! pickup' ! ")
                slugsFile.write("| "*len(pickupZones))
                for (a,b) in pickupZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")
                slugsFile.write("| | deliverypickedup"+str(i)+" ! deliverypickedup"+str(i)+"' & pickup' ")
                slugsFile.write("| "*len(pickupZones))
                for (a,b) in pickupZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")
                slugsFile.write("| | ! deliverypickedup"+str(i)+" deliverypickedup"+str(i)+"' drop'\n")
                slugsFile.write("| | ! deliverypickedup"+str(i)+" ! deliverypickedup"+str(i)+"' ! drop'\n")
                
                # Drop is only allowed at the target position
                slugsFile.write("| | ! deliverypickedup"+str(i)+" ! drop' ")
                slugsFile.write("| "*len(deliveryZones))
                for (a,b) in deliveryZones:
                    slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
                slugsFile.write("0\n")

                # No pickup until dropped
                slugsFile.write("| ! deliverypickedup"+str(i)+" ! pickup'\n")

                # Requests are always satisfied -- Crying for more immediately may be ignored.
                slugsFile.write("\n[SYS_LIVENESS]\n! pendingdelivery"+str(i)+"'\n")

slugsFile.close()
        
