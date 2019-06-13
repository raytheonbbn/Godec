#include "Average.h"

namespace Godec {

LoopProcessor* AverageComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new AverageComponent(id, configPt);
}
std::string AverageComponent::describeThyself() {
    return "Averages the incoming feature stream. Will issue the averaged features when seeing end-of-utterance";
}
AverageComponent::AverageComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    mLogAdd = configPt->get<bool>("apply_log", "Doing summation in log domain (true, false)");

    addInputSlotAndUUID(SlotFeatures, UUID_FeaturesDecoderMessage);
    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotFeatures);
    initOutputs(requiredOutputSlots);
}

AverageComponent::~AverageComponent() {
}

void AverageComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto featMsg = msgBlock.get<FeaturesDecoderMessage>(SlotFeatures);
    if (mSumVec.empty()) {
        uint64_t nDim = featMsg->mFeatures.rows();
        assert(nDim > 0);
        mSumVec.resize(nDim);
        std::fill(mSumVec.begin(), mSumVec.end(), 0);
        mFramesAccumulated = 0;
    }

    uint64_t nFrame = featMsg->mFeatures.cols();
    for (size_t i = 0; i < nFrame; ++i) {
        for (size_t j = 0; j < mSumVec.size(); ++j) {
            Real v = featMsg->mFeatures(j, i);
            if (mLogAdd) {
                if (v <= 0) {
                    GODEC_ERR << "Can not take log when input " << v << " <= 0 at frame="
                              << i << " column=" << j;
                }
                v = log(v);
            }
            mSumVec[j] += v;
        }
    }
    mFramesAccumulated += nFrame;

    if (convStateMsg->mLastChunkInUtt) {
        Matrix M(mSumVec.size(), 1);
        for (size_t j = 0; j < mSumVec.size(); ++j) {
            M(j, 0) = mSumVec[j]/mFramesAccumulated;

        }
        std::vector<uint64_t> featureTimestamps = {featMsg->getTime()};
        DecoderMessage_ptr outfeatMsg = FeaturesDecoderMessage::create(
                                            featMsg->getTime(), featMsg->mUtteranceId,
                                            M, featMsg->mFeatureNames, featureTimestamps);
        pushToOutputs(SlotFeatures, outfeatMsg);
        mSumVec.clear();
    }
}

}
