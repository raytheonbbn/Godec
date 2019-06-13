#include "FeatureMerger.h"

namespace Godec {

LoopProcessor* FeatureMergerComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new FeatureMergerComponent(id, configPt);
}
std::string FeatureMergerComponent::describeThyself() {
    return "Combines incoming feature and audio streams into one stream. Specify each incoming stream in the 'inputs' list with feature_stream_0, feature_stream_1 etc. The features will be merged in that order.";
}
FeatureMergerComponent::FeatureMergerComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id,configPt) {
    mNumStreams = configPt->get<int>("num_streams", "Number of incoming streams to merge");
    for(int streamIdx = 0; streamIdx < mNumStreams; streamIdx++) {
        std::stringstream ss;
        ss << "feature_stream_" << streamIdx;
        addInputSlotAndUUID(ss.str(), UUID_FeaturesDecoderMessage);  // GodecDocIgnore
        addInputSlotAndUUID(ss.str(), UUID_AudioDecoderMessage);  // GodecDocIgnore
        // addInputSlotAndUUID(feature_stream_[0-9], UUID_FeaturesDecoderMessage);  // As replacement for above godec doc ignore
        // addInputSlotAndUUID(feature_stream_[0-9], UUID_AudioDecoderMessage);  // As replacement for above godec doc ignore
    }
    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotFeatures);
    initOutputs(requiredOutputSlots);
}

FeatureMergerComponent::~FeatureMergerComponent() {}

void FeatureMergerComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);

    Matrix mergedFeats(0, 0);
    std::string mergedPfname = "";
    std::vector<uint64_t> featureTimestamps;

    uuid msgType;
    float ticksPerSample;
    float sampleRate;

    for(int streamIdx = 0; streamIdx < mNumStreams; streamIdx++) {
        std::stringstream slotSs;
        slotSs << "feature_stream_" << streamIdx;
        std::string slot = slotSs.str();
        auto baseMsg = msgBlock.getBaseMsg(slot);
        Matrix feats;
        msgType = baseMsg->getUUID();
        if (baseMsg->getUUID() == UUID_FeaturesDecoderMessage) {
            auto featMsg = msgBlock.get<FeaturesDecoderMessage>(slot);
            feats = featMsg->mFeatures;
            mergedPfname += featMsg->mFeatureNames + ";";
            featureTimestamps = featMsg->mFeatureTimestamps;
        } else if (baseMsg->getUUID() == UUID_AudioDecoderMessage) {
            auto audioMsg = msgBlock.get<AudioDecoderMessage>(slot);
            ticksPerSample = audioMsg->mTicksPerSample;
            sampleRate = audioMsg->mSampleRate;
            feats = audioMsg->mAudio.transpose();
            std::stringstream featNameSs;
            featNameSs << "AUDIO[0:" << (mNumStreams-1) << "]%f;";
            mergedPfname = featNameSs.str();
            int64_t msgLength = audioMsg->getTime()-msgBlock.getPrevCutoff();
            std::vector<uint64_t> featTimeStamps;
            for(int sampleIdx = 0; sampleIdx < feats.cols(); sampleIdx++) {
                uint64_t t = msgBlock.getPrevCutoff()+1+msgLength*(sampleIdx/(double)feats.cols());
                featTimeStamps.push_back(t);
            }
            featureTimestamps = featTimeStamps;
        }
        if (mergedFeats.cols() != 0 && mergedFeats.cols() != feats.cols())
            GODEC_ERR << getLPId() << ": Can't merge in stream " << slot << " because it has a different number of features than the previous streams" << std::endl << "Time " << convStateMsg->getTime() << " " << mergedFeats.cols() << "vs" << feats.cols() << std::endl;
        mergedFeats.conservativeResize(mergedFeats.rows() + feats.rows(), feats.cols());
        mergedFeats.bottomRows(feats.rows()) = feats;
    }
    mergedPfname.pop_back(); // Erase the last ;
    DecoderMessage_ptr outMsg = FeaturesDecoderMessage::create(
                                    convStateMsg->getTime(), convStateMsg->mUtteranceId,
                                    mergedFeats, mergedPfname, featureTimestamps);
    if (msgType == UUID_AudioDecoderMessage) {
        (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("ticks_per_sample", boost::lexical_cast<std::string>(ticksPerSample));
        (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("sample_rate", boost::lexical_cast<std::string>(sampleRate));
    }
    pushToOutputs(SlotFeatures, outMsg);
}

}
