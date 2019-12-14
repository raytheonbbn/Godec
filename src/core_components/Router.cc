#include "Router.h"
#include <boost/format.hpp>

namespace Godec {

std::string RouterComponent::SlotRoutingStream = "routing_stream";
std::string RouterComponent::IgnoreData = "ignore_data";

LoopProcessor* RouterComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new RouterComponent(id, configPt);
}
std::string RouterComponent::describeThyself() {
    return "Splits an incoming stream into separate streams, based on the binary decision of another input stream";
}

/* RouterComponent::ExtendedDescription
The router component can be used to split streams across several branches (e.g. if you have a language detector upstream and want to direct the streams to the language-specific component). The mechanism is very easy, the Router create N output conversation state message streams, each of which has the "ignore_data" set to true or false, depending on the routing at that point. The branches' components can check this flag and do some optimization (while still emitting empty messages that account for the stream time). So, all branch components see all the data. The Merger component can be used downstream to merge the output together (it chooses the right results from each stream according to those conversation state messages.

There are two modes the Router decides where to route:

"sad_nbest": "routing_stream" is expected to be an NbestDecoderMessage, where the 0-th nbest entry is expected to contain a sequence of 0 (nonspeech) or 1 (speech), which the router will use to route the stream

"utterance_round_robin": Simple round robin on an utterance-by-utterance basis
*/

RouterComponent::RouterComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    std::list<std::string> requiredOutputSlots;

    std::string router_type_string = configPt->get<std::string>("router_type", "Type of routing. Valid values: 'sad_nbest', 'utterance_round_robin'");
    if (router_type_string == "sad_nbest") {
        mMode = Mode::SadNbest;
        mNumRoutes = 2;
        addInputSlotAndUUID(SlotRoutingStream, UUID_AnyDecoderMessage);
    } else if (router_type_string == "utterance_round_robin") {
        mMode = Mode::UtteranceRoundRobin;
        mNumRoutes = configPt->get<int>("num_outputs", "Number of outputs to distribute to");
        mRRCurrentRoute = 0;
    } else GODEC_ERR << "Unknown router_type '" << router_type_string << "'" << std::endl;


    for (int routeIdx = 0; routeIdx < mNumRoutes; routeIdx++) {
        requiredOutputSlots.push_back(SlotConversationState + "_" + (boost::format("%1%") % routeIdx).str()); // GodecDocIgnore
        // .push_back(Slot: conversation_state_[0-9]);  // replacement for above godec doc ignore
    }

    initOutputs(requiredOutputSlots);
}

RouterComponent::~RouterComponent() {
}

void RouterComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);

    std::vector<int64_t> newRoutingIndices;
    if (mMode == Mode::SadNbest) {
        auto nbestRoutingMsg = msgBlock.get<NbestDecoderMessage>(SlotRoutingStream);
        std::copy(nbestRoutingMsg->mWords[0].begin(), nbestRoutingMsg->mWords[0].end(), std::back_inserter(newRoutingIndices));
        std::copy(nbestRoutingMsg->mAlignment[0].begin(), nbestRoutingMsg->mAlignment[0].end(), std::back_inserter(mAccumAlignment));
    } else {
        newRoutingIndices.push_back(mRRCurrentRoute);
        mAccumAlignment.push_back(convStateMsg->getTime());
    }
    std::copy(newRoutingIndices.begin(), newRoutingIndices.end(), std::back_inserter(mAccumRouteIdx));

    // This loop builds up the remaining message accumulators, and possibly adds additional entries for end-of-convo
    for(int idx = 0; idx < newRoutingIndices.size(); idx++) {
        mAccumEndOfUtt.push_back((convStateMsg->mLastChunkInUtt && (idx == newRoutingIndices.size() -1)) ? true : false);
        mAccumUttId.push_back(convStateMsg->mUtteranceId);
        mAccumEndOfConvo.push_back((convStateMsg->mLastChunkInConvo && (idx == newRoutingIndices.size() -1)) ? true : false);
        mAccumConvoId.push_back(convStateMsg->mConvoId);
    }

    while(!mAccumRouteIdx.empty()) {
        // If this is the only element and it's not a forced utt-end due to convstate, we have to defer until more data comes in
        if (mAccumRouteIdx.size() == 1 && !mAccumEndOfUtt.front()) break;

        uint64_t sliceTime = mAccumAlignment.front();
        int targetRouteIdx = mAccumRouteIdx.front();
        std::string utteranceId = mAccumUttId.front();
        bool lastChunkInUtt = mAccumEndOfUtt.front();
        std::string convoId = mAccumConvoId.front();
        bool lastChunkInConvo = mAccumEndOfConvo.front();

        for (int routeIdx = 0; routeIdx < mNumRoutes; routeIdx++) {
            DecoderMessage_ptr outConvMsg = ConversationStateDecoderMessage::create(sliceTime, utteranceId, lastChunkInUtt, convoId, lastChunkInConvo);
            (boost::const_pointer_cast<DecoderMessage>(outConvMsg))->addDescriptor(IgnoreData, routeIdx == targetRouteIdx ? "false" : "true");
            pushToOutputs(SlotConversationState + "_" + (boost::format("%1%") % routeIdx).str(), outConvMsg);
        }

        mAccumRouteIdx.erase(mAccumRouteIdx.begin());
        mAccumAlignment.erase(mAccumAlignment.begin());
        mAccumEndOfUtt.erase(mAccumEndOfUtt.begin());
        mAccumUttId.erase(mAccumUttId.begin());
        mAccumEndOfConvo.erase(mAccumEndOfConvo.begin());
        mAccumConvoId.erase(mAccumConvoId.begin());
    }
    if (mMode == Mode::UtteranceRoundRobin && convStateMsg->mLastChunkInUtt) {
        mRRCurrentRoute = (mRRCurrentRoute+1) % mNumRoutes;
    }
}

}
