#include <iostream>
#include <godec/HelperFuncs.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional/optional.hpp>

namespace Godec {

std::string CdToFilePath(std::string filePath) {
    boost::filesystem::path cwd(boost::filesystem::current_path());
    boost::filesystem::path bFilePath(filePath);
    boost::filesystem::path bFileParent = bFilePath.parent_path();
    if (!bFileParent.empty()) boost::filesystem::current_path(bFileParent);
    return cwd.string();
}

std::string Json2String(json js) {
    std::string out = js.dump();
    if (out.size() > 0 && out[0] == '"') out = out.substr(1);
    if (out.size() > 0 && out[out.size()-1] == '"') out = out.substr(0, out.size()-1);
    return out;
}

unordered_map<std::string, std::pair<bool, FILE*>> GlobalThreadId2LogHandle;

void RegisterThreadForLogging(boost::thread& thread, FILE* logPtr, bool verbose) {
    std::string threadId = boost::lexical_cast<std::string>(thread.get_id());
    GlobalThreadId2LogHandle[threadId] = std::make_pair(verbose, logPtr);
}

GodecErrorLogger::GodecErrorLogger(LogMessageEnvelope::Severity severity, const char *func, const char *file, int32_t line) {
    // Obviously, we assume the strings survive the destruction of this object.
    envelope_.severity = severity;
    envelope_.func = func;
    boost::filesystem::path p(file);
    envelope_.file = p.filename().string();  // Pointer inside 'file'.
    envelope_.line = line;
}


GodecErrorLogger::~GodecErrorLogger() GODEC_NOEXCEPT(false) {
    // remove trailing '\n',
    std::string str = ss_.str();
    while (!str.empty() && str[str.length() - 1] == '\n')
        str.resize(str.length() - 1);

    // print the message (or send to logging handler)
    GodecErrorLogger::HandleMessage(envelope_, str.c_str());
}


void GodecErrorLogger::HandleMessage(const LogMessageEnvelope &envelope, const char *message) {
    std::string bar = "################################################################################";
    std::stringstream outString;
    if (envelope.severity == LogMessageEnvelope::kError ) {
        outString << "\033[31m" << bar << std::endl << "ERROR\n";
        // fill the other info from the envelope,
        outString <<
#ifdef DEBUG
                  "Godec::" << envelope.func << "()\n" << envelope.file << ':' << envelope.line << "\n" <<
#endif
                  "";
    } else {
        outString << "";
    }
    outString << message;
    if (envelope.severity == LogMessageEnvelope::kError ) {
        outString << std::endl << bar << "\033[0m";
    }

    std::string threadId = boost::lexical_cast<std::string>(boost::this_thread::get_id());
    auto logPair = (GlobalThreadId2LogHandle.find(threadId) != GlobalThreadId2LogHandle.end()) ? GlobalThreadId2LogHandle[threadId] : std::make_pair(true, stderr);
    if (logPair.first || envelope.severity == LogMessageEnvelope::kError) {
        fprintf(logPair.second, "%s\n", outString.str().c_str());
        fflush(logPair.second);
        if (envelope.severity == LogMessageEnvelope::kError && logPair.second != stderr) { // Make sure errors are seen on command line, no matter what
            fprintf(stderr, "%s\n", outString.str().c_str());
            fflush(stderr);
        }
    }

    if (envelope.severity == LogMessageEnvelope::kError) {
        // Should we throw exception, or abort?
        if (std::uncaught_exceptions() == 0) {
            // throw exception with empty message,
            throw std::runtime_error(""); // KALDI_ERR,
        } else {
            // If we got here, this thread has already thrown exception,
            // and this exception has not yet arrived to its 'catch' clause...
            // Throwing a new exception would be unsafe!
            // (can happen during 'stack unwinding', if we have 'KALDI_ERR << msg'
            // in a destructor of some local object).
            abort();
        }
    }
}

std::vector<unsigned char> String2CharVec(std::string s) {
    return std::vector<unsigned char>(s.begin(), s.end());
}
std::string CharVec2String(std::vector<unsigned char> v) {
    if (v.size() == 0) return "";
    return std::string(v.begin(), v.end());
}

double TwoDigitsPrecisionRound(double x) {
    return roundf(x * 100) / 100;
}

bool getJsonPath(const json& j, json::json_pointer jp, json& outJ) {
    try {
        outJ = j.at(jp);
        return true;
    } catch (...) {
        return false;
    }
}

// Tree 2 gets puts on top of tree 1
void OverlayPropertyTrees(const json& tree1, const std::string& tree1Path, const json& tree2, const std::string& tree2Path, json& outTree) {
    std::unordered_map<std::string,bool> leavesToVisit;
    json leaf1;
    if (getJsonPath(tree1, json::json_pointer(tree1Path), leaf1)) {
        for(auto& it : leaf1.items()) {
            if (it.key() == "") continue;
            leavesToVisit[it.key()] = false;
        }
    }
    json leaf2;
    if (getJsonPath(tree2, json::json_pointer(tree2Path), leaf2)) {
        for(auto& it : leaf2.items()) {
            if (it.key() == "") continue;
            auto key = it.key();
            bool forceCreation = false;
            if (key[0] == '!') {
                forceCreation = true;
                key = key.substr(1);
            }
            leavesToVisit[key] |= forceCreation;
        }
    }
    for(auto leafIt = leavesToVisit.begin(); leafIt != leavesToVisit.end(); leafIt++) {
        auto leaf1Path = json::json_pointer(tree1Path +"/"+ leafIt->first);
        auto leaf2Path = json::json_pointer(tree2Path +"/"+ ((leafIt->second ? "!" : "") + leafIt->first));
        json dummy1, dummy2;
        if (getJsonPath(tree2, leaf2Path, dummy2) && dummy2.is_string() && Json2String(dummy2)[0] == '#') {
            // Changing the name of the component
            outTree[json::json_pointer(tree1Path +"/#"+ leafIt->first)] = tree1[leaf1Path];
            continue;
        }
        if (!getJsonPath(tree1, leaf1Path, dummy1) && !leafIt->second) GODEC_ERR << "Trying to overwrite non-existing key '" << leafIt->first << "'";
        if (getJsonPath(tree2, leaf2Path, dummy2) && dummy2.is_string() && (getJsonPath(tree1, leaf1Path, dummy1) && dummy1.is_string())) {
            outTree[leaf1Path] = tree2[leaf2Path];
        } else if (getJsonPath(tree2, leaf2Path, dummy2) && (!getJsonPath(tree1, leaf1Path, dummy1) || tree1[leaf1Path] == "")) {
            outTree[leaf1Path] = tree2[leaf2Path];
        } else if (getJsonPath(tree1, leaf1Path, dummy1) && (!getJsonPath(tree2, leaf2Path, dummy2) || tree2[leaf2Path] == "")) {
            outTree[leaf1Path] = tree1[leaf1Path];
        } else {
            OverlayPropertyTrees(tree1, leaf1Path, tree2, leaf2Path, outTree);
        }
    }
}

/* JNI helpers */

jobject CreateJNIVector(JNIEnv* env, Vector& data) {
    jclass VectorClass = env->FindClass("com/bbn/godec/Vector");
    jmethodID jVectorInit = env->GetMethodID(VectorClass, "<init>", "([F)V");
    jfloatArray jAudioArrayObj = env->NewFloatArray((jsize)data.size());
    env->SetFloatArrayRegion(jAudioArrayObj, 0, (jsize)data.size(), data.data());
    return env->NewObject(VectorClass, jVectorInit, jAudioArrayObj);
}

jobject CreateJNIMatrix(JNIEnv* env, Matrix& data) {
    jclass VectorClass = env->FindClass("com/bbn/godec/Vector");
    jclass MatrixClass = env->FindClass("com/bbn/godec/Matrix");
    jmethodID jMatrixInit = env->GetMethodID(MatrixClass, "<init>", "([Lcom/bbn/godec/Vector;)V");
    jobjectArray jVectorArray = env->NewObjectArray((jsize)data.cols(), VectorClass, NULL);
    for (int colIdx = 0; colIdx < data.cols(); colIdx++) {
        Vector featCol = data.col(colIdx);
        env->SetObjectArrayElement(jVectorArray, colIdx, CreateJNIVector(env, featCol));
    }
    return env->NewObject(MatrixClass, jMatrixInit, jVectorArray);
}

void StripJSONComment(const std::string& str, size_t start, size_t end, std::string& out) {
    for (size_t i = start; i < end; i++) {
        char ch = str[i];

        if (isspace(ch)) {
            out.push_back(ch);
        } else {
            out.push_back(' ');
        }
    }
}

enum class JSONStripState {
    NONE,
    SINGLE,
    MULTI
};

/* Based on https://github.com/andrew-d/json-strip, which in turn is based on https://github.com/sindresorhus/strip-json-comments/ . MIT license */
std::string StripCommentsFromJSON(std::string str) {
    std::string ret;
    ret.reserve(str.length());

    char currentChar, nextChar;
    bool insideString = false;
    JSONStripState commentType = JSONStripState::NONE;

    size_t offset = 0;
    for (size_t i = 0; i < str.length(); i++) {
        currentChar = str[i];

        if (i < str.length() - 1) {
            nextChar = str[i + 1];
        } else {
            nextChar = '\0';
        }

        // If we're not in a comment, check for a quote.
        if (commentType == JSONStripState::NONE && currentChar == '"') {
            bool escaped = false;

            // If the previous character was a single slash, and the one before
            // that was not (i.e. the previous character is escaping this quote
            // and is not itself escaped), then the quote is escaped.
            if (i >= 2 && str[i - 1] == '\\' && str[i - 2] != '\\') {
                escaped = true;
            }

            if (!escaped) {
                insideString = !insideString;
            }
        }

        if (insideString) {
            continue;
        }

        if (commentType == JSONStripState::NONE && currentChar == '/' && nextChar == '/') {
            ret.append(str, offset, i - offset);
            offset = i;
            commentType = JSONStripState::SINGLE;

            // Skip second '/'
            i++;
        } else if (commentType == JSONStripState::SINGLE && currentChar == '\r' && nextChar == '\n') {
            // Skip '\r'
            i++;

            commentType = JSONStripState::NONE;
            StripJSONComment(str, offset, i, ret);
            offset = i;

            continue;
        } else if (commentType == JSONStripState::SINGLE && currentChar == '\n') {
            commentType = JSONStripState::NONE;
            StripJSONComment(str, offset, i, ret);
            offset = i;
        } else if (commentType == JSONStripState::NONE && currentChar == '/' && nextChar == '*') {
            ret.append(str, offset, i - offset);
            offset = i;
            commentType = JSONStripState::MULTI;

            // Skip the '*'
            i++;
            continue;
        } else if (commentType == JSONStripState::MULTI && currentChar == '*' && nextChar == '/') {
            // Skip '*'
            i++;

            commentType = JSONStripState::NONE;
            StripJSONComment(str, offset, i + 1, ret);
            offset = i + 1;
            continue;
        }
    }
    ret.append(str, offset, str.length() - offset);
    ret.shrink_to_fit();
    return ret;
}

} // namespace Godec
