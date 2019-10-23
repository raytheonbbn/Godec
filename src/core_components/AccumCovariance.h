#pragma once

#include <godec/HelperFuncs.h>
#include <iostream>

namespace Godec {

template<typename MatrixType>
MatrixType pinv(const MatrixType &a, double epsilon = std::numeric_limits<double>::epsilon()) {
    Eigen::JacobiSVD<MatrixType> svd(a, Eigen::ComputeThinU | Eigen::ComputeThinV);
    if (svd.nonzeroSingularValues() == 0) {
        return Matrix::Identity(a.rows(), a.cols());
    }
    double tolerance = epsilon * std::max(a.cols(), a.rows()) *svd.singularValues().array().abs()(0);
    return svd.matrixV() *  (svd.singularValues().array().abs() > tolerance).select(svd.singularValues().array().inverse(), 0).matrix().asDiagonal() * svd.matrixU().adjoint();
}

enum CovarianceType {
    Nop,
    Full,
    Diagonal
};

class AccumCovariance {
  public:
    static boost::shared_ptr<AccumCovariance> make(int featureDimension, CovarianceType type, bool normMean, bool normVar, float decayRate = 1.0f);
    // Everything here is considered column-major, i.e. each data vector is a column
    void addData(const Matrix& data);
    void addData(const Vector& data);
    Matrix normalize(const Matrix& data);
    Vector normalize(const Vector& data);
    Matrix getCovariance(bool biased);
    Matrix getInvCovariance(bool biased);
    Vector getMean(bool biased);
    // Produces the log likelihood of each data column, as per current statistics
    Vector getLogLikelihood(const Matrix& data);
    void reset();
  private:
    AccumCovariance(int featureDimension, CovarianceType type, bool normMean, bool normVar, float decayRate);
    CovarianceType mType;

    bool mNormMean;
    bool mNormVar;

    Matrix accumCovar;
    Vector accumMean;
    double accumN;
    float mDecayRate;

    Matrix currentCovar;
    Matrix currentInvCovar;
    bool covarIsCurrent;
    bool currentCovarBias;
    bool invCovarIsCurrent;
    bool currentInvCovarBias;
};

}
