#include <MIDI.h>
#include <EEPROM.h>
MIDI_CREATE_DEFAULT_INSTANCE();

int MUXOUT [5] = {A0, A1, A2, A3, A4};
int ADD [3] = {2, 3, 4};
int LED1 = 5;
int LED2 = 6;

int timeout = 250; //time in ms where switch inputs are ignored for debounce

byte chorus = 0;

byte chan = 0; //sysex send channel, values 0-15 where channel = chan + 1
int stored = 0; //default EEPROM address
byte par [36] = {}; //(0-35), ignores MANUAL mode button
byte val  = 0; //0-1, 0-3, 0-5, 0-127 depending on value type

byte fifthval [37];
byte fourthval [37];
byte thirdval [37];
byte lastval [37];
byte rawval [37];
byte newval [37];   //stores new captured values for all control parameters
byte oldval [37];   //stores last saved values at loop end

byte bytemap(byte x, byte in_min, byte in_max, byte out_min, byte out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

byte toggleFour (byte readvalFour) {
  if (readvalFour < 22) {
    return 0;
  }
  else if (readvalFour < 63) {
    return 1;
  }
  else if (readvalFour < 105) {
    return 2;
  }
  else {
    return 3;
  }
}

byte toggleSix (byte readvalSix) {
  if (readvalSix < 12) {
    return 0;
  }
  else if (readvalSix < 37) {
    return 1;
  }
  else if (readvalSix < 63) {
    return 2;
  }
  else if (readvalSix < 89) {
    return 3;
  }
  else if (readvalSix < 113) {
    return 4;
  }
  else {
    return 5;
  }
}

void setup() {
  //Serial.begin (31250);
  byte ManualRead;
  byte NoiseRead;
  byte SubRead;

  if (EEPROM.read(stored) > 15) {
    EEPROM.update (stored, 0);
  }

  pinMode (ADD [0], OUTPUT);
  pinMode (ADD [1], OUTPUT);
  pinMode (ADD [2], OUTPUT);

  pinMode (LED1, OUTPUT);
  pinMode (LED2, OUTPUT);

  delay (100);

  digitalWrite (ADD [0], 0);
  digitalWrite (ADD [1], 0);
  digitalWrite (ADD [2], 0);

  ManualRead = byte(analogRead (MUXOUT [0]) / 8);

  if ((ManualRead) > 42) {
    
    digitalWrite (ADD [0], 1);
    digitalWrite (ADD [1], 1);
    digitalWrite (ADD [2], 1);

    SubRead = byte(analogRead (MUXOUT [3]) / 8);
    SubRead = toggleFour (SubRead);

    digitalWrite (ADD [0], 0);
    digitalWrite (ADD [1], 0);
    digitalWrite (ADD [2], 1);

    NoiseRead = byte(analogRead (MUXOUT [3]) / 8);
    NoiseRead = toggleFour (NoiseRead);


    chan = NoiseRead + (4 * SubRead);
    EEPROM.update (stored, chan);
    /*
    MIDI CHANNELS GUIDE (accounts for chan being 0 indexed)

             NOISE LEVEL
          | 0   1   2   3
          |---------------
      S 0 | 1   2   3   4
      U   |
      B 1 | 5   6   7   8
      L   |
      E 2 | 9   10  11  12
      V   |
      E 3 | 13  14  15  16
      L   |
    */

    int q = 0;

    while (q < 3) {
      digitalWrite (LED2, HIGH);
      delay (300);
      digitalWrite (LED2, LOW);
      delay (300);    
      q++;
    }


  
  }
  chan = EEPROM.read (stored);
  MIDI.begin(chan + 1); //listening on channel set by chan

}

void singleSend (byte param, byte value) { //this function is equal to IPR routine in the PG-300 manual
  byte singleParam [10] = {0xF0, 0x41, 0x36, chan, 0x23, 0x20, 0x01, param, value, 0xF7};
  MIDI.sendSysEx (sizeof(singleParam), singleParam, true);
  //Serial.print (param);
  //Serial.println (value);
}

void manualSend () { //this function is equal to APR routine in the PG-300 manual
  byte allParams [44] = {0xF0, 0x41, 0x35, chan, 0x23, 0x20, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF7};
  
  int m = 0;

  while (m < 35) {
    allParams [m + 7] = par [m];
    m++;
  }
  MIDI.sendSysEx (sizeof(allParams), allParams, true);
  /*int count = 0;
  while (count < 44) {
    Serial.println(allParams[count]);
    count++;
  }
  */
}

void loop() {
  MIDI.read();

  int i = 0;

  while (i < 5) {

    int j = 0;

    while (j < 8) {
      int inc = (i*8) + j;
      
      digitalWrite(ADD [0], bitRead (j, 0)); //ADD array is pins 2, 3, 4 that set address pins A, B, C of the 5X 4051s
      digitalWrite(ADD [1], bitRead (j, 1));
      digitalWrite(ADD [2], bitRead (j, 2));

      if (inc <= 36) {                                       //count goes up to 39 but there are only 37 controls
        
        //oldval [inc] = newval [inc];                                //newval is an array of values read from the controls, stored in mux order
        //newval [inc] = byte ((analogRead (MUXOUT [i])) / 8); //for each address setting, read the COM pin of all 5x 4051s on A0 - A4
        
        fifthval [inc] = fourthval [inc];
        fourthval [inc] = thirdval [inc];
        thirdval [inc] = lastval [inc];
        lastval [inc] = rawval [inc];
        rawval [inc] = byte ((analogRead (MUXOUT [i])) / 8);

        oldval [inc] = newval [inc];
        newval [inc] = (rawval [inc] + lastval [inc] + thirdval [inc] + fourthval [inc] + fifthval [inc]) / 5;

      }
      else {
        break;
      }
      j++;
    }
    i++;
  }

  //store values so their array location matches PG300 manual
  for (int k = 0; k < 37; k++) {
    unsigned long checkTime = millis();
    if (newval [k] != oldval [k]) {
      
    switch (k) {
      case 0: //MANUAL, 0-1 this command sends ALL values 0-35 in one message
        static unsigned long manualTime;
        if (newval [k] < 42 && (checkTime - manualTime > timeout)) {
          digitalWrite (LED2, HIGH);
          manualSend();
          manualTime = checkTime;
        }
        else {
          digitalWrite (LED2, LOW);

        }
      break;

      case 1: //HPF CUTOFF FREQ, 0-3
        par [9] = toggleFour (newval[k]);
        singleSend (9, par [9]);
      break;

      case 2: //VCF ENV MODE, 0-3
        par [1] = toggleFour (newval[k]);
        singleSend (1, par [1]);
      break;

      case 3: //CHORUS, 0-1
        static unsigned long chorusTime;
        if (newval [k] < 42 && chorus == 0 && (checkTime - chorusTime > timeout)) {
          digitalWrite (LED1, HIGH);
          chorus = 1;
          par [10] = 1;
          singleSend (10, par [10]);
          chorusTime = checkTime;
        }
        else if (newval [k] < 42 && chorus == 1 && (checkTime - chorusTime > timeout)) {
          digitalWrite (LED1, LOW);
          chorus = 0;
          par [10] = 0;
          singleSend (10, par [10]);
          chorusTime = checkTime;
        }
      break;

      case 4: //VCF KEY FOLLOW, 0-127
        par [20] = newval [k];
        singleSend (20, par [20]);
      break;

      case 5: //BENDER RANGE, 0-12
        par [35] = bytemap (newval[k], 0, 127, 0, 12);
        singleSend (35, par [35]);
      break;

      case 6: //VCF AFTER DEPTH, 0-127
        par [21] = newval [k];
        singleSend (21, par [21]);
      break;

      case 7: //CHORUS RATE, 0-127
        par [34] = newval [k];
        singleSend (34, par [34]);
      break;

      case 8: //ENV L3, 0-127 (SUSTAIN LEVEL)
        par [31] = newval [k];
        singleSend (31, par [31]);
      break;

      case 9: //VCF RESONANCE, 0-127
        par [17] = newval [k];
        singleSend (17, par [17]);
      break;

      case 10: //VCF CUTOFF FREQ, 0-127
        par [16] = newval [k];
        singleSend (16, par [16]);
      break;

      case 11: //ENV T3, 0-127 (DECAY TIME)
        par [30] = newval [k];
        singleSend (30, par [30]);
      break;

      case 12: //VCF LFO MOD DEPTH, 0-127
        par [18] = newval [k];
        singleSend (18, par [18]);
      break;

      case 13: //ENV T4, 0-127 (RELEASE TIME)
        par [32] = newval [k];
        singleSend (32, par [32]);
      break;

      case 14: //VCF ENV MOD DEPTH, 0-127
        par [19] = newval [k];
        singleSend (19, par [19]);
      break;

      case 15: //ENV KEY FOLLOW, 0-127
        par [33] = newval [k];
        singleSend (33, par [33]);
      break;

      case 16: //ENV L1, 0-127 (ATTACK LEVEL)
        par [27] = newval [k];
        singleSend (27, par [27]);
      break;

      case 17: //DCO ENV MODE, 0-3
        par [0] = toggleFour (newval[k]);
        singleSend (0, par [0]);
      break;

      case 18: //DCO PWM DEPTH, 0-127
        par [14] = newval [k];
        singleSend (14, par [14]);
      break;

      case 19: //ENV T1, 0-127 (ATTACK TIME)
        par [26] = newval [k];
        singleSend (26, par [26]);
      break;

      case 20: //DCO PWM RATE, 0-127
        par [15] = newval [k];
        singleSend (15, par [15]);
      break;

      case 21: //ENV T2, 0-127 (BREAK TIME)
        par [28] = newval [k];
        singleSend (28, par [28]);
      break;

      case 22: //DCO AFTER DEPTH, 0-127
        par [13] = newval [k];
        singleSend (13, par [13]);
      break;

      case 23: //ENV L2, 0-127 (BREAK LEVEL)
        par [29] = newval [k];
        singleSend (29, par [29]);
      break;
   
      case 24: //VCA AFTER DEPTH, 0-127
        par [23] = newval [k];
        singleSend (23, par [23]);
      break;

      case 25: //VCA ENV MODE, 0-3
        par [2] = toggleFour (newval[k]);
        singleSend (2, par [2]);
      break;

      case 26: //LFO RATE, 0-127
        par [24] = newval [k];
        singleSend (24, par [24]);
      break;
    
      case 27: //LFO DELAY TIME, 0-127
        par [25] = newval [k];
        singleSend (25, par [25]);
      break;

      case 28: //DCO NOISE LEVEL, 0-3
        par [8] = toggleFour (newval[k]);
        singleSend (8, par [8]);
      break;

      case 29: //DCO WAVE PULSE, 0-3
        par [3]  = toggleFour (newval[k]);
        singleSend (3, par [3]);
      break;

      case 30: //DCO WAVE SUB, 0-5
        par [5] = toggleSix (newval[k]);
        singleSend (5, par [5]);
      break;

      case 31: //DCO SUB LEVEL, 0-3
        par [7] = toggleFour (newval[k]);
        singleSend (7, par [7]);
      break;

      case 32: //DCO RANGE, 0-3
        par [6] = toggleFour (newval[k]);
        singleSend (6, par [6]);
      break;

      case 33: //DCO ENV MOD DEPTH, 0-127
        par [12] = newval [k];
        singleSend (12, par [12]);
      break;

      case 34: //DCO LFO MOD DEPTH, 0-127
        par [11] = newval [k];
        singleSend (11, par [11]);
      break;

      case 35: //DCO WAVE SAW, 0-5
        par [4] = toggleSix (newval[k]);
        singleSend (4, par [4]);
      break;

      case 36: //VCA LEVEL, 0-127
        par [22] = newval [k];
        singleSend (22, par [22]);
      break;
    
      }
    }
  }
}