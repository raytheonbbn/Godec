#pragma once

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class JavaComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    JavaComponent(std::string id, ComponentGraphConfig* configPt);
    ~JavaComponent();

    virtual bool RequiresConvStateInput() override { return false; }

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
#ifndef ANDROID
    jobject DecoderMsgHashToJNI(JNIEnv* jniEnv, const unordered_map<std::string, DecoderMessage_ptr>& map);
    unordered_map<std::string, DecoderMessage_ptr> JNIHashToDecoderMsg(JNIEnv* jniEnv, jobject jHash);
    jobject mClassObject;
    jmethodID mProcessMessage;
    JavaVM* mJVM;
    JNIEnv* mJNIEnv;
#endif

    std::string mClassName;
    std::string mClassParam;
};

}

