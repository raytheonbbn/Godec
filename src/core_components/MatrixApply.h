#pragma once
#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class MatrixApplyComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    MatrixApplyComponent(std::string id, ComponentGraphConfig* configPt);
    ~MatrixApplyComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    std::string mMatrixSource;
    Matrix mFixedMatrix;
    bool mAugmentFeatures;
};

}
