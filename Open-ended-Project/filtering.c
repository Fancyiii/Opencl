#include "xparameters.h"
#include "xtmrctr.h"
#include "xuartps.h"
#include "xil_printf.h"
#include "stdio.h"
#include "xllfifo.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef SDT
#define UART_DEVICE_ID XPAR_XUARTPS_0_DEVICE_ID
#else
#define XUARTPS_BASEADDRESS XPAR_XUARTPS_0_BASEADDR
#endif

#ifndef SDT
#define TMRCTR_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#else
#define XTMRCTR_BASEADDRESS XPAR_XTMRCTR_0_BASEADDR
#endif

#define TIMER_COUNTER_0 0

#define TIMEOUT_VALUE (1 << 20) // timeout for reception
#define FIFO_DEV_ID	   	XPAR_AXI_FIFO_0_DEVICE_ID

#ifndef SDT
int UartPsHelloWorldExample(u16 DeviceId);
#else
int UartPsHelloWorldExample(UINTPTR BaseAddress);
#endif


// Function declarations
void imageFiltering_soft(unsigned char* inputImg, unsigned int imgWidth, unsigned int imgHeight, int numChannels);
void imageFiltering_hard(unsigned char* inputImg, unsigned int imgWidth, unsigned int imgHeight, int numChannels);
unsigned int calculateChecksum(const unsigned char* data, size_t size);
int compareChecksums(unsigned int expectedChecksum, unsigned int actualChecksum);

XUartPs Uart_Ps;		/* The instance of the UART Driver */
u16 DeviceId = FIFO_DEV_ID;
XLlFifo FifoInstance; 	// Device instance
XLlFifo *InstancePtr = &FifoInstance; // Device pointer

int main() {
    int Status;
    u32 Value1;
    u32 Value2;
    XTmrCtr TimerCounter; /* The instance of the Tmrctr Device */
    XTmrCtr *TmrCtrInstancePtr = &TimerCounter;

    const char* filename = "input_img.jpg";
    const unsigned int imgWidth;
    const unsigned int imgHeight; 
    const int numChannels; 
    unsigned char *inputImg = stbi_load(filename, &imgWidth, &imgHeight, &numChannels, 0);
    size_t imgSize = imgWidth * imgHeight * numChannels;
    unsigned int initialChecksum = calculateChecksum(inputImg, imgSize);
    /*
     * Initialize the timer counter so that it's ready to use,
     * specify the device ID that is generated in xparameters.h
     */
#ifndef SDT
    Status = XTmrCtr_Initialize(TmrCtrInstancePtr, TMRCTR_DEVICE_ID);
#else
    Status = XTmrCtr_Initialize(TmrCtrInstancePtr, BaseAddr);
#endif
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Perform a self-test to ensure that the hardware was built
     * correctly, use the 1st timer in the device (0)
     */
    Status = XTmrCtr_SelfTest(TmrCtrInstancePtr, TIMER_COUNTER_0);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    /*
     * Enable the Autoreload mode of the timer counters.
     */
    XTmrCtr_SetOptions(TmrCtrInstancePtr, TIMER_COUNTER_0, XTC_AUTO_RELOAD_OPTION);

    /*
     * Get a snapshot of the timer counter value before it's started
     * to compare against later
     */
    Value1 = XTmrCtr_GetValue(TmrCtrInstancePtr, TIMER_COUNTER_0);

    /************************** Initializations *****************************/
    XLlFifo_Config *Config;

    /* Initialize the Device Configuration Interface driver */
    Config = XLlFfio_LookupConfig(DeviceId);
    if (!Config) {
		xil_printf("No config found for %d\r\n", DeviceId);
		return XST_FAILURE;
	}

	Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed\r\n");
		return XST_FAILURE;
	}

    /*
     * Start the timer counter such that it's incrementing by default
     */
    XTmrCtr_Start(TmrCtrInstancePtr, TIMER_COUNTER_0);

    xil_printf("Everything before the imagefiltering is running!\r\n");
    //imageFiltering_soft(inputImg, imgWidth, imgHeight, numChannels);
    //xil_printf("Imagefiltering has finished run on the board!\r\n");
    imageFiltering_hard(inputImg, imgWidth, imgHeight, numChannels);
    xil_printf("Imagefiltering has finished run with coprocessor!\r\n");

    Value2 = XTmrCtr_GetValue(TmrCtrInstancePtr, TIMER_COUNTER_0);
    /*
     * Disable the Autoreload mode of the timer counters.
     */
    XTmrCtr_SetOptions(TmrCtrInstancePtr, TIMER_COUNTER_0, 0);

    unsigned int finalChecksum = calculateChecksum(inputImg, imgSize);

    // Compare checksums
    if (compareChecksums(initialChecksum, finalChecksum) == XST_FAILURE) {
        xil_printf("Imagefiltering mismatch!\r\n");
        return XST_FAILURE;
    }

	int time = 0;
	int num_ticks = Value2 - Value1;
	time = (Value2 - Value1)/(XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ/100);

	xil_printf("System frequency is %d\n", XPAR_AXI_TIMER_0_CLOCK_FREQ_HZ);
	xil_printf("Total time taken (number of ticks): %d ticks\n", num_ticks);
	xil_printf("Total time taken : %d.%d second\n", time/100, time%100);

	xil_printf("Test Success\r\n");

    return XST_SUCCESS;
}

// =================================================================
// ---------------------- Secondary Functions ----------------------
// =================================================================

void imageFiltering_soft(unsigned char* inputImg,
                    unsigned int imgWidth,
                    unsigned int imgHeight,
                    int numChannels) {

    // Allocate memory for grayscale image
    unsigned char* grayImg = (unsigned char*)malloc(imgWidth * imgHeight * sizeof(unsigned char));

    // Convert RGB image to grayscale and apply filters
    for (int i = 0; i < imgWidth; i++) {
        for (int j = 0; j < imgHeight; j++) {
            // Compute average pixel
            size_t idx = i + j * imgWidth;
            grayImg[idx] = (inputImg[idx] + inputImg[idx + imgWidth * imgHeight] + inputImg[idx + 2 * imgWidth * imgHeight]) / 3;
            xil_printf("grayImg is successfully running! Loop %d %d \r\n", i, j);
        }
    }
    // Free dynamically allocated memory
    free(inputImg);
    free(grayImg);
}

void imageFiltering_hard(unsigned char* inputImg,
                    unsigned int imgWidth,
                    unsigned int imgHeight,
                    int numChannels) {
    /* Check for the Reset value */
    int Status = XLlFifo_Status(InstancePtr);
    XLlFifo_IntClear(InstancePtr,0xffffffff);
    Status = XLlFifo_Status(InstancePtr);
    if(Status != 0x0) {
        xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t. Expected : 0x0\r\n",
                XLlFifo_Status(InstancePtr));
        return XST_FAILURE;
    }
    /******************** Input to Coprocessor : Transmit the Data Stream ***********************/
    //xil_printf(" Transmitting Data ... \r\n");

    // Allocate memory for grayscale image
    unsigned char* grayImg = (unsigned char*)malloc(imgWidth * imgHeight * sizeof(unsigned char));

    // Convert RGB image to grayscale and apply filters
    for (int i = 0; i < imgWidth; i++) {
        for (int j = 0; j < imgHeight; j++) {
            // Compute average pixel
            size_t idx = i + j * imgWidth;
            grayImg[idx] = (inputImg[idx] + inputImg[idx + imgWidth * imgHeight] + inputImg[idx + 2 * imgWidth * imgHeight]) / 3;
            xil_printf("grayImg is successfully running with coprocessor! Loop %d %d \r\n", i, j);
        }
    }

    // Transmitter Part
    for (int i = 0; i < imgWidth * imgHeight * numChannels; i++) {
        if( XLlFifo_iTxVacancy(InstancePtr) ){
            XLlFifo_TxPutWord(InstancePtr, grayImg[i]); // Transmitted grayscale image
        }
    }

    // Start Transmission by writing transmission length (number of bytes) into the TLR
    XLlFifo_iTxSetLen(InstancePtr, imgWidth * imgHeight * sizeof(unsigned char)); // Adjusted length

    // Check for Transmission completion
    while( !(XLlFifo_IsTxDone(InstancePtr)) ){

    }

    // Receiver Part
    int timeout_count = TIMEOUT_VALUE;
    // Wait for coprocessor to send data, subject to a timeout
	while(!XLlFifo_iRxOccupancy(InstancePtr)) {
		timeout_count--;
		if (timeout_count == 0)
		{
			xil_printf("Timeout while waiting for data ... \r\n");
			return XST_FAILURE;
		}
	}
    // unsigned int result[RECEIVE_BUFFER_SIZE]; // Assuming RECEIVE_BUFFER_SIZE is defined appropriately

	// u32 ReceiveLength = XLlFifo_iRxGetLen(InstancePtr)/4;
	// for (int i=0; i < ReceiveLength; i++) {
	// 	// read one word at a time
	// 	result[i] = XLlFifo_RxGetWord(InstancePtr);
	// }


	if(XLlFifo_IsRxDone(InstancePtr) != TRUE){
		xil_printf("Failing in receive complete ... \r\n");
		return XST_FAILURE;
	}
    // Free dynamically allocated memory
    free(inputImg);
    free(grayImg);

    return XST_SUCCESS;
}


unsigned int calculateChecksum(const unsigned char* data, size_t size) {
    unsigned int checksum = 0;

    for (size_t i = 0; i < size; ++i) {
        checksum += data[i];
    }
    return checksum;
}

// Function to compare checksums
int compareChecksums(unsigned int expectedChecksum, unsigned int actualChecksum) {
    if (expectedChecksum == actualChecksum) {
        xil_printf("Checksums match. Image data integrity is maintained.\n");
        return XST_SUCCESS; // Success
    } else {
        xil_printf("Checksums do not match. Image data integrity may be compromised.\n");
        return XST_FAILURE; // Failure
    }
}
