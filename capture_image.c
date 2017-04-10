#include "hps_soc_system.h"
#include "socal.h"
#include "hps.h"
#include <stdio.h>

#define KEY_BASE              0xFF200050
#define VIDEO_IN_BASE         0xFF203060
#define FPGA_ONCHIP_BASE      0xC8000000

//function signature
void VGA_text (int, int, char *);

//main
int main(void)
{
	volatile int * KEY_ptr				   						= (int *) KEY_BASE;
	volatile int * Video_In_DMA_ptr	 						= (int *) VIDEO_IN_BASE;
	volatile short * Video_Mem_ptr	 						= (short *) FPGA_ONCHIP_BASE;
	//new
	volatile int * SDRAM_ptr         						= (int *) SDRAM_BASE;
	volatile int * READ_REQ_ptr									= (int *) FIFO_OUT_READ_REQ_PIO_BASE;
	volatile int * WRITE_REQ_ptr							 	= (int *) FIFO_IN_WRITE_REQ_PIO_BASE;
	volatile int * FIFO_IN_FULL_ptr 						= (int *) FIFO_IN_FULL_PIO_BASE;
	volatile int * ODATA_ptr         						= (int *) ODATA_PIO_BASE;
	volatile int * IDATA_ptr         						= (int *) IDATA_PIO_BASE;
	volatile int * RLE_RESET_ptr     						= (int *) RLE_RESET_BASE;
	volatile int * RLE_FLUSH_ptr     						= (int *) RLE_FLUSH_PIO_BASE;
	volatile int * RESULT_READY_ptr 						= (int *) RESULT_READY_PIO_BASE;


	//iterators for image loops
	int x, y;
	//records pressed key
	int keyPressed;
	//temp array to store picture in for transforms
	short pictureArray[320][240];
	//temp compressedPic
	int preProcessedPic[320*240]; //76800

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
				preProcessedPic[240*y+x] = pictureArray[x][y];
			}
		}
	}

	void displayPreProcessedImage() {
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				if(preProcessedPic[240*y+x]==1)
					*(Video_Mem_ptr + (y << 9) + x)=0xFFFFFFFF;
				else
					*(Video_Mem_ptr + (y << 9) + x)=0;
			}
		}
	}

	int roundPixel(int pixel){
		return (pixel>=0xFFFFFFFF/0x2);
	}

	void createAndDisplayBlackAndWhitePicture(){
		for (y = 0; y < 240; y++) {
			for (x = 0; x < 320; x++) {
				pictureArray[x][y] = roundPixel(*(Video_Mem_ptr + (y << 9) + x));
				// printf("%#04x\n", *(Video_Mem_ptr + (y << 9) + x));
				// printf("%d\n", pictureArray[x][y]);
			}
		}
		createPreProcessedImage();
		displayPreProcessedImage();
	}




	//takes compressed info and creates readable image from it
	void decompressImage(){
		//write fancy algorithm here
	}

	// //send data
	// while(!*FIFO_IN_FULL_ptr){
	// 	alt_write_byte(*ODATA_ptr, data);
	// }
	//
	// //receive data
	// if(*RESULT_READY_ptr){
	// 	data = alt_read_word(dataout_pio_base);
	// }


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
			waitForContinue();
			takeStandardPicture();
			waitForContinue();
			createAndDisplayBlackAndWhitePicture();
			waitForContinue();
		}
	}

		mainLoop();
} //end main
