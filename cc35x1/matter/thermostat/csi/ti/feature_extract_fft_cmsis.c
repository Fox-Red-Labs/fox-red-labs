#include <math.h>
#include "user_input_config.h"

/* ARM CMSIS-DSP Header files */
#include "arm_math.h"

/* Try to use predefined const structs if available, fallback to init-based approach */
#ifdef arm_cfft_sR_f32_len256
#include "arm_const_structs.h"
#define USE_CONST_STRUCTS 1
#else
#define USE_CONST_STRUCTS 0
#endif

void FE_cfft(float *input, float *output, uint16_t frame_size, uint16_t fe_stages) {

    for (uint16_t i = 0; i < FE_FRAME_SIZE; i++) {
        output[2*i] = input[i];
        output[2*i+1] = 0.0f;
    }

#if USE_CONST_STRUCTS
    /* Use predefined const structs when available */
    const arm_cfft_instance_f32 *cfft_instance;

    switch (FE_FRAME_SIZE) {
        case 32:
            cfft_instance = &arm_cfft_sR_f32_len32;
            break;
        case 64:
            cfft_instance = &arm_cfft_sR_f32_len64;
            break;
        case 128:
            cfft_instance = &arm_cfft_sR_f32_len128;
            break;
        case 256:
            cfft_instance = &arm_cfft_sR_f32_len256;
            break;
        case 512:
            cfft_instance = &arm_cfft_sR_f32_len512;
            break;
        case 1024:
            cfft_instance = &arm_cfft_sR_f32_len1024;
            break;
        case 2048:
            cfft_instance = &arm_cfft_sR_f32_len2048;
            break;
        case 4096:
            cfft_instance = &arm_cfft_sR_f32_len4096;
            break;
        default:
            return;
    }
    arm_cfft_f32(cfft_instance, output, 0, 1);
#else
    /* Fallback: initialize FFT dynamically when const structs unavailable
     * Use arm_cfft_init_f32 to dynamically create and initialize the instance */
    arm_cfft_instance_f32 cfft_instance;
    arm_status status = arm_cfft_init_f32(&cfft_instance, FE_FRAME_SIZE);
    if (status != ARM_MATH_SUCCESS) {
        return;
    }
    arm_cfft_f32(&cfft_instance, output, 0, 1);
#endif

    arm_cmplx_mag_f32(output, input, frame_size);

}