/*oscilloscope.c
 * Author: Stephanie Salazar
 * Description: The oscilloscope is dual channel, meaning it can display two separate waveforms. It is
 * programmed with a PSoC-5 and Raspberry Pi 3. The program takes in user inputs to set
 * certain features of the oscilloscope. The number of channels can be set to 1 or 2, which
 * allows the user to view 1 or 2 waves. There are two modes- free running and trigger. If
 * trigger is selected, the trigger slope and trigger channel must be selected. This will indicate
 * whether the wave should start at a downward or upward slope depending on either the 1st
 * or 2nd channel. A y-scale can be set to 0.5, 1, 1.5 or 2 which defines the vertical scale of
 * the waveform display in volts per division. An x-scale can be selected from the values 1, 10,
 * 100, 500, 1000, 2000, 5000 or 10000 to define the horizontal scale of the waveform display in
 * microseconds. The oscilloscope starts when the user inputs 'start.'
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include <wiringPi.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>


#define BAUDRATE B115200 // UART speed

typedef struct{
	int nchannels;
	int mode;
	float level;
	int slope;
	int trigger_channel;
	float yscale;
	float xscale;
	int start;
}settings;

typedef struct{
	char nchannels[100];
	char mode[100];
	char level[100];
	char slope[100];
	char trigger_channel[100];
	char yscale[100];
	char xscale[100];
}output;

typedef enum{
	freerun = 0,
	trigger,
	positive,
	negative
}type;

settings input;
output printOut;
	
uint8_t data[3] = {0};
uint8_t read_bytes = 0;
uint8_t old_data[2] = {0};
float waveSpacing;
float move;
float old_move;
int channel1;
int rcount;


int main(){
	
	
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
	
	
	// GET USER INPUT ------------------------------------------------------------------;
	// when equal to 1 or 2, strcmp returns 0 -- ending the while loop
	// GET nchannels
	while (strcmp(printOut.nchannels, "1\n") && strcmp(printOut.nchannels, "2\n")){
		 printf("Set nchannels: ");
		 fgets(printOut.nchannels, 100, stdin); // read from standard input up to 100 chars
	 }
	 if(strcmp(printOut.nchannels, "1\n") == 0){
		 input.nchannels = 1;
	 }else{
		 input.nchannels = 2;
	 }
	 
	 
	 // GET mode
	 while (strcmp(printOut.mode, "f\n") && strcmp(printOut.mode, "t\n")){
		 printf("Set mode (free_run or trigger) f/t: ");
		 fgets(printOut.mode, 100, stdin); // read from standard input up to 100 chars
	 }
	 if(strcmp(printOut.mode, "f\n")==0){
		 input.mode = freerun;
		 printf("Entered free run mode");
	 }else{
		 input.mode = trigger;
		 printf("Entered trigger mode");
	 }
	 
	 if(input.mode == trigger){
		// GET level
		while (1){
			printf("Set trigger level, 0V to 5V in 0.1 increments: ");
			fgets(printOut.level, 100, stdin); // read from standard input up to 100 chars
			float trigger = atof(printOut.level);
			input.level = trigger * 51;
			int temp = trigger * 100;
			if(temp % 10 == 0.0 && temp <= 500){
				break;
			}
		}
		printf("YOU GOT %f \n", input.level);
	 
	 	 
		// GET slope
		while (strcmp(printOut.slope, "p\n") && strcmp(printOut.slope, "n\n")){
			printf("Set trigger slope, positive or negative (p/n): ");
			fgets(printOut.slope, 100, stdin); // read from standard input up to 100 chars
		}
		if(strcmp(printOut.slope, "p\n") == 0){
			input.slope = positive;
		}else{
			input.slope = negative;
		}
	
	
		// GET trigger channel
		while (strcmp(printOut.trigger_channel, "1\n") && strcmp(printOut.trigger_channel, "2\n")){
			printf("Set trigger channel (1/2): ");
			fgets(printOut.trigger_channel, 100, stdin); // read from standard input up to 100 chars
		}
		if(strcmp(printOut.trigger_channel, "1\n") == 0){
			input.trigger_channel = 1;
		}else{
			input.trigger_channel = 2;
		}
	}
	 
	 // Set y scale

	 while (strcmp(printOut.yscale, "0.5\n") && strcmp(printOut.yscale, "1\n") && strcmp(printOut.yscale, "1.5\n") && strcmp(printOut.yscale, "2\n")){
		 printf("Set yscale (0.5, 1, 1.5, 2): ");
		 fgets(printOut.yscale, 100, stdin); // read from standard input up to 100 chars
	 }
	 if(strcmp(printOut.yscale, "0.5\n") == 0){
		 input.yscale = 0.5;
	 }else if(strcmp(printOut.yscale, "1\n") == 0){
		 input.yscale = 1;
	 }else if(strcmp(printOut.yscale, "1.5\n") == 0){
		 input.yscale = 1.5;
	 }else if(strcmp(printOut.yscale, "2\n") == 0){
		 input.yscale = 2;
	 }
	 
	 
	 	 
	 // Set x scale
	 while (strcmp(printOut.xscale, "1\n") && strcmp(printOut.xscale, "10\n") && strcmp(printOut.xscale, "100\n") && strcmp(printOut.xscale, "500\n") 
			&&strcmp(printOut.xscale, "1000\n") && strcmp(printOut.xscale, "2000\n") && strcmp(printOut.xscale, "5000\n") && strcmp(printOut.xscale, "10000\n")){
		 printf("Set xscale (1, 10, 100, 500, 1000, 2000, 5000, 10000): ");
		 fgets(printOut.xscale, 100, stdin); // read from standard input up to 100 chars
	 }
	 input.xscale = atof(printOut.xscale);

	 
	 char begin[100];
	 // Wait for START to begin oscilloscope
	 while (strcmp(begin, "start\n")){
		 printf("Enter 'start' to start oscilloscope :");
		 fgets(begin, 100, stdin); // read from standard input up to 100 chars
	 }
	 
	 
	
	// END OF USER INPUT -------------------------------------------------------------------
	
	 // Now transmit the string
	 uint8_t tx = 0xFF;
     int wcount = write(fd, &tx, 1);
     if (wcount < 0 && !(errno == EAGAIN)){
       perror("Write");
       return -1;
     }
     printf("Transmit \n");
	 int width, height;

	 init(&width, &height);					// Graphics initialization


	for(;;){
		Start(width, height);					// Start the picture
		Background(0, 0, 0);					// Black background
	
		Fill(0, 0, 0, 0);
		Rect(0, 0, width, height);				//grid
	
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
	
		//Draw y ticks
		float tixSpacing = tenth/5;
		float tix = 0;
		Stroke(200, 200, 200, 1);
		StrokeWidth(1);	
		//Line(0, 0, 0, height);
		for(i = 0; i<50; i++){
			Line((int)tix, (height/2)-2, (int)tix, (height/2)+2);
			tix +=tixSpacing;
		}
	
		//Draw x grid
		float eighth = height / 8;
		float yPos = eighth;
		Stroke(200, 200, 200, 1);
		StrokeWidth(1);	
		Line(0, 0, width,0);
		for(i = 0; i<10; i++){
			Line(0, yPos, width, yPos);
			yPos +=eighth;
		}
	
	
		//Draw x ticks
		tixSpacing = eighth/5;
		tix = 0;
		Stroke(200, 200, 200, 1);
		StrokeWidth(1);	
		//Line(0, 0, 0, height);
		for(i = 0; i<50; i++){
			Line((width/2)-2, (int)tix, (width/2)+2, (int)tix);
			tix +=tixSpacing;
		}
	
	
		
		// Check if freerunning/trigger for channel 1
		if((input.mode == trigger) && input.trigger_channel == 1){
			if(input.slope == positive){ //positive slope
				printf("POS\n");
				while(old_data[0]>data[0] || ((data[0] > input.level+2 ) || (data[0] < input.level -2))){
					printf("inside 1\n");
					old_data[0] = data[0];
					tx = 0x11;
					wcount = 0;
					wcount = write(fd, &tx, 1);
					if (wcount < 0 && !(errno == EAGAIN)){
						perror("Write");
						return -1;
					}
					old_data[0] = data[0];
					old_data[1] = data[1];
					// Read new byte
					int rcount = 0;
					while(rcount < 3){

						int read_bytes = read(fd, &data[rcount], sizeof(3)-rcount);
						if(read_bytes<0){
							perror("Read");
							return -1;
						}
						rcount += read_bytes;
					}
					printf("%d\n", data[0]==data[1]);
				}
			}
			if(input.slope == negative){ //negative slope
				printf("NEG\n");
				while(old_data[0]<data[0] || ((data[0] > input.level+2 ) || (data[0] < input.level - 2))){
					printf("inside 2\n");
					old_data[0] = data[0];
					tx = 0x11;
					wcount = 0;
					wcount = write(fd, &tx, 1);
					if (wcount < 0 && !(errno == EAGAIN)){
						perror("Write");
						return -1;
					}
					old_data[0] = data[0];
					old_data[1] = data[1];
					// Read new byte
					int rcount = 0;
					while(rcount < 3){
						int read_bytes = read(fd, &data[rcount], sizeof(3)-rcount);
						if(read_bytes<0){
							perror("Read");
							return -1;
						}
						rcount += read_bytes;
					}
				}
			}
		}
		
		// Check if freerunning/trigger for channel 2
		if((input.mode == trigger) && input.trigger_channel == 2){
			if(input.slope == positive){ //positive slope
				printf("POS\n");
				while(old_data[1]>data[1] || ((data[1] > input.level+2 ) || (data[1] < input.level -2))){
					printf("inside 3\n");
					old_data[1] = data[1];
					tx = 0x11;
					wcount = 0;
					wcount = write(fd, &tx, 1);
					if (wcount < 0 && !(errno == EAGAIN)){
						perror("Write");
						return -1;
					}
					old_data[0] = data[0];
					old_data[1] = data[1];
					// Read new byte
					int rcount = 0;
					while(rcount < 3){
						int read_bytes = read(fd, &data[rcount], sizeof(3)-rcount);
						if(read_bytes<0){
							perror("Read");
							return -1;
						}
						rcount += read_bytes;
					}
				}
			}
			if(input.slope == negative){ //negative slope
				printf("NEG\n");
				while(old_data[1]<data[1] || ((data[1] > input.level+2 ) || (data[1] < input.level - 2))){
					printf("inside 4\n");
					old_data[1] = data[1];
					tx = 0x11;
					wcount = 0;
					wcount = write(fd, &tx, 1);
					if (wcount < 0 && !(errno == EAGAIN)){
						perror("Write");
						return -1;
					}
					old_data[0] = data[0];
					old_data[1] = data[1];
					// Read new byte
					int rcount = 0;
					while(rcount < 3){
						int read_bytes = read(fd, &data[rcount], sizeof(3)-rcount);
						if(read_bytes<0){
							perror("Read");
							return -1;
						}
						rcount += read_bytes;
					}
				}
			}
		}
		
		//Draw wave
		float space = ((float)width/210)*(2000/input.xscale);
		float waveSpacing = space;	
		printf(" %f and %f \n", space, input.yscale);
		move = waveSpacing;
		old_move = 0;
		channel1 = height/2;
		old_data[0] = data[0];
		old_data[1] = data[1];
		Stroke(255, 0, 200, 1);
		StrokeWidth(4);	
		while(move < width){
			tx = 0x11;
			wcount = 0;
			wcount = write(fd, &tx, 1);
			if (wcount < 0 && !(errno == EAGAIN)){
					perror("Write");
					return -1;
			}
			rcount = 0;
			while(rcount < 3){
				read_bytes = read(fd, &data[rcount], 3-rcount);
				if(read_bytes<0){  
					perror("Read");
					return -1;
				}
				rcount += read_bytes;
			}
			Stroke(255, 0, 200, 1);
			Line((int)old_move, ((old_data[0])/input.yscale)+channel1, (int)move, ((data[0])/input.yscale)+channel1);
			if(input.nchannels == 2){
				uint8_t pot = data[2]*height / 255;
				//draw second wave		
				Stroke(0, 180, 200, 1);
				Line((int)old_move, ((old_data[1])/input.yscale)+pot, (int)move, ((data[1])/input.yscale)+pot);
			}
			old_data[0] = data[0];     
			old_data[1] = data[1];                                                            
			old_move = move;
			move = move + waveSpacing;
		} 
		
		// Display text on screen
		Fill(255, 255, 255, 1);
		TextMid(width-(width*9/10),height-(height/30), "Number of Channels: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30), printOut.nchannels, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-25, "xscale: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-25, printOut.xscale, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-50, "yscale: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-50, printOut.yscale, SerifTypeface, 15);
		TextMid(width-(width*9/10),height-(height/30)-75, "Mode: ", SerifTypeface, 15);
		TextMid((width-(width*9/10))+200,height-(height/30)-75, printOut.mode, SerifTypeface, 15);
		
		if(input.mode == trigger){
			TextMid(width-(width*9/10),height-(height/30) - 100, "Trigger Level: ", SerifTypeface, 15);
			TextMid((width-(width*9/10))+200,height-(height/30)-100, printOut.level, SerifTypeface, 15);
			TextMid(width-(width*9/10),height-(height/30)-125, "Trigger Slope: ", SerifTypeface, 15);
			TextMid((width-(width*9/10))+200,height-(height/30)-125, printOut.slope, SerifTypeface, 15);
			TextMid(width-(width*9/10),height-(height/30)-150, "Trigger Channel: ", SerifTypeface, 15);
			TextMid((width-(width*9/10))+200,height-(height/30)-150, printOut.trigger_channel, SerifTypeface, 15);
		}
		
	
		End();						   	    // End the picture
		WindowClear();
		
	}

	finish();					        // Graphics cleanup
	close(fd);
	exit(0);
}
