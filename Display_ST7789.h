#pragma once
#include <Arduino.h>
#include <SPI.h>
#define LCD_WIDTH   240 //LCD width
#define LCD_HEIGHT  240 //LCD height

#define SPIFreq                        80000000
#define EXAMPLE_PIN_NUM_MISO           5
#define EXAMPLE_PIN_NUM_MOSI           6
#define EXAMPLE_PIN_NUM_SCLK           7
#define EXAMPLE_PIN_NUM_LCD_CS         14
#define EXAMPLE_PIN_NUM_LCD_DC         15
#define EXAMPLE_PIN_NUM_LCD_RST        21
#define EXAMPLE_PIN_NUM_BK_LIGHT       22
#define Frequency       1000     
#define Resolution      10       

#define VERTICAL   0
#define HORIZONTAL 1

#define Offset_X 0
#define Offset_Y 0

// 旋转时的偏移量 (1.3寸 240x240 ST7789)
static uint16_t rot_offset_x = 0;
static uint16_t rot_offset_y = 0;


void LCD_Init(void);
void LCD_SetCursor(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend);
void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t* color);
void LCD_WriteCommand(uint8_t Cmd);
void LCD_WriteData(uint8_t Data);
void LCD_WriteData_Word(uint16_t Data);

void Backlight_Init(void);
void Set_Backlight(uint8_t Light);
void LCD_SetRotation(uint8_t rotation);
