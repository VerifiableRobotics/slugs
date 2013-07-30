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
slugsNonIncrementalFiles = [a for (a,b) in slugsFiles if a.endswith(".slugsin")]

# Get result files
resultFiles = [(a,".out/"+a+".txt") for a in slugsIncrementalFiles]
resultFiles = resultFiles + [(a,".out/"+a+".error") for a in slugsNonIncrementalFiles]
resultFiles = resultFiles + [(a+"_norecycle",".out/"+a+"_norecycling.error") for a in slugsNonIncrementalFiles]
resultFiles = sorted(resultFiles, key=operator.itemgetter(0))

# Evaluate!
resultfile = open("result.txt","w")
resultfile.write("Results:\n\n")
lines = [None]

for (a,b) in resultFiles:
    leftColumn = a
    try:
        resultFile = open(b,"r")
        timeNext = False
        for line in resultFile.readlines():
            line = line.strip()
            if line=="RESULT: Specification is realizable.":
                timeNext = True
            elif line=="RESULT: Specification is unrealizable.":
                timeNext = True
            elif timeNext:
                if line.startswith("Time (in s): "):
                    lines.append((leftColumn,line[13:]))
                    leftColumn = ""
                    timeNext = False
                elif line.startswith("Hash "):
                    pass # Hashes may come in between
                else:
                    print >>sys.stderr, "Error in file .out/"+a+".txt - expecting a time after a result."
                    sys.exit(1)
        if timeNext:
            print >>sys.stderr, "Error in file .out/"+a+".txt - Result comes last!"
            sys.exit(1)
        if leftColumn!="":
            lines.append((leftColumn,"no result"))    
    except IOError, e:
        lines.append((leftColumn,"I/O error"))    
    lines.append(None)

# Print graph
maxLenL = 0
maxLenR = 0
for a in lines:
    if a!=None:
        (c,d) = a
        maxLenL = max(maxLenL,len(c))
        maxLenR = max(maxLenR,len(c))
for a in lines:
    if a == None:
        resultfile.write("+")
        for i in xrange(0,maxLenL+2):
            resultfile.write("-")
        resultfile.write("+")
        for i in xrange(0,maxLenR+2):
            resultfile.write("-")
        resultfile.write("+")
    else:
        (c,d) = a
        resultfile.write("| ")
        resultfile.write(c)
        for i in xrange(len(c),maxLenL+1):
            resultfile.write(" ")
        resultfile.write("| "+d)
        for i in xrange(len(d),maxLenR+1):
            resultfile.write(" ")
        resultfile.write("|")
    resultfile.write("\n")
resultfile.close()
