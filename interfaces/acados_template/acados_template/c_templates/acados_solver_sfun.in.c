#define S_FUNCTION_NAME   acados_solver_sfun
#define S_FUNCTION_LEVEL  2

#define MDL_START

#include "acado_common.h"
#include "acado_auxiliary_functions.h"
#include "simstruc.h"

#define SAMPLINGTIME -1

static void mdlInitializeSizes (SimStruct *S)
{
    // specify the number of continuous and discrete states 
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);

    // specify the number of input ports 
    if ( !ssSetNumInputPorts(S, 1) )
        return;

    // specify the number of output ports 
    if ( !ssSetNumOutputPorts(S, 3) )
        return;

    // specify dimension information for the input ports 
    ssSetInputPortVectorDimension(S, 0, {{ ra.dims.nx }});

    // specify dimension information for the output ports 
    ssSetOutputPortVectorDimension(S,  0, {{ ra.dims.nu }} ); // optimal input
    ssSetOutputPortMatrixDimensions(S, 1, 1 );                // solver status
    ssSetOutputPortMatrixDimensions(S, 2, 1 );                // KKT residuals

    // specify the direct feedthrough status 
    ssSetInputPortDirectFeedThrough(S, 0, 1); // current state x0

    // one sample time 
    ssSetNumSampleTimes(S, 1);
    }


#if defined(MATLAB_MEX_FILE)

#define MDL_SET_INPUT_PORT_DIMENSION_INFO
#define MDL_SET_OUTPUT_PORT_DIMENSION_INFO

static void mdlSetInputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo)
{
    if ( !ssSetInputPortDimensionInfo(S, port, dimsInfo) )
         return;
}

static void mdlSetOutputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo)
{
    if ( !ssSetOutputPortDimensionInfo(S, port, dimsInfo) )
         return;
}

    #endif /* MATLAB_MEX_FILE */


static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, SAMPLINGTIME);
    ssSetOffsetTime(S, 0, 0.0);
}


static void mdlStart(SimStruct *S)
{
    acados_create();
}

static void mdlOutputs(SimStruct *S, int_T tid)
{
    // get input signals
    InputRealPtrsType in_x0;

    in_x0 = ssGetInputPortRealSignalPtrs(S, 0);
    
    // assign pointers to output signals 
    real_t *out_u0, *status, *KKT_res;

    // get pointers to acados structures 
    nlp_opts = acados_get_nlp_opts()
    nlp_dims = acados_get_nlp_dims()
    nlp_config = acados_get_nlp_config()
    nlp_out = acados_get_nlp_out()
    nlp_in = acados_get_nlp_in()

    // call acados_solve()
    ocp_nlp_out_get(nlp_config, nlp_dims, nlp_out, 0, "x", in_x0);
    ocp_nlp_out_get(nlp_config, nlp_dims, nlp_out, 0, "u", out_u0);

    out_u0      = ssGetOutputPortRealSignal(S, 0);
    out_status  = ssGetOutputPortRealSignal(S, 1);
    KKT_res     = ssGetOutputPortRealSignal(S, 2);
}

static void mdlTerminate(SimStruct *S)
{
    acados_free();
}


#ifdef  MATLAB_MEX_FILE
#include "simulink.c"
#else
#include "cg_sfun.h"
#endif
