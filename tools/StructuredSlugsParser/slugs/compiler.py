#!/usr/bin/env python

# Translates a structured specification into an unstructured one that is
# suitable to be read by the slugs synthesis tool

from __future__ import absolute_import

import logging
logger = logging.getLogger(__name__)

import re
import sys

from . import parser

# =====================================================
# Allocate global parser and parser context variables:
# - which APs there are
# - how the numbers are encoded
# =====================================================

# =====================================================
# Lexer for the LTL formulas
# =====================================================


def tokenize(str, booleanAPs):

    res = []
    while str:
        # Ignoring stuff
        if str[0].isspace() or (str[0] == '\n'):
            str = str[1:]
            continue

        # Match words
        m = re.match('[a-zA-Z_.\'@]+[a-zA-Z0-9_.\'@]*', str)
        if m:
            currentSymbol = m.group(0)
            if (currentSymbol in [
                    "X", "F", "G", "U", "W", "FALSE", "TRUE", "next", "LEASTSIGNIFICANTBITOVERWRITES"]):
                res.append((currentSymbol,))
            else:
                currentSymbol = m.group(0)
                if currentSymbol in booleanAPs:
                    res.append(('boolID', m.group(0)))
                else:
                    res.append(('numID', m.group(0)))
            str = str[m.end(0):]
            continue

        # Match numbers
        m = re.match('[0-9]+', str)
        if m:
            res.append(('numeral', m.group(0)))
            str = str[m.end(0):]
            continue

        # Single-literal element
        res.append((str[0],))
        str = str[1:]
    return res

# =====================================================
# Simplify the specifications
# =====================================================


def clean_tree(tree, p):
    """ Cleans a parse Tree, i.e. removes brackets and so on """
    if tree[0] in p.terminals:
        return tree
    if (tree[0] == "Brackets"):
        return clean_tree(tree[2], p)
    elif (tree[0] == "Implication") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "Atomic") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "Conjunction") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "Biimplication") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "Disjunction") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "Xor") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "LeastSignificantBitOverwriteExpression") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "BinaryTemporalFormula") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "BooleanAtomicFormula") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "BooleanAtomicFormula") and (len(tree) != 2):
        raise Exception("BooleanAtomic formula must have only one successor")
    elif (tree[0] == "UnaryFormula") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "MultiplicativeNumber") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "NumberExpression") and (len(tree) == 2):
        return clean_tree(tree[1], p)
    elif (tree[0] == "AtomicNumberExpression"):
        if len(tree) != 2:
            raise ValueError("AtomicNumberExpression must have length 2")
        return clean_tree(tree[1], p)
    elif (tree[0] == "AtomicFormula"):
        if len(tree) != 2:
            raise ValueError("AtomicFormula must have length 2")
        return clean_tree(tree[1], p)
    elif (tree[0] == "Implication"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(tree[3], p)]
    elif (tree[0] == "Conjunction"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(tree[3], p)]
    elif (tree[0] == "Biimplication"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(tree[3], p)]
    elif (tree[0] == "Disjunction"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(tree[3], p)]
    elif (tree[0] == "Xor"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(tree[3], p)]
    elif (tree[0] == "BinaryTemporalFormula"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(
            tree[2], p), clean_tree(tree[3], p)]
    elif (tree[0] == "UnaryFormula"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(tree[2], p)]
    elif (tree[0] == "MultiplicativeNumber"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(
            tree[2], p), clean_tree(tree[3], p)]
    elif (tree[0] == "NumberExpression"):
        return [tree[0], clean_tree(tree[1], p), clean_tree(
            tree[2], p), clean_tree(tree[3], p)]
    elif (tree[0] == "BinaryTemporalOperator"):
        # Remove the "superfluous indirection"
        return clean_tree(tree[1], p)
    elif (tree[0] == "UnaryTemporalOperator"):
        # Remove the "superfluous indirection"
        return clean_tree(tree[1], p)
    elif (tree[0] == "Assignment"):
        # Flatten "id" case
        A = [tree[0], tree[1][1]]
        A.extend(tree[2:])
        return A
    else:
        A = [tree[0]]
        for x in tree[1:]:
            A.append(clean_tree(x, p))
        return A


def flatten_as_much_as_possible(tree):
    """ Flattens nested disjunctions/conjunctions """
    # Ground case?
    if len(tree) == 1:
        return tree
    if (isinstance(tree, type("A"))) or (
            isinstance(tree, type(u"A"))):  # TODO: How to do this in the way intended?
        return tree
    newTree = []
    for a in tree:
        newTree.append(flatten_as_much_as_possible(a))
    tree = newTree

    # Conjunction
    if (tree[0] == "Conjunction"):
        parts = [tree[0]]
        for a in tree[1:]:
            if a[0] == "Conjunction":
                parts.extend(a[1:])
            else:
                parts.append(a)
        return parts

    # Disjunction
    if (tree[0] == "Disjunction"):
        parts = [tree[0]]
        for a in tree[1:]:
            if a[0] == "Disjunction":
                parts.extend(a[1:])
            else:
                parts.append(a)
        return parts

    # Xor
    if (tree[0] == "Xor"):
        parts = [tree[0]]
        for a in tree[1:]:
            if a[0] == "Xor":
                parts.extend(a[1:])
            else:
                parts.append(a)
        return parts

    # Every other case
    return tree

# =====================================================
# Print Tree function
# =====================================================


def printTree(tree, depth=0):
    if isinstance(tree, str):
        logger.debug(" " * depth + tree)
    else:
        logger.debug(" " * depth + tree[0])
        for a in tree[1:]:
            printTree(a, depth + 2)


# =====================================================
# The Parsing function
# =====================================================
def parseLTL(ltlTxt, reasonForNotBeingASlugsFormula, booleanAPs, p):

    try:
        input = tokenize(ltlTxt, booleanAPs)
        # logger.debug(input)
        tree = p.parse(input)

    except p.ParseErrors as exception:
        for t, e in exception.errors:
            if t[0] == p.EOF:
                logger.error("Formula end not expected here")
                continue

            found = repr(t[0])
            logger.error("Error in the property line: " + ltlTxt)
            logger.error(
                "... which could not have been a slugs Polish notation line because of: " +
                reasonForNotBeingASlugsFormula)
            logger.error("Could not parse %s, " % found)
            logger.error(
                "Wanted a token of one of the following forms: " + ", ".join([repr(s) for s in e]))
        raise

    # Convert to a tree
    cleaned_tree = flatten_as_much_as_possible(clean_tree(tree, p))
    return cleaned_tree

# =====================================================
# Slugs functions for working with numbers
# This function computes a "memory structure" for the
# slugs input whose final value is the result of the comparison
# =====================================================

#------------------------------------------
# Computation Recursion function
# Assumes that every variable starts from 0
#------------------------------------------


def recurseCalculationSubformula(
        tree, memoryStructureParts, isPrimed, translatedNames):
    if (tree[0] == "NumberExpression"):
        # Everything else would have been filtered out already
        assert len(tree) == 4
        part1MemoryStructurePointers = recurseCalculationSubformula(
            tree[1],
            memoryStructureParts,
            isPrimed,
            translatedNames)
        part2MemoryStructurePointers = recurseCalculationSubformula(
            tree[3],
            memoryStructureParts,
            isPrimed,
            translatedNames)

        # Addition?
        if tree[2][0] == "AdditionOperator":
            if len(part1MemoryStructurePointers) == 0:
                return part2MemoryStructurePointers
            if len(part2MemoryStructurePointers) == 0:
                return part1MemoryStructurePointers
            # Order the result by the number of bits
            if len(part2MemoryStructurePointers) < len(
                    part1MemoryStructurePointers):
                part1MemoryStructurePointers, part2MemoryStructurePointers = part2MemoryStructurePointers, part1MemoryStructurePointers
            startingPosition = len(memoryStructureParts)
            memoryStructureParts.append(
                ["^", part1MemoryStructurePointers[0], part2MemoryStructurePointers[0]])
            memoryStructureParts.append(
                ["&", part1MemoryStructurePointers[0], part2MemoryStructurePointers[0]])
            # First part: Joint part of the word
            for i in xrange(1, len(part1MemoryStructurePointers)):
                memoryStructureParts.append(["^ ^",
                                             part1MemoryStructurePointers[i],
                                             part2MemoryStructurePointers[i],
                                             "?",
                                             str(len(memoryStructureParts) - 1)])
                memoryStructureParts.append(["| | &",
                                             part1MemoryStructurePointers[i],
                                             part2MemoryStructurePointers[i],
                                             "& ?",
                                             str(len(
                                                 memoryStructureParts) - 2),
                                             part1MemoryStructurePointers[i],
                                             "& ?",
                                             str(len(
                                                 memoryStructureParts) - 2),
                                             part2MemoryStructurePointers[i]])

            # Second part: Part of the word where j1 is shorter than j2
            for i in xrange(len(part1MemoryStructurePointers), len(
                    part2MemoryStructurePointers)):
                memoryStructureParts.append(
                    ["^", part2MemoryStructurePointers[i], "?", str(len(memoryStructureParts) - 1)])
                memoryStructureParts.append(
                    ["&", part2MemoryStructurePointers[i], "?", str(len(memoryStructureParts) - 2)])

            # Return new value including the final bit
            return ["? " + str(i * 2 + startingPosition) for i in xrange(0, len(
                part2MemoryStructurePointers))] + ["? " + str(len(memoryStructureParts) - 1)]

        # Subtraction?
        elif tree[2][0] == "SubtractionOperator":
            raise Exception(
                "Subtraction is currently unsupported due to semantic difficulties")
        else:
            raise Exception(
                "Found unsupported NumberExpression operator:" +
                tree[2][0])

    # Parse Numeral
    elif (tree[0] == "numeral"):
        parameter = int(tree[1])
        result = []
        while parameter != 0:
            if (parameter % 2) == 1:
                result.append("1")
            else:
                result.append("0")
            parameter = parameter / 2
        return result

    # Work on NumID
    elif (tree[0] == "numID"):
        apName = tree[1]
        primedLocally = apName[len(apName) - 1] == "'"
        if primedLocally and isPrimed:
            raise Exception(
                "Variable is used primed in the scope of a next-operator, which is not supported: " +
                apName)
        if primedLocally:
            apName = apName[0:len(apName) - 1]
        if (not primedLocally) and (not isPrimed):
            return translatedNames[apName]
        else:
            return [a + "'" for a in translatedNames[apName]]

    elif (tree[0] == "NumberBrackets"):
        logger.debug(tree)
        assert tree[1] == ('(',)
        assert tree[3] == (')',)
        return recurseCalculationSubformula(
            tree[2], memoryStructureParts, isPrimed, translatedNames)

    # Overwrite the least significant bits of some expression
    elif tree[0] == "LeastSignificantBitOverwriteExpression":
        part1MemoryStructurePointers = recurseCalculationSubformula(
            tree[1],
            memoryStructureParts,
            isPrimed,
            translatedNames)
        part2MemoryStructurePointers = recurseCalculationSubformula(
            tree[3],
            memoryStructureParts,
            isPrimed,
            translatedNames)
        assert len(part1MemoryStructurePointers) < len(
            part2MemoryStructurePointers)
        return part1MemoryStructurePointers + \
            part2MemoryStructurePointers[len(part1MemoryStructurePointers):]

    else:
        raise Exception(
            "Found currently unsupported calculator subformula type:" +
            tree[0])

#-----------------------------------------------
# Special function that implicitly adds the lower
# bounds to all variable occurrences. In this
# way, it does not have to be considered in the
# actual translation algorithm.
#-----------------------------------------------


def addMinimumValueToAllVariables(tree, numberAPLimits, p):
    if tree[0] == "numID":
        apName = tree[1]
        primed = apName[len(apName) - 1] == "'"
        if primed:
            apName = apName[0:len(apName) - 1]
        (minimum, maximum) = numberAPLimits[apName]
        if minimum == 0:
            return tree
        else:
            return ("NumberExpression", tree, ("AdditionOperator",),
                    ("numeral", str(minimum)))
    elif tree[0] in p.terminals:
        return tree
    else:
        return tuple(
            [tree[0]] + [addMinimumValueToAllVariables(a, numberAPLimits, p) for a in tree[1:]])

#-----------------------------------------------
# Main Function. Takes the formula tree and adds
#-----------------------------------------------


def computeCalculationSubformula(
        tree, isPrimed, numberAPLimits, translatedNames, p):
    memoryStructureParts = []
    part1MemoryStructurePointers = recurseCalculationSubformula(
        addMinimumValueToAllVariables(
            tree[1],
            numberAPLimits,
            p),
        memoryStructureParts,
        isPrimed,
        translatedNames)
    part2MemoryStructurePointers = recurseCalculationSubformula(
        addMinimumValueToAllVariables(
            tree[3],
            numberAPLimits,
            p),
        memoryStructureParts,
        isPrimed,
        translatedNames)
    logger.debug("P1: " + str(part1MemoryStructurePointers))
    logger.debug("P2: " + str(part2MemoryStructurePointers))
    logger.debug("MSPatComp: " + str(memoryStructureParts))

    # Pick the correct comparison operator
    # ->    Greater or Greater-Equal
    if (tree[2][1][0] == "GreaterOperator") or (
            tree[2][1][0] == "GreaterEqualOperator"):
        thisParts = ["0"] if tree[2][1][0] == "GreaterOperator" else ["1"]
        for i in xrange(
                0, min(len(part1MemoryStructurePointers), len(part2MemoryStructurePointers))):
            thisParts = [
                "|",
                "&",
                part1MemoryStructurePointers[i],
                "!",
                part2MemoryStructurePointers[i],
                "& | !",
                part2MemoryStructurePointers[i],
                part1MemoryStructurePointers[i]] + thisParts
        for i in xrange(len(part1MemoryStructurePointers), len(
                part2MemoryStructurePointers)):
            thisParts = ["& !", part2MemoryStructurePointers[i]] + thisParts
        for i in xrange(len(part2MemoryStructurePointers), len(
                part1MemoryStructurePointers)):
            thisParts = ["|", part1MemoryStructurePointers[i]] + thisParts
        memoryStructureParts.append(thisParts)

    # ->    Smaller or Smaller-Equal
    elif (tree[2][1][0] == "SmallerOperator") or (tree[2][1][0] == "SmallerEqualOperator"):
        thisParts = ["0"] if tree[2][1][0] == "SmallerOperator" else ["1"]
        for i in xrange(
                0, min(len(part1MemoryStructurePointers), len(part2MemoryStructurePointers))):
            thisParts = [
                "|",
                "& !",
                part1MemoryStructurePointers[i],
                part2MemoryStructurePointers[i],
                "& |",
                part2MemoryStructurePointers[i],
                "!",
                part1MemoryStructurePointers[i]] + thisParts
        for i in xrange(len(part2MemoryStructurePointers), len(
                part1MemoryStructurePointers)):
            thisParts = ["& !", part1MemoryStructurePointers[i]] + thisParts
        for i in xrange(len(part1MemoryStructurePointers), len(
                part2MemoryStructurePointers)):
            thisParts = ["|", part2MemoryStructurePointers[i]] + thisParts
        memoryStructureParts.append(thisParts)

    # ->    Equal Operator or Unequal Operator
    elif (tree[2][1][0] == "EqualOperator") or (tree[2][1][0] == "UnequalOperator"):
        thisParts = ["1"]
        for i in xrange(
                0, min(len(part1MemoryStructurePointers), len(part2MemoryStructurePointers))):
            thisParts = [
                "&",
                "! ^",
                part1MemoryStructurePointers[i],
                part2MemoryStructurePointers[i]] + thisParts
        for i in xrange(len(part2MemoryStructurePointers), len(
                part1MemoryStructurePointers)):
            thisParts = ["& !", part1MemoryStructurePointers[i]] + thisParts
        for i in xrange(len(part1MemoryStructurePointers), len(
                part2MemoryStructurePointers)):
            thisParts = ["& !", part2MemoryStructurePointers[i]] + thisParts
        if (tree[2][1][0] == "UnequalOperator"):
            memoryStructureParts.append(["!"] + thisParts)
        else:
            memoryStructureParts.append(thisParts)

    else:
        raise Exception(
            "Found unsupported Number Comparison operator:" +
            tree[2][1][0])

    # Return memory structure
    return ["$", str(len(memoryStructureParts))] + \
        [b for a in memoryStructureParts for b in a]

# ============================================
# Build Slugs file - Temporal logic properties
# ============================================


def parseSimpleFormula(tree, isPrimed, numberAPLimits, translatedNames, p):
    if (tree[0] == "Formula"):
        assert len(tree) == 2
        return parseSimpleFormula(
            tree[1], isPrimed, numberAPLimits, translatedNames, p)
    if (tree[0] == "Biimplication"):
        b1 = parseSimpleFormula(
            tree[1],
            isPrimed,
            numberAPLimits,
            translatedNames,
            p)
        b2 = parseSimpleFormula(
            tree[2], isPrimed, numberAPLimits,
            translatedNames, p)
        return ["|", "&", "!"] + b1 + ["!"] + b2 + ["&"] + b1 + b2
    if (tree[0] == "Implication"):
        b1 = parseSimpleFormula(
            tree[1],
            isPrimed,
            numberAPLimits,
            translatedNames,
            p)
        b2 = parseSimpleFormula(
            tree[2],
            isPrimed,
            numberAPLimits,
            translatedNames,
            p)
        return ["|", "!"] + b1 + b2
    if (tree[0] == "Conjunction"):
        ret = parseSimpleFormula(
            tree[1],
            isPrimed,
            numberAPLimits,
            translatedNames,
            p)
        for a in tree[2:]:
            ret = [
                "&"] + ret + parseSimpleFormula(a, isPrimed, numberAPLimits, translatedNames, p)
        return ret
    if (tree[0] == "Disjunction"):
        ret = parseSimpleFormula(
            tree[1],
            isPrimed,
            numberAPLimits,
            translatedNames,
            p)
        for a in tree[2:]:
            ret = [
                "|"] + ret + parseSimpleFormula(a, isPrimed, numberAPLimits, translatedNames, p)
        return ret
    if (tree[0] == "UnaryFormula"):
        if tree[1][0] == "NotOperator":
            return [
                "!"] + parseSimpleFormula(tree[2], isPrimed, numberAPLimits, translatedNames, p)
        elif tree[1][0] == "NextOperator":
            if isPrimed:
                raise "Nested nexts are not allowed."
            return parseSimpleFormula(
                tree[2], True, numberAPLimits, translatedNames, p)
    if (tree[0] == "Assignment"):
        var = tree[1]
        if isPrimed:
            if "'" in var:
                raise Exception(
                    "Cannot parse input formula: variable is both primed and in the scope of a next-operator")
            var = var + "'"
        return [var]
    if (tree[0] == "TRUE"):
        return ["1"]
    if (tree[0] == "FALSE"):
        return ["0"]
    if (tree[0] == "CalculationSubformula"):
        assert len(tree) == 4
        return computeCalculationSubformula(
            tree, isPrimed, numberAPLimits, translatedNames, p)

    logger.error("Cannot parse sub-tree!")
    logger.error(tree)
    raise Exception("Slugs parsing error")


def translateToSlugsFormat(tree, numberAPLimits, translatedNames, p):
    tokens = parseSimpleFormula(
        tree,
        False,
        numberAPLimits,
        translatedNames,
        p)
    logger.debug(tokens)
    return " ".join(tokens)


# ============================================
# Function to check if an input line is already
# in slugs internal form. Checks that there
# will be one element on the stack left when
# applying the operations from right to left
# ============================================
def isValidRecursiveSlugsProperty(tokens, booleanAPs):
    tokens = [a for a in tokens if a != ""]
    if "$" in tokens:
        return (True, "Found a '$' in the property.")
    stacksize = 0
    for i in xrange(len(tokens) - 1, -1, -1):
        currentToken = tokens[i]
        if currentToken == "|" or currentToken == "&" or currentToken == "^":
            if stacksize < 2:
                return (False, "Rejected part due to stack underflow")
            stacksize -= 1
        elif currentToken == "!":
            pass
        elif currentToken == "1" or currentToken == "0":
            stacksize += 1
        else:
            # Check if valid input or output bit
            if currentToken[len(currentToken) - 1] == "'":
                currentToken = currentToken[0:len(currentToken) - 1]
            if currentToken in booleanAPs:
                stacksize += 1
            elif currentToken == "0" or currentToken == "1":
                stacksize += 1
            else:
                return (
                    False, "Rejected part \"" + tokens[i] + "\" when reading right-to-left.")
    return (stacksize == 1, "Stack size at end: " + str(stacksize))

# ============================================
# Main worker
# ============================================


def convert_to_slugsin(structured_slugs, thoroughly):
    """Convert from structured slugs format to slugs input format.

    @type structured_slugs: str

    @type thoroughly: bool
    """
    p = parser.Parser()
    booleanAPs = []
    numberAPs = []
    numberAPLimits = {}
    numberAPNofBits = {}
    translatedNames = {}

    spec_lines = structured_slugs.split('\n')

    mode = ""
    lines = {
        "[ENV_TRANS]": [],
        "[ENV_INIT]": [],
        "[INPUT]": [],
        "[OUTPUT]": [],
        "[SYS_TRANS]": [],
        "[SYS_INIT]": [],
        "[ENV_LIVENESS]": [],
        "[SYS_LIVENESS]": [],
        "[OBSERVABLE_INPUT]": [],
        "[UNOBSERVABLE_INPUT]": [],
        "[CONTROLLABLE_INPUT]": []}

    for line in spec_lines:
        line = line.strip()
        if line == "":
            pass
        elif line.startswith("["):
            mode = line
            # if not mode in lines:
            #    lines[mode] = []
        else:
            if mode == "" and line.startswith("#"):
                # Initial comments
                pass
            else:
                lines[mode].append(line)

    # ---------------------------------------
    # Reparse input lines
    # Create information along the way that
    # encodes the possible values
    # ---------------------------------------
    translatedIOLines = {
        "[INPUT]": [],
        "[OUTPUT]": [],
        "[OBSERVABLE_INPUT]": [],
        "[UNOBSERVABLE_INPUT]": [],
        "[CONTROLLABLE_INPUT]": []}
    for variableType in ["[INPUT]", "[OUTPUT]", "[OBSERVABLE_INPUT]",
                         "[UNOBSERVABLE_INPUT]", "[CONTROLLABLE_INPUT]"]:
        for line in lines[variableType]:
            if line.startswith("#"):
                translatedIOLines[variableType].append(line.strip())
            if "'" in line:
                logger.error(
                    "Error with atomic signal name " +
                    line +
                    ": the name must not contain any \"'\" characters")
                raise Exception("Translation error")
            if "@" in line:
                logger.error(
                    "Error with atomic signal name " +
                    line +
                    ": the name must not contain any \"@\" characters")
                raise Exception("Translation error")
            if ":" in line:
                parts = line.split(":")
                parts = [a.strip() for a in parts]
                if len(parts) != 2:
                    logger.error(
                        "Error reading line '" +
                        line +
                        "' in section " +
                        variableType +
                        ": Too many ':'s!")
                    raise Exception("Failed to translate file.")
                parts2 = parts[1].split("...")
                if len(parts2) != 2:
                    logger.error(
                        "Error reading line '" +
                        line +
                        "' in section " +
                        variableType +
                        ": Syntax should be name:from...to, where the latter two are numbers")
                    raise Exception("Failed to translate file.")
                try:
                    minValue = int(parts2[0])
                    maxValue = int(parts2[1])
                except ValueError:
                    logger.error(
                        "Error reading line '" +
                        line +
                        "' in section " +
                        variableType +
                        ": the minimal and maximal values are not given as numbers")
                    raise Exception("Failed to translate file.")
                if minValue > maxValue:
                    logger.error(
                        "Error reading line '" +
                        line +
                        "' in section " +
                        variableType +
                        ": the minimal value should be smaller than the maximum one (or at least equal)")
                    raise Exception("Failed to translate file.")

                # Fill the dictionaries numberAPLimits, translatedNames with
                # information
                variable = parts[0]
                numberAPs.append(parts[0])
                numberAPs.append(parts[0] + "'")
                numberAPLimits[parts[0]] = (minValue, maxValue)
                nofBits = 0
                while (2 ** nofBits <= (maxValue - minValue)):
                    nofBits += 1
                numberAPNofBits[variable] = nofBits
                # Translate name into bits in a way such that the first bit
                # carries not only the original name, but also min and max
                # information
                translatedNames[
                    parts[0]] = [
                    parts[0] +
                    "@" +
                    str(i) +
                    (
                        "." +
                        str(minValue) +
                        "." +
                        str(maxValue) if i == 0 else "") for i in xrange(
                        0,
                        nofBits)]
                translatedIOLines[variableType] = translatedIOLines[
                    variableType] + translatedNames[parts[0]]
                booleanAPs.extend(translatedNames[parts[0]])

                # Define limits
                # +1 because the range is inclusive
                limitDiff = maxValue - minValue + 1
                if (2 ** numberAPNofBits[variable]) != limitDiff:

                    propertyDestination = "ENV" if variableType.endswith(
                        "INPUT]") else "SYS"
                    # Init constraint
                    tokens = ["0"]
                    for i in xrange(0, numberAPNofBits[variable]):
                        if 2 ** i & limitDiff:
                            tokens = [
                                "| !",
                                translatedNames[variable][i]] + tokens
                        else:
                            tokens = [
                                "& !",
                                translatedNames[variable][i]] + tokens
                    lines[
                        "[" +
                        propertyDestination +
                        "_INIT]"].append(
                        "## Variable limits: " +
                        str(minValue) +
                        "<=" +
                        variable +
                        "<=" +
                        str(maxValue))
                    lines[
                        "[" +
                        propertyDestination +
                        "_INIT]"].append(
                        " ".join(tokens))

                    # Transition constraint for previous state -- only if using
                    # the "--thorougly mode"
                    if thoroughly:
                        lines[
                            "[" +
                            propertyDestination +
                            "_TRANS]"].append(
                            "## Variable limits: " +
                            str(minValue) +
                            "<=" +
                            variable +
                            "<=" +
                            str(maxValue))
                        lines[
                            "[" +
                            propertyDestination +
                            "_TRANS]"].append(
                            " ".join(tokens))

                    # Trans constraint for next states
                    tokens = ["0"]
                    for i in xrange(0, numberAPNofBits[variable]):
                        if 2 ** i & limitDiff:
                            tokens = [
                                "| !",
                                translatedNames[variable][i] + "'"] + tokens
                        else:
                            tokens = [
                                "& !",
                                translatedNames[variable][i] + "'"] + tokens
                    lines[
                        "[" +
                        propertyDestination +
                        "_TRANS]"].append(
                        "## Variable limits: " +
                        str(minValue) +
                        "<=" +
                        variable +
                        "'<=" +
                        str(maxValue))
                    lines[
                        "[" +
                        propertyDestination +
                        "_TRANS]"].append(
                        " ".join(tokens))

            else:
                # A "normal" atomic proposition
                line = line.strip()
                booleanAPs.append(line)
                booleanAPs.append(line + "'")
                translatedIOLines[variableType].append(line)

    # ---------------------------------------
    # Output new input/output lines
    # ---------------------------------------
    output = []
    for variableType in ["[INPUT]", "[OUTPUT]", "[OBSERVABLE_INPUT]",
                         "[UNOBSERVABLE_INPUT]", "[CONTROLLABLE_INPUT]"]:
        if len(translatedIOLines[variableType]) > 0:
            output.append(variableType)
            for a in translatedIOLines[variableType]:
                output.append(a)
            output.append("")

    # ---------------------------------------
    # Go through the properties and translate
    # ---------------------------------------
    for propertyType in ["[ENV_TRANS]", "[ENV_INIT]", "[SYS_TRANS]",
                         "[SYS_INIT]", "[ENV_LIVENESS]", "[SYS_LIVENESS]"]:
        if len(lines[propertyType]) > 0:
            output.append(propertyType)

            # Test for conformance with recursive definition
            for a in lines[propertyType]:
                logger.debug(a.strip().split(" "))
                if a.strip()[0:1] == "#":
                    output.append(a)
                else:
                    (isSlugsFormula, reasonForNotBeingASlugsFormula) = isValidRecursiveSlugsProperty(
                        a.strip().split(" "), booleanAPs)
                    if isSlugsFormula:
                        output.append(a)
                    else:
                        logger.debug(a)
                        # Try to parse!
                        tree = parseLTL(
                            a,
                            reasonForNotBeingASlugsFormula,
                            booleanAPs,
                            p)
                        # printTree(tree)
                        currentLine = translateToSlugsFormat(
                            tree,
                            numberAPLimits,
                            translatedNames,
                            p)
                        output.append(currentLine)
            output.append("")
    logger.debug(translatedNames)
    return '\n'.join(output)


# ==================================
# Entry point
# ==================================
if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise Exception("Error: Need input file parameter")

    inputFile = None
    thoroughly = False
    for parameter in sys.argv[1:]:
        if parameter.startswith("-"):
            if parameter == "--thorougly":
                thoroughly = True
            else:
                raise Exception(
                    "Error: did not understand parameter '" +
                    parameter +
                    "'")
                sys.exit(1)
        else:
            if inputFile is not None:
                raise Exception("Error: more than one file name given.")
                sys.exit(1)
            inputFile = parameter

    with open(inputFile, "r") as specFile:
        s = specFile.read()

    output = convert_to_slugsin(s, thoroughly)
    print(output)