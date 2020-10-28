#pragma once
#include <godec/ChannelMessenger.h>
#include <godec/GodecMessages.h>

namespace Godec {

enum class Mode {
    SadNbest,
    UtteranceRoundRobin
};

class RouterComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    RouterComponent(std::string id, ComponentGraphConfig* configPt);
    ~RouterComponent();

    static std::string IgnoreData;
  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);

    static std::string SlotRoutingStream;

    Mode mMode;
    int mNumRoutes;
    int mRRCurrentRoute;
    std::vector<uint64_t> mAccumRouteIdx;
    std::vector<uint64_t> mAccumAlignment;
    std::vector<bool> mAccumEndOfUtt;
    std::vector<std::string> mAccumUttId;
    std::vector<bool> mAccumEndOfConvo;
    std::vector<std::string> mAccumConvoId;
};

}
