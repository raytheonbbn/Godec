import sys
import numpy as np
import math

orig_feats = np.load("python_test.npz")
times_two_feats = np.load("python_test_times_two_feats.npz")
if(sorted(orig_feats.files) != sorted(times_two_feats.files)):
  sys.stderr.write("Different entries in times-two npz!\n")
  sys.stderr.flush()
  exit(-1)
for uttId in orig_feats.files:
  if (orig_feats[uttId].shape != times_two_feats[uttId].shape):
    sys.stderr.write("Matrices for utt "+uttId+" have different shape! "+str(orig_feats[uttId].shape)+" vs "+str(times_two_feats[uttId].shape)+"\n")
    sys.stderr.flush()
    exit(-1)
  orig_featsDouble = orig_feats[uttId]*2
  maxDiff = np.max(np.abs(np.subtract(orig_featsDouble,times_two_feats[uttId])))
  if (maxDiff > 1E-07):
    sys.stderr.write("Different matrices for utt "+uttId+"\n")
    sys.stderr.flush()
    exit(-1)

godec_norm_feats = np.load("python_test_norm_feats.npz")
accum_orig_feats = None
highestMaxDiff = 0.0
for uttId in orig_feats.files:
  if (accum_orig_feats is None):
    accum_orig_feats = orig_feats[uttId]
  else:
    accum_orig_feats = np.concatenate((accum_orig_feats,orig_feats[uttId]), axis=1)
  for rowIdx in range(accum_orig_feats.shape[0]):
    mean = np.mean(accum_orig_feats[rowIdx])
    std = np.std(accum_orig_feats[rowIdx]-mean,ddof=1)
    norm_orig = (orig_feats[uttId][rowIdx]-mean)/std
    maxDiff = np.max(np.abs(np.subtract(norm_orig,godec_norm_feats[uttId][rowIdx])))
    highestMaxDiff = max(highestMaxDiff, maxDiff)
    if (maxDiff > 1E-06):
      sys.stderr.write("Different matrices for utt "+uttId+"\n")
      sys.stderr.flush()
      exit(-1)

print("maxDiff= "+str(highestMaxDiff))

