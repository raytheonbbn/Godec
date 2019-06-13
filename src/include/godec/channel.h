#pragma once

#include <list>
#include <float.h>
#include <boost/thread/mutex.hpp>

namespace Godec {

enum ChannelReturnResult {
    ChannelNewItem,
    ChannelClosed,
    ChannelTimeout
};

// This is the channel that contains a list of types items. This is overall a very useful class, as you can use it somewhat like Go channels, or "futures"

template<class item>
class channel {
  private:
    std::list<item> mQueue;
    boost::mutex m;
    boost::condition_variable putCv;
    boost::condition_variable getCv;
    std::string mId;
    bool mVerbose;
    int maxItems;
    int mRefCounter;
  public:
    channel() : maxItems(INT_MAX), mRefCounter(0) {
    }

    void checkIn(std::string who) {
        boost::unique_lock<boost::mutex> lock(m);
        mRefCounter++;
        //if (mVerbose) std::cout << "Channel " << mId << ": " << who << " checked in. New ref count = " << mRefCounter << std::endl << std::flush;
    }

    void checkOut(std::string who) {
        boost::unique_lock<boost::mutex> lock(m);
        mRefCounter--;
        //if (mVerbose) std::cout << "Channel " << mId << ": " << who << " checked out. New ref count = " << mRefCounter << std::endl << std::flush;
        putCv.notify_all();
        getCv.notify_all();
    }

    void setIdVerbose(std::string id, bool verbose) {
        mId = id;
        mVerbose = verbose;
    }

    std::string getId() { return mId; }

    int getRefCount() { return mRefCounter; }

    void setMaxItems(int _maxItems) {
        maxItems = _maxItems;
    }

    bool seenItAll() {
        return mRefCounter == 0 && mQueue.size() == 0;
    }

    void put(const item i) {
        boost::unique_lock<boost::mutex> lock(m);
        if (mRefCounter == 0) GODEC_ERR << "Channel " << mId << ": Somebody is trying push even though ref counter is 0!";
        putCv.wait(lock, [&]() { return mQueue.size() < maxItems; });
        mQueue.push_back(i);
        //if (mVerbose) std::cout << "Channel " << mId << ": Put item, notifying" << std::endl;
        getCv.notify_all();
    }

    int32_t getNumItems() { return (int32_t)mQueue.size(); }

    ChannelReturnResult get(item& out) {
        return get(out, FLT_MAX);
    }

    ChannelReturnResult get(item& out, float maxTimeout) {
        boost::unique_lock<boost::mutex> lock(m);
        if (seenItAll()) return ChannelClosed;
        if (mQueue.size() == 0 && maxTimeout > 0.0f) {
            bool waitResult = false;
            if (maxTimeout == FLT_MAX) {
                getCv.wait(lock, [&]() {
                    return seenItAll() || !mQueue.empty();
                });
                waitResult = true;
            } else {
                const boost::system_time timeLong = boost::get_system_time() + boost::posix_time::milliseconds((long)(1000.0f*maxTimeout));
                waitResult = getCv.timed_wait(lock, timeLong, [&]() {
                    return seenItAll() || !mQueue.empty();
                });
            }
            if (!waitResult) return ChannelTimeout;
        }
        if (seenItAll()) return ChannelClosed;
        out = mQueue.front();
        mQueue.pop_front();
        putCv.notify_all();
        return ChannelNewItem;
    }

    ChannelReturnResult getAll(std::vector<item>& out, float maxTimeout) {
        boost::unique_lock<boost::mutex> lock(m);

        if (seenItAll()) return ChannelClosed;
        if (mQueue.size() == 0 && maxTimeout > 0.0f) {
            maxTimeout = std::min(maxTimeout, 60.0f * 60 * 24);
            const boost::system_time timeLong = boost::get_system_time() + boost::posix_time::milliseconds((long)(1000.0f*maxTimeout));
            bool waitResult = getCv.timed_wait(lock, timeLong, [&]() {
                //if (mVerbose) std::cout << "Channel " << mId << ": Woke up, queue size " << mQueue.size() << std::endl;
                return seenItAll() || !mQueue.empty();
            });
            //if (mVerbose) std::cout << "Channel " << mId << ": Wait result = " << b2s(waitResult) << std::endl;
            if (!waitResult) return ChannelTimeout;
        }
        if (seenItAll()) return ChannelClosed;
        out = std::vector<item>(mQueue.begin(), mQueue.end());
        mQueue.clear();
        putCv.notify_all();
        return ChannelNewItem;
    }
};

} // namespace Godec
