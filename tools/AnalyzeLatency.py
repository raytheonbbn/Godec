import sys
import re
import matplotlib
import matplotlib.pyplot as plt
import decimal


if (len(sys.argv) < 2):
  print("Usage: AnalyzeLatency.py <log file list>\nThis script measures the latency between Godec components. Set the respective components' verbosity to true and set the log_file parameter, then run the script on the log files")
  exit(-1)

def ReadLogFile(logFname):
  rex = re.compile("^(.+)\ (.+)\(([0-9\.]+)\):\ ([^\s]+).+\[([^\]]+),([^\]]+)\]")
  event_list = list()
  with open(logFname) as fp:
    for line in fp:
      line = line.rstrip()
      match = rex.match(line)
      if (match is not None):
        groups = match.groups()
        if (len(groups) == 6):
          new_dict = dict()
          new_dict["name"] = groups[1]
          new_dict["name"] = new_dict["name"].replace("Toplevel.","")
          new_dict["realtime"] = decimal.Decimal(groups[2])
          new_dict["action"] = groups[3]
          new_dict["slot"] = groups[4]
          new_dict["streamtime"] = long(groups[5])
          event_list.append(new_dict)
  return event_list

# Read in all log files
all_events = list()
for argIdx in range(1, len(sys.argv)):
  all_events.extend(ReadLogFile(sys.argv[argIdx]))

def cmpFunc(val1, val2):
  t1 = val1["realtime"] 
  t2 = val2["realtime"] 
  if (t1 == t2):
    a1 = val1["action"] 
    a2 = val2["action"] 
    return cmp(a1,a2)
  return cmp(t1,t2)

all_events.sort(cmp = cmpFunc)

for event in all_events:
  if (event["action"] == "Pushing"):
    AName = event["name"]
    ASlot = event["slot"]
    break

print("Start-point LP and slot: "+AName+":"+ASlot)

realTimeOffset = 0
lastATime = 0
tag2Realtime = dict()
tag2Latency = dict()
for event in all_events:
  #print("event: "+str(event["realtime"])+" streamtime="+str(event["streamtime"])+" name="+event["name"]+" action="+event["action"])
  if (realTimeOffset == 0): 
    realTimeOffset = event["realtime"]
  if (event["name"] == AName and event["action"] == "Pushing" and event["slot"] == ASlot):
    lastATime = event["realtime"]
  if (lastATime != 0 and event["action"] == "incoming" and event["name"] != AName):
    tag = event["name"]+"["+event["slot"]+"]"
    latency = event["realtime"]-lastATime
    if tag not in tag2Realtime:
      tag2Realtime[tag] = list()
      tag2Latency[tag] = list()
    time = event["realtime"]-realTimeOffset 
    tag2Realtime[tag].append(time)
    tag2Latency[tag].append(latency)

fig, ax = plt.subplots()
for tag in tag2Realtime:
  ax.plot(tag2Realtime[tag], tag2Latency[tag], label=tag)

plt.legend()
plt.show()

