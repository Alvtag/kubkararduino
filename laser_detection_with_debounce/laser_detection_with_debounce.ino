const unsigned long debounceDelay = 50;

boolean trackActive0 = true;
boolean trackActive1 = true;
boolean trackActive2 = true;
boolean trackActive3 = true;
boolean trackActive4 = true;
boolean trackActive5 = true;

const int inputPin_Laser0 = 2;
const int inputPin_Laser1 = 3;
const int inputPin_Laser2 = 4;
const int inputPin_Laser3 = 5;
const int inputPin_Laser4 = 6;
const int inputPin_Laser5 = 7;
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

// STATE MACHINE
const int stateMachine_idle = 0;
const int stateMachine_ready = 1;
const int stateMachine_racing = 2;
int stateMachine_state = stateMachine_idle;
const unsigned long stateMachine_blinkDelay_ready = 500;
const unsigned long stateMachine_blinkDelay_racing = 100;
unsigned long stateMachine_blink_last_toggle_time = 0;
int stateMachine_blink_state = LOW;

unsigned long lastDebounceTime_Laser0 = 0;
unsigned long lastDebounceTime_Laser1 = 0;
unsigned long lastDebounceTime_Laser2 = 0;
unsigned long lastDebounceTime_Laser3 = 0;
unsigned long lastDebounceTime_Laser4 = 0;
unsigned long lastDebounceTime_Laser5 = 0;
unsigned long lastDebounceTime_Gate = 0;

// RACE TIMINGS
unsigned long gateOpeningTime;
unsigned long laser0_interrupted_time = 0;
unsigned long laser1_interrupted_time = 0;
unsigned long laser2_interrupted_time = 0;
unsigned long laser3_interrupted_time = 0;
unsigned long laser4_interrupted_time = 0;
unsigned long laser5_interrupted_time = 0;

// SERIAL COMMUNICATION
String serialRxBuffer = "";
boolean DEBUG = false;

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

  stateMachine_state = stateMachine_idle;
  inputPin_Gate_state = HIGH;
  digitalWrite(outputPin_builtInLed, stateMachine_blink_state);
}

void loop() {
  readAllPins();
  loopStateMachine();
  // as much as possible, refrain from using serial comms during racing state, since
  // the single-threadedness of arduino means we'll be using cycles to communicate
  // rather than monitor signals.
  loopSerialCommunication();
}

/**
   IDLE: Nothing going on, waiting for signal from SERIAL to move to READY
         The gate signal should read HIGH (gate closed) before moving to READY\
         SOLID_ON

   READY: Gate should be closed during this state, Humans verify that the laser-input-pins all
          read HIGH. Opening the gate (causing gate to read LOW) will move state to RACING
          MEDIUM BLINK (1 Hz)

   RACING: After all lasers experience an interrupt (HIGH -> LOW) event, all times will be
           reported over serial, then state will move back to IDLE.
           RAPID BLINK (5 Hz)
*/
void loopStateMachine() {
  controlLedForState();
  switch (stateMachine_state) {
    case stateMachine_idle:
      break;
    case stateMachine_ready:
      if (inputPin_Gate_state == LOW) {
        clearRaceVariables();
        gateOpeningTime = millis();
        stateMachine_state = stateMachine_racing;
      }
      break;
    case stateMachine_racing:
      checkLasersInterrupt();
      if (hasAllLasersInterrupted()) {
        notifyRaceFinished();
        stateMachine_state = stateMachine_idle;
      }
      break;
  }
}

/**
   Commands from Serial should be in the format $$COMMAND_WORDS%%
*/
void loopSerialCommunication() {
  char character;
  int origLength = serialRxBuffer.length();
  while (Serial.available()) {
    character = Serial.read();
    serialRxBuffer.concat(character);
  }
  serialRxBuffer.replace("\n", "");

  int newLength = serialRxBuffer.length();
  if (newLength != origLength && DEBUG) {
    Serial.println("\n current buffer:" + serialRxBuffer);
  }

  int beginIndex = serialRxBuffer.indexOf("$$");
  int endIndex = serialRxBuffer.indexOf("%%");
  if (beginIndex != -1 && endIndex != -1) {
    String content = serialRxBuffer.substring(beginIndex + 2, endIndex);
    serialRxBuffer = serialRxBuffer.substring(endIndex + 2);
    onSerialCommandReceived(content);
  }
}

/**
   $$STATUS%% Will print the status of all pins (Gates, Lasers zero through five)
   $$BEGIN_RACE%% change the state from IDLE to READY. please ensure
*/
void onSerialCommandReceived(String content) {
  if (String("STATUS").equals(content)) {
    String output = ("$$STATUS{ \"gate\":");
    output += (inputPin_Gate_state);
    output += (",\"gate0\":");
    output += (inputPin_Laser0_state);
    output += (",\"gate1\":");
    output += (inputPin_Laser1_state);
    output += (",\"gate2\":");
    output += (inputPin_Laser2_state);
    output += (",\"gate3\":");
    output += (inputPin_Laser3_state);
    output += (",\"gate4\":");
    output += (inputPin_Laser4_state);
    output += (",\"gate5\":");
    output += (inputPin_Laser5_state);
    output += (",\"raceState\":");
    output += (stateMachine_state);
    output += ("}");
    output += ("%%");
    Serial.println(output);
  } else if (content.indexOf("BEGIN_RACE") > -1) {
    if (stateMachine_state == stateMachine_idle) {
      stateMachine_state = stateMachine_ready;
      //active tracks determined by if the content contains the track #
      //e.g. $$BEGIN_RACE|0135%% indicates tracks 0,1,3,5 will have cars.
      trackActive0 = content.indexOf('0') > -1;
      trackActive1 = content.indexOf('1') > -1;
      trackActive2 = content.indexOf('2') > -1;
      trackActive3 = content.indexOf('3') > -1;
      trackActive4 = content.indexOf('4') > -1;
      trackActive5 = content.indexOf('5') > -1;
    }
  } else if (content.indexOf("RESEND_END_RACE" > -1)) {
    notifyRaceFinished();
  }
}

void notifyRaceFinished() {
  String output = ("$$END_RACE{\"l0\":");
  if (trackActive0) {
    output += (laser0_interrupted_time - gateOpeningTime);
  } else {
    output += "-1";
  }
  output += (",\"l1\":");
  if (trackActive1) {
    output += (laser1_interrupted_time - gateOpeningTime);
  } else {
    output += "-1";
  }
  output += (",\"l2\":");
  if (trackActive2) {
    output += (laser2_interrupted_time - gateOpeningTime);
  } else {
    output += "-1";
  }
  output += (",\"l3\":");
  if (trackActive3) {
    output += (laser3_interrupted_time - gateOpeningTime);
  } else {
    output += "-1";
  }
  output += (",\"l4\":");
  if (trackActive4) {
    output += (laser4_interrupted_time - gateOpeningTime);
  } else {
    output += "-1";
  }
  output += (",\"l5\":");
  if (trackActive5) {
    output += (laser5_interrupted_time - gateOpeningTime);
  } else {
    output += "-1";
  }
  output += ("}%%");
  Serial.println(output);
}

void controlLedForState() {
  unsigned long currentTime = millis();
  unsigned long delay = 0;
  switch (stateMachine_state) {
    case stateMachine_idle:
      if (stateMachine_blink_state == LOW) {
        stateMachine_blink_state = HIGH;
        digitalWrite(outputPin_builtInLed, stateMachine_blink_state);
      }
      return;
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

void clearRaceVariables() {
  laser0_interrupted_time = 0;
  laser1_interrupted_time = 0;
  laser2_interrupted_time = 0;
  laser3_interrupted_time = 0;
  laser4_interrupted_time = 0;
  laser5_interrupted_time = 0;
  gateOpeningTime = 0;
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
  if (laser0_interrupted_time == 0 && inputPin_Laser0_state == LOW) {
    laser0_interrupted_time = millis();
  }
  if (laser1_interrupted_time == 0 && inputPin_Laser1_state == LOW) {
    laser1_interrupted_time = millis();
  }
  if (laser2_interrupted_time == 0 && inputPin_Laser2_state == LOW) {
    laser2_interrupted_time = millis();
  }
  if (laser3_interrupted_time == 0 && inputPin_Laser3_state == LOW) {
    laser3_interrupted_time = millis();
  }
  if (laser4_interrupted_time == 0 && inputPin_Laser4_state == LOW) {
    laser4_interrupted_time = millis();
  }
  if (laser5_interrupted_time == 0 && inputPin_Laser5_state == LOW) {
    laser5_interrupted_time = millis();
  }
}

boolean hasAllLasersInterrupted() {
  // A track is finished if it's not active or its laser gets interrupted.
  boolean trackFinished0 = !trackActive0 || laser0_interrupted_time > 0;
  boolean trackFinished1 = !trackActive1 || laser1_interrupted_time > 0;
  boolean trackFinished2 = !trackActive2 || laser2_interrupted_time > 0;
  boolean trackFinished3 = !trackActive3 || laser3_interrupted_time > 0;
  boolean trackFinished4 = !trackActive4 || laser4_interrupted_time > 0;
  boolean trackFinished5 = !trackActive5 || laser5_interrupted_time > 0;
  return trackFinished0 && trackFinished1 && trackFinished2 &&
         trackFinished3 && trackFinished4 && trackFinished5;
}
