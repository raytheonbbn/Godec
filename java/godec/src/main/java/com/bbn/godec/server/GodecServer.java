package com.bbn.godec.server;
import com.bbn.godec.ChannelClosedException;
import com.bbn.godec.DecoderMessage;
import com.bbn.godec.Godec;
import com.bbn.godec.GodecJsonOverrides;
import com.bbn.godec.GodecJsonOverrideElement;
import com.bbn.godec.GodecPushEndpoints;
import com.bbn.godec.GodecPullEndpoints;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import com.google.gson.stream.JsonReader;

import org.eclipse.jetty.http.HttpMethod;
import org.eclipse.jetty.server.ConnectionFactory;
import org.eclipse.jetty.server.HttpConfiguration;
import org.eclipse.jetty.server.HttpConnectionFactory;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.ServerConnector;
import org.eclipse.jetty.server.SslConnectionFactory;
import org.eclipse.jetty.server.handler.AbstractHandler;
import org.eclipse.jetty.util.ssl.SslContextFactory;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

import javax.net.SocketFactory;
import javax.net.ssl.SSLSocketFactory;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public class GodecServer extends AbstractHandler
{
  private Server server = null;
  private GodecServer me = null;
  private FrontEndInterface mFrontend = null;
  private Godec mEngine = null;


  public interface FrontEndInterface {
    boolean handleMsg(HttpServletResponse response, String url, HttpMethod method, byte[] inputPayload, HashMap<String,String> headers, Map<String, String[]> parameters);
  }

  public GodecServer(final String configFile, GodecJsonOverrides outsideOv, FrontEndInterface frontend, boolean quiet) throws Exception
  {
    me = this;

    Gson gson = new Gson();
    JsonReader jsonReader = new JsonReader(new FileReader(configFile));
    final Map<String,Object> configMap = gson.fromJson(jsonReader, new TypeToken<Map<String,Object>>(){}.getType());

    for(GodecJsonOverrideElement ovEntry : outsideOv) {
      System.out.println("Override "+ovEntry.key+"="+ovEntry.val);
      configMap.put(ovEntry.key, ovEntry.val);
    }

    GodecPushEndpoints pushList = new GodecPushEndpoints();
    GodecPullEndpoints pullList = new GodecPullEndpoints();

    for(String pullEndpointSnippet : ((String)configMap.get("pull_endpoints")).split(":")) {
      if (pullEndpointSnippet.trim().equals("")) continue;
      HashSet<String> streamsToPull = new HashSet<String>();
      String[] pesEls = pullEndpointSnippet.split(",");
      for(int idx = 1; idx < pesEls.length; idx++) {
        streamsToPull.add(pesEls[idx]);
      }
      pullList.addPullEndpoint(pesEls[0], streamsToPull);
    }

    for(String pushEndpoint : ((String)configMap.get("push_endpoints")).split(",")) {
      if (pushEndpoint.trim().equals("")) continue;
      pushList.addPushEndpoint(pushEndpoint);
    }

    GodecJsonOverrides insideOv = new GodecJsonOverrides();
    if (configMap.containsKey("override")) {
      Map<String,Object> ovMap = (Map<String,Object>)configMap.get("override");
      for(String key : ovMap.keySet()) {
        String val = (String)ovMap.get(key);
        insideOv.addOverride(key, val);
      }
    }

    final int port = Integer.parseInt((String)configMap.get("port"));
    final String networkInterface = (String)configMap.get("network_interface");
    final boolean secureConnection = configMap.containsKey("secure_connection") ? Boolean.parseBoolean((String)configMap.get("secure_connection")) : true;

    final String jsonFilename = (String)configMap.get("godec_config");
    final File jsonFile = new File(jsonFilename);
    mEngine = new Godec(jsonFile, insideOv, pushList, pullList, quiet);
    mFrontend = frontend;

    try
    {
      // This is a quick test whether a server is already running here.
      SocketFactory socketFactory = SSLSocketFactory.getDefault();
      /**
       * As one can see, the host is fixed to localhost. The only argument that can vary
       * is the port. Typically when someone starts CILABL, They can specify the port to start CILABL on. That is the
       * only way an attacker might be able to modify the port number. We're assuming that only Administrators working
       * with CILABL will be able to start it.
       */
      socketFactory.createSocket("localhost", port).close();
      // Yuck, there already is someone running!
      throw new Exception("Already a server running on port "+Integer.toString(port)+"!");
    }
    catch (IOException e)
    {
      //We want an IOException when attempting to connect to the given port; it means it is available
    }

    Thread serverThread = new Thread() {
      public void run()
      {
        try
        {
          org.eclipse.jetty.util.thread.QueuedThreadPool threadPool = new org.eclipse.jetty.util.thread.QueuedThreadPool();
          server = new Server(threadPool);
          server.setHandler(me);

          ConnectionFactory factory = null;
          if (secureConnection) {
            final ArrayList<String> passwords = PasswordTool.readObfuscatedPasswordsFile(new File((String)configMap.get("keystore_pwds")));
            SslContextFactory cf = new SslContextFactory();
            cf.setKeyStorePath(new File((String)configMap.get("keystore")).getCanonicalPath());
            cf.setKeyStorePassword(passwords.get(0));
            cf.setKeyManagerPassword(passwords.get(1));
            factory = new SslConnectionFactory(cf, "http/1.1");
          } else {
            factory = new HttpConnectionFactory();
            System.out.println("\n"
                + "*************************************************\n"
                + "*                ---WARNING---                  *\n"
                + "* This server is configured to transmit data    *\n"
                + "* insecurely over the network. If this is not   *\n"
                + "* desired, set 'https_connection' to 'true'     *\n"
                + "* in the config JSON, as well as the 'keystore' *\n"
                + "* and 'keystore_pwds' fields.                   *\n"
                + "*************************************************\n");
          }

          ServerConnector ssl_connector = new ServerConnector(server, factory, new HttpConnectionFactory(new HttpConfiguration()));
          ssl_connector.setPort(port);
          ssl_connector.setHost(networkInterface);
          server.addConnector(ssl_connector);

          server.start();
          server.join();
        } catch (Exception e) {
          System.err.println(e.getMessage());
          System.exit(-1);
        }
      }
    };
    serverThread.setName("httpServer");
    serverThread.start();
  }

  String normalizeHeader(String header) {
    return header.toLowerCase().replace("-", "_");
  }

  public void handle(String url,
      Request baseRequest,
      HttpServletRequest request,
      HttpServletResponse response)
    throws IOException, ServletException
    {
      HttpMethod httpMethod = HttpMethod.fromString(request.getMethod());
      HashMap<String,String> headers = new HashMap<String,String>();
      Enumeration<String> headerList = request.getHeaderNames();
      while(headerList.hasMoreElements())
      {
        String origHeader = headerList.nextElement();
        String normHeader = normalizeHeader(origHeader);
        String val = request.getHeader(origHeader);
        headers.put(normHeader, val);
      }
      byte[] payloadBytes = null;
      if ((httpMethod == HttpMethod.POST) || (httpMethod == HttpMethod.PUT)) {
        InputStream is = request.getInputStream();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        byte[] byteBuffer = new byte[1024];
        int length = 0;
        while((length = is.read(byteBuffer)) != -1) {
          baos.write(byteBuffer,0,length);
        }
        payloadBytes = baos.toByteArray();
      }

      Map<String, String[]> parameters = request.getParameterMap();

      mFrontend.handleMsg(response, url, httpMethod, payloadBytes, headers, parameters);
      baseRequest.setHandled(true);
    }

  public static void main(String [] args)
  {
    try
    {
      String json = null;
      for(int argIdx = 0; argIdx < args.length; argIdx++) {
        if (args[argIdx].equals("-json")) {
          json = args[argIdx+1];
        }
      }
      if (json == null) throw new Exception("No -json option specified");

      FrontEndInterface frontend = GodecFrontend.getInstance(json, args);
      synchronized(frontend) {
        frontend.wait();
      }
    } catch (Exception e) {
      System.err.println("Error: "+e.getMessage());
      e.printStackTrace();
    }
  }

  public void PushMessage(String endpointName, DecoderMessage msg) {
    mEngine.PushMessage(endpointName, msg);
  }

  public HashMap<String,DecoderMessage> PullMessage(String endpointName, float maxTimeout){
    try {
      return mEngine.PullMessage(endpointName, maxTimeout);
    } catch (ChannelClosedException e) {
      // Do the backward compatible thing and just return null
      return null;
    }
  }

  public ArrayList<HashMap<String,DecoderMessage>> PullAllMessages(String endpointName, float maxTimeout) {
    try {
      return mEngine.PullAllMessages(endpointName, maxTimeout);
    } catch (ChannelClosedException e) {
      // Do the backward compatible thing and just return null
      return null;
    }
  }

  public void Shutdown() {
    mEngine.BlockingShutdown();
  }
}
