#include <stdio.h>
#include "hls_stream.h"
#include "ap_axi_sdata.h"

/***************** AXIS with TLAST structure declaration *********************/
typedef ap_axis<32, 0, 0, 0> AXIS_wLAST;

/***************** Macros *********************/
#define WIDTH 64
#define HEIGHT 48
#define NUM_CHANNELS 3

#define NUMBER_OF_TEST_VECTORS 1  // Number of test vectors (cases)

/***************** Function declaration *********************/
void imageFiltering(unsigned char inputImg[WIDTH * HEIGHT * NUM_CHANNELS],
                    float lpMask[5 * 5],
                    float hpMask[5 * 5],
                    unsigned char outputImg[WIDTH * HEIGHT]);

/************************** Variable Definitions *****************************/
unsigned char inputImg[WIDTH * HEIGHT * NUM_CHANNELS];  // Input image
float lpMask[5 * 5];  // Low-pass mask
float hpMask[5 * 5];  // High-pass mask
unsigned char outputImg_expected[WIDTH * HEIGHT];  // Expected output image

/*****************************************************************************
 * Main function    
 *****************************************************************************/
int main() {
    int word_cnt;
    int success;
    AXIS_wLAST write_input, read_output;
    hls::stream<AXIS_wLAST> S_AXIS;
    hls::stream<AXIS_wLAST> M_AXIS;

    /************** Initialize input data and expected output *****************/
    // Initialize input image
    for (int i = 0; i < WIDTH * HEIGHT * NUM_CHANNELS; i++) {
        inputImg[i] = i % 256;  // Arbitrary input data
    }

    // Initialize low-pass mask
    for (int i = 0; i < 5 * 5; i++) {
        lpMask[i] = 0.04f;
    }

    // Initialize high-pass mask
    for (int i = 0; i < 5 * 5; i++) {
        hpMask[i] = -1.0f;
    }
    hpMask[12] = 24.0f;

    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        outputImg_expected[i] = (inputImg[i] + inputImg[i + WIDTH * HEIGHT] + inputImg[i + 2 * WIDTH * HEIGHT]) / 3;
    }

    /******************** Input to Function : Transmit the Data Stream ***********************/

    printf(" Transmitting Data ...\r\n");

    // Transmit input image
    for (int i = 0; i < WIDTH * HEIGHT * NUM_CHANNELS; i++) {
        write_input.data = inputImg[i];
        write_input.last = 0;
        if ((i + 1) % 64 == 0) {
            write_input.last = 1;
        }
        S_AXIS.write(write_input);
    }

    // Transmit low-pass mask
    for (int i = 0; i < 5 * 5; i++) {
        write_input.data = *((int*)&lpMask[i]);
        write_input.last = 0;
        if (i == 5 * 5 - 1) {
            write_input.last = 1;
        }
        S_AXIS.write(write_input);
    }

    // Transmit high-pass mask
    for (int i = 0; i < 5 * 5; i++) {
        write_input.data = *((int*)&hpMask[i]);
        write_input.last = 0;
        if (i == 5 * 5 - 1) {
            write_input.last = 1;
        }
        S_AXIS.write(write_input);
    }

    /* Transmission Complete */

    /********************* Call the function (invoke the IP) ***************/
    imageFiltering(inputImg, lpMask, hpMask, outputImg_expected);

    /******************** Output from Function : Receive the Data Stream ***********************/

    printf(" Receiving data ...\r\n");

    // Receive output image
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        read_output = M_AXIS.read();
        // Compare output with expected output
        if (read_output.data != outputImg_expected[i]) {
            printf("Output mismatch at index %d\r\n", i);
            return 1;  // Test failed
        }
    }

    /* Reception Complete */

    printf("Test Success\r\n");

    return 0;
}
