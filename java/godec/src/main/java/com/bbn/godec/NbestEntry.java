package com.bbn.godec;

import java.util.Arrays;

public class NbestEntry {
  public NbestEntry(int size) {
    words = new int[size];
    alignment = new long[size];
    text = new String[size];
    wordConfidences = new float[size];
  }

  public NbestEntry(int[] _words, long[] _alignment, String[] _text, float[] _wordConfidences) {
    words = _words;
    alignment = _alignment;
    text = _text;
    wordConfidences = _wordConfidences;
  }

  public boolean compareTo(NbestEntry n) {
    if (!Arrays.equals(words, n.words)) return false;
    if (!Arrays.equals(alignment, n.alignment)) return false;
    if (!Arrays.equals(text, n.text)) return false;
    if (!Arrays.equals(wordConfidences, n.wordConfidences)) return false;
    return true;
  }

  public int[] words;
  public long[] alignment;
  public String[] text;
  public float[] wordConfidences;
}
