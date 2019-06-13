#include "Java.h"
#include <jni.h>
#include "godec/ComponentGraph.h"

namespace Godec {

LoopProcessor* JavaComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new JavaComponent(id, configPt);
}
std::string JavaComponent::describeThyself() {
    return "Loads an arbitrary JAR, allocates a class object and calls ProcessMessage() on it.";
}

/* JavaComponent::ExtendedDescription
This component can load any Java code inside a JAR file. It expects a class to be defined ("class_name") and will instantiate the class constructor with a String that contains the "class_constructor_param" parameter (set this to whatever you need to initialize your instance in a specific manner). The parameters "expected_inputs" and "expected_outputs" configure the input and output streams of this component, and in turn also what gets passed into the the Java ProcessMessage() function.

The signature of ProcessMessage is 

`HashMap<String,DecoderMessage> ProcessMessage(HashMap<String, DecoderMessage>)`

The input HashMap's keys correspond to the input streams, the output is expected to contain the necessary output streams of the component.

See `java/com/bbn/godec/regression/TextTransform.java` and `test/jni_test.json` for an example.
*/

JavaComponent::JavaComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    mClassName = configPt->get<std::string>("class_name", "Full-qualified class name, e.g. java/util/ArrayList");
    mClassParam = configPt->get<std::string>("class_constructor_param", "string parameter passed into constructor");
    std::string expectedInputs = configPt->get<std::string>("expected_inputs", "comma-separated list of expected input slots");
    std::string expectedOutputs = configPt->get<std::string>("expected_outputs", "comma-separated list of expected output slots");

    std::list<std::string> requiredInputSlots;
    boost::split(requiredInputSlots, expectedInputs,boost::is_any_of(","));
    for(auto it = requiredInputSlots.begin(); it != requiredInputSlots.end(); it++) {
        addInputSlotAndUUID(*it, UUID_AnyDecoderMessage); // GodecDocIgnore
        // addInputSlotAndUUID(<slots from 'expected_inputs'>, UUID_AnyDecoderMessage);  // Replacement for above godec doc ignore
    }

    std::list<std::string> requiredOutputSlots;
    boost::split(requiredOutputSlots, expectedOutputs,boost::is_any_of(","));
    // .push_back(Slots From 'expected_outputs');  // godec doc won't catch the above construct
    initOutputs(requiredOutputSlots);
#ifndef ANDROID
    mJNIEnv = NULL;
#endif
}

JavaComponent::~JavaComponent() {
#ifndef ANDROID
    mJVM->DetachCurrentThread();
#endif
}

#ifndef ANDROID
jobject JavaComponent::DecoderMsgHashToJNI(JNIEnv* jniEnv, const unordered_map<std::string, DecoderMessage_ptr>& map) {
    jclass HashClass = jniEnv->FindClass("java/util/HashMap");
    jmethodID HashConstructor = jniEnv->GetMethodID(HashClass, "<init>", "(I)V");
    jobject jMsgHash = jniEnv->NewObject(HashClass, HashConstructor, 10, nullptr);
    jmethodID PutMethod = jniEnv->GetMethodID(HashClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    for(auto it = map.begin(); it != map.end(); it++) {
        auto nonConstMsg = boost::const_pointer_cast<DecoderMessage>(it->second);
        jobject jMsg = nonConstMsg->toJNI(jniEnv);
        jstring jSlot = jniEnv->NewStringUTF(it->first.c_str());
        jniEnv->CallObjectMethod(jMsgHash, PutMethod, jSlot, jMsg);
    }
    return jMsgHash;
}

unordered_map<std::string, DecoderMessage_ptr> JavaComponent::JNIHashToDecoderMsg(JNIEnv* jniEnv, jobject jHash) {
    unordered_map<std::string, DecoderMessage_ptr> out;
    jclass HashClass = jniEnv->FindClass("java/util/HashMap");
    jclass SetClass = jniEnv->FindClass("java/util/Set");
    jmethodID KeySetMethod = jniEnv->GetMethodID(HashClass, "keySet", "()Ljava/util/Set;");
    jobject jKeySet = jniEnv->CallObjectMethod(jHash, KeySetMethod);
    jmethodID ToArrayMethod = jniEnv->GetMethodID(SetClass, "toArray", "()[Ljava/lang/Object;");
    jobjectArray jKeyArray = (jobjectArray)jniEnv->CallObjectMethod(jKeySet, ToArrayMethod);
    int jKeyArrayCount = jniEnv->GetArrayLength(jKeyArray);
    jmethodID GetMethod = jniEnv->GetMethodID(HashClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
    for(int keyIdx = 0; keyIdx < jKeyArrayCount; keyIdx++) {
        jstring jKey = (jstring)jniEnv->GetObjectArrayElement(jKeyArray, keyIdx);
        const char* jKeyChars = jniEnv->GetStringUTFChars(jKey, NULL);
        jobject jVal = jniEnv->CallObjectMethod(jHash, GetMethod, jKey);
        DecoderMessage_ptr msg = GetComponentGraph()->JNIToDecoderMsg(jniEnv, jVal);

        out[jKeyChars] = msg;
        jniEnv->ReleaseStringUTFChars(jKey, jKeyChars);
    }

    return out;
}
#endif

void JavaComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
#ifndef ANDROID
    uint64_t time = msgBlock.getMap().begin()->second->getTime();
    if (mJNIEnv == nullptr) {
        JavaVM* jvms[10];
        jsize numVMs;
        JNI_GetCreatedJavaVMs(jvms, 10, &numVMs);
        if (numVMs == 0) GODEC_ERR << getLPId(false) << ": No Java VM could be retrieved; if running Godec from command line, specify java class path with -java_class_path argument";
        mJVM = jvms[0];
        if (mJVM->AttachCurrentThreadAsDaemon((void**)&mJNIEnv, NULL) != JNI_OK) GODEC_ERR << getLPId(false) << ": Couldn't attach thread to JVM";

        if (mJNIEnv == nullptr) GODEC_ERR << getLPId(false) << ": JNI environment is NULL; make sure you set the Java class path in the Godec command line";
        jclass Class = mJNIEnv->FindClass(mClassName.c_str());
        jmethodID constructor = mJNIEnv->GetMethodID(Class, "<init>", "(Ljava/lang/String;)V");
        if (constructor == nullptr) GODEC_ERR << getLPId(false) << ": Could not find constructor of Java class "+mClassName;
        jstring jParam = mJNIEnv->NewStringUTF(mClassParam.c_str());
        mClassObject = mJNIEnv->NewObject(Class, constructor, jParam);
        if (mClassObject == nullptr) GODEC_ERR << getLPId(false) << ": Couldn't instatiate Java class "+mClassName;
        mProcessMessage = mJNIEnv->GetMethodID(Class, "ProcessMessage", "(Ljava/util/HashMap;J)Ljava/util/HashMap;");
        if (mProcessMessage == nullptr) GODEC_ERR << getLPId(false) << ": Could not find ProcessMessage in Java class "+mClassName;
    }

    jobject jRetHash = mJNIEnv->CallObjectMethod(mClassObject, mProcessMessage, DecoderMsgHashToJNI(mJNIEnv, msgBlock.getMap()), (jlong)time);
    auto retHash = JNIHashToDecoderMsg(mJNIEnv, jRetHash);
    for(auto it = retHash.begin(); it != retHash.end(); it++) {
        pushToOutputs(it->first, it->second);
    }
#endif
}

}
