#include "ApiEndpoint.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include "GodecMessages.h"

namespace Godec {

std::string ApiEndpoint::SlotPassout = "passout";

LoopProcessor* ApiEndpoint::make(std::string id, ComponentGraphConfig* configPt) {
    return new ApiEndpoint(id, configPt);
}
std::string ApiEndpoint::describeThyself() {
    return "An internal component for injecting or retrieving messages. Used heavily by the SubModule component and the Java API";
}
ApiEndpoint::ApiEndpoint(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    mSliceKeeper.setIdVerbose(id + "-SliceKeeper", isVerbose());
    mSliceKeeper.checkIn(getLPId(false));
    bool isInputEndpoint = false;
    if (configPt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("inputs")) {
        isInputEndpoint = true;
        auto pt = configPt->get_json_child("inputs");
        for(auto v = pt.begin(); v != pt.end(); v++) {
            std::string slot = v.key();
            std::string tag = v.value();
            addInputSlotAndUUID(slot, UUID_AnyDecoderMessage);
        }
    }

    std::list<std::string> requiredOutputSlots;
    if (!isInputEndpoint) requiredOutputSlots.push_back("output");
    initOutputs(requiredOutputSlots);
}

void ApiEndpoint::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    mSliceKeeper.put(msgBlock.getMap());
}

void ApiEndpoint::Shutdown() {
    mSliceKeeper.checkOut(getLPId(false));
    LoopProcessor::Shutdown();
}


std::string ApiEndpoint::getOutputSlot() {
    if (mOutputSlots.size() != 1) GODEC_ERR << "Not exactly one output slot defined in Api endpoint " << getLPId(false, true) << std::endl;
    return mOutputSlots.begin()->first;
}

ChannelReturnResult ApiEndpoint::PullMessage(unordered_map<std::string, DecoderMessage_ptr>& slice, float maxTimeout) {
    return mSliceKeeper.get(slice, maxTimeout);
}

ChannelReturnResult ApiEndpoint::PullAllMessages(std::vector<unordered_map<std::string, DecoderMessage_ptr>>& slice, float maxTimeout) {
    return mSliceKeeper.getAll(slice, maxTimeout);
}

}
