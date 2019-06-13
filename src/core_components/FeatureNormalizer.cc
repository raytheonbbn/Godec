#include "FeatureNormalizer.h"
#include <godec/ComponentGraph.h>

namespace Godec {

NFrameAverager::NFrameAverager(int64_t nFrames, int64_t vectorSize, bool zeroPadToSize) {
    mNFrames = nFrames;
    mZeroPadToSize = zeroPadToSize;
    mAccum = Matrix(vectorSize,0);
}

NFrameAverager::~NFrameAverager() {}

void NFrameAverager::endOfUtterance() {
    mAccum = Matrix(mAccum.rows(),0);
}

void NFrameAverager::endOfConvo() {
}

void NFrameAverager::previewData(const Matrix& data, bool lastInShow) {
}

Matrix NFrameAverager::transform(const Matrix& data, bool lastInShow) {
    for(int dataColIdx = 0; dataColIdx < data.cols(); dataColIdx++) {
        if (mAccum.cols() == mNFrames) {
            mAccum = mAccum.rightCols(mAccum.cols()-1);
        }
        mAccum.conservativeResize(mAccum.rows(), mAccum.cols()+1);
        mAccum.col(mAccum.cols()-1) = data.col(dataColIdx);
    }
    Matrix padMatrix = mAccum;
    if (mZeroPadToSize && padMatrix.cols() < mNFrames) {
        padMatrix.conservativeResizeLike(Matrix::Zero(mAccum.rows(), mNFrames));
    }
    Matrix out = padMatrix.rowwise().sum()/padMatrix.cols();
    return out;
}

L2Normalizer::L2Normalizer() {
}

L2Normalizer::~L2Normalizer() {}

void L2Normalizer::endOfUtterance() {
}

void L2Normalizer::endOfConvo() {
}

void L2Normalizer::previewData(const Matrix& data, bool lastInShow) {
}

Matrix L2Normalizer::transform(const Matrix& data, bool lastInShow) {
    Matrix outData = data;
    for (int colIdx = 0; colIdx < data.cols(); colIdx++) {
        outData.col(colIdx) = data.col(colIdx)/data.col(colIdx).norm();
    }
    return outData;
}

LogLimiter::LogLimiter(int vectorSize) : mMax(vectorSize), mMin(vectorSize) {
    endOfUtterance();
    endOfConvo();
}

LogLimiter::~LogLimiter() {}

void LogLimiter::endOfUtterance() {
}

void LogLimiter::endOfConvo() {
    mMax.setConstant(-1.0e+30f);
}

void LogLimiter::previewData(const Matrix& data, bool lastInShow) {
    static const float silFloor = 70.0f;
    mMax = mMax.cwiseMax(data.rowwise().maxCoeff());
    mMin = mMax.array() - silFloor;
}

Matrix LogLimiter::transform(const Matrix& data, bool lastInShow) {
    static const float escale = 0.5f;

    Matrix outData = data;
    for (int colIdx = 0; colIdx < data.cols(); colIdx++) {
        outData.col(colIdx) = ((data.col(colIdx).cwiseMax(mMin) - mMax) * escale).array() + 1.0f;
    }
    return outData;
}


AccumCovarianceNormalizer::AccumCovarianceNormalizer(int cepstraSize_, CovarianceType covarType, bool normMean, bool normVars, float decayRate) {
    onlVariance = AccumCovariance::make(cepstraSize_, covarType, normMean, normVars, decayRate);
}
AccumCovarianceNormalizer::~AccumCovarianceNormalizer() {}

void AccumCovarianceNormalizer::endOfUtterance() {
}

void AccumCovarianceNormalizer::endOfConvo() {
    onlVariance->reset();
}

void AccumCovarianceNormalizer::previewData(const Matrix& data, bool lastInShow) {
    onlVariance->addData(data);
}

Matrix AccumCovarianceNormalizer::transform(const Matrix& data, bool lastInShow) {
    return onlVariance->normalize(data);
}

LoopProcessor* FeatureNormalizerComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new FeatureNormalizerComponent(id, configPt);
}
std::string FeatureNormalizerComponent::describeThyself() {
    return "Normalizes a feature stream with various algorithms (covariance, diagonal etc)";
}

/* FeatureNormalizerComponent::ExtendedDescription
This normalization component supports 4 different types of "normalization":

"covariance:" This builds up a proper covariance matrix (full, or only diagonal) and normalizes the features according to it. The normalization can be selectively be means normalization ("norm_means") and vars, i.e. covariance normalization ("norm_vars"). "decay_num_frames" introduces a "memory decay" through a simple exponential decay

"L2": Normalize each feature column vector with its L2 norm

"n_frame_average": Maybe not really "normalization", but it combines n frames (from "update_stats_every_n_frames") into one averaged output vector. So, it produces one output vector every n input vectors. "zero_pad_input_to_n_frames" specifies if for the case of to little data at the very end (i.e. < N frames), whether to zero-pad it to N frames, or rather averages over the frames we have.

"log_limiter": Not really quite sure what it does, it seems to normalize between max and min with some scaling factor. Look at the source code for more information.
*/
FeatureNormalizerComponent::FeatureNormalizerComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id,configPt) {
    addInputSlotAndUUID(SlotFeatures, UUID_FeaturesDecoderMessage);

    mFeatureTimestampsBuffer.resize(0);

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotFeatures);
    initOutputs(requiredOutputSlots);

    mExpectedFeatureSize = configPt->get<int>("feature_size", "incoming features size");

    mProcMode = StringToProcessMode(configPt->get<std::string>("processing_mode", "Whether to process in batch or low-latency mode (Batch, LowLatency)"));

    if (mProcMode == LowLatency) {
        mUpdateStatsHop = configPt->get<int64_t>("update_stats_every_n_frames", "In LowLatency mode, with what frequency to update the statistics. Lower value = lower latency, but also more CPU.");
        mAccumFeats = Matrix(0,0);
    }

    std::string normalizationTypeString = configPt->get<std::string>("normalization_type", "Normalization type (covariance, log_limiter, L2, n_frame_average)");
    if (normalizationTypeString == "covariance") {
        bool normalizeMeans = configPt->get<bool>("norm_means", "Normalize means");
        bool normalizeVariances = configPt->get<bool>("norm_vars", "Normalize variances");
        CovarianceType covarType = CovarianceType::Full;
        std::string covarTypeString = configPt->get<std::string>("covariance_type", "Full or only diagonal covariance normalization (full, diagonal)");
        if (covarTypeString == "full") {
            covarType = CovarianceType::Full;
        } else if (covarTypeString == "diagonal") {
            covarType = CovarianceType::Diagonal;
        } else GODEC_ERR << getLPId() << ": Unknown covariance type " << covarTypeString;

        int64_t decayNumFrames = configPt->get<int64_t>("decay_num_frames", "Frame window size for 1/e dropoff of memory (set <= 0 for infinite memory)");
        float decayFactor = decayNumFrames <= 0 ? 1.0f : exp(-1.0f / (float)decayNumFrames);
        normalizer = boost::shared_ptr<AccumCovarianceNormalizer>(new AccumCovarianceNormalizer(mExpectedFeatureSize, covarType, normalizeMeans, normalizeVariances, decayFactor));
    } else if (normalizationTypeString == "log_limiter") {
        normalizer = boost::shared_ptr<LogLimiter>(new LogLimiter(mExpectedFeatureSize));
    } else if (normalizationTypeString == "L2") {
        normalizer = boost::shared_ptr<L2Normalizer>(new L2Normalizer());
    } else if (normalizationTypeString == "n_frame_average") {
        bool zeroPadToSize = configPt->get<bool>("zero_pad_input_to_n_frames", "Whether to zero-pad the input matrix to have the size of 'update_stats_every_n_frames'");
        normalizer = boost::shared_ptr<NFrameAverager>(new NFrameAverager(mUpdateStatsHop, mExpectedFeatureSize, zeroPadToSize));
    } else GODEC_ERR << getLPId() << ": Unknown normalization type " << normalizationTypeString;
}

FeatureNormalizerComponent::~FeatureNormalizerComponent() {}

void FeatureNormalizerComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto convStateMsg = msgBlock.get<ConversationStateDecoderMessage>(SlotConversationState);
    auto featMsg = msgBlock.get<FeaturesDecoderMessage>(SlotFeatures);
    if (featMsg->mFeatures.rows() != mExpectedFeatureSize) GODEC_ERR << getLPId() << ": Mismatch of feature size. Expecting size " << mExpectedFeatureSize << ", got " << featMsg->mFeatures.rows();

    mFeatureTimestampsBuffer.insert(mFeatureTimestampsBuffer.end(),
                                    featMsg->mFeatureTimestamps.begin(), featMsg->mFeatureTimestamps.end());

    // Annoying historical exceptions
    std::string outFeatureNames;
    if (featMsg->mFeatureNames == "R0%f") {
        outFeatureNames = "NORM%f";
    } else if (featMsg->mFeatureNames.substr(0, strlen("CEPSMELW")) == "CEPSMELW") {
        outFeatureNames = "N" + featMsg->mFeatureNames;
    } else {
        outFeatureNames = featMsg->mFeatureNames;
    }

    const Matrix& m = featMsg->mFeatures;
    if (mProcMode == LowLatency) {
        if (mAccumFeats.rows() == 0) mAccumFeats.resize(m.rows(),0);
        mAccumFeats.conservativeResize(mAccumFeats.rows(), mAccumFeats.cols()+m.cols());
        mAccumFeats.rightCols(m.cols()) = m;
        std::vector<uint64_t> featureTimestamps;
        Matrix normM = Matrix(mAccumFeats.rows(), 0);
        while(mAccumFeats.cols() / mUpdateStatsHop > 0 || (mAccumFeats.cols() > 0 && convStateMsg->mLastChunkInUtt)) {
            int pickupToCol = (std::min((int)mUpdateStatsHop, (int)mAccumFeats.cols()));
            Matrix subMatrix = mAccumFeats.block(0, 0, mAccumFeats.rows(), pickupToCol);

            normalizer->previewData(subMatrix, convStateMsg->mLastChunkInUtt);
            Matrix normSubMatrix = normalizer->transform(subMatrix, convStateMsg->mLastChunkInUtt);

            normM.conservativeResize(normM.rows(), normM.cols()+normSubMatrix.cols());
            normM.rightCols(normSubMatrix.cols()) = normSubMatrix;

            if (subMatrix.cols() % normSubMatrix.cols() != 0) GODEC_ERR << getLPId() << ": The features going into the normalizer are not an integer multiple of the number of features coming out. Impossible to choose the right timestamps";
            for(int timeIdx = 0; timeIdx < normSubMatrix.cols(); timeIdx++) {
                featureTimestamps.push_back(mFeatureTimestampsBuffer[(timeIdx+1)*(subMatrix.cols()/normSubMatrix.cols())-1]);
            }

            mAccumFeats = (Matrix)mAccumFeats.rightCols(mAccumFeats.cols()-pickupToCol);
            mFeatureTimestampsBuffer.erase(mFeatureTimestampsBuffer.begin(),
                                           mFeatureTimestampsBuffer.begin() + pickupToCol);
        }

        if (normM.cols() > 0) {
            uint64_t time = featureTimestamps.back();

            DecoderMessage_ptr outMsg = FeaturesDecoderMessage::create(
                                            time, convStateMsg->mUtteranceId,
                                            normM, outFeatureNames, featureTimestamps);

            (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(featMsg->getFullDescriptorString());

            pushToOutputs(SlotFeatures, outMsg);
        }
        if (convStateMsg->mLastChunkInUtt) {
            if (!mFeatureTimestampsBuffer.empty() || mAccumFeats.cols() != 0)
                GODEC_ERR << "frame timestamps buffer and feature buffer are expected to be completely erased at the utterance end in the low-latency mode." << std::endl;
        }
    } else if (mProcMode == Batch) {
        normalizer->previewData(m, convStateMsg->mLastChunkInUtt);
        if (uttId2AccumData.size() == 0 || std::get<0>(uttId2AccumData.back()) != convStateMsg->mUtteranceId) {
            uttId2AccumData.push_back(std::make_tuple(convStateMsg->mUtteranceId, convStateMsg->getTime(), Matrix(0, 0)));
        }
        auto& tuple = uttId2AccumData.back();
        Matrix& accumData = std::get<2>(tuple);
        std::get<1>(tuple) = convStateMsg->getTime();
        accumData.conservativeResize(m.rows(), accumData.cols() + m.cols());
        accumData.rightCols(m.cols()) = m;

        if (convStateMsg->mLastChunkInConvo) {
            for (auto tupleIt = uttId2AccumData.begin(); tupleIt != uttId2AccumData.end(); tupleIt++) {
                std::string uttId = std::get<0>(*tupleIt);
                Matrix normM = normalizer->transform(std::get<2>(*tupleIt), true);
                std::vector<uint64_t> featureTimestamps;
                featureTimestamps.insert(featureTimestamps.end(),
                                         mFeatureTimestampsBuffer.begin(),
                                         mFeatureTimestampsBuffer.begin() + normM.cols());
                mFeatureTimestampsBuffer.erase(mFeatureTimestampsBuffer.begin(),
                                               mFeatureTimestampsBuffer.begin() + normM.cols());
                uint64_t time = featureTimestamps.back();

                DecoderMessage_ptr outMsg = FeaturesDecoderMessage::create(
                                                time, uttId, normM, outFeatureNames, featureTimestamps);
                (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(featMsg->getFullDescriptorString());
                pushToOutputs(SlotFeatures, outMsg);
            }

            uttId2AccumData.clear();

            if (!mFeatureTimestampsBuffer.empty())
                GODEC_ERR << "frame timestamps buffer is expected to be completely erased at the conversation end in the batch mode." << std::endl;

        }
    }
    if (convStateMsg->mLastChunkInUtt) {
        normalizer->endOfUtterance();
    }
    if (convStateMsg->mLastChunkInConvo) {
        normalizer->endOfConvo();
    }
}

}
