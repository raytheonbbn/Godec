#include "MatrixApply.h"
#include "cnpy.h"

namespace Godec {

LoopProcessor* MatrixApplyComponent::make(std::string id, ComponentGraphConfig* configPt) {
    return new MatrixApplyComponent(id, configPt);
}
std::string MatrixApplyComponent::describeThyself() {
    return "Applies a matrix (from a stream) to an incoming feature stream";
}

MatrixApplyComponent::MatrixApplyComponent(std::string id, ComponentGraphConfig* configPt) :
    LoopProcessor(id,configPt) {
    addInputSlotAndUUID(SlotFeatures, UUID_FeaturesDecoderMessage);

    mMatrixSource = configPt->get<std::string>("matrix_source", "Where the matrix comes from: From a file ('file') or pushed in through an input stream ('stream')");
    mAugmentFeatures = configPt->get<bool>("augment_features", "Whether to add a row of 1.0 elements to the bottom of the incoming feature vector (to enable all affine transforms)");
    if (mMatrixSource == "file") {
        std::string matrixFilename = configPt->get<std::string>("matrix_npy", "matrix file name. Matrix should be plain Numpy 'npy' format");
        cnpy::NpyArray data = cnpy::npy_load(matrixFilename);
        Eigen::MatrixXd doubleMatrix = Eigen::Map<Eigen::MatrixXd>(data.data<double>(), data.shape[1], data.shape[0]);
        mFixedMatrix = doubleMatrix.cast<float>().transpose();
    } else if (mMatrixSource == "stream") {
        addInputSlotAndUUID(SlotMatrix, UUID_MatrixDecoderMessage);
    } else GODEC_ERR << "Unknown matrix source '" << mMatrixSource << "'";

    std::list<std::string> requiredOutputSlots;
    requiredOutputSlots.push_back(SlotTransformedFeatures);
    initOutputs(requiredOutputSlots);
}

MatrixApplyComponent::~MatrixApplyComponent() {}

void MatrixApplyComponent::ProcessMessage(const DecoderMessageBlock& msgBlock) {
    auto featMsg = msgBlock.get<FeaturesDecoderMessage>(SlotFeatures);
    const Matrix& baseFeats = featMsg->mFeatures;
    Matrix mToApply(0,0);
    if (mMatrixSource == "file") {
        mToApply = mFixedMatrix;
    } else {
        auto matrixMsg = msgBlock.get<MatrixDecoderMessage>(SlotMatrix);
        mToApply = matrixMsg->mMat;
    }

    Matrix procFeats = baseFeats;
    if (mAugmentFeatures) {
        procFeats.conservativeResize(procFeats.rows()+1,procFeats.cols());
        procFeats.bottomRows(1) = Vector::Ones(procFeats.cols());
    }

    if (mToApply.cols() != procFeats.rows()) GODEC_ERR << getLPId() << ": (incoming  features #rows " << (mAugmentFeatures ? "+1" : "") << ") != (Matrix #columns)!  (" << procFeats.rows() << " != " << mToApply.cols() << ")";

    Matrix outFeats = mToApply*procFeats;
    pushToOutputs(SlotTransformedFeatures, FeaturesDecoderMessage::create(
                      featMsg->getTime(), featMsg->mUtteranceId,
                      outFeats, featMsg->mFeatureNames, featMsg->mFeatureTimestamps));

}

}
