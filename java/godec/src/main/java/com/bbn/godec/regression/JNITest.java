package com.bbn.godec.regression;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;

import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioInputStream;
import javax.sound.sampled.AudioSystem;

import com.bbn.godec.BinaryDecoderMessage;
import com.bbn.godec.ChannelClosedException;
import com.bbn.godec.ConversationStateDecoderMessage;
import com.bbn.godec.AudioDecoderMessage;
import com.bbn.godec.FeaturesDecoderMessage;
import com.bbn.godec.JsonDecoderMessage;
import com.bbn.godec.DecoderMessage;
import com.bbn.godec.Godec;
import com.bbn.godec.GodecJsonOverrides;
import com.bbn.godec.GodecPushEndpoints;
import com.bbn.godec.GodecPullEndpoints;
import com.bbn.godec.NbestDecoderMessage;
import com.bbn.godec.NbestEntry;
import com.bbn.godec.Matrix;
import com.bbn.godec.Vector;

public class JNITest {

  Godec mEngine = null;

  public JNITest(String rootJson, String testModes) {
    GodecPushEndpoints pushList = new GodecPushEndpoints();
    GodecPullEndpoints pullList = new GodecPullEndpoints();

    for(String msgType : testModes.split(",")) { 
      pushList.addPushEndpoint(msgType+"_in");
      HashSet<String> pullStreams = new HashSet<String>();
      pullStreams.add(msgType+"_in");
      pullList.addPullEndpoint(msgType+"_out", pullStreams);
    }

    GodecJsonOverrides insideOv = new GodecJsonOverrides();
    mEngine = new Godec(new File(rootJson), insideOv, pushList, pullList, true);
    long ticks = 100;
    if (testModes.contains("ConversationStateDecoderMessage"))
    {
      System.out.println("Checking convstate message");
      DecoderMessage msg = new ConversationStateDecoderMessage(
          "dummy",
          ticks, 
          "utt_id", 
          true, 
          "convo_id", 
          true 
          );
      PushPullCompare(msg);
    }

    if (testModes.contains("BinaryDecoderMessage"))
    {
      System.out.println("Checking binary message");
      DecoderMessage msg = new BinaryDecoderMessage(
          "dummy",
          ticks, 
          new byte[] {3,7,5,8,6,4,2,5,8}, 
          "myformat" 
          );
      PushPullCompare(msg);
    }

    if (testModes.contains("AudioDecoderMessage"))
    {
      System.out.println("Checking audio message");
      DecoderMessage msg = new AudioDecoderMessage(
          "dummy",
          ticks, 
          new Vector(new float[] {23.5f,5645.2f,6756.3f}), 
          44100.0f,
          5.0f
          );
      PushPullCompare(msg);
    }

    if (testModes.contains("FeaturesDecoderMessage"))
    {
      System.out.println("Checking features message");
      Matrix m = new Matrix(23,78);
      m.set(3,5,3423.0f);
      long timestamps[] = new long[78];
      timestamps[77] = ticks;
      DecoderMessage msg = new FeaturesDecoderMessage(
          "dummy",
          ticks, 
          "utt_id",
          m,
          timestamps,
          "SDFS[0:33]%f"
          );
      PushPullCompare(msg);
    }

    if (testModes.contains("JsonDecoderMessage"))
    {
      System.out.println("Checking JSON message");
      DecoderMessage msg = new JsonDecoderMessage(
          "dummy",
          ticks, 
          "{ \"hello\": \"123\" }"
          );
      PushPullCompare(msg);
    }

    if (testModes.contains("NbestDecoderMessage"))
    {
      System.out.println("Checking nbest message");
      ArrayList<NbestEntry> commEntries = new ArrayList<NbestEntry>();
      commEntries.add(
          new NbestEntry(
            new int[] {3,7,34},
            new long[] {0, 23,ticks},
            new String[] {"hello", "one", "two"},
            new float[] {0.34f, 0.5645f, 0.232f}
            )
          );
      NbestDecoderMessage msg = new NbestDecoderMessage(
          "dummy",
          ticks, 
          commEntries
          );
      PushPullCompare(msg);
    }

    mEngine.BlockingShutdown();

  }

  void PushPullCompare(DecoderMessage inMsg) {
    System.out.print("  Push, ");
    String slot = inMsg.getClass().getSimpleName();
    mEngine.PushMessage(slot+"_in", inMsg);
    ArrayList<HashMap<String, DecoderMessage>> msgBlocks = null;
    System.out.print("Pull, ");
    try {
      msgBlocks = mEngine.PullAllMessages(slot+"_out", 1000.0f*500);
    } catch (ChannelClosedException e1) {
      System.err.println(e1.getMessage());
      System.exit(-1);
    }
    if (msgBlocks == null) {
      System.err.println("msgBlocks == null");
      System.exit(-1);
    }

    DecoderMessage outMsg = msgBlocks.get(0).entrySet().iterator().next().getValue();
    if (!inMsg.compareTo(outMsg)) {
      System.err.println("messages not the same");
      System.exit(-1);
    }
    System.out.println("Same.");
  }

  public static void main(String[] args) {
    JNITest jniTest = new JNITest(args[0], args[1]);
  }

}
