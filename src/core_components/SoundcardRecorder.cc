#include "SoundcardRecorder.h"
#include <godec/ComponentGraph.h>

namespace Godec {

SoundDataReceiver::SoundDataReceiver() {
}

SoundDataReceiver::~SoundDataReceiver() {
}

LoopProcessor* SoundcardRecorderComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new SoundcardRecorderComponent(id, configPt);
}
std::string SoundcardRecorderComponent::describeThyself() {
    return "Opens sounds card, records audio and pushes it as audio stream";
}

/* SoundcardRecorderComponent::ExtendedDescription
The "start_on_boot" parameter, when set to "false", will require the "control" stream to be specified. That stream expects a ConversationStateDecoderMessage. If the stream is within an utterance (according to the ConversationState) the soundcard will output recorded audio, if it sees the end-of-utterance it stops, and so on.
*/

SoundcardRecorderComponent::SoundcardRecorderComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {
    float samplingRate = configPt->get<float>("sampling_rate", "Sampling rate");
    int numChannels = configPt->get<int>("num_channels", "Number of channels (1=mono, 2=stereo)");
    int sampleDepth = configPt->get<int>("sample_depth", "Sample depth (8,16,24 etc bit)");
    int chunkSize = configPt->get<int>("chunk_size", "Chunk size in samples (set to -1 for minimum latency configuratio)");
    bool startOnBoot = configPt->get<bool>("start_on_boot", "Whether audio should be pushed immediately");
    mTimeUpsampleFactor = 1;
    mTimeUpsampleFactor = configPt->get<int>("time_upsample_factor", "Factor by which the internal time stamps are increased. This is to prevent multiple subunits having the same time stamp.");
#ifdef _MSC_VER
    mRecorder = new WindowsSoundcardRecorder(samplingRate, numChannels, sampleDepth, chunkSize, this);
#elif ANDROID
    mRecorder = new AndroidAudioRecorder(samplingRate, numChannels, sampleDepth, chunkSize, this);
#else
    std::string soundcardIdentifier = configPt->get<std::string>("soundcard_identifier", "Soundcard identifier on Linux, use 'arecord -L' for list");
    mRecorder = new LinuxAudioRecorder(soundcardIdentifier, samplingRate, numChannels, sampleDepth, chunkSize, this);
#endif

    if (startOnBoot) {
        mState = Pushing;
    } else {
        mState = NotPushing;
        addInputSlotAndUUID(SlotControl, UUID_ConversationStateDecoderMessage);
    }

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotStreamedAudio);
    requiredOutputSlots.push_back(SlotConversationState);
    initOutputs(requiredOutputSlots);

    mTotalPushedSamples = 0;
}

SoundcardRecorderComponent::~SoundcardRecorderComponent() {}

void SoundcardRecorderComponent::Start() {
    mRecorder->startCapture();
    LoopProcessor::Start();
}

bool SoundcardRecorderComponent::receiveData(long numSamples, float sampleRate, int sampleDepth, int numChannels, const unsigned char* data) {
    boost::unique_lock<boost::mutex> lock(mStateMutex);
    if (mState == Pushing || mState == ToldToStopPushing) {
        std::stringstream ss;
        ss << "base_format=PCM;sample_width=" << sampleDepth << ";sample_rate=" << sampleRate << ";vtl_stretch=1.0;num_channels=" << numChannels;
        std::string formatString = ss.str();
        mTotalPushedSamples += numSamples;
        DecoderMessage_ptr audioMsg = BinaryDecoderMessage::create(mTimeUpsampleFactor*mTotalPushedSamples - 1, std::vector<unsigned char>(data, data + numSamples*numChannels*sampleDepth / 8), formatString);
        pushToOutputs(SlotStreamedAudio, audioMsg);
        bool lastChunkInUtt = (mState == ToldToStopPushing);
        DecoderMessage_ptr convMsg = ConversationStateDecoderMessage::create(mTimeUpsampleFactor*mTotalPushedSamples - 1, mCurrentUttId, lastChunkInUtt, "convo", false);
        pushToOutputs(SlotConversationState, convMsg);
        if (lastChunkInUtt) {
            mState = NotPushing;
        }
    }
    return true;
}

void SoundcardRecorderComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    boost::unique_lock<boost::mutex> lock(mStateMutex);
    auto controlMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotControl);

    if (mState == Pushing) {
        if (controlMsg->mLastChunkInUtt) {
            mState = ToldToStopPushing;
        }
    } else if (mState == NotPushing) {
        mCurrentUttId = controlMsg->mUtteranceId;
        mState = Pushing;
    }
}

#ifdef _MSC_VER

WindowsSoundcardRecorder::WindowsSoundcardRecorder(float samplingRate, int numChannels, int sampleDepth, int chunkSizeInSamples, SoundcardRecorderComponent *godecComp) {
    mSamplingRate = samplingRate;
    mNumChannels = numChannels;
    mSampleDepth = sampleDepth;
    mChunkSize = chunkSizeInSamples;

    // Initialize sound-card.

    mGodecComp = godecComp;

    wex.wFormatTag = WAVE_FORMAT_PCM;
    wex.cbSize = 0;
    wex.nChannels = numChannels;
    wex.nSamplesPerSec = (DWORD)(round(samplingRate));
    wex.wBitsPerSample = sampleDepth;
    wex.nBlockAlign = wex.nChannels * wex.wBitsPerSample / 8;
    wex.nAvgBytesPerSec = wex.nSamplesPerSec * wex.nBlockAlign;

    size_t bufferSize = chunkSizeInSamples*wex.wBitsPerSample / 8;

    size_t buffer;
    for (buffer = 0; buffer < numberOfBuffers; buffer++) {
        memset(&(whdr[buffer]), 0x00, sizeof(whdr[buffer]));
        whdr[buffer].dwBufferLength = (DWORD)bufferSize;
        if (!(whdr[buffer].lpData = new char[whdr[buffer].dwBufferLength])) {
            GODEC_ERR << "Failed to allocate memory.";
        }
    }


    size_t numberOfRecordingDevices = waveInGetNumDevs();
    if (numberOfRecordingDevices == 0) GODEC_ERR << "Windows reports no input devices";
    MMRESULT returnCode;

    returnCode = waveInOpen(&mHwi, 0, &wex, 0, 0, CALLBACK_NULL);
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

    bool successfullySetupBuffers = true;
    for (buffer = 0; buffer < numberOfBuffers; buffer++) {
        whdr[buffer].dwFlags = 0;
        if (waveInPrepareHeader(mHwi, &whdr[buffer], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            successfullySetupBuffers = false;
            break;
        }
        if (waveInAddBuffer(mHwi, &whdr[buffer], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            successfullySetupBuffers = false;
            break;
        }
    }
    if (successfullySetupBuffers == false) {
        GODEC_ERR << "Couldn't set up buffers";
    }
}


WindowsSoundcardRecorder::~WindowsSoundcardRecorder() {
    stopCapture();
}

void WindowsSoundcardRecorder::ProcessLoop() {
    size_t currentBuffer = 0;

    if (waveInStart(mHwi) != MMSYSERR_NOERROR) GODEC_ERR << "Couldn't start sound capture";

    while (mKeepRunning) {
        while (!(whdr[currentBuffer].dwFlags & WHDR_DONE)) {
            Sleep(10);
        }

        if (waveInUnprepareHeader(mHwi, &(whdr[currentBuffer]), sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            GODEC_ERR << "Failed to read audio buffers.";
        }

        long numSamples = whdr[currentBuffer].dwBytesRecorded / (mNumChannels*(mSampleDepth / 8));
        mGodecComp->receiveData(numSamples, mSamplingRate, mSampleDepth, mNumChannels, (const unsigned char*)whdr[currentBuffer].lpData);

        if (waveInPrepareHeader(mHwi, &whdr[currentBuffer], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            GODEC_ERR << "Failed to prepare audio buffers for recording. ";
            break;
        }
        if (waveInAddBuffer(mHwi, &whdr[currentBuffer], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            GODEC_ERR << "Failed to prepare audio buffers for recording.";
            break;
        }
        currentBuffer = (currentBuffer + 1) % numberOfBuffers;
    }
}


void WindowsSoundcardRecorder::startCapture() {
    mKeepRunning = true;
    mProcThread = boost::thread(&WindowsSoundcardRecorder::ProcessLoop, this);
    RegisterThreadForLogging(mProcThread, mGodecComp->getLogPtr(), mGodecComp->isVerbose());
}

void WindowsSoundcardRecorder::stopCapture() {
    mKeepRunning = false;
    mProcThread.join();
    if (waveInStop(mHwi) != MMSYSERR_NOERROR) GODEC_ERR << "Failed to stop audio recording.";
    waveInClose(mHwi);
}

#elif ANDROID

void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AndroidAudioRecorder* r = (AndroidAudioRecorder*) context;

    std::vector<unsigned char>& bufferToBeFilled = r->inputBuffer[r->currentInputBuffer];
    (*bq)->Enqueue(bq, &bufferToBeFilled[0], bufferToBeFilled.size());
    r->currentInputBuffer = (r->currentInputBuffer == 1 ? 0 : 1);

    std::vector<unsigned char>& filledBuffer = r->inputBuffer[r->currentInputBuffer];
    long numSamples = filledBuffer.size()/(r->mNumChannels*(r->mSampleDepth / 8));
    r->mGodecComp->receiveData(numSamples, r->mSamplingRate, r->mSampleDepth, r->mNumChannels, &filledBuffer[0]);
}

AndroidAudioRecorder::AndroidAudioRecorder(float samplingRate, int numChannels, int sampleDepth, int chunkSizeInSamples, SoundcardRecorderComponent *godecComp) {
    mSamplingRate = samplingRate;
    mNumChannels = numChannels;
    mSampleDepth = sampleDepth;
    mChunkSize = chunkSizeInSamples;
    mGodecComp = godecComp;

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

}

void AndroidAudioRecorder::startCapture() {
    if ((*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't start capture";
}

void AndroidAudioRecorder::stopCapture() {
    if ((*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_PAUSED) != SL_RESULT_SUCCESS) GODEC_ERR << "Couldn't pause capture";
}

#else

void tttr(std::string s, int error) {
    if (error < 0) {
        std::cerr << s << " reason=" << snd_strerror(error) << std::endl;
        exit(-1);
    }
}

LinuxAudioRecorder::LinuxAudioRecorder(std::string cardId, float samplingRate, int numChannels, int sampleDepth, int chunkSizeInSamples, SoundcardRecorderComponent *godecComp) {
    mSamplingRate = samplingRate;
    mNumChannels = numChannels;
    mSampleDepth = sampleDepth;
    mChunkSize = chunkSizeInSamples;
    mGodecComp = godecComp;

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
    unsigned int uintVal;
    int dir;

    tttr("Couldn't open soundcard", snd_pcm_open (&capture_handle, cardId.c_str(), SND_PCM_STREAM_CAPTURE, 0));
    tttr("Couldn't alloc params", snd_pcm_hw_params_malloc (&hw_params));
    tttr("Couldn't fill HW params",snd_pcm_hw_params_any (capture_handle, hw_params));
    tttr("Couldn't set HW params access", snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED));
    tttr("Couldn't set the sample format", snd_pcm_hw_params_set_format (capture_handle, hw_params, sampleFormat));
    tttr("Couldn't set number of channels", snd_pcm_hw_params_set_channels (capture_handle, hw_params, numChannels));
    tttr("Couldn't set sampling rate", snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &desiredSamplingRate, 0));
    if ((unsigned int)samplingRate != desiredSamplingRate) GODEC_ERR << "Desired sampling rate not available. ALSA says " << desiredSamplingRate << " is nearest";
    tttr("Couldn't set min periods", snd_pcm_hw_params_set_periods(capture_handle, hw_params, 2, 0));
    snd_pcm_uframes_t frames = chunkSizeInSamples;
    if (chunkSizeInSamples >= 0) {
        tttr("Couldn't set period size", snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params, &frames, 0));
    } else { // Minimum latency
        tttr("Couldn't set period size", snd_pcm_hw_params_set_period_size_first(capture_handle, hw_params, &frames, 0));
    }
    mChunkSize = frames;
    snd_pcm_hw_params_get_buffer_time(hw_params, &uintVal, &dir);
    if (godecComp->isVerbose()) std::cout << godecComp->getLPId() << ": Capture latency is " << ((double)uintVal/1000) << "ms" << std::endl;
    tttr("Couldn't set HW params", snd_pcm_hw_params (capture_handle, hw_params));
    tttr("Couldn't prepare sound card", snd_pcm_prepare (capture_handle));
}

void LinuxAudioRecorder::startCapture() {
    if (snd_pcm_start(capture_handle) < 0) GODEC_ERR << "Couldn't start audio capture";
    //if (snd_pcm_pause(capture_handle, 0) < 0) GODEC_ERR << "Couldn't unpause audio capture";
    mKeepRunning = true;
    mProcThread = boost::thread(&LinuxAudioRecorder::ProcessLoop, this);
    RegisterThreadForLogging(mProcThread, mGodecComp->getLogPtr(), mGodecComp->isVerbose());
}

void LinuxAudioRecorder::stopCapture() {
    mKeepRunning = false;
    mProcThread.join();
    if (snd_pcm_pause(capture_handle, 1) < 0) GODEC_ERR << "Couldn't pause audio capture";
}

void LinuxAudioRecorder::ProcessLoop() {
    int bufferSize = mChunkSize*(mSampleDepth/8)*mNumChannels;
    unsigned char* audioBuffer = new unsigned char[bufferSize];
    snd_pcm_sframes_t ret;
    while (mKeepRunning) {
        ret = snd_pcm_readi(capture_handle, audioBuffer, mChunkSize);
        if (ret < 0) {
            if (ret == -EPIPE) {
                std::cout << "CXRUN";
                tttr("Couldn't prepare sound card", snd_pcm_prepare (capture_handle));
            } else if (ret == -EBADFD) GODEC_ERR << "Couldn't read audio, PCM is not in the right state";
            if (ret == -EPIPE) std::cout << "CXRUN";
            else if (ret == -EBADFD) GODEC_ERR << "Couldn't read audio, PCM is not in the right state";
            else if (ret == -ESTRPIPE) GODEC_ERR << "Couldn't read audio, suspend event occurred";
        }
        mGodecComp->receiveData(mChunkSize, mSamplingRate, mSampleDepth, mNumChannels, audioBuffer);
    }
}

#endif

}
