/* Sharp_MIP Sharp Memory Display Driver code
 * Sharp_MIP
.c
 *
 *  Created on: Nov 26, 2020
 *      Author: TinLethax (Thipok Jiamjarapan)
 *      email : thipok17@gmail.com
 */
#include "Sharp_MIP.h"
#include "font8x8_basic.h"
#include <string.h>
#include <stdlib.h>

//Display Commands
uint8_t clearCMD[2] = {0x20,0x00}; // Display Clear 0x04 (HW_LSB)
uint8_t printCMD[2] = {0x80,0x00}; // Display Bitmap (after issued display update) 0x01 (HW_LSB)


//This buffer holds 50 Bytes * 240 Row = 12K of Display buffer
uint8_t *DispBuf;// entire display buffer.

//This buffer holds temporary 2 Command bytes
static uint8_t SendBuf[2];

//This buffer holds 1 Character bitmap image (8x8)
static uint8_t chBuf[8];

//These Vars required for print function
static uint8_t YLine = 1;
static uint8_t Xcol = 1;

uint8_t smallRbit(uint8_t re){
	return (uint8_t)(__RBIT(re) >> 24);
}

// Display Initialization
void LCD_Init(Sharp_MIP *MemDisp, SPI_HandleTypeDef *Bus,
		GPIO_TypeDef *dispGPIO,uint16_t LCDcs){

	//Store params into our struct
	MemDisp->Bus = Bus;
	MemDisp->dispGPIO = dispGPIO;
	MemDisp->LCDcs = LCDcs;


	DispBuf = malloc(BUFFER_LENGTH);
	memset(DispBuf, 0xFF, BUFFER_LENGTH);



	//At lease 3 + 13 clock is needed for Display clear (16 Clock = 8x2 bit = 2 byte)
	HAL_GPIO_WritePin(MemDisp->dispGPIO,MemDisp->LCDcs,GPIO_PIN_SET);
	HAL_SPI_Transmit(MemDisp->Bus, (uint8_t *)clearCMD, 2,150); //According to Datasheet
	HAL_GPIO_WritePin(MemDisp->dispGPIO,MemDisp->LCDcs,GPIO_PIN_RESET);


}

// Display update (Transmit data)
void LCD_Update(Sharp_MIP *MemDisp){
	SendBuf[0] = printCMD[0]; // M0 High, M2 Low
	HAL_GPIO_WritePin(MemDisp->dispGPIO,MemDisp->LCDcs,GPIO_PIN_SET);// Begin

	for(uint8_t count = 0 ;count < DISPLAY_HEIGHT+1;count++){
	SendBuf[1] = smallRbit(count+1);// counting from row number 1 to row number 240
	//row to DispBuf offset
	uint16_t offset = count * LINE_WIDTH;

	HAL_SPI_Transmit(MemDisp->Bus, SendBuf, 2, 150);
	HAL_SPI_Transmit(MemDisp->Bus, DispBuf+offset, LINE_WIDTH, 150);
	}
	//Send the Dummies bytes after whole display data transmission
	HAL_SPI_Transmit(MemDisp->Bus, 0x00,2,150);

	HAL_GPIO_WritePin(MemDisp->dispGPIO,MemDisp->LCDcs,GPIO_PIN_RESET);// Done
}

//Clean the Buffer
void LCD_BufClean(void){
	YLine = 1;
	Xcol = 1;
	memset(DispBuf, 0xFF, BUFFER_LENGTH);
}

// Clear entire Display
void LCD_Clean(Sharp_MIP *MemDisp){
	YLine = 1;
	Xcol = 1;
		//At lease 3 + 13 clock is needed for Display clear (16 Clock = 8x2 bit = 2 byte)
		HAL_GPIO_WritePin(MemDisp->dispGPIO,MemDisp->LCDcs,GPIO_PIN_SET);
		HAL_SPI_Transmit(MemDisp->Bus, (uint8_t *)clearCMD, 2,150); //According to Datasheet
		HAL_GPIO_WritePin(MemDisp->dispGPIO,MemDisp->LCDcs,GPIO_PIN_RESET);

}

// Buffer update (full 400*240 pixels)
void LCD_LoadFull(uint8_t * BMP){
	/*for(uint16_t l; l < 12000; l++){
		DispBuf[l] = (uint8_t)(__RBIT(BMP[l]) >> 24);
	}*/
	memcpy(DispBuf, BMP, BUFFER_LENGTH);
}

// Buffer update (with X,Y Coordinate and image WxH) X,Y Coordinate start at (1,1) to (50,240)
//
//NOTE THAT THE X COOR and WIDTH ARE BYTE NUMBER NOT PIXEL NUMBER (8 pixel = 1 byte). A.K.A IT'S BYTE ALIGNED
//
void LCD_LoadPart(uint8_t* BMP, uint8_t Xcord, uint8_t Ycord, uint8_t bmpW, uint8_t bmpH){

	Xcord = Xcord - 1;
	Ycord = Ycord - 1;
	uint16_t XYoff,WHoff = 0;

	//Counting from Y origin point to bmpH using for loop
	for(uint8_t loop = 0; loop < bmpH; loop++){
		// turn X an Y into absolute offset number for Buffer
		XYoff = (Ycord+loop) * LINE_WIDTH;
		XYoff += Xcord;// offset start at the left most, The count from left to right for Xcord times

		// turn W and H into absolute offset number for Bitmap image
		WHoff = loop * bmpW;

		memcpy(DispBuf + XYoff, BMP + WHoff, bmpW);
	}

}

/* FIXME TODO */
//Similar to LCD_LoadPart, but x,y coordinate are BOTH PIXEL position.
void LCD_LoadPix(uint8_t* BMP, uint16_t Xcord, uint8_t Ycord, uint16_t bmpW, uint8_t bmpH){
	if ((bmpW > DISPLAY_WIDTH) | (Xcord >DISPLAY_WIDTH) | (Ycord > DISPLAY_HEIGHT) | (bmpH > DISPLAY_HEIGHT)) return;

	Xcord = Xcord - 1;
	Ycord = Ycord - 1;


	//bmpW = (uint8_t)(bmpW / 8);

	//Shifting value to align the pixel with the byte
	uint8_t Shiftval = (uint8_t)(Xcord % 8);

	//Counting from Y origin point to bmpH using for loop
	for(uint8_t loop = 0; loop < bmpH; loop++){
		// turn X an Y into absolute offset number for Buffer
		uint16_t XYoff = ((Ycord+loop) * LINE_WIDTH)   + (uint8_t)(Xcord/8)  + (Xcord % 8 ? 1 : 0);

		// turn W and H into absolute offset number for Bitmap image
		uint16_t WHoff = (loop * (uint8_t)(bmpW/8) )  + (bmpW % 8 ? 1 : 0);

		// Byte Filling
		for (uint16_t i=0;i < (bmpW/8) ;i ++){
			DispBuf[i+XYoff] |= (BMP[WHoff + i] >> Shiftval) ;
			DispBuf[i+XYoff+1] = (BMP[WHoff+i] << (7 - Shiftval));
		}

	}
}


//Invert color of Display memory buffer
void LCD_Invert(void){
	uint16_t invt = BUFFER_LENGTH;
	do{
	invt--;
	DispBuf[invt] = ~DispBuf[invt];
	}while(invt);
}

//Fill screen with either black or white color
void LCD_Fill(bool fill){
	memset(DispBuf, (fill ? 0 : 0xFF) , BUFFER_LENGTH);
	HAL_Delay(10);
}


//Print 8x8 Text on screen
void LCD_Print(char txtBuf[], size_t len){

uint16_t strLen = len;
uint16_t chOff = 0;

for (uint16_t p = 0; p < strLen;p++){
	// In case of reached 50 chars or newline detected , Do the newline
	if ((Xcol > LINE_WIDTH) || *txtBuf == 0x0A){
		Xcol = 1;// Move cursor to most left
		YLine += 8;// enter new line
		txtBuf++;// move to next char
	}

	// Avoid printing Newline
	if (*txtBuf != 0x0A){

	chOff = (*txtBuf - 0x20) * 8;// calculate char offset (fist 8 pixel of character)

	for(uint8_t i=0;i < 8;i++){// Copy the inverted color px to buffer
	chBuf[i] = smallRbit(~font8x8_basic[i + chOff]);
	}

	LCD_LoadPart((uint8_t *)chBuf, Xcol, YLine, 1, 8);// Align the char with the 8n pixels

	txtBuf++;// move to next char
	Xcol++;// move cursor to next column
	
	}
  }
}
