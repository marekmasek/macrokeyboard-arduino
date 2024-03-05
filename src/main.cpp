#include <Arduino.h>
#include <HID-Project.h>
#include <HMouse.h>
#include <ClickEncoder.h>
#include <TimerOne.h>

#define NUMBER_OF_KEYS 8       // Count of keys in the keyboard
#define MAX_COMBINATION_KEYS 4 // Maximum number of key codes that can be pressed at the same time (does dont correspond to actually pressed keys)
#define MAX_SEQUENCE_KEYS 16   // Maximum length of key combination sequence (that means first you send CTRL + Z (1. combination), then SHIFT + ALT + X (2. combination), then A (3. combination) ... )

#define DEBOUNCING_MS 20         // wait in ms when key can oscilate
#define FIRST_REPEAT_CODE_MS 500 // after FIRST_REPEAT_CODE_MS ,s if key is still pressed, start sending the command again
#define REPEAT_CODE_MS 150       // when sending command by holding down key, wait this long before sending command egain

// Rotary encoder connections
#define ENCODER_CLK 4
#define ENCODER_DT 3
#define ENCODER_SW 2

// Defining types
enum TKeyState {
  INACTIVE,
  DEBOUNCING,
  ACTIVE,
  HOLDING
}; // Key states - INACTIVE -> DEBOUNCING -> ACTIVE -> HOLDING -> INACTIVE
//                                                                                                               -> INACTIVE
enum TKeyType {
  KEYBOARD,
  CONSUMER,
  SYSTEM,
  MODIFIER
}; // Types of key codes - simulating keyboard, mouse, multimedia or modifier that alters the rotary encoder behavior

typedef struct TActions {
  uint16_t durationMs;
  uint16_t key[MAX_COMBINATION_KEYS];
} TAction;

typedef struct TKeys {
  uint8_t pin;
  enum TKeyType type;
  enum TKeyState state;
  uint32_t stateStartMs;
  uint16_t modificatorKeys[MAX_COMBINATION_KEYS];
  TAction action[MAX_SEQUENCE_KEYS];
} TKey;

// Define actions for your keys
TKey key[NUMBER_OF_KEYS] = {
  {.pin = 9, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 0, .key = {KEY_F13}}}},
  {.pin = 8, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 0, .key = {KEY_F14}}}},
  {.pin = 7, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 0, .key = {KEY_F15}}}},
  {.pin = 6, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 0, .key = {KEY_F16}}}},
  {.pin = 10, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 0, .key = {KEY_F17}}}},
  {.pin = 16, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 0, .key = {KEY_F18}}}},
  {.pin = 14, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 50, .key = {KEY_F19}}}},
  {.pin = 15, .type = KEYBOARD, .state = INACTIVE, .stateStartMs = 0, .modificatorKeys = {}, .action = {{.durationMs = 50, .key = {KEY_F20}}}},
};

// global variables
ClickEncoder *encoder;
int16_t last, value;
bool globalModifier;

// Capture rotary encoder pulses
void timerIsr() {
  encoder->service();
}

// Execute key commands
uint8_t processKey(uint8_t keyIndex) {
  TKey *lkey = &key[keyIndex];
  if (lkey->type == KEYBOARD) {
    // Press modificators
    for (uint8_t i = 0; i < MAX_COMBINATION_KEYS; i++) {
      if (lkey->modificatorKeys[i]) {
        Keyboard.press((KeyboardKeycode)lkey->modificatorKeys[i]);
      } else {
        break;
      }
    }
    for (uint8_t i = 0; i < MAX_SEQUENCE_KEYS; i++) {
      TAction *laction = &lkey->action[i];
      if ((laction->durationMs) || (laction->key[0])) {
        //press keys
        for (uint8_t j = 0; j < MAX_COMBINATION_KEYS; j++) {
          if (laction->key[j]) {
            Keyboard.press((KeyboardKeycode)laction->key[j]);
          } else {
            break;
          }
        }
        // wait
        if (laction->durationMs) {
          delay(laction->durationMs);
        }
        //release keys
        for (uint8_t j = 0; j < MAX_COMBINATION_KEYS; j++) {
          if (laction->key[j]) {
            Keyboard.release((KeyboardKeycode)laction->key[j]);
          } else {
            break;
          }
        }
      }
      else {
        break;
      }
    }
    Keyboard.releaseAll();
  }
  else if (lkey->type == CONSUMER) {
    for (uint8_t i = 0; i < MAX_SEQUENCE_KEYS; i++) {
      TAction *laction = &lkey->action[i];
      if ((laction->durationMs) || (laction->key[0])) {
        //press keys
        for (uint8_t j = 0; j < MAX_COMBINATION_KEYS; j++) {
          if (laction->key[j]) {
            Consumer.press((ConsumerKeycode)laction->key[j]);
          } else {
            break;
          }
        }
        // wait
        if (laction->durationMs) {
          delay(laction->durationMs);
        }
        //release keys
        for (uint8_t j = 0; j < MAX_COMBINATION_KEYS; j++) {
          if (laction->key[j]) {
            Consumer.release((ConsumerKeycode)laction->key[j]);
          } else {
            break;
          }
        }
      } else {
        break;
      }
    }
    Consumer.releaseAll();
  }
  else if (lkey->type == SYSTEM) {
    if (lkey->action[0].key[0]) {
      System.write((SystemKeycode)lkey->action[0].key[0]);
    }
  }
}

void checkKeys() {
  // read the key's states and if one is pressed, execute the associated command
  for (uint8_t i = 0; i < NUMBER_OF_KEYS; i++) {
    uint8_t keyState = digitalRead(key[i].pin);
    if ((key[i].state == INACTIVE) && (keyState == LOW)) {
      key[i].state = DEBOUNCING;
      key[i].stateStartMs = millis();
    }
    else if (key[i].state == DEBOUNCING) {
      if (keyState == HIGH) {
        key[i].stateStartMs = millis();
      }
      else if ((millis() - key[i].stateStartMs) > DEBOUNCING_MS) {
        key[i].state = ACTIVE;
        processKey(i);
      }
    }
    else if (key[i].state == ACTIVE) {
      if (keyState == HIGH) {
        key[i].state = INACTIVE;
        key[i].stateStartMs = millis();
        if (key[i].type == MODIFIER) {
          globalModifier = false;
        }
      }
      else if ((millis() - key[i].stateStartMs) > FIRST_REPEAT_CODE_MS) {
        key[i].state = HOLDING;
        key[i].stateStartMs = millis();
        processKey(i);
      }
    }
    else if (key[i].state == HOLDING) {
      if (keyState == HIGH) {
        key[i].state = INACTIVE;
        key[i].stateStartMs = millis();
        if (key[i].type == MODIFIER) {
          globalModifier = false;
        }
      }
      else if ((millis() - key[i].stateStartMs) > REPEAT_CODE_MS) {
        key[i].stateStartMs = millis();
        processKey(i);
      }
    }
  }
}

void processEncoder() {
  value += encoder->getValue();
  if (value != last) {
    uint16_t diff = abs(value - last);
    signed char wheel = (last < value) ? 1 : -1;
    for (uint8_t i = 0; i < diff; i++) {
      if (globalModifier) {
        HMouse.move(0, 0, wheel, 0);
      } else {
        HMouse.move(0, 0, 0, wheel);
      }
    }
    last = value;
  }
}

void processEncoderBtn() {
  ClickEncoder::Button b = encoder->getButton(); // Asking the button for it's current state
  if (b != ClickEncoder::Open) { // If the button is unpressed, we'll skip to the end of this if block
    switch (b) {
      case ClickEncoder::Clicked: // Button was clicked once
        globalModifier = !globalModifier;
        break;
      case ClickEncoder::DoubleClicked: // Button was double clicked
        Keyboard.write(KEY_F21);
        break;
    }
  }
}

void setup() {
  Keyboard.begin();
  Consumer.begin();
  System.begin();
  HMouse.begin();

  encoder = new ClickEncoder(ENCODER_DT, ENCODER_CLK, ENCODER_SW, 4);
  for (uint8_t i = 0; i < NUMBER_OF_KEYS; i++) {
    pinMode(key[i].pin, INPUT_PULLUP);
  }

  Timer1.initialize(250);
  Timer1.attachInterrupt(timerIsr);

  last = -1;
  globalModifier = false;
}

void loop() {
  checkKeys();
  processEncoder();
  processEncoderBtn();
}
