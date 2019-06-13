package com.bbn.godec.examples;

import java.util.Scanner;
import com.bbn.godec.utils.*;
import javax.sound.sampled.*;
import java.io.*;

public class SimpleStreamingASRCmd implements SimpleASR.ASRResultsInterface{

  SimpleASR mAsr = null;
  Thread mPushThread = null;

  private class AudioRecorder {
    private TargetDataLine m_line;
    private AudioInputStream m_audioInputStream;
    public AudioFormat mAudioFormat = null;

    public AudioRecorder(AudioFormat audioFormat)
    {
      DataLine.Info     info = new DataLine.Info(TargetDataLine.class, audioFormat);
      m_line = null;
      try
      {
        m_line = (TargetDataLine) AudioSystem.getLine(info);
        m_line.open(audioFormat);
      }
      catch (LineUnavailableException e)
      {
        System.out.println("unable to get a recording line");
        e.printStackTrace();
        System.exit(1);
      }

      m_audioInputStream = new AudioInputStream(m_line);
    }

    public void start()
    {
      m_line.start();
    }
    public void stop()
    {
      m_line.stop();
      m_line.close();
    }

    public int read(byte[] b) throws IOException {
      return m_audioInputStream.read(b);
    }
  }


  public SimpleStreamingASRCmd(String rootJson) {
    mAsr = new SimpleASR(rootJson, this, SimpleASR.ASRProcessingMode.Streaming);
    final AudioFormat audioFormat = new AudioFormat(
        AudioFormat.Encoding.PCM_SIGNED,
        44100, 
        16,
        1, 
        2, 
        44100,
        false);

    mPushThread = new Thread() {
      public void run() {
        try {
          AudioRecorder recorder = new AudioRecorder(audioFormat);
          byte[] byteData = new byte[1024];
          int read = -1;
          recorder.start();
          while ((read = recorder.read(byteData)) != -1) {
            byte[] trimmedArray = new byte[read];
            System.arraycopy(byteData, 0, trimmedArray, 0, read);
            mAsr.pushAudioChunk(trimmedArray, audioFormat);
          }
        } catch (Exception e) {
          e.printStackTrace();
        }
      }
    };
    mPushThread.start();
  }

  public static void main(String[] args) {
    if (args.length != 1) {
      System.out.println("Usage: SimpleStreamingASRCmd <path to model JSON>");
      System.exit(-1);
    }
    SimpleStreamingASRCmd cmd = new SimpleStreamingASRCmd(args[0]);

    System.out.println("########################## Recognizing ##########");
    try {Thread.sleep(1000*60*60*24);} catch (Exception e) {}
  }

  String mCurrentAccumASR = "";
  String padSpaces = "                                                                                                               ";

  @Override
    public void AsrUpdate(String utteranceID ,String commitAsr, boolean endOfUtt) {
      if (commitAsr != "") mCurrentAccumASR += commitAsr.trim()+" ";
      String totalUtt = "\r"+mCurrentAccumASR;
      totalUtt += padSpaces;
      totalUtt = totalUtt.substring(0,70);
      System.out.print(totalUtt);
      if (commitAsr != "" && endOfUtt) {
        System.out.print("\n");
        mCurrentAccumASR = "";
      }
      System.out.flush();
    }
  @Override
    public void TentativeAsrUpdate(String utteranceID ,String tentativeAsr, boolean endOfUtt) {
    }
}
