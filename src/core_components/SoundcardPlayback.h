#pragma once

#ifdef _MSC_VER
#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>
#elif ANDROID
#include <sys/types.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#else
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#endif

#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"
#undef PAGE_SIZE
#undef PAGE_MASK
#include "boost/thread/thread.hpp"

namespace Godec {

using namespace std;

class SoundDataPlayer {
  public:
    SoundDataPlayer();
    virtual bool playData(long numSamples, const unsigned char* data) = 0;
    virtual ~SoundDataPlayer() = 0;
};

#ifdef _MSC_VER

class WindowsSoundcardPlayer : public SoundDataPlayer {
  public:
    WindowsSoundcardPlayer(float samplingRate, int numChannels, int sampleDepth);
    ~WindowsSoundcardPlayer();
    bool playData(long numSamples, const unsigned char* data) override;

  private:

    float mSamplingRate;
    int mNumChannels;
    int mSampleDepth;

    WAVEFORMATEX wex;
    WAVEHDR whdr[2];
    int mBufferIdx;
    HWAVEOUT mHwi; // the output device
};

#elif ANDROID

class AndroidAudioPlayer : public SoundDataPlayer {

  public:
    AndroidAudioPlayer(float samplingRate, int numChannels, int sampleDepth);
    ~AndroidAudioPlayer();
    bool playData(long numSamples, const unsigned char* data) override;

  private:
#if 0
    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    // recorder interfaces
    SLObjectItf playbackObject;
    SLRecordItf playerPlay;
#endif
};

#else
class LinuxAudioPlayer : public SoundDataPlayer {
  public:
    LinuxAudioPlayer(std::string cardId, float samplingRate, int numChannels, int sampleDepth);
    ~LinuxAudioPlayer();
    bool playData(long numSamples, const unsigned char* data) override;

  private:
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    bool mStarted;
};
#endif

class SoundcardPlayerComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    SoundcardPlayerComponent(std::string id, ComponentGraphConfig* configPt);
    virtual ~SoundcardPlayerComponent();
  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
    SoundDataPlayer* mPlayer;
    float mSamplingRate;
    int mNumChannels;
    int mSampleDepth;
    std::string mSoundcardIdentifier;
};

}

