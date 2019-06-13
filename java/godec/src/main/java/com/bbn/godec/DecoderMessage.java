package com.bbn.godec;

import java.util.HashMap;
import java.util.Map;

import com.google.gson.JsonObject;


/* This class needs to exactly mirror ChannelMessenger.h */
public abstract class DecoderMessage {
  DecoderMessage(String tag, long time) {
    mTime = time;
    mTag = tag;
  }
  public long mTime;
  public String mTag;
  public abstract byte[] describeThyself();
  public abstract JsonObject toJson();

  public boolean compareTo(DecoderMessage msg) {
    if (mTime != msg.mTime) return false;
    if (!getFullDescriptorString().equals(msg.getFullDescriptorString())) return false;
    return true;
  }

  HashMap<String,String> mDescriptors = new HashMap<String,String>();

  public void addDescriptor(String key, String val) {
    mDescriptors.put(key,val);
  }

  public String getDescriptor(String key) {
    return mDescriptors.get(key);
  }

  public String getFullDescriptorString() {
    String out = ""; 
    for (String key : mDescriptors.keySet()) {
      out += key + "=" + mDescriptors.get(key) + ";";
    } 
    return out;
  }

  public void setFullDescriptorString(String desc) {
    String[] descEls = desc.split(";");
    for (String descEl : descEls) {
      descEl = descEl.trim();
      if (descEl.equals("")) continue;
      String[] keyVal = descEl.split("=");
      addDescriptor(keyVal[0], keyVal[1]);
    }
  }

  void JsonSetBaseClassVals(JsonObject jsonObject) {
    jsonObject.addProperty("time", mTime);
    jsonObject.addProperty("tag", mTag);
    jsonObject.addProperty("descriptors", getFullDescriptorString());
  }

  void JsonGetBaseClassVals(Map<String,Object> map) {
    mTag = (String)map.get("tag");
    mTime = ((Number)map.get("time")).longValue();
    if (map.containsKey("descriptors")) setFullDescriptorString((String)map.get("descriptors"));
  }
}
