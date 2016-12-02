
/*
 *    This file is part of ACADOS.
 *
 *    ACADOS is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    ACADOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with ACADOS; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#if defined(__APPLE__)
#include <mach/mach_time.h>
#else
#include <sys/stat.h>
#endif
#include "acados/sim/sim_erk_integrator.h"
#include "examples/Chen/Chen_model.h"

// system headers
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

// HPMPC headers
#include "hpmpc/include/aux_d.h"

// ACADOS headers
#include "acados/utils/types.h"
#include "acados/ocp_qp/ocp_qp_common.h"
#include "acados/ocp_qp/ocp_qp_hpmpc.h"
#include "acados/utils/tools.h"

// flush denormals to zero
#if defined(TARGET_X64_AVX2) || defined(TARGET_X64_AVX) ||  \
    defined(TARGET_X64_SSE3) || defined(TARGET_X86_ATOM) || \
    defined(TARGET_AMD_SSE3)
#include <xmmintrin.h>  // needed to flush to zero sub-normals with _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON); in the main()
#endif

// define IP solver arguments && number of repetitions
#define NREP 1000
#define MAXITER 10
#define TOL 1e-8
#define MINSTEP 1e-8

#define NN 13
#define NX 2
#define NU 1
#define NBX 0
#define NBU 1

#include "acados/utils/timing.h"

#ifdef DEBUG
static void print_states_controls(real_t **w, int_t N) {
    printf("node\tx\t\t\t\tu\n");
    for (int_t i = 0; i < N; i++) {
        printf("%4d\t%+e %+e\t%+e\n", i, w[i][0], w[i][1], w[i][2]);
    }
    printf("%4d\t%+e %+e\n", N, w[N][0], w[N][1]);
}
#endif  // DEBUG

// Simple SQP example for acados
int main() {

    // Problem data
    int_t   N                         = NN;
    real_t  x0[NX]                    = {0.5, 0};
    real_t  Q[NX*NX]                  = {0};
    real_t  R[NU*NU]                  = {0};
    real_t  xref[NX]                  = {0};
    real_t  uref[NX]                  = {0};
    int_t   max_ip_iters              = 10;
    int_t   timing_iters              = 1;
    real_t  x_end[NX]                 = {0};
    real_t  u_end[NU]                 = {0};

    real_t  *w[NN+1]; //[NN*(NX+NU)+NX]          = {0};  // nlp states and controls stacked
    real_t  *pi_n[NN+1];  // nlp eq. mult
    real_t  *lam_n[NN+1];//NN*2*(NBX+NBU)+NBU] = {0};  // nlp ineq. mult

    for( int_t i = 0; i < NN; i++)
    {
      d_zeros(&w[i], NX+NU, 1);
      d_zeros(&pi_n[i], NX, 1);
      d_zeros(&lam_n[i], 2*(NBX + NBU), 1);
    }

    d_zeros(&w[NN], NX, 1);
    d_zeros(&pi_n[NN], NX, 1);
    d_zeros(&lam_n[NN], NBX, 1);

    for (int_t i = 0; i < NX; i++) w[0][i] = x0[i];
    for (int_t i = 0; i < NX; i++) Q[i*(NX+1)] = 1.0;
    for (int_t i = 0; i < NU; i++) R[i*(NU+1)] = 0.05;

    // Integrator structs
    real_t T = 0.1;
    sim_in  sim_in;
    sim_out sim_out;
    sim_in.nSteps = 100;
    sim_in.step = T/sim_in.nSteps;
    sim_in.VDE_forw = &VDE_fun;
    sim_in.nx = NX;
    sim_in.nu = NU;

    sim_in.sens_forw = true;
    sim_in.sens_adj = false;
    sim_in.sens_hess = false;
    sim_in.nsens_forw = NX+NU;

    sim_in.x = malloc(sizeof(*sim_in.x) * (NX));
    sim_in.u = malloc(sizeof(*sim_in.u) * (NU));
    sim_in.S_forw = malloc(sizeof(*sim_in.S_forw) * (NX*(NX+NU)));
    for (int_t i = 0; i < NX*(NX+NU); i++) sim_in.S_forw[i] = 0.0;
    for (int_t i = 0; i < NX; i++) sim_in.S_forw[i*(NX+1)] = 1.0;

    sim_out.xn = malloc(sizeof(*sim_out.xn) * (NX));
    sim_out.S_forw = malloc(sizeof(*sim_out.S_forw) * (NX*(NX+NU)));

    sim_info erk_info;
    sim_out.info = &erk_info;

    sim_erk_workspace erk_work;
    sim_RK_opts rk_opts;
    sim_erk_create_opts(4, &rk_opts);
    sim_erk_create_workspace(&sim_in, &rk_opts, &erk_work);

    int_t nx[NN+1] = {0};
    int_t nu[NN] = {0};
    int_t nb[NN+1] = {0};
    int_t nc[NN+1] = {0};
    for (int_t i = 0; i < N; i++) {
        nx[i] = NX;
        nu[i] = NU;
    }
    nx[N] = NX;

    real_t *pA[N];
    real_t *pB[N];
    real_t *pb[N];
    real_t *pQ[N+1];
    real_t *pS[N];
    real_t *pR[N];
    real_t *pq[N+1];
    real_t *pr[N];
    real_t *px[N+1];
    real_t *pu[N];
    real_t *px0[1];
    int    *hidxb[N+1];
    real_t *pC[N + 1];
    real_t *pD[N];
    real_t *plg[N + 1];
    real_t *pug[N + 1];

    /************************************************
    * box constraints
    ************************************************/
    int ii, jj;

    nb[0] = NBU;
    for (ii = 1; ii < N; ii++) nb[ii] = NBX;
    nb[N] = NBX;

    int *idxb0;
    i_zeros(&idxb0, nb[0], 1);
    idxb0[0] = 0;
    idxb0[1] = 1;


    int *idxb1;
    i_zeros(&idxb1, nb[1], 1);

    int *idxbN;
    i_zeros(&idxbN, nb[N], 1);

    for (ii = 1; ii < N; ii++) {
        hidxb[ii] = idxb1;
    }

    hidxb[N] = idxbN;

    d_zeros(&px0[0], nx[0], 1);
    for (int_t i = 0; i < N; i++) {
        d_zeros(&pA[i], nx[i+1], nx[i]);
        d_zeros(&pB[i], nx[i+1], nu[i]);
        d_zeros(&pb[i], nx[i+1], 1);
        d_zeros(&pS[i], nu[i], nx[i]);
        d_zeros(&pq[i], nx[i], 1);
        d_zeros(&pr[i], nu[i], 1);
        d_zeros(&px[i], nx[i], 1);
        d_zeros(&pu[i], nu[i], 1);
    }
    d_zeros(&pq[N], nx[N], 1);
    d_zeros(&px[N], nx[N], 1);
    // hidxb[N] = idxbN;

    nx[0] = 0;
    //    d_print_mat(nx, 1, b, nx);
    //    d_print_mat(nx, 1, b0, nx);

    // then A0 is a matrix of size 0x0
    double *A0;
    d_zeros(&A0, 0, 0);

    int ng = 0;  // 4;  // number of general constraints
    int ngN = 0;  // 4;  // number of general constraints at the last stage

    int ngg[N + 1];
    for (ii = 0; ii < N; ii++) ngg[ii] = ng;
    ngg[N] = ngN;

    /************************************************
    * general constraints
    ************************************************/

    double *C0;
    d_zeros(&C0, 0, NX);
    double *D0;
    d_zeros(&D0, 0, NU);
    double *lg0;
    d_zeros(&lg0, 0, 1);
    double *ug0;
    d_zeros(&ug0, 0, 1);

    double *C;
    d_zeros(&C, 0, NX);
    double *D;
    d_zeros(&D, 0, NU);
    double *lg;
    d_zeros(&lg, 0, 1);
    double *ug;
    d_zeros(&ug, 0, 1);

    double *CN;
    d_zeros(&CN, ngN, NX);
    double *lgN;
    d_zeros(&lgN, ngN, 1);  // force all states to 0 at the last stage
    double *ugN;
    d_zeros(&ugN, ngN, 1);  // force all states to 0 at the last stage

    pC[0] = C0;
    pD[0] = D0;
    plg[0] = lg0;
    pug[0] = ug0;
    real_t *ppi[N];
    real_t *plam[N+1];

    ii = 0;
    d_zeros(&ppi[ii], nx[ii+1], 1);
    d_zeros(&plam[ii], 2*nb[ii], 1);
    for (ii = 1; ii < N; ii++) {
        pC[ii] = C;
        pD[ii] = D;
        plg[ii] = lg;
        pug[ii] = ug;
        d_zeros(&ppi[ii], nx[ii+1], 1);
        d_zeros(&plam[ii], 2*nb[ii], 1);
    }
    d_zeros(&plam[N], 2*nb[N], 1);
    pC[N] = CN;
    plg[N] = lgN;
    pug[N] = ugN;

    /************************************************
    * solver arguments
    ************************************************/

    // solver arguments
    ocp_qp_hpmpc_args hpmpc_args;
    hpmpc_args.tol = TOL;
    hpmpc_args.max_iter = 10;
//  hpmpc_args.min_step = MINSTEP;
    hpmpc_args.mu0 = 0.0;
//  hpmpc_args.sigma_min = 1e-3;
    hpmpc_args.warm_start = 0;
    hpmpc_args.N2 = N;

    /************************************************
    * work space
    ************************************************/

    // int work_space_size = ocp_qp_hpmpc_workspace_size_bytes(N, nx, nu, nb, ngg, hidxb, &hpmpc_args);
    // printf("\nwork space size: %d bytes\n", work_space_size);

    // void *workspace = malloc(work_space_size);
    void *workspace = malloc(500000);

    // double workspace[500000];
    // Allocate OCP QP variables
    ocp_qp_in qp_in;
    qp_in.N = N;
    ocp_qp_out qp_out;
    qp_in.nx = nx;
    qp_in.nu = nu;
    qp_in.nb = nb;
    qp_in.nc = nc;
    for (int_t i = 0; i < N; i++) {
        pQ[i] = Q;
        pR[i] = R;
    }
    pQ[N] = Q;
    qp_in.Q = (const real_t **) pQ;
    qp_in.S = (const real_t **) pS;
    qp_in.R = (const real_t **) pR;
    qp_in.q = (const real_t **) pq;
    qp_in.r = (const real_t **) pr;
    qp_in.A = (const real_t **) pA;
    qp_in.B = (const real_t **) pB;
    qp_in.b = (const real_t **) pb;
    // qp_in.lb = (const real_t **) px0;
    // qp_in.ub = (const real_t **) px0;
    // qp_in.idxb = (const int_t **) hidxb;
    qp_in.Cx = (const real_t **) pC;
    qp_in.Cu = (const real_t **) pD;
    qp_in.lc = (const real_t **) plg;
    qp_in.uc = (const real_t **) pug;

    qp_out.x = px;
    qp_out.u = pu;
    qp_out.pi = ppi;
    qp_out.lam = plam;

    acado_timer timer;
    real_t timings = 0;

    // Define residuals
    real_t *res_stat[N+1];
    real_t *res_eq[N]; // TODO: what if we have terminal equality constraints for example?
    real_t *res_ineq[N+1];
    real_t *res_compl[N+1];

    // Allocate memory for the residuals
    for(ii = 0; ii < N; ii ++)
    {
      d_zeros(&res_stat[ii], nx[ii]+nu[ii], 1);
      d_zeros(&res_eq[ii],   nx[ii+1], 1);
      d_zeros(&res_ineq[ii], 2*nb[ii], 1); //TODO: what happens to the general ineqs constraints in hpmpc?
      d_zeros(&res_compl[ii],2*nb[ii], 1); //TODO: what happens to the general ineqs constraints in hpmpc?
    }
    ii = N;
    d_zeros(&res_stat[ii], nx[ii]+nu[ii], 1);
    d_zeros(&res_ineq[ii], 2*nb[ii], 1); //TODO: what happens to the general ineqs constraints in hpmpc?
    d_zeros(&res_compl[ii],2*nb[ii], 1); //TODO: what happens to the general ineqs constraints in hpmpc?

    for (int_t iter = 0; iter < timing_iters; iter++) {
        for (int_t i = 0; i < NX; i++) w[0][i] = x0[i];
        acado_tic(&timer);
        for (int_t ip_iter = 0; ip_iter < max_ip_iters; ip_iter++) {
            printf("\n------ ITERATION %d ------\n", ip_iter);
            for (int_t i = 0; i < N; i++) {

                // Pass state and control to integrator
                for (int_t j = 0; j < NX; j++) sim_in.x[j] = w[i][j];
                for (int_t j = 0; j < NU; j++) sim_in.u[j] = w[i][j+NX];
                sim_erk(&sim_in, &sim_out, &rk_opts, &erk_work);

                // Construct QP matrices
                for (int_t j = 0; j < NX; j++) {
                    pq[i][j] = Q[j*(NX+1)]*(w[i][j]-xref[j]);
                }
                for (int_t j = 0; j < NU; j++) {
                    pr[i][j] = R[j*(NX+1)]*(w[i][j+NX]-uref[j]);
                }
                for (int_t j = 0; j < NX; j++) {
                    pb[i][j] = sim_out.xn[j] - w[i+1][j];
                    for (int_t k = 0; k < NX; k++) pA[i][j*NX+k] = sim_out.S_forw[j*(NX)+k];
                }
                for (int_t j = 0; j < NU; j++)
                    for (int_t k = 0; k < NX; k++) pB[i][j*NU+k] = sim_out.S_forw[NX*NX + NU*j+k];
            }

            for (int_t k = 0; k < nx[0]; k++) pb[0][k] = 0.0; // Andrea: can we keep x0 in hpmpc? Eliminating will not work if we do line-search
            for (int_t j = 0; j < NX; j++)
            // real_t delta_x0[nx[0]];
            // for (int_t k = 0; k < nx[0]; k++) delta_x0[k] = x0[k] - w[k];
            // dgemv_n_3l(NX, NX, pA[0], NX, x0, pb[0]);

            for (int_t j = 0; j < NX; j++) {
                pq[N][j] = Q[j*(NX+1)]*(w[N][j]-xref[j]);
            }

            // Barrier strategy
            // Compute residuals
            int_t i;

            i = 0; //on stage 0 we have no states

            // Stationarity residuals
            for (int_t j = 0; j < NU; j++) res_stat[0][j] = 0.0;
            dgemv_n_3l(NU, NU, pR[0], NU, &w[0][0+NX], res_stat[0]);
            dgemv_t_3l(NX, NU, pB[0], NU, pi_n[0], res_stat[0]);
            // printf("Stationarity residuals = %f\n",twonormv(NU,&res_stat[i][0]));
            double temp_stat[NX];

            for ( i = 1; i < N-1; i++ )
            {
                // Stationarity residuals
                for (int_t j = 0; j < nx[i]; j++) res_stat[i][j] = -1.0*pi_n[i-1][j];
                dgemv_n_3l(NX, NX, pQ[i], NX, w[i], res_stat[i]);
                dgemv_t_3l(NX, NX, pA[i], NX, pi_n[i], res_stat[i]);
                // printf("Stationarity residuals = %f\n",twonormv(NX,&res_stat[i][0]));
                // Eq. feasibility residuals
                for (int_t j = 0; j < NU; j++) res_eq[i][j] = pb[i][j];
                printf("Eq residuals = %f\n",twonormv(NX,&res_eq[i][0]));

                // for (int_t j = 0; j < nx[i]; j++) temp_stat[j] = 0.0;
                // daxpy_3l(NX,-1.0, lam_n[i],temp_stat);
                // dgemv_n_3l(NX, NX, pQ[i], NX, w[i], res_stat[i]);

            }
            i = N;
            // Stationarity residuals
            for (int_t j = 0; j < nx[i]; j++) res_stat[i][j] = -1.0*pi_n[i-1][j];
            dgemv_n_3l(NX, NX, pQ[i], NX, w[i], res_stat[i]);
            // for (int_t j = 0; j < nx[i]; j++) temp_stat[j] = 0.0;
            // daxpy_3l(NX,-1.0, lam_n[i],temp_stat);
            // dgemv_n_3l(NX, NX, pQ[i], NX, w[i], res_stat[i]);

            // printf("Stationarity residuals = %f\n",twonormv(NX,res_stat[i]));

            // Adjust barrier parameter
            int status = ocp_qp_hpmpc(&qp_in, &qp_out, &hpmpc_args, workspace);
            // int status = 0;
            // printf("hpmpc_status=%i\n",status);
            if (status == 1) printf("status = ACADOS_MAXITER\n");

            if (status == 2) printf("status = ACADOS_MINSTEP\n");

            i = 0;
            // printf("checkpoint=%i\n",status);
            for (int_t j = 0; j < NU; j++) w[i][j+NX] += qp_out.u[i][j];
            for (int_t j = 0; j < NX; j++) pi_n[i][j] = qp_out.pi[i][j];
            for (int_t j = 0; j < 2*(NBU); j++) lam_n[i][j] = qp_out.lam[i][j];
            // printf("checkpoint=%i\n",status);
            for (i = 1; i < N; i++) {
                for (int_t j = 0; j < NX; j++) w[i][j] += qp_out.x[i][j];
                for (int_t j = 0; j < NU; j++) w[i][j+NX] += qp_out.u[i][j];
                printf("x_step=%f\n",qp_out.x[i][0]);
                printf("u_step=%f\n",qp_out.u[i][0]);

                for (int_t j = 0; j < NX; j++) pi_n[i][j] = qp_out.pi[i][j];
                for (int_t j = 0; j < 2*(NBX + NBU); j++) lam_n[i][j] = qp_out.lam[i][j];
            }
            i = N;
            for (int_t j = 0; j < 2*(NBU); j++) lam_n[i][j] = qp_out.lam[i][j];
            for (int_t j = 0; j < NX; j++) w[i][j] += qp_out.x[i][j];
            printf("x_step=%f\n",qp_out.x[i][0]);

            // printf("checkpoint=%i\n",status);
        }
        timings += acado_toc(&timer);
    }
    #ifdef DEBUG
    print_states_controls(&w[0], N);
    for(int_t i = 0; i < N+1; i++)
    {
      for(int_t j = 0; j < 2; j++)
          printf("pi_n[%i][%i]= %f\n",i,j,pi_n[i][j]);
    }
    for(int_t i = 0; i < N; i++)
    {

    }
    #endif  // DEBUG
    printf("Average of %.3f ms per iteration.\n", 1e3*timings/timing_iters);
    // free(workspace);
    return 0;
}
