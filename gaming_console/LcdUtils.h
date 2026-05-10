#ifndef _LCD_UTILS_H_
#define _LCD_UTILS_H_

#include "Adafruit_ST7735.h"

/* defines for the collors supported by the LcdUtils module */
#define BLACK  ST77XX_BLACK
#define WHITE  ST77XX_WHITE
#define RED ST77XX_RED
#define GREEN  ST77XX_GREEN
#define BLUE ST77XX_BLUE
#define CYAN ST77XX_CYAN
#define MAGENTA  ST77XX_MAGENTA
#define YELLOW ST77XX_YELLOW
#define ORANGE ST77XX_ORANGE
#define ST77XX_GRAY    0x7BEF  /* medium gray (R=120, G=120, B=120) */
#define ST77XX_DGRAY   0x4208  /* dark gray   (R=64,  G=64,  B=64)  */
#define ST77XX_LGRAY   0xC618  /* light gray  (R=192, G=192, B=192) */

/* define for the pixels buffer length (full row for speed) */
#define BUFFPIXEL 128

/*!
 * \brief enumeration data type for the fonts supported
          by the LcdUtils module */
typedef enum
{
	FONT_DEFAULT,
	FONT_FREE_MONO_9PT
}SupportedFonts_t;

/*! 
 * \brief data structure type for the configuration 
          and control of the LcdUtils module. */
typedef struct 
{
	Adafruit_ST7735* Lcd;      /*!< referenece to the display object                 */
	uint8_t LcdX;              /*!< index of the last written line on the display    */
  uint8_t LcdY;
	SupportedFonts_t TextFont; /*!< last used font for writing a line on the display */
}LcdUtils_config_t;

void LcdUtils_init(Adafruit_ST7735* LcdTft);
void LcdUtils_bmpDraw(char *filename, uint8_t x, uint16_t y);
void LcdUtils_clearScreen(void);
void LcdUtils_printLine(const char*line, uint16_t TextColor, SupportedFonts_t Font);

void LcdUtils_setCursor(int x, int y);
void LcdUtils_getCursorPosition(int *x,int *y);
void LcdUtils_clearLineAt(int x, int y,SupportedFonts_t LastFontUsed);

#endif
