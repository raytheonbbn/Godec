import sys
import re
import matplotlib
import matplotlib.pyplot as plt
import decimal
import argparse

# This script takes in logs written by components (set "log_fiile" param to a value and "verbose" to true for each component) and outputs latency graphs via matplotlib
# The script has two output modes: --realtime and --streamtime
# --realtime: This will show the real-time latency incurred between the "Pusher" (the first component seeing pushing in the logs) and a given components
# --streamtime: This gives you the streamtime latency, and actually gives you a measure of the inherent algorithmic latency of the daisy-chain of components. Note that for this to work, you need to set the feeding component slow enough so that after a new chunk has been pushed, all subsequent components have settled down
# use --exclude to exclude certain messages based on their slot (the value inside "[...]" in the messages)

parser = argparse.ArgumentParser(description='This script measures the latency between Godec components. Set the respective components\' verbosity to true and set the log_file parameter, then run the script on the log files')
parser.add_argument('logfiles', metavar='<log files>', type=str, nargs='+', help='the list of logfiles')
parser.add_argument('--realtime', action='store_true', help='Show realtime latencies')
parser.add_argument('--streamtime', action='store_true', help='Show streamtime, algorithmic latencies (note this presumes runnning Godec intentionally with very slow feeding so that all components have settled down before the next chunk gets fed')
parser.add_argument('--yfac', help='Value to multiply the y-axis by, e.g. to convert stream ticks into seconds')
parser.add_argument('--exclude', help='Ignore messages whose slot names matches this regexp')

args = parser.parse_args()

if ((not args.realtime) and (not args.streamtime)):
  print(str("Need to set either --realtime or --streamtime"))
  exit(-1)

excre = None
if (args.exclude != None):
  excre = re.compile(args.exclude)

yfac = 1
if (args.yfac != None):
  yfac = decimal.Decimal(args.yfac)

def ReadLogFile(logFname, excre):
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
          if ((new_dict["action"] != "Pushing") or ((excre != None) and (excre.match(new_dict["slot"])))):
            continue
          event_list.append(new_dict)
  return event_list

# Read in all log files
all_events = list()
for logfile in args.logfiles:
  all_events.extend(ReadLogFile(logfile, excre))

def cmpFunc(val1, val2):
  rt1 = val1["realtime"] 
  rt2 = val2["realtime"] 
  if (rt1 == rt2):
    st1 = val1["streamtime"] 
    st2 = val2["streamtime"] 
    return cmp(st2,st1) # See high streamtimes first
  else:
    return cmp(rt1,rt2)

all_events.sort(cmp = cmpFunc)

pusherName = all_events[0]["name"]
pusherSlot = all_events[0]["slot"]

print("Start-point LP and slot: "+pusherName+":"+pusherSlot)

realTimeOffset = 0
tag2Realtime = dict()
tag2RealtimeLatency = dict()
tag2StreamtimeLatency = dict()
for idx, event in enumerate(all_events):
  name = event["name"]
  slot = event["slot"]
  realtime = event["realtime"]
  streamtime = event["streamtime"]
  #print("event: "+str(realtime)+" streamtime="+str(streamtime)+" name="+name)
  if (realTimeOffset == 0): 
    realTimeOffset = realtime
  if (name != pusherName):
    # Find the pusher event that had equal or less streamtime
    for scan_idx in reversed(range(idx)):
      scan_event = all_events[scan_idx]
      if (scan_event["name"] == pusherName and scan_event["slot"] == pusherSlot):
        if ((args.streamtime) or (args.realtime and scan_event["streamtime"] <= streamtime)):
          pusherLastRealtime = scan_event["realtime"]
          pusherLastStreamtime = scan_event["streamtime"]
          break
    tag = name+"["+slot+"]"
    realtime_latency = yfac*(realtime-pusherLastRealtime)
    streamtime_latency = yfac*(pusherLastStreamtime-streamtime)
    #if (streamtime_latency > 20000):
    #  print("pusher: realtime="+str(realtime)+" sreamtime"+str(streamtime))
    #  print("pusher: pusherLastRealtime="+str(pusherLastRealtime)+" pusherLastStreamtime"+str(pusherLastStreamtime))
    #  print(str(realtime_latency))
    #  print(str(streamtime_latency))
    #  exit(-1)
    if tag not in tag2Realtime:
      tag2Realtime[tag] = list()
      tag2RealtimeLatency[tag] = list()
      tag2StreamtimeLatency[tag] = list()
    offset_realtime = realtime-realTimeOffset 
    tag2Realtime[tag].append(offset_realtime)
    tag2RealtimeLatency[tag].append(realtime_latency)
    tag2StreamtimeLatency[tag].append(streamtime_latency)

fig, ax = plt.subplots()
for tag in tag2Realtime:
  if (args.realtime):
    ax.plot(tag2Realtime[tag], tag2RealtimeLatency[tag], label=tag)
  else:
    ax.plot(tag2Realtime[tag], tag2StreamtimeLatency[tag], label=tag)

plt.legend()
plt.show()

