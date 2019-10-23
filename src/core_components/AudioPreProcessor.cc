#include "AudioPreProcessor.h"
#include "godec/ComponentGraph.h"

namespace Godec {

Vector preemphasize(const Vector& audio, float preemphasisFactor) {
    Vector outAudio = audio;
    for (int32_t i = outAudio.size() - 1; i >= 1; --i) {
        outAudio(i) -= preemphasisFactor * outAudio(i - 1);
    }
    outAudio(0) *= 1.0 - preemphasisFactor;
    return outAudio;
}

/*
 ** This routine converts from ulaw to 16 bit linear.
 **
 ** Craig Reese: IDA/Supercomputing Research Center
 ** 29 September 1989
 **
 ** References:
 ** 1) CCITT Recommendation G.711  (very difficult to follow)
 ** 2) MIL-STD-188-113,"Interoperability and Performance Standards
 **     for Analog-to_Digital Conversion Techniques,"
 **     17 February 1987
 **
 ** Input: 8 bit ulaw sample
 ** Output: signed 16 bit linear sample
 */

int Mulaw_Decode(unsigned char ulawbyte) {
    static int exp_lut[8] = { 0, 132, 396, 924, 1980, 4092, 8316, 16764 };
    int sign, exponent, mantissa, sample;

    ulawbyte = ~ulawbyte;
    sign = (ulawbyte & 0x80);
    exponent = (ulawbyte >> 4) & 0x07;
    mantissa = ulawbyte & 0x0F;
    sample = exp_lut[exponent] + (mantissa << (exponent + 3));
    if (sign != 0) sample = -sample;

    return(sample);
}

const int SIGN_BIT = 0x80;      /* Sign bit for a A-law byte. */
const int QUANT_MASK = 0xf;  /* Quantization field mask. */
const int SEG_MASK = 0x70;   /* Segment field mask. */
const int SEG_SHIFT = 4;     /* Left shift for segment number. */

int Alaw_Decode(unsigned char a_val) {
    short t;
    short seg;

    a_val ^= 0x55;

    t = (a_val & QUANT_MASK) << 4;
    seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
    switch (seg) {
    case 0:
        t += 8;
        break;
    case 1:
        t += 0x108;
        break;
    default:
        t += 0x108;
        t <<= seg - 1;
    }
    return ((a_val & SIGN_BIT) ? t : -t);
}

AudioFormatParser AudioFormatParser::FromFormatString(std::string formatString) {
    AudioFormatParser out;
    out.vtlStretch = 1.0f;
    out.numChannels = 1;
    out.sampleWidth = 8;
    out.sampleRate = -1.0f;
    std::vector<std::string> formatEls;
    boost::split(formatEls, formatString,boost::is_any_of(";"));
    for (auto it = formatEls.begin(); it != formatEls.end(); it++) {
        std::vector<std::string> keyVal;
        boost::split(keyVal, *it,boost::is_any_of("="));
        if (keyVal[0] == "base_format") {
            if (keyVal[1] == "PCM") out.baseFormat = PCM;
            else if (keyVal[1] == "ulaw") out.baseFormat = MuLaw;
            else if (keyVal[1] == "alaw") out.baseFormat = Alaw;
            else GODEC_ERR << "Unknown base format " << keyVal[1];
        } else if (keyVal[0] == "num_channels") {
            out.numChannels = std::atoi(keyVal[1].c_str());
        } else if (keyVal[0] == "sample_width") {
            out.sampleWidth = std::atoi(keyVal[1].c_str());
        } else if (keyVal[0] == "sample_rate") {
            out.sampleRate = std::atof(keyVal[1].c_str());
        } else if (keyVal[0] == "vtl_stretch") {
            out.vtlStretch = std::atof(keyVal[1].c_str());
        } //else KALDI_WARN << "Unknown format string element '" << keyVal[0] << "'" <<  std::endl;
    }
    if (out.sampleRate < 0) GODEC_ERR << "Sample rate was not set in format string '" << formatString << "'";
    return out;
}

AudioPreProcessorComponent::~AudioPreProcessorComponent() {
}

LoopProcessor* AudioPreProcessorComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new AudioPreProcessorComponent(id, configPt);
}
std::string AudioPreProcessorComponent::describeThyself() {
    return "Component that takes audio and does the following operations on it: Zero-mean, Pre-emphasis, resampling.";
}

/* AudioPreProcessorComponent::ExtendedDescription
This component supports both BinaryDecoderMessage as well as AudioDecoderMessage input in its "stream_audio" slot. The output slots are enumerated in the form of "`streamed_audio_0`", "`streamed_audio_1`" etc, up to the number specified in "`max_out_channels`"
*/

AudioPreProcessorComponent::AudioPreProcessorComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id,configPt) {
    addInputSlotAndUUID(SlotStreamedAudio, UUID_AudioDecoderMessage);
    addInputSlotAndUUID(SlotStreamedAudio, UUID_BinaryDecoderMessage);

    mDoZeroMean = configPt->get<bool>("zero_mean", "Zero-mean incoming waveform (true, false)");
    mPreemphasisFactor = configPt->get<float>("preemphasis_factor", "Pre-emphasis factor (float, no-op value=1.0)");
    if (!(mPreemphasisFactor >= 0.0 && mPreemphasisFactor <= 1.0)) GODEC_ERR << "Bad parameters: 0.0 < preemp < 1.0\n";
    mTargetSamplingRate = configPt->get<float>("target_sampling_rate", "If required, resample audio to this rate");
    int maxOutChannels = configPt->get<int>("max_out_channels", "maximum number of output channels that need to be defined");
    mOutputScale = configPt->get<float>("output_scale", "Scale the output audio by this factor");

    mUttReceivedRawAudio = 0;
    mUttReceivedResampledAudio = 0;
    mUttProducedAudio = 0;
    mUttStartStreamOffset = 0;

    mUpdateStatsHop = 0.25*mTargetSamplingRate; // Update stats every quarter of a second

    std::list<std::string> requiredOutputSlots;
    for(int channelIdx = 0; channelIdx < maxOutChannels; channelIdx++) {
        std::stringstream ss;
        ss << SlotStreamedAudio << "_" << channelIdx;
        requiredOutputSlots.push_back(ss.str());
    }
    initOutputs(requiredOutputSlots);
}

int64_t AudioPreProcessorComponent::getNextChunkSize() {
    return std::min(mUttReceivedResampledAudio, ((mUttProducedAudio / mUpdateStatsHop) + 1)*(mUpdateStatsHop)) - mUttProducedAudio;
}

void AudioPreProcessorComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto audioBaseMsg = msgBlock.getBaseMsg(SlotStreamedAudio);

    float sampleRate = -1.0f;
    float vtlStretch = 1.0f;
    int numChannels = 1;
    std::vector<Vector> audioVecs;

    // If the message is in AudioDecoderMessage format, we already got the float values
    if (audioBaseMsg->getUUID() == UUID_AudioDecoderMessage) {
        auto audioMsg =msgBlock.get<AudioDecoderMessage>(SlotStreamedAudio);
        sampleRate = audioMsg->mSampleRate;
        audioVecs.push_back(audioMsg->mAudio);
        vtlStretch = audioMsg->getDescriptor("vtl_stretch") == "" ? vtlStretch : boost::lexical_cast<float>(audioMsg->getDescriptor("vtl_stretch"));
        // message is in BinaryDecoderMessage format, need to decode first
    } else if (audioBaseMsg->getUUID() == UUID_BinaryDecoderMessage) {
        auto binaryMsg =msgBlock.get<BinaryDecoderMessage>(SlotStreamedAudio);
        auto& binaryData = binaryMsg->mData;
        AudioFormatParser parser = AudioFormatParser::FromFormatString(binaryMsg->mFormat);
        sampleRate = parser.sampleRate;
        vtlStretch = parser.vtlStretch;
        numChannels = parser.numChannels;
        int bytesPerSample = parser.sampleWidth / 8;
        int32_t numSamples = binaryData.size() / (numChannels*bytesPerSample);
        for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
            audioVecs.push_back(Vector(numSamples));
            for (int sampleIdx = 0; sampleIdx < numSamples; sampleIdx++) {
                const unsigned char* samplePtr = &binaryData[bytesPerSample*(sampleIdx*numChannels + channelIdx)];
                if (parser.baseFormat == PCM) {
                    if (bytesPerSample == 1) {
                        audioVecs[channelIdx](sampleIdx) = *samplePtr;
                    } else if (bytesPerSample == 2) {
                        audioVecs[channelIdx](sampleIdx) = *((short*)samplePtr);
                    }
                } else if (parser.baseFormat == MuLaw) {
                    audioVecs[channelIdx](sampleIdx) = Mulaw_Decode(*samplePtr);
                } else if (parser.baseFormat == Alaw) {
                    audioVecs[channelIdx](sampleIdx) = Alaw_Decode(*samplePtr);
                }
            }
        }
    }

    mUttReceivedRawAudio += audioVecs[0].size();
    double inputTimePerSample = (convStateMsg->getTime() - mUttStartStreamOffset + 1) / (double)mUttReceivedRawAudio;
    double outputTimePerSample = inputTimePerSample*(sampleRate / mTargetSamplingRate);
    if (outputTimePerSample < 1.0)
        GODEC_ERR << "Due to upsampling from " << sampleRate << "Hz to " << mTargetSamplingRate << "Hz, each audio sample will no longer have a unique time stamp. To fix this, add the optional 'time_upsample_factor' to the FileFeeder or Soundcard component (whichever you are using) to a value of ceil(" << mTargetSamplingRate << "/" << sampleRate << ")=" << std::ceil(1.0 / outputTimePerSample) << " or higher. If the audio is fed via an API, it is the responsibility of them to increase the timestamps by that factor. Note that this factor was calculated based on this specific audio chunk's sampling rate. If you have audio with even lower sampling rate, you might have to increase the upsampling factor even more";

    // Init
    if (mStatsAccumAudio.size() == 0) {
        mStatsAccumAudio.resize(numChannels);
        zeroMean.resize(numChannels);
        resample.resize(numChannels);
        for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
            zeroMean[channelIdx] = AccumCovariance::make(1, Diagonal, mDoZeroMean, false);
            resample[channelIdx] = nullptr;
        }
    }

    // Maybe at some point this will be supported? It's not clear though what the timestamps would be if you suddenly have a new output stream that didn't exist before
    if (numChannels != zeroMean.size()) GODEC_ERR << getLPId() << ": Number of incoming channels changed mid-stream (from " << zeroMean.size() << " to " << numChannels << "). This is currently not supported";

    // Resample
    std::vector<Vector> resampledAudio(numChannels);
    for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
        if (sampleRate != mTargetSamplingRate) {
            if ((resample[channelIdx] == nullptr) || (mPrevSampleRate != sampleRate)) {
                if (resample[channelIdx] != nullptr) { resample[channelIdx].reset(); }
                float lowpass_freq = 0.45*std::min(sampleRate, mTargetSamplingRate);
                int32_t num_zeros = 10; // Totally made-up value. What's good?!
                resample[channelIdx] = boost::shared_ptr<Godec::LinearResample>(new Godec::LinearResample(sampleRate, mTargetSamplingRate, lowpass_freq, num_zeros));
            }
            mPrevSampleRate = sampleRate;

            Vector tmpResampled;
            resample[channelIdx]->Resample(audioVecs[channelIdx], convStateMsg->mLastChunkInUtt, &tmpResampled);
            resampledAudio[channelIdx] = tmpResampled;
        } else {
            resampledAudio[channelIdx] = audioVecs[channelIdx];
        }
    }

    mUttReceivedResampledAudio += resampledAudio[0].size();

    for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
        mStatsAccumAudio[channelIdx].conservativeResize(mStatsAccumAudio[channelIdx].size() + resampledAudio[channelIdx].size());
        mStatsAccumAudio[channelIdx].tail(resampledAudio[channelIdx].size()) = resampledAudio[channelIdx];
    }

    uint64_t outTimestamp = 0;
    std::vector<Vector> outAudio(numChannels);
    while(true) { // Iterate over audio chunks until there is none left. The point of doing it in this chunking way is to make the normalization independent of how much audio we received. If we just normalized over the entirety of what we got into the input channel, the output would be non-deterministic.
        int64_t nextAudioChunkSize = getNextChunkSize();

        if (nextAudioChunkSize == 0) break;

        for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
            Vector audioChunk = resampledAudio[channelIdx].segment(0, nextAudioChunkSize);
            resampledAudio[channelIdx] = (Vector)resampledAudio[channelIdx].tail(resampledAudio[channelIdx].size() - audioChunk.size());

            // Zero-mean
            Vector normAudioChunk = zeroMean[channelIdx]->normalize(audioChunk);
            // Pre-emphasize
            Vector preempAudio;
            if (mPreemphasisFactor != 0.0f) {
                preempAudio = preemphasize(normAudioChunk, mPreemphasisFactor);
            } else {
                preempAudio = normAudioChunk;
            }

            if (channelIdx == 0) mUttProducedAudio += preempAudio.size();
            outTimestamp = mUttStartStreamOffset + (int64_t)round(outputTimePerSample*mUttProducedAudio) - 1;
            if (convStateMsg->mLastChunkInUtt && getNextChunkSize() == 0) {
                outTimestamp = convStateMsg->getTime();
            }

            outAudio[channelIdx].conservativeResize(outAudio[channelIdx].size() + preempAudio.size());
            outAudio[channelIdx].tail(preempAudio.size()) = preempAudio;

            // Update the statistics
            if (mUttProducedAudio % mUpdateStatsHop == 0) {
                Vector newChunk = mStatsAccumAudio[channelIdx].segment(0, mUpdateStatsHop);
                mStatsAccumAudio[channelIdx] = (Vector)mStatsAccumAudio[channelIdx].tail(mStatsAccumAudio[channelIdx].size() - newChunk.size());
                zeroMean[channelIdx]->addData(newChunk);
            }
        }
    }
    // Rescale output
    for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
        for(int sampleIdx = 0; sampleIdx < outAudio[channelIdx].size(); sampleIdx++) {
            outAudio[channelIdx][sampleIdx] *= mOutputScale;
        }
    }
    // Output each channel separately
    for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
        if (outAudio[channelIdx].size() == 0) continue;
        std::stringstream ss;
        ss << SlotStreamedAudio << "_" << channelIdx;
        auto outputSlots = getOutputSlots();
        if (outputSlots.find(ss.str()) == outputSlots.end()) GODEC_ERR << "Trying to output audio stream " << ss.str() << ", but the output has not been defined";
        auto outMsg = AudioDecoderMessage::create(outTimestamp, outAudio[channelIdx].data(), outAudio[channelIdx].size(), mTargetSamplingRate, outputTimePerSample);
        (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(audioBaseMsg->getFullDescriptorString());
        pushToOutputs(ss.str(), outMsg);
    }

    // End of utterance? Reset everything
    if (convStateMsg->mLastChunkInUtt) {
        mUttStartStreamOffset = outTimestamp + 1;
        mUttReceivedRawAudio = 0;
        mUttReceivedResampledAudio = 0;
        mUttProducedAudio = 0;
        for (int channelIdx = 0; channelIdx < numChannels; channelIdx++) {
            zeroMean[channelIdx]->reset();
            if (resample[channelIdx] != nullptr) resample[channelIdx]->Reset();
        }
    }
}

}
