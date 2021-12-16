#include <godec/ComponentGraph.h>
#include <godec/HelperFuncs.h>
#include <godec/version.h>
#include <godec/json.hpp>
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "core_components/GodecMessages.h"
#include "core_components/ApiEndpoint.h"
#include "core_components/SubModule.h"
#include <iomanip>

#ifndef _MSC_VER
#include <dlfcn.h>
#else
#include <windows.h>
#endif

namespace Godec {

std::string ComponentGraph::API_ENDPOINT_SUFFIX = "(Endpoint)";
std::string ComponentGraph::TOPLEVEL_ID = "Toplevel";
std::string ComponentGraph::TREE_LEVEL_SEPARATOR = ".";

ComponentGraph::ComponentGraph(std::string prefix, std::string inFile, ComponentGraphConfig* overrideTree, json& injectedEndpoints, GlobalComponentGraphVals* globalVals) {
#ifdef GODEC_TIMEBOMB
    if (prefix == TOPLEVEL_ID) {
        int64_t time_in_days_left = 6*30-(int64_t)((double)(std::time(0) -GODEC_TIMEBOMB)/(24.0 * 60.0 * 60));

        if (time_in_days_left < 0) {
            GODEC_ERR << "Evaluation period has expired." << std::endl;
        } else {
            GODEC_INFO << "You have " << time_in_days_left << " days left to evaluate this software." << std::endl;
        }
    }
#endif
    mId = prefix;

    if (!globalVals->get<bool>(LoopProcessor::QuietGodec)) GODEC_INFO << GetIndentationString(prefix) << "Submodule " << prefix << " (" << inFile << ")" << std::endl;
    ComponentGraphConfig config(prefix, inFile, globalVals, this);
    config.AddSubtree(injectedEndpoints);
    config.AddSubtree(overrideTree->GetPtree());
    if (!config.globalVals.get<bool>(LoopProcessor::QuietGodec)) {
        GODEC_INFO << config.AsString() << std::endl;
    }

    std::string cwd = CdToFilePath(inFile);

    // Instantiate components
    unordered_set<std::string> seenComponents;

    for(auto v = config.GetPtree().begin(); v != config.GetPtree().end(); v++) {
        if (v.key() == "global_opts") {
            ComponentGraphConfig* subConfig = new ComponentGraphConfig("globals", v.value(), &config.globalVals, this);
            subConfig->AddSubtree(overrideTree->GetSubtree(v.key(), true)->GetPtree());
            config.globalVals.loadGlobals(*subConfig);
        }
    }

    for(auto v = config.GetPtree().begin(); v != config.GetPtree().end(); v++) {
        LoopProcessor* lp = NULL;
        if (v.key().substr(0, 1) == "#") continue;
        if (overrideTree->get_optional_READ_DECLARATION_BEFORE_USE<std::string>(v.key()) && (overrideTree->get_json_child(v.key()).size() == 0)) {
            std::string val = overrideTree->get<std::string>(v.key(),"");
            if (val[0] == '#') continue;
        }
        if (v.key() == "global_opts") continue;
        std::string componentName = (prefix != "") ? prefix + ComponentGraph::TREE_LEVEL_SEPARATOR + v.key() : v.key();
        std::string componentType;

        ComponentGraphConfig* subConfig = new ComponentGraphConfig(componentName, v.value(), &config.globalVals, this);
        subConfig->globalVals.globalChannelPointerList = &mGlobalOutputSlots;

        componentType = v.value()["type"];
        if (mComponents.find(componentName) != mComponents.end()) {
            GODEC_ERR << "Multiple components with the name " << componentName << " found." << std::endl;
        }

        seenComponents.insert(componentName);

        if (!globalVals->get<bool>(LoopProcessor::QuietGodec)) GODEC_INFO << GetIndentationString(prefix) << "  +" << v.key() << " (" << componentType << ")" << std::endl << std::flush;
        overrideTree->GetSubtree(v.key(), true) ;
        subConfig->AddSubtree(overrideTree->GetSubtree(v.key(), true)->GetPtree());
        lp = LoadComponent(componentType, componentName, subConfig);

        subConfig->ParameterCheck();
        mComponents[componentName] = boost::shared_ptr<LoopProcessor>(lp);
    }

    for(auto it = mComponents.begin(); it != mComponents.end(); it++) {
        it->second->connectInputs(it->second->mInputSlots);
    }


    for(auto it = mComponents.begin(); it != mComponents.end(); it++) {
        it->second->Start();
    }

    boost::filesystem::current_path(cwd);
}

std::string ComponentGraph::GetIndentationString(std::string id) {

    int numIndent = 0;
    size_t pos = -1;
    while ((pos = id.find(ComponentGraph::TREE_LEVEL_SEPARATOR, pos + 1)) != std::string::npos) {
        numIndent++;
    }

    std::string indentString = "";
    for (int idx = 0; idx < numIndent; idx++) indentString += "  ";
    return indentString;
}

LoopProcessor* ComponentGraph::LoadComponent(std::string compType, std::string compName, ComponentGraphConfig* compConfig) {
    std::vector<std::string> els;
    boost::split(els, compType,boost::is_any_of(":"));
    std::string dllName = "libgodec_";
    std::string componentName = "";
    if (els.size() == 1) {
        dllName += "core";
        componentName = els[0];
    } else {
        dllName += els[0];
        componentName = els[1];
    }
    DllPtr dllHandle;
    if (mGlobalDllName2Handle == nullptr) mGlobalDllName2Handle = boost::shared_ptr< unordered_map<std::string, DllPtr > >(new unordered_map<std::string, DllPtr >());
    if (mGlobalDllName2Handle->find(dllName) != mGlobalDllName2Handle->end()) {
        dllHandle = (*mGlobalDllName2Handle)[dllName];
    } else {
        dllHandle = LoadGodecLibrary(dllName);
        (*mGlobalDllName2Handle)[dllName] = dllHandle;
    }
#ifdef _MSC_VER
    GodecGetComponentFunc loadFunc = (GodecGetComponentFunc)GetProcAddress(dllHandle,"GodecGetComponent");
#else
    GodecGetComponentFunc loadFunc = (GodecGetComponentFunc)dlsym(dllHandle, "GodecGetComponent");
#endif
    if (loadFunc == NULL) GODEC_ERR << "Could not get GodecGetComponent() function from "+dllName;
    LoopProcessor* newComp = loadFunc(componentName, compName, compConfig);
    if (newComp == NULL) GODEC_ERR << dllName+" returned NULL when asking for " << els[1];
    return newComp;
}

void ComponentGraph::ListComponents(std::string dllName) {
    DllPtr dllHandle = LoadGodecLibrary("libgodec_"+dllName);
#ifdef _MSC_VER
    GodecListComponentsFunc listFunc = (GodecListComponentsFunc)GetProcAddress(dllHandle,"GodecListComponents");
#else
    GodecListComponentsFunc listFunc = (GodecListComponentsFunc)dlsym(dllHandle, "GodecListComponents");
#endif
    if (listFunc == NULL) GODEC_ERR << "Could not get list function from DLL";
    listFunc();
}

DllPtr ComponentGraph::LoadGodecLibrary(std::string dllName) {
    DllPtr dllHandle;
#ifdef _MSC_VER
    dllName += ".dll";
    dllHandle = LoadLibraryA(dllName.c_str());
    if (dllHandle == NULL) GODEC_ERR << "Couldn't load library " << dllName;
    GodecVersionFunc versionFunc = (GodecVersionFunc)GetProcAddress(dllHandle,"GodecVersion");
#else
    dllName += ".so";
    dllHandle = dlopen (dllName.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (dllHandle == NULL) GODEC_ERR << "Couldn't load library " << dllName << ", reason: " << dlerror();
    GodecVersionFunc versionFunc = (GodecVersionFunc)dlsym(dllHandle, "GodecVersion");
#endif
    if (versionFunc == NULL) GODEC_ERR << "Could not get the GodecVersion() function from DLL "+dllName;
    auto dllVersion = versionFunc();
    if (dllVersion != GODEC_VERSION_STRING) GODEC_ERR << "Shared library " << dllName << " was compiled with Godec version " << versionFunc() << ", whereas this version is " << GODEC_VERSION_STRING << ". Versions need to match.";
    return dllHandle;
}

unordered_map<std::string, boost::shared_ptr<RuntimeStats> > ComponentGraph::getRuntimeStats() {
    unordered_map<std::string, boost::shared_ptr<RuntimeStats> > outStats;
    for (auto compIt = mComponents.begin(); compIt != mComponents.end(); compIt++) {
        if (compIt->first.find(API_ENDPOINT_SUFFIX) != std::string::npos) continue;
        auto stats = compIt->second->getRuntimeStats();
        for(auto statsIt = stats.begin(); statsIt != stats.end(); statsIt++) {
            outStats[statsIt->first] = statsIt->second;
        }
    }
    return outStats;
}

ComponentGraph::~ComponentGraph() {
    std::lock_guard<std::mutex> lock(mComponentsMutex);
    std::stringstream ss;
    if (mId == TOPLEVEL_ID && !mComponents.empty() && mComponents.begin()->second->getRuntimeStats().size() != 0) {
        float highestTotalTime = -FLT_MAX;
        std::vector<std::pair<std::string, RuntimeStats>> pairs;
        int longestName = 0;
        auto allStats = getRuntimeStats();
        for (auto compIt = allStats.begin(); compIt != allStats.end(); compIt++) {
            auto statsPtr = *(compIt->second);
            float totalWaitedTime = 0.0f;
            for (auto it = statsPtr.mWaitedOn.begin(); it != statsPtr.mWaitedOn.end(); it++) totalWaitedTime += it->second;
            auto newPair = std::make_pair(compIt->first,statsPtr);
            pairs.push_back(newPair);
            longestName = std::max(longestName, (int)compIt->first.length());
            highestTotalTime = std::max(highestTotalTime, totalWaitedTime);
        }

        if (longestName != 0) {
            sort(pairs.begin(), pairs.end(), [=](const std::pair<std::string, RuntimeStats>& a, const std::pair<std::string, RuntimeStats>& b) {
                auto aIt = a.second.mWaitedOn.find("myself");
                float aVal = (aIt == a.second.mWaitedOn.end()) ? 0.0f : a.second.mTotalNumTicks / aIt->second;
                auto bIt = b.second.mWaitedOn.find("myself");
                float bVal = (bIt == b.second.mWaitedOn.end()) ? 0.0f : b.second.mTotalNumTicks / bIt->second;
                return aVal > bVal;
            });
        }

        for (auto compIt = pairs.begin(); compIt != pairs.end(); compIt++) {
            auto statsPtr = compIt->second;
            float thisCompTotalTime = 0.0f;
            for (auto it = statsPtr.mWaitedOn.begin(); it != statsPtr.mWaitedOn.end(); it++) thisCompTotalTime += it->second;
            if (statsPtr.mTotalNumTicks != 0 && statsPtr.mWaitedOn["myself"] != 0.0f) {
                ss << std::left << std::setw(longestName) << compIt->first << ": ";
                ss << (statsPtr.mTotalNumTicks / statsPtr.mWaitedOn["myself"]) << std::endl;
            }
        }
        if (ss.str() != "") GODEC_INFO << "################## " << mId << ": Component throughput in ticks/second ###########" << std::endl << ss.str() << "###########################" << std::endl;
    }

    mComponents.clear(); // This should call the respective destructors (which might lie across the DLL boundary)
    if (mId == TOPLEVEL_ID) {
        // It looks weird to transfer over the handles over to a loval vector. Problem is, when we unload the libraries, it destroys the unordered_map because the libraries are aware of it
        std::vector<DllPtr> handles2Delete;
        for (auto dllNameIt = mGlobalDllName2Handle->begin(); dllNameIt != mGlobalDllName2Handle->end(); dllNameIt++) {
            handles2Delete.push_back(dllNameIt->second);
        }
        mGlobalDllName2Handle->clear();
        for(auto it = handles2Delete.begin(); it != handles2Delete.end(); it++) {
#ifdef _MSC_VER
            FreeLibrary(*it);
#else
            dlclose(*it);
#endif
        }
    }
}

void ComponentGraph::PrintHelp() {
    GODEC_ERR << "Not reimplemented again yet";
}

json ComponentGraph::CreateApiEndpoint(bool verbose, std::vector<std::string> inputs, std::string output) {
    std::stringstream ss;
    ss << "{" << std::endl <<
       "\"type\": \"ApiEndpoint\"," << std::endl <<
       "\"verbose\": \"" << (verbose ? "true" : "false") << "\"," << std::endl;
    if (output != "") {
        ss <<
           "\"outputs\": " << std::endl << "{" << std::endl <<
           "\"output\": \"" << output << "\"" << std::endl <<
           "}" << std::endl;
    }
    if (inputs.size() != 0) {
        if (output != "") ss << "," << std::endl;
        ss << "\"inputs\": " << std::endl << "{" << std::endl;
        for (int inputIdx = 0; inputIdx < inputs.size(); inputIdx++) {
            std::string input = inputs[inputIdx];
            ss << "\"" << input << "\": \"" << input << "\"" << std::endl;
            if (inputIdx < inputs.size() - 1) ss << "," << std::endl;
        }
        ss << "}" << std::endl;
    }
    ss << "}";

    std::stringstream cleanJson;
    cleanJson << StripCommentsFromJSON(ss.str());
    json pt = json::parse(cleanJson);
    return pt;
}

boost::shared_ptr<ApiEndpoint> ComponentGraph::GetApiEndpoint(std::string compName) {
    std::lock_guard<std::mutex> lock(mComponentsMutex);
    auto it = mComponents.find(compName + API_ENDPOINT_SUFFIX);
    if (it == mComponents.end()) GODEC_ERR << "No such component " << compName;
    return boost::static_pointer_cast<ApiEndpoint,LoopProcessor>(it->second);
}

void ComponentGraph::DeleteApiEndpoint(std::string endpointName) {
    std::lock_guard<std::mutex> lock(mComponentsMutex);
    auto it = mComponents.find(endpointName + API_ENDPOINT_SUFFIX);
    if (it == mComponents.end()) GODEC_ERR << mId << "No such endpoint " << endpointName;
    mComponents.erase(it);
}

void ComponentGraph::WaitTilShutdown() {
    bool allShutdown = true;
    do {
        allShutdown = true;
        boost::this_thread::sleep(boost::posix_time::seconds(1));
        {
            std::lock_guard<std::mutex> lock(mComponentsMutex);
            for (auto it = mComponents.begin(); it != mComponents.end(); it++) {
                if (!it->second->mIsFinished) {
                    allShutdown = false;
                }
            }
        }
    } while (!allShutdown);
}

void ComponentGraph::PushMessage(std::string channelName, DecoderMessage_ptr msg) {
    auto ep = GetApiEndpoint(channelName);
    ep->pushToOutputs(ep->getOutputSlot(), msg);
}

ChannelReturnResult ComponentGraph::PullMessage(std::string channelName, float maxTimeout, unordered_map<std::string, DecoderMessage_ptr>& newSlice) {
    auto endpoint = GetApiEndpoint(channelName);
    return endpoint->PullMessage(newSlice, maxTimeout);
}

ChannelReturnResult ComponentGraph::PullAllMessages(std::string channelName, float maxTimeout, std::vector<unordered_map<std::string, DecoderMessage_ptr>>& newSlice) {
    auto endpoint = GetApiEndpoint(channelName);
    return endpoint->PullAllMessages(newSlice, maxTimeout);
}

DecoderMessage_ptr ComponentGraph::JNIToDecoderMsg(JNIEnv *env, jobject jMsg) {
    for(auto it = mGlobalDllName2Handle->begin(); it != mGlobalDllName2Handle->end(); it++) {
        std::string dllName = it->first;
        DllPtr dllHandle = it->second;
#ifdef _MSC_VER
        GodecJNIToMsgFunc jniToMsgFunc = (GodecJNIToMsgFunc)GetProcAddress(dllHandle,"GodecJNIToMsg");
#else
        GodecJNIToMsgFunc jniToMsgFunc = (GodecJNIToMsgFunc)dlsym(dllHandle, "GodecJNIToMsg");
#endif
        if (jniToMsgFunc == NULL) GODEC_ERR << "Could not get GodecJNIToMsg function from DLL " << dllName;
        DecoderMessage_ptr newMsg = jniToMsgFunc(env, jMsg);
        if (newMsg != NULL) return newMsg;
    }
    GODEC_ERR << "No library could convert Java message to C++";
    return nullptr;
}

DecoderMessage_ptr ComponentGraph::CSharpToDecoderMsg(_DECODERMESSAGESTRUCT cMsg) {
    for (auto it = mGlobalDllName2Handle->begin(); it != mGlobalDllName2Handle->end(); it++) {
        std::string dllName = it->first;
        DllPtr dllHandle = it->second;
#ifdef _MSC_VER
        GodecCsharpToMsgFunc cSharpToMsgFunc = (GodecCsharpToMsgFunc)GetProcAddress(dllHandle, "GodecCsharpToMsg");
#else
        GodecCsharpToMsgFunc cSharpToMsgFunc = (GodecCsharpToMsgFunc)dlsym(dllHandle, "GodecCsharpToMsg");
#endif
        if (cSharpToMsgFunc == NULL) GODEC_ERR << "Could not get GodecCsharpToMsg function from DLL " << dllName;
        DecoderMessage_ptr newMsg = cSharpToMsgFunc(cMsg);
        if (newMsg != NULL) return newMsg;
    }
    GODEC_ERR << "No library could convert Java message to C++";
    return nullptr;
}

#ifndef ANDROID
DecoderMessage_ptr ComponentGraph::PythonToDecoderMsg(PyObject* pMsg) {
    for(auto it = mGlobalDllName2Handle->begin(); it != mGlobalDllName2Handle->end(); it++) {
        std::string dllName = it->first;
        DllPtr dllHandle = it->second;
#ifdef _MSC_VER
        GodecPythonToMsgFunc pythonToMsgFunc = (GodecPythonToMsgFunc)GetProcAddress(dllHandle,"GodecPythonToMsg");
#else
        GodecPythonToMsgFunc pythonToMsgFunc = (GodecPythonToMsgFunc)dlsym(dllHandle, "GodecPythonToMsg");
#endif
        if (pythonToMsgFunc == NULL) GODEC_ERR << "Could not get GodecPythonToMsg function from DLL " << dllName;
        DecoderMessage_ptr newMsg = pythonToMsgFunc(pMsg);
        if (newMsg != NULL) return newMsg;
    }
    GODEC_ERR << "No library could convert Python message to C++";
    return nullptr;
}
#endif
}
