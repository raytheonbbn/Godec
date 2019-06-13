#pragma once
#include <string>
#include <mutex>
#include <godec/ChannelMessenger.h>

namespace Godec {

class ApiEndpoint : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    ApiEndpoint(std::string id, ComponentGraphConfig* configPt);
    void Shutdown() override;
    void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
    ChannelReturnResult PullMessage(unordered_map<std::string, DecoderMessage_ptr>& slice, float maxTimeout);
    ChannelReturnResult PullAllMessages(std::vector<unordered_map<std::string, DecoderMessage_ptr>>& slice, float maxTimeout);
    std::string getOutputSlot();
    static std::string SlotPassout;
  private:
    channel<unordered_map<std::string, DecoderMessage_ptr>> mSliceKeeper;
    bool RequiresConvStateInput() override { return false; }
};

} // namespace Godec
