#pragma once
#include <string>
#include <mutex>
#include "ChannelMessenger.h"

namespace Godec {

class ApiEndpoint;
class SubModule;
typedef LoopProcessor* (*GodecGetComponentFunc)(std::string, std::string, ComponentGraphConfig*);
typedef void (*GodecListComponentsFunc)();
typedef std::string (*GodecVersionFunc)();
typedef DecoderMessage_ptr (*GodecJNIToMsgFunc)(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
typedef DecoderMessage_ptr (*GodecPythonToMsgFunc)(PyObject* pMsg);
#endif
typedef
#ifdef _MSC_VER
HMODULE
#else
void*
#endif
DllPtr;

// The main component graph class. Note that when using Submodules, these classes are nested inside each other. Meaning, this class only ever contains one level of JSON
class ComponentGraph {
  public:
    ComponentGraph(std::string prefix, std::string inFile, ComponentGraphConfig* overrideTree, json& injectedEndpoints, GlobalComponentGraphVals* globalVals);
    ~ComponentGraph();
    static void PrintHelp();
    static json CreateApiEndpoint(bool verbose, std::vector<std::string> inputs, std::string output);
    void WaitTilShutdown();
    boost::shared_ptr<ApiEndpoint> GetApiEndpoint(std::string endpointName);
    void DeleteApiEndpoint(std::string endpointName);
    unordered_map<std::string, ChannelPointerList*>& getGlobalOutputSlots() {return mGlobalOutputSlots;}
    void PushMessage(std::string channelName, DecoderMessage_ptr msg);
    ChannelReturnResult PullMessage(std::string channelName, float maxTimeout, unordered_map<std::string, DecoderMessage_ptr>& newSlice);
    ChannelReturnResult PullAllMessages(std::string channelName, float maxTimeout, std::vector<unordered_map<std::string, DecoderMessage_ptr>>& newSlice);
    unordered_map<std::string, RuntimeStats> GetRuntimeStats();
    unordered_map<std::string, boost::shared_ptr<RuntimeStats> > getRuntimeStats();
    static void ListComponents(std::string dllName);
    static std::string API_ENDPOINT_SUFFIX;
    static std::string TOPLEVEL_ID;
    static std::string TREE_LEVEL_SEPARATOR;
    static std::string GetIndentationString(std::string id);
    DecoderMessage_ptr JNIToDecoderMsg(JNIEnv *env, jobject jMsg);
#ifndef ANDROID
    DecoderMessage_ptr PythonToDecoderMsg(PyObject *pMsg);
#endif
  private:
    static unordered_map < std::string, std::pair<boost::function<LoopProcessor*(std::string, ComponentGraphConfig*)>, boost::function<std::string()>>> GetComponentHash();
    std::mutex mComponentsMutex;
    unordered_map<std::string, boost::shared_ptr<LoopProcessor>> mComponents;
    unordered_map<std::string, ChannelPointerList*> mGlobalOutputSlots;

    LoopProcessor* LoadComponent(std::string compType, std::string compName, ComponentGraphConfig* compConfig);
    static DllPtr LoadGodecLibrary(std::string dllName);

    std::string mId;
    boost::shared_ptr<unordered_map<std::string, DllPtr >> mGlobalDllName2Handle;
};

} // namespace Godec
