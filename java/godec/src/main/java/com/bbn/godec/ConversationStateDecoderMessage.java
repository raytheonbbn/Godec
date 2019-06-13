package com.bbn.godec;

import java.util.HashMap;
import java.util.Map;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.reflect.TypeToken;

public class ConversationStateDecoderMessage extends DecoderMessage {
  public ConversationStateDecoderMessage(String tag, long time, String uttId, boolean lastChunkInUtt, String convoId, boolean lastChunkInConvo) {
    super(tag, time);
    mUtteranceId = uttId;
    mLastChunkInUtt = lastChunkInUtt;
    mConvoId = convoId;
    mLastChunkInConvo = lastChunkInConvo;
    if (lastChunkInConvo && !lastChunkInUtt) throw new IllegalArgumentException("Trying to construct nonsensical ConversationStateDecoderMessage: isLastChunkInConvo=true but isLastChunkInUtt=false. Utterances can't carry over past conversations!");
  }
  public String mUtteranceId;
  public boolean mLastChunkInUtt;
  public String mConvoId;
  public boolean mLastChunkInConvo;

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

  @Override
    public boolean compareTo(DecoderMessage msg) {
      if (!(msg instanceof ConversationStateDecoderMessage)) return false;
      if (!super.compareTo(msg)) return false;
      ConversationStateDecoderMessage convMsg = (ConversationStateDecoderMessage)msg;
      if (!mUtteranceId.equals(convMsg.mUtteranceId)) return false;
      if (!mConvoId.equals(convMsg.mConvoId)) return false;
      if (mLastChunkInUtt != convMsg.mLastChunkInUtt) return false;
      if (mLastChunkInConvo != convMsg.mLastChunkInConvo) return false;
      return true;
    }

  public JsonObject toJson() {
    JsonObject jsonObject = new JsonObject();
    jsonObject.addProperty("mUtteranceId", mUtteranceId);
    jsonObject.addProperty("mConvoId", mConvoId);
    jsonObject.addProperty("mLastChunkInUtt", mLastChunkInUtt);
    jsonObject.addProperty("mLastChunkInConvo", mLastChunkInConvo);

    JsonSetBaseClassVals(jsonObject);
    return jsonObject;
  }

  public static ConversationStateDecoderMessage fromJson(Map<String,Object> map) {
    ConversationStateDecoderMessage outMsg = new ConversationStateDecoderMessage(
        "", 
        0, 
        (String)map.get("mUtteranceId"),
        (Boolean)map.get("mLastChunkInUtt"),
        (String)map.get("mConvoId"),
        (Boolean)map.get("mLastChunkInConvo")
        );
    outMsg.JsonGetBaseClassVals(map);
    return outMsg;
  }
}
