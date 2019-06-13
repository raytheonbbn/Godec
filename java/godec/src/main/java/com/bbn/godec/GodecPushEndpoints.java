package com.bbn.godec;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;

/*
 * Helper class for specifying push endpoints in Godec
*/
public class GodecPushEndpoints 
{
  HashSet<String> mEps = new  HashSet<String>();
  public GodecPushEndpoints() {
  }

  /*
   * Add another push endpoint
   * @param ep the endpoint name
   */ 
  public void addPushEndpoint(String ep) {
    ep = Objects.requireNonNull(ep);
    mEps.add(ep);
  }

  // helper function so we don't have to do it in JNI land
  public String[] getEndpoints() {
    return mEps.toArray(new String[mEps.size()]);
  }
}
