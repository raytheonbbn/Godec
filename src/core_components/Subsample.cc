#include "Subsample.h"
#include <godec/ComponentGraph.h>

namespace Godec {

SubsampleComponent::~SubsampleComponent() {
}

LoopProcessor* SubsampleComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new SubsampleComponent(id, configPt);
}
std::string SubsampleComponent::describeThyself() {
    return "Godec equivalent of subsample-feats executable, for subsampling/repeating features";
}

SubsampleComponent::SubsampleComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    numProcessed = 0;
    mSkipNum = configPt->get<int>("num_skip_frames", "Only emit every <num_skip_frames> frames. Set to negative to also repeat the emitted frame <num_skip_frames> (resulting in the same number of total frames)");

    addInputSlotAndUUID(SlotFeatures, UUID_FeaturesDecoderMessage);

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotFeatures);
    initOutputs(requiredOutputSlots);
}

void SubsampleComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto featsMsg = msgBlock.get<FeaturesDecoderMessage>(SlotFeatures);

    mFeatsBuffer.conservativeResize(featsMsg->mFeatures.rows(),mFeatsBuffer.cols()+featsMsg->mFeatures.cols());
    mFeatsBuffer.rightCols(featsMsg->mFeatures.cols()) = featsMsg->mFeatures;
    std::copy(featsMsg->mFeatureTimestamps.begin(), featsMsg->mFeatureTimestamps.end(), std::back_inserter(mTimingsBuffer));

    int numInputFeatsToProcess = convStateMsg->mLastChunkInUtt ? mFeatsBuffer.cols() : mFeatsBuffer.cols() - abs(mSkipNum);
    if (numInputFeatsToProcess <= 0) return;

    int outMatrixSize = 0;
    for (int featIdx = 0; featIdx < numInputFeatsToProcess; featIdx++) {
        if ((numProcessed+featIdx)%abs(mSkipNum) == 0) outMatrixSize++;
        else if (mSkipNum < 0) outMatrixSize++;
    }
    if (outMatrixSize == 0) return;

    Matrix outMatrix(mFeatsBuffer.rows(), outMatrixSize);
    std::vector<uint64_t> outTimings(outMatrix.cols());

    uint64_t lastAddedTime = 0;
    int64_t outMatrixIdx = -1;

    for (int featIdx = 0; featIdx < numInputFeatsToProcess; featIdx++) {
        if (numProcessed%abs(mSkipNum) == 0) {
            outMatrixIdx++;
            outMatrix.col(outMatrixIdx) = mFeatsBuffer.col(featIdx);
            outTimings[outMatrixIdx] = mTimingsBuffer[featIdx];
        } else if (mSkipNum < 0) {
            outMatrix.col(outMatrixIdx + 1) = outMatrix.col(outMatrixIdx);
            outTimings[outMatrixIdx + 1] = mTimingsBuffer[featIdx];
            outMatrixIdx++;
        }
        if (outMatrixIdx >= 0) lastAddedTime = outTimings[outMatrixIdx];
        numProcessed++;
    }

    if (convStateMsg->mLastChunkInUtt) {
        outTimings.back() = convStateMsg->getTime();
        lastAddedTime = outTimings.back();
    }

    mFeatsBuffer = (Matrix)mFeatsBuffer.rightCols(mFeatsBuffer.cols() - numInputFeatsToProcess);
    mTimingsBuffer = std::vector<uint64_t>(mTimingsBuffer.begin() + numInputFeatsToProcess, mTimingsBuffer.end());
    pushToOutputs(SlotFeatures, FeaturesDecoderMessage::create(
                      lastAddedTime, convStateMsg->mUtteranceId,
                      outMatrix, featsMsg->mFeatureNames, outTimings));

    if (convStateMsg->mLastChunkInUtt) {
        if (mFeatsBuffer.cols() != 0 || mTimingsBuffer.size() != 0) GODEC_ERR << "Last chunk in utt, but buffers are not empty!";
        numProcessed = 0;
    }
}

}
