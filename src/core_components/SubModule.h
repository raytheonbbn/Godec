#pragma once
#include <string>
#include <mutex>
#include <godec/ChannelMessenger.h>

namespace Godec {

class Submodule : public LoopProcessor {
  public:
    static std::string describeThyself();
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    Submodule(std::string id, ComponentGraphConfig* configPt);
    ~Submodule();
    void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
  private:
    void ProcessLoop() override;
    void PullThread(std::string epToPull, std::string slot);
    std::vector<boost::thread> mPullThreads;
    bool RequiresConvStateInput() override { return false; }
    bool EnforceInputsOutputs() override { return false; }
  protected:
    void Shutdown() override;
    ComponentGraph* mCgraph;
    friend class ComponentGraph;
};

}
