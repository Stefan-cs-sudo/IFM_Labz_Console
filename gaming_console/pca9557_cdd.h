#ifndef PCA9557_h
#define PCA9557_h

#include "Arduino.h"
#include <Wire.h>

bool PCA_initialize(uint8_t IcAddress);

void PCA_setConfiguration(uint8_t IcAddress, uint8_t ConfigMask);

void PCA_enablePin(uint8_t IcAddress, uint8_t IcPin);

void PCA_disablePin(uint8_t IcAddress, uint8_t IcPin);

#endif
