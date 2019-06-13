#include "FileWriter.h"
#include <iomanip>
#include "cnpy.h"

namespace Godec {

std::string FileWriterComponent::SlotInput = "input_stream";
std::string FileWriterComponent::SlotFileFeederConvstate = "file_feeder_conversation_state";

LoopProcessor *FileWriterComponent::make(std::string id, ComponentGraphConfig *configPt) {
    return new FileWriterComponent(id, configPt);
}
std::string FileWriterComponent::describeThyself() {
    return "Component that writes stream to file";
}

/* FileWriterComponent::ExtendedDescription
The writing equivalent to the FileFeeder component, for saving output. Available "input_type":

"audio": For writing AudioDecoderMessage messages. "output_file_prefix" specifies the path prefix that each utterance gets written to

"raw_text": BinaryDecoderMessage expected that gets converted into text and written into "output_file"

"json": Expects JsonDecoderMessage as input, concatenates the JSONs into the "output_file"

"features": FeatureDecoderMessage as input, output file is a Numpy NPZ file, with the utterance IDs as keys

Like the FileFeeder, the "control_type" specifies whether this is a one-shot run that goes straight off the JSON parameters ("single_on_startup"), or whether it receives these JSON parameters through an external channel ("external"). When in the external mode, it also requires the ConversationState stream from the FileFeeder that produced the content
*/

FileWriterInputType String2FWType(std::string ts) {
    if (ts == "audio") return Audio;
    else if (ts == "raw_text") return RawText;
    else if (ts == "json") return Json;
    else if (ts == "features") return Features;

    GODEC_ERR << "Unknown Godec core FileWriter input type '" << ts << "'";
    return Audio; // to make compiler happy
}

boost::shared_ptr<FileWriterHolder> FileWriterComponent::GetFileWriterFromConfig( ComponentGraphConfig* configPt) {
    auto fwh = boost::shared_ptr<FileWriterHolder>(new FileWriterHolder());

    fwh->mInputType = String2FWType(configPt->get<std::string>("input_type", "Input stream type (audio, raw_text, features, json)"));
    if (fwh->mInputType == Audio) {
        fwh->mAudioPrefix = configPt->get<std::string>("output_file_prefix", "Output audio file path prefix");
        fwh->audioFp = NULL;
    } else if (fwh->mInputType == RawText) {
        std::string output_file = configPt->get<std::string>("output_file", "Output text file path");
        fwh->raw_text_writer.open(output_file);
    } else if (fwh->mInputType == Json) {
        std::string output_file = configPt->get<std::string>("output_file", "Json output file path");
        fwh->mJsonOutputFormat = configPt->get<std::string>("json_output_format", "Output format for json (raw_json, ctm, fst_search, mt)");

        if (fwh->mJsonOutputFormat != "raw_json" && fwh->mJsonOutputFormat != "ctm" && fwh->mJsonOutputFormat != "fst_search" && fwh->mJsonOutputFormat != "mt") {
            GODEC_ERR << ": Invalid json_output_format '" << fwh->mJsonOutputFormat << "'. Use 'raw_json' if you just want to dump json string." << std::endl;
        }
        fwh->json_output_writer.open(output_file);
    } else if (fwh->mInputType == Features) {
        fwh->mFeaturesNpz = configPt->get<std::string>("npz_file", "Output Numpy npz file name");
        boost::filesystem::remove(fwh->mFeaturesNpz);
    }
    return fwh;
}


FileWriterComponent::FileWriterComponent(std::string id, ComponentGraphConfig *configPt) :
    LoopProcessor(id, configPt) {

    mCurrentFWH = nullptr;
    mPrevConvoEndTimeInTicksStreamBased = -1;
    mPrevConvoEndTimeInSecondsStreamBased = 0.0;

    mControlType = configPt->get<std::string>("control_type", "Where this FileWriter gets its output configuration from: single-shot on startup ('single_on_startup'), or as JSON input from an input stream ('external')");
    if (mControlType == "single_on_startup") {
        mCurrentFWH = GetFileWriterFromConfig(configPt);
    } else if (mControlType == "external") {
        addInputSlotAndUUID(SlotControl, UUID_JsonDecoderMessage);
    } else GODEC_ERR << "Unknown control type " << mControlType;

    if (mCurrentFWH != nullptr && (mCurrentFWH->mJsonOutputFormat == "ctm" || mCurrentFWH->mJsonOutputFormat == "fst_search")) { // This means we can't have "external mode" file writing for these two formats. We'll cross that bridge when get there (if ever)
        addInputSlotAndUUID(SlotStreamedAudio, UUID_AudioDecoderMessage);
        addInputSlotAndUUID(SlotFileFeederConvstate, UUID_ConversationStateDecoderMessage);
    }
    addInputSlotAndUUID(SlotInput, UUID_AnyDecoderMessage);
}

void FileWriterComponent::Shutdown() {
    LoopProcessor::Shutdown();
}

void FileWriterComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto baseInputMsg = msgBlock.getBaseMsg(SlotInput);

    if (mControlType == "external") {
        auto jsonMsg = msgBlock.get<JsonDecoderMessage>(SlotControl);
        std::stringstream cleanJson;
        cleanJson << StripCommentsFromJSON(jsonMsg->getJsonObj().dump());
        if (cleanJson.str() != mCurrentFWHJson) {
            json pt = json::parse(cleanJson);
            ComponentGraphConfig config("bla", pt, nullptr, nullptr);
            mCurrentFWH = GetFileWriterFromConfig(&config);
            mCurrentFWHJson = jsonMsg->getJsonObj().dump();
        }
    }

    if (mCurrentFWH->mInputType == Audio) {
        if (mCurrentFWH->audioFp == NULL) {
            std::string completeFilename = mCurrentFWH->mAudioPrefix + convStateMsg->mUtteranceId + ".raw";
            for (int idx = 0; idx < completeFilename.length(); idx++) {
                char c = completeFilename[idx];
                if (!(
                            (c >= 'A' && c <= 'z') ||
                            (c >= '0' && c <= '9') ||
                            (c == '_') ||
                            (c == '-') ||
                            (c == '.') ||
                            (idx == 1 && c == ':') ||
                            (c == '\\') ||
                            (c == '/')
                        )) {
                    completeFilename[idx] = '_';
                }
            }
            mCurrentFWH->audioFp = fopen(completeFilename.c_str(), "wb");
            if (mCurrentFWH->audioFp == NULL) GODEC_ERR << "Couldn't open audio file '" << completeFilename << "' for writing";
        }
        if (baseInputMsg->getUUID() == UUID_AudioDecoderMessage) {
            auto audioMsg = msgBlock.get<AudioDecoderMessage>(SlotInput);
            const Vector &audioData = audioMsg->mAudio;
            for (int i = 0; i < audioData.size(); i++) {
                short val = (short)audioData(i);
                fwrite(&val, sizeof(short), 1, mCurrentFWH->audioFp);
            }
        } else if (baseInputMsg->getUUID() == UUID_BinaryDecoderMessage) {
            auto binaryMsg =msgBlock.get<BinaryDecoderMessage>(SlotInput);
            const std::vector<unsigned char> &data = binaryMsg->mData;
            fwrite(&data[0], sizeof(unsigned char), data.size(), mCurrentFWH->audioFp);
        } else
            GODEC_ERR << "Unexpected message type ";
        fflush(mCurrentFWH->audioFp);
        if (convStateMsg->mLastChunkInUtt) {
            fclose(mCurrentFWH->audioFp);
            mCurrentFWH->audioFp = NULL;
        }
    } else if (mCurrentFWH->mInputType == RawText) {
        auto binaryMsg = msgBlock.get<BinaryDecoderMessage>(SlotInput);
        mCurrentFWH->raw_text_writer << CharVec2String(binaryMsg->mData) << std::endl;
        mCurrentFWH->raw_text_writer.flush();
    } else if (mCurrentFWH->mInputType == Json) {
        auto jsonMsg = msgBlock.get<JsonDecoderMessage>(SlotInput);
        WriteJsonOutput(jsonMsg, convStateMsg, msgBlock, mCurrentFWH->mJsonOutputFormat, mCurrentFWH->json_output_writer);
    } else if (mCurrentFWH->mInputType == Features) {
        auto featsMsg = msgBlock.get<FeaturesDecoderMessage>(SlotInput);
        std::vector<size_t> shape{(size_t)featsMsg->mFeatures.rows(), (size_t)featsMsg->mFeatures.cols()};
        Matrix featsT = featsMsg->mFeatures.transpose();
        cnpy::npz_save(mCurrentFWH->mFeaturesNpz, featsMsg->mUtteranceId, featsT.data(), shape, "a");
    }
}
void FileWriterComponent::WriteJsonOutput(boost::shared_ptr<const JsonDecoderMessage> jsonMsg, boost::shared_ptr<const ConversationStateDecoderMessage> convMsg, const DecoderMessageBlock& msgBlock, std::string jsonOutputFormat, std::ofstream& json_output_writer) {
    if (jsonOutputFormat == "raw_json") {
        json_output_writer << jsonMsg->getJsonObj().dump(4) << std::endl;
    } else if (jsonOutputFormat == "ctm" || jsonOutputFormat == "fst_search") {
        auto audioMsg =msgBlock.get<AudioDecoderMessage>(SlotStreamedAudio);
        float secondsPerTick = 1.0 / (audioMsg->mSampleRate*audioMsg->mTicksPerSample);

        // This is an incredibly ugly allowance for the fact that CTMs are in reference to the analist's audio file
        if (audioMsg->getDescriptor("utterance_offset_in_file") != "" && boost::lexical_cast<float>(audioMsg->getDescriptor("utterance_offset_in_file")) != mPrevConvoEndTimeInSecondsStreamBased) {
            mPrevConvoEndTimeInSecondsStreamBased = boost::lexical_cast<float>(audioMsg->getDescriptor("utterance_offset_in_file"));
        }
        std::string file = audioMsg->getDescriptor("wave_file_name") != "" ? audioMsg->getDescriptor("wave_file_name") : "dummy";
        std::string channel = audioMsg->getDescriptor("channel") == "1" ? "A" : "B";
        json_output_writer << std::fixed << std::setprecision(2);
        json j = jsonMsg->getJsonObj();
        if (jsonOutputFormat == "ctm") {
            for (size_t i = 0; i < j["words"].size(); ++i) {
                json w = j["words"][i];

                float wordBeginInSeconds = mPrevConvoEndTimeInSecondsStreamBased + (w["beginTime"].get<int64_t>() - mPrevConvoEndTimeInTicksStreamBased + 1) * secondsPerTick;
                float wordDurationInSeconds = w["duration"].get<int64_t>() * secondsPerTick;

                json_output_writer << file << " "
                                   << channel << " "
                                   << wordBeginInSeconds << " "
                                   << wordDurationInSeconds << " "
                                   << w["word"].get<std::string>() << " "
                                   << TwoDigitsPrecisionRound(w["score"]);

                if(!w["case"].is_null()) {
                    std::string wcase = w["case"].get<std::string>();
                    json_output_writer << " case:" << wcase;
                }
                if(!w["punc"].is_null()) {
                    std::string wpunc = w["punc"].get<std::string>();
                    json_output_writer << " punc:" << wpunc;
                }
                json_output_writer << std::endl;
            }
        } else if (jsonOutputFormat == "fst_search") {
            float beginTime = mPrevConvoEndTimeInSecondsStreamBased + (j["beginTime"].get<int64_t>() - mPrevConvoEndTimeInTicksStreamBased + 1) * secondsPerTick;
            for (size_t i = 0; i < j["searchOutput"].size(); ++i) {
                json& w = j["searchOutput"][i];
                float wordBeginInSeconds = beginTime + w["relativeBeginTime"].get<int64_t>() * secondsPerTick;
                float wordDurationInSeconds = w["duration"].get<int64_t>() * secondsPerTick;
                json_output_writer << file << " "
                                   << channel << " "
                                   << wordBeginInSeconds << " "
                                   << wordDurationInSeconds << " "
                                   << w["outputString"].get<std::string>() << " "
                                   << TwoDigitsPrecisionRound(w["score"]);
                json_output_writer << std::endl;
            }
        }

        // This is an incredibly ugly allowance for the fact that CTMs are in reference to the analist's audio file. Pt II
        auto origConvMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotFileFeederConvstate);
        if (origConvMsg->mLastChunkInUtt) {
            mPrevConvoEndTimeInTicksStreamBased = convMsg->getTime();
        }
    } else if (jsonOutputFormat == "mt") {
        json j = jsonMsg->getJsonObj();
        const std::string& translatedText = j["translatedText"].get<std::string>();
        json_output_writer << translatedText << std::endl;
    }

    json_output_writer.flush();
}

}

