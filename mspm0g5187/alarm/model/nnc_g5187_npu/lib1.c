/*****************************************************************************/
/* Copyright (c) 2025 Texas Instruments Incorporated                         */
/* http://www.ti.com/                                                        */
/*                                                                           */
/*  Redistribution and  use in source  and binary forms, with  or without    */
/*  modification,  are permitted provided  that the  following conditions    */
/*  are met:                                                                 */
/*                                                                           */
/*     Redistributions  of source  code must  retain the  above copyright    */
/*     notice, this list of conditions and the following disclaimer.         */
/*                                                                           */
/*     Redistributions in binary form  must reproduce the above copyright    */
/*     notice, this  list of conditions  and the following  disclaimer in    */
/*     the  documentation  and/or   other  materials  provided  with  the    */
/*     distribution.                                                         */
/*                                                                           */
/*     Neither the  name of Texas Instruments Incorporated  nor the names    */
/*     of its  contributors may  be used to  endorse or  promote products    */
/*     derived  from   this  software  without   specific  prior  written    */
/*     permission.                                                           */
/*                                                                           */
/*  THIS SOFTWARE  IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS    */
/*  "AS IS"  AND ANY  EXPRESS OR IMPLIED  WARRANTIES, INCLUDING,  BUT NOT    */
/*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR    */
/*  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT    */
/*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    */
/*  SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES  (INCLUDING, BUT  NOT    */
/*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,    */
/*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    */
/*  THEORY OF  LIABILITY, WHETHER IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE    */
/*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.     */
/*                                                                           */
/*****************************************************************************/
// tvm target: c -keys=arm_cpu,cpu -mcpu=cortex-m0plus
#define TVM_EXPORTS
#include "tvm/runtime/c_runtime_api.h"
#include "tvm/runtime/c_backend_api.h"
#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "hard/inc/tinie.h"
volatile int32_t tinie_curr_layer __attribute__((weak));
volatile int32_t tinie_curr_sublayer __attribute__((weak));
volatile int32_t tinie_num_sublayers __attribute__((weak));
void (*tinie_sublayer_postproc)(void) __attribute__((weak));
volatile int32_t tvmgen_default_finished;
void tvmgen_default_IRQHandlerHelper();
TINIE_layer_t* __attribute__((weak)) tinie_sublayers;
__attribute__((weak)) void tinie_sublayer_IRQHandler() {
  if (tinie_sublayer_postproc != NULL)  tinie_sublayer_postproc();

  TINIE_setup(&tinie_sublayers[tinie_curr_sublayer]);
  tinie_curr_sublayer += 1;
  TINIE_start();
  return;
}
__attribute__((interrupt)) void tvmgen_default_IRQHandler() {
  (void) tvmgen_default_IRQHandlerHelper();
}
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif
static float* input_buffer_var;
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif
static float* output_buffer_var;
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif
static uint8_t* global_const_workspace_0_var;
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif
static uint8_t* global_workspace_1_var;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_7_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_3_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_9_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_10_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_8_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_2_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_12_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_4_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_5_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_1_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_13_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_6_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C" {
#endif
  static void* sid_11_let;
#ifdef __cplusplus
} // extern "C"
#endif
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_multiply_floor_clip_cast_layout_transform(float* __restrict__ p0, int8_t* __restrict__ T_layout_trans, uint8_t* __restrict__ global_const_workspace_2_var, uint8_t* __restrict__ global_workspace_3_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_cast(int8_t* __restrict__ p0, float* __restrict__ T_cast, uint8_t* __restrict__ global_const_workspace_28_var, uint8_t* __restrict__ global_workspace_29_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default___tvm_main__(float* input_buffer_var, float* output_buffer_var, uint8_t* global_const_workspace_0_var, uint8_t* global_workspace_1_var);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL float floorf(float);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_zeroPaddingNHWC_lib_0(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_convolveNHWC_1(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_maxPoolNHWC_2(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_convolveNHWC_3(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_maxPoolNHWC_4(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_convolveNHWC_5(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_maxPoolNHWC_6(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_convolveNHWC_7(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_maxPoolNHWC_8(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_convolveNHWC_9(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_avgPoolNHWC_10(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_ti_npu_fullyConnectedNHWC_11(int8_t*, int8_t*, uint8_t*, uint8_t*);
#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_add_multiply_floor_clip_cast_layout_transform(float* __restrict__ p0, int8_t* __restrict__ T_layout_trans, uint8_t* __restrict__ global_const_workspace_2_var, uint8_t* __restrict__ global_workspace_3_var) {
  void* fused_add_constant_let = (&(global_const_workspace_2_var[64]));
  void* fused_constant_let = (&(global_const_workspace_2_var[0]));
  for (int32_t ax0_ax1_fused_ax2_fused = 0; ax0_ax1_fused_ax2_fused < 128; ++ax0_ax1_fused_ax2_fused) {
    for (int32_t ax3 = 0; ax3 < 16; ++ax3) {
      float v_ = floorf(((p0[((ax3 * 128) + ax0_ax1_fused_ax2_fused)] + ((float*)fused_constant_let)[ax3]) * ((float*)fused_add_constant_let)[ax3]));
      float v__1 = (v_) < (1.270000e+02f) ? (v_) : (1.270000e+02f);
      T_layout_trans[((ax0_ax1_fused_ax2_fused * 16) + ax3)] = ((int8_t)((v__1) > (-1.280000e+02f) ? (v__1) : (-1.280000e+02f)));
    }
  }
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default_fused_cast(int8_t* __restrict__ p0, float* __restrict__ T_cast, uint8_t* __restrict__ global_const_workspace_28_var, uint8_t* __restrict__ global_workspace_29_var) {
  for (int32_t ax1 = 0; ax1 < 3; ++ax1) {
    T_cast[ax1] = ((float)p0[ax1]);
  }
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL int32_t tvmgen_default___tvm_main__(float* input_buffer_var_arg, float* output_buffer_var_arg, uint8_t* global_const_workspace_0_var_arg, uint8_t* global_workspace_1_var_arg) {
  tinie_curr_layer = 0;
  tvmgen_default_finished = 0;
  tinie_curr_sublayer = 0;
  tinie_num_sublayers = 0;
  tinie_sublayer_postproc = NULL;
  TINIE_registerInterrupt(tvmgen_default_IRQHandler);
  input_buffer_var = input_buffer_var_arg;
  output_buffer_var = output_buffer_var_arg;
  global_const_workspace_0_var = global_const_workspace_0_var_arg;
  global_workspace_1_var = global_workspace_1_var_arg;
  (void)tvmgen_default_IRQHandlerHelper();
  return 0;
}

#ifdef __cplusplus
extern "C"
#endif
TVM_DLL void tvmgen_default_IRQHandlerHelper() {
  TINIE_clearInterruptACK();
  if (tinie_curr_sublayer < tinie_num_sublayers) {
    tinie_sublayer_IRQHandler();
    return;
  }
switch(tinie_curr_layer) {
case 0:
   sid_7_let = (&(global_workspace_1_var[0]));
   sid_3_let = (&(global_workspace_1_var[2112]));
   sid_9_let = (&(global_workspace_1_var[0]));
   sid_10_let = (&(global_workspace_1_var[576]));
   sid_8_let = (&(global_workspace_1_var[1088]));
   sid_2_let = (&(global_workspace_1_var[0]));
   sid_12_let = (&(global_workspace_1_var[256]));
   sid_4_let = (&(global_workspace_1_var[0]));
   sid_5_let = (&(global_workspace_1_var[1088]));
   sid_1_let = (&(global_workspace_1_var[2112]));
   sid_13_let = (&(global_workspace_1_var[0]));
   sid_6_let = (&(global_workspace_1_var[2144]));
   sid_11_let = (&(global_workspace_1_var[0]));
  tvmgen_default_fused_add_multiply_floor_clip_cast_layout_transform(input_buffer_var, sid_1_let, global_const_workspace_0_var, global_workspace_1_var);
  tvmgen_default_ti_npu_zeroPaddingNHWC_lib_0(sid_1_let, sid_2_let, global_const_workspace_0_var, global_workspace_1_var);
  tvmgen_default_ti_npu_convolveNHWC_1(sid_2_let, sid_3_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 1:
  tvmgen_default_ti_npu_maxPoolNHWC_2(sid_3_let, sid_4_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 2:
  tvmgen_default_ti_npu_convolveNHWC_3(sid_4_let, sid_5_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 3:
  tvmgen_default_ti_npu_maxPoolNHWC_4(sid_5_let, sid_6_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 4:
  tvmgen_default_ti_npu_convolveNHWC_5(sid_6_let, sid_7_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 5:
  tvmgen_default_ti_npu_maxPoolNHWC_6(sid_7_let, sid_8_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 6:
  tvmgen_default_ti_npu_convolveNHWC_7(sid_8_let, sid_9_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 7:
  tvmgen_default_ti_npu_maxPoolNHWC_8(sid_9_let, sid_10_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 8:
  tvmgen_default_ti_npu_convolveNHWC_9(sid_10_let, sid_11_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 9:
  tvmgen_default_ti_npu_avgPoolNHWC_10(sid_11_let, sid_12_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 10:
  tvmgen_default_ti_npu_fullyConnectedNHWC_11(sid_12_let, sid_13_let, global_const_workspace_0_var, global_workspace_1_var);
  return;
case 11:
  tvmgen_default_fused_cast(sid_13_let, output_buffer_var, global_const_workspace_0_var, global_workspace_1_var);
  tvmgen_default_finished = 1;
  break;
default:
  break;
}
}

