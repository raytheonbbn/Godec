#include <godec/ComponentGraph.h>
#include "core_components/ApiEndpoint.h"
#include <jni.h>
#include <thread>
#include <malloc.h>
#ifdef ANDROID
#include <pthread.h>
#include <android/log.h>
#endif

#ifdef _MSC_VER
#include <windows.h>
#endif

#include <boost/program_options.hpp>
#include <boost/assign/list_of.hpp>
namespace po = boost::program_options;
using namespace Godec;

/*
 * Explanation of usage of _exit() and mallopt below:
 *
 * Godec loads shared libraries at startup, and these libraries eventually share some global variables (e.g. STL memory allocations etc)
 * During shutdown, we unload all these shared libraries, but what happens is that those global vars are being double-freed, resulting in an error. Using _exit (for command line runs) and mallopt (for Java calls) ignores this behavior during deallocation
 *
 */

#ifdef _MSC_VER
// Is this actually necessary? Not sure, but it can't hurt
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved
                     ) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
#endif

void PrintUsage() {
    std::cout <<
              "Godec, the stream processing engine\n\n"
              "Usage:\n"
              "  godec [overrides] <json>    | Overrides are specified with '-x \"a.b=c\"', where a is top-level component, b its child parameter.\n"
              "  godec list <core|libname>   | List available components in library. Library is looked up as libgodec_<libname>.so\n";
}

#ifndef ANDROID
JNIEnv *mJNIEnv = nullptr;
JavaVM *mJvm = nullptr;
#endif
int main(int argc, char** argv) {

    // Parse command line arguments
    po::options_description desc("Options");
    typedef std::vector<std::string> OverrideValues;
    OverrideValues ovOpts;
    desc.add_options()
    ("q", "Do not print entire JSON on screen")
    ("x", po::value<OverrideValues>(&ovOpts)->default_value(boost::assign::list_of(""), "")->composing(), "overrides")
    ("pos_opts", po::value<OverrideValues>(&ovOpts)->default_value(boost::assign::list_of(""), "")->composing(), "")
    ("java_class_path", po::value<std::string>(), "Java class path")
    ;

    po::variables_map vm;
    std::string jsonOrCommand;
    std::vector<std::string> posOpts;
    try {
        po::positional_options_description p;
        p.add("pos_opts", 2);
        po::store(
            po::command_line_parser( argc, argv).
            options( desc ).
            style( po::command_line_style::unix_style | po::command_line_style::allow_long_disguise).
            positional( p ).
            run(),
            vm
        );
        po::notify( vm );
        posOpts = vm["pos_opts"].as<std::vector<std::string>>();
        if (posOpts[0] == "" ) throw std::exception();
        jsonOrCommand = posOpts[0];
    } catch ( ... ) {
        PrintUsage();
        exit(-1);
    }

    if (jsonOrCommand == "list") {
        if (posOpts.size() != 2) { PrintUsage(); exit(-1);}
        ComponentGraph::ListComponents(posOpts[1]);
        _exit(0); // See beginning of file for explanation
    }

    // Transform override options into Godec required array
    std::vector<std::pair<std::string, std::string>> ov;
    for(auto it = ovOpts.begin(); it != ovOpts.end(); it++) {
        std::string ors = *it;
        if (ors == "") continue;
        int eqSignIdx = (int)ors.find_first_of("=");
        std::string key = ors.substr(0, eqSignIdx);
        std::string val = ors.substr(eqSignIdx + 1);
        ov.push_back(std::make_pair(key, val));
    }

    // Set up Java env variables when using the 'Java' component (not necessary for Android where this is already set up)
#ifndef ANDROID
    if (vm.count("java_class_path")) {
        JavaVMInitArgs vm_args;
        JavaVMOption* options = new JavaVMOption[1];
        options[0].optionString = (char*)("Djava.class.path="+vm["java_class_path"].as<std::string>()).c_str();
        vm_args.version = JNI_VERSION_1_6;
        vm_args.nOptions = 1;
        vm_args.options = options;
        vm_args.ignoreUnrecognized = false;
        JNI_CreateJavaVM(&mJvm, (void**)&mJNIEnv, &vm_args);
        delete options;
    }
#endif

    // All set up, instantiate Godec and call for blocking shutdown
    GlobalComponentGraphVals globals;
    globals.put<bool>(LoopProcessor::QuietGodec,vm.count("q") == 1);
    ComponentGraphConfig* overrides = ComponentGraphConfig::FromOverrideList(ov);
    json endpoints;
    ComponentGraph* cGraph = new ComponentGraph(ComponentGraph::TOPLEVEL_ID, jsonOrCommand, overrides, endpoints, &globals);
    cGraph->WaitTilShutdown();
    delete cGraph;
    _exit(0); // See beginning of file for explanation
    return 0;
}

// The following is for redirecting the stdout and stderr to the Android logging facility
#ifdef ANDROID
static int pfd[2];
static pthread_t thr;
static bool log_redirect_started = false;

static void *thread_func(void*) {
    ssize_t rdsz;
    char buf[128];
    while((rdsz = read(pfd[0], buf, sizeof buf - 1)) > 0) {
        if(buf[rdsz - 1] == '\n') --rdsz;
        buf[rdsz] = 0;  /* add null-terminator */
        __android_log_write(ANDROID_LOG_DEBUG, "GODEC", buf);
    }
    return 0;
}

int android_log_redirect() {
    if (log_redirect_started) return 0;
    __android_log_write(ANDROID_LOG_DEBUG, "GODEC", "Redirecting stdout/err to android log");
    /* make stdout line-buffered and stderr unbuffered */
    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    /* create the pipe and redirect stdout and stderr */
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);

    /* spawn the logging thread */
    if(pthread_create(&thr, 0, thread_func, 0) == -1)
        return -1;
    pthread_detach(thr);
    log_redirect_started = true;
    return 0;
}
#endif

// ################################# Java bindings ##############################

boost::shared_ptr<ComponentGraph> globalGodecInstance;
std::vector<std::string> globalGodecInjectedEndpoints;
extern "C" {
    // Instantiate Godec
    JNIEXPORT void Java_com_bbn_godec_Godec_JLoadGodec( JNIEnv* env, jobject thiz, jobject jsonFile, jobject jOvs, jobject jPushEndpoints, jobject jPullEndpoints, jboolean quiet) {
#ifdef ANDROID
        android_log_redirect();
#endif
        if (globalGodecInstance != nullptr) GODEC_ERR << "Already an instance loaded";

        /* Turn the JSON File into a string of the absolute path. */
        jclass File_class = env->FindClass( "java/io/File" );
        jmethodID abspath_method = env->GetMethodID( File_class, "getAbsolutePath", "()Ljava/lang/String;");
        jstring jsonPath = (jstring)env->CallObjectMethod(jsonFile, abspath_method);
        const char *jsonPathNativeString = env->GetStringUTFChars(jsonPath, 0);

        jclass Overrides_class = env->FindClass("com/bbn/godec/GodecJsonOverrides");
        jmethodID Overrides_size_method = env->GetMethodID(Overrides_class, "size", "()I");
        jmethodID Overrides_get_method = env->GetMethodID(Overrides_class, "get", "(I)Ljava/lang/Object;");

        jclass OverrideElement_class = env->FindClass("com/bbn/godec/GodecJsonOverrideElement");
        jmethodID OverrideElement_getKey_method = env->GetMethodID(OverrideElement_class, "getKey", "()Ljava/lang/String;");
        jmethodID OverrideElement_getVal_method = env->GetMethodID(OverrideElement_class, "getVal", "()Ljava/lang/String;");

        std::vector<std::pair<std::string, std::string>> ov;
        int numOverrides = env->CallIntMethod(jOvs, Overrides_size_method);
        for(int i = 0 ; i < numOverrides ; i++) {
            jobject overrideElementObject = env->CallObjectMethod(jOvs, Overrides_get_method, i);
            jstring key = (jstring)env->CallObjectMethod(overrideElementObject, OverrideElement_getKey_method);
            jstring value = (jstring)env->CallObjectMethod(overrideElementObject, OverrideElement_getVal_method);

            const char* keyChars = env->GetStringUTFChars(key,0);
            const char* valueChars = env->GetStringUTFChars(value,0);

            ov.push_back(std::make_pair(keyChars, valueChars));

            env->ReleaseStringUTFChars(key, keyChars);
            env->ReleaseStringUTFChars(value, valueChars);
        }

        // Push endpoints
        json endpoints;
        {
            jclass GodecPushEndpoints_Class = env->FindClass("com/bbn/godec/GodecPushEndpoints");
            jmethodID GodecPushEndpoints_getEndpoints_method = env->GetMethodID(GodecPushEndpoints_Class, "getEndpoints", "()[Ljava/lang/String;");
            jobject pushEndpointsObject = env->CallObjectMethod(jPushEndpoints, GodecPushEndpoints_getEndpoints_method);
            jobjectArray* pushEndpointsObjectArray = reinterpret_cast<jobjectArray*>(&pushEndpointsObject);
            int numPushEndpoints = env->GetArrayLength(*pushEndpointsObjectArray);
            for (int endpointIdx = 0; endpointIdx < numPushEndpoints; endpointIdx++) {
                jstring endpointName = (jstring)env->GetObjectArrayElement(*pushEndpointsObjectArray, endpointIdx);
                const char* endpointNameChar = env->GetStringUTFChars(endpointName, 0);
                globalGodecInjectedEndpoints.push_back(ComponentGraph::TOPLEVEL_ID+ComponentGraph::TREE_LEVEL_SEPARATOR + endpointNameChar);
                std::vector<std::string> inputs;
                endpoints["!"+std::string(endpointNameChar)+ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(false, inputs, endpointNameChar);
                env->ReleaseStringUTFChars(endpointName, endpointNameChar);
            }
        }

        // Pull endpoints
        {
            jclass GodecPullEndpoints_Class = env->FindClass("com/bbn/godec/GodecPullEndpoints");
            jmethodID GodecPullEndpoints_getEndpoints_method = env->GetMethodID(GodecPullEndpoints_Class, "getEndpoints", "()[Ljava/lang/String;");
            jmethodID GodecPullEndpoints_getStreams_method = env->GetMethodID(GodecPullEndpoints_Class, "getStreamsForEndpoint", "(Ljava/lang/String;)[Ljava/lang/String;");
            jobject pullEndpointsObject = env->CallObjectMethod(jPullEndpoints, GodecPullEndpoints_getEndpoints_method);
            jobjectArray* pullEndpointsObjectArray = reinterpret_cast<jobjectArray*>(&pullEndpointsObject);
            int numPullEndpoints = env->GetArrayLength(*pullEndpointsObjectArray);
            for (int endpointIdx = 0; endpointIdx < numPullEndpoints; endpointIdx++) {
                jstring endpointName = (jstring)env->GetObjectArrayElement(*pullEndpointsObjectArray, endpointIdx);
                const char* endpointNameChar = env->GetStringUTFChars(endpointName, 0);
                jobject streamsObject = env->CallObjectMethod(jPullEndpoints, GodecPullEndpoints_getStreams_method, endpointName);
                jobjectArray* streamsObjectArray = reinterpret_cast<jobjectArray*>(&streamsObject);
                int numStreams = env->GetArrayLength(*streamsObjectArray);

                std::vector<std::string> inputs;
                for (int streamIdx = 0; streamIdx < numStreams; streamIdx++) {
                    jstring streamName = (jstring)env->GetObjectArrayElement(*streamsObjectArray, streamIdx);
                    const char* streamNameChar = env->GetStringUTFChars(streamName, 0);
                    inputs.push_back(streamNameChar);
                    env->ReleaseStringUTFChars(streamName, streamNameChar);
                }
                endpoints["!"+std::string(endpointNameChar)+ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(false, inputs, "");
                env->ReleaseStringUTFChars(endpointName, endpointNameChar);
            }
        }

        // Instantiate Godec
        GlobalComponentGraphVals globals;
        globals.put<bool>(LoopProcessor::QuietGodec, quiet);
        globalGodecInstance = boost::shared_ptr<ComponentGraph>(new ComponentGraph(ComponentGraph::TOPLEVEL_ID, jsonPathNativeString, ComponentGraphConfig::FromOverrideList(ov), endpoints, &globals));
        env->ReleaseStringUTFChars(jsonPath, jsonPathNativeString);
    }

    // Push message
    JNIEXPORT void Java_com_bbn_godec_Godec_JPushMessage( JNIEnv* env, jobject thiz, jstring jEndpointName, jobject jMsg) {
        DecoderMessage_ptr msg = globalGodecInstance->JNIToDecoderMsg(env, jMsg);
        const char* endpointName = env->GetStringUTFChars(jEndpointName,0);
        globalGodecInstance->PushMessage(ComponentGraph::TOPLEVEL_ID+ComponentGraph::TREE_LEVEL_SEPARATOR + std::string(endpointName), msg);
        env->ReleaseStringUTFChars(jEndpointName, endpointName);
    }

    jint throwChannelClosedException(JNIEnv *env, const char *channel_name) {
        std::string message("Channel");
        message += std::string(channel_name);
        message += " is closed.";
        jclass exClass = env->FindClass("com/bbn/godec/ChannelClosedException");
        return env->ThrowNew(exClass, message.c_str());
    }

    // Pull message
    JNIEXPORT jobject Java_com_bbn_godec_Godec_JPullMessage( JNIEnv* env, jobject thiz, jstring jEndpointName, jfloat jMaxTimeout) {
        const char* endpointName = env->GetStringUTFChars(jEndpointName,0);
        unordered_map<std::string, DecoderMessage_ptr> map;
        ChannelReturnResult res = globalGodecInstance->PullMessage(ComponentGraph::TOPLEVEL_ID+ComponentGraph::TREE_LEVEL_SEPARATOR+std::string(endpointName), jMaxTimeout, map);
        env->ReleaseStringUTFChars(jEndpointName, endpointName);
        if (res == ChannelClosed) {
            throwChannelClosedException(env, endpointName);
            return NULL;
        }
        if (res == ChannelTimeout) {
            return NULL;
        }
        jclass HashMapClass = env->FindClass("java/util/HashMap");
        jmethodID jHashMapInit = env->GetMethodID(HashMapClass, "<init>", "()V");
        jobject jHashMapObject = env->NewObject(HashMapClass, jHashMapInit);
        jmethodID jPutMethod = env->GetMethodID(HashMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        for (auto mapIt = map.begin(); mapIt != map.end(); mapIt++) {
            std::string slot = mapIt->first;
            auto msg = boost::const_pointer_cast<DecoderMessage>(mapIt->second);
            env->CallObjectMethod(jHashMapObject, jPutMethod, env->NewStringUTF(slot.c_str()), msg->toJNI(env));
        }
        return jHashMapObject;
    }

    // Pull all messages
    JNIEXPORT jobject Java_com_bbn_godec_Godec_JPullAllMessages( JNIEnv* env, jobject thiz, jstring jEndpointName, jfloat jMaxTimeout) {
        const char* endpointName = env->GetStringUTFChars(jEndpointName,0);
        std::vector<unordered_map<std::string, DecoderMessage_ptr>> mapList;
        ChannelReturnResult res = globalGodecInstance->PullAllMessages(ComponentGraph::TOPLEVEL_ID+ComponentGraph::TREE_LEVEL_SEPARATOR+std::string(endpointName), jMaxTimeout, mapList);
        env->ReleaseStringUTFChars(jEndpointName, endpointName);
        if (res == ChannelClosed) {
            throwChannelClosedException(env, endpointName);
            return NULL;
        }
        if (res == ChannelTimeout) {
            return NULL;
        }
        jclass ArrayListClass = env->FindClass("java/util/ArrayList" );
        jmethodID jArrayListInit = env->GetMethodID(ArrayListClass, "<init>", "()V");
        jmethodID jArrayListAdd = env->GetMethodID(ArrayListClass, "add", "(Ljava/lang/Object;)Z");
        jobject jArrayListObject = env->NewObject(ArrayListClass, jArrayListInit);

        jclass HashMapClass = env->FindClass("java/util/HashMap");
        jmethodID jHashMapInit = env->GetMethodID(HashMapClass, "<init>", "()V");
        jmethodID jHashMapPut = env->GetMethodID(HashMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

        for (auto it = mapList.begin(); it != mapList.end(); it++) {
            auto& map = *it;
            jobject jHashMapObject = env->NewObject(HashMapClass, jHashMapInit);
            for (auto mapIt = map.begin(); mapIt != map.end(); mapIt++) {
                std::string slot = mapIt->first;
                env->PushLocalFrame(2);
                auto msg = boost::const_pointer_cast<DecoderMessage>(mapIt->second);
                env->CallObjectMethod(jHashMapObject, jHashMapPut, env->NewStringUTF(slot.c_str()), msg->toJNI(env));
                env->PopLocalFrame(NULL);
            }
            env->CallBooleanMethod(jArrayListObject, jArrayListAdd, jHashMapObject);
        }
        return jArrayListObject;
    }

    // Shutdown
    JNIEXPORT void Java_com_bbn_godec_Godec_JBlockingShutdown( JNIEnv* env, jobject thiz) {
        if (globalGodecInstance == nullptr) GODEC_ERR << "No instance loaded";
        for(auto it = globalGodecInjectedEndpoints.begin(); it != globalGodecInjectedEndpoints.end(); it++) {
            auto endpoint = globalGodecInstance->GetApiEndpoint(*it);
            endpoint->getInputChannel().checkOut("Java");
        }
        globalGodecInstance->WaitTilShutdown();
#ifndef ANDROID
#ifndef _MSC_VER
        mallopt(M_CHECK_ACTION, 5); // See beginning of file for explanation
#endif
#endif
        globalGodecInstance = nullptr;
        globalGodecInjectedEndpoints.clear();
    }



}
// ################################# C# bindings ##############################

extern "C" {
    __declspec(dllexport) void LoadGodec(char* jsonFilePath) {
        //Get the json location here
        const char *jsonPathNativeString = jsonFilePath;

        // Transform override options into Godec required array
        std::vector<std::pair<std::string, std::string>> ov;
        // Push endpoints
        json endpoints;
        {
            const char* endpointNameChar = "raw_audio_0";
            const char* endpointNameCharConvstate = "convstate_input_0"; 
            std::vector<std::string> inputs;
            endpoints["!" + std::string(endpointNameChar) + ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(false, inputs, endpointNameChar);
            endpoints["!" + std::string(endpointNameCharConvstate) + ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(false, inputs, endpointNameCharConvstate);
        }

        // Pull endpoints
        {
            const char* endpointNameCharCtm = "pull_endpoint_0_0";
            const char* endpointNameCharKws = "pull_endpoint_0_1";
            std::vector<std::string> inputs;
            const char* streamNameCharCtm = "stream0_endpoint0_ctm";
            const char* streamNameCharKws = "stream0_endpoint0_kws";
            inputs.push_back(streamNameCharCtm);
            inputs.push_back(streamNameCharKws);
            endpoints["!" + std::string(endpointNameCharCtm) + ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(false, inputs, "");
            endpoints["!" + std::string(endpointNameCharKws) + ComponentGraph::API_ENDPOINT_SUFFIX] = ComponentGraph::CreateApiEndpoint(false, inputs, "");
        }
        GlobalComponentGraphVals globals;
        globals.put<bool>(LoopProcessor::QuietGodec, true);
        globalGodecInstance = boost::shared_ptr<ComponentGraph>(new ComponentGraph(ComponentGraph::TOPLEVEL_ID, jsonPathNativeString, ComponentGraphConfig::FromOverrideList(ov), endpoints, &globals));
    }
}