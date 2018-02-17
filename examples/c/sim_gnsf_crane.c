/*
 *    This file is part of acados.
 *
 *    acados is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    acados is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with acados; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// external
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
// acados
// #include <acados_c/sim.h>
// #include <acados_c/options.h>

#include <acados/sim/sim_gnsf.h>
#include <acados/sim/sim_common.h>
#include <acados/sim/sim_gnsf_casadi_wrapper.h>

#include "examples/c/gnsf_crane_model/gnsf_crane_model.h"

// blasfeo
// #include <blasfeo/include/blasfeo_target.h>
// #include <blasfeo/include/blasfeo_common.h>
// #include <blasfeo/include/blasfeo_d_aux.h>
// #include <blasfeo/include/blasfeo_d_aux_ext_dep.h>
// #include <blasfeo/include/blasfeo_v_aux_ext_dep.h>
// #include <blasfeo/include/blasfeo_d_blas.h>
int main() {

    gnsf_dims dims;
    gnsf_fixed fix;
    sim_rk_opts opts;
    // opts.A_mat = (double*) calloc(dims.num_stages * dims.num_stages, sizeof(double));
    // *opts.A_mat = 1;
    
    // gnsf_allocate_fixed(&dims,&fix);
    gnsf_get_dims(&dims, get_ints_fun);
    // dims.num_steps = 2;
    gnsf_get_KK_mat(&dims, &fix, KKmat_fun);
    gnsf_get_ZZ_mat(&dims, &fix, ZZmat_fun);
    gnsf_get_ALO_M2_dK2dx2( &dims, &fix, ALO_M2_dK2dx2_fun);
    
    
    //sim_rk_opts *erk_opts = (sim_rk_opts *) args;
    //erk_opts->num_steps = 4;

    // double *res_in;
    // int res_in_size = dims.nff + dims.nx1 + dims.nu;
    // res_in = (double*) calloc(res_in_size, sizeof(double));


    // double *res_out;
    // int res_out_size = dims.nff * (1 + dims.nff);
    // res_out = (double*) calloc(res_out_size, sizeof(double));

    // res_in[2+dims.nff] = 0.8;

    // print_gnsf_res_in( dims, res_in );
    // print_gnsf_res_out( dims, res_out );

    // res_inc_Jff_wrapped(dims.nx1, dims.nu, dims.n_out, dims.num_stages, res_in, res_out, res_inc_Jff_fun);
    // print_gnsf_res_out( dims, res_out );

    gnsf_in in;
    in.res_inc_Jff = res_inc_Jff_fun;
    in.f_LO_inc_J_x1k1uz = f_LO_inc_J_x1k1uz_fun;
    in.jac_res_ffx1u = jac_res_ffx1u_fun;
    gnsf_out out;
    fix.A_dt = (double*) calloc(dims.num_stages * dims.num_stages, sizeof(double)); // TODO write allocate gnsf_in fcn
    in.u = (double*) calloc(dims.nu, sizeof(double));
    in.x = (double*) calloc(dims.nx, sizeof(double));
    in.S_forw = (double*) calloc(dims.nx * (dims.nx + dims.nu), sizeof(double));
    out.S_forw = (double*) calloc(dims.nx * (dims.nx + dims.nu), sizeof(double));

    for (int ii = 0; ii < dims.nx; ii++) {
        in.S_forw[ii+ ii*dims.nx] = 1.0;
    }

    out.xf = (double*) calloc(dims.nx, sizeof(double));

    in.x[2] = 0.8;
    in.u[0] = 40.108149413030752;
    in.u[1] = -50.446662212534974;
    printf("test\n");
    gnsf_get_butcher(&dims, &fix, Butcher_fun);
    gnsf_simulate( &dims, &fix, &in, &out );
    // blasfeo_free_dmat( &fix.ALO);

    return 0;
}