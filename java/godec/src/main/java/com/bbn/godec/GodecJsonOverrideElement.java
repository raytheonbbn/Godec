package com.bbn.godec;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

public class GodecJsonOverrideElement
{
  public String key;
  public String val;
  public GodecJsonOverrideElement(String _key, String _val) {
    key = _key;
    val = _val;
  }

  public String getKey() {return key;}
  public String getVal() {return val;}
}
