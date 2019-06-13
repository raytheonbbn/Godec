#include "godec/version.h"
#include "Energy.h"
#include "NoiseAdd.h"
#include "FeatureMerger.h"
#include "FeatureNormalizer.h"
#include "MatrixApply.h"
#include "Router.h"
#include "Merger.h"
#include "Subsample.h"
#include "Average.h"
#include "SoundcardRecorder.h"
#include "SoundcardPlayback.h"
#include "Java.h"
#include "PythonComponent.h"
#include "ApiEndpoint.h"
#include "FileFeeder.h"
#include "FileWriter.h"
#include "AudioPreProcessor.h"
#include "SubModule.h"

#ifdef _MSC_VER
// Is this actually needed? Leaving it here, doesn't seem to hurt
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

/*
 * This code is the "interface" code for Godec when it loaded the shared library containing the components. Use this as a template for creating your own shared library
 */

namespace Godec {

extern "C" {
    // When Godec loaded the shared library, this function gets called to instantiate a component.
    JNIEXPORT LoopProcessor* GodecGetComponent(std::string compString, std::string id, ComponentGraphConfig* configPt) {
        if (compString == "ApiEndpoint") return ApiEndpoint::make(id,configPt);
        if (compString == "SubModule") return Submodule::make(id,configPt);
        if (compString == "AudioPreProcessor") return AudioPreProcessorComponent::make(id,configPt);
        if (compString == "FileFeeder") return FileFeederComponent::make(id,configPt);
        if (compString == "FileWriter") return FileWriterComponent::make(id,configPt);
        if (compString == "FeatureNormalizer") return FeatureNormalizerComponent::make(id,configPt);
        else if (compString == "MatrixApply") return MatrixApplyComponent::make(id, configPt);
        else if (compString == "Energy") return EnergyComponent::make(id,configPt);
        else if (compString == "NoiseAdd") return NoiseAddComponent::make(id,configPt);
        else if (compString == "FeatureMerger") return FeatureMergerComponent::make(id,configPt);
        else if (compString == "Average") return AverageComponent::make(id,configPt);
        else if (compString == "Router") return RouterComponent::make(id,configPt);
        else if (compString == "Merger") return MergerComponent::make(id,configPt);
        else if (compString == "Subsample") return SubsampleComponent::make(id,configPt);
        else if (compString == "SoundcardRecorder") return SoundcardRecorderComponent::make(id,configPt);
        else if (compString == "SoundcardPlayer") return SoundcardPlayerComponent::make(id,configPt);
        else if (compString == "Java") return JavaComponent::make(id,configPt);
        else if (compString == "Python") return PythonComponent::make(id,configPt);
        else GODEC_ERR << "Godec core library: Asked for unknown component " << compString;
        return NULL;
    }
    // Output for "godec list <library>"
    JNIEXPORT void GodecListComponents() {
        std::cout << "AudioPreProcessor: " << AudioPreProcessorComponent::describeThyself() << std::endl;
        std::cout << "FileFeeder: " << FileFeederComponent::describeThyself() << std::endl;
        std::cout << "FileWriter: " << FileWriterComponent::describeThyself() << std::endl;
        std::cout << "FeatureNormalizer: " << FeatureNormalizerComponent::describeThyself() << std::endl;
        std::cout << "MatrixApply: " << MatrixApplyComponent::describeThyself() << std::endl;
        std::cout << "Energy: " << EnergyComponent::describeThyself() << std::endl;
        std::cout << "NoiseAdd: " << NoiseAddComponent::describeThyself() << std::endl;
        std::cout << "FeatureMerger: " << FeatureMergerComponent::describeThyself() << std::endl;
        std::cout << "Average: " << AverageComponent::describeThyself() << std::endl;
        std::cout << "Router: " << RouterComponent::describeThyself() << std::endl;
        std::cout << "Merger: " << MergerComponent::describeThyself() << std::endl;
        std::cout << "Subsample: " << SubsampleComponent::describeThyself() << std::endl;
        std::cout << "SoundcardRecorder: " << SoundcardRecorderComponent::describeThyself() << std::endl;
        std::cout << "SoundcardPlayer: " << SoundcardPlayerComponent::describeThyself() << std::endl;
        std::cout << "Java: " << JavaComponent::describeThyself() << std::endl;
        std::cout << "Python: " << PythonComponent::describeThyself() << std::endl;
    }

    // For checking whether all libraries are of the same version
    JNIEXPORT std::string GodecVersion() {
        return GODEC_VERSION_STRING;
    }

    // For messages that you want to shuttle across the JNI layer. If you have no custom messages, still define it but just return nullptr
    JNIEXPORT DecoderMessage_ptr GodecJNIToMsg(JNIEnv* env, jobject jMsg) {
        jclass AudioDecoderMessageClass = env->FindClass("com/bbn/godec/AudioDecoderMessage");
        jclass ConversationStateDecoderMessageClass = env->FindClass("com/bbn/godec/ConversationStateDecoderMessage");
        jclass BinaryDecoderMessageClass = env->FindClass("com/bbn/godec/BinaryDecoderMessage");
        jclass FeaturesDecoderMessageClass = env->FindClass("com/bbn/godec/FeaturesDecoderMessage");
        jclass NbestDecoderMessageClass = env->FindClass("com/bbn/godec/NbestDecoderMessage");
        jclass JsonDecoderMessageClass = env->FindClass("com/bbn/godec/JsonDecoderMessage");

        DecoderMessage_ptr outMsg = nullptr;
        if (env->IsInstanceOf(jMsg, AudioDecoderMessageClass)) {
            outMsg = AudioDecoderMessage::fromJNI(env, jMsg);
        } else if (env->IsInstanceOf(jMsg, ConversationStateDecoderMessageClass)) {
            outMsg = ConversationStateDecoderMessage::fromJNI(env, jMsg);
        } else if (env->IsInstanceOf(jMsg, NbestDecoderMessageClass)) {
            outMsg = NbestDecoderMessage::fromJNI(env, jMsg);
        } else if (env->IsInstanceOf(jMsg, BinaryDecoderMessageClass)) {
            outMsg = BinaryDecoderMessage::fromJNI(env, jMsg);
        } else if (env->IsInstanceOf(jMsg, FeaturesDecoderMessageClass)) {
            outMsg = FeaturesDecoderMessage::fromJNI(env, jMsg);
        } else if (env->IsInstanceOf(jMsg, JsonDecoderMessageClass)) {
            outMsg = JsonDecoderMessage::fromJNI(env, jMsg);
        }

        return outMsg;
    }

#ifndef ANDROID
    // The Python version of the same. Also return nullptr if you don't have a custom message
    JNIEXPORT DecoderMessage_ptr GodecPythonToMsg(PyObject* pMsg) {
        DecoderMessage_ptr outMsg = nullptr;
        PyObject* pType = PyDict_GetItemString(pMsg,"type");
        if (pType == NULL) GODEC_ERR << "GodecPythonToMsg: Passed-in message did not have a 'type' key. Invalid message";
        std::string type = PyUnicode_AsUTF8(pType);
        if (type == "AudioDecoderMessage") {
            outMsg = AudioDecoderMessage::fromPython(pMsg);
        } else if (type == "ConversationStateDecoderMessage") {
            outMsg = ConversationStateDecoderMessage::fromPython(pMsg);
        } else if (type == "NbestDecoderMessage") {
            outMsg = NbestDecoderMessage::fromPython(pMsg);
        } else if (type == "BinaryDecoderMessage") {
            outMsg = BinaryDecoderMessage::fromPython(pMsg);
        } else if (type == "FeaturesDecoderMessage") {
            outMsg = FeaturesDecoderMessage::fromPython(pMsg);
        } else if (type == "JsonDecoderMessage") {
            outMsg = JsonDecoderMessage::fromPython(pMsg);
        }

        return outMsg;
    }
#endif

}

}
