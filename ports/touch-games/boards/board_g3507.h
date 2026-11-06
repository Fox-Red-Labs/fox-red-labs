/* LP-MSPM0G3507 + Waveshare 3.5 inch. Wires: the wiring table on the board page.
 * SPI1: SCLK PB9, MOSI PB8, MISO PB7. CS PB6, DC PB1, RST PB4, BL PA28. Touch (software I2C): SDA PB3, SCL PB2, RST PB0. */
#define BOARD_NAME "g3507"
#define LCD_CS_PORT PB_PORT
#define LCD_CS_PIN  PB_LCD_CS_PIN
#define LCD_DC_PORT PB_PORT
#define LCD_DC_PIN  PB_LCD_DC_PIN
#define LCD_RST_PORT PB_PORT
#define LCD_RST_PIN  PB_LCD_RST_PIN
#define LCD_BL_PORT PA_PORT
#define LCD_BL_PIN  PA_LCD_BL_PIN
#define TP_RST_PORT PB_PORT
#define TP_RST_PIN  PB_TP_RST_PIN
#define TP_GPIO     GPIOB
#define TP_SDA_MASK DL_GPIO_PIN_3
#define TP_SCL_MASK DL_GPIO_PIN_2
