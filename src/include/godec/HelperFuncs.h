#pragma once
#define NOMINMAX
#include <string>
#include "Eigen/Eigen"
#include "Eigen/src/Core/util/Macros.h"
#include "version.h"
#include "boost/function.hpp"
#include "json.hpp"
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/timer/timer.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/uuid/uuid.hpp>
using namespace boost::uuids;
#include <set>
#include <jni.h>
#undef PAGE_SIZE
#undef PAGE_MASK
#include "boost/thread/thread.hpp"
#include <unordered_map>
#include <unordered_set>
using std::unordered_map;
using std::unordered_set;

#ifndef _MSC_VER
#define STRING2(x) #x
#define STRING(x) STRING2(x)
#if ((EIGEN_WORLD_VERSION != GODEC_EIGEN_WORLD_VERSION) || (EIGEN_MAJOR_VERSION != GODEC_EIGEN_MAJOR_VERSION))
#pragma message "This version of Godec was compiled with Eigen library version " STRING(GODEC_EIGEN_MAJOR_VERSION) "." STRING(GODEC_EIGEN_MAJOR_VERSION) ", but your Eigen version is " STRING(EIGEN_WORLD_VERSION) "." STRING(EIGEN_MAJOR_VERSION) ". This version mismatch can cause stack corruptions"
#error
#endif
#endif

namespace Godec {

using json = nlohmann::json;

// Eigen convenience mapping
typedef float Real;
typedef Eigen::VectorXf Vector;
typedef Eigen::RowVectorXf RowVector;
typedef Eigen::Ref<Vector> VectorRef;
typedef Eigen::MatrixXf Matrix;
typedef Eigen::Ref<Matrix> MatrixRef;
typedef Eigen::DiagonalMatrix<float, Eigen::Dynamic> DiagonalMatrix;
typedef Eigen::ArrayXf Array;
typedef Eigen::MatrixBase<Matrix> MatrixBase;
typedef Eigen::Map<Eigen::Matrix<float,1,Eigen::Dynamic> > MapVectorToMatrix;
typedef Eigen::Map<Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic> > MapMatrixToMatrix;
typedef Eigen::Map<Eigen::VectorXf> MapMatrixToVector;

#define M_2PI 6.283185307179586476925286766559005

// boolean to string
#define b2s(b) (b ? "y" : "n")

std::string CdToFilePath(std::string path); // Returns cwd, for restoring later

// By adding 'KALDI_NOEXCEPT(bool)' immediately after function declaration,
// we can tell the compiler that the function must-not produce
// exceptions (true), or may produce exceptions (false):
#if _MSC_VER >= 1900 || (!defined(_MSC_VER) && __cplusplus >= 201103L)
#define GODEC_NOEXCEPT(Predicate) noexcept((Predicate))
#elif defined(__GXX_EXPERIMENTAL_CXX0X__) && \
      (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6)
#define GODEC_NOEXCEPT(Predicate) noexcept((Predicate))
#else
#define GODEC_NOEXCEPT(Predicate)
#endif

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

// Logging structures. Each thread maintains its own logging handle
extern unordered_map<std::string, std::pair<bool, FILE*>> GlobalThreadId2LogHandle;

void RegisterThreadForLogging(boost::thread& thread, FILE* logPtr, bool verbose);

struct LogMessageEnvelope {
    enum Severity {
        kError = -1,
        kInfo = 0,
    };
    // An 'enum Severity' value, or a positive number indicating verbosity level.
    int severity;
    std::string func;
    std::string file;
    int32_t line;
};

class GodecErrorLogger {
  public:
    /// Constructor stores the info,
    GodecErrorLogger(LogMessageEnvelope::Severity severity,
                     const char *func,
                     const char *file,
                     int32_t line);

    /// Destructor, calls 'HandleMessage' which prints the message,
    /// (since C++11 a 'throwing' destructor must be declared 'noexcept(false)')
    ~GodecErrorLogger() GODEC_NOEXCEPT(false);

    /// The hook for the 'insertion operator', e.g.
    /// 'KALDI_LOG << "Message,"',
    inline std::ostream &stream() { return ss_; }

  private:
    /// The logging function,
    static void HandleMessage(const LogMessageEnvelope &env, const char *msg);

  private:
    LogMessageEnvelope envelope_;
    std::ostringstream ss_;
};


#define GODEC_ERR Godec::GodecErrorLogger(LogMessageEnvelope::kError, __func__, __FILE__, __LINE__).stream()
#define GODEC_INFO Godec::GodecErrorLogger(LogMessageEnvelope::kInfo, __func__, __FILE__, __LINE__).stream()

std::vector<unsigned char> String2CharVec(std::string s);
std::string CharVec2String(std::vector<unsigned char> v);

double TwoDigitsPrecisionRound(double x);

void OverlayPropertyTrees(const json& tree1, const std::string& tree1Path, const json& tree2, const std::string& tree2Path, json& outTree);

jobject CreateJNIVector(JNIEnv* env, Vector& data);
jobject CreateJNIMatrix(JNIEnv* env, Matrix& data);

std::string StripCommentsFromJSON(std::string in);
std::string Json2String(json js);

#define DEBUG_COUT(X) std::cout << "DEBUG: " << boost::filesystem::path(__FILE__).filename().string() << ":" << __LINE__ << ": " << X << std::endl << std::flush;

} //namespace Godec

