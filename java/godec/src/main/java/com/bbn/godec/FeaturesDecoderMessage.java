package com.bbn.godec;

import java.util.HashMap;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonObject;
import java.util.Arrays;

public class FeaturesDecoderMessage extends DecoderMessage {
  public String mUtteranceId = null;
  public String mFeatureNames = null;
  public Matrix mFeatures = null;
  public long[] mFeatureTimestamps = null;
  public FeaturesDecoderMessage(String tag, long time, String utteranceId, Matrix features, long[] featureTimestamps, String featureNames) {
    super(tag, time);
    mUtteranceId = utteranceId;
    mFeatures = features;
    mFeatureNames = featureNames;
    mFeatureTimestamps = featureTimestamps;
  }

  @Override
    public byte[] describeThyself() {
      Gson gson = new GsonBuilder().setPrettyPrinting().create();
      return gson.toJson(toJson()).getBytes();
    }

  @Override
    public boolean compareTo(DecoderMessage msg) {
      if (!(msg instanceof FeaturesDecoderMessage)) return false;
      if (!super.compareTo(msg)) return false;
      FeaturesDecoderMessage featsMsg = (FeaturesDecoderMessage)msg;
      if (!mUtteranceId.equals(featsMsg.mUtteranceId)) return false;
      if (!mFeatureNames.equals(featsMsg.mFeatureNames)) return false;
      if (!Arrays.equals(mFeatureTimestamps, featsMsg.mFeatureTimestamps)) return false;
      if (!mFeatures.compareTo(featsMsg.mFeatures)) return false;
      return true;
    }

  public JsonObject toJson() {
    JsonObject jsonObject = new JsonObject();
    jsonObject.addProperty("mUtteranceId", mUtteranceId);
    jsonObject.addProperty("mFeatureNames", mFeatureNames);

    // TODO: Add features

    JsonSetBaseClassVals(jsonObject);
    return jsonObject;
  }
}
