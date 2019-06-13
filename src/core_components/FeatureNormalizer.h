#pragma once
#include <godec/ChannelMessenger.h>
#include "GodecMessages.h"
#include "AccumCovariance.h"

namespace Godec {

class Normalizer {
  public:
    virtual void previewData(const Matrix& data, bool lastInShow) = 0;
    virtual Matrix transform(const Matrix& data, bool lastInShow) = 0;
    virtual void endOfUtterance() = 0;
    virtual void endOfConvo() = 0;
};

class NFrameAverager : public Normalizer {
  Matrix mAccum;
  int64_t mNFrames;
  bool mZeroPadToSize;
  public:
    NFrameAverager(int64_t nFrames, int64_t vectorSize, bool zeroPadToSize);
    virtual ~NFrameAverager();
    void previewData(const Matrix& data, bool lastInShow);
    Matrix transform(const Matrix& data, bool lastInShow);
    void endOfUtterance();
    void endOfConvo();
};

class L2Normalizer : public Normalizer {
  public:
    L2Normalizer();
    virtual ~L2Normalizer();
    void previewData(const Matrix& data, bool lastInShow);
    Matrix transform(const Matrix& data, bool lastInShow);
    void endOfUtterance();
    void endOfConvo();
};

class LogLimiter : public Normalizer {
    Vector mMax, mMin;
  public:
    LogLimiter(int vectorSize);
    virtual ~LogLimiter();
    void previewData(const Matrix& data, bool lastInShow);
    Matrix transform(const Matrix& data, bool lastInShow);
    void endOfUtterance();
    void endOfConvo();
};

class AccumCovarianceNormalizer : public Normalizer {
    boost::shared_ptr<AccumCovariance> onlVariance;
  public:
    AccumCovarianceNormalizer(int cepstraSize_, CovarianceType covarType, bool normMean, bool normVars, float decayRate);
    virtual ~AccumCovarianceNormalizer();
    void previewData(const Matrix& data, bool lastInShow);
    Matrix transform(const Matrix& data, bool lastInShow);
    void endOfUtterance();
    void endOfConvo();
};

class FeatureNormalizerComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    FeatureNormalizerComponent(std::string id, ComponentGraphConfig* configPt);
    ~FeatureNormalizerComponent();

  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    ProcessingMode mProcMode;
    int mExpectedFeatureSize;
    std::vector<uint64_t> mFeatureTimestampsBuffer;
    int64_t mUpdateStatsHop;
    Matrix mAccumFeats;

    boost::shared_ptr<Normalizer> normalizer;
    std::list<std::tuple<std::string,uint64_t,Matrix>> uttId2AccumData;
};

}
