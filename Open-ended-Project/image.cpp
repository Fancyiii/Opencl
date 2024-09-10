#include "hls_stream.h"  // HLS streams
#include "ap_int.h"      // define arbitrary precision integer types
#include "ap_axi_sdata.h"  // defines a structure for AXI stream data

#define WIDTH 64
#define HEIGHT 48
#define NUM_CHANNELS 3

// Creating a custom structure which includes the data word and TLAST signal.
typedef ap_axis<32, 0, 0, 0> AXIS_wLAST;

void imageFiltering(unsigned char inputImg[WIDTH * HEIGHT * NUM_CHANNELS],
                    float lpMask[5 * 5],
                    float hpMask[5 * 5],
                    unsigned char outputImg[WIDTH * HEIGHT]) {
#pragma HLS INTERFACE m_axi port=inputImg offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=lpMask offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=hpMask offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=outputImg offset=slave bundle=gmem

#pragma HLS INTERFACE s_axilite port=inputImg bundle=control
#pragma HLS INTERFACE s_axilite port=lpMask bundle=control
#pragma HLS INTERFACE s_axilite port=hpMask bundle=control
#pragma HLS INTERFACE s_axilite port=outputImg bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    // Declare intermediate arrays for RGB channels and grayscale image
    unsigned char rChannel[WIDTH * HEIGHT];
    unsigned char gChannel[WIDTH * HEIGHT];
    unsigned char bChannel[WIDTH * HEIGHT];
    unsigned char grayImg[WIDTH * HEIGHT];

    // Separate RGB channels
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
#pragma HLS PIPELINE II=1
        rChannel[i] = inputImg[i];
        gChannel[i] = inputImg[i + WIDTH * HEIGHT];
        bChannel[i] = inputImg[i + 2 * WIDTH * HEIGHT];
    }

    // Convert RGB image to grayscale
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
#pragma HLS PIPELINE II=1
        grayImg[i] = (unsigned char)(((int)rChannel[i] + (int)gChannel[i] + (int)bChannel[i]) / 3);
    }
}
