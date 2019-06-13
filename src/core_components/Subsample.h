#pragma once

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class SubsampleComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    SubsampleComponent(std::string id, ComponentGraphConfig* configPt);
    ~SubsampleComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);

    int64_t numProcessed;
    int mSkipNum;
    Matrix mFeatsBuffer;
    std::vector<uint64_t> mTimingsBuffer;
};

}
