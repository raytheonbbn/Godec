#pragma once

#include "godec/ChannelMessenger.h"
#include "AccumCovariance.h"
#include "GodecMessages.h"
#include "FileFeeder.h"
#include "resample.h"

namespace Godec {

int Mulaw_Decode(unsigned char ulawbyte);
int Alaw_Decode(unsigned char a_val);

class AudioFormatParser {
  public:
    AudioType baseFormat;
    int sampleWidth;
    int numChannels;
    float sampleRate;
    float vtlStretch;
    static AudioFormatParser FromFormatString(std::string);
};

class AudioPreProcessorComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    AudioPreProcessorComponent(std::string id, ComponentGraphConfig* configPt);
    ~AudioPreProcessorComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    std::vector<boost::shared_ptr<AccumCovariance>> zeroMean;
    std::vector<boost::shared_ptr<Godec::LinearResample>> resample;
    std::vector<Vector> mStatsAccumAudio;

    int64_t getNextChunkSize();

    float mTargetSamplingRate;
    int64_t mUttReceivedRawAudio;
    int64_t mUttReceivedResampledAudio;
    int64_t mUttProducedAudio;
    int64_t mUttStartStreamOffset;
    float mPrevSampleRate;
    int mUpdateStatsHop;
    float mOutputScale;

    bool mDoZeroMean;
    float mPreemphasisFactor;
};

}
