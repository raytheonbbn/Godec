#include "Merger.h"
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
Refer to the Router extended description to a more detailed description
*/
MergerComponent::MergerComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    int numStreams = configPt->get<int>("num_streams", "Number of streams that are merged");
    addInputSlotAndUUID(SlotTimeMap, UUID_TimeMapDecoderMessage);

    mMessageQueue[SlotTimeMap].queueOffset = -1;
    for(int streamIdx = 0; streamIdx < numStreams; streamIdx++) {
        std::stringstream ss;
        ss << SlotInputStreamPrefix << streamIdx;
        addInputSlotAndUUID(ss.str(), UUID_AnyDecoderMessage); //GodecDocIgnore
        // addInputSlotAndUUID(input_streams_[0-9], UUID_AnyDecoderMessage);  // Replacement for above godec doc ignore
        mMessageQueue[ss.str()].queueOffset = -1;
    }
    GODEC_INFO << "MergerDbg 0 num streams " << numStreams;
    mTimeMapStream.addStream(SlotTimeMap);
    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotOutputStream);
    initOutputs(requiredOutputSlots);
}

MergerComponent::~MergerComponent() {}

int64_t getOrigTime(int64_t msgMappedEnd, const TimeMapEntry& tm) {
    int64_t msgMappedLength = msgMappedEnd - tm.startMappedTime;
    if (msgMappedLength == 0)
        return tm.startOrigTime;
    else {
        assert(tm.endMappedTime - tm.startMappedTime > 0);
        // fbernard debug
        double not_rounded = tm.startOrigTime+msgMappedLength*(tm.endOrigTime-tm.startOrigTime)/(tm.endMappedTime-tm.startMappedTime);
        GODEC_INFO << "MergerDbg 0 orig time raw " << not_rounded;
        int64_t rounded = round(not_rounded);
        GODEC_INFO << "MergerDbg 0 orig time rounded " << rounded;
        return rounded;
        //
        //return round(tm.startOrigTime+msgMappedLength*(tm.endOrigTime-tm.startOrigTime)/(tm.endMappedTime-tm.startMappedTime));
    }
}

void MergerComponent::ProcessLoop() {
    while (true) {
        DecoderMessage_ptr newMessage;
        ChannelReturnResult res = mInputChannel.get(newMessage, FLT_MAX);
        if (res == ChannelClosed) break;

        if (isVerbose()) {
            std::stringstream verboseStr;
            verboseStr << "LP " << getLPId() << ": incoming " << newMessage->describeThyself() << std::endl;
            GODEC_INFO << verboseStr.str();
        }

        if (mInputTag2Slot.find(newMessage->getTag()) == mInputTag2Slot.end()) {
            GODEC_ERR << mId << ": Can't find slot for tag '" << newMessage->getTag() << "'" << std::endl;
        }

        for(auto slotIt = mInputTag2Slot[newMessage->getTag()].begin(); slotIt != mInputTag2Slot[newMessage->getTag()].end(); slotIt++) {

            std::string& slot = *slotIt;
            auto inputSlotIt = mInputSlots.begin();
            while (inputSlotIt != mInputSlots.end()) {
                if (inputSlotIt->first == slot) break;
                inputSlotIt++;
            }
            bool foundExpected = false;
            for (auto it = inputSlotIt->second.begin(); it != inputSlotIt->second.end(); it++) {
                if (*it == UUID_AnyDecoderMessage || newMessage->getUUID() == *it) { foundExpected = true; break; }
            }
            if (!foundExpected) {
                std::string __uuid = boost::lexical_cast<std::string>(newMessage->getUUID());
                GODEC_ERR << getLPId() << ": Slot '" << slot << "' got unexpected message of UUID " << __uuid;
            }

            if (newMessage->getUUID() == UUID_TimeMapDecoderMessage) {
                GODEC_INFO << "MergerDbg adding new message";
                mTimeMapStream.addMessage(newMessage, slot);
            } else {
                mMessageQueue[slot].queue.push_back(newMessage);
            }
        }

        SingleTimeStream& timeMapStream = mTimeMapStream.getStream(SlotTimeMap);

        bool removedFromTimeMap = true;
        GODEC_INFO << "MergerDbg 1 time map stream size " << timeMapStream.size();
        while (removedFromTimeMap && timeMapStream.size() != 0) {
            int64_t lastPushedTime = -1;
            auto timeMapMsg = boost::static_pointer_cast<const TimeMapDecoderMessage>(timeMapStream.front());
            const TimeMapEntry& timeMapEntry = timeMapMsg->mMapping;
            std::stringstream ss;
            ss << SlotInputStreamPrefix << timeMapEntry.routeIndex;
            auto& mQ = mMessageQueue[ss.str()];
            bool removedFromMsgQueue = true;
            GODEC_INFO << "MergerDbg 2 msg queue size " << mQ.queue.size();
            while (removedFromMsgQueue && mQ.queue.size() != 0) {
                auto firstMsg = mQ.queue.front();
                int64_t firstMsgStart = mQ.queueOffset + 1;
                int64_t firstMsgEnd = firstMsg->getTime();
                GODEC_INFO << "MergerDbg 3 msg start " << firstMsgStart;
                GODEC_INFO << "MergerDbg 3 map start " << timeMapEntry.startMappedTime;
                GODEC_INFO << "MergerDbg 3 msg end " << firstMsgEnd;
                GODEC_INFO << "MergerDbg 3 map end " << timeMapEntry.endMappedTime;
                if (firstMsgStart >= timeMapEntry.startMappedTime && firstMsgEnd <= timeMapEntry.endMappedTime) {
                    auto clonedMsg = firstMsg->clone();
                    auto nonConstMsg = boost::const_pointer_cast<DecoderMessage>(clonedMsg);
                    lastPushedTime = getOrigTime(firstMsgEnd, timeMapEntry);
                    nonConstMsg->shiftInTime((int64_t)lastPushedTime-(int64_t)nonConstMsg->getTime());
                    if (nonConstMsg->getUUID() == UUID_NbestDecoderMessage) {
                        auto nbestMsg = boost::static_pointer_cast<NbestDecoderMessage>(nonConstMsg);
                        for(int nbestIdx = 0; nbestIdx < nbestMsg->mAlignment.size(); nbestIdx++) {
                            for(int idx = 0; idx < nbestMsg->mAlignment[nbestIdx].size(); idx++) {
                                nbestMsg->mAlignment[nbestIdx][idx] = getOrigTime(nbestMsg->mAlignment[nbestIdx][idx],timeMapEntry);
                            }
                        }
                    } else if (nonConstMsg->getUUID() == UUID_ConversationStateDecoderMessage) {
                        GODEC_ERR << "The Merger component intentionally does not support merging ConversationState messages. Instead use the original convstate stream that was fed into the Router component";
                    }
                    pushToOutputs(SlotOutputStream, clonedMsg);

                    mQ.queue.erase(mQ.queue.begin());
                    mQ.queueOffset = firstMsg->getTime();
                } else {
                    removedFromMsgQueue = false;
                }
            }
            removedFromTimeMap = false;
            GODEC_INFO << "MergerDbg 4 lastPushedTime " << lastPushedTime;
            GODEC_INFO << "MergerDbg 4 time map msg time " << timeMapMsg->getTime();
            if (lastPushedTime == timeMapMsg->getTime()) {
                timeMapStream.erase(timeMapStream.begin());
                removedFromTimeMap = true;
            }
        }
    }
    Shutdown();
}

void MergerComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {}

}

