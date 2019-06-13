#pragma once
#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class FeatureMergerComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    FeatureMergerComponent(std::string id, ComponentGraphConfig* configPt);
    ~FeatureMergerComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    int mNumStreams;
};

}
