#! /usr/bin/env python
# -*- coding: ISO-8859-15 -*-
# 
# Python script to create a Makefile for evaluating synthesis performance
#
from __future__ import with_statement
import os
import pprint
import binascii
import md5
import lxml
import inspect
import operator
import sys

# Get list of slugs files
slugsFiles = [ (files, os.path.getsize(files)) for files in os.listdir(".") ]
slugsFiles = sorted(slugsFiles, key=operator.itemgetter(1))
slugsIncrementalFiles = [a for (a,b) in slugsFiles if a.endswith(".slugsincremental")]

fileEndings = [".slugsincremental",".slugsin",".slugsin_norecycling"]


# Evaluate!
resultfile = open("result.txt","w")
resultfile.write("Results:\n\n")
lines = [None,["File name"]+fileEndings,None]

for incrementalFile in slugsIncrementalFiles:
    filebase = incrementalFile[0:incrementalFile.rindex(".")]

    # Try to read incremenetal file.
    columns = []
    try:
        resultFile = open(".out/"+incrementalFile+".txt","r")
        timeNext = False
        timesIncremental = []
        for line in resultFile.readlines():
            line = line.strip()
            if line=="RESULT: Specification is realizable.":
                timeNext = True
            elif line=="RESULT: Specification is unrealizable.":
                timeNext = True
            elif timeNext:
                if line.startswith("Time (in s): "):
                    timesIncremental.append(line[13:])
                    timeNext = False
                elif line.startswith("Hash "):
                    pass # Hashes may come in between
                else:
                    print >>sys.stderr, "Error in file .out/"+incrementalFile+".txt - expecting a time after a result."
                    sys.exit(1)
        if timeNext:
            print >>sys.stderr, "Error in file .out/"+incrementalFile+".txt - Result comes last!"
            sys.exit(1)
        columns.append(timesIncremental)
    except IOError, e:
        columns.append(["I/O Error"])    
    
    # Now start the others    
    for fileEnding in fileEndings[1:]:
        thisColumn = []
        timesSoFar = 0.0
        for i in xrange(0,len(timesIncremental)):
            timesFound = None
            try:
                resultFile = open(".out/"+filebase+"_"+str(i+1)+fileEnding+".error","r")
                timeNext = False
                for line in resultFile.readlines():
                    line = line.strip()
                    if line=="RESULT: Specification is realizable.":
                        timeNext = True
                    elif line=="RESULT: Specification is unrealizable.":
                        timeNext = True
                    elif timeNext:
                        if line.startswith("Time (in s): "):
                            timesSoFar += float(line[13:])
                            timesFound = timesSoFar
                            timeNext = False
                        elif line.startswith("Hash "):
                            pass # Hashes may come in between
                        else:
                            print >>sys.stderr, "Error in file - expecting a time after a result."
                            sys.exit(1)
                if timeNext:
                    print >>sys.stderr, "Error in file - Result comes last!"
                    sys.exit(1)
            except IOError, e:
                timesFound = "I/O error: .out/"+filebase+"_"+str(i+1)+fileEnding+".error"   
            thisColumn.append(str(timesFound))
        columns.append(thisColumn)

    leftColumn = filebase
    for a in xrange(0,len(columns[0])):
        newLine = [leftColumn]
        leftColumn = ""
        for k in columns:
            newLine.append(k[a])
        lines.append(newLine)
    lines.append(None)    
    
# Print graph
maxLens = [0 for a in fileEndings+[None]]
for a in lines:
    if a!=None:
        for i in xrange(0,len(maxLens)):
            maxLens[i] = max(maxLens[i],len(a[i]))
for a in lines:
    if a == None:
        for j in xrange(0,len(maxLens)):
            resultfile.write("+")
            for i in xrange(0,maxLens[j]+2):
                resultfile.write("-")
        resultfile.write("+")
    else:
        for j in xrange(0,len(maxLens)):
            resultfile.write("| ")
            resultfile.write(a[j])
            for i in xrange(len(a[j]),maxLens[j]+1):
                resultfile.write(" ")
        resultfile.write("|")
    resultfile.write("\n")
resultfile.close()
