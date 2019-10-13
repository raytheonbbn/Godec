#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include "godec/ChannelMessenger.h"
#include "GodecMessages.h"
#include "godec/json.hpp"
#include "cnpy.h"

namespace Godec {

enum AudioType {
    PCM,
    MuLaw,
    Alaw,
};

class AudioFileReader {
  public:
    void readData(std::vector<unsigned char> &audioData, int64_t beginSample, int64_t &endSample, std::vector<int> channels);
    void close();
    int64_t getTotalNumSamples();
    FILE* fPtr;
    int64_t fileSize;
    float samplingFrequency;
    int numChannels;
    int bytesPerSample;
    AudioType audioType;
    int64_t headerSize;
};

class AnalistFileFeeder {
  public:
    AnalistFileFeeder(char* analistFile, char* waveFileDir, char* waveFileExtension);
    bool getNextUtterance(std::vector<unsigned char> &audioData,
                          int &sampleWidth,
                          std::string &utteranceId,
                          std::string &episodeName,
                          bool &episodeDone,
                          bool &fileDone,
                          std::string &waveFile,
                          std::vector<int> &channels,
                          std::string &formatString,
                          float &audioChunkTimeInSeconds,
                          int64_t &beginSample,
                          float &uttOffsetInFileInSeconds);
  private:
    FILE* listFileFp;
    char waveFileDir[1024 * 10]; // XXXX be dumb for now
    char waveFileExtension[1024 * 10];
    long utteranceCounter;
    std::vector<std::string> episodeList;
};

class TextFileFeeder {
  public:
    TextFileFeeder(const std::string &text_file);
    ~TextFileFeeder();
    bool getNextUtterance(std::string &utteranceId, std::string &text, bool &fileDone);
  private:
    std::ifstream text_reader;
    int32_t lineCounter;
};

class NumpyFileFeeder {
  public:
    NumpyFileFeeder(std::string npzKeysListFile);
    bool getNextUtterance(float*&features,
                          int64_t &numFrames,
                          int &frameLenth,
                          std::string &utteranceId,
                          std::string &episodeName,
                          bool &episodeDone,
                          bool &fileDone);
  private:
    std::ifstream mKeysListFile;
    int64_t mKeyListFileLineCount;
    int64_t mKeysListNumLines;
    Matrix mCurrentFeatures;
};

class JsonFileFeeder {
  public:
    JsonFileFeeder(const std::string &jsonFile);
    ~JsonFileFeeder();
    bool getNextMessage(json &jsonMessage, json &jsonConvState);
  private:
    json jsonData;
    size_t itemCounter;
};

class FileFeederHolder {
  public:
    FileFeederHolder() {
        numpyFileFeeder = nullptr;
        analistFileFeeder = nullptr;
        textFileFeeder = nullptr;
        jsonFileFeeder = nullptr;
        mTimeUpsampleFactor = 1;
    }
    ~FileFeederHolder() {
        delete numpyFileFeeder;
        delete analistFileFeeder;
        delete textFileFeeder;
        delete jsonFileFeeder;
    }

    NumpyFileFeeder* numpyFileFeeder;
    AnalistFileFeeder* analistFileFeeder;
    TextFileFeeder* textFileFeeder;
    JsonFileFeeder* jsonFileFeeder;

    int chunkSizeInSamples;
    int chunkSizeInFrames;
    float mFeedRealtimeFactor;
    int mTimeUpsampleFactor;
    std::string inputFile;
};

class FileFeederComponent : public LoopProcessor {
  public:
    static LoopProcessor* make(std::string id, ComponentGraphConfig* configPt);
    static std::string describeThyself();
    FileFeederComponent(std::string id, ComponentGraphConfig* configPt);
    ~FileFeederComponent();
    void Start();
  private:
    void ProcessMessage(const DecoderMessageBlock& msgBlock);
    bool RequiresConvStateInput() override { return false; }
    boost::shared_ptr<FileFeederHolder> GetFileFeederFromConfig(ComponentGraphConfig* configPt);

    std::string mControlType;
    static std::string SlotOutput;

    channel<boost::shared_ptr<FileFeederHolder> > mFFHChannel;

    boost::thread mFeedThread;
    void FeedLoop();
};

}
