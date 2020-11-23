#include "Merger.h"
#include "Router.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/format.hpp>

namespace Godec {

static const std::string SlotInputStreamPrefix = "input_stream_";
static const std::string SlotOutputStream = "output_stream";

LoopProcessor* MergerComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new MergerComponent(id, configPt);
}
std::string MergerComponent::describeThyself() {
    return "Merges streams that were created by Router. Streams are specified as input_stream_0, input_stream_1 etc, up to \"num_streams\". Time map is the one created by the Router";
}
/* MergerComponent::ExtendedDescription
Refer to the Router extended description to a more detailed description. This component expects all conversation states from the upstream Router, as well as all the processed streams
*/
MergerComponent::MergerComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    mNumStreams = configPt->get<int>("num_streams", "Number of streams that are merged");

    GODEC_INFO << getLPId() << ":num streams = " << mNumStreams;
    for(int streamIdx = 0; streamIdx < mNumStreams; streamIdx++) {
        std::stringstream inputStreamSs;
        inputStreamSs << SlotInputStreamPrefix << streamIdx;
        GODEC_INFO << getLPId() << ":stream input = " << inputStreamSs.str();

        addInputSlotAndUUID(inputStreamSs.str(), UUID_AnyDecoderMessage); //GodecDocIgnore
        // addInputSlotAndUUID(input_streams_[0-9], UUID_AnyDecoderMessage);  // Replacement for above godec doc ignore
        std::stringstream convStateSs;
        convStateSs << SlotConversationState << "_" << streamIdx;
        GODEC_INFO << getLPId() << ":convstate input = " << convStateSs.str();
        addInputSlotAndUUID(convStateSs.str(), UUID_ConversationStateDecoderMessage); //GodecDocIgnore
        // addInputSlotAndUUID(conversation_state_[0-9], UUID_ConverstionStateDecoderMessage);  // Replacement for above godec doc ignore
    }
    GODEC_INFO << getLPId() << ":output = " << SlotOutputStream;

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotOutputStream);
    initOutputs(requiredOutputSlots);
}

MergerComponent::~MergerComponent() {}

void MergerComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    MergerComponent::ProcessIgnoreDataMessageBlock(msgBlock);
}



/*
void MergerComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    for(int streamIdx = 0; streamIdx < mNumStreams; streamIdx++) {
        std::stringstream inputStreamSs;
        inputStreamSs << SlotInputStreamPrefix << streamIdx;
        auto streamBaseMsg = msgBlock.getBaseMsg(inputStreamSs.str());
        std::stringstream convStateSs;
        convStateSs << SlotConversationState << "_" << streamIdx;
        auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(convStateSs.str());
        auto ignoreData = convStateMsg->getDescriptor(RouterComponent::IgnoreData);
        if (ignoreData == "") GODEC_ERR << getLPId() << ": Stream " << streamIdx << " has a conversation state connected to it that did not come from a Router!";
        if (ignoreData == "false") {
            pushToOutputs(SlotOutputStream, streamBaseMsg);
        }
    }
}*/
}
