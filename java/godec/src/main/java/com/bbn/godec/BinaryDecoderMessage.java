package com.bbn.godec;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.HashMap;
import java.util.Arrays;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;


public class BinaryDecoderMessage extends DecoderMessage {
	public BinaryDecoderMessage(String tag, long time, byte[] data, String format) {
		super(tag, time);
		mData = new byte[data.length];
		System.arraycopy(data,0,mData,0, data.length);
		mFormat = format;
	}

	public byte[] mData;
	public String mFormat;

  @Override
  public boolean compareTo(DecoderMessage msg) {
    if (!(msg instanceof BinaryDecoderMessage)) return false;
    if (!super.compareTo(msg)) return false;
    BinaryDecoderMessage binMsg = (BinaryDecoderMessage)msg;
    if (!Arrays.equals(mData, binMsg.mData)) return false;
    if (!mFormat.equals(binMsg.mFormat)) return false;
    return true;
  }

	@Override
	public byte[] describeThyself() {
		Gson gson = new GsonBuilder().setPrettyPrinting().create();
		return gson.toJson(toJson()).getBytes();
	}
	
	public JsonObject toJson() {
		JsonObject jsonObject = new JsonObject();
		jsonObject.addProperty("mFormat", mFormat);
		
		// TODO: Add data

		JsonSetBaseClassVals(jsonObject);
		return jsonObject;
	}
}
