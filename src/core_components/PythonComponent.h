#pragma once

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"

namespace Godec {

class PythonComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    PythonComponent(std::string id, ComponentGraphConfig* configPt);
    ~PythonComponent();

    virtual bool RequiresConvStateInput() override { return false; }

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
#ifndef ANDROID
    PyObject* DecoderMsgHashToPython(const DecoderMessageBlock& msgBlock);
    unordered_map<std::string, DecoderMessage_ptr> PythonDictToDecoderMsg(PyObject* pDict);
    PyObject* mPModule;
    PyObject* mPModuleDict;
    PyObject* mPClass;
    PyThreadState* mPMainThread;
    std::wstring mPythonExec;
#endif
};

}

