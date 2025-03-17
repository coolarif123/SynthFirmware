#ifndef KNOB_H
#define KNOB_H

#include <STM32FreeRTOS.h>
class Knob{
    public:
        uint8_t rotation;
        uint8_t max; 
        uint8_t min; 
        uint8_t lastIncrement = 0;
        uint8_t previousStateA;
        uint8_t previousStateB;
        uint8_t no;

        Knob(uint8_t _no, uint8_t _minVal, uint8_t _maxVal, uint8_t _start);
        uint8_t clamp(uint8_t r);
        void storeValue(uint8_t r);        
        void updateValues(uint8_t currentStateA, uint8_t currentStateB);
};

#endif