#pragma once
#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

struct MsgQueue {
    std::vector<DecoderMessage_ptr> queue;
    int64_t queueOffset;
};

class MergerComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    MergerComponent(std::string id, ComponentGraphConfig* configPt);
    virtual ~MergerComponent();
    virtual void ProcessLoop() override;

  private:
    virtual void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
    bool RequiresConvStateInput() override { return false; }
    TimeStreams mTimeMapStream;
    unordered_map<std::string, MsgQueue> mMessageQueue;
};

}
