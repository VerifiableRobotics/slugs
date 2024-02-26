#!/usr/bin/env python3
#
# Takes the output of "slugs --explicitStrategy ...other options..." and generates input to GraphViz from it.
#
# Pipe the output of slugs to this tool
import sys

propositions = None
states = {}
transitions = {}

for line in sys.stdin.readlines():
  if line.startswith("State"):
    assert line.count("<")==1
    
    # Split line into state/rank number and proposition values
    linePart1 = line[0:line.index("<")]
    linePart2 = line[line.index("<"):].strip()
    linePart1 = linePart1.split(" ")
    stateNo = linePart1[1]
    rank = linePart1[4]
    
    # Parsing proposition values
    assert linePart2[0]=="<"
    assert linePart2[-1]==">"
    linePart2 = [a.strip().split(":") for a in linePart2[1:len(linePart2)-1].split(",")]
    thesePropositions = [a[0] for a in linePart2]
    theseValues = [a[1] for a in linePart2]
    
    if not propositions is None:
      assert propositions == thesePropositions
    propositions = thesePropositions
    
    states[stateNo] = (rank,theseValues)
  if line.strip().startswith("With successors : "):
    successors = line.strip()[18:]
    successors = [a.strip() for a in successors.split(",")]
    transitions[stateNo] = successors

# Empty output
if len(states)==0:
  print("digraph { \"Empty implementation\"; }")
  sys.exit(0)

# Parsing done. Write
print("digraph {")
print("node [shape=tab] \""+",".join(propositions)+"\";")

for (state,(rank,values)) in states.items():
  print("\""+state+"\" [shape=box,label=\""+"".join(values)+"/"+rank+"\"];")
for state in states:
  for b in transitions[state]:
    print("\""+state+"\" -> \""+str(b)+"\";")    
print("}")
  

