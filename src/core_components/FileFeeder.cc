#include "FileFeeder.h"
#include "godec/ComponentGraph.h"
#include "sys/stat.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <iomanip>

namespace Godec {

const uint16_t WAVE_FORMAT_PCM = 0x0001;
const uint16_t WAVE_FORMAT_ALAW = 0x0006;
const uint16_t WAVE_FORMAT_MULAW = 0x0007;
const uint16_t WAVE_FORMAT_EXTENDED = 0xFFFE;

void trimLine(char* segFileLine) {
    for (size_t idx = 0; idx < strlen(segFileLine); idx++)
        if ((segFileLine[idx] == '\n') || (segFileLine[idx] == '\r')) segFileLine[idx] = '\0';
}

bool isEndOfFile(FILE* fp) {
    auto prePos = ftell(fp);
    int c = 0;
    do {
        c = fgetc(fp);
    } while ((c == '\n' || c == '\r' || c == ' ') && (c != EOF));
    fseek(fp, prePos, SEEK_SET);
    return (c == EOF);
}

bool isEndOfFile(std::ifstream& is) {
    auto prePos = is.tellg();
    int c = 0;
    do {
        c = is.get();
    } while ((c == '\n' || c == '\r' || c == ' ') && (c != EOF));
    is.seekg(prePos);
    return (c == EOF);
}

int64_t AudioFileReader::getTotalNumSamples() {
    return fileSize / (numChannels*bytesPerSample);
}

void AudioFileReader::readData(std::vector<unsigned char>& audioData, int64_t beginSample, int64_t& endSample, std::vector<int> channels) {
    int64_t segmentBeginBytes = numChannels*bytesPerSample*beginSample;
    // int64_t segmentEndBytes = numChannels*bytesPerSample*endSample;

    int64_t numSamples = (endSample - beginSample);
    int64_t allChannelsTmpAudioSize = numSamples*numChannels*bytesPerSample;
    unsigned char* allChannelsTmpAudio = new unsigned char[allChannelsTmpAudioSize];

    fseek(fPtr, (int64_t)(headerSize + segmentBeginBytes), SEEK_SET);
    int64_t actuallyReadBytes = fread(allChannelsTmpAudio, 1, allChannelsTmpAudioSize, fPtr);

    if (actuallyReadBytes != allChannelsTmpAudioSize) {
        //printf("Sample request beyond end of file! Only read to position %li\n", ftell(fPtr));
        numSamples = actuallyReadBytes / (numChannels*bytesPerSample);
        endSample = beginSample+numSamples;
    }

    audioData.resize(numSamples*bytesPerSample*channels.size());

    for (int64_t sampleIdx = 0; sampleIdx < numSamples; sampleIdx++) {
        for(int channelIdx = 0; channelIdx < channels.size(); channelIdx++) {
            int channel = channels[channelIdx];
            int64_t readPosition = bytesPerSample*(numChannels*sampleIdx + (channel - 1));
            int64_t writePos = sampleIdx*bytesPerSample*channels.size()+channelIdx*bytesPerSample;
            memcpy(&audioData[writePos], allChannelsTmpAudio + readPosition, bytesPerSample);
        }
    }

    delete[](allChannelsTmpAudio);
}

void AudioFileReader::close() {
    fclose(fPtr);
}

AudioFileReader *openWaveFile(std::string fileName) {
    AudioFileReader* outFR = new AudioFileReader();
    struct stat statBuf;
    stat(fileName.c_str(), &statBuf);
    outFR->headerSize = 44;

    FILE *f = fopen(fileName.c_str(), "rb");
    if (f == NULL) GODEC_ERR << "Failed to open audio file: " << fileName;

    std::vector<char> buffer;
    buffer.resize(12);

    // WAVE format description: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

    fread(&buffer[0], 1, 12, f);
    if (strncmp(&buffer[0], "RIFF", strlen("RIFF")) != 0 || strncmp(&buffer[0]+8, "WAVE", strlen("WAVE") != 0)) {
        GODEC_ERR << "Not a MS RIFF WAV file: " << fileName;
    }

    do {
        fread(&buffer[0], 1, 8, f);
        uint32_t chunkSize = *((uint32_t *)(&buffer[0] + 4));
        buffer.resize(chunkSize);
        //printf("Chunk type: %c%c%c%c, size %i\n", buffer[0],buffer[1],buffer[2], buffer[3],chunkSize);
        if (strncmp(&buffer[0],"fmt ",strlen("fmt ")) == 0) {
            fread(&buffer[0], 1, chunkSize, f);

            uint16_t audioFormat = *((uint16_t*)(&buffer[0] + 0));
            if (audioFormat == WAVE_FORMAT_PCM || audioFormat == WAVE_FORMAT_EXTENDED)
                outFR->audioType = PCM;
            else if (audioFormat == WAVE_FORMAT_ALAW)
                outFR->audioType = Alaw;
            else if (audioFormat == WAVE_FORMAT_MULAW)
                outFR->audioType = MuLaw;
            else {
                GODEC_ERR << "Unsupported audio format '" << audioFormat << "', only PCM, Alaw and MuLaw are supported in wav file.";
            }
            outFR->numChannels = *((uint16_t*)(&buffer[0] + 2));

            uint32_t samplingRate = *((uint32_t *)(&buffer[0] + 4));
            outFR->samplingFrequency = samplingRate;
            short bitsPerSample = *((short *)(&buffer[0] + 14));

            //printf("Format: numchan=%i, sr: %f, bps: %i.\n", outFR->numChannels, outFR->samplingFrequency, bitsPerSample); fflush(stdout);
            outFR->bytesPerSample = bitsPerSample / 8;
        } else if (
            (strncmp(&buffer[0],"fact",strlen("fact")) == 0) ||
            (strncmp(&buffer[0],"list",strlen("list")) == 0) ||
            (strncmp(&buffer[0],"LIST",strlen("LIST")) == 0)
        ) {
            fread(&buffer[0], 1, chunkSize, f);
        } else if (strncmp(&buffer[0],"data",strlen("data")) == 0) {
            outFR->headerSize = ftell(f);
            outFR->fileSize = statBuf.st_size - outFR->headerSize;
            outFR->fPtr = f;
            return outFR;
        } else {
            GODEC_ERR << "Unknown chunk type " << buffer[0] << buffer[1] << buffer[2] << buffer[3] << " in audio file";
        }
    } while(true);
    return NULL;
}


AudioFileReader *openNIST1AFile(std::string fileName) {
    AudioFileReader* outFR = new AudioFileReader();

    struct stat statBuf;
    stat(fileName.c_str(), &statBuf);

    FILE *f = fopen(fileName.c_str(), "rb");
    if (f == NULL) GODEC_ERR << "Error opening file " << fileName << ": " << strerror(errno) << std::endl;

    outFR->audioType = PCM; // Default
    outFR->numChannels = 1;
    char nistFileLine[1000];
    fgets(nistFileLine, 1000, f);
    trimLine(nistFileLine);
    if (strcmp(nistFileLine, "NIST_1A") != 0) {
        GODEC_ERR << "Not a NIST file (" << nistFileLine << ")";
    }
    long long headerSize;
    fgets(nistFileLine, 1000, f);
    trimLine(nistFileLine);
    sscanf(nistFileLine, "%lld", &headerSize);
    do {
        fgets(nistFileLine, 1000, f);
        trimLine(nistFileLine);
        if (strncmp(nistFileLine, "sample_rate", strlen("sample_rate")) == 0) {
            long long sampleRate;
            sscanf(nistFileLine, "sample_rate -i %lld", &sampleRate);
            outFR->samplingFrequency = (float)sampleRate;
        }
        if (strncmp(nistFileLine, "channel_count", strlen("channel_count")) == 0) {
            sscanf(nistFileLine, "channel_count -i %i", &outFR->numChannels);
            if (outFR->numChannels != 1 && outFR->numChannels != 2) {
                GODEC_ERR << "Only mono and stereo supported right now. Implement me!";
            }
        }
        if (strncmp(nistFileLine, "sample_n_bytes", strlen("sample_n_bytes")) == 0) {
            sscanf(nistFileLine, "sample_n_bytes -i %i", &outFR->bytesPerSample);
        }
        if (strncmp(nistFileLine, "sample_coding", strlen("sample_coding")) == 0) {
            char tmp[1024];
            int dummyInt;
            sscanf(nistFileLine, "sample_coding -s%d %s", &dummyInt, tmp);
            if (strncmp(tmp, "mu-law", strlen("mu-law")) == 0 || strncmp(tmp, "ulaw", strlen("ulaw")) == 0) {
                outFR->audioType = MuLaw;
            } else if (strncmp(tmp, "alaw", strlen("alaw")) == 0) {
                outFR->audioType = Alaw;
            }
        }
    } while (strcmp(nistFileLine, "end_head") != 0);

    outFR->fileSize = statBuf.st_size - headerSize;
    outFR->fPtr = f;
    outFR->headerSize = headerSize;
    fseek(f, outFR->headerSize, SEEK_SET);

    return outFR;
}

void parseAnalistLine(std::string& analistFileLine, std::string& waveFile, std::vector<int>& channels, std::string& typeString, int64_t& beginSample, int64_t& endSample, double& stretch, std::string& utteranceId, std::string& episodeName) {
    boost::algorithm::trim_right(analistFileLine);

    std::istringstream iss(analistFileLine);
    iss >> waveFile;
    stretch = 1.0;
    episodeName = "";
    do {
        std::string lineEl;
        iss >> lineEl;
        if (lineEl.compare("") == 0 || lineEl.empty()) continue;
        else if (lineEl == "-c") {
            std::string channelString;
            channels.resize(0);
            iss >> channelString;
            std::vector<std::string> channelEls;
            boost::split(channelEls, channelString, boost::is_any_of(","));
            for(int idx = 0; idx < channelEls.size(); idx++) {
                if (channelEls[idx] == "") continue;
                channels.push_back(boost::lexical_cast<int>(channelEls[idx]));
            }
        } else if (lineEl == "-t") {
            iss >> typeString;
        } else if (lineEl == "-f") {
            std::string sampleRange;
            iss >> sampleRange;
            sscanf(sampleRange.c_str(), "%lld-%lld", &beginSample, &endSample);
        } else if (lineEl == "-o") {
            iss >> utteranceId;
        } else if (lineEl == "-spkr") {
            iss >> episodeName;
        } else if (lineEl == "-e") {
            std::string dummyString;
            iss >> dummyString;
        } else if (lineEl == "-s") {
            iss >> stretch;
        } else {
            GODEC_ERR << "Unknown line element '" << lineEl << "'";
        }
    } while (iss);
    if (episodeName == "") {
        std::ostringstream episodeStream;
        std::string channelString = boost::algorithm::join( channels | boost::adaptors::transformed( static_cast<std::string(*)(int)>(std::to_string) ), ",");
        episodeStream << waveFile << "-" << channelString;
        episodeName = episodeStream.str();
    }
}


AnalistFileFeeder::AnalistFileFeeder(std::string analistFile, std::string _waveFileDir, std::string _waveFileExtension) {
    mWaveFileDir = _waveFileDir;
    mWaveFileExtension = _waveFileExtension;
    fflush(stdout);
    mListFileFp = fopen(analistFile.c_str(), "rb");
    if (mListFileFp == NULL) GODEC_ERR << "Failed to open " << analistFile;
    char analistFileLine[1000];
    while (fgets(analistFileLine, 1000, mListFileFp)) {
        std::string analistLineString(analistFileLine);

        std::string waveFile;
        std::vector<int> channels;
        std::string typeString;
        int64_t beginSample, endSample;
        double stretch;
        std::string utteranceId;
        std::string episodeName;
        parseAnalistLine(analistLineString, waveFile, channels, typeString, beginSample, endSample, stretch, utteranceId, episodeName);

        episodeList.push_back(episodeName);
    }
    fseek(mListFileFp, 0, SEEK_SET);

    utteranceCounter = 0;
}

bool AnalistFileFeeder::getNextUtterance(std::vector<unsigned char>& audioData, int& sampleWidth, std::string& utteranceId, std::string& episodeName, bool& episodeDone, bool& fileDone, std::string& waveFile, std::vector<int>& channels, std::string& formatString, float& audioChunkTimeInSeconds, int64_t& beginSample, float& uttOffsetInFileInSeconds) {
    utteranceCounter++;
    utteranceId = std::to_string(utteranceCounter);
    episodeDone = false;

    char analistFileLine[1000];
    if (fgets(analistFileLine, 1000, mListFileFp)) {
        std::string analistLineString(analistFileLine);

        std::string typeString;
        int64_t endSample;
        double vtlStretch;
        parseAnalistLine(analistLineString, waveFile, channels, typeString, beginSample, endSample, vtlStretch, utteranceId, episodeName);

        if (utteranceCounter == episodeList.size() || episodeName != episodeList[utteranceCounter]) episodeDone = true;

        std::string fullWavePath = mWaveFileDir+"/"+waveFile+"."+mWaveFileExtension;

        AudioFileReader* reader = NULL;
        std::string baseFormat = "";
        if (typeString == "WAV") {
            reader = openWaveFile(fullWavePath);
        } else if (typeString == "NIST_1A") {
            reader = openNIST1AFile(fullWavePath);
        } else {
            GODEC_ERR << "Unknown type '" << typeString << "'";
        }
        if (reader == NULL) {
            GODEC_ERR << "Couldn't open file " << fullWavePath;
        }

        if (reader->audioType == PCM) baseFormat = "PCM";
        else if (reader->audioType == MuLaw) baseFormat = "ulaw";
        else if (reader->audioType == Alaw) baseFormat = "alaw";

        std::stringstream ss;
        sampleWidth = reader->bytesPerSample*8;
        ss << std::setprecision(50) << "base_format=" << baseFormat << ";sample_width=" << sampleWidth << ";sample_rate=" << reader->samplingFrequency << ";vtl_stretch=" << vtlStretch << ";num_channels=" << channels.size() << ";";
        formatString = ss.str();
        reader->readData(audioData, beginSample, endSample, channels);
        audioChunkTimeInSeconds = audioData.size() /((float)channels.size()*reader->samplingFrequency*reader->bytesPerSample);
        uttOffsetInFileInSeconds = beginSample/(float)reader->samplingFrequency;
        reader->close();
        delete reader;

        fileDone = isEndOfFile(mListFileFp);
    } else return false;

    return true;
}

TextFileFeeder::TextFileFeeder(const std::string& text_file) {
    lineCounter = 0;
    text_reader.open(text_file, std::ifstream::in | std::ifstream::binary);
}

TextFileFeeder::~TextFileFeeder() {
}

bool TextFileFeeder::getNextUtterance(std::string& utteranceId, std::string& text, bool &fileDone) {
    if (isEndOfFile(text_reader)) {
        return false;
    } else {
        std::getline(text_reader, text);
        lineCounter++;
        utteranceId = (boost::format("text_line_%1%") % lineCounter).str();
        fileDone = isEndOfFile(text_reader);
        return true;
    }
}

JsonFileFeeder::JsonFileFeeder(const std::string& jsonFile) {
    itemCounter = 0;
    std::ifstream is(jsonFile);
    if (!is) {
        GODEC_ERR << "Fail to open file '" << jsonFile << "' for reading." << std::endl;

    }
    try {
        is >> jsonData;
    } catch(json::parse_error& e) {
        GODEC_ERR << "Fail to load JSON data from '" << jsonFile << "'. " << e.what() << std::endl;
    }

    if (!jsonData.is_array()) {
        GODEC_ERR << "Toplevel JSON data must be an array." << std::endl;
    }
}

bool JsonFileFeeder::getNextMessage(json &jsonMessage, json &jsonConvState) {
    if (itemCounter < jsonData.size()) {
        json jsonObj = jsonData[itemCounter++];

        if (!jsonObj.is_object() || jsonObj.find("json_message") == jsonObj.end() || jsonObj.find("conversation_state") == jsonObj.end()) {
            GODEC_ERR << "JsonFileFeeder: the " << itemCounter << "-th entry is not a JSON object with 'json_message' and 'conversation_state' fields." << std::endl;
        }
        jsonMessage = jsonObj["json_message"];
        jsonConvState = jsonObj["conversation_state"];
        return true;
    } else {
        return false;
    }
}

JsonFileFeeder::~JsonFileFeeder() {
}

NumpyFileFeeder::NumpyFileFeeder(std::string npzKeysListFile) {
    std::ifstream tmpIf(npzKeysListFile);
    mKeysListNumLines = std::count(std::istreambuf_iterator<char>(tmpIf), std::istreambuf_iterator<char>(), '\n');
    mKeysListFile.open(npzKeysListFile);
    if (!mKeysListFile) GODEC_ERR << "Failed to open " << npzKeysListFile;
    mKeyListFileLineCount = 0;
}

bool NumpyFileFeeder::getNextUtterance(float*& features, int64_t& numFrames, int& frameLength, std::string& utteranceId, std::string& episodeName, bool& episodeDone, bool& fileDone) {
    std::string listLine;
    mKeysListFile >> listLine;
    mKeyListFileLineCount++;
    if (mKeysListFile.eof()) return false;

    std::vector<std::string> listLineEls;
    boost::split(listLineEls, listLine, boost::is_any_of(":"));
    if (listLineEls.size() != 2) GODEC_ERR << "npz list file parsing error, line " << mKeyListFileLineCount << ". Format for each line is <npz file name>:<hash key insize npz for features>";
    std::string npzFile = listLineEls[0];
    std::string npzKey = listLineEls[1];
    cnpy::NpyArray data = cnpy::npz_load(npzFile, npzKey);

    if (data.shape.size() != 2) GODEC_ERR << "npz entry '" << npzKey << "' in file " << npzFile << " is not two-dimensional! Don't know how to feed";
    numFrames = data.shape[1];
    frameLength = data.shape[0];
    Eigen::MatrixXd doubleMatrix = Eigen::Map<Eigen::MatrixXd>(data.data<double>(), numFrames, frameLength);
    mCurrentFeatures = doubleMatrix.cast<float>().transpose();

    features = mCurrentFeatures.data();
    utteranceId = npzKey;
    episodeName = "dummy";
    episodeDone = mKeyListFileLineCount == mKeysListNumLines;
    fileDone = mKeysListFile.eof();
    return true;
}

LoopProcessor* FileFeederComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new FileFeederComponent(id, configPt);
}
std::string FileFeederComponent::describeThyself() {
    return "Component that feeds file content into the network. Can read analist, text, JSON and Numpy npz features";
}

/* FileFeederComponent::ExtendedDescription
This is the component for providing your Godec graph with data for processing. The different feeding formats in details:

"analist": A text file describing line-by-line segments in audio file (support formats: WAV and NIST_1A, i.e. SPHERE). The general format is "\<wave file base name without extension\> -c \<channel number, 1-based\> -t \<audio format\> -f \<sample start\>-\<sample end\> -o \<utterance ID\> -spkr \<speaker ID\>" . "wave_dir" parameter specifies the direction of where to find the wave files, "wave_extension" the file extension. The "feed_realtime_factor" specifies how much faster than realtime it should feed the audio (higher value = faster)

"text": A simple line-by-line file with text in it. The component will feed one line at a time, as a BinaryDecoderMessage with timestamps according to how many words were in the line

"json": A single file containing a JSON array of items. Each item will be pushed as a JsonDecoderMessage

"numpy_npz": A Python Numpy npz file. The parameter "keys_list_file" specifies a file which contains a line-by-line list of npz+key combo, e.g. "my_feats.npz:a", which would extract key "a" from my_feats.npz. "feature_chunk_size" sets the size of the feature chunks to be pushed

The "control_type" parameter describes whether the FileFeeder will just work off one single configuration on startup, i.e. batch processing ("single_on_startup"), or whether it should receive these configurations via an external slot ("external") that pushes the exact same component JSON configuration from the outside via the Java API. The latter is essentially for "dynamic batch processing" where the FileFeeder gets pointed to new data dynamically.

*/

boost::shared_ptr<FileFeederHolder> FileFeederComponent::GetFileFeederFromConfig( ComponentGraphConfig* configPt) {
    auto ffh = boost::shared_ptr<FileFeederHolder>(new FileFeederHolder());
    std::string sourceType = configPt->get<std::string>("source_type", "File type of source file (analist, text, numpy_npz, json)");
    std::string inputFile;
    if (sourceType != "numpy_npz") {
        inputFile = configPt->get<std::string>("input_file", "Input file");
    }
    ffh->inputFile = inputFile;

    if (sourceType == "analist") {
        ffh->mFeedRealtimeFactor = configPt->get<float>("feed_realtime_factor", "Controls how fast the audio is pushed. A value of 1.0 simulates soundcard reading of audio (i.e. pushing a 1-second chunk takes 1 second), a higher value pushes faster. Use 100000 for batch pushing");
        boost::filesystem::path waveFileDirPath(configPt->get<std::string>("wave_dir", "Audio waves directory"));
        std::string waveFileDir;
        try {
            if (waveFileDirPath.is_absolute()) {
                waveFileDir = waveFileDirPath.string();
            } else {
                waveFileDir = boost::filesystem::canonical(boost::filesystem::current_path() / waveFileDirPath).string();
            }
        } catch (...) {
            GODEC_ERR << "Couldn't find wave_dir '" << waveFileDirPath << "'";
        }
        std::string waveFileExtension = configPt->get<std::string>("wave_extension", "Wave file extension");
        ffh->analistFileFeeder = new AnalistFileFeeder((char*)inputFile.c_str(), (char*)waveFileDir.c_str(), (char*)waveFileExtension.c_str());
        ffh->chunkSizeInSamples = configPt->get<int>("audio_chunk_size", "Audio size in samples for each chunk");
        ffh->mTimeUpsampleFactor = configPt->get<int>("time_upsample_factor", "Factor by which the internal time stamps are increased. This is to prevent multiple subunits having the same time stamp.");
    } else if (sourceType == "text") {
        ffh->textFileFeeder = new TextFileFeeder(inputFile);
    } else if (sourceType == "json") {
        ffh->jsonFileFeeder = new JsonFileFeeder(inputFile);
    } else if (sourceType == "numpy_npz") {
        std::string npzKeysList = configPt->get<std::string>("keys_list_file", "File containing a list (line by line) of keys that are contained in the npz file and are then fed in that order");
        ffh->numpyFileFeeder = new NumpyFileFeeder(npzKeysList);
        ffh->chunkSizeInFrames = configPt->get<int>("feature_chunk_size", "Size in frames of each pushed chunk");
    } else {
        GODEC_ERR << "Unknown source type '" << sourceType << "'. Valid options are: analist, text, numpy_mpz, and json." << std::endl;
    }
    return ffh;
}

FileFeederComponent::FileFeederComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id, configPt) {

    mFFHChannel.checkIn(getLPId(false));

    mControlType = configPt->get<std::string>("control_type", "Where this FileFeeder gets its source data from: single-shot feeding on startup ('single_on_startup'), or as JSON input from an input stream ('external')");
    if (mControlType == "single_on_startup") {
        mFFHChannel.put(GetFileFeederFromConfig(configPt));
    } else if (mControlType == "external") {
        addInputSlotAndUUID(SlotControl, UUID_JsonDecoderMessage);
    } else GODEC_ERR << "Unknown control type " << mControlType;

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotOutput);
    requiredOutputSlots.push_back(SlotConversationState);
    initOutputs(requiredOutputSlots);
}

FileFeederComponent::~FileFeederComponent() {
    mFFHChannel.checkOut(getLPId(false));
    mFeedThread.interrupt();
    mFeedThread.join();
}

void FileFeederComponent::Start() {
    mFeedThread = boost::thread(&FileFeederComponent::FeedLoop, this);
    RegisterThreadForLogging(mFeedThread, mLogPtr, isVerbose());
    if (mControlType == "single_on_startup") {
        mFFHChannel.checkOut(getLPId(false));
    } else if (mControlType == "external") {
        LoopProcessor::Start();
    }
}

void FileFeederComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto jsonMsg = msgBlock.get<JsonDecoderMessage>(SlotControl);
    std::stringstream cleanJson;
    cleanJson << StripCommentsFromJSON(jsonMsg->getJsonObj().dump());
    json pt = json::parse(cleanJson);
    ComponentGraphConfig config("bla", pt, nullptr, nullptr);
    mFFHChannel.put(GetFileFeederFromConfig(&config));
}

std::string FileFeederComponent::SlotOutput = "output_stream";

void FileFeederComponent::FeedLoop() {
    ChannelReturnResult res;
    int64_t totalTime = -1;
    while (true) {
        boost::shared_ptr<FileFeederHolder> ffh;
        res = mFFHChannel.get(ffh, FLT_MAX);
        if (res == ChannelClosed) break;

        std::vector<unsigned char> audioData;
        int sampleWidth;
        float* features = NULL;
        int64_t nFrames = 0;
        int frameLength = 0;
        std::string utteranceId;
        std::string episodeName;
        std::string waveFile;
        std::vector<int> channels;
        std::string ctm_channel;
        bool episodeDone = false;
        bool fileDone = false;
        int64_t beginSamples = 0;
        std::string text;
        std::string formatString;
        float audioChunkTimeInSeconds;
        float uttOffsetInFileInSeconds;
        std::vector<std::string> wordStrings;
        std::vector<std::string> caseStrings;
        std::vector<std::string> puncStrings;
        std::vector<float> word_begin_times;
        std::vector<float> word_dur_times;
        json jsonMessage;
        json jsonConvState;

        while (
            ((ffh->analistFileFeeder != NULL) && (ffh->analistFileFeeder->getNextUtterance(audioData, sampleWidth, utteranceId, episodeName, episodeDone, fileDone, waveFile, channels, formatString, audioChunkTimeInSeconds, beginSamples, uttOffsetInFileInSeconds))) ||
            ((ffh->numpyFileFeeder != NULL) && (ffh->numpyFileFeeder->getNextUtterance(features, nFrames, frameLength, utteranceId, episodeName, episodeDone, fileDone))) ||
            ((ffh->textFileFeeder != NULL) && (ffh->textFileFeeder->getNextUtterance(utteranceId, text, fileDone))) ||
            ((ffh->jsonFileFeeder != NULL) && (ffh->jsonFileFeeder->getNextMessage(jsonMessage, jsonConvState)))
        ) {
            if (features != NULL) {
                int64_t chunk_size = ffh->chunkSizeInFrames == 0 ? nFrames : ffh->chunkSizeInFrames;
                float * feature_runner = features;
                uint64_t remaining_frames = nFrames;
                while (remaining_frames > 0) {
                    uint64_t chunk_frames = remaining_frames >= chunk_size ? chunk_size : remaining_frames;
                    remaining_frames -= chunk_frames;
                    Matrix featsMatrix = Eigen::Map<Matrix>(feature_runner, frameLength, chunk_frames);
                    std::vector<uint64_t> featureTimestamps;
                    for (uint64_t i = 0; i < chunk_frames; ++i) {
                        featureTimestamps.push_back(++totalTime);
                    }
                    bool utt_done = remaining_frames == 0;
                    feature_runner += frameLength*chunk_frames;
                    boost::format pfname("RAW[0:%1%]%%f");
                    pfname % (frameLength - 1);
                    pushToOutputs(SlotConversationState, ConversationStateDecoderMessage::create(totalTime, utteranceId, utt_done, episodeName, episodeDone&&utt_done));
                    pushToOutputs(SlotOutput, FeaturesDecoderMessage::create(totalTime, utteranceId, featsMatrix, pfname.str(), featureTimestamps));
                }
            } else if (audioData.size() != 0) {
                int64_t audioRunner = 0;
                auto feedStartTime = std::chrono::system_clock::now();
                while (audioRunner < audioData.size()) {
                    int64_t actualIncrement = std::min((int64_t)(audioData.size() - audioRunner), (int64_t)channels.size()*ffh->chunkSizeInSamples*(sampleWidth/8));
                    float secondsToAdd = audioChunkTimeInSeconds*((audioRunner+actualIncrement)/(float)audioData.size())/ffh->mFeedRealtimeFactor;

                    auto timeToWaitUntil = feedStartTime+std::chrono::milliseconds((int)(1000*secondsToAdd));
                    auto currTime = std::chrono::system_clock::now();
                    std::chrono::duration<double> diff = timeToWaitUntil - currTime;
                    if (diff.count() > 0) {
                        std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(diff));
                    }

                    bool isLastInUtt = (audioRunner + actualIncrement) == audioData.size();
                    totalTime += actualIncrement;
                    DecoderMessage_ptr convoMsg = ConversationStateDecoderMessage::create(ffh->mTimeUpsampleFactor*(totalTime+1)-1, utteranceId, isLastInUtt, episodeName, isLastInUtt && episodeDone);
                    pushToOutputs(SlotConversationState, convoMsg);

                    std::vector<unsigned char> pushData(audioData.begin() + audioRunner, audioData.begin() + audioRunner + actualIncrement);
                    auto outMsg = BinaryDecoderMessage::create(ffh->mTimeUpsampleFactor*(totalTime + 1) - 1, pushData, formatString);
                    (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("file_feeder_input_file", boost::lexical_cast<std::string>(ffh->inputFile));
                    (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("wave_file_name", waveFile);
                    std::string channelString = boost::algorithm::join( channels | boost::adaptors::transformed( static_cast<std::string(*)(int)>(std::to_string) ), ",");
                    (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("channel", channelString);
                    (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("speaker", episodeName);
                    (boost::const_pointer_cast<DecoderMessage>(outMsg))->addDescriptor("utterance_offset_in_file", boost::lexical_cast<std::string>(uttOffsetInFileInSeconds));

                    pushToOutputs(SlotOutput, outMsg);
                    audioRunner += actualIncrement;
                }
            } else if (ffh->textFileFeeder != NULL) {
                std::vector<std::string> wordVec;
                boost::split(wordVec, text, boost::is_any_of(" "));
                totalTime += wordVec.size();
                pushToOutputs(SlotConversationState, ConversationStateDecoderMessage::create(totalTime, utteranceId, true, episodeName, episodeDone));
                pushToOutputs(SlotOutput, BinaryDecoderMessage::create(totalTime, String2CharVec(text), "string"));
            } else if (ffh->jsonFileFeeder != NULL) {
                totalTime = jsonConvState["time"].get<int64_t>();
                utteranceId = jsonConvState["utterance_id"].get<std::string>();
                episodeName = jsonConvState["conversation_id"].get<std::string>();
                bool endOfUtt = jsonConvState["end_of_utterance"].get<bool>();
                episodeDone = jsonConvState["end_of_conversation"].get<bool>();
                pushToOutputs(SlotConversationState, ConversationStateDecoderMessage::create(totalTime, utteranceId, endOfUtt, episodeName, episodeDone));
                pushToOutputs(SlotOutput, JsonDecoderMessage::create(totalTime, jsonMessage));

            }
        }
    }
    Shutdown();
}

}
