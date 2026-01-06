
#ifndef INC_SHARP_MIP_H_
#define INC_SHARP_MIP_H_

#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_tim.h"
#include <stdbool.h>





#define DISPLAY_WIDTH 144
#define DISPLAY_HEIGHT 168
#define BUFFER_LENGTH 3024
#define LINE_WIDTH 18
#define DISPLAY_ROTATION 0



/*##### 	DISPLAY DRIVER FUNCTIONS 	 ########

LCD_Init //			 Display Init
LCD_Clean //		 Display Clear
LCD_LoadFull // 	 Load Full Screen data onto buffer mem
LCD_LoadPart // 	 Load specific bitmap to buffer mem with X,Y coordinate (Byte aligned)
LCD_LoadPix // 		 load specific bitmap to buffer mem with X,Y coordinate (Pixel aligned)
LCD_Print // 		 Print string with 8x8 font
LCD_Invert // 		 Invert Color of all pixels in buffer mem
LCD_BufClean // 	 Set all byte in buffer to 0xFF (appear as white on Display when LCD_Update)
LCD_Fill // 		 true : fill with black, false : fill with white
LCD_Update // 		 Transmit buffer to display memory


#################################################*/




// This typedef holds the hardware parameters. For GPIO and SPI
typedef struct {
	SPI_HandleTypeDef 	*Bus;
	GPIO_TypeDef 		*dispGPIO;
	uint16_t 			 LCDcs;

}Sharp_MIP;


void LCD_Init(Sharp_MIP *MemDisp, SPI_HandleTypeDef *Bus,
		GPIO_TypeDef *dispGPIO,uint16_t LCDcs);
void LCD_Clean(Sharp_MIP *MemDisp);
void LCD_Update(Sharp_MIP *MemDisp);
void LCD_LoadFull(uint8_t * BMP);
void LCD_LoadPart(uint8_t* BMP, uint8_t Xcord, uint8_t Ycord, uint8_t bmpW, uint8_t bmpH);
void LCD_LoadPix(uint8_t* BMP, uint16_t Xcord, uint8_t Ycord, uint16_t bmpW, uint8_t bmpH);
void LCD_Print(char txtBuf[],size_t len);
void LCD_BufClean(void);
void LCD_Invert(void);
void LCD_Fill(bool fill);
void testDisplay(Sharp_MIP *MemDisp);

#endif /* INC_SHARP_MIP_H_ */
