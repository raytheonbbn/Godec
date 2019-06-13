#include "Energy.h"
#include <godec/ComponentGraph.h>

namespace Godec {

#define MINLARG 1e-25
#define LZERO 1e-10

EnergyComponent::~EnergyComponent() {
}

LoopProcessor* EnergyComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new EnergyComponent(id, configPt);
}
std::string EnergyComponent::describeThyself() {
    return "Calculates energy features on incoming windowed audio (feature input message produced by Window component)";
}
EnergyComponent::EnergyComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id,configPt) {
    addInputSlotAndUUID(SlotWindowedAudio, UUID_FeaturesDecoderMessage);
    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotFeatures);
    initOutputs(requiredOutputSlots);
}

void EnergyComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto audioMsg = msgBlock.get<FeaturesDecoderMessage>(SlotWindowedAudio);

    if (audioMsg->mFeatureNames.substr(0, strlen("WINAUDIO")) != "WINAUDIO") GODEC_ERR << getLPId() << ": Expected windowed audio, got " << audioMsg->mFeatureNames;

    Matrix audioSnippets = audioMsg->mFeatures;
    Matrix outMat(1, audioSnippets.cols());

    for (int frameIdx = 0; frameIdx < audioSnippets.cols(); frameIdx++) {
        Vector audio = audioSnippets.col(frameIdx);
        float energy = audio.squaredNorm();
        energy = (energy < MINLARG) ? LZERO : 10.0*log10(energy);
        outMat(0, frameIdx) = energy;
    }

    pushToOutputs(SlotFeatures, FeaturesDecoderMessage::create(
                      convStateMsg->getTime(), convStateMsg->mUtteranceId,
                      outMat, "R0%f", audioMsg->mFeatureTimestamps));
}

}
