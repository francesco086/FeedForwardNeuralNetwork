#include <iostream>
#include <cmath>
#include <assert.h>

#include "NNTrainerGSL.hpp"
#include "../../../src/trainer/NNTrainerGSL.cpp" // to access "local" functions

double gaussian(const double x, const double a = 1., const double b = 0.) {
    return exp(-a*pow(x-b, 2));
};

// first derivative of gaussian
double gaussian_ddx(const double x, const double a = 1., const double b = 0.) {
    return 2.0*a*(b-x) * exp(-a*pow(x-b, 2));
};

// first derivative of gaussian
double gaussian_d2dx(const double x, const double a = 1., const double b = 0.) {
    return (pow(2.0*a*(b-x), 2) - 2.0*a) * exp(-a*pow(x-b, 2));
};

void validate_beta(FeedForwardNeuralNetwork * const ffnn, const double * const beta, const double &TINY)
{
    assert((abs(ffnn->getBeta(0) - beta[0]) < TINY && abs(ffnn->getBeta(1) - beta[1]) < TINY && abs(ffnn->getBeta(2) - beta[2]) < TINY) || (abs(ffnn->getBeta(0) + beta[0]) < TINY && abs(ffnn->getBeta(1) + beta[1]) < TINY && abs(ffnn->getBeta(2) + beta[2]) < TINY)); // symmetric gaussian allows both combinations
    for (int i=3; i<ffnn->getNBeta(); ++i) assert(abs(ffnn->getBeta(i) - beta[i]) < TINY);
}


int main (void) {
    using namespace std;

    const int verbose = 0;
    const double TINY = 0.000001;

    const int ndim = 2;
    const int xndim = ndim;
    const int nhid = 2;
    const int yndim = ndim;

    // create FFNN
    FeedForwardNeuralNetwork * ffnn = new FeedForwardNeuralNetwork(xndim+1, nhid, yndim+1);
    ffnn->connectFFNN();
    ffnn->addVariationalFirstDerivativeSubstrate();
    ffnn->addFirstDerivativeSubstrate();
    ffnn->addSecondDerivativeSubstrate();

    // set gaussian activation function on the single hidden neuron, id activation on output
    ffnn->getNNLayer(0)->getNNUnit(0)->setActivationFunction(std_actf::provideActivationFunction("GSS"));
    ffnn->getNNLayer(1)->getNNUnit(0)->setActivationFunction(std_actf::provideActivationFunction("ID"));
    ffnn->getNNLayer(1)->getNNUnit(1)->setActivationFunction(std_actf::provideActivationFunction("ID"));

    // the variational parameters of our model (ffnn beta should be equal to beta array after fitting)
    const double nbeta = (nhid-1) * (xndim+1) + yndim * nhid;
    const double beta[7] = {0.2, -1.1, 0.9, 1., -1., 0., 0.5};

    // check basic things
    assert(ffnn->getLayer(0)->getNUnits() == xndim+1);
    assert(ffnn->getLayer(1)->getNUnits() == nhid);
    assert(ffnn->getLayer(2)->getNUnits() == yndim+1);
    assert(ffnn->getNBeta() == nbeta);

    // allocate data arrays
    const int ntraining = 20;
    const int nvalidation = 20;
    const int ntesting = 20;
    const int ndata = ntraining + nvalidation + ntesting;
    double ** xdata = new double*[ndata];
    double ** ydata = new double*[ndata];
    double *** d1data = new double**[ndata];
    double *** d2data = new double**[ndata];
    double ** weights = new double*[ndata];
    for (int i = 0; i<ndata; ++i) {
        xdata[i] = new double[xndim];
        ydata[i] = new double[yndim];
        weights[i] = new double[yndim];
        d1data[i] = new double*[yndim];
        d2data[i] = new double*[yndim];
        for (int j = 0; j<yndim; ++j) {
            d1data[i][j] = new double[xndim];
            d2data[i][j] = new double[xndim];
        }
    }

    // generate the data to be fitted (the output of a "gaussian" NN, same shape as ffnn)
    const double lb = -2;
    const double ub = 2;
    random_device rdev;
    mt19937_64 rgen = std::mt19937_64(rdev());
    uniform_real_distribution<double> rd(lb,ub);
    for (int i=0; i<ndata; ++i) {
        for (int j=0; j<xndim; ++j) {
            xdata[i][j] = rd(rgen);
        }
        const double feed = beta[0] + beta[1] * xdata[i][0] + beta[2] * xdata[i][1];
        const double hnv = gaussian(feed);
        ydata[i][0] = beta[3] + beta[4] * hnv;
        ydata[i][1] = beta[5] + beta[6] * hnv;

        const double d1feed = gaussian_ddx(feed);
        const double d2feed = gaussian_d2dx(feed);

        d1data[i][0][0] = (beta[4] * d1feed * beta[1]);
        d1data[i][0][1] = (beta[4] * d1feed * beta[2]);
        d1data[i][1][0] = (beta[6] * d1feed * beta[1]);
        d1data[i][1][1] = (beta[6] * d1feed * beta[2]);

        d2data[i][0][0] = (beta[4] * d2feed * pow(beta[1], 2));
        d2data[i][0][1] = (beta[4] * d2feed * pow(beta[2], 2));
        d2data[i][1][0] = (beta[6] * d2feed * pow(beta[1], 2));
        d2data[i][1][1] = (beta[6] * d2feed * pow(beta[2], 2));

        weights[i][0] = 1.0;
        weights[i][1] = 1.0;
    };

    // create data/config structs
    const int maxn_steps = 50;
    const int maxn_novali = 5;
    const int maxn_fits = 50;
    const double lambda_r = 0.000000001, lambda_d1 = 0.01, lambda_d2 = 0.01;
    NNTrainingData tdata = {ndata, ntraining, nvalidation, xndim, yndim, xdata, ydata, d1data, d2data, weights};
    NNTrainingConfig tconfig = {false, false, false, lambda_r, lambda_d1, lambda_d2, maxn_steps, maxn_novali};
    NNTrainerGSL * trainer;


    // find fit without derivs or regularization
    trainer = new NNTrainerGSL(tdata, tconfig); // NOTE: we do not normalize, to keep known beta targets
    trainer->bestFit(ffnn, maxn_fits, TINY, verbose); // fit until residual<TINY or maxn_fits reached
    assert(trainer->computeResidual(ffnn, false, false) <= TINY);
    validate_beta(ffnn, beta, TINY);
    delete trainer;


    // find fit without derivs but with regularization
    tconfig.flag_r = true;
    trainer = new NNTrainerGSL(tdata, tconfig);
    trainer->bestFit(ffnn, maxn_fits, TINY, verbose);
    assert(trainer->computeResidual(ffnn, false, false) <= TINY); // we check the unregularized residual against TINY
    validate_beta(ffnn, beta, TINY);
    delete trainer;


    // find fit with derivs but without regularization
    ffnn->addCrossFirstDerivativeSubstrate();
    ffnn->addCrossSecondDerivativeSubstrate();
    tconfig.flag_r = false;
    tconfig.flag_d1 = true;
    tconfig.flag_d2 = true;
    trainer = new NNTrainerGSL(tdata, tconfig);
    trainer->bestFit(ffnn, maxn_fits, TINY, verbose);
    assert(trainer->computeResidual(ffnn, false, true) <= TINY);
    validate_beta(ffnn, beta, TINY);
    delete trainer;


    // find fit with derivs and regularization
    tconfig.flag_r = true;
    trainer = new NNTrainerGSL(tdata, tconfig);
    trainer->bestFit(ffnn, maxn_fits, TINY, verbose);
    assert(trainer->computeResidual(ffnn, false, true) <= TINY);
    validate_beta(ffnn, beta, TINY);
    delete trainer;


    //ffnn->storeOnFile("nn.txt");

    // Delete allocations
    delete ffnn;

    for (int i = 0; i<ndata; ++i) {
        delete [] xdata[i];
        delete [] ydata[i];
        delete [] weights[i];
        for (int j = 0; j<yndim; ++j) {
            delete [] d1data[i][j];
            delete [] d2data[i][j];
        }
        delete [] d1data[i];
        delete [] d2data[i];
    }
    delete [] xdata;
    delete [] ydata;
    delete [] weights;
    delete [] d1data;
    delete [] d2data;

    return 0;
    //
}