#include <IRremote.h>

#define debug true
 
#define IR_RECEIVE_PIN 2		//IR sensor pin
IRrecv IrReceiver(IR_RECEIVE_PIN);
decode_results results;

#define STATUS_LED 3
#define ERR_LED 13
byte channelPins[] = {5, 9, 10};
/* Mind the PWM timers, pins 3 and 11 are controlled by timer 2,
 * which is used by default by IRremote for ATmega328s.
 * Change the pin number, or change the timer in the libary
 * (IRremote/src/private/IRremoteBoardDefs.h)
 */

#define MAX_OUT 255				//max analogWrite() value

#define STEP_SIZE 5				//step size for the up/down buttons
#define MIN_BRIGHTNESS 50		//lowest observable brightness, so as not to suddenly blind the user
#define STEP_TRANSITION 15		//the point at which the steps transition from STEP_SIZE to 1

#define PAUSE_DELAY 750			//the basic pause between command acceptance

int outValue = 0;				//current output value
int lastValue = MIN_BRIGHTNESS; //last valid value
long currCom = 0;				//current command
long lastCom = 0;				//last valid command

//special actions variables:
bool specialAction = false; 	//an action other than user input
int specialCode = 0;			//which special action?

bool fadeRising = true;			//is the outValue rising or falling
byte fadeDelay = 10;			//delay between each outValue change

void setup() {
	OSCCAL = 0x39;
    pinMode(STATUS_LED, OUTPUT);
    pinMode(ERR_LED, OUTPUT);
    for(byte i=0; i<(sizeof(channelPins)/sizeof(channelPins[0])); i++) {
    	pinMode(channelPins[i], OUTPUT);
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
		action(currCom);
		//update lastCom
		if(currCom != 0xFFFFFFFF && currCom != 0) lastCom = currCom;
	}
} //end commands()

void action(int currCom) {
	bool delayAfter = false; //add a delay after this command
	
	switch(currCom) {
		case 0xFF38C7: //on/off
      		if(debug) Serial.println(F("On/off"));
			if(outValue > 0) outValue = 0;
  			else outValue = lastValue;
  			delayAfter = true;
			specialAction = false;
			break;
		case 0xFF18E7: //up
      		if(debug) Serial.println(F("Up"));
			//if you're in a fade routine:
			if(specialAction && specialCode == 0xFADE) {
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
			specialAction = false;
			break;
		case 0xFF4AB5: //down
      		if(debug) Serial.println(F("Down"));
			//if you're in a fade routine:
			if(specialAction && specialCode == 0xFADE) {
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
			specialAction = false;
			break;
			
		case 0xFF9867: //0 button pressed
			outValue = numButton(0);
			delayAfter = true;
			break;
		case 0xFFA25D: //1 button pressed
			outValue = numButton(1);
			delayAfter = true;
			break;
		case 0xFF629D: //2 button pressed
			outValue = numButton(2);
			delayAfter = true;
			break;
		case 0xFFE21D: //3 button pressed
			outValue = numButton(3);
			delayAfter = true;
			break;
		case 0xFF22DD: //4 button pressed
			outValue = numButton(4);
			delayAfter = true;
			break;
		case 0xFF02FD: //5 button pressed
			outValue = numButton(5);
			delayAfter = true;
			break;
		case 0xFFC23D: //6 button pressed
			outValue = numButton(6);
			delayAfter = true;
			break;
		case 0xFFE01F: //7 button pressed
			outValue = numButton(7);
			delayAfter = true;
			break;
		case 0xFFA857: //8 button pressed
			outValue = numButton(8);
			delayAfter = true;
			break;
		case 0xFF906F: //9 button pressed
			outValue = numButton(9);
			delayAfter = true;
			break;

		case 0xFF6897:
			//toggle the action state and print:
			if(debug) Serial.print("Fade ");
			specialAction = !specialAction;
			if(specialAction) {
				if(debug) Serial.println("turned on");
				delay(100);
			}
			else {
				if(debug) Serial.println("turned off");
				delayAfter = true;
			}
			specialCode = 0xFADE; //which special action
		
		case 0xFADE: //asterisk button pressed, fade
			//switch direction when at edge to avoid overflow:
			if(outValue == MAX_OUT) fadeRising = false;
			if(outValue == 0) 		fadeRising = true;
			
			if(fadeRising) 	outValue++;
			else 			outValue--;
			delay(fadeDelay);
		break;

		case 0xFFB04F: //hash button pressed
		default:
			if(debug) Serial.println(F("Unrecognized command."));
			digitalWrite(ERR_LED, HIGH);
  			delayAfter = true;
			break;
	}

	if(!specialAction) { //if you're in the middle of a special action routine
		if(debug) {
			Serial.print(F("Out value: "));
			Serial.println(outValue, DEC);
			Serial.print(F("Last value: "));
			Serial.println(lastValue, DEC);
			Serial.print(F("Last command: 0x"));
			Serial.println(lastCom, HEX);
		}
	}

	analogWrite(channelPins[0], outValue);
	analogWrite(channelPins[1], outValue);
	analogWrite(channelPins[2], outValue);

	if(delayAfter) {
			digitalWrite(STATUS_LED, HIGH);
			delay(PAUSE_DELAY);
			digitalWrite(STATUS_LED, LOW);
			digitalWrite(ERR_LED, LOW);
	}
	
	if(outValue >= MIN_BRIGHTNESS) lastValue = outValue;
}

byte numButton(byte num) {
	specialAction = false;
	if(debug) {
		Serial.print(F("No. "));
		Serial.println(num);
	}
	return MAX_OUT * ((int) num) / 9;
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
    if(specialAction) action(specialCode);
}
