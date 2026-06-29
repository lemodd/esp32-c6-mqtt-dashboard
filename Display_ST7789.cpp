#include "Display_ST7789.h"
   
#define SPI_WRITE(_dat)         SPI.transfer(_dat)
#define SPI_WRITE_Word(_dat)    SPI.transfer16(_dat)
void SPI_Init()
{
  SPI.begin(EXAMPLE_PIN_NUM_SCLK,EXAMPLE_PIN_NUM_MISO,EXAMPLE_PIN_NUM_MOSI); 
}

void LCD_WriteCommand(uint8_t Cmd)  
{ 
  SPI.beginTransaction(SPISettings(SPIFreq, MSBFIRST, SPI_MODE0));
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, LOW);  
  digitalWrite(EXAMPLE_PIN_NUM_LCD_DC, LOW); 
  SPI_WRITE(Cmd);
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, HIGH);  
  SPI.endTransaction();
}
void LCD_WriteData(uint8_t Data) 
{ 
  SPI.beginTransaction(SPISettings(SPIFreq, MSBFIRST, SPI_MODE0));
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, LOW);  
  digitalWrite(EXAMPLE_PIN_NUM_LCD_DC, HIGH);  
  SPI_WRITE(Data);  
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, HIGH);  
  SPI.endTransaction();
}    
void LCD_WriteData_Word(uint16_t Data)
{
  SPI.beginTransaction(SPISettings(SPIFreq, MSBFIRST, SPI_MODE0));
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, LOW);  
  digitalWrite(EXAMPLE_PIN_NUM_LCD_DC, HIGH); 
  SPI_WRITE_Word(Data);
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, HIGH);  
  SPI.endTransaction();
}   
void LCD_WriteData_nbyte(uint8_t* SetData,uint8_t* ReadData,uint32_t Size) 
{ 
  SPI.beginTransaction(SPISettings(SPIFreq, MSBFIRST, SPI_MODE0));
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, LOW);  
  digitalWrite(EXAMPLE_PIN_NUM_LCD_DC, HIGH);  
  SPI.transferBytes(SetData, ReadData, Size);
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, HIGH);  
  SPI.endTransaction();
} 

void LCD_Reset(void)
{
  digitalWrite(EXAMPLE_PIN_NUM_LCD_CS, LOW);       
  delay(50);
  digitalWrite(EXAMPLE_PIN_NUM_LCD_RST, LOW); 
  delay(50);
  digitalWrite(EXAMPLE_PIN_NUM_LCD_RST, HIGH); 
  delay(50);
}
void LCD_Init(void)
{
  pinMode(EXAMPLE_PIN_NUM_LCD_CS, OUTPUT);
  pinMode(EXAMPLE_PIN_NUM_LCD_DC, OUTPUT);
  pinMode(EXAMPLE_PIN_NUM_LCD_RST, OUTPUT); 
  Backlight_Init();
  SPI_Init();

  LCD_Reset();
  //************* Start Initial Sequence **********// 
  LCD_WriteCommand(0x11);
  delay(120);
  LCD_WriteCommand(0x36);
  if (HORIZONTAL)
      LCD_WriteData(0x00);
  else
      LCD_WriteData(0x70);

  LCD_WriteCommand(0x3A);
  LCD_WriteData(0x05);

  LCD_WriteCommand(0xB0);
  LCD_WriteData(0x00);
  LCD_WriteData(0xE8);
  
  LCD_WriteCommand(0xB2);
  LCD_WriteData(0x0C);
  LCD_WriteData(0x0C);
  LCD_WriteData(0x00);
  LCD_WriteData(0x33);
  LCD_WriteData(0x33);

  LCD_WriteCommand(0xB7);
  LCD_WriteData(0x35);

  LCD_WriteCommand(0xBB);
  LCD_WriteData(0x35);

  LCD_WriteCommand(0xC0);
  LCD_WriteData(0x2C);

  LCD_WriteCommand(0xC2);
  LCD_WriteData(0x01);

  LCD_WriteCommand(0xC3);
  LCD_WriteData(0x13);

  LCD_WriteCommand(0xC4);
  LCD_WriteData(0x20);

  LCD_WriteCommand(0xC6);
  LCD_WriteData(0x0F);

  LCD_WriteCommand(0xD0);
  LCD_WriteData(0xA4);
  LCD_WriteData(0xA1);

  LCD_WriteCommand(0xD6);
  LCD_WriteData(0xA1);

  LCD_WriteCommand(0xE0);
  LCD_WriteData(0xF0);
  LCD_WriteData(0x00);
  LCD_WriteData(0x04);
  LCD_WriteData(0x04);
  LCD_WriteData(0x04);
  LCD_WriteData(0x05);
  LCD_WriteData(0x29);
  LCD_WriteData(0x33);
  LCD_WriteData(0x3E);
  LCD_WriteData(0x38);
  LCD_WriteData(0x12);
  LCD_WriteData(0x12);
  LCD_WriteData(0x28);
  LCD_WriteData(0x30);

  LCD_WriteCommand(0xE1);
  LCD_WriteData(0xF0);
  LCD_WriteData(0x07);
  LCD_WriteData(0x0A);
  LCD_WriteData(0x0D);
  LCD_WriteData(0x0B);
  LCD_WriteData(0x07);
  LCD_WriteData(0x28);
  LCD_WriteData(0x33);
  LCD_WriteData(0x3E);
  LCD_WriteData(0x36);
  LCD_WriteData(0x14);
  LCD_WriteData(0x14);
  LCD_WriteData(0x29);
  LCD_WriteData(0x32);

  LCD_WriteCommand(0x21);

  LCD_WriteCommand(0x11);
  delay(120);
  LCD_WriteCommand(0x29); 
}
/******************************************************************************
function: Set the cursor position
parameter :
    Xstart:   Start uint16_t x coordinate
    Ystart:   Start uint16_t y coordinate
    Xend  :   End uint16_t coordinates
    Yend  :   End uint16_t coordinatesen
******************************************************************************/
void LCD_SetCursor(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t  Yend)
{ 
  if (HORIZONTAL) {
    // set the X coordinates
    LCD_WriteCommand(0x2A);
    LCD_WriteData(Xstart >> 8);
    LCD_WriteData(Xstart + rot_offset_x);
    LCD_WriteData(Xend >> 8);
    LCD_WriteData(Xend + rot_offset_x);
    
    // set the Y coordinates
    LCD_WriteCommand(0x2B);
    LCD_WriteData(Ystart >> 8);
    LCD_WriteData(Ystart + rot_offset_y);
    LCD_WriteData(Yend >> 8);
    LCD_WriteData(Yend + rot_offset_y);
  }
  else {
    // set the X coordinates
    LCD_WriteCommand(0x2A);
    LCD_WriteData(Ystart >> 8);
    LCD_WriteData(Ystart + rot_offset_y);
    LCD_WriteData(Yend >> 8);
    LCD_WriteData(Yend + rot_offset_y);
    // set the Y coordinates
    LCD_WriteCommand(0x2B);
    LCD_WriteData(Xstart >> 8);
    LCD_WriteData(Xstart + rot_offset_x);
    LCD_WriteData(Xend >> 8);
    LCD_WriteData(Xend + rot_offset_x);
  }
  LCD_WriteCommand(0x2C);
}
/******************************************************************************
function: Refresh the image in an area
parameter :
    Xstart:   Start uint16_t x coordinate
    Ystart:   Start uint16_t y coordinate
    Xend  :   End uint16_t coordinates
    Yend  :   End uint16_t coordinates
    color :   Set the color
******************************************************************************/
void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,uint16_t* color)
{          
  // uint16_t i,j;
  // LCD_SetCursor(Xstart, Ystart, Xend,Yend);
  // uint16_t Show_Width = Xend - Xstart + 1;
  // uint16_t Show_Height = Yend - Ystart + 1;
  // for(i = 0; i < Show_Height; i++){               
  //   for(j = 0; j < Show_Width; j++){
  //     LCD_WriteData_Word(color[(i*(Show_Width))+j]);                           
  //   }
  // }           
  uint16_t Show_Width = Xend - Xstart + 1;
  uint16_t Show_Height = Yend - Ystart + 1;
  uint32_t numBytes = Show_Width * Show_Height * sizeof(uint16_t);
  uint8_t Read_D[numBytes];
  LCD_SetCursor(Xstart, Ystart, Xend, Yend);
  LCD_WriteData_nbyte((uint8_t*)color, Read_D, numBytes);        
}
// backlight
void Backlight_Init(void)
{
  ledcAttach(EXAMPLE_PIN_NUM_BK_LIGHT, Frequency, Resolution);   
  ledcWrite(EXAMPLE_PIN_NUM_BK_LIGHT, 100);                        
}

void Set_Backlight(uint8_t Light)                        //
{

  if(Light > 100 || Light < 0)
    printf("Set Backlight parameters in the range of 0 to 100 \r\n");
  else{
    uint32_t Backlight = Light*10;
    ledcWrite(EXAMPLE_PIN_NUM_BK_LIGHT, Backlight);
  }
}

/******************************************************************************
function: Set screen rotation
parameter :
    rotation: 0=0°, 1=90°, 2=180°, 3=270°
******************************************************************************/
void LCD_SetRotation(uint8_t rotation) {
  Serial.printf("LCD_SetRotation: %d\n", rotation);
  LCD_WriteCommand(0x36);
  switch (rotation) {
    case 0: LCD_WriteData(0x00); Serial.println("MADCTL: 0x00 (0°)"); break;
    case 1: LCD_WriteData(0x70); Serial.println("MADCTL: 0x70 (90°)"); break;
    case 2: LCD_WriteData(0x02); Serial.println("MADCTL: 0x02 (180°)"); break;
    case 3: LCD_WriteData(0x06); Serial.println("MADCTL: 0x06 (270°)"); break;
    default: LCD_WriteData(0x00); Serial.println("MADCTL: 0x00 (default)"); break;
  }
  delay(10);
}





