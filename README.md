# lcd-mbed-shield-stm32f
Part of library for LCD on MBED Application Shield applied to Nucleo STM32F

Using sample:
<pre>
#include "mbed_shield_lcd.h"        // include public functions
...
int main(void)
{
...
  if (!MBED_LCD_init())             // check success
  {
    while(1)                        // defined stop when it fails
      ;
  }

  MBED_LCD_InitVideoRam(0x00);      // fill content with 0 = clear memory buffer

  sprintf(buf, "Disp: %02dx%02d pix", MBED_LCD_GetColumns(), MBED_LCD_GetRows());
  MBED_LCD_WriteStringXY(buf, 0, 2);    // example string output

  MBED_LCD_VideoRam2LCD();          // move changes in video buffer to LCD

...
  MBED_LCD_DrawCircle(80, 16, 8, true);     // sample drawing
  MBED_LCD_DrawCircle(96, 16, 12, true);

  MBED_LCD_VideoRam2LCD();         // move changes in video buffer to LCD
</pre>
