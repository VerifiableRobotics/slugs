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
slugsFiles = sorted(slugsFiles, key=operator.itemgetter(0))
slugsIncrementalFiles = [a for (a,b) in slugsFiles if a.endswith(".slugsincremental")]
slugsNonIncrementalFiles = [a for (a,b) in slugsFiles if a.endswith(".slugsin")]

# Write a makefile!!!!
makefile = open("Makefile","w")
makefile.write("default: outdir "+" ".join([".out/"+a+".txt" for a in slugsIncrementalFiles+slugsNonIncrementalFiles]+[".out/"+a+"_norecycling.txt" for a in slugsNonIncrementalFiles])+"\n\n")
makefile.write("outdir:\n\tmkdir .out | echo \"\"\n\n")
for a in slugsIncrementalFiles:
    makefile.write(".out/"+a+".txt: outdir "+a+"\n\tslugs --experimentalIncrementalSynthesis < "+a+" > .out/"+a+".txt 2> .out/"+a+".error\n\n")
for a in slugsNonIncrementalFiles:
    makefile.write(".out/"+a+".txt: outdir "+a+"\n\tslugs --onlyRealizability --fixedPointRecycling "+a+" > .out/"+a+".txt 2> .out/"+a+".error\n\n")
    makefile.write(".out/"+a+"_norecycling.txt: outdir "+a+"\n\tslugs --onlyRealizability "+a+" > .out/"+a+"_norecycling.txt 2> .out/"+a+"_norecycling.error\n\n")
makefile.close()

