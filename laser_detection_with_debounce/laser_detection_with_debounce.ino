const unsigned long debounceDelay = 50;

const int inputPin_Laser0 = 2;
const int inputPin_Laser1 = 2;
const int inputPin_Laser2 = 2;
const int inputPin_Laser3 = 2;
const int inputPin_Laser4 = 2;
const int inputPin_Laser5 = 2;

//TODO These are the REAL laser pin mappings, for when we get more lasers/detectors
//const int inputPin_Laser0 = 2;
//const int inputPin_Laser1 = 3;
//const int inputPin_Laser2 = 4;
//const int inputPin_Laser3 = 5;
//const int inputPin_Laser4 = 6;
//const int inputPin_Laser5 = 7;
const int inputPin_Gate = 8;

int inputPin_Laser0_state = LOW;
int inputPin_Laser1_state = LOW;
int inputPin_Laser2_state = LOW;
int inputPin_Laser3_state = LOW;
int inputPin_Laser4_state = LOW;
int inputPin_Laser5_state = LOW;
int inputPin_Gate_state = LOW;

int inputPin_Laser0_lastState = LOW;
int inputPin_Laser1_lastState = LOW;
int inputPin_Laser2_lastState = LOW;
int inputPin_Laser3_lastState = LOW;
int inputPin_Laser4_lastState = LOW;
int inputPin_Laser5_lastState = LOW;
int inputPin_Gate_lastState = LOW;

const int outputPin_builtInLed = LED_BUILTIN;

const int stateMachine_idle = 0;
const int stateMachine_ready = 1;
const int stateMachine_racing = 2;
const unsigned long stateMachine_blinkDelay_idle = 1000;
const unsigned long stateMachine_blinkDelay_ready = 500;
const unsigned long stateMachine_blinkDelay_racing = 166;
unsigned long stateMachine_blink_last_toggle_time = 0;
int stateMachine_blink_state = LOW;

int stateMachine_state = stateMachine_idle;

unsigned long lastDebounceTime_Laser0 = 0;
unsigned long lastDebounceTime_Laser1 = 0;
unsigned long lastDebounceTime_Laser2 = 0;
unsigned long lastDebounceTime_Laser3 = 0;
unsigned long lastDebounceTime_Laser4 = 0;
unsigned long lastDebounceTime_Laser5 = 0;
unsigned long lastDebounceTime_Gate = 0;

unsigned long gateOpeningTime;
unsigned long laser0_interrupted_time = 0;
unsigned long laser1_interrupted_time = 0;
unsigned long laser2_interrupted_time = 0;
unsigned long laser3_interrupted_time = 0;
unsigned long laser4_interrupted_time = 0;
unsigned long laser5_interrupted_time = 0;

void setup() {
  Serial.begin(9600);
  pinMode(outputPin_builtInLed, OUTPUT);
  pinMode(inputPin_Laser0, INPUT);
  pinMode(inputPin_Laser1, INPUT);
  pinMode(inputPin_Laser2, INPUT);
  pinMode(inputPin_Laser3, INPUT);
  pinMode(inputPin_Laser4, INPUT);
  pinMode(inputPin_Laser5, INPUT);
  pinMode(inputPin_Gate, INPUT);

  stateMachine_state = stateMachine_ready;
  inputPin_Gate_state = HIGH;
}

void loop() {
  readAllPins();
  loopStateMachine();
}

/**
   IDLE: Nothing going on, waiting for signal from SERIAL to move to READY
         The gate signal should read HIGH (gate closed) before moving to READY\
         SLOW BLINK (0.5 Hz)

   READY: Gate should be closed during this state, Humans verify that the laser-input-pins all
          read HIGH. Opening the gate (causing gate to read LOW) will move state to RACING
          MEDIUM BLINK (1 Hz)

   RACING: After all lasers experience an interrupt (HIGH -> LOW) event, all times will be
           reported over serial, then state will move back to IDLE.
           RAPID BLINK (3 Hz)
*/
void loopStateMachine() {
  controlLedForState();
  switch (stateMachine_state) {
    case stateMachine_idle:
      //TODO: receive signal from serial to continue// stateMachine_state = stateMachine_ready;
      if (false) {
        stateMachine_state = stateMachine_ready;
      }
      break;
    case stateMachine_ready:
      Serial.print("STATE:READY");
      Serial.print("gateState:");
      Serial.println(inputPin_Gate_state);
      if (inputPin_Gate_state == LOW) {
        gateOpeningTime = millis();
        clearRaceVariables();
        stateMachine_state = stateMachine_racing;
      }
      break;
    case stateMachine_racing:
      Serial.print("RACING ");
      checkLasersInterrupt();
      if (hasAllLasersInterrupted()) {
        notifyRaceFinished();
        stateMachine_state = stateMachine_idle;
        Serial.println("statemachine to IDLE");
      }
      break;
  }
}

void controlLedForState() {
  unsigned long currentTime = millis();
  unsigned long delay = 0;
  switch (stateMachine_state) {
    case stateMachine_idle:
      delay = stateMachine_blinkDelay_idle;
      break;
    case stateMachine_ready:
      delay = stateMachine_blinkDelay_ready;
      break;
    case stateMachine_racing:
      delay = stateMachine_blinkDelay_racing;
      break;
  }
  if (currentTime - stateMachine_blink_last_toggle_time > delay ) {
    stateMachine_blink_state = invertSignal(stateMachine_blink_state);
    digitalWrite(outputPin_builtInLed, stateMachine_blink_state);
    stateMachine_blink_last_toggle_time = currentTime;
  }
}

boolean invertSignal(int state) {
  if (state == HIGH) {
    return LOW;
  } else {
    return HIGH;
  }
}

void notifyRaceFinished() {
  Serial.print("RACE_END");
  Serial.print(";0,");
  Serial.print(laser0_interrupted_time);
  Serial.print(";1,");
  Serial.print(laser1_interrupted_time);
  Serial.print(";2,");
  Serial.print(laser2_interrupted_time);
  Serial.print("35,");
  Serial.print(laser3_interrupted_time);
  Serial.print(";4,");
  Serial.print(laser4_interrupted_time);
  Serial.print(";5,");
  Serial.print(laser5_interrupted_time);
  Serial.println("%%");
}

void clearRaceVariables() {
  laser0_interrupted_time = 0;
  laser1_interrupted_time = 0;
  laser2_interrupted_time = 0;
  laser3_interrupted_time = 0;
  laser4_interrupted_time = 0;
  laser5_interrupted_time = 0;
}

void readAllPins() {
  readPin(inputPin_Laser0, &inputPin_Laser0_state, &inputPin_Laser0_lastState, &lastDebounceTime_Laser0);
  readPin(inputPin_Laser1, &inputPin_Laser1_state, &inputPin_Laser1_lastState, &lastDebounceTime_Laser1);
  readPin(inputPin_Laser2, &inputPin_Laser2_state, &inputPin_Laser2_lastState, &lastDebounceTime_Laser2);
  readPin(inputPin_Laser3, &inputPin_Laser3_state, &inputPin_Laser3_lastState, &lastDebounceTime_Laser3);
  readPin(inputPin_Laser4, &inputPin_Laser4_state, &inputPin_Laser4_lastState, &lastDebounceTime_Laser4);
  readPin(inputPin_Laser5, &inputPin_Laser5_state, &inputPin_Laser5_lastState, &lastDebounceTime_Laser5);
  readPin(inputPin_Gate, &inputPin_Gate_state, &inputPin_Gate_lastState, &lastDebounceTime_Gate);
}

void readPin(int pin, int* state, int* lastState, long unsigned* lastDebounceTime ) {
  int reading = digitalRead(pin);
  if (reading != *lastState) {
    *lastDebounceTime = millis();
  }
  if ((millis() - *lastDebounceTime) > debounceDelay) {
    if (reading != *state) {
      *state = reading;
    }
  }
  *lastState = reading;
}

void checkLasersInterrupt () {
  Serial.print("\n checkLasersInterrupt inputPin_Laser0_state: ");
  Serial.print(inputPin_Laser0_state);
  if (laser0_interrupted_time == 0 && inputPin_Laser0_state == LOW) {
    Serial.print("LANE A");
    laser0_interrupted_time = millis();
  }
  if (laser1_interrupted_time == 0 && inputPin_Laser1_state == LOW) {
    Serial.print("LANE B");
    laser1_interrupted_time = millis();
  }
  if (laser2_interrupted_time == 0 && inputPin_Laser2_state == LOW) {
    Serial.print("LANE C");
    laser2_interrupted_time = millis();
  }
  if (laser3_interrupted_time == 0 && inputPin_Laser3_state == LOW) {
    Serial.print("LANE D");
    laser3_interrupted_time = millis();
  }
  if (laser4_interrupted_time == 0 && inputPin_Laser4_state == LOW) {
    Serial.print("LANE E");
    laser4_interrupted_time = millis();
  }
  if (laser5_interrupted_time == 0 && inputPin_Laser5_state == LOW) {
    Serial.print("LANE F");
    laser5_interrupted_time = millis();
  }
}

boolean hasAllLasersInterrupted() {
  boolean result = laser0_interrupted_time > 0 &&
                   laser1_interrupted_time > 0 &&
                   laser2_interrupted_time > 0 &&
                   laser3_interrupted_time > 0 &&
                   laser4_interrupted_time > 0 &&
                   laser5_interrupted_time > 0;

  Serial.print("hasAllLasersInterrupted:");
  Serial.println(result);
  return result ;
}
