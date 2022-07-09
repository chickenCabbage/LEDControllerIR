#include <IRremote.h>

#define debug true
 
#define IR_RECEIVE_PIN 2		//IR sensor pin
IRrecv IrReceiver(IR_RECEIVE_PIN);
decode_results results;

#define STATUS_LED 3
#define ERR_LED 13

#define MAX_OUT 255			//max analogWrite() value

#define STEP_SIZE 5			//step size for the up/down buttons
#define MIN_BRIGHTNESS 50	//lowest observable brightness, so as not to suddenly blind the user
#define STEP_TRANSITION 15	//the point at which the steps transition from STEP_SIZE to 1

#define PAUSE_DELAY 750		//the basic pause between command acceptance

long currCom = 0;			//current command
long lastCom = 0;			//last valid command

byte selectedChannel = 0;	//the channel to be acted upon
bool channelSelect = false;	//are we selecting a channel?
byte channelsAmnt;			//the amount of available channels according to channels[]

struct channel {
	byte pin;		//arduino pin
	int outValue;	//current output value
	int lastValue;	//last output value
	
	//special actions variables:
	int specialCode; //what action is performed
	bool fadeRising; //is the outValue rising or falling
	byte fadeDelay;	 //delay between each outValue change
};

channel channels[] = {
	{ 5,  MIN_BRIGHTNESS, MIN_BRIGHTNESS, 0, true, 10 },
	{ 9,  MIN_BRIGHTNESS, MIN_BRIGHTNESS, 0, true, 10 },
	{ 10, MIN_BRIGHTNESS, MIN_BRIGHTNESS, 0, true, 10 }
};

/* Mind the PWM timers, pins 3 and 11 are controlled by timer 2,
 * which is used by default by IRremote for ATmega328s.
 * Change the pin number, or change the timer in the libary
 * (IRremote/src/private/IRremoteBoardDefs.h)
 */

void setup() {
	OSCCAL = 0x39;
	channelsAmnt = sizeof(channels)/sizeof(channels[0]);
    pinMode(STATUS_LED, OUTPUT);
    pinMode(ERR_LED, OUTPUT);
    for(byte i=0; i<channelsAmnt; i++) {
    	pinMode(channels[i].pin, OUTPUT);
    }
    
    IrReceiver.enableIRIn();  //start the receiver
    IrReceiver.blink13(false); //enable feedback LED

    digitalWrite(ERR_LED, LOW);
    
    if(debug) {
    	Serial.begin(115200);
		while(!Serial);
    	Serial.println("Ready.");
    }
}
void commands() {
	//if there is a decoded result input:
	if(IrReceiver.decode(&results)) {
		if(debug) {
			Serial.print("0x"); //clarify the values are hex numbers
			Serial.println(results.value, HEX);
		}
		currCom = results.value;
		if(currCom == 0xFFFFFFFF) { //repeat
      		currCom = lastCom;
			if(debug) {
				Serial.print(F("Repeating 0x"));
      			Serial.println(currCom, HEX);
			}
		}

		bool delayAfter = false;
		if(selectedChannel == 0) { //if all channels are selected:
			for(byte i=0; i<channelsAmnt; i++) { //go over every channel:
				delayAfter = action(currCom, channels[i]);
			}
		}
		else { //if not all channels are selected, act only for one channel:
			delayAfter = action(currCom, channels[selectedChannel-1]);
		}
		
		if(delayAfter) {
			digitalWrite(STATUS_LED, HIGH);
			delay(PAUSE_DELAY);
			digitalWrite(STATUS_LED, LOW);
			digitalWrite(ERR_LED, LOW);
		}

		//now update the channel outValues:
		for(byte i=0; i<channelsAmnt; i++) {
			analogWrite(channels[i].pin, channels[i].outValue);
		}
		//update lastCom
		if(currCom != 0xFFFFFFFF && currCom != 0) lastCom = currCom;
	}
} //end commands()

bool action(int currCom, channel selCh) {
	int outValue = selCh.outValue;
	int lastValue = selCh.lastValue;
	int specialCode = selCh.specialCode;
	bool fadeRising = selCh.fadeRising;
	byte fadeDelay = selCh.fadeDelay;
	
	bool delayAfter = false; //add a delay after this command. returned at end.
	switch(currCom) {
		case 0xFF38C7: //on/off
      		if(debug) Serial.println(F("On/off"));
			if(outValue > 0) outValue = 0;
  			else outValue = lastValue;
  			delayAfter = true;
			specialCode = 0;
			break;
		case 0xFF18E7: //up
      		if(debug) Serial.println(F("Up"));
			//if you're in a fade routine:
			if(specialCode == 0xFADE) {
				fadeDelay++; //decrement the delay value
				if(debug) {
					Serial.print("Fade delay: ");
					Serial.println(fadeDelay);
				}
				break; //and exit the switch
			}
      		
      		//determine the needed step size:
      		if(outValue < STEP_TRANSITION)
      			outValue += 1;
      		else outValue += STEP_SIZE;
      		
			//prevent overflow:
			if(outValue > MAX_OUT) outValue = MAX_OUT;
			specialCode = 0;
			break;
		case 0xFF4AB5: //down
      		if(debug) Serial.println(F("Down"));
			//if you're in a fade routine:
			if(specialCode == 0xFADE) {
				fadeDelay--; //decrement the delay value
				if(fadeDelay == 0) fadeDelay = 1;
				if(debug) {
					Serial.print("Fade delay: ");
					Serial.println(fadeDelay);
				}
				break; //and exit the switch
			}
      		
      		//determine the needed step size:
      		if(outValue-STEP_SIZE < STEP_TRANSITION)
      			outValue -= 1;
      		else outValue -= STEP_SIZE;
      		
			//prevent overflow:
			if(outValue < 0) outValue = 0;
			specialCode = 0;
			break;
			
		case 0xFF9867: //0 button pressed
			if(!channelSelect) {
				outValue = numButton(0);
				specialCode = 0;
			}
			else selectChannel(0);
			delayAfter = true;
			break;
		case 0xFFA25D: //1 button pressed
			if(!channelSelect) {
				outValue = numButton(1);
				specialCode = 0;
			}
			else selectChannel(1);
			delayAfter = true;
			break;
		case 0xFF629D: //2 button pressed
			if(!channelSelect) {
				outValue = numButton(2);
				specialCode = 0;
			}
			else selectChannel(2);
			delayAfter = true;
			break;
		case 0xFFE21D: //3 button pressed
			if(!channelSelect) {
				outValue = numButton(3);
				specialCode = 0;
			}
			else selectChannel(3);
			delayAfter = true;
			break;
		case 0xFF22DD: //4 button pressed
			if(!channelSelect) {
				outValue = numButton(4);
				specialCode = 0;
			}
			else selectChannel(4);
			delayAfter = true;
			break;
		case 0xFF02FD: //5 button pressed
			if(!channelSelect) {
				outValue = numButton(5);
				specialCode = 0;
			}
			else selectChannel(5);
			delayAfter = true;
			break;
		case 0xFFC23D: //6 button pressed
			if(!channelSelect) {
				outValue = numButton(6);
				specialCode = 0;
			}
			else selectChannel(6);
			delayAfter = true;
			break;
		case 0xFFE01F: //7 button pressed
			if(!channelSelect) {
				outValue = numButton(7);
				specialCode = 0;
			}
			else selectChannel(7);
			delayAfter = true;
			break;
		case 0xFFA857: //8 button pressed
			if(!channelSelect) {
				outValue = numButton(8);
				specialCode = 0;
			}
			else selectChannel(8);
			delayAfter = true;
			break;
		case 0xFF906F: //9 button pressed
			if(!channelSelect) {
				outValue = numButton(9);
				specialCode = 0;
			}
			else selectChannel(9);
			delayAfter = true;
			break;

		case 0xFF6897: //asterisk button pressed, fade
			//toggle the action state and print:
			if(debug) Serial.print("Fade ");
			if(!specialCode) {
				specialCode = 0xFADE; //define the action
				currCom = specialCode;
				if(debug) Serial.println("turned on");
				delay(100);
			}
			else {
				specialCode = 0;
				if(debug) Serial.println("turned off");
				delayAfter = true;
			}
		
		case 0xFADE: //fade action
			//switch direction when at edge to avoid overflow:
			if(outValue == MAX_OUT) fadeRising = false;
			if(outValue == 0) 		fadeRising = true;
			
			if(fadeRising) 	outValue++;
			else 			outValue--;
			delay(fadeDelay);
			break;

		case 0xFFB04F: //hash button pressed
			channelSelect = !channelSelect;
			delayAfter = true;
			break;
		
		default:
			if(debug) Serial.println(F("Unrecognized command."));
			digitalWrite(ERR_LED, HIGH);
  			delayAfter = true;
			break;
	}

	if(!specialCode) { //if you're in the middle of a special action routine
		if(debug) {
			Serial.print(F("Out value: "));
			Serial.println(outValue, DEC);
			Serial.print(F("Last value: "));
			Serial.println(lastValue, DEC);
			Serial.print(F("Last command: 0x"));
			Serial.println(lastCom, HEX);
		}
	}

	if(outValue >= MIN_BRIGHTNESS) lastValue = outValue;
	
	selCh.outValue = outValue;
	selCh.lastValue = lastValue;
	selCh.specialCode = specialCode;
	selCh.fadeRising = fadeRising;
	selCh.fadeDelay = fadeDelay;	
	return delayAfter;
}

byte numButton(byte num) {
	if(debug) {
		Serial.print(F("No. "));
		Serial.println(num);
	}
	return MAX_OUT * ((int) num) / 9;
}

void selectChannel(byte chNum) {
	//validate channel selection:
	if(chNum > channelsAmnt) {
		digitalWrite(ERR_LED, HIGH);
		return;
	}
	//flash selected channels:
	if(chNum == 0) { //all channels selected
		//we'll be changing the pin write values away from the outValue but we'll change them back soon:
		for(byte i=0; i<channelsAmnt; i++) {
			if(channels[i].outValue > MIN_BRIGHTNESS) analogWrite(channels[i].pin, 0);
			else analogWrite(channels[i].pin, MIN_BRIGHTNESS);
		}
		delay(PAUSE_DELAY);
		for(byte i=0; i<channelsAmnt; i++) {
			analogWrite(channels[i].pin, channels[i].outValue);
		}
		//2*O(n) is still linear complexity O(n^1) :)
	}
	else { //only one channel selected
		channel selected = channels[chNum-1];
		if(selected.outValue > MIN_BRIGHTNESS) analogWrite(selected.pin, 0);
		else analogWrite(selected.pin, MIN_BRIGHTNESS);
		delay(PAUSE_DELAY);
		analogWrite(selected.pin, selected.outValue);
	}

	selectedChannel = chNum; //set the global
}

void loop() {
    if (IrReceiver.decode()) {
    	digitalWrite(STATUS_LED, HIGH);
    	delay(50);
        if(debug) Serial.println();
        commands();
		IrReceiver.resume(); //receive the next value
    	digitalWrite(STATUS_LED, LOW);
    }
    //look for specialCodes in every channel:
	for(byte i=0; i<channelsAmnt; i++) {
		if(channels[i].specialCode) action(channels[i].specialCode, channels[i]);
	}
}
