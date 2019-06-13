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

class SoundDataReceiver {
  public:
    SoundDataReceiver();
    virtual bool receiveData(long numSamples, float sampleRate, int sampleDepth, int numChannels, const unsigned char* data) = 0;
    virtual ~SoundDataReceiver() = 0;
};

#ifdef _MSC_VER

class WindowsSoundcardRecorder {
  public:
    WindowsSoundcardRecorder(float samplingRate, int numChannels, int sampleDepth, int chunkSizeInSamples, SoundDataReceiver *receiver);
    ~WindowsSoundcardRecorder();
    void startCapture();
    void stopCapture();

  private:
    enum { numberOfBuffers = 30 };
    WAVEFORMATEX wex;
    WAVEHDR whdr[numberOfBuffers];
    HWAVEIN mHwi; // the input-device

    SoundDataReceiver *mReceiver;
    bool mKeepRunning;

    boost::thread mProcThread;
    void ProcessLoop();

    float mSamplingRate;
    int mNumChannels;
    int mSampleDepth;
    int mChunkSize;
};

#elif ANDROID

class AndroidAudioRecorder {

  public:
    AndroidAudioRecorder(float samplingRate, int numChannels, int sampleDepth, int chunkSizeInSamples, SoundDataReceiver *receiver);
    ~AndroidAudioRecorder();
    void startCapture();
    void stopCapture();

    std::vector<unsigned char> inputBuffer[2];
    int currentInputBuffer;
    SoundDataReceiver *mReceiver;

    float mSamplingRate;
    int mNumChannels;
    int mSampleDepth;
    int mChunkSize;

  private:
    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    // recorder interfaces
    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;

    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

};

#else
class LinuxAudioRecorder {
  public:
    LinuxAudioRecorder(std::string cardId, float samplingRate, int numChannels, int sampleDepth, int chunkSizeInSamples, SoundDataReceiver *receiver);
    ~LinuxAudioRecorder();
    void startCapture();
    void stopCapture();

  private:
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;

    SoundDataReceiver *mReceiver;
    bool mKeepRunning;

    float mSamplingRate;
    int mNumChannels;
    int mSampleDepth;
    int mChunkSize;

    boost::thread mProcThread;
    void ProcessLoop();
};
#endif

enum SoundcardRecorderState {
    NotPushing,
    //ToldToStartPushing,
    Pushing,
    ToldToStopPushing
};

class SoundcardRecorderComponent : public LoopProcessor, SoundDataReceiver {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    SoundcardRecorderComponent(std::string id, ComponentGraphConfig* configPt);
    virtual ~SoundcardRecorderComponent();
    bool receiveData(long numSamples, float sampleRate, int sampleDepth, int numChannels, const unsigned char* data) override;
  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock) override;
    void Start() override;
    bool RequiresConvStateInput() override { return false; }
#ifdef _MSC_VER
    WindowsSoundcardRecorder* mRecorder;
#elif ANDROID
    AndroidAudioRecorder* mRecorder;
#else
    LinuxAudioRecorder* mRecorder;
#endif

    uint64_t mTotalPushedSamples;
    std::string mCurrentUttId;
    int mTimeUpsampleFactor;

    boost::mutex mStateMutex;
    SoundcardRecorderState mState;
};

}
