#include "LcdUtils.h"
#include "SD.h"
#include "Fonts/FreeMono9pt7b.h"

/* Static functions declarations */

static uint16_t read16(File f);
static uint32_t read32(File f);
static void printLineSmall(const char *line, uint16_t color);
static void printLineLarge(const char *line, uint16_t color);

/* END OF static functions declarations */

/*!
 * \brief data structuire instance storing the configuration 
 *        of the LcdUtils module. */
LcdUtils_config_t LcdUtilCfg = {
  NULL,
  0,
  FONT_DEFAULT
};

/*! 
 * \brief initialization function of the LcdUtils module.
 * \param *LcdTft is a reference to an already initialized 
 *        Adafruit_ST7735 object. */
  LcdUtilCfg.Lcd = LcdTft;
}

/*!
 * \brief function used to draw a BMP file on the display controlled 
 *        using the Adafruit_ST7735 reference object sent over the 
          initialization file. 
 * \param *filename is a reference to a string containing the path on the SD card 
          to the BMP file to be draw on the display.
 * \param x is the display's line number to start drawing on - the x coordinate.
 * \param y is the display's column number to start drawing on - the y coordinate. 
   \note this function must only be called after LcdUtils_init has already been called.
   \note source: https://simple-circuit.com/draw-bmp-images-arduino-sd-card-st7735/ */
void LcdUtils_bmpDraw(char *filename, uint8_t x, uint16_t y) {

  File bmpFile;
  int bmpWidth, bmpHeight;
  uint8_t bmpDepth;
  uint32_t bmpImageoffset;
  uint32_t rowSize;
  boolean goodBmp = false;
  boolean flip = true;
  int w, h;
  uint32_t startTime = millis();

  if (LcdUtilCfg.Lcd == NULL) return;
  if ((x >= LcdUtilCfg.Lcd->width()) || (y >= LcdUtilCfg.Lcd->height())) return;

  char pathBuf[64];
  if (filename[0] != '/') {
    pathBuf[0] = '/';
    strncpy(&pathBuf[1], filename, sizeof(pathBuf) - 2);
    pathBuf[sizeof(pathBuf) - 1] = '\0';
  } else {
    strncpy(pathBuf, filename, sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = '\0';
  }

  bmpFile = SD.open(pathBuf);
  if (!bmpFile) {
    Serial.print(F("File not found: "));
    Serial.println(pathBuf);
    return;
  }

  if (read16(bmpFile) == 0x4D42) {
    (void)read32(bmpFile);               // file size
    (void)read32(bmpFile);               // creator bytes
    bmpImageoffset = read32(bmpFile);
    (void)read32(bmpFile);               // header size
    bmpWidth = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) {
      bmpDepth = read16(bmpFile);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) {
        goodBmp = true;
        rowSize = (bmpWidth * 3 + 3) & ~3;

        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
        }

        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= LcdUtilCfg.Lcd->width())  w = LcdUtilCfg.Lcd->width()  - x;
        if ((y + h - 1) >= LcdUtilCfg.Lcd->height()) h = LcdUtilCfg.Lcd->height() - y;

        // Allocate heap buffers for entire image
        uint32_t rawSize = (uint32_t)rowSize * bmpHeight;
        uint8_t  *rawBuf = (uint8_t *)malloc(rawSize);
        uint16_t *pixBuf = (uint16_t *)malloc((uint32_t)w * h * 2);

        if (rawBuf && pixBuf) {
          // One bulk SD read — eliminates 96 seeks
          bmpFile.seek(bmpImageoffset);
          bmpFile.read(rawBuf, rawSize);
          bmpFile.close();

          // Convert BGR → RGB565 in display order
          for (int row = 0; row < h; row++) {
            int srcRow = flip ? (bmpHeight - 1 - row) : row;
            uint8_t  *rp = rawBuf + (uint32_t)srcRow * rowSize;
            uint16_t *dp = pixBuf + (uint32_t)row * w;
            for (int col = 0; col < w; col++) {
              dp[col] = ((rp[col * 3 + 2] & 0xF8) << 8)
                      | ((rp[col * 3 + 1] & 0xFC) << 3)
                      | ( rp[col * 3]              >> 3);
            }
          }

          // Single DMA blast to LCD
          LcdUtilCfg.Lcd->startWrite();
          LcdUtilCfg.Lcd->setAddrWindow(x, y, w, h);
          LcdUtilCfg.Lcd->writePixels(pixBuf, (uint32_t)w * h);
          LcdUtilCfg.Lcd->endWrite();
        } else {
          Serial.println(F("malloc failed"));
          bmpFile.close();
        }

        if (rawBuf) free(rawBuf);
        if (pixBuf) free(pixBuf);

        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(F(" ms"));
      }
    }
  }
  if (!goodBmp) {
    bmpFile.close();
    Serial.println(F("BMP format not recognized."));
  }
}

/*!
 * \brief function used to clear the device display 
 * \note the function must be called after LcdUtils_init 
 *       has already been called */
void LcdUtils_clearScreen(void) {
  if (LcdUtilCfg.Lcd != NULL) {
    LcdUtilCfg.Lcd->fillScreen(ST77XX_BLACK);
    LcdUtilCfg.LcdX = 0;
    LcdUtilCfg.LcdY = 0;
    LcdUtilCfg.TextFont = FONT_DEFAULT;
  }
}

/*!
 * \brief function used to clear the line on the device display
 * \note this function does not update the lcd x and lcd y coordinates, this means that it is possible to write over other lines
 */
void LcdUtils_clearLineAt(int x, int y, SupportedFonts_t LastFontUsed) {
  uint8_t height;
  if (LastFontUsed == FONT_FREE_MONO_9PT) {
    height = 17;
  } else {
    height = 9;
  }

  if (LcdUtilCfg.Lcd != NULL) {
    LcdUtilCfg.Lcd->fillRect(x, y, 128, height, ST7735_BLACK);
  }
}

/*!
 * \brief function used to print a string on a line on the display.
 * \param *line is a reference to a string that is required to be printed on the display.
 * \param Color is the color to be used for the writing of the string on the display, 
          as defined in the header file of this module.
 * \param Font indicates one of the two Font options available, 
          as defind by the SupportedFonts_t enumeration data type. 
 * \note the function must be called after LcdUtils_init 
 *       has already been called
 * \note if the string is longer than the display length, the function will not print the extra characters. */
void LcdUtils_printLine(const char *line, uint16_t TextColor, SupportedFonts_t Font) {
  if (LcdUtilCfg.Lcd != NULL) {
    if (Font == FONT_FREE_MONO_9PT) {
      printLineLarge(line, TextColor);
    } else {
      printLineSmall(line, TextColor);
    }
  }
}

/*!
 * \brief function used to set the cursor on the display.
 * \param x horizontal position (bigger the nnumber, more at the right part of the screen)
 * \param y vertical position (bigger the number, more at the bottom at the screen)
 * \note the 0 0 coordinates are at the top left corner, so you should use just positive numbers :)
  */
void LcdUtils_setCursor(int x, int y) {
  LcdUtilCfg.LcdX = x;
  LcdUtilCfg.LcdY = y;
}

void LcdUtils_getCursorPosition(int *x,int *y)
{
  uint8_t x_loc = LcdUtilCfg.LcdX;
  uint8_t y_loc = LcdUtilCfg.LcdY;

  *x = x_loc;
  *y = y_loc;
}

/*!
 * \brief internal static function used to print a large font string on the line of the display, 
 *        using the FreeMono9pt7b font.
 * \param *line is a reference to a string that is required to be printed on the display.
 * \param TextColor is the color to be used for the writing of the string on the display, 
          as defined in the header file of this module.
 * \note  the function must be called after LcdUtils_init 
 *        has already been called
 * \note  if the string is longer than the display length, the function will not print the extra characters. */
static void printLineLarge(const char *line, uint16_t TextColor) {
    LcdUtilCfg.LcdY = LcdUtilCfg.LcdY + 13;
  LcdUtilCfg.Lcd->setTextColor(TextColor);
  LcdUtilCfg.Lcd->setFont(&FreeMono9pt7b);
  LcdUtilCfg.Lcd->setCursor(LcdUtilCfg.LcdX, LcdUtilCfg.LcdY);
  LcdUtilCfg.Lcd->println(line);
  LcdUtilCfg.LcdY = LcdUtilCfg.LcdY + 4;
  LcdUtilCfg.TextFont = FONT_FREE_MONO_9PT;
}

/*!
 * \brief internal static function used to print a small font string on the line of the display, 
 *        using the default font of the Adafruit_ST7735 library.
 * \param *line is a reference to a string that is required to be printed on the display.
 * \param color is the color to be used for the writing of the string on the display, 
          as defined in the header file of this module.
 * \note  the function must be called after LcdUtils_init 
 *        has already been called
 * \note  if the string is longer than the display length, the function will not print the extra characters. */
static void printLineSmall(const char *line, uint16_t TextColor) {
  if (LcdUtilCfg.TextFont == FONT_FREE_MONO_9PT) {
    LcdUtilCfg.LcdY = LcdUtilCfg.LcdY + 9;
  }

  LcdUtilCfg.Lcd->setTextColor(TextColor);
  LcdUtilCfg.Lcd->setFont();
  LcdUtilCfg.Lcd->setCursor(LcdUtilCfg.LcdX, LcdUtilCfg.LcdY);
  LcdUtilCfg.Lcd->println(line);

  LcdUtilCfg.LcdY = LcdUtilCfg.LcdY + 9;

  LcdUtilCfg.TextFont = FONT_DEFAULT;
}

/*!
 * \brief internal static function used to read the next 16 bits from a File object.
   \param f is the file object reference to read the data from. 
   \return the next 16 bits of data read from the file f.
   \note source: https://simple-circuit.com/draw-bmp-images-arduino-sd-card-st7735/ */
static uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();  // MSB
  return result;
}

/*!
 * \brief internal static function used to read the next 16 bits from a File object.
   \param f is the file object reference to read the data from. 
   \return the next 16 bits of data read from the file f.
   \note source: https://simple-circuit.com/draw-bmp-images-arduino-sd-card-st7735/ */
static uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();  // MSB
  return result;
}
