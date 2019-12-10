#pragma once

#include "godec/ChannelMessenger.h"
#include "GodecMessages.h"
#include <stdio.h>
#include <fstream>

namespace Godec {

enum FileWriterInputType {
    Audio,
    Features,
    RawText,
    Json
};

class FileWriterHolder {
  public:
    FileWriterHolder() {
        audioFp = nullptr;
    }
    ~FileWriterHolder() {
        if (audioFp != NULL) {
            fclose(audioFp);
        }
        if (raw_text_writer.is_open()) {
            raw_text_writer.close();
        }
        if (json_output_writer.is_open()) {
            json_output_writer.close();
        }
    }

    FILE* audioFp;
    std::string mAudioPrefix;
    int mAudioSampleDepth;

    std::ofstream raw_text_writer;
    std::ofstream json_output_writer;
    FileWriterInputType mInputType;
    std::string mFstPrefix;
    std::string mJsonOutputFormat;
    std::string mFeaturesNpz;
    Matrix mFeaturesAccum;
};


class FileWriterComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    FileWriterComponent(std::string id, ComponentGraphConfig* configPt);
    void Shutdown();
  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    void WriteJsonOutput(boost::shared_ptr<const JsonDecoderMessage> jsonMsg, boost::shared_ptr<const ConversationStateDecoderMessage> convMsg, const DecoderMessageBlock& msgBlock, std::string jsonOutputFormat, std::ofstream& json_output_writer);
    boost::shared_ptr<FileWriterHolder> GetFileWriterFromConfig( ComponentGraphConfig* configPt);
    static std::string SlotInput;
    static std::string SlotFileFeederConvstate;
    float mPrevConvoEndTimeInSecondsStreamBased;
    int64_t mPrevConvoEndTimeInTicksStreamBased;

    std::string mControlType;
    boost::shared_ptr<FileWriterHolder> mCurrentFWH;
    std::string mCurrentFWHJson;
};

}
