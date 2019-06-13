#pragma once

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class AverageComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    AverageComponent(std::string id, ComponentGraphConfig* configPt);
    ~AverageComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    bool mLogAdd= false;
    size_t mFramesAccumulated = 0;
    std::vector<Real> mSumVec;

};

}
