import json
import sys
import os
import re
from subprocess import call

SkipConvstate = True
PrintLabels = False

if (len(sys.argv) != 2):
  sys.exit("Usage: Json2graph.py <json file>");

def depthToColor(level):
  if level == 0:
    return "#dddddd"
  if level == 1:
    return "#bbbbbb"
  return "#000000"

def loadSubmodule(parent, level, data_file, outGraphString, outputNickname2Component):
  baseDir = os.getcwd()
  fileDir = os.path.dirname(data_file)
  if (fileDir == ""):
    fileDir = "./"
  fileName = os.path.basename(data_file)
  os.chdir(fileDir)

  json_object = json.load(open(fileName))
  connections = {}
  isSubmodule = {}
  for component, config in json_object.iteritems():
    if (component.startswith("#") or component == "global_opts"):
      continue
    if (parent != ""):
      component = parent+"."+component
    compType = config["type"]
    inputs = {}
    if ("inputs" in config):
      for slot in config["inputs"]:
        inputs[slot] = config["inputs"][slot]
    if ("outputs" in config):
      for slot in config["outputs"]:
        nickname = config["outputs"][slot]
        outputNickname2Component[nickname] = component
    if (compType == "SubModule"):
      isSubmodule[component] = "yes"
      subModuleOutNick2Comp = {}
      for slot in inputs:
        subModuleOutNick2Comp[slot] = component
      outGraphString[0] += "subgraph \"cluster_"+component+"\" { \ngraph [style=\"filled,rounded\" color=\""+depthToColor(level)+"\"];\n"
      subConnections = loadSubmodule(component,level+1,config["file"],outGraphString,subModuleOutNick2Comp)
      outGraphString[0] += "label = \""+component+"\";\n}\n"
      for slot in config["outputs"]:
        nickname = config["outputs"][slot]
        foundComp = ""
        for subNickname in subModuleOutNick2Comp:
          if (subNickname == slot):
            foundComp = subModuleOutNick2Comp[subNickname]
        if (foundComp ==""):
          sys.exit("Could not find nickname "+nickname+" in "+config["file"]) 
        outputNickname2Component[nickname] = foundComp


  for component, config in json_object.iteritems():
    if (component.startswith("#") or component == "global_opts"):
      continue
    if (parent != ""):
      component = parent+"."+component
    if ("inputs" in config):
      for slot in config["inputs"]:
        if (SkipConvstate and slot.startswith("conv")):
          continue
        nickname = config["inputs"][slot]
        if (nickname not in outputNickname2Component):
          if (level == 0):
            outputNickname2Component[nickname] = "Audio"
          else:
            sys.exit("Ugh "+nickname);
        connectedComponent = outputNickname2Component[nickname]
        if (connectedComponent not in connections):
          connections[connectedComponent] = {}
        if (component not in connections[connectedComponent]):
          connections[connectedComponent][component] = list()
        connections[connectedComponent][component].append(nickname)
  os.chdir(baseDir)
  for subModule in isSubmodule:
    outGraphString[0] += "\""+subModule+"\" [shape=point];\n"
  for component in connections:
    for targetComponent in connections[component]:
      for nickname in connections[component][targetComponent]:
        outGraphString[0] += "\""+component + "\"->\"" + targetComponent+"\" ["
        #if (targetComponent in isSubmodule):
        #  outGraphString[0] += " lhead=\"cluster_"+targetComponent+"\","
        if (PrintLabels):
          outGraphString[0] += " label=\""+nickname+"\","
        outGraphString[0] = re.sub(',$','',outGraphString[0])
        outGraphString[0] += "];\n"
  return connections

outFile = open("tmp.dot", "w")
outFile.write("digraph G {\noverlap = false;\ngraph [compound=true];\n")

topLevelOutNick2Comp = {}
graphString = [""]
loadSubmodule("",0,sys.argv[1],graphString, topLevelOutNick2Comp )

outFile.write(graphString[0])
outFile.write("}\n")
outFile.close()
call(["dot", "-Tsvg","tmp.dot","-o","graph.svg"])
#os.remove("tmp.dot")
