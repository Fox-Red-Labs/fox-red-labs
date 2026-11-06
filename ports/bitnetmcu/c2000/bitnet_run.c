/*
 * bitnet_run.c: run BitNetMCU's MNIST models on the 10 test digits that ship with it.
 * C28x copy of ../bitnet_run.c (the Mac and MSPM0 version); the two patches are marked "C28x patch".
 *
 * BitMnistInference() is copied from upstream mcu/BitNetMCUdemo.c without changes, so
 * this file is GPL-3.0 like the rest of BitNetMCU (upstream/LICENSE).
 *
 * Pick the model at build time: -DMODEL_H='"BitNetMCU_model_cnn_64.h"' (or _cnn_16small, _12k, _1k).
 */
#include <stdint.h>
#include MODEL_H
#include "BitNetMCU_inference.c"
/* C28x patch: the test digits come from test_data_c28.h (make_test_data.py): the same values,
   written as signed numbers because int8_t is 16 bits here, and already const */
#include "test_data_c28.h"

#ifdef MODEL_CNNMNIST

uint32_t BitMnistInference(const int8_t *input) {
    int32_t layer_out[256];  // has to hold 16x16 image
    int8_t layer_in[MAX_N_ACTIVATIONS*4] __attribute__((aligned(2)));  /* C28x patch: also used as int32_t[],
                                                                     which needs an even word address */

    // Depthwise separable convolution with 32 bit activations and 8 bit weights
    int32_t *tmpbuf=(int32_t*)layer_out;
    int32_t *outputptr=(int32_t*)layer_in;
    for (uint32_t channel=0; channel < L7_out_channels; channel++) {

        for (uint32_t i=0; i < 16*16; i++) {
            tmpbuf[i]=input[i];
        }
        processconv33ReLU(tmpbuf,  L2_weights + 9*channel,  L2_incoming_x, 4, tmpbuf);
        processconv33ReLU(tmpbuf, L4_weights + 9*channel,  L4_incoming_x, 4, tmpbuf);
        processmaxpool22(tmpbuf, L6_incoming_x, tmpbuf);
        processconv33ReLU(tmpbuf, L7_weights + 9*channel,  L7_incoming_x, 4, tmpbuf);

        outputptr= processmaxpool22(tmpbuf, L9_incoming_x, outputptr);
    }

    // Normalization and conversion to 8-bit
    ReLUNorm((int32_t*)layer_in, layer_in, L7_out_channels * L9_outgoing_x * L9_outgoing_y);

    // Fully connected layers
    processfclayer(layer_in, L11_weights, L11_bitperweight, L11_incoming_weights,  L11_outgoing_weights, layer_out);
    ReLUNorm(layer_out, layer_in, L11_outgoing_weights);

    processfclayer(layer_in, L13_weights, L13_bitperweight, L13_incoming_weights,  L13_outgoing_weights, layer_out);
    ReLUNorm(layer_out, layer_in, L13_outgoing_weights);

    processfclayer(layer_in, L15_weights, L15_bitperweight, L15_incoming_weights,  L15_outgoing_weights, layer_out);
    return ReLUNorm(layer_out, layer_in, L15_outgoing_weights);
}

#elif defined(MODEL_FCMNIST)

uint32_t BitMnistInference(const int8_t *input) {
    int32_t layer_out[MAX_N_ACTIVATIONS];
    int8_t layer_in[MAX_N_ACTIVATIONS];
	int32_t prediction;

    processfclayer((int8_t*)input, L1_weights, L1_bitperweight, L1_incoming_weights, L1_outgoing_weights, layer_out);
    ReLUNorm(layer_out, layer_in, L1_outgoing_weights);

    processfclayer(layer_in, L2_weights, L2_bitperweight, L2_incoming_weights,  L2_outgoing_weights, layer_out);
    ReLUNorm(layer_out, layer_in, L2_outgoing_weights);

    processfclayer(layer_in, L3_weights, L3_bitperweight, L3_incoming_weights,  L3_outgoing_weights, layer_out);
    prediction=ReLUNorm(layer_out, layer_in, L3_outgoing_weights);

#if NUM_LAYERS == 4
    processfclayer(layer_in, L4_weights, L4_bitperweight, L4_incoming_weights,  L4_outgoing_weights, layer_out);
    prediction=ReLUNorm(layer_out, layer_in, L4_outgoing_weights);
#endif

	return prediction;
}

#endif

/* the 10 test digits and their labels, in one table */
#define N_TEST 10
static const int8_t *const test_in[N_TEST] = {input_data_0, input_data_1, input_data_2, input_data_3, input_data_4,
                                              input_data_5, input_data_6, input_data_7, input_data_8, input_data_9};
static const uint8_t *const test_label[N_TEST] = {&label_0, &label_1, &label_2, &label_3, &label_4,
                                                  &label_5, &label_6, &label_7, &label_8, &label_9};
