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
#define TINIE_NPU_EXTRA_BUF_PADDING 32
#include "tvm/runtime/c_runtime_api.h"
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((section(".rodata.tvm"), ))
static const struct global_const_workspace {
  float fused_constant_let[16] __attribute__((aligned(16))); // 64 bytes, aligned offset: 0
  float fused_add_constant_let[16] __attribute__((packed, aligned(16))); // 64 bytes, aligned offset: 64
} const global_const_workspace = {
  .fused_constant_let = {
    0x1.164ab6p+2, 0x1.7de38ap+1, 0x1.ecdc44p+1, 0x1.85b44ep+1, 0x1.e952aap+1, 0x1.cdc83p+1, 0x1.ee2c24p+1, 0x1.942dd4p+1, 
    0x1.eba2ecp+1, 0x1.eb61fep+0, -0x1.ff0a64p-2, -0x1.73e15ep+0, 0x1.052334p+1, 0x1.135532p+1, 0x1.fccbe4p-5, 0x1.0a6ef2p+2
  },
  .fused_add_constant_let = {
    0x1.69cp+3, 0x1.02cp+4, 0x1.2fap+4, 0x1.88cp+3, 0x1.cep+3, 0x1.22cp+4, 0x1.fc4p+3, 0x1.826p+4, 
    0x1.12cp+4, 0x1.604p+4, 0x1.98p+6, 0x1.822p+6, 0x1.138p+3, 0x1.606p+4, 0x1.2acp+5, 0x1.7f8p+2
  },
};// of total size 128 bytes
__attribute__((section(".bss.noinit.tvm"), aligned(16)))
static uint8_t global_workspace[TINIE_NPU_EXTRA_BUF_PADDING + 4192];
#include "tvmgen_default.h"
TVM_DLL int32_t tvmgen_default___tvm_main__(void* input,void* output0,uint8_t* global_const_workspace_0_var,uint8_t* global_workspace_1_var);
int32_t tvmgen_default_run(struct tvmgen_default_inputs* inputs,struct tvmgen_default_outputs* outputs) {return tvmgen_default___tvm_main__(inputs->input,outputs->output,((uint8_t*)&global_const_workspace),((uint8_t*)&global_workspace));
}
#ifdef __cplusplus
}
#endif
;