#!/usr/bin/python
#
# Translates from LittleMOP to the Slugs format

import math
import os
import sys
import resource
import subprocess
import signal
from parser import Parser
from re import match
import StringIO


# Allocate global parser
p = Parser()

# =====================================================
# Lexer for the PSL formulas
# =====================================================
def tokenize(str):

    res = []
    while str:
        # Ignoring stuff        
        if str[0].isspace() or (str[0]=='\n'):
            str = str[1:]
            continue

        if str.startswith("FALSE"):
            str = str[5:]
            res.append(("FALSE",))
            continue

        if str.startswith("TRUE"):
            str = str[4:]
            res.append(("TRUE",))
            continue

        if str.startswith("next"):
            str = str[4:]
            res.append(("next",))
            continue

        m = match('[a-zA-Z0-9_.]+', str)
        if m:
            # Special case: ["X","F","U","W","0","1"]
            if m.end()==1:
                if str[0] in ["X","F","G","U","W","0","1"]:
                    res.append((str[0],))
                    str = str[1:]
                else:
                    res.append(('id', m.group(0)))
                    str = str[m.end(0):]
            else:
                res.append(('id', m.group(0)))
                str = str[m.end(0):]
            continue

        res.append((str[0],))
        str = str[1:]
    return res

# =====================================================
# Simplify the specifications
# =====================================================
def clean_tree(tree):
    """ Cleans a parse Tree, i.e. removes brackets and so on """
    if tree[0] in p.terminals:
        return tree
    if (tree[0]=="Brackets"):
        return clean_tree(tree[2])
    elif (tree[0]=="Implication") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="Atomic") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="Conjunction") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="Biimplication") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="Disjunction") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="Xor") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="BinaryTemporalFormula") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="UnaryFormula") and (len(tree)==2):
        return clean_tree(tree[1])
    elif (tree[0]=="AtomicFormula"):
        if len(tree)!=2:
            raise ValueError("AtomicFormula must have length 2")
        return clean_tree(tree[1])
    elif (tree[0]=="Implication"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[3])]
    elif (tree[0]=="Conjunction"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[3])]
    elif (tree[0]=="Biimplication"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[3])]
    elif (tree[0]=="Disjunction"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[3])]
    elif (tree[0]=="Xor"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[3])]
    elif (tree[0]=="BinaryTemporalFormula"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[2]),clean_tree(tree[3])]
    elif (tree[0]=="UnaryFormula"):
        return [tree[0],clean_tree(tree[1]),clean_tree(tree[2])]
    elif (tree[0]=="BinaryTemporalOperator"):
        # Remove the "superfluous indirection"
        return clean_tree(tree[1])
    elif (tree[0]=="UnaryTemporalOperator"):
        # Remove the "superfluous indirection"
        return clean_tree(tree[1])
    elif (tree[0]=="Assignment"):
        # Flatten "id" case
        A = [tree[0],tree[1][1]]
        A.extend(tree[2:])
        return A
    else:
        A = [tree[0]]
        for x in tree[1:]:
           A.append(clean_tree(x))
        return A

def flatten_as_much_as_possible(tree):
    """ Flattens nested disjunctions/conjunctions """
    # Ground case?
    if len(tree)==1:
        return tree
    if (type(tree)==type("A")) or (type(tree)==type(u"A")): # TODO: How to do this in the way intended?
        return tree
    newTree = []
    for a in tree:
        newTree.append(flatten_as_much_as_possible(a))
    tree = newTree

    # Conjunction
    if (tree[0]=="Conjunction"):
        parts = [tree[0]]
        for a in tree[1:]:
            if a[0]=="Conjunction":
                parts.extend(a[1:])
            else:
                parts.append(a)
        return parts

    # Disjunction
    if (tree[0]=="Disjunction"):
        parts = [tree[0]]
        for a in tree[1:]:
            if a[0]=="Disjunction":
                parts.extend(a[1:])
            else:
                parts.append(a)
        return parts

    # Xor
    if (tree[0]=="Xor"):
        parts = [tree[0]]
        for a in tree[1:]:
            if a[0]=="Xor":
                parts.extend(a[1:])
            else:
                parts.append(a)
        return parts

    # Every other case
    return tree



# =====================================================
# The Parsing function
# =====================================================
def parseLTL(ltlTxt):

    try:
        input = tokenize(ltlTxt)
        tree = p.parse(input)

    except p.ParseErrors, exception:
        for t,e in exception.errors:
            if t[0] == p.EOF:
                print >>sys.stderr, "Formula end not expected here"
                continue

            found = repr(t[0])
            if len(e) == 1:
                print >>sys.stderr, "Error in LTL formula: "+ltlTxt
                print >>sys.stderr, "Expected %s, but found %s " %(repr(e[0]), found)
            else:
                print >>sys.stderr, "Error in LTL formula: "+ltlTxt
                print >>sys.stderr, "Could not parse %s, "%found
                print >>sys.stderr, "Wanted a token of one of the following forms: "+", ".join([ repr(s) for s in e ])
        raise

    # Convert to a tree
    cleaned_tree = flatten_as_much_as_possible(clean_tree(tree))
    return cleaned_tree

def performConversion(orderFile,ltlFile):

    # Read Order File
    orderFileStream = open(orderFile,"r")
    orderFileLines = orderFileStream.readlines()

    # Read LTL file
    ltlFileStream = open(ltlFile,"r")
    mode = 0
    group = "NOT_ASSIGNED"
    stringsOfType = {"NOT_ASSIGNED":{1:"",2:""}}
    for line in ltlFileStream.readlines():
        line = line.strip()
        # print >>sys.stderr,line
        if line.startswith("#"):
            if line.endswith(":"):
                group = line[1:len(line)-1].strip()
                if not stringsOfType.has_key(group):
                    stringsOfType[group] = {}
                    for i in xrange(1,9):
                        stringsOfType[group][i] = ""
        elif (line==""):
            pass # Empty Line        
        elif (line=="[INPUT_VARIABLES]"):
            mode = 1
        elif (line=="[OUTPUT_VARIABLES]"):
            mode = 2
        elif (line=="[ENV_INITIAL]"):
            mode = 3
        elif (line=="[SYS_INITIAL]"):
            mode = 4
        elif (line=="[ENV_TRANSITIONS]"):
            mode = 5
        elif (line=="[SYS_TRANSITIONS]"):
            mode = 6
        elif (line=="[ENV_FAIRNESS]"):
            mode = 7
        elif (line=="[SYS_FAIRNESS]"):
            mode = 8
        elif line[0]=="[":
            print >>sys.stderr, "Error: Cannot recognize section: "+line
            raise        
        else:
            if mode==0:
                print >>sys.stderr, "Error: Expective setting the section before the first activity."
                print >>sys.stderr, "Line: "+line
                raise
            stringsOfType[group][mode] = stringsOfType[group][mode]+line
    ltlFileStream.close()

    # Read inputBits
    inputBits = [a.strip() for a in stringsOfType["NOT_ASSIGNED"][1].split(";") if a.strip()!=""]
    outputBits = [a.strip() for a in stringsOfType["NOT_ASSIGNED"][2].split(";") if a.strip()!=""]

    # ==================================
    # Build Slugs file - I/O Variables
    # ==================================
    print "EXITONERROR 1"
    for bit in inputBits:
        print "INPUT "+bit
    for bit in outputBits:
        print "OUTPUT "+bit
    print ""

    if len(stringsOfType["NOT_ASSIGNED"])!=2:
        print >>sys.stderr,"Error: Unguarded properties in the input file."
        raise
    stringsOfType.pop("NOT_ASSIGNED",None)


    # ==================================
    # Build Slugs file - Rest
    # ==================================
    for line in orderFileLines:
        line = line.strip()
        if len(line)>0 and line[0]!="#":
            toDelete = []
            currentCategory = line          
            for groups in stringsOfType.keys():
                # print "Considering group: "+groups+" for "+line
                if not groups in toDelete:
                    for group in groups.split(","):
                        group = group.strip()
                        compressedGroupName = groups.replace(" ","")
                        if group==currentCategory:
                            # print "Match "+group

                            assumptionProperties = [a.strip() for a in stringsOfType[groups][3].split(";")]+[a.strip() for a in stringsOfType[groups][5].split(";")]+[a.strip() for a in stringsOfType[groups][7].split(";")]
                            guaranteeProperties = [a.strip() for a in stringsOfType[groups][4].split(";")]+[a.strip() for a in stringsOfType[groups][6].split(";")]+[a.strip() for a in stringsOfType[groups][8].split(";")]

                            assumptionProperties = [parseLTL(a+";") for a in assumptionProperties if a!=""]
                            guaranteeProperties = [parseLTL(a+";") for a in guaranteeProperties if a!=""]

                            # Iterate over the property types.
                            print "# "+groups
                            buildPropertySet(assumptionProperties,"A",compressedGroupName,False,False)
                            buildPropertySet(assumptionProperties,"A",compressedGroupName,False,True)
                            buildPropertySet(guaranteeProperties,"G",compressedGroupName,True,False)
                            buildPropertySet(guaranteeProperties,"G",compressedGroupName,True,True)

                            toDelete.append(groups)
            print "CR"
            if toDelete==[]:
                print >>sys.stderr,"Error: Cannot fit category: "+currentCategory
                sys.exit(1)
            for a in toDelete:
                stringsOfType.pop(a,None)

    if len(stringsOfType)>0:
        print >>sys.stderr, "Warning: Did not use all property types: "
        for a in stringsOfType.keys():
            print >>sys.stderr," - "+a

# ============================================
# Build Slugs file - Temporal logic properties
# ============================================
def parseSimpleFormula(tree, isPrimed):
    if (tree[0]=="Biimplication"):
        b1 = parseSimpleFormula(tree[1],isPrimed)
        b2 = parseSimpleFormula(tree[2],isPrimed)
        return ["|","&","!"]+b1+["!"]+b2+["&"]+b1+b2
    if (tree[0]=="Implication"):
        b1 = parseSimpleFormula(tree[1],isPrimed)
        b2 = parseSimpleFormula(tree[2],isPrimed)
        return ["|","!"]+b1+b2
    if (tree[0]=="Conjunction"):
        ret = parseSimpleFormula(tree[1],isPrimed)
        for a in tree[2:]:
            ret = ["&"]+ret+parseSimpleFormula(a,isPrimed)
        return ret
    if (tree[0]=="Disjunction"):
        ret = parseSimpleFormula(tree[1],isPrimed)
        for a in tree[2:]:
            ret = ["|"]+ret+parseSimpleFormula(a,isPrimed)
        return ret
    if (tree[0]=="UnaryFormula"):
        if tree[1][0]=="NotOperator":
            return ["!"]+parseSimpleFormula(tree[2],isPrimed)
        elif tree[1][0]=="NextOperator":
            if isPrimed:
                raise "Nested nexts are not allowed."
            return parseSimpleFormula(tree[2],True)
    if (tree[0]=="Assignment"):
        var = tree[1]
        if isPrimed:
            var = var + "'"
        if len(tree)==4 and tree[3][0]=="0":
            return ["!",var]
        elif len(tree)==4 and tree[3][0]=="1":
            return [var]
        else:
            print >>sys.stderr, "Unexpected RATSY input."
            raise
    if (tree[0]=="TRUE"):
        return ["1"]
    if (tree[0]=="FALSE"):
        return ["0"]
    else:
        print "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        print tree
        raise 123
    

def buildPropertySet(properties,fileprefix,namePrefix,startWithNextOperatorForLivenessProperties,handleInitialisationProperties):

    # Iterate over the types
    initializationProperties = []
    safetyProperties = []
    livenessProperties = []

    for tree in properties:

        if (tree[0]!="Formula"):
            print tree
            raise "Wrong tree type"
        tree = tree[1]    
        conjuncts = []
        if tree[0]=="Conjunction":
            conjuncts = tree[1:]
        else:
            conjuncts = [tree]

        for c in conjuncts:
            # print c
            if c[0]=="UnaryFormula":
                if c[1][0]!="GloballyOperator":
                    initializationProperties.append(parseSimpleFormula(c,False))
                    # print "INIT!"
                else:
                    rest = c[2]
                    if rest[0]=="UnaryFormula":
                        if rest[1][0]!="FinallyOperator":
                            safetyProperties.append(parseSimpleFormula(rest,False))
                            # print "SAFETY!"
                        else:
                            livenessProperties.append(parseSimpleFormula(rest[2],startWithNextOperatorForLivenessProperties))
                            # print "LIVENESS!"
                    else:
                        safetyProperties.append(parseSimpleFormula(rest,False))
                        # print "SAFETY2!"
            else:
                initializationProperties.append(parseSimpleFormula(c,False))
                # print "INIT2!"

    if handleInitialisationProperties:
        number = 0
        for a in initializationProperties:
            print "AI"+fileprefix+" "+namePrefix+fileprefix+"I"+str(number)+" "+" ".join(a)
            number += 1
    else:
        number = 0
        for a in safetyProperties:
            print "AS"+fileprefix+" "+namePrefix+fileprefix+"S"+str(number)+" "+" ".join(a)
            number += 1
        for a in livenessProperties:
            print "AL"+fileprefix+" "+namePrefix+fileprefix+"L"+str(number)+" "+" ".join(a)
            number += 1

# ==================================
# Entry point
# ==================================
if __name__ == "__main__":
    if len(sys.argv)<3:
        print >>sys.stderr, "Error: Expecting Order and Ratsy file as parameters"
        sys.exit(1)

    orderFile = sys.argv[1]
    ltlFile = sys.argv[2]
    performConversion(orderFile,ltlFile)


