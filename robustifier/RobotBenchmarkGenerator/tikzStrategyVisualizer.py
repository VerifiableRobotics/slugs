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
if len(sys.argv)<3:
    print >>sys.stderr, "Error: Need PNG file and strategy as parameter"
    sys.exit(1)
specFile = sys.argv[1]
automatonFile = sys.argv[2]

# ==================================
# Read input image
# ==================================
import os,sys
pngfile = Image.open(specFile)
# print "Size of Workspace:",pngfile.size
xsize = pngfile.size[0]
ysize = pngfile.size[1]
imageData = pngfile.getdata()

# =============================================================
# Locate "inactive reagions" (Doors, Pick-up zones, drop zones, etc.)
# =============================================================
nonFeatureColors = set([0,7])
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

# print "ActionZones: "
colorsUsed = set([])
for (a,b) in actionZones:
    colorsUsed.add(a)
#     print " - "+str(a)+": "+" ".join([str(c) for c in b])

# =============================================================
# Count doors and delivery request
# =============================================================
deliveries = []
doors = []
for (a,b) in actionZones:
    if ((a%2)==0) and (not (a+1 in colorsUsed)):
        doors.append(b)
    elif (a>=2) and ((a%2)==0):
        deliveries.append((a,b))

# =============================================================
# Function to print a Tikz Figure
# =============================================================
def printBasicTikzFigure():
    # Basic
    for x in xrange(0,xsize):
        for y in xrange(0,ysize):
            print "\\draw[line width=0.2pt,color=black!50!white,fill=black!10!white] ("+str(x)+","+str(y)+") rectangle ("+str(x+1)+","+str(y+1)+");"
    # Walls
            if imageData[y*xsize+x]==1:
                print "\\fill[color=black!50!white,line width=0.2pt,fill=black] ("+str(x)+","+str(y)+") rectangle ("+str(x+1)+","+str(y+1)+");"
    
    
    # Complex objects
    for pngCode in [a for (a,b) in actionZones]:
        # Collect lines
        lines = set([])
        for x in xrange(0,xsize):
            for y in xrange(0,ysize):
                if imageData[y*xsize+x]==pngCode:
                    lines.update([(x,y,x+1,y),(x+1,y,x+1,y+1),(x+1,y+1,x,y+1),(x,y+1,x,y)])
        # Filter lines
        filteredLines = set([(x1,y1,x2,y2) for (x1,y1,x2,y2) in lines if not (x2,y2,x1,y1) in lines])
        # Now start somewhere and take all the transitions;
        if len(filteredLines)>0:
            print "\\draw[color=black!50!white,fill=img"+str(pngCode)+",line width=0.2pt,even odd rule]",
            while len(filteredLines)>0:         
                currentPoint = filteredLines.pop()
                print "("+str(currentPoint[0])+","+str(currentPoint[1])+")",
                notDone = True
                while notDone:
                    print "-- ("+str(currentPoint[2])+","+str(currentPoint[3])+")",
                    nextPoint = None
                    for a in filteredLines:
                        if (currentPoint[2]==a[0]) and (currentPoint[3]==a[1]):
                            nextPoint = a
                    if nextPoint==None:
                        notDone = False
                    else:
                        filteredLines.remove(nextPoint)
                        currentPoint = nextPoint
                print "",
            print ";"


# =============================================================
# Compute coordinates
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


# =============================================================
# Read strategy
# =============================================================
automatonIn = open(automatonFile,"r")
lines = automatonIn.readlines()

# Compute set of "free" vars
firstline = lines[0].strip()
vars = firstline[firstline.find("<")+1:len(firstline)-1].split(", ")
vars = [a.split(":")[0] for a in vars]
vars = [a for a in vars if not a in ["left","right","up","down"]]
vars = [a for a in vars if not a in ["x"+str(i) for i in xrange(0,nofXCoordinateBits)]]
vars = [a for a in vars if not a in ["y"+str(i) for i in xrange(0,nofYCoordinateBits)]]

# =============================================================
# Print Header
# =============================================================
print "\\documentclass[a4paper,10pt,DIV15]{scrartcl}"
print "\\usepackage{tikz}"
print "\\begin{document}"
# Color
palette = pngfile.getpalette()
for i in xrange(0,256):
    print "\definecolor{img"+str(i)+"}{RGB}{"+str(palette[i*3])+","+str(palette[i*3+1])+","+str(palette[i*3+2])+"}"



# =============================================================
# Parse everything
# =============================================================
preStatesTotal = {}
states = {}

for preVarCombination in xrange(0,(1 << len(vars))):

    # Compute the way from here
    preStates = []
    for linenr in xrange(0,len(lines),2):
        line = lines[linenr].strip()
        stateNr = int(line.split(" ")[1])
        if not line.startswith("State"):
            print >>sys.stderr.write("Error in strategy file: Expected State line")
        valuation = line[line.find("<")+1:len(line)-1].split(", ")
        fits = True
        for i,v in enumerate(vars):
            if (preVarCombination & (1<<i))>0:
                fits = fits and ((v+":1") in valuation)
            else:
                fits = fits and ((v+":0") in valuation)

        if fits:
            xmove = (1 if ("right:1" in valuation) else 0) + (-1 if ("left:1" in valuation) else 0)
            ymove = (1 if ("down:1" in valuation) else 0) + (-1 if ("up:1" in valuation) else 0)
            xpos = 0
            for i in xrange(0,nofXCoordinateBits):
                if "x"+str(i)+":1" in valuation:
                    xpos += (1 << i)         
            ypos = 0
            for i in xrange(0,nofYCoordinateBits):
                if "y"+str(i)+":1" in valuation:
                    ypos += (1 << i)        

            successorLine = lines[linenr+1].strip()
            if not successorLine.startswith("With successors : "):
                print >>sys.stderr,"Expected successors in input file."
                print >>sys.stderr,"But i got: "+successorLine
                sys.exit(1)
            successors = successorLine[18:].split(", ")
            successors = [int(a) for a in successors]
            preStates.append((stateNr,xpos,ypos,xmove,ymove,successors))
            states[stateNr] = (preVarCombination,xpos,ypos,xmove,ymove,successors)

    preStatesTotal[preVarCombination] = preStates

# =============================================================
# Print the graphs
# =============================================================
for preVarCombination in xrange(0,(1 << len(vars))):

    # Anything? Then add section header
    if len(preStatesTotal[preVarCombination])>0:
        print "\\section{ From: $",
        for i,v in enumerate(vars):
            if i>0:
                print ", \\allowbreak",
            if (preVarCombination & (1<<i))==0:
                print "\\neg",
            print v,
        print "$}"

        # print "General plot:\n\n"

        print "\\resizebox{0.99\\linewidth}{!}{\\begin{tikzpicture}[yscale=-1]"
        printBasicTikzFigure()
        for (stateNr,xpos,ypos,xmove,ymove,successors) in preStatesTotal[preVarCombination]:
            if (xmove==0) and (ymove==0):
                print "\\draw[fill,color=black,line width=0.2pt,fill=red] ("+str(xpos+0.5)+","+str(ypos+0.5)+") circle (0.1cm);"
        for (stateNr,xpos,ypos,xmove,ymove,successors) in preStatesTotal[preVarCombination]:
            if not ((xmove==0) and (ymove==0)):
                print "\\draw[->,thick,color=red] ("+str(xpos+0.5)+","+str(ypos+0.5)+") -- ("+str(xpos+xmove*0.92+0.5)+","+str(ypos+ymove*0.92+0.5)+");"
        print "\\end{tikzpicture}}"

        
    
        # for postVarCombination in xrange(0,(1 << len(vars))):
        #
        #    # Lookup successors
        #    arrows = []
        #    for (stateNr,xpos,ypos,xmove,ymove,successors) in preStatesTotal[preVarCombination]:
        #        for s in successors:
        #            if states[s][0]==postVarCombination:
        #                arrows.append((stateNr,s))
        #
        #    if len(arrows)>0:
                
                
        





    









# =============================================================
# End of the document
# =============================================================
print "\\end{document}"


sys.exit(1)























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
        if imageData[y*xsize+x]==0:
            # Forbidden zone!!!!!!
            slugsFile.write("| ! "+encodeXPrimeValue(x)+" ! "+encodeYPrimeValue(y)+"\n")
            

# =============================================================
# We can only go left or right and up or down
# =============================================================
slugsFile.write("\n[SYS_TRANS]\n| ! left' ! right'\n| ! up' ! down'\n")


# =============================================================
# Always eventually do not obstruct a door
# =============================================================
allDoors = []
for d in doors:
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
# Bring stuff from A to B whenever requested to do so!
# =============================================================
for i,(a,pickupZones) in enumerate(deliveries):
    slugsFile.write("\n[SYS_INIT]\n! pendingdelivery"+str(i)+"\n! deliverypickedup"+str(i)+"\n\n[SYS_TRANS]\n")

    deliveryZoneNumber = a+1
    deliveryZones = None
    for (a,b) in actionZones:
        if a==deliveryZoneNumber:
            if deliveryZones!=None:
                print >>sys.stderr, "Error: Non-continuous delivery zone: "+str(a)
                sys.exit(1)
            deliveryZones = b

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
    for (a,b) in deliveryZones:
        slugsFile.write("& "+encodeXPrimeValue(a)+" "+encodeYPrimeValue(b)+" ")
    slugsFile.write("0\n")
    slugsFile.write("| | deliverypickedup"+str(i)+" ! deliverypickedup"+str(i)+"' & pickup' ")
    slugsFile.write("| "*len(pickupZones))
    for (a,b) in deliveryZones:
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
        
