package com.bbn.godec;

import java.util.Arrays;

/* Note, this class exists as a replacement for com.bbn.godec.Vector because com.bbn.godec.Vector is not nearly as easily converted through JNI as a simple float[] that this class here has. Also, Java doesn't have a Matrix class, so we'd otherwise have two vastly different classes */

public class Vector {
  public float[] mData;

  public Vector(int size) {
    mData = new float[size];	
  }

  public Vector(float[] data) {
    mData = data;
  }
  public Vector(int[] data) {
    if (data == null) {
      mData = null;
      return;
    }
    float[] newData = new float[data.length];
    for(int idx = 0; idx < newData.length; idx++) {
      newData[idx] = data[idx];
    }
    mData = newData;
  }

  public boolean compareTo(Vector v) {
    if (mData.length != v.mData.length) return false;
    return Arrays.equals(mData, v.mData);
  }

  public int size() { 
    int s = (mData == null) ? 0 : mData.length;
    return s;
  }

  void conservativeResize(int newSize) {
    float[] newData = new float[newSize];
    System.arraycopy(mData, 0, newData, 0, Math.min(size(), newSize));
    mData = newData;
  }
  public void addData(Vector featureTimings) {
    int prevSize = size();
    conservativeResize(prevSize+featureTimings.size());
    System.arraycopy(featureTimings.mData, 0, mData, prevSize, featureTimings.size());
  }

  public float get(int idx) {
    return mData[idx];
  }

  public void set(int idx, float f) {
    mData[idx] = f;
  }
}
