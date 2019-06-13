#pragma once

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"
#include "AccumCovariance.h"

namespace Godec {

// Based on C code carrying this notice:
/* boxmuller.c           Implements the Polar form of the Box-Muller
   Transformation
   (c) Copyright 1994, Everett F. Carter Jr.
   Permission is granted by the author to use
   this software for any application provided this
   copyright notice is preserved.
*/
class BoxMuller {
    float y2;
    bool use_last;

    float ranf();

  public:
    explicit BoxMuller();

    float of(float m, float s);
};

class NoiseAddComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    NoiseAddComponent(std::string id, ComponentGraphConfig* configPt);
    ~NoiseAddComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);

    boost::shared_ptr<AccumCovariance> onlVariance;
    std::string currentUttId;
    float noiseFactor;
};

}
