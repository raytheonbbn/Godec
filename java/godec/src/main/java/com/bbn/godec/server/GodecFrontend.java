package com.bbn.godec.server;

import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import javax.servlet.http.HttpServletResponse;

import org.eclipse.jetty.http.HttpMethod;

import com.bbn.godec.AudioDecoderMessage;
import com.bbn.godec.BinaryDecoderMessage;
import com.bbn.godec.ConversationStateDecoderMessage;
import com.bbn.godec.DecoderMessage;
import com.bbn.godec.Godec;
import com.bbn.godec.GodecJsonOverrides;
import com.bbn.godec.Vector;
import com.bbn.godec.server.GodecServer.FrontEndInterface;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;

public class GodecFrontend implements FrontEndInterface {

	private GodecServer mServer = null;

	private static String[] decomposeUrl(String url) {
		String[] urlEls = url.split("/");
		if (!urlEls[1].equals("ep")) throw new RuntimeException("For godec frontend, first endpoint part needs to be 'ep'");
		return urlEls;
	}

	@Override
	synchronized public boolean handleMsg(HttpServletResponse response, String url, HttpMethod httpMethod, byte[] payload, HashMap<String, String> inputHeaders, Map<String, String[]> parameters) {
		try
		{
			String[] urlEls = decomposeUrl(url);

			if (httpMethod == HttpMethod.POST) {
				DecoderMessage msg = null;
				if (urlEls[3].equals("audio")) {
					if (payload.length %2 != 0) throw new RuntimeException("Uneven binary audio payload!");
					float[] floatData = new float[payload.length/4];
					ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer().get(floatData);
					//msg = new AudioDecoderMessage("dummy", Integer.parseInt(inputHeaders.get(DecoderMessage.GODEC_HEADER_TIME)), new Vector(floatData), Float.parseFloat(inputHeaders.get(AudioDecoderMessage.HEADER_VTL_STRETCH)), Float.parseFloat(inputHeaders.get(AudioDecoderMessage.HEADER_SAMPLE_RATE)));
				}
				if (urlEls[3].equals("binary")) {
					//msg = new BinaryDecoderMessage("dummy", Integer.parseInt(inputHeaders.get(DecoderMessage.GODEC_HEADER_TIME)), payload, inputHeaders.get(BinaryDecoderMessage.HEADER_FORMAT));
				}
				else if (urlEls[3].equals("conversation_state")) {
          Gson gson = new Gson();
					msg = ConversationStateDecoderMessage.fromJson((Map<String,Object>)gson.fromJson(new String(payload), new TypeToken<Map<String,Object>>(){}.getType()));
				}
				else throw new RuntimeException("Unknown message type "+urlEls[3]);

				mServer.PushMessage(urlEls[2], msg);
				response.setStatus(HttpServletResponse.SC_OK);
			}
			else if (httpMethod == HttpMethod.GET) {
				HashMap<String,DecoderMessage> msgBlock = mServer.PullMessage(urlEls[2], Float.MAX_VALUE);
				String nonConvoSlot = null;
				for(String slot : msgBlock.keySet()) {
					if (!slot.equals("conversation_state")) {
						if (nonConvoSlot != null) throw new RuntimeException("Pulled more than one non-convo state stream!");
						nonConvoSlot = slot;
					}
				}
				if (nonConvoSlot == null)  throw new RuntimeException("Could not find singular non-convo stream when pulling!");
				DecoderMessage msg = msgBlock.get(nonConvoSlot);
				OutputStream os = response.getOutputStream(); 
				byte[] bodyBytes = msg.describeThyself();
				os.write(bodyBytes);
				os.flush();
				response.setStatus(HttpServletResponse.SC_OK);
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
		return true;
	}

	public static FrontEndInterface getInstance(String json, String[] args) throws Exception {
		GodecFrontend frontend = new GodecFrontend();
		GodecJsonOverrides ov = Godec.GetOverridesFromArgs(args);
		frontend.mServer = new GodecServer(args[args.length-1], ov, frontend, false);
		return frontend;
	}
}
