import sys
import traceback
import numpy as np
import numpy.core.multiarray
import dummy_module as dm

class PythonTest:
  def __init__(self,config, stdout, stderr, verbose):
    self.verbose = verbose
    # Redirect stdout/stderr
    sys.stdout = stdout 
    sys.stderr = stderr
    try:
      if (self.verbose):
        print("Constructor was called with config '"+config+"'")
      # The config paramater is an arbitrary string that is specified in the JSON, so you can customize the instance
      self.mult_fac = float(config) # This example multiplies incoming features by a factor
  
    except:
      print(traceback.format_exc())
      raise
    finally:
      # Flush, otherwise you won't see anything 
      sys.stdout.flush() 
      sys.stderr.flush() 

  def ProcessMessage(self, msgHash):
    try:
      if (self.verbose):
        print("ProcessMessage was called with features of shape "+str(msgHash['features']['features'].shape))
        print("Ignore data is set to "+str(msgHash['conversation_state']['descriptor']))
      msgHash['features']['features'] *= self.mult_fac
      del msgHash['conversation_state']
  
    except:
      print(traceback.format_exc())
      raise
    finally:
      # Flush, otherwise you won't see anything 
      sys.stdout.flush() 
      sys.stderr.flush() 
    return msgHash

