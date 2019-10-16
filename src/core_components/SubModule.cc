#include "SubModule.h"
#include <godec/ComponentGraph.h>
#include <boost/foreach.hpp>
#include "ApiEndpoint.h"

namespace Godec {

std::string Submodule::describeThyself() {
    return "The Godec equivalent of a 'function call'";
}

/* Submodule::ExtendedDescription
This component allows for a "Godec within a Godec". You specify the JSON file for that graph with the "file" parameter, and the entire sub-graph gets treated as a single component. Great for building reusable libraries.

For a detailed discussion, [see here](UsingGodec.md), section "Submodules".
*/

LoopProcessor* Submodule::make(std::string id, ComponentGraphConfig* configPt) {
    return new Submodule(id, configPt);
}


Submodule::Submodule(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    std::string includeJson = configPt->get<std::string>("file", "The json for this subnetwork");

    if (!configPt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("inputs") || !configPt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("outputs"))
        GODEC_ERR << id << ": Either no inputs or outputs defined. This make no sense for a Submodule.";

    json endpoints;
    auto inputsChild = configPt->get_json_child("inputs");
    for(auto overrideV = inputsChild.begin(); overrideV != inputsChild.end(); overrideV++) {
        std::string endpointName = overrideV.key();
        std::vector<std::string> inputs;
        endpoints["!"+endpointName+ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(isVerbose(), inputs, overrideV.key());
    }

    auto outputsChild = configPt->get_json_child("outputs");
    for(auto overrideV = outputsChild.begin(); overrideV != outputsChild.end(); overrideV++) {
        std::string endpointName = overrideV.key();
        std::vector<std::string> inputs;
        inputs.push_back(overrideV.key());
        endpoints["!"+endpointName+ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(isVerbose(), inputs, "");
    }

    // addInputSlotAndUUID(name of slot inside sub-network that will be injected, UUID_AnyDecoderMessage);  // Replacement for above godec doc ignore

    std::list<std::string> requiredOutputSlots;
    // .push_back(Slot: name of stream inside sub-network to be pulled out);  // For godec doc
    initOutputs(requiredOutputSlots);

    GlobalComponentGraphVals* subGlobals = new GlobalComponentGraphVals(configPt->globalVals);
    json overrideTree;
    if (configPt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("override")) {
        auto overrideChild = configPt->get_json_child("override");
        for(auto overrideV = overrideChild.begin(); overrideV != overrideChild.end(); overrideV++) {
            if ((overrideV.key().find(".") != std::string::npos) || (overrideV.key().find("#") != std::string::npos)) GODEC_ERR << "NOTE: The override format in Submodules has changed, do not use \".\" or \"#\", that is only used now for command line overrides. For Submodules, now use an actual proper JSON tree structure, e.g. instead of \"a.b=c\", do " << std::endl <<
                        "\"override\": " << std::endl <<
                        "{ " << std::endl <<
                        "  \"a\":" << std::endl <<
                        "  {" << std::endl <<
                        "    \"b\": \"c\"" << std::endl <<
                        "  }" << std::endl <<
                        "}";
        }

        overrideTree = configPt->get_json_child("override");
    }
    ComponentGraphConfig subCgc(id, overrideTree, NULL, GetComponentGraph());
    mCgraph = new ComponentGraph(id, includeJson, &subCgc, endpoints, subGlobals);

    for(auto v = outputsChild.begin(); v != outputsChild.end(); v++) {
        std::string endpointName = id + ComponentGraph::TREE_LEVEL_SEPARATOR + v.key();
        std::string slotName = v.key();
        mPullThreads.push_back(boost::thread(&Submodule::PullThread, this, endpointName, slotName));
        RegisterThreadForLogging(mPullThreads.back(), mLogPtr, isVerbose());
    }
}

void Submodule::Shutdown() {
    for (auto tagIt = mInputTag2Slot.begin(); tagIt != mInputTag2Slot.end(); tagIt++) {
        for (auto slotIt = tagIt->second.begin(); slotIt != tagIt->second.end(); slotIt++) {
            std::string& slot = *slotIt;
            mCgraph->DeleteApiEndpoint(getLPId(false) + ComponentGraph::TREE_LEVEL_SEPARATOR + slot);
        }
    }

    mCgraph->WaitTilShutdown();
    auto stats = mCgraph->getRuntimeStats();
    for(auto it = stats.begin(); it != stats.end(); it++) {
        mRuntimeStats[it->first] = it->second;
    }
    delete mCgraph;
    mCgraph = NULL;

    for (auto it = mPullThreads.begin(); it != mPullThreads.end(); it++) {
        it->join();
    }

    for (auto slotIt = mOutputSlots.begin(); slotIt != mOutputSlots.end(); slotIt++) {
        auto& channelList = slotIt->second;
        for (auto channelIt = channelList.begin(); channelIt != channelList.end(); channelIt++) {
            (*channelIt)->checkOut(mOutputSlot2Tag[slotIt->first]);
        }
    }
    mIsFinished = true;
}

Submodule::~Submodule() {
}

void  Submodule::ProcessLoop() {
    while (true) {
        DecoderMessage_ptr newMessage;
        ChannelReturnResult res = mInputChannel.get(newMessage, FLT_MAX);
        if (res == ChannelClosed) break;

        if (isVerbose()) {
            std::stringstream verboseStr;
            verboseStr << "Submodule " << getLPId() << ": incoming " << newMessage->describeThyself() << std::endl;
            GODEC_INFO << verboseStr.str();
        }

        if (mInputTag2Slot.find(newMessage->getTag()) == mInputTag2Slot.end()) {
            GODEC_ERR << getLPId() << ": Can't find slot for tag '" << newMessage->getTag() << "'" << std::endl;
        }

        for (auto slotIt = mInputTag2Slot[newMessage->getTag()].begin(); slotIt != mInputTag2Slot[newMessage->getTag()].end(); slotIt++) {
            std::string& slot = *slotIt;
            DecoderMessage_ptr clonedMessage = newMessage->clone();
            auto ep = mCgraph->GetApiEndpoint(getLPId(false) + ComponentGraph::TREE_LEVEL_SEPARATOR + slot);
            ep->pushToOutputs(ep->getOutputSlot(), clonedMessage);
        }
    }
    Shutdown();
}

void Submodule::ProcessMessage(const DecoderMessageBlock& msgBlock) { }

void Submodule::PullThread(std::string endpoint, std::string slotName) {
    auto ep = mCgraph->GetApiEndpoint(endpoint);
    while (true) {
        unordered_map<std::string, DecoderMessage_ptr> newSlice;
        ChannelReturnResult res = ep->PullMessage(newSlice, FLT_MAX);
        if (res == ChannelClosed) break;
        if (isVerbose()) GODEC_INFO << "Submodule " << getLPId() << ": endpoint " << endpoint << ": Pulled messages: " << std::endl;
        for (auto slotIt = newSlice.begin(); slotIt != newSlice.end(); slotIt++) {
            if (isVerbose()) GODEC_INFO << "  " << slotIt->second->describeThyself();
            DecoderMessage_ptr clonedMessage = slotIt->second->clone();
            pushToOutputs(slotName, clonedMessage);
        }
    }
}

}

