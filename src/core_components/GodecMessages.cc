#include "GodecMessages.h"
#include <godec/HelperFuncs.h>
#include <boost/format.hpp>
#ifndef ANDROID
#define PY_ARRAY_UNIQUE_SYMBOL GODEC_MESSAGES_ARRAY_API
#include <numpy/arrayobject.h>
#endif

namespace Godec {

string_generator godec_uuid_gen;
// From https://www.uuidgenerator.net/
uuid UUID_AnyDecoderMessage = godec_uuid_gen("00000000-0000-0000-0000-000000000000");
uuid UUID_ConversationStateDecoderMessage = godec_uuid_gen("8daa46ce-6253-4bfb-b6af-d9c2fa9ae3c2");
uuid UUID_AudioDecoderMessage = godec_uuid_gen("6ed34b01-8032-4895-9211-6cc913e514ee");
uuid UUID_FeaturesDecoderMessage = godec_uuid_gen("f7e7f3d0-0798-4ae6-9c3b-375ac645b127");
uuid UUID_MatrixDecoderMessage = godec_uuid_gen("38bb96d4-42c7-4797-a4af-a0eda018cf9e");
uuid UUID_NbestDecoderMessage = godec_uuid_gen("e01bbc32-73ad-40d9-9ad6-ce491a5b02c4");
uuid UUID_TimeMapDecoderMessage = godec_uuid_gen("c335ce22-97ae-4b07-b952-6c509e27e6c3");
uuid UUID_BinaryDecoderMessage = godec_uuid_gen("e754362e-58da-4488-831e-9277f8be1d66");
uuid UUID_JsonDecoderMessage = godec_uuid_gen("ebe880c8-f6d9-4b7d-8d15-7589302b6946");
uuid UUID_AudioInfoDecoderMessage = godec_uuid_gen("b1464c2d-8d1e-4e75-bb4e-83632b1201d0");

ProcessingMode StringToProcessMode(std::string s) {
    if (s == "LowLatency") return LowLatency;
    else if (s == "Batch") return Batch;
    else GODEC_ERR << "Unknown processing mode";
    return LowLatency; // Dummy, just to get rid of compiler warning
}

#ifndef ANDROID
int GodecMessages_init_numpy() {
    import_array();
}
#endif

/*
############ Audio decoder message ###################
*/

std::string AudioDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Audio, " << mAudio.size() << " samples, sampleRate " << mSampleRate << ", ticksPerSample " << mTicksPerSample << ", RMS=" << mAudio.norm() << ", desc:";
    ss << getFullDescriptorString() << std::endl;
    return ss.str();
}

DecoderMessage_ptr AudioDecoderMessage::create(uint64_t _time, const float* _audioData, unsigned int _numSamples, float _sampleRate, float _ticksPerSample) {
    if (_numSamples == 0) {
        GODEC_ERR << "Can not create AudioDecoderMessage from empty data (numSamples=0).";
    }
    AudioDecoderMessage* msg = new AudioDecoderMessage();
    msg->setTime(_time);
    msg->mSampleRate = _sampleRate;
    msg->mTicksPerSample = _ticksPerSample;
    msg->mAudio = Eigen::Map<Vector>((float*)_audioData, _numSamples);
    return DecoderMessage_ptr(msg);
}



DecoderMessage_ptr AudioDecoderMessage::clone() const { return  DecoderMessage_ptr(new AudioDecoderMessage(*this)); }

bool AudioDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    auto newAudioMsg = boost::static_pointer_cast<const AudioDecoderMessage>(msg);
    bool canBeMerged = true;
    if (mSampleRate != newAudioMsg->mSampleRate) canBeMerged = false;
    if (mTicksPerSample != newAudioMsg->mTicksPerSample) canBeMerged = false;

    if (getFullDescriptorString() != newAudioMsg->getFullDescriptorString()) canBeMerged = false;

    if (!canBeMerged) {
        remainingMsg = msg;
        return true;
    }
    const Vector& newAudio = newAudioMsg->mAudio;
    mAudio.conservativeResize(mAudio.size() + newAudio.size());
    mAudio.tail(newAudio.size()) = newAudio;
    setTime(msg->getTime());
    return false;
}

bool AudioDecoderMessage::canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    // We are returning to the stringent check even though fractional ticksPerSample due to downsamplig can cause problems when trying to slice out streams that came from different ticksPerSample (e.g. slicing out features computed from 44.1kHz and 10kHz). The solution has to rather be to create a custom component that "resamples" the features so they align the timestamps with each other
    bool canSlice = (((int64_t)getTime()-(int64_t)sliceTime) % (int64_t)round(mTicksPerSample) == 0);
    return canSlice;
}

bool AudioDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<AudioDecoderMessage>(boost::const_pointer_cast<DecoderMessage>(msgList[0]));
    uint64_t messageTimeLength = firstMsg->getTime() - streamStartOffset;
    uint64_t timeToSliceOutOfMessage = sliceTime - streamStartOffset;
    double frac = (getTime() == (int64_t)sliceTime) ? 1.0 : (double)timeToSliceOutOfMessage / (double)messageTimeLength;
    uint64_t audioToRemove = (uint64_t)round(frac*firstMsg->mAudio.size());
    sliceMsg = AudioDecoderMessage::create(sliceTime, firstMsg->mAudio.data(), audioToRemove, firstMsg->mSampleRate, firstMsg->mTicksPerSample);
    (boost::const_pointer_cast<DecoderMessage>(sliceMsg))->setFullDescriptorString(firstMsg->getFullDescriptorString());
    auto audioSliceMsg = boost::static_pointer_cast<AudioDecoderMessage>(boost::const_pointer_cast<DecoderMessage>(sliceMsg));
    uint64_t remainingAudioSize = firstMsg->mAudio.size() - audioToRemove;

#ifdef DEBUG
    if (verbose) {
        std::stringstream verboseStr;
        verboseStr << "Slicing out from " << firstMsg->describeThyself()
                   << ", " << sliceTime << " sliceTime"
                   << ", " << streamStartOffset << " streamStartOffset"
                   << ", " << firstMsg->getTime() << " firstMsg->getTime()"
                   << ", " << audioToRemove << " audioToRemove"
                   << ", " << remainingAudioSize << " remainingAudioSize"
                   << std::endl;
        GODEC_INFO << verboseStr.str();
    }
#endif

    if (remainingAudioSize == 0) {
        msgList.erase(msgList.begin());
    } else {
        Matrix tmp = firstMsg->mAudio.tail(remainingAudioSize);
        boost::static_pointer_cast<AudioDecoderMessage>(firstMsg)->mAudio = tmp;
    }
    return true;
}

void AudioDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
}

jobject AudioDecoderMessage::toJNI(JNIEnv* env) {
    jclass AudioDecoderMessageClass = env->FindClass("com/bbn/godec/AudioDecoderMessage");
    jclass DecoderMessageClass = env->FindClass("com/bbn/godec/DecoderMessage");
    jmethodID jAudioMsgInit = env->GetMethodID(AudioDecoderMessageClass, "<init>", "(Ljava/lang/String;JLcom/bbn/godec/Vector;FF)V");
    std::string tag = getTag();
    jstring jTag = env->NewStringUTF(tag.c_str());
    jlong jTime = getTime();
    jobject jAudioVectorObj = CreateJNIVector(env, mAudio);
    jobject jAudioMsg = env->NewObject(AudioDecoderMessageClass, jAudioMsgInit, jTag, jTime, jAudioVectorObj, mSampleRate, mTicksPerSample);

    jstring jDescriptor = env->NewStringUTF(getFullDescriptorString().c_str());
    jmethodID jSetFullDescriptor = env->GetMethodID(DecoderMessageClass, "setFullDescriptorString", "(Ljava/lang/String;)V");
    env->CallVoidMethod(jAudioMsg, jSetFullDescriptor, jDescriptor);
    return jAudioMsg;
};

DecoderMessage_ptr AudioDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    JNIGetDecoderMessageVals(env, jMsg, tag, time, descriptorString);

    jclass AudioDecoderMessageClass = env->FindClass("com/bbn/godec/AudioDecoderMessage");
    jclass VectorClass = env->FindClass("com/bbn/godec/Vector");
    jfieldID jVectorId = env->GetFieldID(AudioDecoderMessageClass, "mAudio", "Lcom/bbn/godec/Vector;");
    jfieldID jAudioVectorDataId = env->GetFieldID(VectorClass, "mData", "[F");
    jobject jAudioVectorObj = env->GetObjectField(jMsg, jVectorId);
    jobject jAudioFloatObject = env->GetObjectField(jAudioVectorObj, jAudioVectorDataId);
    jfloatArray* jAudioFloatArray = reinterpret_cast<jfloatArray*>(&jAudioFloatObject);
    jfloat* audioData = env->GetFloatArrayElements(*jAudioFloatArray, 0);
    jint audioDataSize = env->GetArrayLength(*jAudioFloatArray);
    jfieldID jSampleRateFieldId = env->GetFieldID(AudioDecoderMessageClass, "mSampleRate", "F");
    jfloat jSampleRate = env->GetFloatField(jMsg, jSampleRateFieldId);
    jfieldID jTicksPerSampleFieldId = env->GetFieldID(AudioDecoderMessageClass, "mTicksPerSample", "F");
    jfloat jTicksPerSample = env->GetFloatField(jMsg, jTicksPerSampleFieldId);

    DecoderMessage_ptr outMsg = AudioDecoderMessage::create(time, audioData, audioDataSize, jSampleRate, jTicksPerSample);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    env->ReleaseFloatArrayElements(*jAudioFloatArray, audioData, 0);
    return outMsg;
}

#ifndef ANDROID
PyObject* AudioDecoderMessage::toPython() {
    GodecMessages_init_numpy();
    PyObject* dict = PyDict_New();

    PyDict_SetItemString(dict, "type", PyUnicode_FromString("AudioDecoderMessage"));
    PyDict_SetItemString(dict, "tag", PyUnicode_FromString(getTag().c_str()));
    PyDict_SetItemString(dict, "sample_rate", PyFloat_FromDouble(mSampleRate));
    PyDict_SetItemString(dict, "ticks_per_sample", PyFloat_FromDouble(mTicksPerSample));
    PyDict_SetItemString(dict, "time", PyLong_FromLong(getTime()));
    PyDict_SetItemString(dict, "descriptor", PyUnicode_FromString(getFullDescriptorString().c_str()));

    npy_intp featDims[1] {mAudio.size()};
    PyObject* pAudio = PyArray_SimpleNew(1, featDims, NPY_FLOAT32);
    if (pAudio == NULL) GODEC_ERR << "Could not allocate feature memory";
    for(int idx = 0; idx < mAudio.size(); idx++) {
        *((float*)PyArray_GETPTR1(pAudio, idx)) = mAudio(idx);
    }
    PyDict_SetItemString(dict, "audio", pAudio);
    return dict;
}

DecoderMessage_ptr AudioDecoderMessage::fromPython(PyObject* pMsg) {
    GodecMessages_init_numpy();
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    PythonGetDecoderMessageVals(pMsg, tag, time, descriptorString);

    PyObject* pSampleRate = PyDict_GetItemString(pMsg,"sample_rate");
    if (pSampleRate == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'sample_rate'!";
    float sampleRate = boost::lexical_cast<float>(PyUnicode_AsUTF8(pSampleRate));

    PyObject* pTicksPerSample = PyDict_GetItemString(pMsg,"ticks_per_sample");
    if (pTicksPerSample == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'ticks_per_sample'!";
    float ticksPerSample = boost::lexical_cast<float>(PyUnicode_AsUTF8(pTicksPerSample));

    // matrix
    PyArrayObject* pAudio = (PyArrayObject*)PyDict_GetItemString(pMsg,"audio");
    if (pAudio == NULL) GODEC_ERR << "AudioDecoderMessage::fromPython : Dict does not contain field 'audio'!";
    if (PyArray_NDIM(pAudio) != 1) GODEC_ERR << "AudioDecoderMessage::fromPython : audio element is not one-dimensional! Dimension is " << PyArray_NDIM(pAudio);
    npy_intp* pAudioDims = PyArray_SHAPE(pAudio);
    Vector audioVector(pAudioDims[0]);
    for(int idx = 0; idx < pAudioDims[0]; idx++) {
        audioVector(idx) = *((float*)PyArray_GETPTR1(pAudio, idx));
    }

    DecoderMessage_ptr outMsg = AudioDecoderMessage::create(time, audioVector.data(), audioVector.size(), sampleRate, ticksPerSample);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}
#endif

/*
############ Features decoder message ###################
*/

std::string getTimingString(const std::vector<uint64_t>& t) {
    std::stringstream ss;
    ss << " (";
    for (auto timeIt = t.begin(); timeIt != t.end(); timeIt++) {
        ss << *timeIt << ",";
    }
    ss << ")";
    return ss.str();
}

std::string FeaturesDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Features, " << mFeatures.rows() << "x" << mFeatures.cols() << ", sum=" << mFeatures.sum() << ", pfnames: " << mFeatureNames << ", uttId: " << mUtteranceId;
#if 0
    ss << getTimingString(mFeatureTimestamps);
#endif
    ss << std::endl;
    return ss.str();
}

DecoderMessage_ptr FeaturesDecoderMessage::create(
    uint64_t _time, std::string _utteranceId,
    Matrix _feats, std::string _featureNames, std::vector<uint64_t> _featureTimestamps) {
    if (_feats.cols() != _featureTimestamps.size()) GODEC_ERR << "FeaturesDecoderMessage::create: feature #columns != timestamps size!";
    if (_time != _featureTimestamps.back()) GODEC_ERR << "FeaturesDecoderMessage::create: time != last timestamp entry!";
    if (_feats.cols() == 0) GODEC_ERR << "FeaturesDecoderMessage::create: No empty payload allowed!";
    FeaturesDecoderMessage* msg = new FeaturesDecoderMessage();
    msg->setTime(_time);
    msg->mUtteranceId = _utteranceId;
    msg->mFeatures = _feats;
    msg->mFeatureNames = _featureNames;
    msg->mFeatureTimestamps = _featureTimestamps;
    return DecoderMessage_ptr(msg);
}



DecoderMessage_ptr FeaturesDecoderMessage::clone()  const { return  DecoderMessage_ptr(new FeaturesDecoderMessage(*this)); }
bool FeaturesDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    auto newFeatsMsg = boost::static_pointer_cast<const FeaturesDecoderMessage>(msg);
    if (mUtteranceId == newFeatsMsg->mUtteranceId) { // Convo is assumed to be the same in this case
        const Matrix& newFeats = newFeatsMsg->mFeatures;
        if (mFeatures.rows() != newFeats.rows()) GODEC_ERR << "Trying to merge incompatible features";
        mFeatures.conservativeResize(mFeatures.rows(), mFeatures.cols() + newFeats.cols());
        mFeatures.rightCols(newFeats.cols()) = newFeats;
        mFeatureTimestamps.insert(mFeatureTimestamps.end(),
                                  newFeatsMsg->mFeatureTimestamps.begin(), newFeatsMsg->mFeatureTimestamps.end());
        setTime(msg->getTime());
        return false;
    } else {
        remainingMsg = msg;
        return true;
    }
}

int compareUInt64 (const void * a, const void * b) {
    uint64_t a64 = *(uint64_t*)a;
    uint64_t b64 = *(uint64_t*)b;
    if (a64 > b64) return 1;
    else if (a64 == b64) return 0;
    else return -1;
}

bool FeaturesDecoderMessage::canSliceAt(uint64_t sliceTime,
                                        std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<const FeaturesDecoderMessage>(msgList[0]);
#ifdef DEBUG
    if (verbose) {
        std::stringstream verboseStr;
        verboseStr << "Checking slicing " << firstMsg->describeThyself()
                   << ", " << sliceTime << " sliceTime, "
                   << ", " << firstMsg->getTime() << " firstMsg->getTime(), "
                   << ", " << firstMsg->mFeatureTimestamps.front() << " firstMsg->mFeatureTimestamps.front(), "
                   << ", " << firstMsg->mFeatureTimestamps.back() << " firstMsg->mFeatureTimestamps.back()"
                   << std::endl;
        GODEC_INFO << verboseStr.str();
    }
#endif
    return (bsearch(&sliceTime, &firstMsg->mFeatureTimestamps[0], firstMsg->mFeatureTimestamps.size(), sizeof(uint64_t), compareUInt64) != NULL);
    //return (std::binary_search(firstMsg->mFeatureTimestamps.begin(),
    //	firstMsg->mFeatureTimestamps.end(), sliceTime));
}

bool FeaturesDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg,
                                      std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<FeaturesDecoderMessage>(boost::const_pointer_cast<DecoderMessage>(msgList[0]));
    auto sliceAtFrameTimestampIt = std::lower_bound(
                                       firstMsg->mFeatureTimestamps.begin(), firstMsg->mFeatureTimestamps.end(), sliceTime);
    if (sliceAtFrameTimestampIt == firstMsg->mFeatureTimestamps.end() ||
            *sliceAtFrameTimestampIt != sliceTime) {
        GODEC_ERR << "Attempting to slice at invalid time " << sliceTime << ", msg=" << firstMsg->describeThyself() << std::endl;
    }

    int nRemovedFrames = (int)std::distance(
                             firstMsg->mFeatureTimestamps.begin(), sliceAtFrameTimestampIt) + 1;

    Matrix slicedFeats = (Matrix)firstMsg->mFeatures.leftCols(nRemovedFrames);
    std::vector<uint64_t> featureTimestamps;
    featureTimestamps.insert(featureTimestamps.end(),
                             firstMsg->mFeatureTimestamps.begin(),
                             firstMsg->mFeatureTimestamps.begin() + nRemovedFrames);
    firstMsg->mFeatureTimestamps.erase(
        firstMsg->mFeatureTimestamps.begin(),
        firstMsg->mFeatureTimestamps.begin() + nRemovedFrames);
    uint64_t remainingFeatureSize = firstMsg->mFeatures.cols() - nRemovedFrames;

    if (remainingFeatureSize == 0) {
        msgList.erase(msgList.begin());
    } else {
        firstMsg->mFeatures = (Matrix)firstMsg->mFeatures.rightCols(remainingFeatureSize);
    }

#if 0
    if (verbose) {
        std::stringstream verboseStr;
        verboseStr << "Slicing out from " << firstMsg->describeThyself()
                   << ", " << firstMsg->getTime() << " firstMsg->getTime()"
                   << ", " << nRemovedFrames << " nRemovedFrames"
                   << ", " << remainingFeatureSize << " remainingFeatureSize"
                   << std::endl;
        GODEC_INFO << verboseStr.str();
    }
#endif

    sliceMsg = FeaturesDecoderMessage::create(
                   sliceTime, firstMsg->mUtteranceId,
                   slicedFeats, firstMsg->mFeatureNames, featureTimestamps);
    (boost::const_pointer_cast<DecoderMessage>(sliceMsg))->setFullDescriptorString(firstMsg->getFullDescriptorString());
    auto featSliceMsg = boost::static_pointer_cast<FeaturesDecoderMessage>(boost::const_pointer_cast<DecoderMessage>(sliceMsg));

    return true;
}

void FeaturesDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
    for(int featTimeIdx = 0; featTimeIdx < mFeatureTimestamps.size(); featTimeIdx++) {
        mFeatureTimestamps[featTimeIdx] += deltaT;
    }
}


jobject FeaturesDecoderMessage::toJNI(JNIEnv* env) {
    jclass FeaturesDecoderMessageClass = env->FindClass("com/bbn/godec/FeaturesDecoderMessage");
    jclass DecoderMessageClass = env->FindClass("com/bbn/godec/DecoderMessage");
    jmethodID jFeatMsgInit = env->GetMethodID(FeaturesDecoderMessageClass, "<init>", "(Ljava/lang/String;JLjava/lang/String;Lcom/bbn/godec/Matrix;[JLjava/lang/String;)V");
    std::string tag = getTag();
    jstring jTag = env->NewStringUTF(tag.c_str());
    jstring jUtteranceId = env->NewStringUTF(mUtteranceId.c_str());
    jstring jFeatNames = env->NewStringUTF(mFeatureNames.c_str());
    jlong jTime = getTime();
    jobject jFeatMatrixObj = CreateJNIMatrix(env, mFeatures);
    jlongArray jTimestampsArray = env->NewLongArray((jsize)mFeatureTimestamps.size());
    jlong* tmpArray = new jlong[mFeatureTimestamps.size()];
    for (int idx = 0; idx < mFeatureTimestamps.size(); idx++) {
        tmpArray[idx] = mFeatureTimestamps[idx];
    }
    env->SetLongArrayRegion(jTimestampsArray, 0, (jsize)mFeatureTimestamps.size(), tmpArray);
    delete[] tmpArray;
    jobject jFeatMsg = env->NewObject(FeaturesDecoderMessageClass, jFeatMsgInit, jTag, jTime, jUtteranceId, jFeatMatrixObj, jTimestampsArray, jFeatNames);

    jstring jDescriptor = env->NewStringUTF(getFullDescriptorString().c_str());
    jmethodID jSetFullDescriptor = env->GetMethodID(DecoderMessageClass, "setFullDescriptorString", "(Ljava/lang/String;)V");
    env->CallVoidMethod(jFeatMsg, jSetFullDescriptor, jDescriptor);
    return jFeatMsg;
};

DecoderMessage_ptr FeaturesDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    JNIGetDecoderMessageVals(env, jMsg, tag, time, descriptorString);

    // utterance ID
    jclass FeaturesDecoderMessageClass = env->FindClass("com/bbn/godec/FeaturesDecoderMessage");
    jfieldID jUttFieldId = env->GetFieldID(FeaturesDecoderMessageClass, "mUtteranceId", "Ljava/lang/String;");
    jstring jUttIdObj = (jstring)env->GetObjectField(jMsg, jUttFieldId);
    const char* uttIdChars = env->GetStringUTFChars(jUttIdObj, 0);
    std::string uttId = uttIdChars;
    env->ReleaseStringUTFChars(jUttIdObj, uttIdChars);

    // pfnames
    jfieldID jNamesFieldId = env->GetFieldID(FeaturesDecoderMessageClass, "mFeatureNames", "Ljava/lang/String;");
    jstring jNamesObj = (jstring)env->GetObjectField(jMsg, jNamesFieldId);
    const char* namesChars = env->GetStringUTFChars(jNamesObj, 0);
    std::string pfnames = namesChars;
    env->ReleaseStringUTFChars(jNamesObj, namesChars);

    // timestamps
    jfieldID jTimestampsFieldId = env->GetFieldID(FeaturesDecoderMessageClass, "mFeatureTimestamps", "[J");
    jobject jTimestampsObj = env->GetObjectField(jMsg, jTimestampsFieldId);
    jlongArray* jTimestampsArray = reinterpret_cast<jlongArray*>(&jTimestampsObj);
    jlong* jTimestamps = env->GetLongArrayElements(*jTimestampsArray, 0);
    jint timestampsSize = env->GetArrayLength(*jTimestampsArray);
    std::vector<uint64_t> featureTimestamps(timestampsSize);
    for (int iTimestamp=0; iTimestamp < timestampsSize; iTimestamp++) {
        featureTimestamps[iTimestamp] = (uint64_t) jTimestamps[iTimestamp];
    }

    env->ReleaseLongArrayElements(*jTimestampsArray, jTimestamps, 0);

    // matrix
    jfieldID jFeaturesFieldId = env->GetFieldID(FeaturesDecoderMessageClass, "mFeatures", "Lcom/bbn/godec/Matrix;");
    jclass MatrixClass = env->FindClass("com/bbn/godec/Matrix");

    jobject jFeaturesObj = env->GetObjectField(jMsg, jFeaturesFieldId);
    jmethodID jColsMethodId = env->GetMethodID(MatrixClass, "cols", "()I");
    jmethodID jRowsMethodId = env->GetMethodID(MatrixClass, "rows", "()I");
    jmethodID jGetMethodId  = env->GetMethodID(MatrixClass, "get", "(II)F");

    jint numCols = env->CallIntMethod(jFeaturesObj, jColsMethodId);
    jint numRows = env->CallIntMethod(jFeaturesObj, jRowsMethodId);

    Matrix featureMatrix;
    featureMatrix.conservativeResize(numRows, numCols);

    // "First make it work, then make it efficient"
    for (int i=0; i < numCols; i++) {
        for (int j=0; j < numRows; j++) {
            jfloat val = env->CallFloatMethod(jFeaturesObj, jGetMethodId, j, i);
            featureMatrix(j, i) = val;
        }
    }

    DecoderMessage_ptr outMsg = FeaturesDecoderMessage::create(time, uttId, featureMatrix, pfnames, featureTimestamps);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}

#ifndef ANDROID

PyObject* FeaturesDecoderMessage::toPython() {
    GodecMessages_init_numpy();
    PyObject* dict = PyDict_New();

    PyDict_SetItemString(dict, "type", PyUnicode_FromString("FeaturesDecoderMessage"));
    PyDict_SetItemString(dict, "tag", PyUnicode_FromString(getTag().c_str()));
    PyDict_SetItemString(dict, "utterance_id", PyUnicode_FromString(mUtteranceId.c_str()));
    PyDict_SetItemString(dict, "feature_names", PyUnicode_FromString(mFeatureNames.c_str()));
    PyDict_SetItemString(dict, "time", PyLong_FromLong(getTime()));
    PyDict_SetItemString(dict, "descriptor", PyUnicode_FromString(getFullDescriptorString().c_str()));

    npy_intp timeDims[1] {(npy_intp)mFeatureTimestamps.size()};
    PyObject* pTimestamps = PyArray_SimpleNewFromData(1, timeDims, NPY_UINT64, &mFeatureTimestamps[0]);
    if (pTimestamps == NULL) GODEC_ERR << "Could not allocate feature timestamps memory";
    PyDict_SetItemString(dict, "feature_timestamps", pTimestamps);

    npy_intp featDims[2] {(npy_intp)mFeatures.rows(), (npy_intp)mFeatures.cols()};
    PyObject* pFeats = PyArray_SimpleNew(2, featDims, NPY_FLOAT32);
    if (pFeats == NULL) GODEC_ERR << "Could not allocate feature memory";
    for(int row = 0; row < mFeatures.rows(); row++) {
        for(int col = 0; col < mFeatures.cols(); col++) {
            *((float*)PyArray_GETPTR2(pFeats, row, col)) = mFeatures(row,col);
        }
    }
    PyDict_SetItemString(dict, "features", pFeats);
    return dict;
}
DecoderMessage_ptr FeaturesDecoderMessage::fromPython(PyObject* pMsg) {
    GodecMessages_init_numpy();
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    PythonGetDecoderMessageVals(pMsg, tag, time, descriptorString);
    PyObject* pUttId = PyDict_GetItemString(pMsg,"utterance_id");
    if (pUttId == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'feature_timestamps'!";
    std::string uttId = PyUnicode_AsUTF8(pUttId);

    PyObject* pFeatureNames = PyDict_GetItemString(pMsg,"feature_names");
    if (pFeatureNames == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'feature_names'!";
    std::string featureNames = PyUnicode_AsUTF8(pFeatureNames);

    // timestamps
    PyArrayObject* pTimestamps = (PyArrayObject*)PyDict_GetItemString(pMsg,"feature_timestamps");
    if (pTimestamps == NULL) GODEC_ERR << "Incoming Python message dict does not contain field 'feature_timestamps'!";
    if (PyArray_NDIM(pTimestamps) != 1) GODEC_ERR << "Incoming Python message, 'feature_timestamps' element is not a one-dimensional numpy array!";
    std::vector<uint64_t> featureTimestamps(PyArray_DIM(pTimestamps,0));
    for(int idx = 0; idx < featureTimestamps.size(); idx++) {
        featureTimestamps[idx] = *((uint64_t*)PyArray_GETPTR1(pTimestamps, idx));
    }

    // matrix
    PyArrayObject* pFeatures = (PyArrayObject*)PyDict_GetItemString(pMsg,"features");
    if (pFeatures == NULL) GODEC_ERR << "FeaturesDecoderMessage::fromPython : Dict does not contain field 'features'!";
    if (PyArray_NDIM(pFeatures) != 2) GODEC_ERR << "FeaturesDecoderMessage::fromPython : features element is not two-dimensional! Dimension is " << PyArray_NDIM(pFeatures);
    npy_intp* pFeaturesDims = PyArray_SHAPE(pFeatures);
    Matrix featureMatrix;
    featureMatrix.conservativeResize(pFeaturesDims[0], pFeaturesDims[1]);
    for(int row = 0; row < pFeaturesDims[0]; row++) {
        for(int col = 0; col < pFeaturesDims[1]; col++) {
            featureMatrix(row,col) = *((float*)PyArray_GETPTR2(pFeatures, row, col));
        }
    }

    DecoderMessage_ptr outMsg = FeaturesDecoderMessage::create(time, uttId, featureMatrix, featureNames, featureTimestamps);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}
#endif

/*
############ Matrix decoder message ###################
*/

std::string MatrixDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Matrix, " << mMat.rows() << "x" << mMat.cols() << ",sum=" << mMat.sum() << std::endl;
    return ss.str();
}

DecoderMessage_ptr MatrixDecoderMessage::create(uint64_t _time, Matrix _mat) {
    MatrixDecoderMessage* msg = new MatrixDecoderMessage();
    msg->setTime(_time);
    msg->mMat = _mat;
    return DecoderMessage_ptr(msg);
}


DecoderMessage_ptr MatrixDecoderMessage::clone()  const { return  DecoderMessage_ptr(new MatrixDecoderMessage(*this)); }
bool MatrixDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    remainingMsg = msg;
    return true;
}
bool MatrixDecoderMessage::canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    // matrices apply to all time points
    return true;
}
bool MatrixDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<MatrixDecoderMessage>(boost::const_pointer_cast<DecoderMessage>(msgList[0]));
    if (firstMsg->getTime() == sliceTime) {
        sliceMsg = msgList[0];
        msgList.erase(msgList.begin());
    } else {
        sliceMsg = msgList[0]->clone();
        auto adaptSliceMsg = boost::static_pointer_cast<MatrixDecoderMessage>(boost::const_pointer_cast<DecoderMessage>(sliceMsg));
        adaptSliceMsg->setTime(sliceTime);
    }
    return true;
}

void MatrixDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
}

jobject MatrixDecoderMessage::toJNI(JNIEnv* env) {
    GODEC_ERR << "MatrixDecoderMessage::toJNI not implemented yet!";
    return NULL;
};

#ifndef ANDROID
PyObject* MatrixDecoderMessage::toPython() {
    GODEC_ERR << "MatrixDecoderMessage::toPython not implemented yet";
    return nullptr;
}
DecoderMessage_ptr MatrixDecoderMessage::fromPython(PyObject* pMsg) {
    GODEC_ERR << "MatrixDecoderMessage::fromPython not implemented yet";
    return DecoderMessage_ptr();
}
#endif

/*
############ Top 1 decoder message ###################
*/

std::string NbestDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Nbest, " << mWords.size() << " entries, top entry " << (mWords.size() > 0 ? mWords[0].size() : 0) << " words";
    ss << std::endl;
    return ss.str();
}

DecoderMessage_ptr NbestDecoderMessage::create(uint64_t _time, std::vector<std::vector<std::string>> _text, std::vector<std::vector<uint64_t>> _words, std::vector<std::vector<uint64_t>> _alignment, std::vector<std::vector<float>> _confidences) {
    NbestDecoderMessage* msg = new NbestDecoderMessage();
    msg->setTime(_time);
    msg->mWords = _words;
    msg->mAlignment = _alignment;
    msg->mText = _text;
    msg->mConfidences = _confidences;
    return DecoderMessage_ptr(msg);
}



DecoderMessage_ptr NbestDecoderMessage::clone()  const { return  DecoderMessage_ptr(new NbestDecoderMessage(*this)); }
bool NbestDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    remainingMsg = msg;
    return true;
}
bool NbestDecoderMessage::canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    //Can only be sliced if borders are exact
    return msgList[0]->getTime() == sliceTime;
}
bool NbestDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<const NbestDecoderMessage>(msgList[0]);
    if (firstMsg->getTime() != sliceTime) return false;
    sliceMsg = msgList[0];
    msgList.erase(msgList.begin());
    return true;
}

void NbestDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
    for(int nbestIdx = 0; nbestIdx < mAlignment.size(); nbestIdx++) {
        for(int algIdx = 0; algIdx < mAlignment[nbestIdx].size(); algIdx++) {
            mAlignment[nbestIdx][algIdx] += deltaT;
        }
    }
}

jobject NbestToJniHelper(JNIEnv* env, const NbestDecoderMessage& msg) {
    jclass NbestDecoderMessageClass = env->FindClass("com/bbn/godec/NbestDecoderMessage");
    jclass NbestEntryClass = env->FindClass("com/bbn/godec/NbestEntry");
    jclass StringClass = env->FindClass("java/lang/String");
    jclass ArrayListClass = env->FindClass("java/util/ArrayList");

    jmethodID nbestInit = env->GetMethodID(NbestDecoderMessageClass, "<init>", "(Ljava/lang/String;JLjava/util/ArrayList;)V");
    jmethodID nbestEntryInit = env->GetMethodID(NbestEntryClass, "<init>", "([I[J[Ljava/lang/String;[F)V");
    jmethodID arrayListInit = env->GetMethodID(ArrayListClass, "<init>", "()V");
    jmethodID arrayListAdd = env->GetMethodID(ArrayListClass, "add", "(Ljava/lang/Object;)Z");

    std::string tag = msg.getTag();
    jlong time = msg.getTime();

    env->PushLocalFrame(3); // jTag, jArrayListObject, retObj
    jstring jTag = env->NewStringUTF(tag.c_str());
    jobject jArrayListObject = env->NewObject(ArrayListClass, arrayListInit);

    for (int entryIdx = 0; entryIdx < msg.mWords.size(); entryIdx++) {
        const std::vector<std::string>& text = msg.mText[entryIdx];
        const std::vector<uint64_t>& words = msg.mWords[entryIdx];
        const std::vector<uint64_t>& alignment = msg.mAlignment[entryIdx];
        const std::vector<float>& confidences = msg.mConfidences[entryIdx];

        env->PushLocalFrame(4); // jWordsArray, jAlignmentArray, jConfidencesArray, jTextArray

        jintArray jWordsArray = env->NewIntArray((jsize)words.size());
        jlongArray jAlignmentArray = env->NewLongArray((jsize)alignment.size());
        jfloatArray jConfidencesArray = env->NewFloatArray((jsize)confidences.size());
        jobjectArray jTextArray = env->NewObjectArray((jsize)text.size(), StringClass, NULL);
        if (words.size() > 0) {
            env->SetIntArrayRegion(jWordsArray, 0, (jsize)words.size(), &(std::vector<jint>(words.begin(), words.end())[0]));
            env->SetLongArrayRegion(jAlignmentArray, 0, (jsize)alignment.size(), &(std::vector<jlong>(alignment.begin(), alignment.end())[0]));
            env->SetFloatArrayRegion(jConfidencesArray, 0, (jsize)confidences.size(), &confidences[0]);
            for (int wordIdx = 0; wordIdx < words.size(); wordIdx++) {
                env->SetObjectArrayElement(jTextArray, wordIdx, env->NewStringUTF(text[wordIdx].c_str()));
            }
        }

        jobject jNbestEntryObject = env->NewObject(NbestEntryClass, nbestEntryInit, jWordsArray, jAlignmentArray, jTextArray, jConfidencesArray);
        env->CallBooleanMethod(jArrayListObject, arrayListAdd, jNbestEntryObject);

        env->PopLocalFrame(NULL);
    }

    jobject retObj = env->NewObject(NbestDecoderMessageClass, nbestInit, jTag, time, jArrayListObject);
    return env->PopLocalFrame(retObj);
}

jobject NbestDecoderMessage::toJNI(JNIEnv* env) {
    env->PushLocalFrame(2); // mainObject, tentativeObject
    jobject mainObject = NbestToJniHelper(env, *this);
    jclass DecoderMessageClass = env->FindClass("com/bbn/godec/DecoderMessage");
    jstring jDescriptor = env->NewStringUTF(getFullDescriptorString().c_str());
    jmethodID jSetFullDescriptor = env->GetMethodID(DecoderMessageClass, "setFullDescriptorString", "(Ljava/lang/String;)V");
    env->CallVoidMethod(mainObject, jSetFullDescriptor, jDescriptor);

    return env->PopLocalFrame(mainObject);
};

void GetNbestEntryVals(JNIEnv* env, jobject jEntryObj, std::vector<std::string>& text, std::vector<uint64_t>& words, std::vector<uint64_t>& alignment, std::vector<float>& confidences) {
    jclass NbestEntryClass = env->FindClass("com/bbn/godec/NbestEntry");

    // Words
    jfieldID jWordsFieldId = env->GetFieldID(NbestEntryClass, "words", "[I");
    jintArray jWordsArray = (jintArray)env->GetObjectField(jEntryObj, jWordsFieldId);
    int jWordsArraySize = env->GetArrayLength(jWordsArray);
    jint* jWordsArrayInts = env->GetIntArrayElements(jWordsArray, 0);
    for (int idx = 0; idx < jWordsArraySize; idx++) words.push_back(jWordsArrayInts[idx]);
    env->ReleaseIntArrayElements(jWordsArray, jWordsArrayInts, 0);

    // Alignment
    jfieldID jAlignmentFieldId = env->GetFieldID(NbestEntryClass, "alignment", "[J");
    jlongArray jAlignmentArray = (jlongArray)env->GetObjectField(jEntryObj, jAlignmentFieldId);
    jlong* jAlignmentArrayInts = env->GetLongArrayElements(jAlignmentArray, 0);
    for (int idx = 0; idx < jWordsArraySize; idx++) alignment.push_back(jAlignmentArrayInts[idx]);
    env->ReleaseLongArrayElements(jAlignmentArray, jAlignmentArrayInts, 0);

    // Text
    jfieldID jTextFieldId = env->GetFieldID(NbestEntryClass, "text", "[Ljava/lang/String;");
    jobjectArray jTextArray = (jobjectArray)env->GetObjectField(jEntryObj, jTextFieldId);
    for (int idx = 0; idx < jWordsArraySize; idx++) {
        jstring jTextArrayString = (jstring)env->GetObjectArrayElement(jTextArray, idx);
        const char* textChars = env->GetStringUTFChars(jTextArrayString, 0);
        text.push_back(textChars);
        env->ReleaseStringUTFChars(jTextArrayString, textChars);
    }

    // Confidences
    jfieldID jConfidencesFieldId = env->GetFieldID(NbestEntryClass, "wordConfidences", "[F");
    jfloatArray jConfidencesArray = (jfloatArray)env->GetObjectField(jEntryObj, jConfidencesFieldId);
    jfloat* jConfidencesArrayFloats = env->GetFloatArrayElements(jConfidencesArray, 0);
    for (int idx = 0; idx < jWordsArraySize; idx++) confidences.push_back(jConfidencesArrayFloats[idx]);
    env->ReleaseFloatArrayElements(jConfidencesArray, jConfidencesArrayFloats, 0);
}

DecoderMessage_ptr NbestDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    JNIGetDecoderMessageVals(env, jMsg, tag, time, descriptorString);

    jclass NbestDecoderMessageClass = env->FindClass("com/bbn/godec/NbestDecoderMessage");
    jclass ArrayListClass = env->FindClass("java/util/ArrayList");

    std::vector<std::vector<std::string>> textV;
    std::vector<std::vector<uint64_t>> wordsV;
    std::vector<std::vector<uint64_t>> alignmentV;
    std::vector<std::vector<float>> confidencesV;

    jfieldID jEntriesFieldId = env->GetFieldID(NbestDecoderMessageClass, "mEntries", "Ljava/util/ArrayList;");
    jobject jEntriesObj = (jobject)env->GetObjectField(jMsg, jEntriesFieldId);
    jmethodID jArraySizeMethodId = env->GetMethodID(ArrayListClass, "size", "()I");
    jmethodID jArrayGetMethodId = env->GetMethodID(ArrayListClass, "get", "(I)Ljava/lang/Object;");
    jint numEntries = env->CallIntMethod(jEntriesObj, jArraySizeMethodId);
    for (int entryIdx = 0; entryIdx < numEntries; entryIdx++) {
        jobject jEntryObj = env->CallObjectMethod(jEntriesObj, jArrayGetMethodId, entryIdx);
        std::vector<std::string> text;
        std::vector<uint64_t> words;
        std::vector<uint64_t> alignment;
        std::vector<float> confidences;
        GetNbestEntryVals(env, jEntryObj, text, words, alignment, confidences);
        textV.push_back(text);
        wordsV.push_back(words);
        alignmentV.push_back(alignment);
        confidencesV.push_back(confidences);
    }
    DecoderMessage_ptr outMsg = NbestDecoderMessage::create(time, textV, wordsV, alignmentV, confidencesV);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}

#ifndef ANDROID
PyObject* NbestDecoderMessage::toPython() {
    GODEC_ERR << "NbestDecoderMessage::toPython not implemented yet";
    return nullptr;
}
DecoderMessage_ptr NbestDecoderMessage::fromPython(PyObject* pMsg) {
    GODEC_ERR << "NbestDecoderMessage::fromPython not implemented yet";
    return DecoderMessage_ptr();
}
#endif

/*
############ Audio Info decoder message ###################
*/

std::string AudioInfoDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Audio Information, sample rate=" << mSampleRate << ", ticks per sample=" << mTicksPerSample << std::endl;
    return ss.str();
}

DecoderMessage_ptr AudioInfoDecoderMessage::create(uint64_t time, float sampleRate, float ticksPerSample) {
    AudioInfoDecoderMessage* msg = new AudioInfoDecoderMessage();
    msg->setTime(time);
    msg->mSampleRate = sampleRate;
    msg->mTicksPerSample = ticksPerSample;
    return DecoderMessage_ptr(msg);
}

DecoderMessage_ptr AudioInfoDecoderMessage::clone()  const { return  DecoderMessage_ptr(new AudioInfoDecoderMessage(*this)); }

bool AudioInfoDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    auto mergeMsg = boost::static_pointer_cast<const AudioInfoDecoderMessage>(msg); 
    if ((mSampleRate != mergeMsg->mSampleRate) && (mTicksPerSample == mergeMsg->mTicksPerSample)) GODEC_ERR << "sample_rate or ticks_per_sample has changed.\n Current message : " << describeThyself() << "\n, to be merged: " << mergeMsg->describeThyself();
    setTime(mergeMsg->getTime());
    mSampleRate = mergeMsg->mSampleRate;
    mTicksPerSample = mergeMsg->mTicksPerSample;
    return false;
}

bool AudioInfoDecoderMessage::canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    // Convo state can always be sliced
    return true;
}
bool AudioInfoDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<const AudioInfoDecoderMessage>(msgList[0]);
    if (firstMsg->getTime() == sliceTime) {
        sliceMsg = msgList[0];
        msgList.erase(msgList.begin());
        return true;
    } else {
        sliceMsg = AudioInfoDecoderMessage::create(sliceTime,
                   firstMsg->mSampleRate, firstMsg->mTicksPerSample);
        return true;
    }
}

void AudioInfoDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
}

jobject AudioInfoDecoderMessage::toJNI(JNIEnv* env) {
    GODEC_ERR << "AudioInfoDecoderMessage::toJNI not implemented yet!";
    return NULL;
};

DecoderMessage_ptr AudioInfoDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    GODEC_ERR << "AudioInfoDecoderMessage::fromJNI not implemented yet!";
    return NULL;
}

#ifndef ANDROID
PyObject* AudioInfoDecoderMessage::toPython() {
    GODEC_ERR << "AudioInfoDecoderMessage::toPython not implemented yet!";
    return NULL;
}
DecoderMessage_ptr AudioInfoDecoderMessage::fromPython(PyObject* pMsg) {
    GODEC_ERR << "AudioInfoDecoderMessage::fromPython not implemented yet!";
    return NULL;

}
#endif


/*
############ Conversation state decoder message ###################
*/

std::string ConversationStateDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Conversation state, utt ID=" << mUtteranceId << ", last in utt=" << b2s(mLastChunkInUtt) << ", convo ID=" << mConvoId << ", last in convo=" << b2s(mLastChunkInConvo) << std::endl;
    return ss.str();
}

DecoderMessage_ptr ConversationStateDecoderMessage::create(uint64_t _time, std::string _utteranceId, bool _isLastChunkInUtt, std::string _convoId, bool _isLastChunkInConvo) {
    ConversationStateDecoderMessage* msg = new ConversationStateDecoderMessage();
    msg->setTime(_time);
    msg->mUtteranceId = _utteranceId;
    msg->mLastChunkInUtt = _isLastChunkInUtt;
    msg->mConvoId = _convoId;
    msg->mLastChunkInConvo = _isLastChunkInConvo;
    if (_isLastChunkInConvo && !_isLastChunkInUtt) GODEC_ERR << "Trying to construct nonsensical ConversationStateDecoderMessage: isLastChunkInConvo=true but isLastChunkInUtt=false. Utterances can't carry over past conversations!";
    return DecoderMessage_ptr(msg);
}



DecoderMessage_ptr ConversationStateDecoderMessage::clone()  const { return  DecoderMessage_ptr(new ConversationStateDecoderMessage(*this)); }

bool ConversationStateDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    auto mergeMsg = boost::static_pointer_cast<const ConversationStateDecoderMessage>(msg);

    // Scenario: Previous message wasn't declared as last of utt, but now the utterance ID switches!
    if (!mLastChunkInUtt && (mUtteranceId != mergeMsg->mUtteranceId)) GODEC_ERR << "last utterance was not finished but new one has different utterance ID. \n Current message : " << describeThyself() << "\n, to be merged: " << mergeMsg->describeThyself();
    // Scenario: Previous messages was declared as last of utt, but the utterance ID stayed the same!
    if (mLastChunkInUtt && (mUtteranceId == mergeMsg->mUtteranceId)) GODEC_ERR << "last utterance was finished but new one has same utterance ID. \n Current message : " << describeThyself() << "\n, to be merged: " << mergeMsg->describeThyself();

    // Scenario: Previous message wasn't declared as last of convo, but now the convo ID switches!
    if (!mLastChunkInConvo && (mConvoId != mergeMsg->mConvoId)) GODEC_ERR << "last conversation was not finished but new one has different convo ID. \n Current message : " << describeThyself() << "\n, to be merged: " << mergeMsg->describeThyself();
    // Scenario: Previous messages was declared as last of convo, but the convo ID stayed the same!
    if (mLastChunkInConvo && (mConvoId == mergeMsg->mConvoId)) GODEC_ERR << "last conversation was finished but new one has same convo ID. \n Current message : " << describeThyself() << "\n, to be merged: " << mergeMsg->describeThyself();

    if (mLastChunkInUtt) { // Utterance closed, can't merge
        remainingMsg = msg;
        return true;
    } else {
        setTime(mergeMsg->getTime());
        mLastChunkInUtt = mergeMsg->mLastChunkInUtt;
        mLastChunkInConvo = mergeMsg->mLastChunkInConvo;
        return false;
    }
}

bool ConversationStateDecoderMessage::canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    // Convo state can always be sliced
    return true;
}
bool ConversationStateDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<const ConversationStateDecoderMessage>(msgList[0]);
    if (firstMsg->getTime() == sliceTime) {
        sliceMsg = msgList[0];
        msgList.erase(msgList.begin());
        return true;
    } else {
        sliceMsg = ConversationStateDecoderMessage::create(sliceTime,
                   firstMsg->mUtteranceId, false, firstMsg->mConvoId, false);
        auto ignoreData = firstMsg->getDescriptor("ignore_data");
        (boost::const_pointer_cast<DecoderMessage>(sliceMsg))->addDescriptor("ignore_data", ignoreData);
        return true;
    }
}

void ConversationStateDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
}

jobject ConversationStateDecoderMessage::toJNI(JNIEnv* env) {
    env->PushLocalFrame(4); // jTag, jUttId, jConvoId, jConvoMsg
    jclass ConvoDecoderMessageClass = env->FindClass("com/bbn/godec/ConversationStateDecoderMessage");
    jclass DecoderMessageClass = env->FindClass("com/bbn/godec/DecoderMessage");
    jmethodID jConvoMsgInit = env->GetMethodID(ConvoDecoderMessageClass, "<init>", "(Ljava/lang/String;JLjava/lang/String;ZLjava/lang/String;Z)V");
    std::string tag = getTag();
    jstring jTag = env->NewStringUTF(tag.c_str());
    jstring jUttId = env->NewStringUTF(mUtteranceId.c_str());
    jstring jConvoId = env->NewStringUTF(mConvoId.c_str());
    jlong jTime = getTime();
    jobject jConvoMsg = env->NewObject(ConvoDecoderMessageClass, jConvoMsgInit, jTag, jTime, jUttId, mLastChunkInUtt, jConvoId, mLastChunkInConvo);

    jstring jDescriptor = env->NewStringUTF(getFullDescriptorString().c_str());
    jmethodID jSetFullDescriptor = env->GetMethodID(DecoderMessageClass, "setFullDescriptorString", "(Ljava/lang/String;)V");
    env->CallVoidMethod(jConvoMsg, jSetFullDescriptor, jDescriptor);
    return env->PopLocalFrame(jConvoMsg);
};

DecoderMessage_ptr ConversationStateDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    JNIGetDecoderMessageVals(env, jMsg, tag, time, descriptorString);

    jclass ConversationStateDecoderMessageClass = env->FindClass("com/bbn/godec/ConversationStateDecoderMessage");
    jfieldID jUttFieldId = env->GetFieldID(ConversationStateDecoderMessageClass, "mUtteranceId", "Ljava/lang/String;");
    jstring jUttIdObj = (jstring)env->GetObjectField(jMsg, jUttFieldId);
    const char* uttIdChars = env->GetStringUTFChars(jUttIdObj, 0);
    std::string uttId = uttIdChars;
    env->ReleaseStringUTFChars(jUttIdObj, uttIdChars);

    // utterance ID
    jfieldID jConvoFieldId = env->GetFieldID(ConversationStateDecoderMessageClass, "mConvoId", "Ljava/lang/String;");
    jstring jConvoObj = (jstring)env->GetObjectField(jMsg, jConvoFieldId);
    const char* convoChars = env->GetStringUTFChars(jConvoObj, 0);
    std::string convoId = convoChars;
    env->ReleaseStringUTFChars(jConvoObj, convoChars);

    // booleans
    jfieldID jLastChunkInUttFieldId = env->GetFieldID(ConversationStateDecoderMessageClass, "mLastChunkInUtt", "Z");
    jboolean isLastChunkInUtt = env->GetBooleanField(jMsg, jLastChunkInUttFieldId);
    jfieldID jLastChunkInConvoFieldId = env->GetFieldID(ConversationStateDecoderMessageClass, "mLastChunkInConvo", "Z");
    jboolean isLastChunkInConvo = env->GetBooleanField(jMsg, jLastChunkInConvoFieldId);

    DecoderMessage_ptr outMsg = ConversationStateDecoderMessage::create(time, uttId, isLastChunkInUtt, convoId, isLastChunkInConvo);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}

#ifndef ANDROID
PyObject* ConversationStateDecoderMessage::toPython() {
    GodecMessages_init_numpy();
    PyObject* dict = PyDict_New();

    PyDict_SetItemString(dict, "type", PyUnicode_FromString("ConversationStateDecoderMessage"));
    PyDict_SetItemString(dict, "tag", PyUnicode_FromString(getTag().c_str()));
    PyDict_SetItemString(dict, "utterance_id", PyUnicode_FromString(mUtteranceId.c_str()));
    PyDict_SetItemString(dict, "convo_id", PyUnicode_FromString(mConvoId.c_str()));
    PyDict_SetItemString(dict, "last_chunk_in_utt", mLastChunkInUtt ? Py_True : Py_False);
    PyDict_SetItemString(dict, "last_chunk_in_convo", mLastChunkInConvo ? Py_True : Py_False);
    PyDict_SetItemString(dict, "time", PyLong_FromLong(getTime()));
    PyDict_SetItemString(dict, "descriptor", PyUnicode_FromString(getFullDescriptorString().c_str()));
    return dict;
}
DecoderMessage_ptr ConversationStateDecoderMessage::fromPython(PyObject* pMsg) {
    GodecMessages_init_numpy();
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    PythonGetDecoderMessageVals(pMsg, tag, time, descriptorString);

    PyObject* pUttId = PyDict_GetItemString(pMsg,"utterance_id");
    if (pUttId == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'utterance_id'!";
    std::string uttId = PyUnicode_AsUTF8(pUttId);

    PyObject* pConvoId = PyDict_GetItemString(pMsg,"convo_id");
    if (pConvoId == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'convo_id'!";
    std::string convoId = PyUnicode_AsUTF8(pConvoId);

    PyObject* pLastChunkInUtt = PyDict_GetItemString(pMsg,"last_chunk_in_utt");
    if (pLastChunkInUtt == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'last_chunk_in_utt'!";
    bool lastChunkInUtt = pLastChunkInUtt == Py_True ? true : false;

    PyObject* pLastChunkInConvo = PyDict_GetItemString(pMsg,"last_chunk_in_convo");
    if (pLastChunkInConvo == nullptr) GODEC_ERR << "Incoming Python message dict does not contain field 'last_chunk_in_convo'!";
    bool lastChunkInConvo = pLastChunkInConvo  == Py_False ? true : false;

    DecoderMessage_ptr outMsg = ConversationStateDecoderMessage::create(time, uttId, lastChunkInUtt, convoId, lastChunkInConvo);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}
#endif

//############ Raw text message ###################

std::string BinaryDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << "Binary data, " << mData.size() << " bytes, format " << mFormat << std::endl;
    return ss.str();
}

DecoderMessage_ptr BinaryDecoderMessage::create(uint64_t _time, std::vector<unsigned char> data, std::string format) {
    BinaryDecoderMessage* msg = new BinaryDecoderMessage();
    msg->setTime(_time);
    msg->mData = data;
    msg->mFormat = format;
    return DecoderMessage_ptr(msg);
}



DecoderMessage_ptr BinaryDecoderMessage::clone()  const { return  DecoderMessage_ptr(new BinaryDecoderMessage(*this)); }
bool BinaryDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    remainingMsg = msg;
    return true;
}
bool BinaryDecoderMessage::canSliceAt(uint64_t sliceTime, std::vector<DecoderMessage_ptr>& msgList, uint64_t streamStartOffset, bool verbose) {
    //Can only be sliced if borders are exact
    return msgList[0]->getTime() == sliceTime;
}
bool BinaryDecoderMessage::sliceOut(uint64_t sliceTime, DecoderMessage_ptr& sliceMsg, std::vector<DecoderMessage_ptr>& msgList, int64_t streamStartOffset, bool verbose) {
    auto firstMsg = boost::static_pointer_cast<const BinaryDecoderMessage>(msgList[0]);
    if (firstMsg->getTime() != sliceTime) return false;
    sliceMsg = msgList[0];
    msgList.erase(msgList.begin());
    return true;
}

void BinaryDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
}

jobject BinaryDecoderMessage::toJNI(JNIEnv* env) {
    jclass BinaryDecoderMessageClass = env->FindClass("com/bbn/godec/BinaryDecoderMessage");
    jclass DecoderMessageClass = env->FindClass("com/bbn/godec/DecoderMessage");
    jmethodID jBinaryMsgInit = env->GetMethodID(BinaryDecoderMessageClass, "<init>", "(Ljava/lang/String;J[BLjava/lang/String;)V");
    std::string tag = getTag();
    jstring jTag = env->NewStringUTF(tag.c_str());
    jlong jTime = getTime();
    jbyteArray jDataArrayObj = env->NewByteArray((jsize)mData.size());
    env->SetByteArrayRegion(jDataArrayObj, 0, (jsize)mData.size(), (const jbyte*)mData.data());
    jstring jFormatString = env->NewStringUTF(mFormat.c_str());
    jobject jAudioMsg = env->NewObject(BinaryDecoderMessageClass, jBinaryMsgInit, jTag, jTime, jDataArrayObj, jFormatString);

    jstring jDescriptor = env->NewStringUTF(getFullDescriptorString().c_str());
    jmethodID jSetFullDescriptor = env->GetMethodID(DecoderMessageClass, "setFullDescriptorString", "(Ljava/lang/String;)V");
    env->CallVoidMethod(jAudioMsg, jSetFullDescriptor, jDescriptor);
    return jAudioMsg;
};

DecoderMessage_ptr BinaryDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    JNIGetDecoderMessageVals(env, jMsg, tag, time, descriptorString);

    jclass BinaryDecoderMessageClass = env->FindClass("com/bbn/godec/BinaryDecoderMessage");
    jfieldID jDataFieldId = env->GetFieldID(BinaryDecoderMessageClass, "mData", "[B");
    jobject jDataObj = env->GetObjectField(jMsg, jDataFieldId);
    jbyteArray* jDataArray = reinterpret_cast<jbyteArray*>(&jDataObj);
    jbyte* jData = env->GetByteArrayElements(*jDataArray, 0);
    jint dataSize = env->GetArrayLength(*jDataArray);
    std::vector<unsigned char> dataVec(dataSize);
    memcpy(&dataVec[0], jData, dataSize);
    env->ReleaseByteArrayElements(*jDataArray, jData, 0);

    jfieldID jFormatFieldId = env->GetFieldID(BinaryDecoderMessageClass, "mFormat", "Ljava/lang/String;");
    jstring jFormatObj = (jstring)env->GetObjectField(jMsg, jFormatFieldId);
    const char* formatChars = env->GetStringUTFChars(jFormatObj, 0);
    std::string format = formatChars;
    env->ReleaseStringUTFChars(jFormatObj, formatChars);

    DecoderMessage_ptr outMsg = BinaryDecoderMessage::create(time, dataVec, format);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}

#ifndef ANDROID
PyObject* BinaryDecoderMessage::toPython() {
    GODEC_ERR << "BinaryDecoderMessage::toPython not implemented yet";
    return nullptr;
}
DecoderMessage_ptr BinaryDecoderMessage::fromPython(PyObject* pMsg) {
    GODEC_ERR << "BinaryDecoderMessage::fromPython not implemented yet";
    return DecoderMessage_ptr();
}
#endif

std::string JsonDecoderMessage::describeThyself() const {
    std::stringstream ss;
    ss << DecoderMessage::describeThyself();
    ss << mJson.dump(4);
    return ss.str();
}

DecoderMessage_ptr JsonDecoderMessage::clone() const {
    return DecoderMessage_ptr(new JsonDecoderMessage(*this));
}

jobject JsonDecoderMessage::toJNI(JNIEnv *env) {
    jclass JsonDecoderMessageClass = env->FindClass("com/bbn/godec/JsonDecoderMessage");
    jmethodID msgInit = env->GetMethodID(JsonDecoderMessageClass, "<init>", "(Ljava/lang/String;JLjava/lang/String;)V");
    std::string tag = getTag();
    jlong time = getTime();
    env->PushLocalFrame(3); // jTag, JJsonStr, retObj
    jstring jTag = env->NewStringUTF(tag.c_str());
    jstring jJsonString = env->NewStringUTF(mJson.dump().c_str());
    jobject retObj = env->NewObject(JsonDecoderMessageClass, msgInit, jTag, time, jJsonString);
    return env->PopLocalFrame(retObj);
}

DecoderMessage_ptr JsonDecoderMessage::fromJNI(JNIEnv* env, jobject jMsg) {
    jclass JsonDecoderMessageClass = env->FindClass("com/bbn/godec/JsonDecoderMessage");
    jmethodID jtoStringId = env->GetMethodID(JsonDecoderMessageClass, "toString", "()Ljava/lang/String;");
    jstring jJsonString = (jstring)env->CallObjectMethod(jMsg, jtoStringId);
    const char* jsonChars = env->GetStringUTFChars(jJsonString, 0);
    std::string jsonString = jsonChars;
    env->ReleaseStringUTFChars(jJsonString, jsonChars);

    std::string tag;
    uint64_t time;
    std::string descriptorString;
    JNIGetDecoderMessageVals(env, jMsg, tag, time, descriptorString);
    json j = json::parse(jsonString);
    return JsonDecoderMessage::create(time, j);
}

#ifndef ANDROID
PyObject* JsonDecoderMessage::toPython() {
    GodecMessages_init_numpy();
    PyObject* dict = PyDict_New();

    PyDict_SetItemString(dict, "type", PyUnicode_FromString("JsonDecoderMessage"));
    PyDict_SetItemString(dict, "tag", PyUnicode_FromString(getTag().c_str()));
    PyDict_SetItemString(dict, "time", PyLong_FromLong(getTime()));
    PyDict_SetItemString(dict, "descriptor", PyUnicode_FromString(getFullDescriptorString().c_str()));
    PyDict_SetItemString(dict, "json", PyUnicode_FromString(mJson.dump().c_str()));
    return dict;
}

DecoderMessage_ptr JsonDecoderMessage::fromPython(PyObject* pMsg) {
    GodecMessages_init_numpy();
    std::string tag;
    uint64_t time;
    std::string descriptorString;
    PythonGetDecoderMessageVals(pMsg, tag, time, descriptorString);

    PyObject* pJson = PyDict_GetItemString(pMsg,"json");
    if (pJson == nullptr) GODEC_ERR << "JsonDecoderMessage::fromPython: Passed in dict does not contain 'json' field!";
    json j = json::parse(PyUnicode_AsUTF8(pJson));

    DecoderMessage_ptr outMsg = JsonDecoderMessage::create(time, j);
    (boost::const_pointer_cast<DecoderMessage>(outMsg))->setFullDescriptorString(descriptorString);
    return outMsg;
}
#endif

// json message can not be merged
bool JsonDecoderMessage::mergeWith(DecoderMessage_ptr msg, DecoderMessage_ptr &remainingMsg, bool verbose) {
    remainingMsg = msg;
    return true;
}

bool JsonDecoderMessage::sliceOut(uint64_t sliceTime,
                                  DecoderMessage_ptr &sliceMsg,
                                  std::vector<DecoderMessage_ptr> &streamList,
                                  int64_t streamStartOffset,
                                  bool verbose) {
    auto firstMsg = boost::static_pointer_cast<const JsonDecoderMessage>(streamList[0]);
    if (firstMsg->getTime() != sliceTime) return false;
    sliceMsg = streamList[0];
    streamList.erase(streamList.begin());
    return true;
}

void JsonDecoderMessage::shiftInTime(int64_t deltaT) {
    setTime((int64_t)getTime()+deltaT);
}

bool JsonDecoderMessage::canSliceAt(uint64_t sliceTime,
                                    std::vector<DecoderMessage_ptr> &streamList,
                                    uint64_t streamStartOffset,
                                    bool verbose) {
    return streamList[0]->getTime() == sliceTime;
}

DecoderMessage_ptr JsonDecoderMessage::create(uint64_t time, json &jsonObj) {
    JsonDecoderMessage* msg = new JsonDecoderMessage();
    msg->setTime(time);
    msg->mJson = jsonObj;

    return DecoderMessage_ptr(msg);
}

}

