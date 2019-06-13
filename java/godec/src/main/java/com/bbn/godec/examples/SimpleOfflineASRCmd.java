package com.bbn.godec.examples;

import java.io.File;
import com.bbn.godec.utils.*;

class SimpleOfflineASRCmd implements SimpleASR.ASRResultsInterface {

  SimpleASR asr = null;
  public SimpleOfflineASRCmd(String rootJson) {
    asr = new SimpleASR(rootJson, this, SimpleASR.ASRProcessingMode.Offline);
  }

  void recognizeFolder(String audioFilesDir) {
    try {
      File folder = new File(audioFilesDir);
      for (final File fileEntry : folder.listFiles()) {
        String fileName = folder+"/"+fileEntry.getName();
        if (fileName.endsWith(".wav")) {
          System.out.println("Pushing "+fileName);
          asr.recognizeWaveFile(fileName, fileName);
        }
      }
    } catch (Exception e) {}
  }

  void Shutdown() {
    asr.Shutdown();
  }

  @Override 
    public void AsrUpdate(String utteranceID, String newASR, boolean endOfUtt) {
      System.out.println("commit result for utt " +utteranceID+": "+newASR);
    }

  @Override 
    public void TentativeAsrUpdate(String utteranceID, String newASR, boolean endOfUtt) {
      System.out.println("tentative result for utt " +utteranceID+": "+newASR);
    }

  public static void main(String[] args) {
    if (args.length != 2) {
      System.out.println("Usage: SimpleOfflineASRCmd <path to model JSON> <path to audio folder>");
      System.exit(-1);
    }
    SimpleOfflineASRCmd cmd = new SimpleOfflineASRCmd(args[0]);
    cmd.recognizeFolder(args[1]);
    cmd.Shutdown();
  }
}
