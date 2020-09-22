#include <godec/ChannelMessenger.h>
#include <godec/ComponentGraph.h>
#include "core_components/GodecMessages.h"
#include <godec/json.hpp>
#include <boost/bimap.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <time.h>
#include <fstream>

namespace Godec {

// Default slot names, use these unless your special component does something more specific
std::string LoopProcessor::SlotConversationState = "conversation_state";
std::string LoopProcessor::SlotFeatures = "features";
std::string LoopProcessor::SlotIvectors = "ivectors";
std::string LoopProcessor::SlotScores = "scores";
std::string LoopProcessor::SlotSadFeatures = "sad_features";
std::string LoopProcessor::SlotBaseFeatures = "base_features";
std::string LoopProcessor::SlotTransformedFeatures = "transformed_features";
std::string LoopProcessor::SlotStreamedAudio = "streamed_audio";
std::string LoopProcessor::SlotWindowedAudio = "windowed_audio";
std::string LoopProcessor::SlotNbest = "nbest";
std::string LoopProcessor::SlotTentativeNbest = "tentative_nbest";
std::string LoopProcessor::SlotLattice = "lattice";
std::string LoopProcessor::SlotCompactLattice = "compact_lattice";
std::string LoopProcessor::SlotMatrix = "matrix";
std::string LoopProcessor::SlotCtm = "ctm";
std::string LoopProcessor::SlotJson = "json";
std::string LoopProcessor::SlotRawText = "raw_text";
std::string LoopProcessor::SlotDecoderStats = "decoder_stats";
std::string LoopProcessor::SlotCnet = "cnet";
std::string LoopProcessor::SlotFst = "fst";
std::string LoopProcessor::SlotCnetLattice = "cnet_lattice";
std::string LoopProcessor::GatherRuntimeStats = "gather_runtime_stats";
std::string LoopProcessor::QuietGodec = "quiet_godec";
std::string LoopProcessor::SlotTimeMap = "time_map";
std::string LoopProcessor::SlotControl = "control";
std::string LoopProcessor::SlotSearchOutput = "fst_search_output";
std::string LoopProcessor::SlotAudioInfo = "audio_info";

std::string DecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << "[" << getTag() << "," << getTime() << "] ";
    return ss.str();
}

void DecoderMessage::JNIGetDecoderMessageVals(JNIEnv* env, jobject& jMsg, std::string& tag, uint64_t& time, std::string& descriptor) {
    jclass DecoderMessageClass = env->FindClass("com/bbn/godec/DecoderMessage");
    jfieldID jTagId = env->GetFieldID(DecoderMessageClass, "mTag", "Ljava/lang/String;");
    jfieldID jTimeId = env->GetFieldID(DecoderMessageClass, "mTime", "J");
    jstring jTagObj = (jstring)env->GetObjectField(jMsg, jTagId);
    const char* tagChars = env->GetStringUTFChars(jTagObj, 0);
    tag = tagChars;
    env->ReleaseStringUTFChars(jTagObj, tagChars);
    time = env->GetLongField(jMsg, jTimeId);

    jmethodID jGetDescriptor = env->GetMethodID(DecoderMessageClass, "getFullDescriptorString", "()Ljava/lang/String;");
    jstring jDescriptor = (jstring)env->CallObjectMethod(jMsg, jGetDescriptor);
    const char* descriptorChars = env->GetStringUTFChars(jDescriptor, 0);
    descriptor = descriptorChars;
    env->ReleaseStringUTFChars(jDescriptor, descriptorChars);
}

#ifndef ANDROID
void DecoderMessage::PythonGetDecoderMessageVals(PyObject* pMsg, std::string& tag, uint64_t& time, std::string& descriptor) {
    PyObject* pTag = PyDict_GetItemString(pMsg,"tag");
    if (pTag == nullptr) GODEC_ERR << "Python message dict does not contain 'tag' entry!";
    tag = PyUnicode_AsUTF8(pTag);
    PyObject* pTime = PyDict_GetItemString(pMsg,"time");
    if (pTime == nullptr) GODEC_ERR << "Python message dict does not contain 'time' entry!";
    time = PyLong_AsLong(pTime);
    PyObject* pDesc = PyDict_GetItemString(pMsg,"descriptor");
    if (pDesc == nullptr) GODEC_ERR << "Python message dict does not contain 'descriptor' entry!";
    descriptor = PyUnicode_AsUTF8(pDesc);
}
#endif


/*
############ Loop processor ###################
*/
LoopProcessor::LoopProcessor(std::string id, ComponentGraphConfig* pt) : mVerbose(false), mIsFinished(false) {
    mId = id;
    mComponentGraph = pt->GetComponentGraph();

    if (pt->get_optional_READ_DECLARATION_BEFORE_USE<bool>("verbose")) {
        mVerbose = pt->get<bool>("verbose", "Shows incoming and outgoing messages, as well as internal proceedings of the component");
    }
    if (pt->globalVals.get<bool>(GatherRuntimeStats)) {
        mRuntimeStats[getLPId(false, true)] = boost::shared_ptr<RuntimeStats>(new RuntimeStats());
    }

    mLogPtr = stderr;
    if (pt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("log_file")) {
        std::string logFileString = pt->get<std::string>("log_file", "Location of the log file for this component");
        mLogPtr = fopen(logFileString.c_str(), "wb");
        if (mLogPtr == NULL) GODEC_ERR << "Couldn't open log file " << logFileString << " for component " << mId;
    }

    mInputChannel.setIdVerbose(mId, mVerbose);
    bool debugSlicing = false;
    if (pt->get_optional_READ_DECLARATION_BEFORE_USE<bool>("debug_slicing")) {
        debugSlicing = pt->get<bool>("debug_slicing", "Show how the component tries to slice the messages");
    }
    mFullStream.setIdVerbose(id, debugSlicing);
    mPt = pt;
}

void LoopProcessor::addInputSlotAndUUID(std::string slot, uuid _uuid) {
    mInputSlots[slot].insert(_uuid);
}

void LoopProcessor::initOutputs(std::list<std::string> requiredSlots) {
    unordered_set<std::string> filledInSlots;
    if (mPt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("outputs")) {
        auto outputs = mPt->get_json_child("outputs");
        for(auto v = outputs.begin(); v != outputs.end(); v++) {
            std::string slot = v.key();
            std::string tag = v.value();
            mOutputSlot2Tag[slot] = tag;
            mOutputSlots[slot] = std::set < channel<DecoderMessage_ptr>* >();
            filledInSlots.insert(slot);
            if (mPt->globalVals.globalChannelPointerList->find(tag) != mPt->globalVals.globalChannelPointerList->end()) GODEC_ERR << getLPId(false) << ":Trying to redefine output slot '" << tag << "'" << std::endl;
            (*(mPt->globalVals.globalChannelPointerList))[tag] = &mOutputSlots[slot];
        }
    }

    for (auto it = requiredSlots.begin(); it != requiredSlots.end(); it++) {
        if (filledInSlots.find(*it) == filledInSlots.end()) GODEC_ERR << "Component '" << getLPId(false) << "': Required output slot '" << *it << "' not defined" << std::endl;
    }
    if (EnforceInputsOutputs()) {
        for (auto it = filledInSlots.begin(); it != filledInSlots.end(); it++) {
            if (std::find(requiredSlots.begin(), requiredSlots.end(), *it) == requiredSlots.end()) GODEC_ERR << "Component '" << getLPId(false) << "': Specified unnecessary output slot '" << *it << "'" << std::endl;
        }
    }
}

void LoopProcessor::connectInputs(unordered_map<std::string, std::set<uuid>> requiredSlots) {
    bool convoStateAlreadyDefined = false;
    for (auto it = mInputSlots.begin(); it != mInputSlots.end(); it++) {
        if (it->first == SlotConversationState) convoStateAlreadyDefined = true;
    }
    if (convoStateAlreadyDefined) GODEC_ERR << getLPId(false) << "All components are now required to consume the conversation state, so no need to explicitly add it. Remove from component code";
    if (RequiresConvStateInput()) {
        addInputSlotAndUUID(SlotConversationState, UUID_ConversationStateDecoderMessage);
        requiredSlots[SlotConversationState].insert(UUID_ConversationStateDecoderMessage);
    }
    if (mPt->get_optional_READ_DECLARATION_BEFORE_USE<std::string>("inputs")) {
        auto inputs = mPt->get_json_child("inputs");
        for(auto v = inputs.begin(); v != inputs.end(); v++) {
            std::string slot = v.key();
            std::string tag = v.value();

            mInputTag2Slot[tag].push_back(slot);
            if (mPt->globalVals.globalChannelPointerList->find(tag) == mPt->globalVals.globalChannelPointerList->end()) GODEC_ERR << getLPId(false) << ": No such output slot '" << tag << "'" << std::endl;
            auto list = (*(mPt->globalVals.globalChannelPointerList))[tag];
            if (list->find(&mInputChannel) == list->end()) {
                list->insert(&mInputChannel);
                mInputChannel.checkIn(tag);
            }
            bool alreadyDefined = false;
            for (auto it = mInputSlots.begin(); it != mInputSlots.end(); it++) {
                if (it->first == slot) alreadyDefined = true;
            }
            if (!alreadyDefined) {
                addInputSlotAndUUID(slot, UUID_AnyDecoderMessage);
            }
        }
    }

    for (auto reqIt = mInputSlots.begin(); reqIt != mInputSlots.end(); reqIt++) {
        bool found = false;
        for (auto tagIt = mInputTag2Slot.begin(); tagIt != mInputTag2Slot.end(); tagIt++) {
            for (auto slotIt = tagIt->second.begin(); slotIt != tagIt->second.end(); slotIt++) {
                if (*slotIt == reqIt->first) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) GODEC_ERR << "Component '" << getLPId(false) << "': Required input slot '" << reqIt->first << "' not defined!" << std::endl;
    }
    if (EnforceInputsOutputs()) {
        for (auto tagIt = mInputTag2Slot.begin(); tagIt != mInputTag2Slot.end(); tagIt++) {
            for (auto slotIt = tagIt->second.begin(); slotIt != tagIt->second.end(); slotIt++) {
                bool found = false;
                for (auto reqIt = requiredSlots.begin(); reqIt != requiredSlots.end(); reqIt++) {
                    if (*slotIt == reqIt->first) {
                        found = true;
                        break;
                    }
                }
                if (!found) GODEC_ERR << "Component '" << getLPId(false) << "': Specified unnecessary input slot '" << *slotIt << "'!" << std::endl;
            }
        }
    }
    for (auto slotIt = mInputSlots.begin(); slotIt != mInputSlots.end(); slotIt++) {
        mFullStream.addStream(slotIt->first);
    }
    if (mInputSlots.size() == 0) mInputChannel.checkIn(getLPId(false));
}

LoopProcessor::~LoopProcessor() {
    if (mInputSlots.size() == 0) mInputChannel.checkOut(getLPId(false));
    mProcThread.join();
}

void LoopProcessor::Start() {
    startDecodingLoop();
    auto statsPtr = getRuntimeStats()[getLPId(false, true)];
    if (statsPtr != nullptr) statsPtr->mDetailedTimer.start();
}

std::string LoopProcessor::getLPId(bool withTime, bool trimmed) {
    std::stringstream ss;
    std::string trimmedId = mId;
    if (trimmed) {
        if (trimmedId.find(ComponentGraph::TOPLEVEL_ID) != std::string::npos) trimmedId = trimmedId.substr((ComponentGraph::TOPLEVEL_ID + ComponentGraph::TREE_LEVEL_SEPARATOR).length());
        if (trimmedId.find(ComponentGraph::API_ENDPOINT_SUFFIX) != std::string::npos) trimmedId = trimmedId.substr(0, trimmedId.length()- ComponentGraph::API_ENDPOINT_SUFFIX.length());
    }
    ss << trimmedId;
    if (withTime) {
        ss.precision(6);
        ss << std::fixed;
#ifdef ANDROID
        // Switch to timespec_get on Android once it is available
        auto seconds = (boost::posix_time::microsec_clock::local_time() - boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1))).total_nanoseconds()/1.0E9;
#else
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);
        auto seconds = ts.tv_sec+ts.tv_nsec/1.0E9;
#endif
        ss << "(" << seconds << ")";
    }
    return ss.str();
}

void LoopProcessor::startDecodingLoop() {
    mProcThread = boost::thread(&LoopProcessor::ProcessLoop, this);
    RegisterThreadForLogging(mProcThread, mLogPtr, isVerbose());
}

void LoopProcessor::ProcessLoopMessages() {
    int64_t timeCutoff = -1;
    auto statsPtr = getRuntimeStats()[getLPId(false, true)];
    ChannelReturnResult res;
    while (true) {
        do {
            DecoderMessage_ptr newMessage;

            std::string leastFilledSlot;
            if (statsPtr != nullptr) {
                boost::chrono::duration<float> seconds = boost::chrono::nanoseconds(statsPtr->mDetailedTimer.elapsed().wall);
                statsPtr->mWaitedOn["myself"] += seconds.count();
                leastFilledSlot = mFullStream.getLeastFilledSlot();
                statsPtr->mDetailedTimer.start();
            }
            res = mInputChannel.get(newMessage, FLT_MAX);
            if (res == ChannelClosed)  break;

            if (statsPtr != nullptr) {
                boost::chrono::duration<float> seconds = boost::chrono::nanoseconds(statsPtr->mDetailedTimer.elapsed().wall);
                statsPtr->mWaitedOn[leastFilledSlot] += seconds.count();
                statsPtr->mDetailedTimer.start();
            }

            if (isVerbose()) {
                std::stringstream verboseStr;
                verboseStr << "LP " << getLPId() << ": incoming " << newMessage->describeThyself() << std::endl;
                GODEC_INFO << verboseStr.str();
            }

            if (mInputTag2Slot.find(newMessage->getTag()) == mInputTag2Slot.end()) {
                GODEC_ERR << getLPId(false) << ": Can't find slot for tag '" << newMessage->getTag() << "'" << std::endl;
            }

            for (auto slotIt = mInputTag2Slot[newMessage->getTag()].begin(); slotIt != mInputTag2Slot[newMessage->getTag()].end(); slotIt++) {
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
                    GODEC_ERR << getLPId(false) << ": Slot '" << slot << "' got unexpected message of UUID " << __uuid;
                }

                mFullStream.addMessage(newMessage, slot);
            }
        } while(mInputChannel.getNumItems() > 0);

        bool gotCoherent = false;
        do {
            gotCoherent = false;
            int64_t prevCutoff = timeCutoff;
            auto newMessages = mFullStream.getNewCoherent(timeCutoff);
            if (newMessages.size() > 0) {
                DecoderMessageBlock msgBlock(getLPId(false), newMessages, prevCutoff);
                ProcessMessage(msgBlock);
                if ((statsPtr != nullptr) && isVerbose()) {
                    boost::chrono::duration<double> seconds = boost::chrono::nanoseconds(statsPtr->mDetailedTimer.elapsed().wall);
                    GODEC_INFO << "LP " << getLPId() << ": Took " << seconds.count() << "s to process " << (timeCutoff-prevCutoff) << " ticks" << std::endl;
                }
                gotCoherent = true;
            }
        } while (gotCoherent);
        if (statsPtr != nullptr) statsPtr->mTotalNumTicks = timeCutoff;
        if (res == ChannelClosed)  break;
    }
    if (!mFullStream.isEmpty()) {
        GODEC_ERR << mId << ":TimeStream structure was not empty at shutdown. this is a bug. This is the content: " << std::endl << mFullStream.print() << std::endl;
    }
}

void LoopProcessor::ProcessLoop() {
    ProcessLoopMessages();
    Shutdown();
}

void LoopProcessor::Shutdown() {
    for (auto slotIt = mOutputSlots.begin(); slotIt != mOutputSlots.end(); slotIt++) {
        auto& channelList = slotIt->second;
        for (auto channelIt = channelList.begin(); channelIt != channelList.end(); channelIt++) {
            (*channelIt)->checkOut(mOutputSlot2Tag[slotIt->first]);
        }
    }
    mIsFinished = true;
}

void LoopProcessor::addToInputChannel(DecoderMessage_ptr msg) {
    mInputChannel.put(msg);
}

void LoopProcessor::pushToOutputs(std::string slot, DecoderMessage_ptr msg) {
    GODEC_INFO << "ChannelMsgrDbg pushing to slot " << slot;
    
    auto slotIt = mOutputSlots.find(slot);
    if (slotIt == mOutputSlots.end()) {
        GODEC_ERR << getLPId(false) << ":Trying to push to undefined output slot '" << slot << "'. This is a bug in the component code. " << std::endl;
    }
    auto nonConstMsg = boost::const_pointer_cast<DecoderMessage>(msg);

    GODEC_INFO << "ChannelMsgrDbg msg description " << nonConstMsg->describeThyself();

    nonConstMsg->setTag(mOutputSlot2Tag[slot]);

    if (isVerbose() && slotIt->second.size() != 0) {
        std::stringstream verboseStr;
        verboseStr << "LP " << getLPId() << ": Pushing to " << slotIt->second.size() << " consumers:" << msg->describeThyself();
        GODEC_INFO << verboseStr.str();
    }
    for (auto it = slotIt->second.begin(); it != slotIt->second.end(); it++) {
        (*it)->put(msg);
    }
}

GlobalComponentGraphVals::GlobalComponentGraphVals() {
    put<bool>(LoopProcessor::GatherRuntimeStats, false);
    put<bool>(LoopProcessor::QuietGodec, false);
}

void GlobalComponentGraphVals::loadGlobals(ComponentGraphConfig& pt) {
    auto& ptree = pt.GetPtree();
    for(auto v = ptree.begin(); v != ptree.end(); v++) {
        put(v.key(), Json2String(v.value()));
    }
}

ComponentGraphConfig::ComponentGraphConfig(std::string id, std::string& inFile, GlobalComponentGraphVals* _globalVals, ComponentGraph* cGraph) {
    try {
        std::ifstream input(inFile);
        if (input.fail()) GODEC_ERR << "Could not load JSON file '" << inFile << "'";
        std::stringstream sstr;
        while(input >> sstr.rdbuf());
        std::stringstream cleanJson;
        cleanJson << StripCommentsFromJSON(sstr.str());
        pt = json::parse(cleanJson);
    } catch (const json::parse_error& e) {
        GODEC_ERR << "Could not parse JSON file '" << inFile << "', error: " << e.what() << ".\nPlease make sure the file exists and is a valid JSON (i.e. no trailing commas)." << std::endl;
    }
    mTopLevel = true;
    mId = id;
    mComponentGraph = cGraph;
    if (_globalVals != NULL) globalVals = *_globalVals;
}

ComponentGraphConfig::ComponentGraphConfig(std::string id, json cpt, GlobalComponentGraphVals* _globalVals, ComponentGraph* cGraph) {
    pt = cpt;
    mId = id;
    mComponentGraph = cGraph;
    mTopLevel = false;
    if (_globalVals != NULL) globalVals = *_globalVals;
};

void ComponentGraphConfig::ParameterCheck() {
    if (mTopLevel) return;
    for(auto v = pt.begin(); v != pt.end(); v++) {
        std::string p = v.key();
        if (
            p[0] == '#' ||
            p == "type" ||
            p == "inputs" ||
            p == "outputs" ||
            p == "override" ||
            p == "verbose"
        ) continue;
        if (mConsumedOptions.find(p) == mConsumedOptions.end()) {
            GODEC_ERR << mId << ": Superfluous parameter '" << v.key() << "'. Either remove, or park it by prefixing it with '#' character" << std::endl;
        }
    }
}

void ComponentGraphConfig::AddSubtree(json& subTree) {
    json newTree;
    OverlayPropertyTrees(pt, "", subTree, "", newTree);
    pt = newTree;
}

void ComponentGraphConfig::AddNode(json::json_pointer key, std::string val, bool forceCreation) {
    auto ptChild = pt.find(key);
    if (ptChild == pt.end()) {
        if (!forceCreation) GODEC_ERR << "Trying to override non-existing key " << key;
    }
    pt[key] = val;
}

ComponentGraphConfig* ComponentGraphConfig::FromOverrideList(std::vector<std::pair<std::string, std::string>> ov) {
    json emptyTree;
    ComponentGraphConfig* cgc = new ComponentGraphConfig("overrides", emptyTree, nullptr, nullptr);
    for (auto it = ov.begin(); it != ov.end(); it++) {
        std::string key = it->first;
        std::replace(key.begin(), key.end(), '.', '/');
        cgc->AddNode(json::json_pointer("/"+key), it->second, true);
    }
    return cgc;
}

ComponentGraphConfig* ComponentGraphConfig::GetSubtree(std::string key, bool remove) {
    json subTree;
    if (pt.find(key) != pt.end()) {
        subTree = pt[key];
        if (remove) {
            pt.erase(key);
        }
    }
    ComponentGraphConfig* out = new ComponentGraphConfig("overrides", subTree, nullptr, nullptr);
    return out;
}

std::string ComponentGraphConfig::AsString() {
    return pt.dump(4);
}

}
