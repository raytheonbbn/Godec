package com.bbn.godec;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonObject;


public class AudioDecoderMessage extends DecoderMessage {
  public AudioDecoderMessage(String tag, long time, Vector audioVec, float sampleRate, float ticksPerSample) {
    super(tag, time);
    if (audioVec.size() == 0)
      throw new IllegalArgumentException("Can not create AudioDecoderMessage from empty data vector.");
    mAudio = audioVec;
    mSampleRate = sampleRate;
    mTicksPerSample = ticksPerSample;
  }

  public Vector mAudio;
  float mSampleRate;
  float mTicksPerSample;

  @Override
    public byte[] describeThyself() {
      Gson gson = new GsonBuilder().setPrettyPrinting().create();
      return gson.toJson(toJson()).getBytes();
    }

  @Override
    public boolean compareTo(DecoderMessage msg) {
      if (!(msg instanceof AudioDecoderMessage)) return false;
      if (!super.compareTo(msg)) return false;
      AudioDecoderMessage audioMsg = (AudioDecoderMessage)msg;
      if (!mAudio.compareTo(audioMsg.mAudio)) return false;
      if (mSampleRate != audioMsg.mSampleRate) return false;
      if (mTicksPerSample != audioMsg.mTicksPerSample) return false;
      return true;
    }

  public JsonObject toJson() {
    JsonObject jsonObject = new JsonObject();
    jsonObject.addProperty("mSampleRate", mSampleRate);
    jsonObject.addProperty("mTickPerSample", mTicksPerSample);

    JsonSetBaseClassVals(jsonObject);
    return jsonObject;
  }
}
