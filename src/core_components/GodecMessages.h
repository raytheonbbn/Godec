#pragma once

#include <string>
#include <vector>
#include "boost/function.hpp"
#include <godec/json.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/uuid/string_generator.hpp>
#include <godec/TimeStream.h>
#include <godec/ChannelMessenger.h>
#include <jni.h>
#undef PAGE_SIZE
#undef PAGE_MASK
#include "boost/thread/thread.hpp"


namespace Godec {

extern uuid UUID_AnyDecoderMessage;
extern uuid UUID_ConversationStateDecoderMessage;
extern uuid UUID_AudioDecoderMessage;
extern uuid UUID_FeaturesDecoderMessage;
extern uuid UUID_MatrixDecoderMessage;
extern uuid UUID_NbestDecoderMessage;
extern uuid UUID_BinaryDecoderMessage;
extern uuid UUID_JsonDecoderMessage;
extern uuid UUID_AudioInfoDecoderMessage;

enum ProcessingMode {
    LowLatency,
    Batch,
    DummyBatch,
    DummyLowLatency
};

ProcessingMode StringToProcessMode(std::string s);

class AudioDecoderMessage : public DecoderMessage {
  public:
    Vector mAudio;
    float mSampleRate;
    float mTicksPerSample;

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;
    static DecoderMessage_ptr create(uint64_t time, const float* audioData, unsigned int numSamples, float sampleRate, float ticksPerSample);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif
    static DecoderMessage_ptr fromCSharp(_AUDIODECODERMESSAGESTRUCT cMsg);

    uuid getUUID() const  { return UUID_AudioDecoderMessage; }
    static uuid getUUIDStatic() { return UUID_AudioDecoderMessage; }

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mSampleRate;
        ar & mAudio;
        ar & mTicksPerSample;
    }
};

class AudioInfoDecoderMessage : public DecoderMessage {
  public:
    float mSampleRate;
    float mTicksPerSample;

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;

    static DecoderMessage_ptr create(uint64_t time, float sampleRate, float ticksPerSample);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif

    uuid getUUID() const  { return UUID_AudioInfoDecoderMessage; }
    static uuid getUUIDStatic() { return UUID_AudioInfoDecoderMessage; }

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mSampleRate;
        ar & mTicksPerSample;
    }
};

class FeaturesDecoderMessage : public DecoderMessage {
  public:
    Matrix mFeatures;
    std::vector<uint64_t> mFeatureTimestamps;
    std::string mFeatureNames;
    std::string mUtteranceId; // This is only here so that don't merge on utterance boundaries in the stream

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;

    static DecoderMessage_ptr create(uint64_t time, std::string utteranceId, Matrix feats, std::string featureNames, std::vector<uint64_t> _featureTimestamps);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif

    uuid getUUID() const  { return UUID_FeaturesDecoderMessage;};
    static uuid getUUIDStatic() { return UUID_FeaturesDecoderMessage;};

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mFeatureTimestamps;
        ar & mFeatureNames;
        ar & mUtteranceId;
        ar & mFeatures;
    }
};

class MatrixDecoderMessage : public DecoderMessage {
  public:
    Matrix mMat;

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;

    static DecoderMessage_ptr create(uint64_t time, Matrix mat);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif

    uuid getUUID() const  { return UUID_MatrixDecoderMessage; }
    static uuid getUUIDStatic() { return UUID_MatrixDecoderMessage; }

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version)  {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mMat;
    }
};

class NbestDecoderMessage : public DecoderMessage {
  public:
    std::vector<std::vector<uint64_t>> mWords;
    std::vector<std::vector<uint64_t>> mAlignment;
    std::vector<std::vector<std::string>> mText;
    std::vector<std::vector<float>> mConfidences;

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;

    static DecoderMessage_ptr create(uint64_t time, std::vector<std::vector<std::string>> text, std::vector<std::vector<uint64_t>> words, std::vector<std::vector<uint64_t>> alignment, std::vector<std::vector<float>> confidences);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif

    uuid getUUID() const  { return UUID_NbestDecoderMessage; }
    static uuid getUUIDStatic() { return UUID_NbestDecoderMessage; }

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version)  {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mWords;
        ar & mAlignment;
        ar & mText;
        ar & mConfidences;
    }
};

class ConversationStateDecoderMessage : public DecoderMessage {
  public:
    std::string mUtteranceId;
    bool mLastChunkInUtt;
    std::string mConvoId;
    bool mLastChunkInConvo;

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;

    static DecoderMessage_ptr create(uint64_t time, std::string utteranceId, bool isLastChunkInUtt, std::string convoId, bool isLastChunkInConvo);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif
    static DecoderMessage_ptr fromCSharp(_CONVODECODERMESSAGESTRUCT cMsg);

    uuid getUUID() const  { return UUID_ConversationStateDecoderMessage; }
    static uuid getUUIDStatic() { return UUID_ConversationStateDecoderMessage; }

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mUtteranceId;
        ar & mLastChunkInUtt;
        ar & mConvoId;
        ar & mLastChunkInConvo;
    }
};

class BinaryDecoderMessage : public DecoderMessage {
  public:
    std::vector<unsigned char> mData;
    std::string mFormat;

    std::string describeThyself() const;
    DecoderMessage_ptr clone() const;

    static DecoderMessage_ptr create(uint64_t time, std::vector<unsigned char> data, std::string format);
    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose);
    bool canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose);
    bool sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose);
    void shiftInTime(int64_t deltaT);
    jobject toJNI(JNIEnv* env);
    _DECODERMESSAGESTRUCT* toCSHARP();
    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif
    static DecoderMessage_ptr fromCSharp(_BINARYDECODERMESSAGESTRUCT cMsg);

    uuid getUUID() const { return UUID_BinaryDecoderMessage; }
    static uuid getUUIDStatic() { return UUID_BinaryDecoderMessage; }

  private:
    friend class boost::serialization::access;
    template<typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        ar & mFormat;
        ar & mData;
    }
};

/**
 * A generic message that carries around data as a JSON object.
 * Current JSON implementation uses JSON for Modern C++ (https://github.com/nlohmann/json)
 *
 * See CTMComponent for an example of passing data out as a JsonDecoderMessage.
 * See FileWriterComponent for an example of reading from a JsonDecoderMessage
 */
class JsonDecoderMessage : public DecoderMessage {
  public:
    ~JsonDecoderMessage() {};

    std::string describeThyself() const override;
    DecoderMessage_ptr clone() const override;

    jobject toJNI(JNIEnv *env) override;

    _DECODERMESSAGESTRUCT* toCSHARP() override;

    static DecoderMessage_ptr fromJNI(JNIEnv* env, jobject jMsg);
#ifndef ANDROID
    PyObject* toPython();
    static DecoderMessage_ptr fromPython(PyObject* pMsg);
#endif

    bool mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) override;
    bool sliceOut(uint64_t sliceTime,
                  DecoderMessage_ptr &sliceMsg,
                  std::vector<DecoderMessage_ptr> &streamList,
                  int64_t streamStartOffset,
                  bool verbose) override;
    bool canSliceAt(uint64_t sliceTime,
                    std::vector<DecoderMessage_ptr> &streamList,
                    uint64_t streamStartOffset,
                    bool verbose) override;
    void shiftInTime(int64_t deltaT);
    uuid getUUID() const override { return UUID_JsonDecoderMessage; };
    static uuid getUUIDStatic() { return UUID_JsonDecoderMessage; };
    json getJsonObj() const { return mJson; }
    void setJsonObj(const json& rhs) { mJson = rhs; }
    static DecoderMessage_ptr create(uint64_t time, json& jsonObj);

  private:
    json mJson;

    friend class boost::serialization::access;
    template<class Archive>
    void save(Archive & ar, const unsigned int version) const {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        std::string s = mJson.dump(2);
        ar & s;
    }

    template<class Archive>
    void load(Archive & ar, const unsigned int version) {
        ar & boost::serialization::base_object<DecoderMessage>(*this);
        std::string s;
        ar & s;
        mJson = json::parse(s);
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

}

// serialize Eigen matrix: http://stackoverflow.com/a/22903063
namespace boost {
namespace serialization {
template<class Archive,
         class S,
         int Rows_,
         int Cols_,
         int Ops_,
         int MaxRows_,
         int MaxCols_>
inline void save(
    Archive &ar,
    const Eigen::Matrix<S, Rows_, Cols_, Ops_, MaxRows_, MaxCols_> &g,
    const unsigned int version) {
    Eigen::DenseIndex rows = g.rows();
    Eigen::DenseIndex cols = g.cols();

    ar & rows;
    ar & cols;
    ar & ::boost::serialization::make_array(g.data(), rows * cols);
}

template<class Archive,
         class S,
         int Rows_,
         int Cols_,
         int Ops_,
         int MaxRows_,
         int MaxCols_>
inline void load(
    Archive &ar,
    Eigen::Matrix<S, Rows_, Cols_, Ops_, MaxRows_, MaxCols_> &g,
    const unsigned int version) {
    Eigen::DenseIndex rows, cols;
    ar & rows;
    ar & cols;
    g.resize(rows, cols);
    ar & ::boost::serialization::make_array(g.data(), rows * cols);
}

template<class Archive,
         class S,
         int Rows_,
         int Cols_,
         int Ops_,
         int MaxRows_,
         int MaxCols_>
inline void serialize(
    Archive &ar,
    Eigen::Matrix<S, Rows_, Cols_, Ops_, MaxRows_, MaxCols_> &g,
    const unsigned int version) {
    split_free(ar, g, version);
}

}
}


