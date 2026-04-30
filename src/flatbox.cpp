#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "bsp/board.h"
#include "tusb.h"
#include "hardware/gpio.h"
#define _USE_MATH_DEFINES

int input_mode = 0;
int macro_mode = 1;
int pro_mode = 1;
int led_mode = 0;
int dpad_mode = 1;
bool prevXLstick = false;
bool prevYLstick = false;
bool prevcup = false;
bool TILTE = false;
bool SJ = false;
bool EightBottuns = false;
bool prevEight = false;
bool moving = false;
bool moving2 = false;
bool Release1 = false;
bool Release2 = false;
bool Buffer1 = false;
bool Buffer2 = false;

int Dragontime = 0;
int NAtime = 0;
int NBtime = 0;
int NIL1time = 0;
int NIL2time = 0;
int cooltime = 0;
int CT = 260;
int prevSUM = 0;

int up_angle = -5; //0 ~ 90
int down_angle = -5; //0 ~ 90
int half_angle = 23; //0 ~ 45
double tilt_abs_value = 64.5; //(0 ~ 127) 64.5 is the value of Double Up Zip
int hold_timeX = 120;
int hold_timeY = 250;
int prev_x_value = 0;
int prev_y_value = 0;
int x_i = 0; //左右どちらかに入力され続けた回数(0でリセット)
int y_i = 0;
int Changetime = 0;

// ##### multicore ##################################### START
#include "pico/multicore.h"
// #######################  ############################## END

// ##### Flash ######################################### START
#include <hardware/flash.h>

//busy_wait_us_32(500);0.5ms
//sleep_ms(5);5ms
static void save_setting_to_flash(int input_mode, int macro_mode, int dpad_mode, int pro_mode)
{
    // W25Q16JVの最終ブロック(Block31)のセクタ0の先頭アドレス = 0x1F0000
    const uint32_t FLASH_TARGET_OFFSET = 0x1F0000;
    // W25Q16JVの書き込み最小単位 = FLASH_PAGE_SIZE(256Byte)
    // FLASH_PAGE_SIZE(256Byte)はflash.hで定義済
    uint8_t write_data[FLASH_PAGE_SIZE];

    // 保存データのセット
    write_data[0] = input_mode;
    write_data[1] = macro_mode;
    write_data[2] = dpad_mode;
    write_data[3] = pro_mode;

    // 割り込み無効にする
    uint32_t ints = save_and_disable_interrupts();
    // Flash消去。
    //  消去単位はflash.hで定義されている FLASH_SECTOR_SIZE(4096Byte) の倍数とする
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    // Flash書き込み。
    //  書込単位はflash.hで定義されている FLASH_PAGE_SIZE(256Byte) の倍数とする
    flash_range_program(FLASH_TARGET_OFFSET, write_data, FLASH_PAGE_SIZE);
    // 割り込みフラグを戻す
    restore_interrupts(ints);
}

uint8_t g_read_data[4];

void load_setting_from_flash(void)
{
    g_read_data[0] = 0;
    g_read_data[1] = 0;
    g_read_data[2] = 0;
    g_read_data[3] = 0;
    // W25Q16JVの最終ブロック(Block31)のセクタ0の先頭アドレス = 0x1F0000
    const uint32_t FLASH_TARGET_OFFSET = 0x1F0000;
    // XIP_BASE(0x10000000)はflash.hで定義済み
    const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

    g_read_data[0] = flash_target_contents[0];
    g_read_data[1] = flash_target_contents[1];
    g_read_data[2] = flash_target_contents[2];
    g_read_data[3] = flash_target_contents[3];
}
// ##################################################### END

// ##### neopixel ###################################### START
#include "pico/stdio.h"
#include "pico/time.h"
#include "Adafruit_NeoPixel.hpp"

#define PIN 25
#define DEBUG_MODE 1

Adafruit_NeoPixel strip = Adafruit_NeoPixel(128, PIN, NEO_GRB + NEO_KHZ800);

void delay(uint32_t ms) {
	sleep_ms(ms);
};

uint32_t Wheel(uint8_t WheelPos);
void colorWipe(uint32_t c, uint8_t wait);
void rainbow(uint8_t wait);
void rainbowCycle(uint8_t wait);

uint32_t Wheel(uint8_t WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}
// ##################################################### END

// ##### multicore ##################################### START
void main_core1(void) {
    multicore_lockout_victim_init();

    // ##### neopixel ##### START
    strip.begin();
    strip.setBrightness(50);
    strip.show(); // Initialize all pixels to 'off'
    // #################### END

    // ##### oled ##### START
    /* static uint8_t ucBuffer[1024];
    uint8_t uc[8];
    int i, j, rc;
    char szTemp[32];
    picoSSOLED myOled(OLED_128x64, 0x3c, 0, 0, PICO_I2C, SDA_PIN, SCL_PIN, I2C_SPEED);

    rc = myOled.init() ;
    myOled.set_back_buffer(ucBuffer);
    */
    // ################ END

    while (1) {
        // ##### neopixel ##### START
        switch (led_mode) {
            case 0:
                rainbow(20);
                break;
            case 1:
                rainbowCycle(20);
                break;
            case 2:
                colorWipe(strip.Color(127, 127, 127), 50); // White
                break;
            default:
                rainbow(20);
        }
        // #################### END

        // ##### oled ##### START
        //if (rc != OLED_NOT_FOUND) {
            /* myOled.fill(0,1);
            //myOled.set_contrast(127);
            myOled.write_string(0,0,0,(char *)"**************** ", FONT_8x8, 0, 1);
            if (input_mode == 1) {
                myOled.write_string(0,0,1,(char *)"TYPE:SWITCH", FONT_8x8, 0, 1);
            } else {
                myOled.write_string(0,0,1,(char *)"TYPE:PS3/PC", FONT_8x8, 0, 1);
            }
            if (stick_mode == 1) {
                myOled.write_string(0,0,2,(char *)"STICK:LEFT STICK", FONT_8x8, 0, 1);
            } else {
                myOled.write_string(0,0,2,(char *)"STICK:DPAD", FONT_8x8, 0, 1);
            }
            switch (led_mode) {
                case 0:
                    myOled.write_string(0,0,3,(char *)"LED:RAINBOW", FONT_8x8, 0, 1);
                case 1:
                    myOled.write_string(0,0,3,(char *)"LED:RAINBOWCYCLE", FONT_8x8, 0, 1);
                case 2:
                    myOled.write_string(0,0,3,(char *)"LED:WHITE", FONT_8x8, 0, 1);
                default:
                    myOled.write_string(0,0,3,(char *)"LED:RAINBOW", FONT_8x8, 0, 1);
            }
            sleep_ms(500); */
        //}
        // ################ END
    }
}
// ##################################################### END

// These IDs are bogus. If you want to distribute any hardware using this,
// you will have to get real ones.
#define USB_VID 0x0F0D //0xCAFE -> 0x0F0D
#define USB_PID 0x0092 //0xBABA -> 0x0092

#ifdef FLATBOX_REV4

#define PIN_UP        15
#define PIN_DOWN      27
#define PIN_LEFT      28
#define PIN_RIGHT     26
#define PIN_A         5
#define PIN_B         7
#define PIN_X         2
#define PIN_Y         6
#define PIN_L1        4
#define PIN_L2        1
#define PIN_R1        3
#define PIN_R2        0
#define PIN_TILTE     29
#define PIN_START     13
#define PIN_SJ        14
#define PIN_NA        10
#define PIN_CUP       9
#define PIN_CDOWN     11
#define PIN_CLEFT     12
#define PIN_CRIGHT    8

#endif

#ifdef FLATBOX_REV5

#define PIN_UP        15
#define PIN_DOWN      27
#define PIN_LEFT      28
#define PIN_RIGHT     26
#define PIN_A         5
#define PIN_B         7
#define PIN_X         2
#define PIN_Y         6
#define PIN_L1        4
#define PIN_L2        1
#define PIN_R1        3
#define PIN_R2        0
#define PIN_TILTE     29
#define PIN_START     13
#define PIN_SJ        14
#define PIN_NA        10
#define PIN_CUP       9
#define PIN_CDOWN     11
#define PIN_CLEFT     12
#define PIN_CRIGHT    8

#endif

uint32_t pin_mask = 1 << PIN_UP | 1 << PIN_DOWN | 1 << PIN_LEFT | 1 << PIN_RIGHT | 1 << PIN_A | 1 << PIN_B | 1 << PIN_X | 1 << PIN_Y | 1 << PIN_L1 | 1 << PIN_L2 | 1 << PIN_R1 | 1 << PIN_R2 | 1 << PIN_TILTE | 1 << PIN_START | 1 << PIN_SJ | 1 << PIN_NA | 1 << PIN_CUP | 1 << PIN_CDOWN | 1 << PIN_CLEFT | 1 << PIN_CRIGHT;

static inline uint remap_pin_for_pro_mode(uint pin) {
    if (pro_mode == 1) {
        if (pin == PIN_DOWN) return PIN_LEFT;
        if (pin == PIN_LEFT) return PIN_DOWN;
    }
    return pin;
}

static inline bool flatbox_gpio_get(uint pin) {
    return gpio_get(remap_pin_for_pro_mode(pin));
}

#define gpio_get(pin) flatbox_gpio_get(pin)

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,

    .bNumConfigurations = 0x01
};

// HID report descriptor taken from a HORI Fighting Stick V3
// with feature and output bits omitted
// works out of the box with PC and PS3
// as dumped by usbhid-dump and parsed by https://eleccelerator.com/usbdescreqparser/
uint8_t const ps3_desc_hid_report[] = {
    0x05, 0x01,                 // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                 // Usage (Game Pad)
    0xA1, 0x01,                 // Collection (Application)
    0x15, 0x00,                 //   Logical Minimum (0)
    0x25, 0x01,                 //   Logical Maximum (1)
    0x35, 0x00,                 //   Physical Minimum (0)
    0x45, 0x01,                 //   Physical Maximum (1)
    0x75, 0x01,                 //   Report Size (1)
    0x95, 0x0D,                 //   Report Count (13)
    0x05, 0x09,                 //   Usage Page (Button)
    0x19, 0x01,                 //   Usage Minimum (0x01)
    0x29, 0x0D,                 //   Usage Maximum (0x0D)
    0x81, 0x02,                 //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x03,                 //   Report Count (3)
    0x81, 0x01,                 //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,                 //   Usage Page (Generic Desktop Ctrls)
    0x25, 0x07,                 //   Logical Maximum (7)
    0x46, 0x3B, 0x01,           //   Physical Maximum (315)
    0x75, 0x04,                 //   Report Size (4)
    0x95, 0x01,                 //   Report Count (1)
    0x65, 0x14,                 //   Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,                 //   Usage (Hat switch)
    0x81, 0x42,                 //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,                 //   Unit (None)
    0x95, 0x01,                 //   Report Count (1)
    0x81, 0x01,                 //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x26, 0xFF, 0x00,           //   Logical Maximum (255)
    0x46, 0xFF, 0x00,           //   Physical Maximum (255)
    0x09, 0x30,                 //   Usage (X)
    0x09, 0x31,                 //   Usage (Y)
    0x09, 0x32,                 //   Usage (Z)
    0x09, 0x35,                 //   Usage (Rz)
    0x75, 0x08,                 //   Report Size (8)
    0x95, 0x04,                 //   Report Count (4)
    0x81, 0x02,                 //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,           //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20,                 //   Usage (0x20)
    0x09, 0x21,                 //   Usage (0x21)
    0x09, 0x22,                 //   Usage (0x22)
    0x09, 0x23,                 //   Usage (0x23)
    0x09, 0x24,                 //   Usage (0x24)
    0x09, 0x25,                 //   Usage (0x25)
    0x09, 0x26,                 //   Usage (0x26)
    0x09, 0x27,                 //   Usage (0x27)
    0x09, 0x28,                 //   Usage (0x28)
    0x09, 0x29,                 //   Usage (0x29)
    0x09, 0x2A,                 //   Usage (0x2A)
    0x09, 0x2B,                 //   Usage (0x2B)
    0x95, 0x0C,                 //   Report Count (12)
    0x81, 0x02,                 //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
/*
    0x0A, 0x21, 0x26,           //   Usage (0x2621)
    0x95, 0x08,                 //   Report Count (8)
    0xB1, 0x02,                 //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x0A, 0x21, 0x26,           //   Usage (0x2621)
    0x91, 0x02,                 //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
*/
    0x26, 0xFF, 0x03,           //   Logical Maximum (1023)
    0x46, 0xFF, 0x03,           //   Physical Maximum (1023)
    0x09, 0x2C,                 //   Usage (0x2C)
    0x09, 0x2D,                 //   Usage (0x2D)
    0x09, 0x2E,                 //   Usage (0x2E)
    0x09, 0x2F,                 //   Usage (0x2F)
    0x75, 0x10,                 //   Report Size (16)
    0x95, 0x04,                 //   Report Count (4)
    0x81, 0x02,                 //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                       // End Collection
};

// HID report descriptor for NINTENDO SWITCH
uint8_t const switch_desc_hid_report[] = {
    0x05, 0x01,       //   USAGE_PAGE (Generic Desktop)
    0x09, 0x05,       //   USAGE (Game Pad)
    0xa1, 0x01,       //   COLLECTION (Application)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
    0x35, 0x00,       //   PHYSICAL_MINIMUM (0)
    0x45, 0x01,       //   PHYSICAL_MAXIMUM (1)
    0x75, 0x01,       //   REPORT_SIZE (1)
    0x95, 0x10,       //   REPORT_COUNT (16)
    0x05, 0x09,       //   USAGE_PAGE (Button)
    0x19, 0x01,       //   USAGE_MINIMUM (1)
    0x29, 0x10,       //   USAGE_MAXIMUM (16)
    0x81, 0x02,       //   INPUT (Data,Var,Abs)
    0x05, 0x01,       //   USAGE_PAGE (Generic Desktop)
    0x25, 0x07,       //   LOGICAL_MAXIMUM (7)
    0x46, 0x3b, 0x01, //   PHYSICAL_MAXIMUM (315)
    0x75, 0x04,       //   REPORT_SIZE (4)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x65, 0x14,       //   UNIT (20)
    0x09, 0x39,       //   USAGE (Hat Switch)
    0x81, 0x42,       //   INPUT (Data,Var,Abs)
    0x65, 0x00,       //   UNIT (0)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x81, 0x01,       //   INPUT (Cnst,Arr,Abs)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM (255)
    0x46, 0xff, 0x00, //   PHYSICAL_MAXIMUM (255)
    0x09, 0x30,       //   USAGE (X)
    0x09, 0x31,       //   USAGE (Y)
    0x09, 0x32,       //   USAGE (Z)
    0x09, 0x35,       //   USAGE (Rz)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x95, 0x04,       //   REPORT_COUNT (4)
    0x81, 0x02,       //   INPUT (Data,Var,Abs)
    0x06, 0x00, 0xff, //   USAGE_PAGE (Vendor Defined 65280)
    0x09, 0x20,       //   USAGE (32)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x81, 0x02,       //   INPUT (Data,Var,Abs)
    0x0a, 0x21, 0x26, //   USAGE (9761)
    0x95, 0x08,       //   REPORT_COUNT (8)
    0x91, 0x02,       //   OUTPUT (Data,Var,Abs)
    0xc0              // END_COLLECTION
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID 0x81

uint8_t const ps3_desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(ps3_desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1)
};

uint8_t const switch_desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(switch_desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1)
};

char const *string_desc_arr[] = {
    (const char[]) {0x09, 0x04},        // 0: is supported language is English (0x0409)
    "Flatbox",                          // 1: Manufacturer
#ifdef FLATBOX_REV4
    "Flatbox rev4",                     // 2: Product(RP2040)
#endif
#ifdef FLATBOX_REV5
    "Flatbox rev5",                     // 2: Product(RP2040 Zero)
#endif
};

// HID report
typedef struct __attribute__ ((packed)) {
    uint16_t buttons;           // bits: 0 = square, 1 = cross, 2 = circle, 3 = triangle,
                                // 4 = L1, 5 = R1, 6 = L2, 7 = R2, 8 = select, 9 = start,
                                // 10 = L3, 11 = R3, 12 = PS
    uint8_t dpadHat;            // 0 = up, 1 = up/right, 2 = right etc., 0x0f = neutral
    uint8_t leftStickXAxis;
    uint8_t leftStickYAxis;
    uint8_t rightStickXAxis;
    uint8_t rightStickYAxis;
    uint8_t dpadRightAxis;
    uint8_t dpadLeftAxis;
    uint8_t dpadUpAxis;
    uint8_t dpadDownAxis;
    uint8_t triangleAxis;
    uint8_t circleAxis;
    uint8_t crossAxis;
    uint8_t squareAxis;
    uint8_t L1Axis;
    uint8_t L2Axis;
    uint8_t R1Axis;
    uint8_t R2Axis;
    uint16_t accelerometerXAxis;        // 10 bits (these are guesses BTW)
    uint16_t accelerometerYAxis;        // 10 bits
    uint16_t accelerometerZAxis;        // 10 bits
    uint16_t gyroscopeAxis;             // 10 bits
} hid_report_t;

hid_report_t report;
hid_report_t prevReport;

void cpad(bool cup, bool cdown, bool cleft, bool cright){
    if(cup && cdown) {
        cup = cdown = false;
    }
    if(cleft && cright) {
        cleft = cright = false;
    }
    if(gpio_get(PIN_NA) && cleft && cright && cup && cdown){
        report.dpadRightAxis = cright ? 0xff : 0x00;
        report.dpadLeftAxis = cleft ? 0xff : 0x00;
        report.dpadUpAxis = cup ? 0xff : 0x00;
        report.dpadDownAxis = cdown ? 0xff : 0x00;
        if (cup && !cright && !cleft)
            report.dpadHat = 0;
        else if (cup && cright)
            report.dpadHat = 1;
        else if (cright && !cup && !cdown)
            report.dpadHat = 2;
        else if (cright && cdown)
            report.dpadHat = 3;
        else if (cdown && !cright && !cleft)
            report.dpadHat = 4;
        else if (cdown && cleft)
            report.dpadHat = 5;
        else if (cleft && !cdown && !cup)
            report.dpadHat = 6;
        else if (cleft && cup)
            report.dpadHat = 7;
        else
            report.dpadHat = 0x0f;
        cleft = false;
        cright = false;
        cup = false;
        cdown = false;
    }
    else{
        report.dpadRightAxis = 0x00;
        report.dpadLeftAxis = 0x00;
        report.dpadUpAxis = 0x00;
        report.dpadDownAxis = 0x00;
        report.dpadHat = 0x0f;
    }
    if(dpad_mode != 0 || NAtime > 0 || NBtime > 0 && NBtime < 20 || NIL1time > 0 && NIL1time < 284 || NIL2time > 0 && NIL2time < 250){
        cup = cdown = cleft = cright = false;
    }
    if(macro_mode > 0 && gpio_get(PIN_B) && ((cleft || cright) && !(cup || cdown) || !(cleft || cright) && (cup || cdown))){
        report.rightStickXAxis = cleft ? 0x00 : (cright ? 0xFF : (!gpio_get(PIN_LEFT) ? 0x49 : (!gpio_get(PIN_RIGHT) ? 0xB6 : 0x80)));
        report.rightStickYAxis = cup ? 0x00 : (cdown ? 0xFF : (!gpio_get(PIN_UP) ? 0x49 : (!gpio_get(PIN_DOWN) ? 0xB6 : 0x80)));
    }
    else if(gpio_get(PIN_B)){
        report.rightStickXAxis = cleft ? 0x00 : (cright ? 0xFF :  0x80);
        report.rightStickYAxis = cup ? 0x00 : (cdown ? 0xFF : 0x80);
    }
    else{
        report.rightStickXAxis = 0x80;
        report.rightStickYAxis = 0x80;
    }
}
void macro(){
    if(gpio_get(PIN_NA) && !gpio_get(PIN_CLEFT) && !gpio_get(PIN_CRIGHT) && !gpio_get(PIN_CUP) && !gpio_get(PIN_CDOWN)){
        if (!gpio_get(PIN_B)) {
            macro_mode = 1; //Steve
        }
        else if (!gpio_get(PIN_A)) {
            macro_mode = 0; //None
        }
        // DPAD_MODE: 0 = smash ,1 = hybrid , 2 = cpt
        if (!gpio_get(PIN_Y)) {
            dpad_mode = 0;
        }
        else if(!gpio_get(PIN_X)) {
            dpad_mode = 1;
        }
        else if(!gpio_get(PIN_L2)) {
            dpad_mode = 2;
        }
    }

    //####################<NA>start
    if(NAtime > 0 && NAtime < 20){
        sleep_ms(1);
        NAtime += 1;
    }
    else if(NAtime == 0 && !gpio_get(PIN_NA) && NIL1time == 0 && NIL2time == 0){
        NAtime = 1;
    }
    else if(NAtime == 20 && gpio_get(PIN_NA)){
        NAtime = 0;
        sleep_ms(17);
    }
    //####################<NA>end

    //####################<NB>start
    if(NBtime > 0 && NBtime < 20){
        sleep_ms(1);
        NBtime += 1;
    }
    else if(NBtime == 0 && !gpio_get(PIN_Y) && (!SJ && macro_mode == 1 || macro_mode > 1)){
        NBtime = 1;
    }
    else if(NBtime == 20 && gpio_get(PIN_Y)){
        NBtime = 0;
        sleep_ms(17);
    }
    //####################<NB>end

    //####################<Dragon>start
    if(Dragontime > 0 && Dragontime < 40){
        sleep_ms(1);
        Dragontime += 1;
    }
    else if(Dragontime == 0 && SJ && EightBottuns && abs(x_i) >= y_i && !gpio_get(PIN_DOWN) && (!gpio_get(PIN_LEFT) && gpio_get(PIN_RIGHT) || gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT)) && gpio_get(PIN_TILTE)){
        Dragontime = 1;
    }
    else if(Dragontime == 40 && (gpio_get(PIN_DOWN) || gpio_get(PIN_LEFT) && gpio_get(PIN_RIGHT) || !gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT) || !EightBottuns)){
        Dragontime = 0;
        sleep_ms(15);
    }
    //####################<Dragon>end

    //####################<Change>start
    if(Changetime > 0 && Changetime < 20){
        sleep_ms(1);
        Changetime += 1;
    }
    else if(Changetime == 0 && !gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT) && !gpio_get(PIN_DOWN) && gpio_get(PIN_UP) && gpio_get(PIN_CLEFT) && gpio_get(PIN_CRIGHT) && gpio_get(PIN_CUP) && gpio_get(PIN_CDOWN) && gpio_get(PIN_TILTE) && abs(y_i) > 1){
        Changetime = 1;
    }
    else if(Changetime == 20 && (gpio_get(PIN_LEFT) || gpio_get(PIN_RIGHT) || gpio_get(PIN_DOWN) || !gpio_get(PIN_UP) || !gpio_get(PIN_CLEFT) || !gpio_get(PIN_CRIGHT) || !gpio_get(PIN_CUP) || !gpio_get(PIN_CDOWN))){
        Changetime = 0;
        sleep_ms(15);
    }
    //####################<Change>end

    //####################<NIL1>start
    if((NIL1time != 0 || NIL2time != 0) && gpio_get(PIN_B) && !Release1){
        Release1 = true;
    }
    else if((NIL1time != 0 || NIL2time != 0) && !gpio_get(PIN_B) && Release1){
        Release1 = false;
        Buffer1  = true;
    }
    if(NIL1time > 0 && NIL1time < 284 + CT){
        sleep_ms(1);
        NIL1time += 1;
    }
    else if(NIL1time == 0 && (cooltime == 0 || cooltime == CT) && !gpio_get(PIN_B) && SJ && macro_mode == 1){
        NIL1time = 1;
        Release1 = false;
    }
    else if(NIL1time == 284 + CT && gpio_get(PIN_B)){
        Release1 = false;
        Buffer1  = false;
        NIL1time = 0;
        sleep_ms(17);
    }
    if(NIL1time == 284 + CT && (cooltime == 0 || cooltime == CT) && Buffer1){
        NIL1time = 1;
        Release1 = false;
        Buffer1  = false;
    }
    //####################<NIL1>end

    //####################<NIL2>start
    if((NIL1time != 0 || NIL2time != 0) && gpio_get(PIN_Y) && !Release2){
        Release2 = true;
    }
    else if((NIL1time != 0 || NIL2time != 0) && !gpio_get(PIN_Y) && Release2){
        Release2 = false;
        Buffer2  = true;
    }
    if(NIL2time > 0 && NIL2time < 250 + CT){
        sleep_ms(1);
        NIL2time += 1;
    }
    else if(NIL2time == 0 && (cooltime == 0 || cooltime == CT) && gpio_get(PIN_CLEFT) && gpio_get(PIN_CRIGHT) && !gpio_get(PIN_Y) && SJ && macro_mode == 1){
        NIL2time = 1;
        Release2 = false;
    }
    else if(NIL2time == 250 + CT && gpio_get(PIN_Y)){
        Release2 = false;
        Buffer2  = false;
        NIL2time = 0;
        sleep_ms(17);
    }
    if(NIL2time == 250 + CT && (cooltime == 0 || cooltime == CT) && Buffer2){
        NIL2time = 1;
        Release2 = false;
        Buffer2  = false;
    }
    //####################<NIL2>end

    //####################<NA NB>start
    if(NAtime > 0 && NAtime < 20){
        if(dpad_mode == 0){
            report.buttons |= 1 << 2;
        }
        report.leftStickXAxis = 0x80;
        report.leftStickYAxis = 0x80;
    }
    if(NBtime > 0 && NBtime < 20){
        report.buttons |= 1 << 0;
        report.leftStickXAxis = 0x80;
        report.leftStickYAxis = 0x80;
    }
    //####################END

    //####################<Yoga>start
    if(Changetime > 0 && Changetime < 20){
        report.leftStickXAxis = 0x80;
        report.leftStickYAxis = 0xFF;
    }
    else if(Changetime == 20){
        report.leftStickXAxis = SJ && EightBottuns ? (x_i > 0 ? 0x00 : 0xFF) : (x_i > 0 ? 38 : 218);
        report.leftStickYAxis = SJ && EightBottuns ? 128 : 218;
    }
    //####################END

    //####################<Dragon>start
    if(Dragontime > 0 && Dragontime <= 20){
        report.leftStickXAxis = 0x80;
        report.leftStickYAxis = 0xFF;
    }
    else if(Dragontime > 20 && Dragontime < 40){
        report.leftStickXAxis = x_i < 0 ? 38 : 218;
        report.leftStickYAxis = 218;
    }
    else if(Dragontime == 40){
        report.leftStickXAxis = x_i < 0 ? 0x00 : 0xFF;
        report.leftStickYAxis = 0x80;
    }
    //####################END

    //####################<NIL1>start
    if((NIL1time > 0) && (NIL1time <= 20)){
        report.buttons |= 1 << 6;
        report.leftStickXAxis = 128 + ((!gpio_get(PIN_RIGHT) ? 1 : 0) - (!gpio_get(PIN_LEFT) ? 1 : 0)) * (TILTE ? tilt_abs_value : 127);
        report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : 0x80;
        if(NIL1time == 20 && (!gpio_get(PIN_RIGHT) || !gpio_get(PIN_LEFT)) && gpio_get(PIN_DOWN) && gpio_get(PIN_UP) && gpio_get(PIN_TILTE))
            NIL1time = 100;
    }
    else if((NIL1time > 20) && (NIL1time <= 183)){
        report.leftStickXAxis = 128 + ((!gpio_get(PIN_RIGHT) ? 1 : 0) - (!gpio_get(PIN_LEFT) ? 1 : 0)) * (TILTE ? tilt_abs_value : 127);
        report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : 0x80;
    }
    else if((NIL1time > 183) && (NIL1time <= 220)){
        report.buttons |= 1 << 1;
        report.leftStickXAxis = 0x80;
        report.leftStickYAxis = 0x80;
    }
    else if((NIL1time > 220) && (NIL1time <= 284)){
        report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xCD : (!gpio_get(PIN_LEFT) ? 0x32 : 0x80);
        report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : 0x80;
    }
    else if((NIL1time > 284) && (NIL1time <= 284 + 30)){
        if(!gpio_get(PIN_NA)){
            report.buttons |= 1 << 1;
            report.buttons |= 1 << 2;
            report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xFF : (!gpio_get(PIN_LEFT) ? 0x00 : 0x80);
            report.leftStickYAxis = gpio_get(PIN_RIGHT) && gpio_get(PIN_LEFT) || !gpio_get(PIN_UP) ? 0x00 : 0x80;
        }
        else if(!gpio_get(PIN_CUP)){
            report.rightStickXAxis = 0x80;
            report.rightStickYAxis = 0x00;
            report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xCD : (!gpio_get(PIN_LEFT) ? 0x32 : 0x80);
            report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : 0x80;
        }
        else{
            NIL1time = 284 + CT;
        }
    }
    else if((NIL1time > 284 + 30) && (NIL1time < 284 + CT)){
        report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xCD : (!gpio_get(PIN_LEFT) ? 0x32 : 0x80);
        report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : 0x80;
    }
    //####################END

    //####################<NIL2>start
    if((NIL2time > 0) && (NIL2time <= 67)){
        report.buttons |= 1 << 6;
        report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xFF : (!gpio_get(PIN_LEFT) ? 0x00 : 0x80);
        report.leftStickYAxis = 0x80;
    }
    else if((NIL2time > 67) && (NIL2time <= 140)){
        report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xFF : (!gpio_get(PIN_LEFT) ? 0x00 : 0x80);
        report.leftStickYAxis = 0x80;
    }
    else if((NIL2time > 140) && (NIL2time <= 180)){
        report.buttons |= 1 << 6;
        report.buttons |= 1 << 1;
        report.leftStickXAxis = 0x80;
        report.leftStickYAxis = 0x80;
    }
    else if((NIL2time > 180) && (NIL2time <= 250)){
        report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xB9 : (!gpio_get(PIN_LEFT) ? 0x46 : 0x80);
        report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : (!gpio_get(PIN_DOWN) ? 0xFF : 0x80);
    }
    else if((NIL2time > 250) && (NIL2time <= 250 + 30)){
        if(!gpio_get(PIN_NA)){
            report.buttons |= 1 << 1;
            report.buttons |= 1 << 2;
            report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xCD : (!gpio_get(PIN_LEFT) ? 0x32 : 0x80);
            report.leftStickYAxis = gpio_get(PIN_RIGHT) && gpio_get(PIN_LEFT) || !gpio_get(PIN_UP) ? 0x00 : 0x80;
        }
        else if(!gpio_get(PIN_CUP)){
            report.rightStickXAxis = 0x80;
            report.rightStickYAxis = 0x00;
            report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xB9 : (!gpio_get(PIN_LEFT) ? 0x46 : 0x80);
            report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : (!gpio_get(PIN_DOWN) ? 0xFF : 0x80);
        }
        else{
            NIL2time = 250 + CT;
        }
    }
    else if((NIL2time > 250 + 30) && (NIL2time < 250 + CT)){
        report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? 0xCD : (!gpio_get(PIN_LEFT) ? 0x32 : 0x80);
        report.leftStickYAxis = !gpio_get(PIN_UP) ? 0x00 : 0x80;
    }
    //####################END
}

void dpad(bool cup, bool cdown, bool cleft, bool cright) {
    int up = !gpio_get(PIN_UP) ? 1 : 0;
    int down = !gpio_get(PIN_DOWN) ? 1 : 0;
    int left = !gpio_get(PIN_LEFT) ? 1 : 0;
    int right = !gpio_get(PIN_RIGHT) ? 1 : 0;
    int x_value;
    int y_value;
    int quadrant;
    int angle = 0;
    int up_value = 90;
    int left_value = 180;
    int down_value = 270;
    int right_value = (1 - up) * 360;
    if(NAtime > 0 && NAtime < 20 || NBtime > 0 && NBtime < 20){
        left = right = up = down = 0;
    }
    if(prevSUM == 0 && left + right + up + down > 0 && (cleft || cright || cup || cdown) && !moving){
        moving = true;
        x_i = y_i = prev_x_value = prev_y_value = 0;
    }
    if(prevSUM != left + right + up + down && !moving2){
        moving2 = true;
    }
    if(left + right + up + down == 0){
        moving = moving2 = false;
    }
    switch(dpad_mode){
        case 0: //smash
            hold_timeX = hold_timeY = 0;

        case 1: //hybrid
            if(dpad_mode != 0 && !moving && (cleft || cright || cup || cdown)){
                x_value = cleft ? 0 : (cright ? 255 : 128);
                y_value = cup ? 0 : (cdown ? 255 : 128);
                break;
            }
            if(left + right + up + down == 4){
                if(abs(x_i) > abs(y_i)){
                    down = up = 0;
                    if(prev_x_value == 1){
                        right = 0;
                    }
                    else{
                        left = 0;
                    }
                }
                else{
                    right = left = 0;
                    if(prev_y_value == 1){
                        down = 0;
                    }
                    else{
                        up = 0;
                    }
                }
            }
            else{
                if(right + left == 2){
                    y_i = 0;
                    if(x_i > hold_timeX){
                        right = 0;
                    }
                    else if(x_i < -hold_timeX){
                        left = 0;
                    }
                    else{
                        right = left = 0;
                    }
                }
                else{
                    prev_x_value = right - left;
                    if(down + up <= 1){
                        if(Changetime == 0 && Dragontime == 0 && NAtime == 0 && NBtime == 0 && NIL1time == 0 && NIL2time == 0 && (cooltime == 0 || cooltime == CT)){
                            sleep_ms(1);
                        }
                        x_i = abs(prev_x_value) * (x_i + prev_x_value);
                    }
                }
                if(down + up == 2){
                    if(y_i > hold_timeY){
                        down = 0;
                    }
                    else if(y_i < 0){
                        up = 0;
                    }
                    else{
                        down = up = 0;
                        y_i = 0;
                    }
                }
                else{
                    prev_y_value = down - up;
                    if(right + left <= 1){
                        if(Changetime == 0 && Dragontime == 0 && NAtime == 0 && NBtime == 0 && NIL1time == 0 && NIL2time == 0 && (cooltime == 0 || cooltime == CT)){
                            sleep_ms(1);
                        }
                        y_i = abs(prev_y_value) * (y_i + prev_y_value);
                    }
                }

                if((left + right == 1 && (cup || cdown) || up + down == 1 && (cleft || cright)) && dpad_mode == 0){
                    if(left == 1 && cup){
                        angle = 157;
                    }
                    else if(left == 1 && cdown){
                        angle = 203;
                    }
                    else if(right == 1 && cup){
                        angle = 23;
                    }
                    else if(right == 1 && cdown){
                        angle = 337;
                    }
                    else if(up == 1 && cleft){
                        angle = 113;
                    }
                    else if(up == 1 && cright){
                        angle = 67;
                    }
                    else if(down == 1 && cleft){
                        angle = 247;
                    }
                    else if(down == 1 && cright){
                        angle = 293;
                    }
                    x_value = 128 + (TILTE ? tilt_abs_value : 127) * cos(angle * M_PI / 180);
                    y_value = 128 + (TILTE ? tilt_abs_value : 127) * sin(angle * M_PI / 180) * -1;
                    break;
                }
            }
        default: //cpt
            angle = (left_value * left + right_value * right + up_value * up + down_value * down) / (left + right + up + down);
            angle %= 360; //0 ~ 360
            quadrant = floor(angle / 90);
            if(!cleft && !cright && !cup && !cdown && (left + right + up + down == 2) && dpad_mode == 0){
                angle += (quadrant % 2 == 1 ? 1 : -1) * (up == 1 ? up_angle : (down == 1 ? down_angle : 0));
            }
            x_value = 128 + (right - left) * (TILTE ? tilt_abs_value : 127) * abs(cos(angle * M_PI / 180));
            y_value = 128 + (down - up) * (TILTE ? tilt_abs_value : 127) * abs(sin(angle * M_PI / 180));
    }
    if (SJ && EightBottuns && abs(x_i) < y_i && down == 1 && left + right >= 1 && gpio_get(PIN_TILTE)){
        if(prevSUM > 1){
            x_value = x_value > 128 ? 0xFF : 0x00;
            y_value = 128;
        }
        else{
            left = right = 0;
        }
    }
    else if(SJ && EightBottuns && (!cleft && !cright && !cup && !cdown || moving) && (!moving2 || !prevEight) && (gpio_get(PIN_RIGHT) && !gpio_get(PIN_LEFT) || !gpio_get(PIN_RIGHT) && gpio_get(PIN_LEFT)) && gpio_get(PIN_DOWN) && dpad_mode == 1 && gpio_get(PIN_TILTE)){
        x_value = 128;
        moving2 = false;
    }
    else if(SJ && (!gpio_get(PIN_R1) || !gpio_get(PIN_R2)) && (!cleft && !cright && !cup && !cdown || moving) && down == 1 && left + right >= 1 && dpad_mode == 1 && gpio_get(PIN_TILTE)){
        x_value = x_value > 128 ? 0xFF : 0x00;
        y_value = 128;
    }
    if(Changetime == 0 && Dragontime == 0 && (NIL1time == 0 || NIL1time == 284 + CT) && (NIL2time == 0 || NIL2time == 250 + CT)){
        report.leftStickXAxis = x_value;
        report.leftStickYAxis = y_value;
    }
    prevSUM = left + right + up + down;
}
void send_hid_report() {
    if (!tud_hid_ready()) {
        return;
    }

    if (memcmp(&prevReport, &report, sizeof(report))) {
        tud_hid_report(0, &report, sizeof(report));
        memcpy(&prevReport, &report, sizeof(report));
    }
}

void hid_task(void) {
    // bits: 0 = Y, 1 = B, 2 = A, 3 = X
    // 4 = L1, 5 = R1, 6 = L2, 7 = R2, 8 = select, 9 = start,
    // 10 = L3, 11 = R3, 12 = Home
    uint32_t pins = ~gpio_get_all() & pin_mask;

    EightBottuns = !gpio_get(PIN_A) || !gpio_get(PIN_B) || !gpio_get(PIN_L1) || (dpad_mode == 0 ? !gpio_get(PIN_R1) : !gpio_get(PIN_X) || !gpio_get(PIN_Y) || !gpio_get(PIN_L2));
    report.buttons = 0x00;
    report.triangleAxis = 0x00;
    report.circleAxis = 0x00;
    report.crossAxis = 0x00;
    report.squareAxis = 0x00;
    report.L1Axis = 0x00;
    report.L2Axis = 0x00;
    report.R1Axis = 0x00;
    report.R2Axis = 0x00;
    macro();
    cpad(pins & (1 << PIN_CUP),
         pins & (1 << PIN_CDOWN),
         pins & (1 << PIN_CLEFT),
         pins & (1 << PIN_CRIGHT));
    dpad(pins & (1 << PIN_CUP),
         pins & (1 << PIN_CDOWN),
         pins & (1 << PIN_CLEFT),
         pins & (1 << PIN_CRIGHT));

    if (pins & (1 << PIN_SJ)) {
        SJ = true;
        if(dpad_mode != 0){
            report.buttons |= 1 << 10; //L3
        }
    }
    else{
        SJ = false;
    }
    if (pins & (1 << PIN_TILTE)) {
        TILTE = true;
        if(dpad_mode != 0){
            report.buttons |= 1 << 8; //select
        }
    }
    else{
        TILTE = false;
    }
    if(NIL1time == 0 && NIL2time == 0 && cooltime == 0 && (Dragontime == 0 || Dragontime > 20)){
        if(dpad_mode == 0){
            if (pins & (1 << PIN_Y) && NIL2time == 0){
                report.buttons |= 1 << 0; //Y
            }
            if (pins & (1 << PIN_B) && NIL1time == 0){
                report.buttons |= 1 << 1; //B
            }
            if (pins & (1 << PIN_A)){
                report.buttons |= 1 << 2; //A
            }
            if (pins & (1 << PIN_X)){
                report.buttons |= 1 << 3; //X
            }
            if (pins & (1 << PIN_L1) && (Dragontime == 0 || Dragontime >= 40)) {
                report.buttons |= 1 << 4; //L1
            }
            if (pins & (1 << PIN_R1)) {
                report.buttons |= 1 << 5; //R1
            }
            if (pins & (1 << PIN_L2)){
                report.buttons |= 1 << 6; //L2
            }
            if (pins & (1 << PIN_R2)) {
                report.buttons |= 1 << 7; //R2
            }
            if ((pins & (1 << PIN_NA)) && NAtime > 0){
                report.buttons |= 1 << 2; //A
            }
        }
        else{
            tilt_abs_value = 127;
            if (pins & (1 << PIN_Y)){
                report.buttons |= 1 << 0; //Y
            }
            if (pins & (1 << PIN_B)){
                report.buttons |= 1 << 1; //B
            }
            if (pins & (1 << PIN_A)){
                report.buttons |= 1 << 2; //A
            }
            if (pins & (1 << PIN_X)){
                report.buttons |= 1 << 3; //X
            }
            if (pins & (1 << PIN_L1)) {
                report.buttons |= 1 << 7; //R2
            }
            if (pins & (1 << PIN_R1)) {
                report.buttons |= 1 << 6; //L2
            }
            if (pins & (1 << PIN_L2)){
                report.buttons |= 1 << 5; //R1
            }
            if (pins & (1 << PIN_R2)) {
                report.buttons |= 1 << 4; //L1
            }
            if (!gpio_get(PIN_NA)){
                report.buttons |= 1 << 11; //R3
            }
        }
    }
    if ((pins & (1 << PIN_START)) && TILTE && dpad_mode == 0){
        report.buttons |= 1 << 8; //select
    }
    else if ((pins & (1 << PIN_START)) && SJ && dpad_mode == 0) {
        report.buttons |= 1 << 12; //Home
    }
    else if ((pins & (1 << PIN_START))) {
        report.buttons |= 1 << 9; //start
    }
    send_hid_report();
    prevEight = EightBottuns;
}

void pin_init(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void pins_init(void) {
    pin_init(PIN_UP);
    pin_init(PIN_DOWN);
    pin_init(PIN_LEFT);
    pin_init(PIN_RIGHT);
    pin_init(PIN_A);
    pin_init(PIN_B);
    pin_init(PIN_X);
    pin_init(PIN_Y);
    pin_init(PIN_L1);
    pin_init(PIN_L2);
    pin_init(PIN_R1);
    pin_init(PIN_R2);
    pin_init(PIN_TILTE);
    pin_init(PIN_START);
    pin_init(PIN_SJ);
    pin_init(PIN_NA);
    pin_init(PIN_CUP);
    pin_init(PIN_CDOWN);
    pin_init(PIN_CLEFT);
    pin_init(PIN_CRIGHT);

    // Load Setting
    g_read_data[0] = 0;
    g_read_data[1] = 0;
    g_read_data[2] = 0;
    g_read_data[3] = 0;
    load_setting_from_flash();

    // INPUT_MODE: 1 = PC/PS3, 0 = NINTENDO SWITCH
    if (!gpio_get(PIN_CUP)) {
        input_mode = 1;
    } else if (!gpio_get(PIN_CDOWN)) {
        input_mode = 0;
    } else {
        if (g_read_data[0] == 1) {
            input_mode = 1;
        } else {
            input_mode = 0;
        }
    }

    if (!gpio_get(PIN_B)) {
        macro_mode = 1;
    }
    else if (!gpio_get(PIN_A)) {
        macro_mode = 0;
    }
    else{
        if(g_read_data[1] == 0)
            macro_mode = 0;
        if(g_read_data[1] == 1)
            macro_mode = 1;
    }

    // DPAD_MODE: 0 = smash ,1 = hybrid , 2 = cpt
    if (!gpio_get(PIN_Y)) {
        dpad_mode = 0;
    }
    else if(!gpio_get(PIN_X)) {
        dpad_mode = 1;
    }
    else if(!gpio_get(PIN_L2)) {
        dpad_mode = 2;
    }
    else {
        if(g_read_data[2] == 0)
            dpad_mode = 0;
        if(g_read_data[2] == 1)
            dpad_mode = 1;
        if(g_read_data[2] == 2)
            dpad_mode = 2;
    }

    // PRO_MODE: 0 = normal ,1 = pro
    if (!gpio_get(PIN_CLEFT)) {
        pro_mode = 0;
    }
    else if(!gpio_get(PIN_CRIGHT)) {
        pro_mode = 1;
    }
    else {
        if(g_read_data[3] == 0)
            pro_mode = 0;
        if(g_read_data[3] == 1)
            pro_mode = 1;
    }
    save_setting_to_flash(input_mode, macro_mode, dpad_mode, pro_mode);
}

void report_init(void) {
    memset(&report, 0, sizeof(report));
    report.dpadHat = 0x0f;
    report.leftStickXAxis = 0x80;
    report.leftStickYAxis = 0x80;
    report.rightStickXAxis = 0x80;
    report.rightStickYAxis = 0x80;
    report.accelerometerXAxis = 0x0200; // not sure why 0x02, but that's what HORI Fighting Stick V3 sends
    report.accelerometerYAxis = 0x0200;
    report.accelerometerZAxis = 0x0200;
    memcpy(&prevReport, &report, sizeof(report));
}

int main(void) {
    //stdio_init_all();
    board_init();
    pins_init();
    report_init();
    tusb_init();

    // ###### multicore ###############
    multicore_launch_core1(main_core1);
    // ################################

    while (true) {
        tud_task();             // tinyusb device task
        hid_task();
    }

    return 0;
}

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
    switch (input_mode) {
    case 0:
        return ps3_desc_hid_report;
        break;
    case 1:
        return switch_desc_hid_report;
        break;
    default:
        return ps3_desc_hid_report;
        break;
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t * buffer, uint16_t reqlen) {
    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    switch (input_mode) {
    case 0:
        return ps3_desc_configuration;
        break;
    case 1:
        return switch_desc_configuration;
        break;
    default:
        return ps3_desc_configuration;
        break;
    }
}

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;

        const char *str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
