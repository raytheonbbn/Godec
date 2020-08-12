package com.bbn.godec;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

public class Godec 
{

  private static final String LOG_TAG = "Godec";

  /**
   * Load native library 
   */
  static {
    try {
      String OS = System.getProperty("os.name").toLowerCase();
      if (OS.indexOf("win") >= 0) {
        System.loadLibrary("libgodec");
      } else {
        System.loadLibrary("godec");
      }
    } catch (Exception e) {
      System.err.print("Ugh: "+e.getMessage());
    }
  }

  private native void JLoadGodec(File jsonFile, GodecJsonOverrides overrides, GodecPushEndpoints pushEndpoints, GodecPullEndpoints pullEndpoints, boolean quiet);
  private native void JPushMessage(String endpointName, DecoderMessage msg);
  private native HashMap<String,DecoderMessage> JPullMessage(String endpointName, float maxTimeout);
  private native ArrayList<HashMap<String,DecoderMessage>> JPullAllMessages(String endpointName, float maxTimeout);
  private native void JBlockingShutdown();

  /*
   * Godec constructor
   * @param jsonFile the location of the root JSON file
   * @param overrides for overriding values in the JSON 
   * @param pushEndpoints list of endpoints that messages will get pushed into
   * @param pullEndpoints list of endpoints that messages will get pulled from
   * @param quiet whether to print the full JSON on startup
  */
  public Godec(File jsonFile, GodecJsonOverrides overrides, GodecPushEndpoints pushEndpoints, GodecPullEndpoints pullEndpoints, boolean quiet)
  {
    jsonFile = Objects.requireNonNull(jsonFile);
    overrides = Objects.requireNonNull(overrides);
    pushEndpoints = Objects.requireNonNull(pushEndpoints);
    pullEndpoints = Objects.requireNonNull(pullEndpoints);
    JLoadGodec(jsonFile, overrides, pushEndpoints, pullEndpoints, quiet);
  }

  /*
   * Feeding messages into Godec 
   * @param endpointName name of the endpoint (which was defined during construction)
   * @param msg message to push
  */
  public void PushMessage(String endpointName, DecoderMessage msg) {
    long milli = System.currentTimeMillis();
    double seconds = milli / 1000.0;
    System.out.printf("PushMessage-Java-Debug(%.3f): pushing to " + endpointName + " timestamp: " + msg.mTime + "\n", seconds); 
    JPushMessage(endpointName, msg);
  }

  /*
   * Pulling messages from Godec 
   * @param endpointName name of the endpoint (which was defined during construction)
   * @param maxTimeout timeout to wait (in seconds) before definitely returning. This can mean returning with no messages
   * @return A "block" of messages that all have the same end time. Indexed by stream name that was defined in the GodecPullEndpoints during construction
  */
  public HashMap<String,DecoderMessage> PullMessage(String endpointName, float maxTimeout) throws ChannelClosedException {
    return JPullMessage(endpointName, maxTimeout);
  }

  /*
   * PullAllMessages
   * Same as PullMessage, but will return all blocks of messages that can be retrieved
  */
  public ArrayList<HashMap<String,DecoderMessage>> PullAllMessages(String endpointName, float maxTimeout) throws ChannelClosedException {
    return JPullAllMessages(endpointName, maxTimeout);
  }

  /*
   * This call blocks until the Godec network has shut itself down
  */
  public void BlockingShutdown() {
    JBlockingShutdown();
  }

  /* Main function and helper
   */
  public static GodecJsonOverrides GetOverridesFromArgs(String [] args) {
    GodecJsonOverrides ov = new GodecJsonOverrides();
    for (int argIdx = 0; argIdx < args.length; argIdx++)
    {
      if (args[argIdx].equals("-x"))
      {
        String ors = args[argIdx + 1];
        int eqSignIdx = ors.indexOf("=");
        String key = ors.substring(0, eqSignIdx);
        String val = ors.substring(eqSignIdx + 1);
        ov.addOverride(key,val);
      }
    }
    return ov;
  }
  public static void main(String [] args)
  {
    if(args.length == 0) {
      System.out.println("You need to provide at least one command line argument for godec");
      System.exit(-1);
    }
    boolean quiet = false;
    for(int argIdx = 0; argIdx < args.length; argIdx++) {
      if (args[argIdx].equals("-q")) quiet = true;
    }
    GodecJsonOverrides ov = GetOverridesFromArgs(args);
    GodecPushEndpoints pushList = new GodecPushEndpoints();
    GodecPullEndpoints pullList = new GodecPullEndpoints();
    Godec godec = new Godec(new File(args[args.length-1]), ov, pushList, pullList, quiet);
    godec.BlockingShutdown();
  } 
}
