/* logicAnalyzer.c
 *Author: Stephanie Salazar
 *Description: This logic analyzer can take in up to 8 channels. It utilizes the PSoC-5 and Raspberry Pi to
 * view a signal's binary states. It takes in user inputs such as the number of channels the user
 * wants to use. This can be between 1-8. A trigger condition can then be entered. For this
 * data analyzer, it takes in a binary number and triggers at this state. The trigger condition
 * can be positive or negative where positive direction specifies that the trigger should become
 * active when the trigger condition changes from false to true, and negative direction specifies
 * that the trigger should become active when the condition changes from true to false. A
 * memory depth can be entered, so that the size of the window can be set. The sampling
 * frequency can be taken in which maxes out at the max clock speed. The x-scale can be set
 * as 1, 10, 100, 500, 1000, 2000, 5000 or 10000, and 'run' is entered to begin the logic analyzer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <wiringPi.h>
#include <errno.h>

#define BAUDRATE B115200 // UART speed
#define SCALE xscale
#define WAVEH (height/10)

typedef enum{
	POS = 0,
	NEG 
}direction;

int i,j;
uint8_t data[2] = {0};
uint8_t read_bytes = 0;

int main() {
	
//--------------------------------------------------------------------------------------
//Sets up UART channel
	struct termios serial; // Structure to contain UART parameters

    char* dev_id = "/dev/serial0"; // UART device identifier
	
	printf("Opening %s\n", dev_id);
    int fd = open(dev_id, O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (fd == -1){ // Open failed
		perror(dev_id);
		return -1;
    }

    // Get UART configuration
    if (tcgetattr(fd, &serial) < 0){
		perror("Getting configuration");
		return -1;
    }

    // Set UART parameters in the termios structure
    serial.c_iflag = 0;
    serial.c_oflag = 0;
    serial.c_lflag = 0;
    serial.c_cflag = BAUDRATE | CS8 | CREAD | PARENB | PARODD;
    // Speed setting + 8-bit data + Enable RX + Enable Parity + Odd Parity
  
    serial.c_cc[VMIN] = 0; // 0 for Nonblocking mode
    serial.c_cc[VTIME] = 0; // 0 for Nonblocking mode
  
    // Set the parameters by writing the configuration
    tcsetattr(fd, TCSANOW, &serial);


//Inputs-------------------------------------------------------------------


	
	//Prompt for number of channels
	char nChannels[20];
	int numChannels;
	printf("Number of channels desired : ");
	while(1) {
		fgets(nChannels, 20, stdin);
		numChannels = atof(nChannels);
		if ((numChannels >= 1) && (numChannels <= 8)) {
			break;
		}
		printf("Number of channels desired : ");
	}
	
	//Prompt for the trigger condition
	char trigger_cond[50];
	int trigger;
	printf("Enter a trigger condition: \n");
	
	//Prompt for trigger direction
	char trigger_dir[20];
	int trig_direction;
	printf("Enter a trigger direction (p/n): ");
	while(1){
		fgets(trigger_dir, 20, stdin);
		if(strcmp(trigger_dir, "p\n") == 0) {
			trig_direction = POS;
			break;
		} else if (strcmp(trigger_dir, "n\n") == 0) {
			trig_direction = NEG;
			break;
		}
		printf("Enter a trigger direction (p/n): ");
	}
	
	//Prompt for the memory depth
	char mem_depth[20];
	int sample_count;
	printf("Enter a sample count: ");
	while(1){
		fgets(mem_depth, 20, stdin);
		sample_count = atof(mem_depth);
		if ((sample_count>200) && (sample_count<=5000)) {
			break;
		}
		printf("Enter a sample count: ");
	}
	
	// Prompt for frequency
	char freq[20];
	int frequency;
	printf("Enter a sampling frequency (1-max): ");
	while(1){
		fgets(freq,20,stdin);
		frequency = atof(freq);
		if(frequency>=1 && frequency<10){
			break;
		}
		printf("Enter a sampling frequency (1-max): ");
	}
	
	//Prompt for xscale
	char hscale[20];
	int xscale;
	printf("Set the xscale(10, 100, 500, 1000, 2000, 5000, 10000): ");
	while(1){
		fgets(hscale, 20, stdin);
		xscale = atof(hscale);
		if ((xscale==10) || (xscale==100) || (xscale == 500) || (xscale == 1000) || (xscale == 2000) || (xscale == 5000) || (xscale = 10000)){
			break;
		}
		printf("Set the xscale(10, 100, 500, 1000, 2000, 5000, 10000): ");
	}

	
	//Prompt to begin program
	char start[20];
	printf("Enter 'run' to start logic analyzer: ");
	while(1){
		fgets(start, 20, stdin);
		if(strcmp(start, "run\n") == 0) {
			break;
		}
		printf("Enter 'run': ");
	}

//------------------------------------------------------------------------------
	int width, height;
	init(&width, &height);
	
	int x =
	 (width*500)/(xscale*105);
	uint8_t display[width];
	uint8_t samples[sample_count*2];
	uint8_t bits[sample_count * x];
	float move[width];
	float CH0[width];
	float CH1[width];
	float CH2[width];
	float CH3[width];
	float CH4[width];
	float CH5[width];
	float CH6[width];
	float CH7[width];

	
	
	for(i = 0; i < sample_count; i++){
		samples[i] = i;
	}
	for(i = 0; i < sample_count; i++){
		for(j = 0; j < x; j++){
			bits[(i * x) + j] = samples[i];
		}
	}
	for(i = 0; i < width; i++){
		move[i] = i;
	}
	
	while (1) {
		Start(width, height);
		Background(0,0,0);
		
		/*
		uint8_t tx = 0x11;
		int wcount = 0;
		wcount = write(fd, &tx, 1);
		if (wcount < 0 && !(errno == EAGAIN)){
			perror("Write");
			return -1;
		}
		// Read new byte
		int rcount = 0;
		while(rcount < 3){
			int read_bytes = read(fd, &data[rcount], sizeof(3)-rcount);
			if(read_bytes<0){
				perror("Read");
				return -1;
			}	
		}
		rcount += read_bytes; */
		
		//Draw y grid
		int i;
		float tenth = width / 10;
		float xPos = tenth;
		Stroke(200, 200, 200, 1);
		StrokeWidth(1);	
		Line(0, 0, 0, height);
		for(i = 0; i<10; i++){
			Line((int)xPos, 0, (int)xPos, height);
			xPos +=tenth;
		}
		
		for (i = 0; i < width; i++) {
			display[i] = bits[(((sample_count*x)/2) - (width/2)) + i];
		}
		for(i = 0; i < width; i++){
			CH0[i] = (display[i] & 0b00000001) * WAVEH;
			CH1[i] = (((display[i] & 0b00000010)>>1) * WAVEH) + height/8;
			CH2[i] = (((display[i] & 0b00000100)>>2) * WAVEH) + height/4;
			CH3[i] = (((display[i] & 0b00001000)>>3) * WAVEH) + height*3/8;
			CH4[i] = (((display[i] & 0b00010000)>>4) * WAVEH) + height/2;
			CH5[i] = (((display[i] & 0b00100000)>>5) * WAVEH) + height*5/8;
			CH6[i] = (((display[i] & 0b01000000)>>6) * WAVEH) + height*6/8;
			CH7[i] = (((display[i] & 0b10000000)>>7) * WAVEH) + height*7/8;
		}
		
	
		StrokeWidth(1);
		Stroke(200, 200, 200, 1);

		

		StrokeWidth(2);
		Stroke(255, 0, 0, 1);
		Polyline(move, CH0, width);
		if(numChannels > 1){
			Stroke(255, 120, 0, 1);
			Polyline(move, CH1, width);
		}
		if(numChannels > 2){
			Stroke(255, 255, 0, 1);
			Polyline(move, CH2, width);
		}
		if(numChannels > 3){
			Stroke(0, 204, 0, 1);
			Polyline(move, CH3, width);
		}
		if(numChannels > 4){
		Stroke(0, 204, 204, 1);
		Polyline(move, CH4, width);
		}
		if(numChannels > 5){
		Stroke(0, 76, 156, 1);
		Polyline(move, CH5, width);
		}
		if(numChannels > 6){
		Stroke(102, 0, 204, 1);
		Polyline(move, CH6, width);
		}
		if(numChannels > 7){
		Stroke(255, 0, 127, 1);
		Polyline(move, CH7, width);	
		}	
		
		// Display text on screen
		Fill(255, 255, 255, 1);
		TextMid(width-(width*9/10),height-(height/30), "Number of Channels: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30), nChannels, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-25, "Trigger Condition ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-25, trigger_cond, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-50, "Trigger direction: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-50, trigger_dir, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-75, "Memory Depth: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-75, mem_depth, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30) - 100, "Sample frequency: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-100, freq, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-125, "X scale: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-125, hscale, SerifTypeface, 15);
		
		TextMid(width-(width/20), (height-height/50), "Channel 7", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height/8), "Channel 6", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height*2/8), "Channel 5", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height*3/8), "Channel 4", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height*4/8), "Channel 3", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height*5/8), "Channel 2", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height*6/8), "Channel 1", SerifTypeface, 15);
		TextMid(width-(width/20), (height-height/50) - (height*7/8), "Channel 0", SerifTypeface, 15);
		
		End();
		WindowClear();
	}
    
    close(fd);
    exit(0);
}
	
