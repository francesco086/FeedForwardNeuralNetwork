#include "NNTrainerGSL.hpp"

// --- Helper functions

// set new NN betas
inline int setBetas(FeedForwardNeuralNetwork * const ffnn, const gsl_vector * const betas)
{
    const int nbeta = ffnn->getNBeta();
    for (int i=0; i<nbeta; ++i){
        ffnn->setBeta(i, gsl_vector_get(betas, i));
    }
    return nbeta;
}

// counts total residual vector size
// set nbeta or xndim to >0 to count regularization and derivative residual terms, respectively
// set nderiv = 1 if only one of both deriv residuals should be counted
inline int calcNData(const int &nbase, const int &yndim, const int &nbeta = 0, const int &xndim = 0, const int nderiv = 2)
{
    return nbase*yndim + nbeta + nderiv * nbase*xndim*yndim;
}

// calculate index offset pointing right behind the basic residual part
inline void calcOffset(const int &nbase, const int &yndim, int &off)
{
    off = nbase*yndim;
}

// also calculate offset behind first derivative part
inline void calcOffset(const int &nbase, const int &yndim, const int &xndim, int &offd1, int &offd2)
{
    calcOffset(nbase, yndim, offd1);
    offd2 = offd1 + nbase*xndim*yndim;
}

// also calculate offset behind second derivative part
inline void calcOffset(const int &nbase, const int &yndim, const int &xndim, int &offd1, int &offd2, int &offr)
{
    calcOffset(nbase, yndim, xndim, offd1, offd2);
    offr = offd2 + nbase*xndim*yndim;
}

// store (root) square sum of residual vector f in chisq (chi)
inline void calcRSS(const gsl_vector * const f, double &chisq, double &chi)
{
    gsl_blas_ddot(f, f, &chisq);
    chi = sqrt(chisq);
}


// --- Cost functions

// cost function without regularization and derivative terms
int ffnn_f_pure(const gsl_vector * betas, void * const tstruct, gsl_vector * f) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int n = ntrain + ((struct GSLFitStruct *)tstruct)->nvalidation; // we also fill the validation residuals here
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double * const * const x = ((struct GSLFitStruct *)tstruct)->x;
    const double * const * const y = ((struct GSLFitStruct *)tstruct)->y;
    const double * const * const w = ((struct GSLFitStruct *)tstruct)->w;
    FeedForwardNeuralNetwork * const ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;
    gsl_vector * const fvali = ((struct GSLFitStruct *)tstruct)->fvali_pure;

    double resi;

    setBetas(ffnn, betas);

    //get difference NN vs data
    for (int i=0; i<n; ++i) {
        ffnn->setInput(x[i]);
        ffnn->FFPropagate();
        for (int j=0; j<yndim; ++j) {
            resi = w[i][j] * (ffnn->getOutput(j) - y[i][j]);
            if (i<ntrain) gsl_vector_set(f, i*yndim + j, resi);
            else gsl_vector_set(fvali, (i-ntrain)*yndim + j, resi);
        }
    }

    return GSL_SUCCESS;
};

// gradient of cost function without regularization and derivative terms
int ffnn_df_pure(const gsl_vector * betas, void * const tstruct, gsl_matrix * J) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining; // gradients only for training set though
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double * const * const x = ((struct GSLFitStruct *)tstruct)->x;
    const double * const * const w = ((struct GSLFitStruct *)tstruct)->w;
    FeedForwardNeuralNetwork * const ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;

    const int nbeta = setBetas(ffnn, betas);

    //calculate cost gradient
    for (int ibeta=0; ibeta<nbeta; ++ibeta) {
        for (int i=0; i<ntrain; ++i) {
            ffnn->setInput(x[i]);
            ffnn->FFPropagate();
            for (int j=0; j<yndim; ++j) {
                gsl_matrix_set(J, i*yndim + j, ibeta, w[i][j] * ffnn->getVariationalFirstDerivative(j, ibeta));
            }
        }
    }

    return GSL_SUCCESS;
};

// cost function with derivative but without regularization
int ffnn_f_deriv(const gsl_vector * betas, void * const tstruct, gsl_vector * f) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int nvali = ((struct GSLFitStruct *)tstruct)->nvalidation;
    const int n = ntrain + nvali;
    const int xndim = ((struct GSLFitStruct *)tstruct)->xndim;
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double * const * const x = ((struct GSLFitStruct *)tstruct)->x;
    const double * const * const y = ((struct GSLFitStruct *)tstruct)->y;
    const double * const * const * const yd1 = ((struct GSLFitStruct *)tstruct)->yd1;
    const double * const * const * const yd2 = ((struct GSLFitStruct *)tstruct)->yd2;
    const double * const * const w = ((struct GSLFitStruct *)tstruct)->w;
    const double lambda_d1 = ((struct GSLFitStruct *)tstruct)->lambda_d1;
    const double lambda_d2 = ((struct GSLFitStruct *)tstruct)->lambda_d2;
    const bool flag_d1 = ((struct GSLFitStruct *)tstruct)->flag_d1;
    const bool flag_d2 = ((struct GSLFitStruct *)tstruct)->flag_d2;
    FeedForwardNeuralNetwork * const ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;
    gsl_vector * const fvali = ((struct GSLFitStruct *)tstruct)->fvali_noreg;

    gsl_vector * fnow;
    int nshift, nshift2, ishift, inshift, inshift2;
    const double lambda_d1_red = sqrt(lambda_d1), lambda_d2_red = sqrt(lambda_d2);

    setBetas(ffnn, betas);

    fnow = f;
    calcOffset(ntrain, yndim, xndim, nshift, nshift2);
    //get difference NN vs data
    for (int i=0; i<n; ++i) {
        ffnn->setInput(x[i]);
        ffnn->FFPropagate();

        if (i < ntrain) ishift = i*yndim;
        else {
            if (i == ntrain) {
                fnow = fvali; // switch working pointer
                calcOffset(ntrain, yndim, xndim, nshift, nshift2);
            }
            ishift = (i-ntrain)*yndim;
        }
        inshift = ishift + nshift;
        inshift2 = ishift + nshift2;

        for (int j=0; j<yndim; ++j) {
            gsl_vector_set(fnow, ishift + j,  w[i][j] * (ffnn->getOutput(j) - y[i][j]));
            for (int k=0; k<xndim; ++k) {
                gsl_vector_set(fnow, inshift + k*nshift + j, flag_d1 ? w[i][j] * lambda_d1_red * (ffnn->getFirstDerivative(j, k) - yd1[i][j][k]) : 0.0);
                gsl_vector_set(fnow, inshift2 + k*nshift + j, flag_d2 ? w[i][j] * lambda_d2_red * (ffnn->getSecondDerivative(j, k) - yd2[i][j][k]) : 0.0);
            }
        }
    }

    return GSL_SUCCESS;
};

// gradient of cost function with derivative but without regularization
int ffnn_df_deriv(const gsl_vector * betas, void * const tstruct, gsl_matrix * J) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int xndim = ((struct GSLFitStruct *)tstruct)->xndim;
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double * const * const x = ((struct GSLFitStruct *)tstruct)->x;
    const double * const * const w = ((struct GSLFitStruct *)tstruct)->w;
    const double lambda_d1 = ((struct GSLFitStruct *)tstruct)->lambda_d1;
    const double lambda_d2 = ((struct GSLFitStruct *)tstruct)->lambda_d2;
    const bool flag_d1 = ((struct GSLFitStruct *)tstruct)->flag_d1;
    const bool flag_d2 = ((struct GSLFitStruct *)tstruct)->flag_d2;
    FeedForwardNeuralNetwork * const ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;

    const double lambda_d1_red = sqrt(lambda_d1), lambda_d2_red = sqrt(lambda_d2);
    int nshift, nshift2, ishift, inshift, inshift2;

    const int nbeta = setBetas(ffnn, betas);
    calcOffset(ntrain, yndim, xndim, nshift, nshift2);

    //calculate cost gradient
    for (int ibeta=0; ibeta<nbeta; ++ibeta) {
        for (int i=0; i<ntrain; ++i) {
            ffnn->setInput(x[i]);
            ffnn->FFPropagate();

            ishift = i*yndim;
            inshift = ishift + nshift;
            inshift2 = ishift + nshift2;

            for (int j=0; j<yndim; ++j) {
                gsl_matrix_set(J, ishift + j, ibeta, w[i][j] * ffnn->getVariationalFirstDerivative(j, ibeta));
                for (int k=0; k<xndim; ++k) {
                    gsl_matrix_set(J, inshift + k*nshift + j, ibeta, flag_d1? w[i][j] * lambda_d1_red * ffnn->getCrossFirstDerivative(j, k, ibeta) : 0.0);
                    gsl_matrix_set(J, inshift2 + k*nshift + j, ibeta, flag_d2? w[i][j] * lambda_d2_red * ffnn->getCrossSecondDerivative(j, k, ibeta) : 0.0);
                }
            }
        }
    }

    return GSL_SUCCESS;
};

// cost function for fitting, without derivative but with regularization
int ffnn_f_pure_reg(const gsl_vector * betas, void * const tstruct, gsl_vector * f) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int nvali =  ((struct GSLFitStruct *)tstruct)->nvalidation;
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double lambda_r = ((struct GSLFitStruct *)tstruct)->lambda_r;
    FeedForwardNeuralNetwork * const ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;
    gsl_vector * const fvali = ((struct GSLFitStruct *)tstruct)->fvali_full;
    gsl_vector * const fvali_pure = ((struct GSLFitStruct *)tstruct)->fvali_pure;

    const int nbeta = ffnn->getNBeta(), n_reg = calcNData(ntrain, yndim, nbeta), nvali_reg = calcNData(nvali, yndim, nbeta);
    const double lambda_r_red = sqrt(lambda_r / nbeta);
    int nshift;

    ffnn_f_pure(betas, tstruct, f);

    //append regularization
    calcOffset(ntrain, yndim, nshift);
    for (int i=nshift; i<n_reg; ++i) {
        gsl_vector_set(f, i, lambda_r_red * gsl_vector_get(betas, i-nshift));
    }

    calcOffset(nvali, yndim, nshift);
    for (int i=0; i<nvali_reg; ++i) {
        if (i<nshift) gsl_vector_set(fvali, i, gsl_vector_get(fvali_pure, i));
        else gsl_vector_set(fvali, i, lambda_r_red * gsl_vector_get(betas, i-nshift));
    }

    return GSL_SUCCESS;
};

// gradient of cost function without derivatives but with regularization
int ffnn_df_pure_reg(const gsl_vector * betas, void * const tstruct, gsl_matrix * J) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double lambda_r = ((struct GSLFitStruct *)tstruct)->lambda_r;
    FeedForwardNeuralNetwork * ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;

    const int nbeta = ffnn->getNBeta(), n_reg = calcNData(ntrain, yndim, nbeta);
    const double lambda_r_red = sqrt(lambda_r / nbeta);
    int nshift;

    ffnn_df_pure(betas, tstruct, J);

    //append regularization gradient
    calcOffset(ntrain, yndim, nshift);
    for (int i=nshift; i<n_reg; ++i) {
        for (int j=0; j<nbeta; ++j) {
            gsl_matrix_set(J, i, j, 0.0);
        }
        gsl_matrix_set(J, i, i-nshift, lambda_r_red);
    }

    return GSL_SUCCESS;
};

// cost function for fitting, with derivative and regularization
int ffnn_f_deriv_reg(const gsl_vector * betas, void * const tstruct, gsl_vector * f) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int nvali = ((struct GSLFitStruct *)tstruct)->nvalidation;
    const int xndim = ((struct GSLFitStruct *)tstruct)->xndim;
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double lambda_r = ((struct GSLFitStruct *)tstruct)->lambda_r;
    FeedForwardNeuralNetwork * const ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;
    gsl_vector * const fvali = ((struct GSLFitStruct *)tstruct)->fvali_full;
    gsl_vector * const fvali_noreg = ((struct GSLFitStruct *)tstruct)->fvali_noreg;

    const int nshift = calcNData(ntrain, yndim, 0, xndim), nshift_vali = calcNData(nvali, yndim, 0, xndim);
    const int nbeta = ffnn->getNBeta(), n_reg = nshift + nbeta, nvali_reg = nshift_vali + nbeta;
    const double lambda_r_red = sqrt(lambda_r / nbeta);

    ffnn_f_deriv(betas, tstruct, f);

    //append regularization
    for (int i=nshift; i<n_reg; ++i) {
        gsl_vector_set(f, i, lambda_r_red * gsl_vector_get(betas, i-nshift));
    }

    for (int i=0; i<nvali_reg; ++i) {
        if (i<nshift_vali) gsl_vector_set(fvali, i, gsl_vector_get(fvali_noreg, i));
        else gsl_vector_set(fvali, i, lambda_r_red * gsl_vector_get(betas, i-nshift_vali));
    }

    return GSL_SUCCESS;
};

// gradient of cost function with derivatives and regularization
int ffnn_df_deriv_reg(const gsl_vector * betas, void * const tstruct, gsl_matrix * J) {
    const int ntrain = ((struct GSLFitStruct *)tstruct)->ntraining;
    const int xndim = ((struct GSLFitStruct *)tstruct)->xndim;
    const int yndim = ((struct GSLFitStruct *)tstruct)->yndim;
    const double lambda_r = ((struct GSLFitStruct *)tstruct)->lambda_r;
    FeedForwardNeuralNetwork * ffnn = ((struct GSLFitStruct *)tstruct)->ffnn;

    const int nbeta = ffnn->getNBeta(), nshift = calcNData(ntrain, yndim, 0, xndim), n_reg = nshift + nbeta;
    const double lambda_r_red = sqrt(lambda_r / nbeta);

    ffnn_df_deriv(betas, tstruct, J);

    //append regularization gradient
    for (int i=nshift; i<n_reg; ++i) {
        for (int j=0; j<nbeta; ++j) {
            gsl_matrix_set(J, i, j, 0.0);
        }
        gsl_matrix_set(J, i, i-nshift, lambda_r_red);
    }

    return GSL_SUCCESS;
};

// gets called once for every fit iteration
void callback(const size_t iter, void *params, const gsl_multifit_nlinear_workspace *w) {
    gsl_vector *f = gsl_multifit_nlinear_residual(w);
    gsl_vector *x = gsl_multifit_nlinear_position(w);
    double rcond = 0.0;

    // compute reciprocal condition number of J(x)
    gsl_multifit_nlinear_rcond(&rcond, w);

    fprintf(stderr, "iter %zu: cond(J) = %8.4f, |f(x)| = %.4f\n", iter, 1.0 / rcond, gsl_blas_dnrm2(f));

    for (size_t i=0; i<x->size; ++i) fprintf(stderr, "b%zu: %f, ", i,  gsl_vector_get(x, i));
    fprintf(stderr, "\n");
};


void NNTrainerGSL::findFit(double * const fit, double * const err, const int &nsteps, const int &verbose) {

    //   Fit NN with the following passed variables:
    //   fit: holds the to be fitted variables, i.e. betas
    //   err: holds the corresponding fit error
    //   resi_full: holds the full residual value, including all terms
    //   resi_noreg: holds the residual value without regularization
    //   resi_pure: holds the function-only residual value
    //
    //   and with the following parameters:
    //   nsteps : number of fitting iterations
    //   verbose: print verbose output while fitting


    int npar = _ffnn->getNBeta(), ntrain = _tstruct.ntraining, nvali = _tstruct.nvalidation;
    const gsl_multifit_nlinear_type *T_full = gsl_multifit_nlinear_trust, *T_noreg = gsl_multifit_nlinear_trust, *T_pure = gsl_multifit_nlinear_trust;
    gsl_multifit_nlinear_fdf fdf_full, fdf_noreg, fdf_pure;
    gsl_multifit_nlinear_workspace * w_full, * w_noreg, * w_pure;
    gsl_multifit_nlinear_parameters fdf_params = gsl_multifit_nlinear_default_parameters();

    gsl_vector *f;
    gsl_matrix *J;
    gsl_matrix * covar = gsl_matrix_alloc (npar, npar);

    gsl_vector_view gx = gsl_vector_view_array (fit, npar);
    double chisq, chi0, chi0_vali;
    double resi_full, resi_noreg, resi_pure;
    double resi_vali_full, resi_vali_noreg, resi_vali_pure;
    double resih, c;
    const int dof = ntrain - npar;
    int ntrain_noreg, nvali_noreg, ntrain_full, nvali_full;
    int status, info;

    const double xtol = 0.0;
    const double gtol = 0.0;
    const double ftol = 0.0;

    const bool flag_d = _tstruct.flag_d1 || _tstruct.flag_d2;

    // configure all three fdf objects

    // first the pure fdf
    fdf_pure.f = ffnn_f_pure;
    fdf_pure.df = ffnn_df_pure;
    fdf_pure.fvv = NULL;
    fdf_pure.n = calcNData(ntrain, _tstruct.yndim);
    fdf_pure.p = npar;
    fdf_pure.params = &_tstruct;

    if (flag_d) {
        ntrain_noreg = calcNData(ntrain, _tstruct.yndim, 0, _tstruct.xndim);
        nvali_noreg = calcNData(nvali, _tstruct.yndim, 0, _tstruct.xndim);

        // deriv fdf without regularization
        fdf_noreg.f = ffnn_f_deriv;
        fdf_noreg.df = ffnn_df_deriv;
        fdf_noreg.fvv = NULL;
        fdf_noreg.n = ntrain_noreg;
        fdf_noreg.p = npar;
        fdf_noreg.params = &_tstruct;
    }
    else {
        ntrain_noreg = calcNData(ntrain, _tstruct.yndim);
        nvali_noreg = calcNData(nvali, _tstruct.yndim);
        fdf_noreg = fdf_pure;
    };

    if (_tstruct.flag_r) {
        ntrain_full = ntrain_noreg + npar;
        nvali_full = nvali_noreg + npar;

        if (flag_d) {
            // deriv with regularization
            fdf_full.f = ffnn_f_deriv_reg;
            fdf_full.df = ffnn_df_deriv_reg;
        }
        else {
            // pure fdf with regularization
            fdf_full.f = ffnn_f_pure_reg;
            fdf_full.df = ffnn_df_pure_reg;
        }
        fdf_full.fvv = NULL;
        fdf_full.n = ntrain_full;
        fdf_full.p = npar;
        fdf_full.params = &_tstruct;
    }
    else {
        ntrain_full = ntrain_noreg;
        nvali_full = nvali_noreg;
        fdf_full = fdf_noreg;
    };

    // allocate workspace with default parameters, also allocate space for validation
    w_full = gsl_multifit_nlinear_alloc (T_full, &fdf_params, ntrain_full, npar);
    w_noreg = gsl_multifit_nlinear_alloc (T_noreg, &fdf_params, ntrain_noreg, npar);
    w_pure = gsl_multifit_nlinear_alloc (T_pure, &fdf_params, ntrain, npar);
    _tstruct.fvali_pure = gsl_vector_alloc(nvali);
    _tstruct.fvali_noreg = flag_d ? gsl_vector_alloc(nvali_noreg) : _tstruct.fvali_pure;
    _tstruct.fvali_full = _tstruct.flag_r ? gsl_vector_alloc(nvali_full) : _tstruct.fvali_noreg;

    // initialize solver with starting point
    gsl_multifit_nlinear_init(&gx.vector, &fdf_full, w_full);

    // compute initial cost function
    f = gsl_multifit_nlinear_residual(w_full);
    gsl_blas_ddot(f, f, &resih);
    chi0 = sqrt(resih);
    gsl_blas_ddot(_tstruct.fvali_full, _tstruct.fvali_full, &resih);
    chi0_vali = sqrt(resih);

    // solve the system with a maximum of nsteps iterations
    if (verbose > 1) status = gsl_multifit_nlinear_driver(nsteps, xtol, gtol, ftol, callback, NULL, &info, w_full);
    else status = gsl_multifit_nlinear_driver(nsteps, xtol, gtol, ftol, NULL, NULL, &info, w_full);

    // compute covariance of best fit parameters
    J = gsl_multifit_nlinear_jac(w_full);
    f = gsl_multifit_nlinear_residual(w_full);
    gsl_multifit_nlinear_covar(J, 0.0, covar);

    // compute final cost
    gsl_blas_ddot(f, f, &chisq);
    resi_full = sqrt(chisq);
    c = GSL_MAX_DBL(1, sqrt(resih / dof));
    gsl_blas_ddot(_tstruct.fvali_full, _tstruct.fvali_full, &resih);
    resi_vali_full = sqrt(resih);

    // unregularized cost calculation
    for (int i = 0; i<npar; ++i) {
        fit[i] = gsl_vector_get(w_full->x, i);
        err[i] = c*sqrt(gsl_matrix_get(covar,i,i));
    }
    gsl_multifit_nlinear_init(&gx.vector, &fdf_noreg, w_noreg);
    f = gsl_multifit_nlinear_residual(w_noreg);
    gsl_blas_ddot(f, f, &resih);
    resi_noreg = sqrt(resih);
    gsl_blas_ddot(_tstruct.fvali_noreg, _tstruct.fvali_noreg, &resih);
    resi_vali_noreg = sqrt(resih);

    // pure (no deriv, no reg) cost calculation
    gsl_multifit_nlinear_init(&gx.vector, &fdf_pure, w_pure);
    f = gsl_multifit_nlinear_residual(w_pure);
    gsl_blas_ddot(f, f, &resih);
    resi_pure = sqrt(resih);
    gsl_blas_ddot(_tstruct.fvali_pure, _tstruct.fvali_pure, &resih);
    resi_vali_pure = sqrt(resih);

    if (verbose > 1) {
        fprintf(stderr, "summary from method '%s/%s'\n", gsl_multifit_nlinear_name(w_full), gsl_multifit_nlinear_trs_name(w_full));
        fprintf(stderr, "number of iterations: %zu\n", gsl_multifit_nlinear_niter(w_full));
        fprintf(stderr, "function evaluations: %zu\n", fdf_full.nevalf);
        fprintf(stderr, "Jacobian evaluations: %zu\n", fdf_full.nevaldf);
        fprintf(stderr, "reason for stopping: %s\n", (info == 1) ? "small step size" : "small gradient");
        fprintf(stderr, "initial |f(x)| = %f (train), %f (vali)\n", chi0, chi0_vali);
        fprintf(stderr, "final   |f(x)| = %f (train), %f (vali)\n", resi_full, resi_vali_full);
        fprintf(stderr, "w/o reg |f(x)| = %f (train), %f (vali)\n", resi_noreg, resi_vali_noreg);
        fprintf(stderr, "pure    |f(x)| = %f (train), %f (vali)\n", resi_pure, resi_vali_pure);
        fprintf(stderr, "chisq/dof = %g\n", chisq / dof);

        for(int i=0; i<npar; ++i) fprintf(stderr, "b%i      = %.5f +/- %.5f\n", i, fit[i], err[i]);

        fprintf(stderr, "status = %s\n", gsl_strerror (status));
    }

    gsl_multifit_nlinear_free(w_full);
    gsl_multifit_nlinear_free(w_noreg);
    gsl_multifit_nlinear_free(w_pure);
    gsl_vector_free(_tstruct.fvali_pure);
    if (flag_d) gsl_vector_free(_tstruct.fvali_noreg);
    if (_tstruct.flag_r) gsl_vector_free(_tstruct.fvali_full);
    gsl_matrix_free(covar);
};
