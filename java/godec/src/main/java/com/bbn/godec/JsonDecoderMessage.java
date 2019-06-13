package com.bbn.godec;

import com.google.gson.*;

public class JsonDecoderMessage extends DecoderMessage {
  public JsonObject jsonObject;

  public JsonDecoderMessage(String tag, long time, String jsonString) {
    super(tag, time);
    JsonParser parser = new JsonParser();
    jsonObject = parser.parse(jsonString).getAsJsonObject();
  }

  @Override
    public boolean compareTo(DecoderMessage msg) {
      if (!(msg instanceof JsonDecoderMessage)) return false;
      if (!super.compareTo(msg)) return false;
      JsonDecoderMessage jsonMsg = (JsonDecoderMessage)msg;
      if (!jsonObject.equals(jsonMsg.jsonObject)) return false;
      return true;
    }

  @Override
    public String toString() {
      return toString(false);
    }

  public String toString(boolean prettyPrint) {
    if (prettyPrint) {
      Gson gson = new GsonBuilder().setPrettyPrinting().create();
      return gson.toJson(toJson());
    } else {
      return toJson().toString();
    }
  }
  @Override
    public byte[] describeThyself() {
      return toString().getBytes();
    }

  public JsonObject toJson() {
    return jsonObject;
  }

}
