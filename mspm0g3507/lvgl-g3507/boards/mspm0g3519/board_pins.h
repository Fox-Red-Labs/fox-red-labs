/* LP-MSPM0G3519 pins that differ from the G3507:
 * reset and backlight on port C, touch I2C2 on PC3 (SDA) and PC2 (SCL), touch reset on PB11. */
#define BOARD_NAME "MSPM0G3519"
#define LCD_RST_PORT      PC_PORT
#define LCD_RST_PIN_MASK  PC_LCD_RST_PIN
#define LCD_BL_PORT       PC_PORT
#define LCD_BL_PIN_MASK   PC_LCD_BL_PIN
#define TP_RST_PORT       PB_PORT
#define TP_RST_PIN_MASK   PB_TP_RST_PIN
#define TP_GPIO           GPIOC
#define TP_SDA_MASK       DL_GPIO_PIN_3
#define TP_SCL_MASK       DL_GPIO_PIN_2
