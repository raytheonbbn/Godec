package com.bbn.godec;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.reflect.TypeToken;
import com.google.gson.stream.JsonReader;

public class NbestDecoderMessage extends DecoderMessage {
  public NbestDecoderMessage(String tag, long time, ArrayList<NbestEntry> entries) {
    super(tag,time);
    mEntries = entries;
  }

  public ArrayList<NbestEntry> mEntries;

  @Override
    public byte[] describeThyself() {
      Gson gson = new GsonBuilder().setPrettyPrinting().create();
      return gson.toJson(toJson()).getBytes();
    }

  @Override
    public boolean compareTo(DecoderMessage msg) {
      if (!(msg instanceof NbestDecoderMessage)) return false;
      if (!super.compareTo(msg)) return false;
      NbestDecoderMessage nbestMsg = (NbestDecoderMessage)msg;
      if (mEntries.size() != nbestMsg.mEntries.size()) return false;
      for(int idx = 0; idx < mEntries.size(); idx++) {
        if (!mEntries.get(idx).compareTo(nbestMsg.mEntries.get(idx))) return false;
      }
      return true;
    }

  public JsonObject toJson() {
    JsonObject jsonObject = new JsonObject();
    int nbestCounter = 0;
    for (NbestEntry entry : mEntries) {
      JsonObject entryJsonObject = new JsonObject();
      for (int segIdx = 0; segIdx < entry.words.length; segIdx++) {
        JsonObject segmentJsonObject = new JsonObject();
        segmentJsonObject.addProperty("text", entry.text[segIdx]);
        segmentJsonObject.addProperty("alignment", entry.alignment[segIdx]);
        segmentJsonObject.addProperty("words", entry.words[segIdx]);
        segmentJsonObject.addProperty("confidences", entry.wordConfidences[segIdx]);
        entryJsonObject.add("segment_"+(segIdx), segmentJsonObject);
      }
      jsonObject.add("nbest_"+nbestCounter, entryJsonObject);
      nbestCounter++;
    }

    JsonSetBaseClassVals(jsonObject);
    return jsonObject;
  }
}
