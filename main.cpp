/* Program to run the heartbeat sensor project
 * for Electronic Design Project 2
 * James Campbell GUID 2398110C
 */

#include "mbed.h"

// setting up io other than the display
AnalogIn sensorin(PTB0);
DigitalIn modeSwitch(PTD5);
DigitalOut led1(LED1);
DigitalOut led2(LED2);
// ticker to do timed interrupts for each mode
Ticker timerint;
Ticker bpmdisplayticker;
// timer for timing for the bpm
Timer bpmtimer;
// serial link to pc for debugging
Serial pc(PTA2, PTA1);

// numerical variables
double y;
double runhistory[32];
double bpmrunhistory[128];
int hiscounter;
int bpmhiscounter;
double maxvalue;
double minvalue;
int displarray[8];
double newfloat;
int newpos;
int tickersetting;
int peak;
double times[4];
int timescounter;
double lastime;
double upperbound;
double lowerbound;


// 8x8 matrix display for final product bits and bobs
#define max7219_reg_noop         0x00
#define max7219_reg_digit0       0x01
#define max7219_reg_digit1       0x02
#define max7219_reg_digit2       0x03
#define max7219_reg_digit3       0x04
#define max7219_reg_digit4       0x05
#define max7219_reg_digit5       0x06
#define max7219_reg_digit6       0x07
#define max7219_reg_digit7       0x08
#define max7219_reg_decodeMode   0x09
#define max7219_reg_intensity    0x0a
#define max7219_reg_scanLimit    0x0b
#define max7219_reg_shutdown     0x0c
#define max7219_reg_displayTest  0x0f

#define LOW 0
#define HIGH 1

// pins for the spi link to the max chip
SPI max72_spi(PTD2, NC, PTD1);
DigitalOut load(PTD0); //will provide the load signal

// array to hold the possible places for a dot, for the output trace
char  dots[8] = { 0x01, 0x2,0x4,0x08,0x10,0x20,0x40,0x80};

// arrray hold numbers to display the bpm mode
char numbers[10][3] = { {0x1f, 0x11, 0x1f}, {0x00,0x00,0x1f}, {0x17,0x15,0x1d}, {0x15,0x15,0x1f}, {0x1c, 0x04, 0x1f}, {0x1d, 0x15,0x17}, {0x1f, 0x15, 0x17}, {0x10, 0x17, 0x18}, {0x1f, 0x15, 0x1f}, {0x1d, 0x15, 0x1f} };

// helper functions for the 8x8 display

// write via spi, args register and the column data
void write_to_max( int reg, int col)
{
    load = LOW;            // begin
    max72_spi.write(reg);  // specify register
    max72_spi.write(col);  // put data
    load = HIGH;           // make sure data is loaded (on rising edge of LOAD/CS)
}

//writes 8 bytes to the display  
void pattern_to_display(char *testdata){
    int cdata; 
    for(int idx = 0; idx <= 7; idx++) {
        cdata = testdata[idx]; 
        write_to_max(idx+1,cdata);
    }
} 
 
void setup_dot_matrix ()
{
    // initiation of the max 7219
    // SPI setup: 8 bits, mode 0
    max72_spi.format(8, 0);
    max72_spi.frequency(100000); //down to 100khx easier to scope ;-)
    write_to_max(max7219_reg_scanLimit, 0x07);
    write_to_max(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
    write_to_max(max7219_reg_shutdown, 0x01);    // not in shutdown mode
    write_to_max(max7219_reg_displayTest, 0x00); // no display test
    for (int e=1; e<=8; e++) {    // empty registers, turn all LEDs off
        write_to_max(e,0);
    }
   // maxAll(max7219_reg_intensity, 0x0f & 0x0f);    // the first 0x0f is the value you can set
     write_to_max(max7219_reg_intensity,  0x08);     
 
}

void clear(){
     for (int e=1; e<=8; e++) {    // empty registers, turn all LEDs off
        write_to_max(e,0);
    }
}

// waveform display mode
void wave() {
    // digital filtering was removed since due to the low polling rate, it wasn't helpful
    y=sensorin;
    // storing the last 32 values (4 seconds) to sort drift
    runhistory[hiscounter] = y;
    hiscounter++;
    if (hiscounter >= 32) {
        hiscounter = 0;
    }

    // work out the maximum and minimum values in recent history in order to scale the  trace
    maxvalue = runhistory[0];
    minvalue = runhistory[0];
    for (int i = 1; i< 31; i++) {
        if (runhistory[i] > maxvalue){
            maxvalue = runhistory[i];
        } else if (runhistory[i] < minvalue){
            minvalue = runhistory[i];
        }
    }

    // work out the place on the display and store it in the displarray

    // first, make room at the start of the array for the new value by traversing the array backwards
    for (int i = 7; i > 0; i--){
        displarray[i] = displarray[i-1];
    }
    // now decide on what the new value will be and place it
    // find a float value for the proportion that the current voltage is between max and min
    newfloat = (y-minvalue)/(maxvalue-minvalue);
    // multiply it by 8 to give the 0 to 7 value and cast to int
    newpos = newfloat*8;
    // for newfloat = max, newpos = 8, which would be out of bounds for 0 to 7 so make it 7
    if (newpos == 8){
        newpos = 7;
    }
    // finally, add it to the array
    displarray[0] = newpos;

    // construct the image to send to the display using the displarray
    char pattern[8];
    for (int i = 0; i < 8; i++ ){
        pattern[i] = dots[displarray[7-i]];
    }
    // update the display
    pattern_to_display(pattern);
}

// bpm counting mode
void bpm(){
    
    // digital filtering may need to be added, but we will see
    y=sensorin;
    // storing the last 128 values (4 seconds) to sort drift
    bpmrunhistory[hiscounter] = y;
    hiscounter++;
    if (hiscounter >= 128) {
        hiscounter = 0;
    }

    // work out the maximum and minimum values in recent history 
    maxvalue = bpmrunhistory[0];
    minvalue = bpmrunhistory[0];
    for (int i = 1; i< 128; i++) {
        if (bpmrunhistory[i] > maxvalue){
            maxvalue = bpmrunhistory[i];
        } else if (bpmrunhistory[i] < minvalue){
            minvalue = bpmrunhistory[i];
        }
    }
    upperbound = (0.6*(maxvalue-minvalue))+minvalue;
    lowerbound = (0.4*(maxvalue-minvalue))+minvalue;

    // detect a peak and if one is detected then read the time and shoogle the array
    if (peak == 1 && y < lowerbound){
        // if there was previously a high value and now it's going low
        // come off peak
        peak = 0;
    } else if (peak == 0 && y > upperbound){
        // if there was previously a low value and now it's going high
        // go on peak
        peak = 1;
        // record the time
        times[timescounter] = bpmtimer.read() - lastime;
        lastime = lastime + times[timescounter];
        timescounter ++;
        if (timescounter >=4 ){
            timescounter = 0;
        }
    }

}
void bpmdisplay() {
    // calculate a bpm from the times array
    
    double bpmperiod = 0;
    for (int i = 0; i<4; i++){
        bpmperiod = bpmperiod + times[i];
    }
    bpmperiod = bpmperiod/4;
    bpmperiod = 1/bpmperiod;
    int bpm = bpmperiod *60;
    // now display the calculated bpm
    char pattern[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    int working = bpm % 10;
    for (int i = 0; i < 3; i++) {
        pattern[2-i] = numbers[working][i];
    }
    working = (bpm/10)%10;
    for (int i = 0; i<3; i++){
        pattern[6-i] = numbers[working][i];
    }
    pc.printf("bpm %d", bpm);
    
    pattern_to_display(pattern);

}


int main()
{
    // set up the matrix display
    setup_dot_matrix();
    // set up the initial values of some variables
    y = 0.0f;
    hiscounter = 0;
    newfloat = 0.0f;
    newpos = 0;
    led1=1;
    led2 = 1;
    peak = 0;
    // start the timer
    bpmtimer.start();
    // set up the serial link
    pc.baud(9600);
    pc.printf("Hello \n");
    // deciding which mode to start in based on the switch
    if (modeSwitch == 0 ){
            // set the ticker to call the wave interrupt at 8hz
            timerint.attach_us(&wave, 125000);
            tickersetting = 0;
    } else {
            // set the ticker to call the bpm interrupt at 
            lastime = bpmtimer.read();
            timerint.attach_us(&bpm, 31250);
            tickersetting = 1;
            bpmdisplayticker.attach(&bpmdisplay, 1);
    }

    while (1) {

    	if ( modeSwitch == 1 && tickersetting == 0){
            timerint.detach();
            lastime = bpmtimer.read();
            timerint.attach_us(&bpm, 31250);
            bpmdisplayticker.attach(&bpmdisplay, 1);
            tickersetting = 1;
        }
        if ( modeSwitch == 0 && tickersetting == 1){
            timerint.detach();
            bpmdisplayticker.detach();
            timerint.attach_us(&wave, 125000);
            tickersetting = 0;
        }
        wait(0.1);

    }
}
