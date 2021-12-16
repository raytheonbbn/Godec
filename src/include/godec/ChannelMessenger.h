#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "TimeStream.h"
#include "channel.h"
#ifndef ANDROID
#include <Python.h>
#endif
#include <jni.h>
#include <type_traits>
#if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ > 8))
#include <regex>
#define USE_STD_REGEX
#endif
#include <boost/uuid/string_generator.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/optional.hpp>
#include <boost/serialization/unordered_map.hpp>

namespace Godec {

#pragma region CSharp Message Structs
    typedef struct _CONVODECODERMESSAGESTRUCT
    {
        long _time;
        char* _utteranceId;
        bool _isLastChunkInUtt;
        char* _convoId;
        bool _isLastChunkInConvo;
        char* _descriptorString;
    } CONVODECODERMESSAGESTRUCT, * LP_CONVODECODERMESSAGESTRUCT;

    typedef struct _AUDIODECODERMESSAGESTRUCT
    {
        long _time;
        float* _audioData;
        unsigned int _numSamples;
        float _sampleRate;
        float _ticksPerSample;
        char* _descriptorString;
    } AUDIODECODERMESSAGESTRUCT, * LP_AUDIODECODERMESSAGESTRUCT;

    typedef struct _BINARYDECODERMESSAGESTRUCT
    {
        long _time;
        uint8_t* _data;
        int _dataSize;
        char* _format;
        char* _descriptorString;
    } BINARYDECODERMESSAGESTRUCT, * LP_BINARYDECODERMESSAGESTRUCT;

    typedef struct _JSONDECODERMESSAGESTRUCT
    {
        long _time;
        char* _json;
        char* _descriptorString;
    } JSONDECODERMESSAGESTRUCT, * LP_JSONDECODERMESSAGESTRUCT;
    
    typedef struct _DECODERMESSAGESTRUCT
    {
        //CHANGE THIS TO SOMETHING 
        void* _msg;
        char* _msgType;

    } DECODERMESSAGESTRUCT, * LP_DECODERMESSAGESTRUCT;
#pragma endregion

class ComponentGraph;
// The base message that all messages need to inherit from
class DecoderMessage {
  public:

    // Destructor
    virtual ~DecoderMessage() {}

    // The message should create a human readable string that describes its contents succinctly.
    // This string gets output when you do a "godec list", and is also scanned for by the documentstion autogen

    virtual std::string describeThyself() const = 0;

    // Clone this message entirely. Everything must be a new copy, including internal structures
    virtual DecoderMessage_ptr clone() const = 0;

    // Convert the C++ message contents into a Java class via JNI. The method needs to be defined, but unless you plan
    // on actually passing it out, you can just define a dummy with a KALDI_ERR in it
    virtual jobject toJNI(JNIEnv* env) = 0;

    // Convert the C++ message contents into a csharp struct for PInvoke calls. The method needs to be defined, but unless you plan
    // on actually passing it out, you can just define a dummy with a GODEC_ERR in it
    virtual _DECODERMESSAGESTRUCT* toCSHARP() = 0;

#ifndef ANDROID
    // Convert the C++ message contents into a Python object. The method needs to be defined, but unless you plan
    // on actually passing it out, you can just define a dummy with a KALDI_ERR in it
    virtual PyObject* toPython() = 0;
#endif

    // Due to a shortcoming of C++ (yes, it IS a shortcoming) there is no way of declaring a static function to be virtual,
    // which would be nice since we could enforce here that every derived message needs to implement fromJNI() and fromPython() functions.
    // So, instead we put the suggested signature here as a comment.
    //
    // static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg) = 0;
    // static DecoderMessage_ptr fromPython(PyObject* pMsg) = 0;
    // static DecoderMessage_ptr fromCSharp(_DECODERMESSAGESTRUCT cMsg) = 0;

    // Static functions for extracting values from JNI and Python
    static void JNIGetDecoderMessageVals(JNIEnv* env, jobject& jMsg, std::string& tag, uint64_t& time, std::string& descriptor);
#ifndef ANDROID
    static void PythonGetDecoderMessageVals(PyObject* pMsg, std::string& tag, uint64_t& time, std::string& descriptor);
#endif

    // mergeWith gets called when new content arrived at a component, and the new message gets "packed" onto the existing
    // ones. The question here to ask is, "can I merge the new message into the old one and just make that one bigger?".
    // An audio message for example can just pack new audio data into the previous audio message (provided the sample
    // rate etc are the same!), whereas say a lattice message can never be merged together with an old one (because it's
    // not always clear how to do so).
    // So, depending on whether you can merge in or not, the remainingMsg receives the "left over" message. If you merged
    // the new message entirely into the existing one, remainingMsg should be empty, otherwise it essentially becomes
    // msg. The boolean return value says whether remainingMsg is non-empty.
    virtual bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) = 0;

    // canSliceAt gets called when Godec tries to cut out a contiguous set of messages. It "asks" messages the question
    // "hey, can you be cut in half at time index sliceTime?". A lattice message can not be sliced, so it will only
    // return true if the sliceTime equals its end time. streamList is the entirety of the messages of this kind and can
    // be used additionally to answer the question, as well as streamStartOffset
    virtual bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& streamList, uint64_t streamStartOffset, bool verbose) = 0;

    // sliceOut then actually does the slicing of the message. Note that you will never be asked to slice out at a time
    // that would span more than one message; so, the only two scenarios are that you shorten the first message in the
    // streamList (putting the sliced out part into sliceMsg), or entirely pop the first message off streamList (and
    // putting it again into sliceMsg)
    virtual bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& streamList, int64_t streamStartOffset, bool verbose) = 0;

    // Call this function for shifting the entire message (including all internal time indices) by deltaT
    virtual void shiftInTime(int64_t deltaT) = 0;

    std::string getTag() const { return mTag; }
    uint64_t getTime() const { return mTime; }
    void setTag(const std::string c) { mTag = c; }
    void setTime(uint64_t time) { mTime = time; }
    void addDescriptor(const std::string& key, const std::string& val) { mDescriptors[key] = val; }
    // Descriptors are free-form key/value pairs that describe auxiliary information about the message.
    std::string getDescriptor(const std::string& key) const { return mDescriptors.find(key) != mDescriptors.end() ? mDescriptors.at(key) : ""; }
    std::string getFullDescriptorString() const {
        std::string out;
        for (auto it = mDescriptors.begin(); it != mDescriptors.end(); it++) {
            out += it->first + "=" + it->second + ";";
        }
        return out;
    }
    void setFullDescriptorString(const std::string desc) {
        std::vector<std::string> descEls;
        boost::split(descEls, desc, boost::is_any_of(";"));
        for (auto it = descEls.begin(); it != descEls.end(); it++) {
            boost::algorithm::trim(*it);
            if (*it == "") { continue; }
            std::vector<std::string> keyVal;
            boost::split(keyVal, *it, boost::is_any_of("="));
            addDescriptor(keyVal[0], keyVal[1]);
        }
    }

    // Each new message needs a unique UUID that identifies it. Go to one of those websites that generate them.
    virtual uuid getUUID() const = 0;

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & mTag;
        ar & mTime;
        ar & mDescriptors;
    }

    std::string mTag;
    uint64_t mTime;
  protected:
    std::unordered_map<std::string, std::string> mDescriptors;

};



typedef std::set<channel<DecoderMessage_ptr>*> ChannelPointerList;
class TimeStreamSegment;
class ComponentGraphConfig;

// Holds the runtime stats for each component
struct RuntimeStats {
    unordered_map<std::string, float> mWaitedOn;
    boost::timer::cpu_timer mDetailedTimer;
    uint64_t mTotalNumTicks;
};

// This is the structure holding a sliced-out block of messages
class DecoderMessageBlock {
  public:
    DecoderMessageBlock(std::string id, const unordered_map<std::string, DecoderMessage_ptr> map, int64_t prevCutoff) {mId = id; mMap = map; mPrevCutoff = prevCutoff;}
    // Get a typed message for a specific slot, and check that it is the right type
    template<class T>
    boost::shared_ptr<const T> get(const std::string& slot) const {
        auto msgPtr = getBaseMsg(slot);
        std::string typeName = typeid(T).name();
        if (T::getUUIDStatic() != msgPtr->getUUID()) { GODEC_ERR << mId << ": Message type in slot " << slot << " is not the correct type, can't cast to " << typeName << ". (msg type is " << boost::lexical_cast<std::string>(msgPtr->getUUID()) << "). Fix this in the code."; }
        return boost::static_pointer_cast<const T>(msgPtr);
    }
    // This retrieves a message as a generic DecoderMessage_ptr. You can use this when you don't know the type of the message type, e.g. when a component can deal with more than one message type for the same slot. Once you have it you can check with getUUID() on the message what type it is
    DecoderMessage_ptr getBaseMsg(const std::string& slot) const {
        auto msgIt = mMap.find(slot);
        if (msgIt == mMap.end()) { GODEC_ERR << mId << ":Component code tried to retrieve slot " << slot << " which doesn't exist in message block. This is likely due to a mismatch between addInputSlotAndUUID in the constructor and the msgBlock get() call in ProcessMessage. Fix this in the code."; }
        return msgIt->second;
    }
    unordered_map<std::string, DecoderMessage_ptr> getMap() const { return mMap;}
    // The previous cutoff (i.e. the end of the previously retrieved block). Can be useful for certain calculations and comparisons
    int64_t getPrevCutoff() const {return mPrevCutoff;}
  private:
    unordered_map<std::string, DecoderMessage_ptr> mMap;
    int64_t mPrevCutoff;
    std::string mId;
};

// The main base class all components inherit from. Read the documentation for a detailed description of what to do with it
class LoopProcessor {
  public:
    LoopProcessor(std::string id, ComponentGraphConfig* pt);
    virtual ~LoopProcessor();

    // ##### These are the only functions you should use inside your component code

    // This return the ID of the component as specificed in the JSON. "withTime" adds a current timestamp, ignore "trimmed", it's for internal use
    std::string getLPId(bool withTime=true, bool trimmed = false);
    // Was the "verbose" parameter set to "true"? Use this for verbose output of your component
    bool isVerbose() { return mVerbose; }
    // The two functions that configure the inputs and outputs. Call inside component constructor
    void initOutputs(std::list<std::string> requiredSlots);
    void addInputSlotAndUUID(std::string slot, uuid _uuid);
    // Push out a message
    void pushToOutputs(std::string slot, DecoderMessage_ptr msg);

    // ##### End of functions used inside component

    // After the whole Godec network is established, all components get called with this function. If you have anything like internal threads etc, start them here
    virtual void Start();
    // Gets called during shutdown. Don't forget to call LoopProcessor::Shutdown inside it if you choose to override it
    virtual void Shutdown();

    // Predefined slot names. Use this if possible
    static std::string SlotConversationState;
    static std::string SlotFeatures;
    static std::string SlotIvectors;
    static std::string SlotScores;
    static std::string SlotSadFeatures;
    static std::string SlotBaseFeatures;
    static std::string SlotTransformedFeatures;
    static std::string SlotStreamedAudio;
    static std::string SlotWindowedAudio;
    static std::string SlotLattice;
    static std::string SlotCompactLattice;
    static std::string SlotMatrix;
    static std::string SlotNbest;
    static std::string SlotTentativeNbest;
    static std::string SlotRawText;
    static std::string SlotDecoderStats;
    static std::string SlotCtm;
    static std::string SlotJson;
    static std::string SlotFst;
    static std::string SlotCnet;
    static std::string SlotCnetLattice;
    static std::string GatherRuntimeStats;
    static std::string QuietGodec;
    static std::string SlotTimeMap;
    static std::string SlotControl;
    static std::string SlotSearchOutput;
    static std::string SlotAudioInfo;

    static std::string SlotInputStreamPrefix;
    static std::string SlotOutputStream;

    // Don't use the following
    void startDecodingLoop();
    void addToInputChannel(DecoderMessage_ptr msg);
    unordered_map<std::string, boost::shared_ptr<RuntimeStats> > getRuntimeStats() { return mRuntimeStats; }
    channel<DecoderMessage_ptr>& getInputChannel() { return mInputChannel;}
    std::set<std::string> getOutputSlots() {
        std::set<std::string> out;
        for (auto it = mOutputSlots.begin(); it != mOutputSlots.end(); it++) { out.insert(it->first); }
        return out;
    }

    ComponentGraph* GetComponentGraph() {return mComponentGraph;}
    FILE* getLogPtr() {return mLogPtr;}

  protected:
    void ProcessLoopMessages();
    // added to handle ignoreDataTag
    void ProcessIgnoreDataMessageBlock(const DecoderMessageBlock& msgBlock);
    // The main function to override. It gets called every time a new contiguous chunk of messages is available. The map's keys are the slot names
    virtual void ProcessMessage(const DecoderMessageBlock& msgBlock) = 0;
    // Only in rare circumstances should you override this
    virtual void ProcessLoop();
    // Almost all components require convstate input except a few. Override this function if it is such a component
    virtual bool RequiresConvStateInput() { return true; }
    // Virtually all components (except SubModule) strictly enforce their in/outputs
    virtual bool EnforceInputsOutputs() { return true; }

    boost::thread mProcThread;
    std::string mId;
    bool mVerbose;
    bool mIsFinished;
    FILE* mLogPtr;
    int mNumStreams;
    static std::string SlotRoutingStream;
    static std::string SlotToRouteStream;
    static std::string SlotRoutedOutputStreamedPrefix;


    void connectInputs(unordered_map<std::string, std::set<uuid>> requiredSlots);

    ComponentGraphConfig* mPt;

    unordered_map<std::string, ChannelPointerList> mOutputSlots;
    unordered_map<std::string, std::set<uuid>> mInputSlots;
    channel<DecoderMessage_ptr> mInputChannel;

    unordered_map<std::string, std::vector<std::string>> mInputTag2Slot;
    unordered_map<std::string, std::string> mOutputSlot2Tag;
    TimeStreams mFullStream;

    friend class ComponentGraph;

    ComponentGraph* mComponentGraph;
    // Stats
    void populateRuntimeStats();
    unordered_map<std::string, boost::shared_ptr<RuntimeStats> > mRuntimeStats;
};


// The structure containing the "global" variables as set in the global_opts section. global_opts sections inside Submodules overwrite inherited values
class GlobalComponentGraphVals {
  public:
    GlobalComponentGraphVals();
    void loadGlobals(ComponentGraphConfig& pt);

    template <typename T>
    T get(const std::string& s) {
        if (!exists(s)) { GODEC_ERR << "No global var called '" << s << "'" << std::endl; }
        auto sval = Json2String(keyVals[s]);
        if constexpr (std::is_same<T, bool>::value) {
            std::istringstream ss(sval);
            bool b;
            ss >> std::boolalpha >> b;
            return b;
        } else {
            return boost::lexical_cast<T>(sval);
        }
    }

    bool exists(std::string s) { return keyVals.find(s) != keyVals.end(); }

    template <typename T>
    void put(std::string key, T val) {
        if constexpr (std::is_same<T, bool>::value) {
            keyVals[key] = val ? "true" : "false";
        } else {
            keyVals[key] = boost::lexical_cast<std::string>(val);
        }
    }

    unordered_map<std::string, ChannelPointerList*>* globalChannelPointerList;
    //private:
    unordered_map<std::string, std::string> keyVals;

    void printVals() {
        for(auto it = keyVals.begin(); it != keyVals.end(); it++) {
            std::cout << "Global: " << it->first << "=" << it->second << std::endl;
        }
    }
};

// A bit of a bad name, this is the structure containing a component's configuration (parameters etc)
class ComponentGraphConfig {
  public:
    ComponentGraphConfig(std::string id, std::string& infile, GlobalComponentGraphVals* _globalVals, ComponentGraph* cGraph);
    ComponentGraphConfig(std::string id, json cpt, GlobalComponentGraphVals* _globalVals, ComponentGraph* cGraph);

    // Probably the only function you will need from this class. It extracts a parameter from the JSON config
    template <typename T>
    T get(const std::string& s, const std::string& desc) {
        if (pt.find(s) == pt.end()) {
            GODEC_ERR << mId << ": Parameter '" << s << "' not specified" << std::endl << "Description: " << desc << std::endl;
        }
        mConsumedOptions.insert(s);
        // Resolve vars
        std::string outVal = Json2String(pt[s]);
        std::string baseVal = outVal;
#ifdef USE_STD_REGEX
        std::regex regex("\\$\\([^)]+\\)");
        std::sregex_token_iterator iter(baseVal.begin(), baseVal.end(), regex, 0);
        std::sregex_token_iterator end;
#else
        boost::regex regex("\\$\\([^)]+\\)");
        boost::sregex_token_iterator iter(baseVal.begin(), baseVal.end(), regex, 0);
        boost::sregex_token_iterator end;
#endif
        for (; iter != end; iter++) {
            std::string var = *iter;
            var = var.substr(2, var.length() - 3);
            if (!globalVals.exists(var)) { GODEC_ERR << mId << ": Global variable '" << var << "' is not defined!" << std::endl; }
            boost::replace_all(outVal, "$(" + var + ")", globalVals.get<std::string>(var));
        }
        pt[s] = outVal;
        try {
            if constexpr (std::is_same<T, bool>::value) {
                std::istringstream ss(outVal);
                bool b;
                ss >> std::boolalpha >> b;
                return b;
            } else {
                return boost::lexical_cast<T>(outVal);
            }
        } catch (const std::exception& e) {
            GODEC_ERR << mId << ": Unable to extract parameter '" << s << "' of type " << typeid(T).name() << std::endl << std::endl << "Reason: " << e.what();
        }
        // Only here to quiet the compiler
        GODEC_ERR << mId << ": This code path should never be executed";
        return *((T*)nullptr);
    }

    /* This function should NOT be used unless in very specific circumstances!
       The idea is, all parameters in the JSON should be non-optional, because that enables the internal checks the ComponentGraphConfig class provides. By using
       the optional version, you disable all of those checks, and the user is left guessing what parameters are necessary. Even worse, it might use default values nobody knows
       until they look it up in the code.
       You CAN use this function when a parameter is truly optional, and has no functional influence on the component */
    template <typename T>
    boost::optional<T> get_optional_READ_DECLARATION_BEFORE_USE(std::string s) {
        auto val = pt.find(s);
        try {
            if (!pt.empty() && val != pt.end()) {
                auto sval = Json2String(*val);
                if constexpr (std::is_same<T, bool>::value) {
                    std::istringstream ss(sval);
                    bool b;
                    ss >> std::boolalpha >> b;
                    return boost::optional<bool>(b);
                } else {
                    return boost::optional<T>(boost::lexical_cast<T>(sval));
                }
            }
        } catch (const std::exception& e) {
            GODEC_ERR << mId << ": Unable to extract parameter '" << s << "' of type " << typeid(T).name() << std::endl << std::endl << "Reason: " << e.what();
        }
        return boost::none;
    }

    void ParameterCheck();
    std::string AsString();
    static ComponentGraphConfig* FromOverrideList(std::vector<std::pair<std::string, std::string>> ov);
    ComponentGraphConfig* GetSubtree(std::string child, bool remove);
    void AddSubtree(json& subtree);
    void AddNode(json::json_pointer key, std::string val, bool forceCreation);
    json& GetPtree() { return pt; }
    void printGlobals() {globalVals.printVals();}

    json& get_json_child(std::string s) {
        if (pt.find(s) == pt.end()) {
            GODEC_ERR << mId << ": JSON child '" << s << "' does not exist" << std::endl;
        }
        return pt[s];
    }

    json& get_parameter(std::string s, std::string desc) {
        if (pt.find(s) == pt.end()) {
            GODEC_ERR << mId << ": Parameter '" << s << "' not specified" << std::endl << "Description: " << desc << std::endl;
        }
        mConsumedOptions.insert(s);
        return pt[s];
    }

    int32_t numChildren() {return (int32_t)pt.size();}

    GlobalComponentGraphVals globalVals;

    ComponentGraph* GetComponentGraph() {return mComponentGraph;}

  private:
    std::string mId;
    json pt;
    unordered_set<std::string> mConsumedOptions;
    bool mTopLevel;
    ComponentGraph* mComponentGraph;
};

} // namespace Godec
