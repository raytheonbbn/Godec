package com.bbn.godec;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

/*
 * Helper class for specifying JSON node overrides
 */
public class GodecJsonOverrides extends ArrayList<GodecJsonOverrideElement>
{
  public GodecJsonOverrides() {
  }

  /*
   * @param jsonNode the JSON node to override, in "a.b.c" form
   * @param newVal the new value for the node
   */
  public void addOverride(String jsonNode, String newVal) {
    jsonNode = Objects.requireNonNull(jsonNode);
    newVal = Objects.requireNonNull(newVal);
    this.add(new GodecJsonOverrideElement(jsonNode,newVal));
  }
}
