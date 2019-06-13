package com.bbn.godec.regression;

import java.io.File;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Iterator;

import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioInputStream;
import javax.sound.sampled.AudioSystem;

import com.bbn.godec.BinaryDecoderMessage;
import com.bbn.godec.ChannelClosedException;
import com.bbn.godec.ConversationStateDecoderMessage;
import com.bbn.godec.AudioDecoderMessage;
import com.bbn.godec.FeaturesDecoderMessage;
import com.bbn.godec.JsonDecoderMessage;
import com.bbn.godec.DecoderMessage;
import com.bbn.godec.Godec;
import com.bbn.godec.GodecJsonOverrides;
import com.bbn.godec.GodecPushEndpoints;
import com.bbn.godec.GodecPullEndpoints;
import com.bbn.godec.NbestDecoderMessage;
import com.bbn.godec.NbestEntry;
import com.bbn.godec.Matrix;
import com.bbn.godec.Vector;

public class TextTransform {

  public TextTransform(String params) {
    System.out.println("TextTransform was constructed with params="+params);
  }

  public HashMap<String, DecoderMessage> ProcessMessage(HashMap<String, DecoderMessage> map, long i) {
    HashMap<String, DecoderMessage> outMap = new HashMap<String, DecoderMessage>();
    Iterator it = map.entrySet().iterator();
    while (it.hasNext()) {
      Map.Entry pair = (Map.Entry)it.next();
      String slot = (String)pair.getKey();
      DecoderMessage msg = (DecoderMessage)pair.getValue();
/*      try { 
        System.out.println(new String(msg.describeThyself(),"UTF-8"));
      } catch (Exception e) {} */
      if (msg instanceof NbestDecoderMessage) {
        NbestDecoderMessage nbestMsg = (NbestDecoderMessage)msg;
        for (NbestEntry entry : nbestMsg.mEntries) {
          for (int segIdx = 0; segIdx < entry.text.length; segIdx++) {
            entry.text[segIdx] = entry.text[segIdx].toUpperCase();
          }
        }
      } 
      outMap.put(slot.replace("_in","_out"), msg);
    }
    return outMap;
  }

}
