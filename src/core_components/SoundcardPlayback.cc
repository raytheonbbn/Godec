#include "SoundcardPlayback.h"
#include <godec/ComponentGraph.h>

namespace Godec {

SoundDataPlayer::SoundDataPlayer() {
}

SoundDataPlayer::~SoundDataPlayer() {
}

LoopProcessor* SoundcardPlayerComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new SoundcardPlayerComponent(id, configPt);
}
std::string SoundcardPlayerComponent::describeThyself() {
    return "Opens sounds card, plays incoming audio data";
}

SoundcardPlayerComponent::SoundcardPlayerComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    mSamplingRate = configPt->get<float>("sampling_rate", "Sampling rate");
    mNumChannels = configPt->get<int>("num_channels", "Number of channels (1=mono, 2=stereo)");
    mSampleDepth = configPt->get<int>("sample_depth", "Sample depth (8,16,24 etc bit)");
#ifdef _MSC_VER
    mPlayer = new WindowsSoundcardPlayer(mSamplingRate, mNumChannels, mSampleDepth);
#elif ANDROID
    mPlayer = new AndroidAudioPlayer(mSamplingRate, mNumChannels, mSampleDepth);
#else
    mSoundcardIdentifier = configPt->get<std::string>("soundcard_identifier", "Soundcard identifier on Linux, use 'aplay -L' for list");
    mPlayer = new LinuxAudioPlayer(mSoundcardIdentifier, mSamplingRate, mNumChannels, mSampleDepth);
#endif

    addInputSlotAndUUID(SlotStreamedAudio, UUID_AudioDecoderMessage);

    std::list<std::string> requiredOutputSlots;
    initOutputs(requiredOutputSlots);
}

SoundcardPlayerComponent::~SoundcardPlayerComponent() {}

void SoundcardPlayerComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto audioMsg = msgBlock.get<AudioDecoderMessage>(SlotStreamedAudio);
    std::vector<int16_t> outAudio;
    outAudio.resize(audioMsg->mAudio.size());
    for(int sampleIdx = 0; sampleIdx < outAudio.size(); sampleIdx++) {
        outAudio[sampleIdx] = (int16_t)audioMsg->mAudio(sampleIdx);
    }
    mPlayer->playData((long)outAudio.size(), (const unsigned char*)outAudio.data());
}

#ifdef _MSC_VER

WindowsSoundcardPlayer::WindowsSoundcardPlayer(float samplingRate, int numChannels, int sampleDepth) {

    mSamplingRate = samplingRate;
    mNumChannels = numChannels;
    mSampleDepth = sampleDepth;

    // Initialize sound-card.

    wex.wFormatTag = WAVE_FORMAT_PCM;
    wex.cbSize = 0;
    wex.nChannels = numChannels;
    wex.nSamplesPerSec = (DWORD)(round(samplingRate));
    wex.wBitsPerSample = sampleDepth;
    wex.nBlockAlign = wex.nChannels * wex.wBitsPerSample / 8;
    wex.nAvgBytesPerSec = wex.nSamplesPerSec * wex.nBlockAlign;

    for (int headerIdx = 0; headerIdx < 2; headerIdx++) {
        memset(&(whdr[headerIdx]), 0x00, sizeof(whdr[headerIdx]));
    }
    mBufferIdx = 0;

    size_t numberOfPlaybackDevices = waveOutGetNumDevs();
    if (numberOfPlaybackDevices == 0) GODEC_ERR << "Windows reports no output devices";
    MMRESULT returnCode;

    returnCode = waveOutOpen(&mHwi, 0, &wex, 0, 0, CALLBACK_NULL);
    if (returnCode != MMSYSERR_NOERROR) {
        string error;

        switch (returnCode) {
        case MMSYSERR_ALLOCATED:
            error = "Specified resource is already allocated.";
            break;
        case MMSYSERR_BADDEVICEID:
            error = "Specified device identifier is out of range.";
            break;
        case MMSYSERR_NODRIVER:
            error = "No device driver is present.";
            break;
        case MMSYSERR_NOMEM:
            error = "Unable to allocate or lock memory.";
            break;
        case WAVERR_BADFORMAT:
            error = "Attempted to open with an unsupported waveform-audio format.";
            break;
        default:
            error = "Unknown error.";
            break;
        }

        GODEC_ERR << "Couldn't open soundcard";
    }
}


WindowsSoundcardPlayer::~WindowsSoundcardPlayer() {
}

bool WindowsSoundcardPlayer::playData(long numSamples, const unsigned char* data) {
    int requiredBufSize = (mSampleDepth/8)*mNumChannels*numSamples;
    mBufferIdx = (mBufferIdx + 1) % 2;
    if (whdr[mBufferIdx].dwBufferLength != requiredBufSize) {
        waveOutUnprepareHeader(mHwi, &whdr[mBufferIdx], sizeof(WAVEHDR));
        whdr[mBufferIdx].dwBufferLength = requiredBufSize;
        whdr[mBufferIdx].lpData = new char[whdr[mBufferIdx].dwBufferLength];
        if (waveOutPrepareHeader(mHwi, &whdr[mBufferIdx], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            GODEC_ERR << "Couldn't set up buffers";
    }
    memcpy(whdr[mBufferIdx].lpData, data, whdr[mBufferIdx].dwBufferLength);
    MMRESULT writeRes;
    if ((writeRes = waveOutWrite(mHwi, &whdr[mBufferIdx], sizeof(WAVEHDR))) != MMSYSERR_NOERROR) {
        GODEC_ERR << "Couldn't write audio";
    }
    return true;
}

#elif ANDROID

#if 0
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AndroidAudioRecorder* r = (AndroidAudioRecorder*) context;

    std::vector<unsigned char>& bufferToBeFilled = r->inputBuffer[r->currentInputBuffer];
    (*bq)->Enqueue(bq, &bufferToBeFilled[0], bufferToBeFilled.size());
    r->currentInputBuffer = (r->currentInputBuffer == 1 ? 0 : 1);

    std::vector<unsigned char>& filledBuffer = r->inputBuffer[r->currentInputBuffer];
    long numSamples = filledBuffer.size()/(r->mNumChannels*(r->mSampleDepth / 8));
    r->mReceiver->receiveData(numSamples, r->mSamplingRate, r->mSampleDepth, r->mNumChannels, &filledBuffer[0]);
}
#endif

AndroidAudioPlayer::AndroidAudioPlayer(float samplingRate, int numChannels, int sampleDepth) {
    GODEC_ERR << "Android audio playback is not supported yet. Fix me!";
#if 0
    mSamplingRate = samplingRate;
    mNumChannels = numChannels;
    mSampleDepth = sampleDepth;
    mChunkSize = chunkSizeInSamples;
    mReceiver = receiver;

    inputBuffer[0].resize(mChunkSize*mNumChannels*(mSampleDepth/8));
    inputBuffer[1].resize(mChunkSize*mNumChannels*(mSampleDepth/8));
    currentInputBuffer = 0;

    if (slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't create engine";
    if ((*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't realize engine";
    if ((*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't get IID_ENGINE interface";

    SLuint32 sr = round(samplingRate);
    switch(sr) {

    case 8000:
        sr = SL_SAMPLINGRATE_8;
        break;
    case 11025:
        sr = SL_SAMPLINGRATE_11_025;
        break;
    case 16000:
        sr = SL_SAMPLINGRATE_16;
        break;
    case 22050:
        sr = SL_SAMPLINGRATE_22_05;
        break;
    case 24000:
        sr = SL_SAMPLINGRATE_24;
        break;
    case 32000:
        sr = SL_SAMPLINGRATE_32;
        break;
    case 44100:
        sr = SL_SAMPLINGRATE_44_1;
        break;
    case 48000:
        sr = SL_SAMPLINGRATE_48;
        break;
    case 64000:
        sr = SL_SAMPLINGRATE_64;
        break;
    case 88200:
        sr = SL_SAMPLINGRATE_88_2;
        break;
    case 96000:
        sr = SL_SAMPLINGRATE_96;
        break;
    case 192000:
        sr = SL_SAMPLINGRATE_192;
        break;
    default:
        GODEC_ERR << "Unsupported sample rate on Android";
    }

    // configure audio source
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};

    // configure audio sink
    int speakers = SL_SPEAKER_FRONT_CENTER;
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, sr,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   speakers, SL_BYTEORDER_LITTLEENDIAN
                                  };
    SLDataSink audioSnk = {&loc_bq, &format_pcm};

    GODEC_INFO << "AndroidAudioRecorder: Before CreateAudioRecorder, freq " << samplingRate <<  std::endl << std::flush;
    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    if ((*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc, &audioSnk, 1, id, req) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't create audio recorder";
    if ((*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't realize audio engine";
    if ((*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't get IID_RECORD interface";
    if ((*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recorderBufferQueue) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't get IID_SAMPLEBUFQ interface";
    if ((*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback, this) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't register callback";
    if ((*recorderBufferQueue)->Enqueue(recorderBufferQueue, &(inputBuffer[currentInputBuffer][0]), inputBuffer[currentInputBuffer].size()) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't enqueue buffer";
    GODEC_INFO << "AndroidAudioRecorder: Opened recording engine" << std::endl << std::flush;
#endif
}

AndroidAudioPlayer::~AndroidAudioPlayer() {
}

bool AndroidAudioPlayer::playData(long numSamples, const unsigned char* data) {
    return true;
}

#else

bool tttp(std::string s, bool b) {if (!b) GODEC_ERR << s << std::endl; return b;}

LinuxAudioPlayer::LinuxAudioPlayer(std::string cardId, float samplingRate, int numChannels, int sampleDepth) {
    snd_pcm_format_t sampleFormat = SND_PCM_FORMAT_S16_LE;
    if (sampleDepth == 8) {
        sampleFormat = SND_PCM_FORMAT_S8;
    } else if (sampleDepth == 16) {
        sampleFormat = SND_PCM_FORMAT_S16_LE;
    } else if (sampleDepth == 24) {
        sampleFormat = SND_PCM_FORMAT_S24_LE;
    } else if (sampleDepth == 32) {
        sampleFormat = SND_PCM_FORMAT_S32_LE;
    } else GODEC_ERR << "Unsupported sample depth " << sampleDepth;

    unsigned int desiredSamplingRate = samplingRate;

    tttp("Couldn't open soundcard", snd_pcm_open (&playback_handle, cardId.c_str(), SND_PCM_STREAM_PLAYBACK, 0) >= 0);
    tttp("Couldn't alloc params", snd_pcm_hw_params_malloc (&hw_params) >= 0);
    tttp("Couldn't fill HW params",snd_pcm_hw_params_any (playback_handle, hw_params) >= 0);
    tttp("Couldn't set HW params access", snd_pcm_hw_params_set_access (playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) >= 0);
    tttp("Couldn't set the sample format", snd_pcm_hw_params_set_format (playback_handle, hw_params, sampleFormat) >= 0);
    tttp("Couldn't set number of channels", snd_pcm_hw_params_set_channels (playback_handle, hw_params, numChannels) >= 0);
    tttp("Couldn't set sampling rate", snd_pcm_hw_params_set_rate_near (playback_handle, hw_params, &desiredSamplingRate, 0) >= 0);
    if ((unsigned int)samplingRate != desiredSamplingRate) GODEC_ERR << "Desired sampling rate not available. ALSA says " << desiredSamplingRate << " is nearest";
    tttp("Couldn't set HW params", snd_pcm_hw_params (playback_handle, hw_params) >= 0);
    tttp("Couldn't prepare sound card", snd_pcm_prepare (playback_handle) >= 0);
    tttp("Couldn't start audio capture", snd_pcm_start(playback_handle) < 0);
}

LinuxAudioPlayer::~LinuxAudioPlayer() {}

bool LinuxAudioPlayer::playData(long numSamples, const unsigned char* data) {
    unsigned int pcm;
    if ((pcm = snd_pcm_writei(playback_handle, data, numSamples)) == -EPIPE) {
        printf("XRUN.\n");
        snd_pcm_prepare(playback_handle);
    } else if (pcm < 0) {
        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
    }
    return true;
}

#endif

}
