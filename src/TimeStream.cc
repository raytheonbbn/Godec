#include <godec/TimeStream.h>
#include <godec/ChannelMessenger.h>
#include "core_components/GodecMessages.h"
#include <boost/format.hpp>
#include <algorithm>
#include <limits>
#include <iomanip>

namespace Godec {

// Add a new stream
void TimeStreams::addStream(std::string streamName) {
    mStream[streamName] = SingleTimeStream();
}

// Insert message into stream
void TimeStreams::addMessage(DecoderMessage_ptr msg, std::string slot) {
    if (mStream.find(slot) == mStream.end()) GODEC_ERR << "Feeding into uninitialized stream '" << slot << "'";
    SingleTimeStream& stream = mStream[slot];
    if (stream.size() == 0) {
        stream.push_back(msg->clone()); // We have to place cloned messages because otherwise we're modifying shared messages
        return;
    }
    DecoderMessage_ptr remainingMsg;
    auto lastMsg = const_cast<DecoderMessage*>(stream[stream.size() - 1].get());

    // Consistency checking
    if (lastMsg->getTime() >= msg->getTime()) GODEC_ERR << mId << ": Received out-of-order messages in slot " << slot << ". Previous msg: " << std::endl << "  " << lastMsg->describeThyself() << std::endl << "  " << msg->describeThyself() << std::endl;

    bool isThereRemainderMessage = lastMsg->mergeWith(msg->clone(), remainingMsg, mVerbose);
    if (isThereRemainderMessage) {
        stream.push_back(remainingMsg);
    }
}

// Find the slot with the shortest total amount of time span in it
std::string TimeStreams::getLeastFilledSlot() {
    uint64_t lowestTime = std::numeric_limits<uint64_t>::max();
    std::string lowestSlot;
    for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
        std::string slot = slotIt->first;
        auto& list = slotIt->second;
        uint64_t thisTime = 0;
        if (list.size() != 0) thisTime = list.back()->getTime();
        if (thisTime < lowestTime) {
            lowestSlot = slot;
            lowestTime = thisTime;
        } else if (thisTime == lowestTime && lowestSlot != "") {
            lowestSlot = "multiple";
        }
    }
    return lowestSlot;
}

SingleTimeStream::SingleTimeStream() {
    mStreamOffset = -1;
}

// Just sandwich functions that call the message-specific canSliceAt and sliceOut
bool SingleTimeStream::canSliceAt(uint64_t sliceTime, bool verbose, std::string& id) {
    if (size() == 0) return false;
#ifdef DEBUG
    if (verbose) {
        std::stringstream verboseStr;
        verboseStr << "Checking stream slicing " << (*this)[0]->describeThyself()
                   << ", " << sliceTime << " sliceTime, "
                   << ", " << (*this)[0]->getTime() << " (*this)[0]->getTime()"
                   << std::endl;
        GODEC_INFO << verboseStr.str();
    }
#endif
    auto ptr = const_cast<DecoderMessage*>((*this)[0].get());
    if (sliceTime > ptr->getTime()) GODEC_ERR << id << ": We should not slice past the first message: sliceTime (" << sliceTime << ") can not be greater than message time (" << ptr->getTime() << ")";
    return ptr->canSliceAt(sliceTime, *this, mStreamOffset, verbose);
}

bool SingleTimeStream::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, bool verbose, std::string& id) {
    if (size() == 0) return false;
    auto ptr = const_cast<DecoderMessage*>((*this)[0].get());
    if (sliceTime > ptr->getTime()) GODEC_ERR << id << ": We should not slice past the first message: sliceTime (" << sliceTime << ") can not be greater than message time (" << ptr->getTime() << ")";
    bool successVal = ptr->sliceOut(sliceTime, sliceMsg, *this, mStreamOffset, verbose);
    if (!successVal) return false;
    mStreamOffset = sliceTime;
    return true;
}

std::string padToLength(std::string s, int length) {
    std::ostringstream ss;
    ss << std::setw(length) << std::setfill(' ') << s;
    return ss.str();
}

// Diagnostic print of the entire structure
std::string TimeStreams::print() {
    std::set<uint64_t> timeSet;
    std::stringstream ss;
    ss << std::endl;
    for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
        std::string slot = slotIt->first;
        ss << slot << " | ";
        auto& list = slotIt->second;
        for (auto msgIt = list.begin(); msgIt != list.end(); msgIt++) {
            timeSet.insert((*msgIt)->getTime());
        }
    }
    ss << std::endl;
    for(auto timeIt = timeSet.begin(); timeIt != timeSet.end(); timeIt++) {
        for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
            std::string slot = slotIt->first;
            auto& list = slotIt->second;
            bool foundTime = false;
            for (auto msgIt = list.begin(); msgIt != list.end(); msgIt++) {
                if ((*msgIt)->getTime() == (*timeIt)) foundTime = true;
            }
            if (foundTime) {
                std::string timeString = boost::str(boost::format("%1%") % (*timeIt));
                ss << padToLength(timeString, (int)slot.length()) << " | ";
            } else {
                ss << padToLength("", (int)slot.length()) << " | ";
            }
        }
        ss << std::endl;
    }
    ss << std::endl;
    return ss.str();
}

// The key function that slices out a continguous ("coherent") chunk of messages
unordered_map<std::string, DecoderMessage_ptr> TimeStreams::getNewCoherent(int64_t& previousCutoff) {
    // Hmm, the following loop seems to do just about the same as getLeastFilledSlot(). Might have to revisit if this ever exhibits a bug or is shown to be inefficient. It establishes the lowest timestamp across the streams
    uint64_t lastFullyAccountedForTime = std::numeric_limits<uint64_t>::max();
    for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
        std::string slot = slotIt->first;
        auto& list = slotIt->second;
        uint64_t thisStreamTimeAccountedFor = 0;
        for (auto msgIt = list.begin(); msgIt != list.end(); msgIt++) {
            thisStreamTimeAccountedFor = (*msgIt)->getTime();
        }
        //GODEC_INFO << mId << ": Slot " << slotIt->first << ": this stream acc: " << thisStreamTimeAccountedFor << std::endl;
        lastFullyAccountedForTime = std::min(lastFullyAccountedForTime, thisStreamTimeAccountedFor);
        //GODEC_INFO << mId << " ->mandatoryMinTime" << mandatoryMinTime << std::endl;
    }
    // Collect the set of possible slice times that we need to ask the messages for
    std::set<uint64_t> sliceTimes;
    for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
        std::string slot = slotIt->first;
        auto& list = slotIt->second;
        for (auto msgIt = list.begin(); msgIt != list.end(); msgIt++) {
            DecoderMessage_ptr& msg = *msgIt;
            uint64_t msgTime = msg->getTime();
            if ((int64_t) msgTime > previousCutoff && msgTime <= lastFullyAccountedForTime) {
                //if (mVerbose) GODEC_INFO << "Inserting slice time " << msgTime << std::endl;
                sliceTimes.insert(msgTime);
            }
        }
    }
    unordered_map<std::string, DecoderMessage_ptr> outList;
    if (sliceTimes.size() > 0) {
        // We really only look at the first slice time in the list
        uint64_t sliceTime = *(sliceTimes.begin());
#ifdef DEBUG
        if (mVerbose) {
            std::stringstream ss;
            ss << "Trying to slice at time  " << sliceTime << ":" << std::endl;
            for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
                std::string slot = slotIt->first;
                auto& stream = slotIt->second;
                ss << "  " << slot << ": ";
                for(auto streamIt = stream.begin(); streamIt != stream.end(); streamIt++) {
                    DecoderMessage_ptr& msg = (*streamIt);
                    ss << "[" << msg->getTime() << "] ";
                    if (msg->getUUID() == UUID_FeaturesDecoderMessage) {
                        auto featMsg = boost::static_pointer_cast<const FeaturesDecoderMessage>(msg);
                        ss << "(";
                        for(auto timeIt = featMsg->mFeatureTimestamps.begin(); timeIt != featMsg->mFeatureTimestamps.end(); timeIt++) {
                            ss << *timeIt << ",";
                        }
                        ss << ") ";
                    }
                }
                ss << std::endl;
            }
            GODEC_INFO << ss.str();
        }
#endif
        // Can we slice all messages at this time?
        bool canSliceAllAtFirstTime = true;
        for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
            std::string slot = slotIt->first;
            auto& stream = slotIt->second;

            DecoderMessage_ptr sliceMsg;
            std::string comboId = mId+"->"+slot;
            if (!stream.canSliceAt(sliceTime, mVerbose, comboId)) {
                canSliceAllAtFirstTime = false;
                break;
            }
        }
        if (!canSliceAllAtFirstTime) {
            // Let's check whether the reason for not getting a new chunk is because it's degenerate
            bool allStreamsMoreThan2 = true;
            for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
                auto& list = slotIt->second;
                if (list.size() < 2) allStreamsMoreThan2 = false;
            }
            if (allStreamsMoreThan2) {
                std::stringstream ss;
                ss << mId << ": Detected degenerate TimeStream. This is indicative of one of the upstream components a) not calculating the output ticks correctly or b) said component dropping content which then might cause the next message to span across utts. In rare cases this error can also indicate that one of the message types is incoherent in its canSliceAt() and sliceOut() functions." << std::endl << std::endl;
                ss << "Here's the TimeStream content for analysis. Times should ideally line up. " << std::endl;
                ss << print();
                GODEC_ERR << ss.str();
            }
            // Nope, just not enough stuff in the stream yet. Just return empty list
            return outList;
        }

        //Actually slice
        for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
            std::string slot = slotIt->first;
            auto& stream = slotIt->second;

            DecoderMessage_ptr sliceMsg;
            std::string comboId = mId+"->"+slot;
            if (stream.sliceOut(sliceTime, sliceMsg, mVerbose, comboId)) {
                if (sliceMsg->getTime() != sliceTime)
                    GODEC_ERR << "Sliced-out message needs to have correct time";
                outList[slot] = sliceMsg;
            }
        }
#if DEBUG
        if (mVerbose) {
            std::stringstream verboseBuff;
            verboseBuff << "New coherent:" << std::endl;
            for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
                std::string slot = slotIt->first;
                verboseBuff << "   " << outList[slot]->describeThyself() << std::endl;
            }
            GODEC_INFO << verboseBuff.str();
        }
#endif

        previousCutoff = (int64_t) sliceTime;
    }
    return outList;
}

bool TimeStreams::isEmpty() {
    bool isEmpty = true;
    for (auto slotIt = mStream.begin(); slotIt != mStream.end(); slotIt++) {
        std::string slot = slotIt->first;
        auto& stream = slotIt->second;
        if (stream.size() != 0) isEmpty = false;
    }
    return isEmpty;
}

}
