//#define DEBUG
//#define NOIR
//define DEBUG to allow printing of debug messages to the serial monitor
//define NOIR to make the controller listen to commands from serial instead of from the sensor

#if defined(NOIR) && !defined(DEBUG)
	#define DEBUG
#endif

#define STATUS_LED	0		//green
#define ERR_LED		1		//red

#define MAX_OUT 255			//max analogWrite() value

#define STEP_SIZE		5	//step size for the up/down buttons
#define MIN_BRIGHTNESS	50	//lowest observable brightness, so as not to suddenly blind the user
#define STEP_TRANSITION	15	//the point at which the steps transition from STEP_SIZE to 1

#define PAUSE_DELAY		750	//the basic pause between command acceptance

byte selectedChannel = 0;	//the channel to be acted upon
bool chSelActive = false;	//are we selecting a channel?
byte channelsAmnt;			//the amount of available channels according to channels[]

struct channel {
	byte pin;				//arduino pin
	bool selected;			//is the channel active?
	
	int outValue;			//current output value
	int lastValue;			//last output value
	
	//special actions variables:
	int specialCode;		//what action is performed
	bool fadeRising;		//is the outValue rising or falling
	int fadeDelay;			//delay between each outValue change
};

channel channels[] = {
	{ 5,  false, MIN_BRIGHTNESS, MIN_BRIGHTNESS, 0, true, 10 },
	{ 10, false, MIN_BRIGHTNESS, MIN_BRIGHTNESS, 0, true, 10 },
	{ 9,  false, MIN_BRIGHTNESS, MIN_BRIGHTNESS, 0, true, 10 }
};

/* Mind the PWM timers, pins 3 and 11 are controlled by timer 2,
 * which is used by default by IRremote for ATmega328s.
 * Change the pin number, or change the timer in the libary
 * (IRremote/src/private/IRremoteBoardDefs.h)
 */

enum action {
	ONOFF,
	UP,
	DOWN,
	
	ZERO,
	ONE,
	TWO,
	THREE,
	FOUR,
	FIVE,
	SIX,
	SEVEN,
	EIGHT,
	NINE,
	
	ASTERIX,
	HASH,
	
	SPECIAL,	//special action, determined by the specialCode
	HELP,		//display help message for serial
	REPEAT,		//repeat the lastCom
	ERR			//no action taken because of wrong command
};

//actions that should be ran only once, and not for every channel:
action runOnceActions[] = {HASH, HELP, ERR};

action currCom = 0; //current command
action lastCom = 0; //last valid command

#ifndef NOIR
	#include <IRremote.h>
	#define IR_RECEIVE_PIN 2 //IR sensor pin
	IRrecv IrReceiver(IR_RECEIVE_PIN);
	decode_results results;

	//if defined to use IR, create translate() for IR codes.
	//otherwise create translate() for serial char commands (see #else)
	action translate(int code) {
		switch(code) {
			case 0xFF38C7:
				return ONOFF;
			case 0xFF18E7:
				return UP;
			case 0xFF4AB5:
				return DOWN;
				
			case 0xFF9867:
				return ZERO;
			case 0xFFA25D: 
				return ONE;
			case 0xFF629D: 
				return TWO;
			case 0xFFE21D: 
				return THREE;
			case 0xFF22DD: 
				return FOUR;
			case 0xFF02FD: 
				return FIVE;
			case 0xFFC23D: 
				return SIX;
			case 0xFFE01F: 
				return SEVEN;
			case 0xFFA857: 
				return EIGHT;
			case 0xFF906F: 
				return NINE;
				
			case 0xFF6897: 
				return ASTERIX;
			case 0xFFB04F: 
				return HASH;

			case 0xFFFFFF:
				return REPEAT;

			default:
				return ERR;
		}
	}
#else		
	action translate(char code) {
		switch(code) {
			case 'h':
			case 'H':
				return HELP;

			case 'r':
			case 'R':
				return REPEAT;
			
			case 'o':
			case 'O':
				return ONOFF;
			case 'u':
			case 'U':
				return UP;
			case 'd':
			case 'D':
				return DOWN;
			case '0':
				return ZERO;
			case '1': 
				return ONE;
			case '2': 
				return TWO;
			case '3': 
				return THREE;
			case '4': 
				return FOUR;
			case '5': 
				return FIVE;
			case '6': 
				return SIX;
			case '7': 
				return SEVEN;
			case '8': 
				return EIGHT;
			case '9': 
				return NINE;
			case '*': 
				return ASTERIX;
			case '#': 
				return HASH;

			default:
				return ERR;
		}
	}
#endif

void setup() {
	OSCCAL = 0x39;
	channelsAmnt = sizeof(channels)/sizeof(channels[0]);
	
	pinMode(STATUS_LED, OUTPUT);
	pinMode(ERR_LED, OUTPUT);
	digitalWrite(STATUS_LED, HIGH);
	digitalWrite(ERR_LED, HIGH);
	
	for(byte i=0; i<channelsAmnt; i++) {
		pinMode(channels[i].pin, OUTPUT);
	}

	#ifndef NOIR
		IrReceiver.enableIRIn(); //start the receiver
		IrReceiver.blink13(false); //disable feedback LED
	#else
		Serial.begin(9600);
		while(!Serial);
		Serial.println("Ready.");
	#endif

	PORTD = PORTD | 0x00000011;
	delay(PAUSE_DELAY);
	digitalWrite(ERR_LED, LOW);
	digitalWrite(STATUS_LED, LOW);
}

void numButton(byte num, byte chIndex) {
	//takes action according to the number button value
	#ifdef DEBUG
		Serial.print(F("No. "));
		Serial.println(num);
	#endif
	channels[chIndex].specialCode = 0;
	channels[chIndex].outValue = (MAX_OUT * ((int) num) / 9);
}

void selectChannel(byte chNum) {
	#ifdef DEBUG
		if(chNum) {
			Serial.print(F("Selecting channel no. "));
			Serial.println(chNum);
		}
		else Serial.println(F("Selecting all channels."));
	#endif
	//validate channel selection:
	if(chNum > channelsAmnt || chNum < 0) {
		digitalWrite(ERR_LED, HIGH);
		#ifdef DEBUG
			Serial.print("Illegal channel selection: ");
			Serial.println(chNum);
		#endif
		chNum = 0;
	}
	
	//flash selected channels:
	if(chNum == 0) { //all channels selected
		//we'll be flashing each channel in channels:
		for(byte i=0; i<channelsAmnt; i++) {
			if(channels[i].outValue > MIN_BRIGHTNESS*2) analogWrite(channels[i].pin, MIN_BRIGHTNESS);
			else analogWrite(channels[i].pin, 0);
			delay(PAUSE_DELAY/channelsAmnt);
			analogWrite(channels[i].pin, channels[i].outValue);
			#ifdef DEBUG
				Serial.print(F("Marked channel "));
				Serial.print(i+1);
				Serial.print(F("... "));
			#endif
		}
		#ifdef DEBUG
			Serial.println();
		#endif
	}
	else { //only one channel selected
		byte i = chNum-1; //chNum 1 means channel at index 0 and so on.
		if(channels[i].outValue > MIN_BRIGHTNESS*2) analogWrite(channels[i].pin, MIN_BRIGHTNESS);
		else analogWrite(channels[i].pin, 0);
		delay(PAUSE_DELAY);
		analogWrite(channels[i].pin, channels[i].outValue);
		#ifdef DEBUG
			Serial.print(F("Selected channel "));
			Serial.println(i+1);
		#endif
	}

	selectedChannel = chNum; //set the global
	chSelActive = false; //clear the select flag. resume normal operation
}

void commands() {
	//check if there is a decoded result input, and if there is, work with it:
	#ifndef NOIR
	if(IrReceiver.decode(&results)) {
		currCom = translate(results.value);
	#else
	//get the first char:
	char recCom = Serial.readString()[0];
	currCom = translate(recCom);
	#ifdef DEBUG
		Serial.print("Got: ");
		Serial.println(recCom);
	#endif //end #ifdef DEBUG
	if(currCom) {
	#endif //end #ifdef NOIR
		if(currCom == REPEAT) currCom = lastCom;
		bool chSelActiveOriginal = chSelActive;
		bool delayAfter;
		
		bool runOnce = false;
		//check if action should be ran for every channel, or only once.
		//get amount of single-channel actions and iterate over all:
		byte runOnceActionsAmnt = sizeof(runOnceActions) / sizeof(runOnceActions[0]);
		for(byte i=0; i<runOnceActionsAmnt; i++) {
			if(currCom == runOnceActions[i]) {
				runOnce = true; 
				#ifdef DEBUG
					Serial.println(F("Acting only once, although all channels selected."));
				#endif
			}
		}
			
		//check channel selection and act:
		if(selectedChannel == 0 && !runOnce) { //if running for all channels is needed:
			for(byte i=0; i<channelsAmnt; i++) { //go over every channel:
				//if chSelActiveOriginal is true but chSelActive is false, a channel has just been selected. do not run act().
				//if chSelActiveOriginal is false but chSelActive is true, a select command has just been issued. no need to run act() again.
				//if both are false, normal operation. run act().
				//if both are true, a selection is pending. run act().
				if(chSelActiveOriginal == chSelActive) delayAfter = act(currCom, i);
			}
		}
		else { //if not all channels are selected or it's a one-run command, act only for one channel:
			if(runOnce)	delayAfter = act(currCom, channelsAmnt);
			else 		delayAfter = act(currCom, selectedChannel-1);
		}
		
		//update lastCom if the last command wasn't a repeat and it wasn't a channel selection:
		if(currCom != REPEAT && !chSelActiveOriginal) lastCom = currCom;
	} //end if(currCom)
} //end commands()

bool act(action currCom, byte chIndex) {
	bool delayAfter = false; //add a delay after this command.
	
	switch(currCom) {
		#ifdef NOIR
			case HELP:
				Serial.println(F("O - on/off.\nU - up, D - down.\nDigits - brightness n out of 9."));
				Serial.println(F("* - Special action.\n # - Active channel select.\n"));
			break;
		#endif
		
		case ONOFF:
			#ifdef DEBUG
				Serial.println(F("On/off"));
			#endif
			if(channels[chIndex].outValue > 0) channels[chIndex].outValue = 0;
			else channels[chIndex].outValue = channels[chIndex].lastValue;
			delayAfter = true;					//prevent rapid flashing due to IR repeat commands
			channels[chIndex].specialCode = 0;	//reset any special actions
		break;
			
		case UP:
		//in standard operation, UP raises the brightness.
		//in fade, it lengthens the cycle time.
			#ifdef DEBUG
				Serial.println(F("Up"));
			#endif
			//if you're in a fade routine:
			if(channels[chIndex].specialCode == 0xFADE) {
				channels[chIndex].fadeDelay++; //increment the delay value
				#ifdef DEBUG
					Serial.print(F("Fade delay: "));
					Serial.println(channels[chIndex].fadeDelay);
				#endif
			}
			else { //if not in fade:
				//determine the needed step size:
				if(channels[chIndex].outValue < STEP_TRANSITION)
					channels[chIndex].outValue += 1;
				else channels[chIndex].outValue += STEP_SIZE;
	
				//prevent overflow:
				if(channels[chIndex].outValue > MAX_OUT) channels[chIndex].outValue = MAX_OUT;
				channels[chIndex].specialCode = 0;
			}
		break; //end case UP:
		
		case DOWN: //down
		//in standard operation, DOWN lowers the brightness.
		//in fade, it shortens the cycle time.
			#ifdef DEBUG
				Serial.println(F("Down"));
			#endif
			//if you're in a fade routine
			if(channels[chIndex].specialCode == 0xFADE) {
				channels[chIndex].fadeDelay--; //decrement the delay value
				//prevent no delay and overflow:
				if(channels[chIndex].fadeDelay == 0) channels[chIndex].fadeDelay = 1;
				#ifdef DEBUG
					Serial.print(F("Fade delay: "));
					Serial.println(channels[chIndex].fadeDelay);
				#endif
			}
			else { //if not in fade
				//determine the needed step size:
				if(channels[chIndex].outValue-STEP_SIZE < STEP_TRANSITION)
					channels[chIndex].outValue -= 1;
				else channels[chIndex].outValue -= STEP_SIZE;
				
				//prevent overflow:
				if(channels[chIndex].outValue < 0) channels[chIndex].outValue = 0;
				channels[chIndex].specialCode = 0;
			}
		break; //end case DOWN:
			
		case ZERO:
			if(!chSelActive) {
				delayAfter = true;
				numButton(0, chIndex);
			}
			else selectChannel(0);
		break;
		case ONE:
			if(!chSelActive) {
				delayAfter = true;
				numButton(1, chIndex);
			}
			else selectChannel(1);
		break;
		case TWO:
			if(!chSelActive) {
				delayAfter = true;
				numButton(2, chIndex);
			}
			else selectChannel(2);
		break;
		case THREE:
			if(!chSelActive) {
				delayAfter = true;
				numButton(3, chIndex);
			}
			else selectChannel(3);
		break;
		case FOUR:
			if(!chSelActive) {
				delayAfter = true;
				numButton(4, chIndex);
			}
			else selectChannel(4);
		break;
		case FIVE:
			if(!chSelActive) {
				delayAfter = true;
				numButton(5, chIndex);
			}
			else selectChannel(5);
		break;
		case SIX:
			if(!chSelActive) {
				delayAfter = true;
				numButton(6, chIndex);
			}
			else selectChannel(6);
		break;
		case SEVEN:
			if(!chSelActive) {
				delayAfter = true;
				numButton(7, chIndex);
			}
			else selectChannel(7);
		break;
		case EIGHT:
			if(!chSelActive) {
				delayAfter = true;
				numButton(8, chIndex);
			}
			else selectChannel(8);
		break;
		case NINE:
			if(!chSelActive) {
				delayAfter = true;
				numButton(9, chIndex);
			}
			else selectChannel(9);
		break;

		case ASTERIX:
			//toggle the action state and print:
			#ifdef DEBUG
				Serial.print(F("Special turned "));
			#endif
			//toggle the specialCode
			if(!channels[chIndex].specialCode) {
				channels[chIndex].specialCode = 0xFADE; //define the action
				currCom = SPECIAL;
				#ifdef DEBUG
					Serial.println(F("on"));
				#endif
			}
			else {
				channels[chIndex].specialCode = 0;
				#ifdef DEBUG
					Serial.println(F("off"));
				#endif
				delayAfter = true;
			}
		 //end case ASTERIX:
		//no break! flow into case SPECIAL to begin the action right away
		
		case SPECIAL:
			//if special action is fade:
			if(channels[chIndex].specialCode == 0xFADE) {
				//switch direction when at edge to avoid overflow:
				if(channels[chIndex].outValue == MAX_OUT) channels[chIndex].fadeRising = false;
				if(channels[chIndex].outValue == 0) 		channels[chIndex].fadeRising = true;
				
				if(channels[chIndex].fadeRising) 	channels[chIndex].outValue++;
				else 								channels[chIndex].outValue--;
				delay(channels[chIndex].fadeDelay);
			}
		break;

		case HASH:
			#ifdef DEBUG
				Serial.print(F("Channel selection "));
			#endif
			chSelActive = !chSelActive;
			#ifdef DEBUG
				if(chSelActive)	Serial.println(F("turned on"));
				else			Serial.println(F("turned off"));
			#endif
			delayAfter = true;
		break;

		case ERR:
		default:
			#ifdef DEBUG
				Serial.println(F("Unrecognized command."));
			#endif
			digitalWrite(ERR_LED, HIGH);
			delayAfter = true;
			delay(100);
		break;
	}

	analogWrite(channels[chIndex].pin, channels[chIndex].outValue);

	//delay if necessary to display the status and error LEDs:
	if(delayAfter) delay(PAUSE_DELAY/channelsAmnt);
	digitalWrite(ERR_LED, LOW);
	if(!chSelActive) digitalWrite(STATUS_LED, LOW);
	
	#ifdef DEBUG
		//if you're not in the middle of a special action routine, show debugs
		//if a special action is in progress, do not print and do not spam
		if(!channels[chIndex].specialCode) {
			Serial.print(F("Acted on channel at index: "));
			Serial.println(chIndex, DEC);
			Serial.print(F("Currently selected channel: "));
			Serial.println(selectedChannel);
			Serial.print(F("Out value: "));
			Serial.println(channels[chIndex].outValue, DEC);
			Serial.print(F("Last value: "));
			Serial.println(channels[chIndex].lastValue, DEC);
			Serial.println();
		}
	#endif

	///update lastValue:
	if(channels[chIndex].outValue >= MIN_BRIGHTNESS)
		channels[chIndex].lastValue = channels[chIndex].outValue;

	analogWrite(channels[chIndex].pin, channels[chIndex].outValue);
	return delayAfter;
} //end act()

void loop() {
	//wait for data:
	#ifndef NOIR
	if(IrReceiver.decode()) {
	#else
	if(Serial.available() > 0) {
	#endif
		#ifdef DEBUG
			Serial.println();
		#endif

		digitalWrite(STATUS_LED, HIGH);
		commands();
		
		#ifndef NOIR
			IrReceiver.resume(); //receive the next value
		#endif
		
		//digitalWrite(STATUS_LED, LOW); //end busy/waiting
	}
	
	//look for specialCodes in every channel:
	for(byte i=0; i<channelsAmnt; i++) {
		if(channels[i].specialCode) act(SPECIAL, i);
	}
}
