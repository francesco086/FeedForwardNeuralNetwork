#include "NNTrainer.hpp"

double NNTrainer::computeResidual(FeedForwardNeuralNetwork * const ffnn, const bool &flag_r, const bool &flag_d)
{
    const int offset = _tdata.ntraining + _tdata.nvalidation;
    const int nbeta = ffnn->getNBeta();
    const double lambda_r_red = _tconfig.lambda_r / nbeta;

    double resi = 0.;

    // add regularization residual from NN betas
    for (int i=0; i<nbeta; ++i){
        resi += (_tconfig.flag_r && flag_r) ? lambda_r_red * pow(ffnn->getBeta(i), 2) : 0.;
    }

    //get difference NN vs data
    for (int i=offset; i<_tdata.ndata; ++i) {
        ffnn->setInput(_tdata.x[i]);
        ffnn->FFPropagate();
        for (int j=0; j<_tdata.yndim; ++j) {
            resi += pow(_tdata.w[i][j] * (ffnn->getOutput(j) - _tdata.y[i][j]), 2);

            for (int k=0; k<_tdata.xndim; ++k) {
                resi += (_tconfig.flag_d1 && flag_d) ? _tconfig.lambda_d1 * pow(_tdata.w[i][j] * (ffnn->getFirstDerivative(j, k) - _tdata.yd1[i][j][k]), 2) : 0.;
                resi += (_tconfig.flag_d2 && flag_d) ? _tconfig.lambda_d2 * pow(_tdata.w[i][j] * (ffnn->getSecondDerivative(j, k) - _tdata.yd2[i][j][k]), 2) : 0.;
            }
        }
    }
    return sqrt(0.5*resi);
}

void NNTrainer::bestFit(FeedForwardNeuralNetwork * const ffnn, double * bestfit, double * bestfit_err, const int &maxnsteps, const int &nfits, const double &resi_target, const int &verbose)
{
    int npar = ffnn->getNBeta();
    double fit[npar], err[npar];
    double resi_pure = -1.0, resi_noreg = -1.0, resi_full = -1.0, bestresi_pure = -1.0, bestresi_noreg = -1.0, bestresi_full = -1.0;

    int ifit = 0;
    while(true) {
        // initial parameters
        ffnn->randomizeBetas();
        for (int i = 0; i<npar; ++i) {
            fit[i] = ffnn->getBeta(i);
        }

        findFit(ffnn, fit, err, maxnsteps, verbose); // try new fit
        ffnn->setBeta(fit); // make sure ffnn is set to fit betas
        resi_full = computeResidual(ffnn, true, true);
        resi_noreg = computeResidual(ffnn, false, true);
        resi_pure = computeResidual(ffnn, false, false);

        // check for new best testing residual
        if(ifit < 1 || (resi_noreg < bestresi_noreg)) {
            for(int i = 0; i<npar; ++i){
                bestfit[i] = fit[i];
                bestfit_err[i] = err[i];
            }
            bestresi_full = resi_full;
            bestresi_noreg = resi_noreg;
            bestresi_pure = resi_pure;
        }

        ++ifit;

        // check break conditions
        if (bestresi_noreg <= resi_target) {
            if (verbose > 0) fprintf(stderr, "Unregularized fit residual %f (full: %f, pure: %f) meets tolerance %f. Exiting with good fit.\n\n", bestresi_noreg, bestresi_full, bestresi_pure, resi_target);
            break;
        } else {
            if (verbose > 0) fprintf(stderr, "Unregularized fit residual %f (full: %f, pure: %f) above tolerance %f.\n", resi_noreg, resi_full, resi_pure, resi_target);
            if (ifit >= nfits) {
                if (verbose > 0) fprintf(stderr, "Maximum number of fits reached (%i). Exiting with best unregularized fit residual %f.\n\n", nfits, bestresi_noreg);
                break;
            }
            if (verbose > 0) fprintf(stderr, "Let's try again.\n");
        }
    }

    if (verbose > 0) { // print summary
        fprintf(stderr, "best fit summary:\n");
        for(int i=0; i<npar; ++i) fprintf(stderr, "b%i      = %.5f +/- %.5f\n", i, bestfit[i], bestfit_err[i]);
        fprintf(stderr, "|f(x)| = %f (w/o reg: %f, pure: %f)\n", bestresi_full, bestresi_noreg, bestresi_pure);
    }

    // set ffnn to bestfit betas
    ffnn->setBeta(bestfit);
}

void NNTrainer::bestFit(FeedForwardNeuralNetwork * const ffnn, const int &maxnsteps, const int &nfits, const double &resi_target, const int &verbose)
{
    double bestfit[ffnn->getNBeta()], bestfit_err[ffnn->getNBeta()];
    bestFit(ffnn, bestfit, bestfit_err, maxnsteps, nfits, resi_target, verbose);
}

// print output of fitted NN to file
void NNTrainer::printFitOutput(FeedForwardNeuralNetwork * const ffnn, const double &min, const double &max, const int &npoints, const double &xscale, const double &yscale, const double &xshift, const double &yshift, const bool &print_d1, const bool &print_d2)
{
    using namespace std;
    double base_input = 0.;

    for (int i = 0; i<_tdata.xndim; ++i) {
        for (int j = 0; j<_tdata.yndim; ++j) {
            stringstream ss;
            ss << i << "_" << j << ".txt";
            writePlotFile(ffnn, &base_input, i, j, min, max, npoints, "getOutput", "v_" + ss.str(), xscale, yscale, xshift, yshift);
            if (print_d1) writePlotFile(ffnn, &base_input, i, j, min, max, npoints, "getFirstDerivative", "d1_" + ss.str(), xscale, yscale, xshift, yshift);
            if (print_d2) writePlotFile(ffnn, &base_input, i, j, min, max, npoints, "getSecondDerivative", "d2_" + ss.str(), xscale, yscale, xshift, yshift);
        }
    }
}