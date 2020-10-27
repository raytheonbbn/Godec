#pragma once
#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class MergerComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    MergerComponent(std::string id, ComponentGraphConfig* configPt);
    virtual ~MergerComponent();

  private:
    virtual void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
    bool RequiresConvStateInput() override { return false; }
    
};

}
