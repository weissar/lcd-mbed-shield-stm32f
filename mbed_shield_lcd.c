/**
 * Include common core function, must contain setGPIO, setAF, GPIOWrite
 */
#include "stm_core.h"

#include "mbed_shield_lcd.h"

/**
 * Used SPI channel
 */
#define _MBED_LCD_SPI         SPI1
/**
 * Alternate Funcion number for GPIO AF signals (SCK and MOSI)
 */
#define _MBED_LCD_SPI_AF_NUM  5
/**
 * Definition of GPIO ports for concrete control signals
 */
#define _MBED_LCD_SPI_SCK     GPIOA,5       ///< SPI clock
#define _MBED_LCD_SPI_MOSI    GPIOA,7       ///< SPI MOSI signal
#define _MBED_LCD_PIN_RSTN    GPIOA,6       ///< display RST signal, active in LO
#define _MBED_LCD_PIN_CSN     GPIOB,6       ///< display CS signal, active in LO
#define _MBED_LCD_PIN_A0      GPIOA,8       ///< display A0 signal, LO = commands, HI = data
/**
 * Differencies between platforms
 * !! not completed, can emit error
 */
#if defined(STM32F4)
#define SPI_IS_BUSY(SPIx) (((SPIx)->SR & (SPI_SR_TXE | SPI_SR_RXNE)) == 0 || ((SPIx)->SR & SPI_SR_BSY))
//TODO #elseif another platform, for example - F103: SPIx->SR & SPI_SR_BSY
#else
#error Not supported platform
#endif

/**
 * Physical dimensions of LCD, controler can support 132x64 max
 */
#define _MBED_LCD_COLUMNS   128           ///< Horizontal pixels count
#define _MBED_LCD_ROWS      32            ///< Vertical pixels couns
#define _MBED_LCD_LINES     (_MBED_LCD_ROWS / 8)              ///< Count of 8x8 chars horizontaly
#define _MBED_LCD_CHAR_PER_LINE     (_MBED_LCD_COLUMNS / 8)   ///< Count of 8x8 chars verticaly

/**
 * Returns count of pixel horizontaly
 */
uint8_t MBED_LCD_GetColumns(void)
{
  return _MBED_LCD_COLUMNS;
}

/**
 * Return count of pixel verticaly
 */
uint8_t MBED_LCD_GetRows(void)
{
  return _MBED_LCD_ROWS;
}

/**
 * Returns count of text rows with default fonr 8x8
 */
uint8_t MBED_LCD_GetLines(void)
{
  return _MBED_LCD_LINES;
}

/**
 * Returns count of text columns with default fonr 8x8
 */
uint8_t MBED_LCD_GetCharPerLine(void)
{
  return _MBED_LCD_CHAR_PER_LINE;
}

/**
 *  Private memory buffer
 *  Reuqired LINES * COLUMNS bytes at BSS segment
 */
static uint8_t m_videoRam[_MBED_LCD_LINES][_MBED_LCD_COLUMNS];

/**
 * Private funcions
 */
static void MBED_LCD_send(uint8_t val, bool a0)       ///< Write single value to LCD - using SPI, A0 selects CMD = 0, DATA = 1
{
  GPIOWrite(_MBED_LCD_PIN_A0, a0 ? 1 : 0);
  GPIOWrite(_MBED_LCD_PIN_CSN, 0);

  _MBED_LCD_SPI->DR = val;
  while(SPI_IS_BUSY(_MBED_LCD_SPI))                   // waiting is different fo F4xx and another Fxxx
    ;                                                 // blocking waiting

  GPIOWrite(_MBED_LCD_PIN_CSN, 1);
}

static void MBED_LCD_sendData(uint8_t *val, uint16_t len)   ///< Write block of data, pointer to start and length
{
  GPIOWrite(_MBED_LCD_PIN_A0, 1);      // always data
  GPIOWrite(_MBED_LCD_PIN_CSN, 0);

  for(; len; len--)
  {
    _MBED_LCD_SPI->DR = *val;
    val++;

    while(_MBED_LCD_SPI->SR & SPI_SR_BSY)               // blocking wait
      ;
  }

  GPIOWrite(_MBED_LCD_PIN_CSN, 1);
}

static bool MBED_LCD_reset(void)                        ///< Perform reset sequence
{
  uint16_t w, x = 0;

  GPIOWrite(_MBED_LCD_PIN_A0, 0);
  GPIOWrite(_MBED_LCD_PIN_RSTN, 0);

  for(w = 0; w < 10000; w++)
    x++;                                                // Dummy increment prevents optimalisation

  GPIOWrite(_MBED_LCD_PIN_RSTN, 1);

  for(w = 0; w < 1000; w++)
    x++;

  return (x > 0);                                       // Trick to keep vaiable unoptimalised ...
}

static void MBED_LCD_set_start_line(uint8_t x)          ///< Send command to LCD, info from DS
{
  MBED_LCD_send(0x10 | ((x & 0xf0) >> 4), 0);           // (2) Display start line set = Sets the display RAM display start lineaddress - lower 6 bits
  MBED_LCD_send(0x00 | (x & 0x0f), 0);                  // (2) Display start line set = Sets the display RAM display start lineaddress - lower 6 bits
}

static void MBED_LCD_set_page(uint8_t p)                ///< Send command to LCD, info from DS
{
   MBED_LCD_send(0xB0 | (p & 0x0f), 0);                 // (3) Page address set = Sets the display RAM page address - lower 4 bits
}

static bool MBED_LCD_init_hw()                          ///< Init SPI, GPIO, ...
{
  STM_SetPinGPIO(_MBED_LCD_PIN_RSTN, ioPortOutputPP);
  GPIOWrite(_MBED_LCD_PIN_RSTN, 1);
  STM_SetPinGPIO(_MBED_LCD_PIN_CSN, ioPortOutputPP);
  GPIOWrite(_MBED_LCD_PIN_CSN, 1);
  STM_SetPinGPIO(_MBED_LCD_PIN_A0, ioPortOutputPP);

  STM_SetPinGPIO(_MBED_LCD_SPI_MOSI, ioPortAlternatePP);
  STM_SetAFGPIO(_MBED_LCD_SPI_MOSI, _MBED_LCD_SPI_AF_NUM);        // AFxx
  STM_SetPinGPIO(_MBED_LCD_SPI_SCK, ioPortAlternatePP);
  STM_SetAFGPIO(_MBED_LCD_SPI_SCK, _MBED_LCD_SPI_AF_NUM);         // AFxx

  switch((uint32_t)_MBED_LCD_SPI)                       // Switch reuired due using ohter APBx for some SPIx
  {
    case (uint32_t)SPI1:
      if (!(RCC->APB2ENR & RCC_APB2ENR_SPI1EN))
      {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
        RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
        RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
      }
      break;
    //TODO other SPIx peripheral
    default:
      return false;
  }

  _MBED_LCD_SPI->CR1 = 0
      | SPI_CR1_CPHA | SPI_CR1_CPOL     // polarity from DS
      | SPI_CR1_SSI | SPI_CR1_SSM       // required for correct function
      | SPI_CR1_MSTR;
  _MBED_LCD_SPI->CR2 = 0;

  {                                     // from DS - max clock 10MHz (100ns period)
    uint32_t apb2 = SystemCoreClock;    //TODO calculate from RCC
    uint32_t BRDiv = 0;                 // 000 = pclk / 2

    if (apb2 > 20e6) BRDiv = 0x01;      // 001 = pclk / 4
    if (apb2 > 40e6) BRDiv = 0x02;      // 010 = pclk / 8
    if (apb2 > 80e6) BRDiv = 0x03;      // 011 = pclk / 16

    _MBED_LCD_SPI->CR1 &= ~SPI_CR1_BR;
    //  only for F4xx:
    _MBED_LCD_SPI->CR1 |= (BRDiv & 0x07) << 3;    // isolate 3 bits and set to bits 5..3
    //TODO another platforms
  }

  _MBED_LCD_SPI->CR1 |= SPI_CR1_SPE;             // enable
  return true;
}

/**
 * Area of public functions
 */

/**
 * Fill videoram with value, bytes = columns, MSB on top
 */
void MBED_LCD_InitVideoRam(uint8_t val)
{
  for(int r = 0; r < _MBED_LCD_LINES; r++)      // repaired 2019-09-23
    for(int x = 0; x < _MBED_LCD_COLUMNS; x++)
      m_videoRam[r][x] = val;
}

/**
 * Initialisation - HW parts and init commands for LCD controller (see DS and MBED sample init code)
 * Returns false if ini fails
 */
bool MBED_LCD_init(void)
{
  if (!MBED_LCD_init_hw())  // check success of HW init
    return false;

  MBED_LCD_reset();

  MBED_LCD_send(0xAE, 0);   //  display off
  MBED_LCD_send(0xA2, 0);   //  bias voltage

  MBED_LCD_send(0xA0, 0);
  MBED_LCD_send(0xC8, 0);   //  colum normal

  MBED_LCD_send(0x22, 0);   //  voltage resistor ratio
  MBED_LCD_send(0x2F, 0);   //  power on
  //wr_cmd(0xA4);   //  LCD display ram
  MBED_LCD_send(0x40, 0);   // start line = 0
  MBED_LCD_send(0xAF, 0);     // display ON

  MBED_LCD_send(0x81, 0);   //  set contrast
  MBED_LCD_send(0x17, 0);   //  set contrast

  MBED_LCD_send(0xA6, 0);     // display normal
//  MBED_LCD_send(0xA7, 0);     // display inverted

//  MBED_LCD_send(0xa5, 0);
  return true;                // ALL init OK
}

#include "font_8x8.h"       ///< Font defintion, 128 characters with ASCII codes 0..127

/**
 * Writes 8x8 character at position
 * Return false if coordinates are outside working area
 */
bool MBED_LCD_WriteCharXY(char c, uint8_t col, uint8_t row)
{
  int i;

  if ((col > _MBED_LCD_COLUMNS / 8) || (row > (_MBED_LCD_ROWS - 1)))
    return false;

  if (c > 127)
    c %= 128;

  for (i = 0; i < 8; i++)
    m_videoRam[row][col * 8 + i] = font8x8_basic[c * 8 + i];

  return true;
}

/**
 * Writes string of 8x8 character at position
 * Return false if coordinates of first chracter are outside working area
 * TODO chceking max. length on line
 */
bool MBED_LCD_WriteStringXY(char *cp, uint8_t col, uint8_t row)
{
  if ((col > _MBED_LCD_COLUMNS / 8) || (row > (_MBED_LCD_ROWS - 1)))
    return false;

  for (; *cp; cp++)
  {
    MBED_LCD_WriteCharXY(*cp, col, row);
    col++;
  }

  return true;
}

/**
 * Copying content of videoRAM to LCD controller, based on SPI bulk transfer
 */
void MBED_LCD_VideoRam2LCD()
{
  for(uint8_t r = 0; r < _MBED_LCD_LINES; r++)
  {
    MBED_LCD_set_page(r);
    MBED_LCD_set_start_line(0);

#if 1
    MBED_LCD_sendData(m_videoRam[r], _MBED_LCD_COLUMNS);      // block operation
#else
    for(uint8_t x = 0; x < _MBED_LCD_COLUMNS; x++)
      MBED_LCD_send(m_videoRam[r][x], 1);
#endif
  }
}

/**
 * Puts pixel with color black = 1, background = 0
 * TODO check valied coordinates
 */
void MBED_LCD_PutPixel(uint8_t x, uint8_t y, bool black)
{
  //TODO check x < 128 and y < 32
  if (black)
    m_videoRam[y / 8][x] |= 1 << (y % 8);
  else
    m_videoRam[y / 8][x] &= ~(1 << (y % 8));
}

/**
 * Draw line with color black = 1, background = 0
 * Coordinates x,y of start point a x,y of end point
 * Bresenham algorithm used (see rosetacode.org)
 * TODO check valied coordinates
 */
void MBED_LCD_DrawLine(int x0, int y0, int x1, int y1, bool color)
{
  int dx = (x0 < x1) ? (x1 - x0) : (x0 - x1), sx = (x0 < x1) ? 1 : -1;
  int dy = (y0 < y1) ? (y1 - y0) : (y0 - y1), sy = (y0 < y1) ? 1 : -1;
  int err = ((dx > dy) ? dx : -dy) / 2, e2;

  for (; ; )
  {
    if ((x0 == x1) && (y0 == y1))
      break;

    MBED_LCD_PutPixel((uint8_t)x0, (uint8_t)y0, color);

    e2 = err;
    if (e2 > -dx)
    {
      err -= dy; x0 += sx;
    }

    if (e2 < dy)
    {
      err += dx; y0 += sy;
    }
  }
}

/**
 * Draw lines as rectangle with color black = 1, background = 0
 * TODO check valied coordinates
 */
void MBED_LCD_DrawRect(int x, int y, int w, int h, bool color)
{
  MBED_LCD_DrawLine(x, y, x + w, y, color);
  MBED_LCD_DrawLine(x + w, y, x + w, y + h, color);
  MBED_LCD_DrawLine(x + w, y + h, x, y + h, color);
  MBED_LCD_DrawLine(x, y + h, x, y, color);
}

/**
 * Draw lines as filled rectangle with color black = 1, background = 0
 * TODO check valied coordinates
 */
void MBED_LCD_FillRect(int x, int y, int w, int h, bool color)
{
  int ww = w, xx = x;

  for(; h; h--)
  {
    x = xx;

    for(w = ww; w; w--)
    {
      MBED_LCD_PutPixel(x, y, color);
      x++;
    }

    y++;
  }
}

/**
 * Draw circle with color black = 1, background = 0
 * Algorithm see rosetacode.org
 * Checks valied coordinates for clipping at display margins
 */
void MBED_LCD_DrawCircle(int centerX, int centerY, int radius, bool colorSet)
{
  int d = (5 - radius * 4) / 4;
  int x = 0;
  int y = radius;

  do
  {
    // ensure index is in range before setting (depends on your image implementation)
    // in this case we check if the pixel location is within the bounds of the image before setting the pixel
    if (((centerX + x) >= 0) && ((centerX + x) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY + y) >= 0) && ((centerY + y) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX + x, centerY + y, colorSet);
    if (((centerX + x) >= 0) && ((centerX + x) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY - y) >= 0) && ((centerY - y) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX + x, centerY - y, colorSet);
    if (((centerX - x) >= 0) && ((centerX - x) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY + y) >= 0) && ((centerY + y) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX - x, centerY + y, colorSet);
    if (((centerX - x) >= 0) && ((centerX - x) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY - y) >= 0) && ((centerY - y) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX - x, centerY - y, colorSet);
    if (((centerX + y) >= 0) && ((centerX + y) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY + x) >= 0) && ((centerY + x) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX + y, centerY + x, colorSet);
    if (((centerX + y) >= 0) && ((centerX + y) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY - x) >= 0) && ((centerY - x) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX + y, centerY - x, colorSet);
    if (((centerX - y) >= 0) && ((centerX - y) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY + x) >= 0) && ((centerY + x) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX - y, centerY + x, colorSet);
    if (((centerX - y) >= 0) && ((centerX - y) <= (_MBED_LCD_COLUMNS - 1)) && ((centerY - x) >= 0) && ((centerY - x) <= (_MBED_LCD_ROWS - 1))) MBED_LCD_PutPixel(centerX - y, centerY - x, colorSet);
    if (d < 0)
    {
      d += 2 * x + 1;
    }
    else
    {
      d += 2 * (x - y) + 1;
      y--;
    }
    x++;
  } while (x <= y);
}

/**
 * Draw lines as filled circle with color black = 1, background = 0
 * TODO check valied coordinates
 */
void MBED_LCD_FillCircle(int x0, int y0, int radius, bool color)
{
  int f = 1 - radius;
  int ddF_x = 0;
  int ddF_y = -2 * (int)radius;
  int x = 0;
  int y = (int)radius;
  int t1, t2;

  MBED_LCD_PutPixel(x0, y0 + radius, color);
  t1 = y0 - radius;
  MBED_LCD_PutPixel(x0, (t1 > 0) ? t1 : 0, color);
  t1 = x0 - radius;
  MBED_LCD_DrawLine(x0 + radius, y0, (t1 > 0) ? t1 : 0, y0, color);

  while(x < y)
  {
    if(f >= 0)
    {
        y--;
        ddF_y += 2;
        f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x + 1;

    t1 = x0 - x;
    t2 = y0 - y;
    MBED_LCD_DrawLine((t1 > 0) ? t1 : 0, y0 + y, x0 + x, y0 + y, color);
    MBED_LCD_DrawLine((t1 > 0) ? t1 : 0, (t2 > 0) ? t2 : 0, x0 + x, (t2 > 0) ? t2 : 0, color);
    t1 = x0 - y;
    t2 = y0 - x;
    MBED_LCD_DrawLine((t1 > 0) ? t1 : 0, y0 + x, x0 + y, y0 + x, color);
    MBED_LCD_DrawLine((t1 > 0) ? t1 : 0, (t2 > 0) ? t2 : 0, x0 + y, (t2 > 0) ? t2 : 0, color);
  }
}
