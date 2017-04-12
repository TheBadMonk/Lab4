#include "hps_soc_system.h"
#include "socal.h"
#include "hps.h"
#include <stdio.h>

#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000

//function signature
#define MASK_1								0x00000001

//ALT_FPGA_BRIDGE_LWH2F_OFST 0xc0000000

//main
int main(void)
{
	//********************
	//pointer declarations
	//********************
	volatile int * KEY_ptr				   						= (int *) KEY_BASE;
	volatile int * Video_In_DMA_ptr	 						= (int *) VIDEO_IN_BASE;
	volatile short * Video_Mem_ptr	 						= (short *) FPGA_ONCHIP_BASE;
	//************************
	//end pointer declarations
	//************************

	//*********************
	//variable declarations
	//*********************
	//iterators for image loops
	int x, y, i, j;
	//records pressed key
	int keyPressed;
	//temp array to store picture in for transforms
	short pictureArray[320][240];
	//temp compressedPic
	int preProcessedPic[320*240]; //76800//before encodeing
	//buffer for inData
	int inData, outData;//in = encoded out = tobe encoded
	//output picture
	int decompressedPic[320*240];
	//values used for decompression
	int bitId, repeatCount;
	int writeCount, readCount, outputBaseCounter;//how many 8 bits have we given the encoder
	uint8_t sdramBuffer, fifoOutBuffer;
	uint32_t readInBuffer;
	//*************************
	//end variable declarations
	//*************************

	//self-explanatory wrapper for readability
	void enableVideo(){
		*(Video_In_DMA_ptr + 3)	= 0x4; // Enable the video
	}

	//self-explanatory wrapper for readability
	void disableVideo(){
		*(Video_In_DMA_ptr + 3) = 0x0;	// Disable the video to capture one frame
	}

	//initially, saves picture to array instead of writing directly to VGA
	void writeToArray(){
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				short tempPixel = *(Video_Mem_ptr + (y << 9) + x);
				pictureArray[x][y] = tempPixel;
			}
		}
	}

	//writes output of pictureArray to VGA
	void writeToVGA(){
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				*(Video_Mem_ptr + (y << 9) + x) = pictureArray[x][y];
			}
		}
	}

	//takes a picture, increments picture count, displays picture, displays overlay text
	void takeStandardPicture(){
		disableVideo();
		writeToArray();
		writeToVGA();
	}

	//does in-array (pictureArray) compression of image and displays it
	void createPreProcessedImage(){
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				preProcessedPic[320*y+x] = pictureArray[x][y];
			}
		}
		alt_write_byte(SDRAM_BASE, preProcessedPic);
	}

	void displayBinaryImage(int binaryImage[]) {
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				if(binaryImage[320*y+x]==1)
					*(Video_Mem_ptr + (y << 9) + x)=0xFFFFFFFF;
				else
					*(Video_Mem_ptr + (y << 9) + x)=0;
			}
		}
	}

	int roundPixel(int pixel){
		return (pixel>=0xFFFFFFFF/0x2);
	}

	void preProcessImageAndDisplay(){
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				pictureArray[x][y] = roundPixel(*(Video_Mem_ptr + (y << 9) + x));
				// printf("%#04x\n", *(Video_Mem_ptr + (y << 9) + x));
				// printf("%d\n", pictureArray[x][y]);
			}
		}
		createPreProcessedImage(); //saves to preProcessedPic
		displayBinaryImage(preProcessedPic); //pass it in to display
	}


	//****************************************************
	//WIP WIP WIP WIP WIP WIP WIP WIP WIP WIP WIP WIP WIP
	//****************************************************

	void writeImgToSdram(){
		for(i = 0; i < (240*320); i+=8) {
			for(j = 0; j < 8; j++) {
				sdramBuffer = sdramBuffer << 1;
				sdramBuffer = sdramBuffer + (preProcessedPic[i+j] & MASK_1);
			}
			alt_write_byte(SDRAM_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST+(8*i), sdramBuffer);
		}
	}

	void writeOutData() {
		alt_write_byte(ODATA_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST, alt_read_byte(SDRAM_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST+(8*writeCount)));
		writeCount++;
		if(writeCount >= ((240*320)/8)-1) {
			alt_write_byte(FIFO_IN_WRITE_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST, 0);
		}
	}

	void readInData(){
		readInBuffer = alt_read_word(IDATA_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST);
		bitId = readInBuffer >> 23;
		repeatCount = readInBuffer & 0x007FFFFF;
		for(i = outputBaseCounter; i < outputBaseCounter + repeatCount; i++) {
			decompressedPic[i] = bitId;
			if(i==(240*320-1)){
				alt_write_byte(FIFO_OUT_READ_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST, 0);
			}
		}
		outputBaseCounter += repeatCount;
		readCount++;
	}

	int getCompressionRatio() {
		return (240*320)/(readCount*24);
	}

	void processData() {
		writeCount = 0;
		readCount = 0;
		outputBaseCounter = 0;
		alt_write_byte(FIFO_IN_WRITE_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST, 0x01);
		alt_write_byte(FIFO_OUT_READ_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST, 0x01);
		while(alt_read_byte(FIFO_IN_WRITE_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST) || alt_read_byte(FIFO_OUT_READ_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST)) {
			printf("NOT FIFO_IN_FULL_PIO_BASE = %d\n", !alt_read_byte(FIFO_IN_FULL_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST));
			printf("FIFO_IN_WRITE_REQ_PIO_BASE = %d\n", alt_read_byte(FIFO_IN_WRITE_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST));
			if(alt_read_byte(FIFO_IN_WRITE_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST) && !alt_read_byte(FIFO_IN_FULL_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST))
			{
				writeOutData();
				printf("writing\n");
			}
			printf("RESULT_READY_PIO_BASE = %d\n", !alt_read_byte(RESULT_READY_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST));
			printf("FIFO_OUT_READ_REQ_PIO_BASE = %d\n", alt_read_byte(FIFO_OUT_READ_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST));
			if(alt_read_byte(FIFO_OUT_READ_REQ_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST) && !alt_read_byte(RESULT_READY_PIO_BASE+ALT_FPGA_BRIDGE_LWH2F_OFST))
			{
				readInData();
				printf("reading\n");
			}
		}
	}
	//****************************************************************
	//END WIP END WIP END WIP END WIP END WIP END WIP END WIP END WIP
	//****************************************************************

	//waits for keypress and release, then records keypress to global var
	void waitForContinue(){
		while(1) {
			if(*KEY_ptr!=0){
				keyPressed = *KEY_ptr;
				while(*KEY_ptr!=0) {
					if(*KEY_ptr==0) {
						break;
					}
				}
					break;
			}
		}
	}

	//perma loop
	void mainLoop(){
		while(1){
			//at beginning of execution, enable video
			enableVideo();
			printf("video enabled\n");
			waitForContinue();
			takeStandardPicture();
			printf("took a pic, press to continue\n");
			waitForContinue();
			preProcessImageAndDisplay();
			printf("processed the pic, press to continue\n");
			waitForContinue();
			writeImgToSdram();
			printf("pic is in SDRAM?, press to continue\n");
			waitForContinue();
			printf("Starting processing\n");
			processData();
			printf("Result ready, press to continue\n");
			waitForContinue();
			displayBinaryImage(decompressedPic);
			printf("compression ratio %d\n", getCompressionRatio());
			waitForContinue();
		}
	}

		mainLoop();
} //end main
