#define _USE_MATH_DEFINES
#include <cmath>
#include "AccumCovariance.h"
#include <iostream>
#include <unsupported/Eigen/MatrixFunctions>

namespace Godec {

boost::shared_ptr<AccumCovariance> AccumCovariance::make(int featureDimension, CovarianceType type, bool normMean, bool normVar, float decayRate) {
    return boost::shared_ptr<AccumCovariance>(new AccumCovariance(featureDimension, type, normMean, normVar, decayRate));
}

AccumCovariance::AccumCovariance(int featureDimension, CovarianceType type, bool normMean, bool normVar, float decayRate) {
    mType = type;
    mNormMean = normMean;
    mNormVar = normVar;
    accumMean = Vector(featureDimension);
    accumCovar = Matrix(featureDimension, featureDimension);
    mDecayRate = decayRate;
    reset();
}

void AccumCovariance::reset() {
    accumMean.setConstant(0.0f);
    accumCovar.setConstant(0.0f);
    accumN = 0;
    covarIsCurrent = false;
    invCovarIsCurrent = false;
}

void AccumCovariance::addData(const Matrix& newData) {
    if (mType == Nop) {
        accumCovar = Matrix::Identity(newData.rows(), newData.rows());
        return;
    }

    int64_t newDataN = newData.cols();
    Vector newDataMean = mNormMean ? (Vector)newData.rowwise().mean() : Vector::Zero(newData.rows());

    Matrix newDataCentered = newData.colwise() - newDataMean;
    Matrix newDataCovar = (mType == Full) ? newData * newDataCentered.adjoint() : (Matrix)newDataCentered.rowwise().squaredNorm().asDiagonal();

    double nX = (double)accumN + (double)newDataN;
    Vector meanDelta = getMean(true) - newDataMean;
    double scaleFactor = (double)accumN*(double)newDataN / nX;

    Matrix meanDeltaSquared = (mType == Full) ? meanDelta*meanDelta.transpose() : (Matrix)((Matrix)meanDelta.asDiagonal()*(Matrix)meanDelta.asDiagonal());
    accumCovar += newDataCovar + meanDeltaSquared*scaleFactor;
    accumMean += newDataN*newDataMean;
    accumN = nX;

    if (mDecayRate != 1.0f) {
        double cumDecayRate = exp(log(mDecayRate)*newDataN);
        accumN *= cumDecayRate; // Simply pretend we have less history
        accumMean *= cumDecayRate;
        accumCovar *= cumDecayRate;
    }

    covarIsCurrent = false;
    invCovarIsCurrent = false;
}

void AccumCovariance::addData(const Vector& newData) {
    addData((Matrix)newData.transpose());
}

Matrix AccumCovariance::getCovariance(bool biased) {
    if (covarIsCurrent && currentCovarBias == biased) return currentCovar;
    currentCovar = accumCovar/(accumN - (biased ? 0 : 1));
    covarIsCurrent = true;
    currentCovarBias = biased;
    return currentCovar;
}

Vector AccumCovariance::getMean(bool biased) {
    double n = accumN - (biased ? 0 : 1);
    if (n == 0) return Vector::Zero(accumMean.size());
    return accumMean/n;
}

Matrix AccumCovariance::getInvCovariance(bool biased) {
    if (invCovarIsCurrent && currentInvCovarBias == biased) return currentInvCovar;
    if (mType == Diagonal) {
        currentInvCovar = getCovariance(biased);
        if (currentInvCovar.diagonal().array().abs().minCoeff() > std::numeric_limits<double>::epsilon()) {
            currentInvCovar.diagonal() = currentInvCovar.diagonal().array().inverse();
        } else {
            currentInvCovar = Matrix::Identity(currentInvCovar.rows(), currentInvCovar.cols());
        }
    } else {
        currentInvCovar = pinv(getCovariance(biased));
    }
    invCovarIsCurrent = true;
    currentInvCovarBias = biased;
    return currentInvCovar;
}

Matrix AccumCovariance::normalize(const Matrix& data) {
    Matrix meanData = mNormMean ? (data.colwise() - getMean(true)) : data;
    Matrix invCov = getInvCovariance(false);
    Matrix sqrtInvCov;
    if (!mNormVar) return meanData;
    if (mType == Diagonal) {
        sqrtInvCov = invCov;
        sqrtInvCov.diagonal() = invCov.diagonal().array().sqrt();
    } else {
        sqrtInvCov = invCov.sqrt();
    }
    return sqrtInvCov*meanData;
}

Vector AccumCovariance::normalize(const Vector& data) {
    return normalize((Matrix)data.transpose()).row(0);
}

Vector AccumCovariance::getLogLikelihood(const Matrix& data) {
    Matrix xMinusMu = data.colwise()-getMean(true);
    Vector ll(data.cols());
    double covarLogDet = 0.0;
    Matrix covar = getCovariance(false);
    Matrix invCovar = getInvCovariance(false);
    if (mType == Diagonal) {
        covarLogDet = covar.diagonal().array().log().sum();
    } else {
        Eigen::ColPivHouseholderQR<Matrix> dec(covar);
        covarLogDet = dec.logAbsDeterminant();
    }
    for(int colIdx = 0; colIdx < data.cols(); colIdx++) {
        ll(colIdx) = -0.5*(covarLogDet+xMinusMu.col(colIdx).transpose()*invCovar*xMinusMu.col(colIdx)+data.rows()*log(2*M_PI));
    }
    return ll;
}

}
