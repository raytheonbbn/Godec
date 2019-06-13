#pragma once

#include <stdint.h>
#include <map>
#include <vector>
#include <list>
#include "HelperFuncs.h"

namespace Godec {

class DecoderMessage;
typedef boost::shared_ptr<const DecoderMessage> DecoderMessage_ptr;

// This structure is the one that contains the lined-up messages and attempts to slice out a continuous ("coherent") chunk of messages. Confer with the documentation for what exactly that means

class timeCompare {
  public:
    bool operator()(const uint64_t x, const uint64_t y) const { return x > y; }
};

// One stream
class SingleTimeStream : public  std::vector<DecoderMessage_ptr> {
  public:
    SingleTimeStream();
    bool canSliceAt(uint64_t, bool verbose, std::string& id);
    bool sliceOut(uint64_t, DecoderMessage_ptr& sliceMsg, bool verbose, std::string& id);
  private:
    int64_t mStreamOffset;
};

// All streams together
class TimeStreams {
  public:
    void setIdVerbose(std::string _id, bool verbose_) { mId = _id; mVerbose = verbose_; }
    void addMessage(DecoderMessage_ptr msg, std::string slot);
    void addStream(std::string streamName);
    std::string getLeastFilledSlot();
    std::string print();
    unordered_map<std::string, DecoderMessage_ptr> getNewCoherent(int64_t& cutoff);
    SingleTimeStream& getStream(std::string slot) { return mStream[slot]; }
    bool isEmpty();
  private:
    std::map<std::string, SingleTimeStream> mStream;
    std::string mId;
    bool mVerbose;
};

} // namespace Godec
