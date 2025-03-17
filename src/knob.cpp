#include "Knob.h"
#include <algorithm>
#include <STM32FreeRTOS.h>
  
Knob::Knob(uint8_t _no, uint8_t _minVal, uint8_t _maxVal, uint8_t _start) 
    : no(_no), min(_minVal), max(_maxVal), rotation(_start) {  // Member initializer list
    previousStateA = 0;
    previousStateB = 0;
}

uint8_t Knob::clamp(uint8_t r){
    return std::max(std::min(r, max), min);
}


void Knob::storeValue(uint8_t r){
    __atomic_store_n(&rotation, r, __ATOMIC_RELEASE);
}

void Knob::updateValues(uint8_t currentStateA, uint8_t currentStateB){
    if(previousStateB == currentStateB && previousStateA != currentStateA){ //if B stays the same and A flips
        if(previousStateB == previousStateA){ //if prev state B = A
            storeValue(clamp(rotation + 1));
            lastIncrement = 1;
        }
        else{
            storeValue(clamp(rotation - 1));
            lastIncrement = -1;
        }
    }
    else{   //If B flips
        if(previousStateA != currentStateA){ //if A flips
            storeValue(clamp(rotation + lastIncrement));  
        }
    }
    previousStateA = currentStateA;
    previousStateB = currentStateB;
}