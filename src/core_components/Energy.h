#pragma once

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class EnergyComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    EnergyComponent(std::string id, ComponentGraphConfig* configPt);
    ~EnergyComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
};

}
