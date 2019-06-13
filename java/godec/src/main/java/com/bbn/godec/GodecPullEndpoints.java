package com.bbn.godec;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;

/*
 * Helper class for specifying Godec pulling endpoints during construction. 
 * You cluster together message streams that you want to pull out in one block (e.g. your decoder nbest and the corresponding conversation state), and give them an endpoint label (e.g. "results")
*/
public class GodecPullEndpoints
{
  HashMap<String, HashSet<String>> mEps = new HashMap<String, HashSet<String>>();
  public GodecPullEndpoints() {
  }

  /*
   * Add another endpoint
   * @param endpointName Name of the endpoint
   * @param streamsToPull All the streams to pull out in block for this endpoint 
  */
  public void addPullEndpoint(String endpointName, HashSet<String> streamsToPull) {
    endpointName = Objects.requireNonNull(endpointName);
    streamsToPull = Objects.requireNonNull(streamsToPull);
    mEps.put(endpointName, streamsToPull);
  }
  
  // helper functions so we don't have to do this in JNI land
  public String[] getEndpoints() {
    Set<String> keySet = mEps.keySet();
    return keySet.toArray(new String[keySet.size()]);
  }

  public String[] getStreamsForEndpoint(String endpoint) {
    Set<String> set = mEps.get(endpoint);
    return set.toArray(new String[set.size()]);
  }
}
