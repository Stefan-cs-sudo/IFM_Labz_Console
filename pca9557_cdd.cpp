#include "Arduino.h"
#include "pca9557_cdd.h"

#define COM_BUFFER_SIZE 2

bool PCA_initialize(uint8_t IcAddress) {
  Wire.begin();
  PCA_setConfiguration(IcAddress, 0x00);

  uint8_t I2C_data[COM_BUFFER_SIZE] = { 0x01, 0x00 };
  Wire.beginTransmission(IcAddress);
  Wire.write(I2C_data, COM_BUFFER_SIZE);
  Wire.endTransmission();

  return true;
}

void PCA_setConfiguration(uint8_t IcAddress, uint8_t ConfigMask) {
  // Configuration register and register value
  uint8_t I2C_data[COM_BUFFER_SIZE] = { 0x03, 0x00 };
  I2C_data[1] &= ConfigMask;

  Wire.beginTransmission(IcAddress);
  Wire.write(I2C_data, COM_BUFFER_SIZE);
  Wire.endTransmission();
}

void PCA_enablePin(uint8_t IcAddress, uint8_t IcPin) {
  if (IcPin > 7) return;

  /* Read current output register value */
  Wire.beginTransmission(IcAddress);
  Wire.write(0x01);
  Wire.endTransmission(false);
  Wire.requestFrom(IcAddress, (uint8_t)1);
  uint8_t current = Wire.available() ? Wire.read() : 0x00;

  /* Set the requested pin bit */
  current |= (1 << IcPin);

  uint8_t I2C_data[COM_BUFFER_SIZE] = { 0x01, current };
  Wire.beginTransmission(IcAddress);
  Wire.write(I2C_data, COM_BUFFER_SIZE);
  Wire.endTransmission();
}

void PCA_disablePin(uint8_t IcAddress, uint8_t IcPin) {
  if (IcPin > 7) return;

  /* Read current output register value */
  Wire.beginTransmission(IcAddress);
  Wire.write(0x01);
  Wire.endTransmission(false);
  Wire.requestFrom(IcAddress, (uint8_t)1);
  uint8_t current = Wire.available() ? Wire.read() : 0x00;

  /* Clear the requested pin bit */
  current &= ~(1 << IcPin);

  uint8_t I2C_data[COM_BUFFER_SIZE] = { 0x01, current };
  Wire.beginTransmission(IcAddress);
  Wire.write(I2C_data, COM_BUFFER_SIZE);
  Wire.endTransmission();
}
