#include "NoiseAdd.h"
#include <godec/ComponentGraph.h>

namespace Godec {

BoxMuller::BoxMuller() : y2(), use_last() {}

float BoxMuller::ranf() { /* ranf() is uniform in 0..1 */
    return float(rand())/RAND_MAX;
}

// normal random variate generator
float BoxMuller::of(float m /* mean */, float s /* standard deviation */) {
    float x1, x2, w, y1;

    if (use_last) { // use value from previous call
        y1 = y2;
        use_last = false;
    } else {
        do {
            x1 = 2.0 * ranf() - 1.0;
            x2 = 2.0 * ranf() - 1.0;
            w = x1 * x1 + x2 * x2;
        } while ( w >= 1.0 );

        w = sqrt( (-2.0 * log( w ) ) / w );
        y1 = x1 * w;
        y2 = x2 * w;
        use_last = true;
    }

    return( m + y1 * s );
}

NoiseAddComponent::~NoiseAddComponent() {
}

LoopProcessor* NoiseAddComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new NoiseAddComponent(id, configPt);
}
std::string NoiseAddComponent::describeThyself() {
    return "Adds noise to audio stream";
}
NoiseAddComponent::NoiseAddComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id,configPt) {
    addInputSlotAndUUID(SlotStreamedAudio, UUID_AudioDecoderMessage);
    noiseFactor = configPt->get<float>("noise_scaling_factor", "Scale factor, in relation to incoming audio, of noise. 1.0 = 0dB noise, lower is quieter");
    onlVariance = AccumCovariance::make(1, Diagonal, true, true);

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotStreamedAudio);
    initOutputs(requiredOutputSlots);
}

void NoiseAddComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto audioMsg = msgBlock.get<AudioDecoderMessage>(SlotStreamedAudio);

    // Utterance start?
    if (currentUttId != convStateMsg->mUtteranceId) {
        currentUttId = convStateMsg->mUtteranceId;
    }

    double waveStdDev = 0.0;
    const Vector& audio = audioMsg->mAudio;
    onlVariance->addData(audio);
    waveStdDev = sqrt(onlVariance->getCovariance(false)(0, 0));
    if (waveStdDev == 0.0) {
        waveStdDev = 0.001;
    }
    srand(0);  /*Seeds the random number generator  */

    Vector noisyAudio = audio; // Copy, not reference

    BoxMuller boxMuller;
    for (int64_t i = 0; i < noisyAudio.cols(); i++) {
        noisyAudio(i) += noiseFactor*boxMuller.of(0, waveStdDev);
    }

    DecoderMessage_ptr msg = AudioDecoderMessage::create(convStateMsg->getTime(), noisyAudio.data(), noisyAudio.size(), audioMsg->mSampleRate, audioMsg->mTicksPerSample);
    (boost::const_pointer_cast<DecoderMessage>(msg))->setFullDescriptorString(audioMsg->getFullDescriptorString());
    pushToOutputs(SlotStreamedAudio, msg);
}

}
