/*
 * Copyright (c) 2012 by Cristian Maglie <c.maglie@arduino.cc>
 * Audio library for Arduino Due.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

//#define DEBUG
 
#include "wwAudio.h"

void AudioClass::begin(uint32_t sampleRate, uint32_t msPreBuffer) {
	// Allocate a buffer to keep msPreBuffer milliseconds of audio
	// 1024 for 100 ms works
	// 2048 for 200 ms works with Woody
	bufferSize = msPreBuffer * sampleRate / 1000;
	if (bufferSize < 1024)      
		bufferSize = 1024;                 // the buffer should be at least 1024 long.
	buffer = (uint32_t *) malloc(bufferSize * sizeof(uint32_t));
	half = buffer + bufferSize / 2;
	last = buffer + bufferSize;

	// Buffering starts from the beginning
	running = buffer;
	next = buffer;

	// Start DAC
	dac->begin(VARIANT_MCK / sampleRate);
	dac->setOnTransmitEnd_CB(onTransmitEnd, this);  // seems to be the event handler if the end of the buffer is reached. -ra
}

void AudioClass::end() {
	dac->end();
	free( buffer);
}

void AudioClass::prepare(int16_t *buffer, int S, int volume){
    uint16_t *ubuffer = (uint16_t*) buffer;
	
    for (int i=0; i<S; i++) {
        // set volume amplitude (signed multiply)
        buffer[i] = buffer[i] * volume / 100;
        // convert from signed 16 bit to unsigned 12 bit for DAC.
        ubuffer[i] += 0x8000;
        ubuffer[i] >>= 4;
		//buffer[i] += 0x8000;
        //buffer[i] >>= 4;		
    }		
}

size_t AudioClass::write(const uint32_t *data, size_t size) {
	const uint32_t TAG = 0x10000000;
	#ifdef DEBUG	
		unsigned long time;  //ra	
	#endif
	int i;
	int run = 0;                             //will become 1 if we have written data -ra
	for (i = 0; i < size; i++) {           
		*next = data[i] | TAG;               // this TAG indicates if this data set should be played or skiped -Ra
		next++;

		if (next == half || next == last) {  // we will skip some of the data!
			//time = *next;
			//SerialUSB.println(time);
			enqueue();
			#ifdef DEBUG			
				SerialUSB.println(i);
				time = millis();		          //ra		
			#endif			
						
			while (next == running);      // Here we are waiting till one half of the buffer is used and ready to be filled again??? Ra
			#ifdef DEBUG
				time = millis() -time;		          //ra
				SerialUSB.print("nach Schleife ");
				SerialUSB.println(time);	      //ra
			#endif					
			run = 1;
		}
	}	
	#ifdef DEBUG	
		SerialUSB.println("   write");			
	#endif	
	if (run == 1) i=0;   // indicates that we have written some data into the buffer -ra
		
	return i;
}


void AudioClass::rst(void){
	// Buffering starts from the beginning
	running = buffer;
	next = buffer;
}


void AudioClass::enqueue() {	
	//unsigned long time;  //ra
	
	if (!dac->canQueue()) {                  // for security only - will not be called in general - ra
		// DMA queue full
		//SerialUSB.println("geht nicht");
		return;
	}

    //time = millis();		          //ra
	// SerialUSB.print("half");	      //ra	
	//SerialUSB.println(time);	      //ra	
	
	if (next == half) {
		// Enqueue the first half
		dac->queueBuffer(buffer, bufferSize / 2);	

	} else {
		// Enqueue the second half
		dac->queueBuffer(half, bufferSize / 2);
		next = buffer; // wrap around
		//SerialUSB.println(" else");     //ra
	}
}

void AudioClass::onTransmitEnd(void *_me) {
	AudioClass *me = reinterpret_cast<AudioClass *> (_me);
	if (me->running == me->buffer) {
		me->running = me->half;
		#ifdef DEBUG		
		SerialUSB.println("half");	      //ra
		#endif
	}
	else {
		me->running = me->buffer;
		#ifdef DEBUG
		SerialUSB.println("buffer");	      //ra
		#endif
	}
}

AudioClass Audio(DAC);
