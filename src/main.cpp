#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h>
#include <algorithm>
#include <ES_CAN.h>
#include "Knob.h"
#define DISABLE_THREADS  
#define DISABLE_ISR      
#define TEST_SCANKEYS


//Constants
  const uint32_t interval = 100; //Display update interval

//Pin definitions
  //Row select and enable
  const int RA0_PIN = D3;
  const int RA1_PIN = D6;
  const int RA2_PIN = D12;
  const int REN_PIN = A5;

  //Matrix input and output
  const int C0_PIN = A2;
  const int C1_PIN = D9;
  const int C2_PIN = A6;
  const int C3_PIN = D1;
  const int OUT_PIN = D11;

  //Audio analogue out
  const int OUTL_PIN = A4;
  const int OUTR_PIN = A3;

  //Joystick analogue in
  const int JOYY_PIN = A0;
  const int JOYX_PIN = A1;

  //Output multiplexer bits
  const int DEN_BIT = 3;
  const int DRST_BIT = 4;
  const int HKOW_BIT = 5;
  const int HKOE_BIT = 6;

//Display driver object
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

//Instantiate the message

//Step Sizes
const uint32_t stepSizes [] = {51076056,54113197,57330935,60740010,64351798,68178356,72232452,76527617,81078186,85899345,91007186, 96418755};
const std::string notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const uint8_t key_size = 60;
volatile uint32_t currentStepSize[key_size] = {0};

//Keyboard Connections
bool westMost = false, eastMost = false, prevWestMost = false, prevEastMost = false, setUp = false;
volatile uint8_t position = 1; 
int uniqueID = 6;

//Knob values
volatile uint8_t volume = 4;
volatile uint8_t octave = 4;
volatile uint8_t waveform = 0;

//waveforms
const unsigned char sinetable[128] = {
  0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,
 10,  11,  12,  14,  15,  17,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,
 37,  40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
 79,  82,  85,  88,  90,  93,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124,
128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173,
176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215,
218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240, 241, 243, 244,
245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
};


//Interupt timer
HardwareTimer sampleTimer(TIM1);

//Queue handler 
QueueHandle_t msgInQ;
QueueHandle_t msgOutQ;


SemaphoreHandle_t CAN_TX_Semaphore;

//Global system state struct
struct {
  std::bitset<32> inputs;
  Knob knob0{0, 8, 8, 0};
  Knob volume{1, 1, 8, 1};
  Knob octave{2, 1, 7, 4};
  Knob waveform{3, 1, 4, 1};
  SemaphoreHandle_t mutex;
} sysState;

uint8_t RX_Message[8];

Knob* knobs[] = {&sysState.knob0, &sysState.waveform, &sysState.octave, &sysState.volume};

//Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value) {
      digitalWrite(REN_PIN,LOW);
      digitalWrite(RA0_PIN, bitIdx & 0x01);
      digitalWrite(RA1_PIN, bitIdx & 0x02);
      digitalWrite(RA2_PIN, bitIdx & 0x04);
      digitalWrite(OUT_PIN,value);
      digitalWrite(REN_PIN,HIGH);
      delayMicroseconds(2);
      digitalWrite(REN_PIN,LOW);
}

//Function to read inputs from switch matrix columns
std::bitset<4> readCols(){
  std::bitset<4> result;
  result[0] = digitalRead(C0_PIN);
  result[1] = digitalRead(C1_PIN);
  result[2] = digitalRead(C2_PIN);
  result[3] = digitalRead(C3_PIN);
  return result;

}

void setRow(uint8_t rowIdx){
  digitalWrite(REN_PIN, LOW);
  digitalWrite(RA0_PIN, rowIdx & 0x01);
  digitalWrite(RA1_PIN, rowIdx & 0x02);
  digitalWrite(RA2_PIN, rowIdx & 0x04);
  digitalWrite(REN_PIN, HIGH);
}

int setOctave(int idx, int octave){
  int stepSize{0};
  if(octave > 3){
    stepSize = stepSizes[idx] << (octave - 4);
  }
  else{
    stepSize = stepSizes[idx] >> (4 - octave);
  }

  return stepSize;
}

void setISR(){
  static uint32_t phaseAcc[key_size] = {0};
  int32_t Vout = 0;
  int k3r = __atomic_load_n(&volume, __ATOMIC_ACQUIRE); //volume
  int k2r = __atomic_load_n(&octave, __ATOMIC_ACQUIRE); //octave
  int k1r = __atomic_load_n(&waveform, __ATOMIC_ACQUIRE); //waveform
  
  for (int i = 0; i < key_size; i++) {
    if (currentStepSize[i] != 0) {
      phaseAcc[i] += currentStepSize[i];

      //adjust the wavetype
      if(k1r == 1){
        // sawtooth waveform
        Vout += (phaseAcc[i] >> 24) - 128;
      }
      else if(k1r == 2) {
        // sqaure waveform
        Vout += (phaseAcc[i] >> 24) > 128 ? -128 : 127;
      }
      else if(k1r == 3){
        // triangle waveform
        if ((phaseAcc[i] >> 24) >= 128) {
          Vout += (((255 - (phaseAcc[i] >> 24)) * 2) - 127);
        }
        else {
          Vout += ((phaseAcc[i] >> 23) - 128);
        }
      }
      else if(k1r == 4){
        //sine waveform
        int idx;

        if ((phaseAcc[i] >> 24) >= 128) {
          idx = 255 - (phaseAcc[i] >> 24);
        }
        else {
          idx = phaseAcc[i] >> 24;
        }

        Vout += (sinetable[idx] - 128);
      }
    }
  }
  //adjust the volume
  Vout = Vout >> (8 - k3r);
  Vout = std::max(std::min((int)Vout, 127), -128);


  analogWrite(OUTR_PIN, Vout + 128);
}

void CAN_RX_ISR (void) {
	uint8_t RX_Message_ISR[8];
	uint32_t ID;
	CAN_RX(ID, RX_Message_ISR);
	xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL);
}


void scanKeysTask(void * pvParameters) {
  
  const TickType_t xFrequency = 20/portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  std::bitset<4> cols;
  std::bitset<32> inputs_local;
  uint32_t currentStepSize_local;

  for(;;){ 
    #ifndef TEST_SCANKEYS
      vTaskDelayUntil( &xLastWakeTime, xFrequency );
    #endif

    for(int i=0;i<7;i++){
      setRow(i);
      delayMicroseconds(3);
      cols = readCols();
      for (int j = 0; j < 4; j++){ 

        #ifdef TEST_SCANKEYS  
        if(i<3){
          inputs_local[4*i + j] = 1; //Set all keys to being pressed for worst case scenario
        }
        else{ 
          inputs_local[4*i + j] = cols[j];
        }
        #endif
        #ifndef TEST_SCANKEYS
        inputs_local[4*i + j] = cols[j];
        #endif
      }
        
    }
    
    uint8_t octave_local = __atomic_load_n(&octave, __ATOMIC_ACQUIRE);

    bool west = !inputs_local[23];
    bool east = !inputs_local[27];

    bool westMost_local = !west;
    bool eastMost_local = !east;

    __atomic_store_n(&eastMost, eastMost_local, __ATOMIC_RELAXED);
    __atomic_store_n(&westMost, westMost_local, __ATOMIC_RELAXED);

    if(!west) __atomic_store_n(&prevEastMost, true, __ATOMIC_RELAXED);
    if(!east) __atomic_store_n(&prevWestMost, true, __ATOMIC_RELAXED);

    xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    for(int i=0;i<4;i++){
      int currentStateA = inputs_local[(12 + (3-i)*2)];
      int currentStateB = inputs_local[(13 + (3-i)*2)];
   
      knobs[i]->updateValues(currentStateA, currentStateB);
    }
    if(westMost_local && setUp){
      int prevOctave = __atomic_load_n(&octave, __ATOMIC_RELAXED);
      if (prevOctave != sysState.octave.rotation) {
        uint8_t TX_Message[8] = {0};

        TX_Message[0] = int('O'); //for octave change
        TX_Message[1] = sysState.octave.rotation;
        xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
      }
      __atomic_store_n(&volume, sysState.volume.rotation, __ATOMIC_RELAXED);
      __atomic_store_n(&octave, sysState.octave.rotation, __ATOMIC_RELAXED);
      __atomic_store_n(&waveform, sysState.waveform.rotation, __ATOMIC_RELAXED);
    }
    xSemaphoreGive(sysState.mutex);
    
    //Adding a new board request the previous most east or west octave and position
    if(!setUp && eastMost_local && !westMost_local) {
      uint8_t TX_Message[8] = {0};

      TX_Message[0] = int('E');
      TX_Message[1] = int('?');

      xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
    }

    if(!setUp && westMost_local && !eastMost_local) {
      uint8_t TX_Message[8] = {0};

      TX_Message[0] = int('W');
      TX_Message[1] = int('?');

      xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
    }

    if (westMost_local && eastMost_local) setUp = true;

    xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    for (int i = 0; i < 32; i++){
      if ( i < 12 ) {
        if (sysState.inputs[i] && !inputs_local[i]){
          if (!westMost_local) {
            uint8_t TX_Message[8] = {0};

            TX_Message[0] = int('P');
            TX_Message[1] = octave_local;
            TX_Message[2] = i;
            TX_Message[3] = position;

            xQueueSend( msgOutQ, TX_Message, portMAX_DELAY);
          }
          else {
            currentStepSize_local = setOctave(i, octave_local);
            __atomic_store_n(&currentStepSize[i * position], currentStepSize_local, __ATOMIC_RELAXED);
          }
        }
        if (!sysState.inputs[i] && inputs_local[i]) {
          if (!westMost_local) {
            uint8_t TX_Message[8] = {0};

            TX_Message[0] = int('R');
            TX_Message[1] = octave_local;
            TX_Message[2] = i;
            TX_Message[3] = position;

            xQueueSend( msgOutQ, TX_Message, portMAX_DELAY);
          }
          else{
            currentStepSize_local = 0;
            __atomic_store_n(&currentStepSize[i * position], currentStepSize_local, __ATOMIC_RELAXED);
          }
        }
      }
      sysState.inputs[i] = inputs_local[i];
    }
    xSemaphoreGive(sysState.mutex);
    //__atomic_store_n(&currentStepSize, localCurrentStepSize, __ATOMIC_RELAXED);
    #ifdef TEST_SCANKEYS
    break;
    #endif
  }  
}

void displayUpdateTask(void * pvParameters) {
  const TickType_t xFrequency = 100/portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint32_t ID;
  uint8_t local_RX_Message[8] = {0};
  

  while (1) {
    vTaskDelayUntil( &xLastWakeTime, xFrequency );

      //Update display
    u8g2.clearBuffer();         // clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
      // Load shared variables atomically
    uint8_t local_volume = __atomic_load_n(&volume, __ATOMIC_ACQUIRE);
    uint8_t local_octave = __atomic_load_n(&octave, __ATOMIC_ACQUIRE);
    uint8_t local_waveform = __atomic_load_n(&waveform, __ATOMIC_ACQUIRE);

    // Display Volume
    u8g2.setCursor(2, 10);
    u8g2.print("Volume: ");
    u8g2.print(local_volume);

    // Display Octave
    u8g2.setCursor(2, 20);
    u8g2.print("Octave: ");
    u8g2.print(local_octave);

    // Display Waveform
    u8g2.setCursor(2, 30);
    u8g2.print("Waveform: ");
    switch (local_waveform) {
      case 1:
        u8g2.print("Sawtooth");
        break;
      case 2:
        u8g2.print("Triangular");
        break;
      case 3:
        u8g2.print("Square");
        break;
      case 4:
        u8g2.print("Sine");
        break;
      default:
        u8g2.print("Unknown");
        break;
    }

    // // Display Notes Played
    // u8g2.setCursor(2, 40);
    // u8g2.print("Notes Played: ");
    // xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    // u8g2.print(sysState.inputs.to_ulong(), HEX);
    // xSemaphoreGive(sysState.mutex);

    // // Display RX Message (if applicable)
    // u8g2.setCursor(2, 50);
    // u8g2.print("RX: ");
    // u8g2.print((char) RX_Message[0]);
    // u8g2.print(RX_Message[1]);
    // u8g2.print(RX_Message[2]);
    // u8g2.print(RX_Message[3]);
    // //debugging code 
    // //u8g2.drawStr(2,10,"Hello World!");  // write something to the internal memory
    // uint8_t local_volume = __atomic_load_n(&volume, __ATOMIC_ACQUIRE);
    // uint8_t local_octave = __atomic_load_n(&octave, __ATOMIC_ACQUIRE);
    // uint8_t local_waveform = __atomic_load_n(&waveform, __ATOMIC_ACQUIRE);
    // uint8_t local_position = __atomic_load_n(&position, __ATOMIC_ACQUIRE);
    // char cstr0[2], cstr1[2], cstr2[2], cstr3[2];
    // snprintf(cstr0, sizeof(cstr0), "%d", local_position);
    // snprintf(cstr1, sizeof(cstr1), "%d", local_waveform);
    // snprintf(cstr2, sizeof(cstr2), "%d", local_octave);
    // snprintf(cstr3, sizeof(cstr3), "%d", local_volume);

    // u8g2.drawStr(2,10, cstr0);
    // u8g2.drawStr(20,10, cstr1);
    // u8g2.drawStr(38,10, cstr2);
    // u8g2.drawStr(56,10, cstr3);  

    // u8g2.setCursor(66,30);
    // u8g2.print((char) RX_Message[0]);
    // u8g2.print(RX_Message[1]);
    // u8g2.print(RX_Message[2]);
    // u8g2.print(RX_Message[3]);


    // u8g2.setCursor(2,20);
    // xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    // u8g2.print(sysState.inputs.to_ulong(),HEX);

    // Serial.print(setUp);
    // Serial.print(" ");
    // Serial.print(eastMost);
    // Serial.print(" ");
    // Serial.print(westMost);
    // Serial.print(" ");
    // Serial.print(prevEastMost);
    // Serial.print(" ");
    // Serial.print(prevWestMost);
    // Serial.println(" ");
    
    // xSemaphoreGive(sysState.mutex);
    u8g2.sendBuffer();          // transfer internal memory to the display

    //Toggle LED
    digitalToggle(LED_BUILTIN);
  }
}

void decodeTask (void * pvParameters) {
  uint32_t currentStepSize_local = 0;
  uint8_t local_RX_Message[8] = {0};
  bool westMost_local;
  bool eastMost_local;
  bool prevWestMost_local;
  bool prevEastMost_local;
  bool setUp_local; 

  int key_index;
  int octave_local;
  int position_local;

  while (1) {
    xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY);
    westMost_local = __atomic_load_n(&westMost, __ATOMIC_RELAXED);
    if(local_RX_Message[0] == int('P') && westMost_local){
      octave_local = local_RX_Message[1];
      key_index = local_RX_Message[2];
      position_local = local_RX_Message[3];
      int currentStepSize_local = setOctave(key_index, octave_local);

      __atomic_store_n(&currentStepSize[key_index * position], currentStepSize_local, __ATOMIC_RELAXED);
    } 

    if (local_RX_Message[0] == int('R') && westMost_local) {
      currentStepSize_local = 0;
      key_index = local_RX_Message[2];
      position_local = local_RX_Message[3];

      __atomic_store_n(&currentStepSize[key_index * position], currentStepSize_local, __ATOMIC_RELAXED);
    }

    if (local_RX_Message[1] == int('?')){
      westMost_local = __atomic_load_n(&westMost, __ATOMIC_RELAXED);
      eastMost_local = __atomic_load_n(&eastMost, __ATOMIC_RELAXED);
      prevEastMost_local = __atomic_load_n(&prevEastMost, __ATOMIC_RELAXED);
      prevWestMost_local = __atomic_load_n(&prevWestMost, __ATOMIC_RELAXED);
      setUp_local = __atomic_load_n(&setUp, __ATOMIC_RELAXED);
      if (local_RX_Message[0] == int('W') && prevWestMost_local && setUp_local) {
        uint8_t TX_Message[8] = {0};
        uint8_t position_local = __atomic_load_n(&position, __ATOMIC_RELAXED);
        uint8_t octave_local = __atomic_load_n(&octave, __ATOMIC_RELAXED);
        TX_Message[0] = int('W');
        TX_Message[1] = int('!');
        TX_Message[2] = octave_local;
        TX_Message[3] = position_local;
        __atomic_store_n(&prevWestMost, false ,__ATOMIC_RELAXED);
        __atomic_store_n(&position, position_local + 1, __ATOMIC_RELAXED);
        __atomic_store_n(&octave, octave_local + 1, __ATOMIC_RELAXED);
        xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
      }

      else if (local_RX_Message[0] == int('E') && prevEastMost_local && setUp_local) {
        uint8_t TX_Message[8] = {0};

        TX_Message[0] = int('E');
        TX_Message[1] = int('!');
        TX_Message[2] = __atomic_load_n(&octave, __ATOMIC_RELAXED);
        TX_Message[3] = __atomic_load_n(&position, __ATOMIC_RELAXED);
        __atomic_store_n(&prevEastMost, false ,__ATOMIC_RELAXED);
        xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
      }
    }
    
    if (local_RX_Message[1] == int('!')){
      westMost_local = __atomic_load_n(&westMost, __ATOMIC_RELAXED);
      eastMost_local = __atomic_load_n(&eastMost, __ATOMIC_RELAXED);
      prevWestMost_local = __atomic_load_n(&prevWestMost, __ATOMIC_RELAXED);
      prevEastMost_local = __atomic_load_n(&prevEastMost, __ATOMIC_RELAXED);
      setUp_local = __atomic_load_n(&setUp, __ATOMIC_RELAXED);

      int receivedOctave = local_RX_Message[2];
      int receivedPosition = local_RX_Message[3];

      if (local_RX_Message[0] == int('E') && !setUp_local && eastMost_local && !westMost_local) {
        __atomic_store_n(&octave, receivedOctave + 1, __ATOMIC_RELAXED);
        __atomic_store_n(&position, receivedPosition + 1, __ATOMIC_RELAXED);
        __atomic_store_n(&prevWestMost, false ,__ATOMIC_RELAXED);
        __atomic_store_n(&setUp, true, __ATOMIC_RELAXED);
      }

      else if (local_RX_Message [0] == int('W') && !setUp_local && westMost_local && !eastMost_local) {
        __atomic_store_n(&octave, receivedOctave - 1, __ATOMIC_RELAXED);
        __atomic_store_n(&position, 1, __ATOMIC_RELAXED);
        __atomic_store_n(&prevEastMost, false ,__ATOMIC_RELAXED);
        __atomic_store_n(&setUp, true, __ATOMIC_RELAXED);
      }
    }

    if (local_RX_Message[0] == int('O') && !westMost_local && setUp) {
      uint8_t newOctave = local_RX_Message[1];
      __atomic_store_n(&octave, newOctave + position - 1, __ATOMIC_RELAXED);
    }


    for (int i = 0; i < 8; i++) {
      __atomic_store_n(&RX_Message[i], local_RX_Message[i], __ATOMIC_RELAXED);
    }
  }  
}
 
void CAN_TX_Task (void * pvParameters) {
	uint8_t msgOut[8];
	while (1) {
		xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
		xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
		CAN_TX(uniqueID, msgOut);
	}
}

void CAN_TX_ISR (void) {
	xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL);
}

void setup() {
  // put your setup code here, to run once:
  msgInQ = xQueueCreate(36,8);
  msgOutQ = xQueueCreate(384,8); //12 runs of the task
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3,3);

  //Set pin directions
  pinMode(RA0_PIN, OUTPUT);
  pinMode(RA1_PIN, OUTPUT);
  pinMode(RA2_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUTL_PIN, OUTPUT);
  pinMode(OUTR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(C0_PIN, INPUT);
  pinMode(C1_PIN, INPUT);
  pinMode(C2_PIN, INPUT);
  pinMode(C3_PIN, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  //Initialise display
  setOutMuxBit(DRST_BIT, LOW);  //Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH);  //Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH);  //Enable display power supply

  #ifndef DISABLE_ISR
  sampleTimer.setOverflow(22000, HERTZ_FORMAT);
  sampleTimer.attachInterrupt(setISR);
  sampleTimer.resume();
  #endif

  //Initialise UART
  Serial.begin(9600);
  delay(2000);
  Serial.println("Hello World");

  #ifndef DISABLE_ISR
  CAN_Init(false);
  CAN_RegisterRX_ISR(CAN_RX_ISR);
  CAN_RegisterTX_ISR(CAN_TX_ISR);
  // setCANFilter(0x123,0x7ff);
  setCANFilter(6,0);
  CAN_Start();
  #endif
  
  sysState.mutex = xSemaphoreCreateMutex();
  TaskHandle_t scanKeysHandle = NULL;

  #ifndef DISABLE_THREADS
  xTaskCreate(
  scanKeysTask,		/* Function that implements the task */
  "scanKeys",		/* Text name for the task */
  256,      		/* Stack size in words, not bytes */
  NULL,			/* Parameter passed into the task */
  1,			/* Task priority */
  &scanKeysHandle);	/* Pointer to store the task handle */

  TaskHandle_t displayUpdateHandle = NULL;
  xTaskCreate(
  displayUpdateTask,		/* Function that implements the task */
  "displayUpdate",		/* Text name for the task */
  256,      		/* Stack size in words, not bytes */
  NULL,			/* Parameter passed into the task */
  1,			/* Task priority */
  &displayUpdateHandle);	/* Pointer to store the task handle */

  TaskHandle_t decodeHandle = NULL;
  xTaskCreate(
  decodeTask,		/* Function that implements the task */
  "decode",		/* Text name for the task */
  256,      		/* Stack size in words, not bytes */
  NULL,			/* Parameter passed into the task */
  1,			/* Task priority */
  &decodeHandle);	/* Pointer to store the task handle */
  
  TaskHandle_t CAN_TX_Handle = NULL;
  xTaskCreate(
  CAN_TX_Task,		/* Function that implements the task */
  "CAN_TX",		/* Text name for the task */
  256,      		/* Stack size in words, not bytes */
  NULL,			/* Parameter passed into the task */
  1,			/* Task priority */
  &CAN_TX_Handle);	/* Pointer to store the task handle */
  vTaskStartScheduler();
  #endif // Disable threads

  #ifdef TEST_SCANKEYS
	uint32_t startTime = micros();
  Serial.println(startTime);
	for (int iter = 0; iter < 32; iter++) {
		scanKeysTask(NULL);
	}
	Serial.println(micros()-startTime);
	while(1);
  #endif
}

void loop() {
}