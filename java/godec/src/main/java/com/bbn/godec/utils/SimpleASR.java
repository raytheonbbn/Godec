package com.bbn.godec.utils;

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
import com.bbn.godec.DecoderMessage;
import com.bbn.godec.Godec;
import com.bbn.godec.GodecJsonOverrides;
import com.bbn.godec.GodecPushEndpoints;
import com.bbn.godec.GodecPullEndpoints;
import com.bbn.godec.NbestDecoderMessage;

public class SimpleASR {

  Godec mEngine = null;
  Thread mResultsPullThread = null;
  Thread mTentativeResultsPullThread = null;
  int mTotalNumSamples = 0;
  boolean mKeepPulling = true;
  String mDecOutputPullEndpoint = null;
  String mTentativeDecOutputPullEndpoint = null;
  String mConvStatePullEndpoint = null;

  public interface ASRResultsInterface {
    void AsrUpdate(String utteranceID, String newASR, boolean endOfUtt);
    void TentativeAsrUpdate(String utteranceID, String newASR, boolean endOfUtt);
  }

  public enum ASRProcessingMode {
    Offline,
    Streaming
  }

  public SimpleASR(String rootJson, final ASRResultsInterface asrItf, ASRProcessingMode mode) {
    System.out.println("Starting in "+mode.toString() +" mode");

    GodecPushEndpoints pushList = new GodecPushEndpoints();
    pushList.addPushEndpoint("raw_audio");
    pushList.addPushEndpoint("convstate_input");

    GodecPullEndpoints pullList = new GodecPullEndpoints();
    HashSet<String> pullStreams = new HashSet<String>();
    mDecOutputPullEndpoint = "dec_output";
    mConvStatePullEndpoint = "convstate_output";
    pullStreams.add(mDecOutputPullEndpoint);
    pullStreams.add(mConvStatePullEndpoint);
    pullList.addPullEndpoint("results", pullStreams);

    HashSet<String> tentPullStreams = new HashSet<String>();
    mTentativeDecOutputPullEndpoint = "dec_tentative_output";
    tentPullStreams.add(mDecOutputPullEndpoint);
    tentPullStreams.add(mConvStatePullEndpoint);
    pullList.addPullEndpoint("tentative_results", tentPullStreams);

    GodecJsonOverrides insideOv = new GodecJsonOverrides();
    mEngine = new Godec(new File(rootJson), insideOv, pushList, pullList, true);

    mResultsPullThread = new Thread() {
      public void run() {
        while(mKeepPulling) {
          ArrayList<HashMap<String, DecoderMessage>> msgBlocks;
          try {
            msgBlocks = mEngine.PullAllMessages("results", 1000.0f*5);
          } catch (ChannelClosedException e1) {
            // Do the backward compatible things and set msgBlocks to null
            msgBlocks = null;
          }
          if (msgBlocks != null) {
            for(HashMap<String, DecoderMessage> msgBlock : msgBlocks) {
              ConversationStateDecoderMessage convoMessage = (ConversationStateDecoderMessage)msgBlock.get(mConvStatePullEndpoint);
              DecoderMessage decOutMsg = msgBlock.get(mDecOutputPullEndpoint);
              String commitString = "";
              NbestDecoderMessage nbestMsg = (NbestDecoderMessage)decOutMsg;
              if (nbestMsg.mEntries.size() > 0) {
                for(String word : nbestMsg.mEntries.get(0).text) {
                  commitString += word+" ";
                }
                commitString = commitString.trim();
              }
              asrItf.AsrUpdate(convoMessage.mUtteranceId, commitString, convoMessage.mLastChunkInUtt);

            }
          }
        }
      }
    };
    mResultsPullThread.start();

    mTentativeResultsPullThread = new Thread() {
      public void run() {
        while(mKeepPulling) {
          ArrayList<HashMap<String, DecoderMessage>> msgBlocks;
          try {
            msgBlocks = mEngine.PullAllMessages("tentative_results", 1000.0f*5);
          } catch (ChannelClosedException e1) {
            // Do the backward compatible things and set msgBlocks to null
            msgBlocks = null;
          }
          if (msgBlocks != null) {
            for(HashMap<String, DecoderMessage> msgBlock : msgBlocks) {
              ConversationStateDecoderMessage convoMessage = (ConversationStateDecoderMessage)msgBlock.get(mConvStatePullEndpoint);
              DecoderMessage decOutMsg = msgBlock.get(mTentativeDecOutputPullEndpoint);
              String tentativeString = "";
              NbestDecoderMessage nbestMsg = (NbestDecoderMessage)decOutMsg;
              if (nbestMsg.mEntries.size() > 0) {
                for(String word : nbestMsg.mEntries.get(0).text) {
                  tentativeString += word+" ";
                }
                tentativeString = tentativeString.trim();
              }
              asrItf.AsrUpdate(convoMessage.mUtteranceId, tentativeString, convoMessage.mLastChunkInUtt);

            }
          }
        }
      }
    };
    mTentativeResultsPullThread.start();
  }

  public void pushAudioChunk(byte[] audioData, AudioFormat audioFormat) {
    String formatString = "base_format=PCM;sample_width="+(audioFormat.getFrameSize()*8)+";sample_rate="+audioFormat.getSampleRate()+";num_channels="+audioFormat.getChannels();
    mTotalNumSamples += audioData.length;
    DecoderMessage audioMsg = new BinaryDecoderMessage(
        "dummy", 
        mTotalNumSamples-1,
        audioData,
        formatString);
    mEngine.PushMessage("raw_audio", audioMsg);

    DecoderMessage convMsg = new ConversationStateDecoderMessage(
        "dummy",
        mTotalNumSamples-1, // time stamp
        "utt_id", // utterance identifier
        false, // last chunk in utt?
        "convo_id", // conversation identifier
        false // last chunk in convo?
        );
    mEngine.PushMessage("convstate_input", convMsg);
  }

  public void recognizeWaveFile(String utteranceID, String audioFileName) throws Exception {
    try {
      AudioInputStream ais = AudioSystem.getAudioInputStream(new File(audioFileName));
      AudioFormat af = ais.getFormat();
      String formatString = "base_format=PCM;sample_width="+(af.getFrameSize()*8)+";sample_rate="+af.getSampleRate()+";num_channels="+af.getChannels();
      byte[] byteData = new byte[16000*2];
      int read = -1;
      while ((read = ais.read(byteData)) != -1) {
        boolean lastChunkInUtt = read != byteData.length;
        byte[] trimmedArray = new byte[read];
        System.arraycopy(byteData, 0, trimmedArray, 0, read);
        mTotalNumSamples += read/2;
        DecoderMessage audioMsg = new BinaryDecoderMessage("dummy", mTotalNumSamples, trimmedArray, formatString);
        mEngine.PushMessage("raw_audio", audioMsg);

        DecoderMessage convMsg = new ConversationStateDecoderMessage(
            "dummy",
            mTotalNumSamples, // time stamp
            utteranceID, // utterance identifier
            lastChunkInUtt, // last chunk in utt?
            "eternal_convo", // conversation identifier
            false // last chunk in convo?
            );
        mEngine.PushMessage("convstate_input", convMsg);
      }
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

  public void Shutdown() {
    mEngine.BlockingShutdown();
    try {
      mKeepPulling = false;
      mResultsPullThread.join();
    } catch (Exception e) {}
  }
}
