#pragma once
#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

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

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);

    static std::string SlotRoutingStream;
    static std::string SlotToRouteStream;
    static std::string SlotRoutedOutputStreamedPrefix;

    Mode mMode;
    int mNumRoutes;

    int64_t mToRouteStreamOffset;
    std::vector<int64_t> mRoutedStreamOffsets;
    std::vector<uint64_t> mAccumRouteIdx;
    int mCurrentRouteIdx;

    // SAD Nbest router
    std::vector<DecoderMessage_ptr> mAccumToRouteBaseMsg;
    std::vector<uint64_t> mAccumAlignment;
    std::vector<bool> mAccumEndOfUtt;
    std::vector<std::string> mAccumUttId;
    std::vector<bool> mAccumEndOfConvo;
    std::vector<std::string> mAccumConvoId;
    std::vector<std::string> mCurrentUttIdByRoute;
    int64_t mLastUttStart;

};

}
