/*
 * ST7920_lib.c
 *
 *  Created on: 10 дек. 2020 г.
 *      Author: Oleg Volkov, Konstantin Golinsky
 *
 *  YouTube: https://www.youtube.com/channel/UCzZKTNVpcMSALU57G1THoVw
 *  GitHub: https://github.com/Solderingironspb/Lessons-Stm32/blob/master/README.md
 *  Группа ВК: https://vk.com/solderingiron.stm32
 *
 *  Кодировка UTF-8 Basic Latin: https://www.utf8-chartable.de/unicode-utf8-table.pl
 *  Кодировка UTF-8 Cyrillic: https://www.utf8-chartable.de/unicode-utf8-table.pl?start=1024&names=-&unicodeinhtml=hex
 *  Программа для конвертации изображения.bmp в bitmap: http://en.radzio.dxp.pl/bitmap_converter/
 */

#include "ST7920_lib.h"
/*-----------------------------------Настройки----------------------------------*/

extern SPI_HandleTypeDef hspi1; //Используемая шина spi
extern DMA_HandleTypeDef hdma_spi1_tx; //Задействован модуль DMA. Подключена отправка по 8 bit.

/*Внимание! Для совместимости с контроллерами STM32F4хх, для выключения ножки используем регистр BSRR, т.к. регистр BRR в них уже отсутствует*/
/*Собственно, ниже, так и сделано:                                                                                                           */
#define cs_set() GPIOA-> BSRR = CS_Pin;                       //CS притягиваем к 3.3v
#define cs_reset() GPIOA->BSRR = (uint32_t) CS_Pin << 16u;    //CS_притягиваем к земле
#define RST_set() GPIOA-> BSRR = RST_Pin;                     //RST притягиваем к 3.3v
#define RST_reset() GPIOA->BSRR = (uint32_t) RST_Pin << 16u;  //RST притягиваем к земле

char tx_buffer[128] = { 0, }; //Буфер для отправки текста на дисплей
uint8_t Frame_buffer[1024] = { 0, }; //Буфер кадра
uint8_t ST7920_width = 128; //Ширина дисплея в пикселях
uint8_t ST7920_height = 64; //Высота дисплея в пикселях

uint8_t startRow, startCol, endRow, endCol; // coordinates of the dirty rectangle
uint8_t numRows = 64;
uint8_t numCols = 128;
uint8_t Graphic_Check = 0;

/*-----------------------------------Настройки----------------------------------*/

/*-----------------------------------Шрифт 5*7----------------------------------*/
const uint8_t Font[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0/ --> space     20
		0x00, 0x00, 0x4F, 0x00, 0x00, 0x00, 0x00, 0x00, //1/ --> !         21
		0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, //2/ --> "         22 и т.д.
		0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00, 0x00, //3/ --> #
		0x00, 0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00, 0x00, //4/ --> $
		0x00, 0x23, 0x13, 0x08, 0x64, 0x62, 0x00, 0x00, //5/ --> %
		0x00, 0x36, 0x49, 0x55, 0x22, 0x40, 0x00, 0x00, //6/ --> &
		0x00, 0x00, 0x05, 0x03, 0x00, 0x00, 0x00, 0x00, //7/ --> '
		0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x00, 0x00, //8/ --> (
		0x00, 0x41, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, //9/ --> )
		0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00, 0x00, //10/ --> *
		0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, //11/ --> +
		0x00, 0xA0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, //12/ --> ,
		0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, //13/ --> -
		0x00, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, //14/ --> .
		0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00, //15/ --> /
		0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00, //16/ --> 0
		0x00, 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00, 0x00, //17/ --> 1
		0x00, 0x42, 0x61, 0x51, 0x49, 0x46, 0x00, 0x00, //18/ --> 2
		0x00, 0x21, 0x41, 0x45, 0x4B, 0x31, 0x00, 0x00, //19/ --> 3
		0x00, 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, 0x00, //20/ --> 4
		0x00, 0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00, //21/ --> 5
		0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00, 0x00, //22/ --> 6
		0x00, 0x01, 0x71, 0x09, 0x05, 0x03, 0x00, 0x00, //23/ --> 7
		0x00, 0x36, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, //24/ --> 8
		0x00, 0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x00, //25/ --> 9
		0x00, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, //26/ --> :
		0x00, 0x00, 0x56, 0x36, 0x00, 0x00, 0x00, 0x00, //27/ --> ;
		0x00, 0x08, 0x14, 0x22, 0x41, 0x00, 0x00, 0x00, //28/ --> <
		0x00, 0x24, 0x24, 0x24, 0x24, 0x24, 0x00, 0x00, //29/ --> =
		0x00, 0x00, 0x41, 0x22, 0x14, 0x08, 0x00, 0x00, //30/ --> >
		0x00, 0x02, 0x01, 0x51, 0x09, 0x06, 0x00, 0x00, //31/ --> ?
		0x00, 0x32, 0x49, 0x79, 0x41, 0x3E, 0x00, 0x00, //32/ --> @
		0x00, 0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00, 0x00, //33/ --> A
		0x00, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, //34/ --> B
		0x00, 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, 0x00, //35/ --> C
		0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00, 0x00, //36/ --> D
		0x00, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00, 0x00, //37/ --> E
		0x00, 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00, 0x00, //38/ --> F
		0x00, 0x3E, 0x41, 0x49, 0x49, 0x3A, 0x00, 0x00, //39/ --> G
		0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x00, //40/ --> H
		0x00, 0x00, 0x41, 0x7F, 0x41, 0x00, 0x00, 0x00, //41/ --> I
		0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, 0x00, 0x00, //42/ --> J
		0x00, 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00, 0x00, //43/ --> K
		0x00, 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, //44/ --> L
		0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x00, 0x00, //45/ --> M
		0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00, 0x00, //46/ --> N
		0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00, 0x00, //47/ --> O
		0x00, 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, //48/ --> P
		0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00, 0x00, 0x00, //49/ --> Q
		0x00, 0x7F, 0x09, 0x19, 0x29, 0x46, 0x00, 0x00, //50/ --> R
		0x00, 0x46, 0x49, 0x49, 0x49, 0x31, 0x00, 0x00, //51/ --> S
		0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x00, //52/ --> T
		0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00, 0x00, //53/ --> U
		0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00, 0x00, //54/ --> V
		0x00, 0x3F, 0x40, 0x60, 0x40, 0x3F, 0x00, 0x00, //55/ --> W
		0x00, 0x63, 0x14, 0x08, 0x14, 0x63, 0x00, 0x00, //56/ --> X
		0x00, 0x07, 0x08, 0x70, 0x08, 0x07, 0x00, 0x00, //57/ --> Y
		0x00, 0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x00, //58/ --> Z
		0x00, 0x7F, 0x41, 0x41, 0x00, 0x00, 0x00, 0x00, //59/ --> [
		0x00, 0x15, 0x16, 0x7C, 0x16, 0x15, 0x00, 0x00, //60/ --> '\'
		0x00, 0x41, 0x41, 0x7F, 0x00, 0x00, 0x00, 0x00, //61/ --> ]
		0x00, 0x04, 0x02, 0x01, 0x02, 0x04, 0x00, 0x00, //62/ --> ^
		0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, //63/ --> _
		0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, //64/ --> `
		0x00, 0x20, 0x54, 0x54, 0x54, 0x78, 0x00, 0x00, //65/ --> a
		0x00, 0x7F, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00, //66/ --> b
		0x00, 0x38, 0x44, 0x44, 0x44, 0x00, 0x00, 0x00, //67/ --> c
		0x00, 0x38, 0x44, 0x44, 0x48, 0x7F, 0x00, 0x00, //68/ --> d
		0x00, 0x38, 0x54, 0x54, 0x54, 0x18, 0x00, 0x00, //69/ --> e
		0x00, 0x10, 0x7E, 0x11, 0x01, 0x02, 0x00, 0x00, //70/ --> f
		0x00, 0x0C, 0x52, 0x52, 0x52, 0x3E, 0x00, 0x00, //71/ --> g
		0x00, 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x00, //72/ --> h
		0x00, 0x00, 0x44, 0x7D, 0x40, 0x00, 0x00, 0x00, //73/ --> i
		0x00, 0x20, 0x40, 0x40, 0x3D, 0x00, 0x00, 0x00, //74/ --> j
		0x00, 0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, //75/ --> k
		0x00, 0x00, 0x41, 0x7F, 0x40, 0x00, 0x00, 0x00, //76/ --> l
		0x00, 0x7C, 0x04, 0x18, 0x04, 0x78, 0x00, 0x00, //77/ --> m
		0x00, 0x7C, 0x08, 0x04, 0x04, 0x78, 0x00, 0x00, //78/ --> n
		0x00, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00, //79/ --> o
		0x00, 0x7C, 0x14, 0x14, 0x14, 0x08, 0x00, 0x00, //80/ --> p
		0x00, 0x08, 0x14, 0x14, 0x18, 0x7C, 0x00, 0x00, //81/ --> q
		0x00, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x00, 0x00, //82/ --> r
		0x00, 0x48, 0x54, 0x54, 0x54, 0x20, 0x00, 0x00, //83/ --> s
		0x00, 0x04, 0x3F, 0x44, 0x40, 0x20, 0x00, 0x00, //84/ --> t
		0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00, 0x00, //85/ --> u
		0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00, 0x00, //86/ --> v
		0x00, 0x1E, 0x20, 0x10, 0x20, 0x1E, 0x00, 0x00, //87/ --> w
		0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00, //88/ --> x
		0x00, 0x06, 0x48, 0x48, 0x48, 0x3E, 0x00, 0x00, //89/ --> y
		0x00, 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00, 0x00, //90/ --> z
		0x00, 0x08, 0x36, 0x41, 0x00, 0x00, 0x00, 0x00, //91/ --> {
		0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00, //92/ --> |
		0x00, 0x41, 0x36, 0x08, 0x00, 0x00, 0x00, 0x00, //93/ --> }
		0x00, 0x08, 0x08, 0x2A, 0x1C, 0x08, 0x00, 0x00, //94/ --> ~
		0x00, 0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00, 0x00, //95/ --> А
		0x00, 0x7F, 0x49, 0x49, 0x49, 0x33, 0x00, 0x00, //96/ --> Б
		0x00, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, //97/ --> В
		0x00, 0x7F, 0x01, 0x01, 0x01, 0x03, 0x00, 0x00, //98/ --> Г
		0x00, 0xE0, 0x51, 0x4F, 0x41, 0xFF, 0x00, 0x00, //99/ --> Д
		0x00, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00, 0x00, //100/ --> Е
		0x00, 0x77, 0x08, 0x7F, 0x08, 0x77, 0x00, 0x00, //101/ --> Ж
		0x00, 0x41, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, //102/ --> З
		0x00, 0x7F, 0x10, 0x08, 0x04, 0x7F, 0x00, 0x00, //103/ --> И
		0x00, 0x7C, 0x21, 0x12, 0x09, 0x7C, 0x00, 0x00, //104/ --> Й
		0x00, 0x7F, 0x08, 0x14, 0x22, 0x41, 0x00, 0x00, //105/ --> К
		0x00, 0x20, 0x41, 0x3F, 0x01, 0x7F, 0x00, 0x00, //106/ --> Л
		0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x00, 0x00, //107/ --> М
		0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x00, //108/ --> Н
		0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00, 0x00, //109/ --> О
		0x00, 0x7F, 0x01, 0x01, 0x01, 0x7F, 0x00, 0x00, //110/ --> П
		0x00, 0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, //111/ --> Р
		0x00, 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, 0x00, //112/ --> С
		0x00, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x00, //113/ --> Т
		0x00, 0x47, 0x28, 0x10, 0x08, 0x07, 0x00, 0x00, //114/ --> У
		0x00, 0x1C, 0x22, 0x7F, 0x22, 0x1C, 0x00, 0x00, //115/ --> Ф
		0x00, 0x63, 0x14, 0x08, 0x14, 0x63, 0x00, 0x00, //116/ --> Х
		0x00, 0x7F, 0x40, 0x40, 0x40, 0xFF, 0x00, 0x00, //117/ --> Ц
		0x00, 0x07, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x00, //118/ --> Ч
		0x00, 0x7F, 0x40, 0x7F, 0x40, 0x7F, 0x00, 0x00, //119/ --> Ш
		0x00, 0x7F, 0x40, 0x7F, 0x40, 0xFF, 0x00, 0x00, //120/ --> Щ
		0x00, 0x01, 0x7F, 0x48, 0x48, 0x30, 0x00, 0x00, //121/ --> Ъ
		0x00, 0x7F, 0x48, 0x30, 0x00, 0x7F, 0x00, 0x00, //122/ --> Ы
		0x00, 0x00, 0x7F, 0x48, 0x48, 0x30, 0x00, 0x00, //123/ --> Э
		0x00, 0x22, 0x41, 0x49, 0x49, 0x3E, 0x00, 0x00, //124/ --> Ь
		0x00, 0x7F, 0x08, 0x3E, 0x41, 0x3E, 0x00, 0x00, //125/ --> Ю
		0x00, 0x46, 0x29, 0x19, 0x09, 0x7f, 0x00, 0x00, //126/ --> Я
		0x00, 0x20, 0x54, 0x54, 0x54, 0x78, 0x00, 0x00, //127/ --> a
		0x00, 0x3c, 0x4a, 0x4a, 0x49, 0x31, 0x00, 0x00, //128/ --> б
		0x00, 0x7c, 0x54, 0x54, 0x28, 0x00, 0x00, 0x00, //129/ --> в
		0x00, 0x7c, 0x04, 0x04, 0x04, 0x0c, 0x00, 0x00, //130/ --> г
		0x00, 0xe0, 0x54, 0x4c, 0x44, 0xfc, 0x00, 0x00, //131/ --> д
		0x00, 0x38, 0x54, 0x54, 0x54, 0x18, 0x00, 0x00, //132/ --> е
		0x00, 0x6c, 0x10, 0x7c, 0x10, 0x6c, 0x00, 0x00, //133/ --> ж
		0x00, 0x44, 0x44, 0x54, 0x54, 0x28, 0x00, 0x00, //134/ --> з
		0x00, 0x7c, 0x20, 0x10, 0x08, 0x7c, 0x00, 0x00, //135/ --> и
		0x00, 0x7c, 0x41, 0x22, 0x11, 0x7c, 0x00, 0x00, //136/ --> й
		0x00, 0x7c, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, //137/ --> к
		0x00, 0x20, 0x44, 0x3c, 0x04, 0x7c, 0x00, 0x00, //138/ --> л
		0x00, 0x7c, 0x08, 0x10, 0x08, 0x7c, 0x00, 0x00, //139/ --> м
		0x00, 0x7c, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00, //140/ --> н
		0x00, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00, //141/ --> о
		0x00, 0x7c, 0x04, 0x04, 0x04, 0x7c, 0x00, 0x00, //142/ --> п
		0x00, 0x7C, 0x14, 0x14, 0x14, 0x08, 0x00, 0x00, //143/ --> р
		0x00, 0x38, 0x44, 0x44, 0x44, 0x28, 0x00, 0x00, //144/ --> с
		0x00, 0x04, 0x04, 0x7c, 0x04, 0x04, 0x00, 0x00, //145/ --> т
		0x00, 0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00, 0x00, //146/ --> у
		0x00, 0x30, 0x48, 0xfc, 0x48, 0x30, 0x00, 0x00, //147/ --> ф
		0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00, 0x00, //148/ --> х
		0x00, 0x7c, 0x40, 0x40, 0x40, 0xfc, 0x00, 0x00, //149/ --> ц
		0x00, 0x0c, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00, //150/ --> ч
		0x00, 0x7c, 0x40, 0x7c, 0x40, 0x7c, 0x00, 0x00, //151/ --> ш
		0x00, 0x7c, 0x40, 0x7c, 0x40, 0xfc, 0x00, 0x00, //152/ --> щ
		0x00, 0x04, 0x7c, 0x50, 0x50, 0x20, 0x00, 0x00, //153/ --> ъ
		0x00, 0x7c, 0x50, 0x50, 0x20, 0x7c, 0x00, 0x00, //154/ --> ы
		0x00, 0x7c, 0x50, 0x50, 0x20, 0x00, 0x00, 0x00, //155/ --> э
		0x00, 0x28, 0x44, 0x54, 0x54, 0x38, 0x00, 0x00, //156/ --> ь
		0x00, 0x7c, 0x10, 0x38, 0x44, 0x38, 0x00, 0x00, //157/ --> ю
		0x00, 0x08, 0x54, 0x34, 0x14, 0x7c, 0x00, 0x00, //158/ --> я
		0x00, 0x7E, 0x4B, 0x4A, 0x4B, 0x42, 0x00, 0x00, //159/ --> Ё
		0x00, 0x38, 0x55, 0x54, 0x55, 0x18, 0x00, 0x00, //160/ --> ё
		0x00, 0x00, 0x06, 0x09, 0x09, 0x06, 0x00, 0x00 //161/ --> °
		};

/*-----------------------------------Шрифт 5*7----------------------------------*/

/*================= Демонстрационное лого. Можно вырезать. =====================*/
/*-----------------------------Демонстрационное Logo----------------------------*/
const char solderingiron[1024] = { 0xC0, 0xE0, 0x30, 0x30, 0x30, 0x30, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC1, 0x83, 0x06, 0x04, 0x0C, 0x0C, 0x8C, 0xF8, 0x70, 0x00, 0x78, 0xFC, 0x86, 0x03, 0x03, 0x03, 0x03, 0x86,
		0xFC, 0x78, 0x00, 0xFF, 0xFF, 0x78, 0xFE, 0x86, 0x03, 0x03, 0x03, 0x86, 0xFF, 0xFF, 0x00, 0x00, 0xFC, 0xFE, 0xB7, 0x33, 0x33, 0x33, 0x37, 0x3E, 0x3C,
		0x00, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x03, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x03, 0x03, 0x06, 0xFE, 0xF8, 0x00, 0x00, 0x38, 0xFC, 0x86, 0x03,
		0x03, 0x03, 0x86, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x03, 0x78,
		0xFC, 0x86, 0x03, 0x03, 0x03, 0x03, 0x86, 0xFC, 0x78, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x03, 0x03, 0x06, 0xFE, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x09, 0x09, 0x0B, 0x0B, 0x0B, 0x09, 0x09, 0x08, 0x08, 0x08, 0x08, 0x09, 0x0B, 0x0B,
		0x0B, 0x0B, 0x09, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x08, 0x09, 0x09, 0x0B, 0x0B, 0x0B, 0x09, 0x0B, 0x0B, 0x08, 0x08, 0x08, 0x09, 0x0B, 0x0B, 0x0B, 0x0B,
		0x0B, 0x09, 0x09, 0x08, 0x08, 0x0B, 0x0B, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x08, 0x0B, 0x0B, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x08, 0x08, 0x00,
		0x10, 0x39, 0x63, 0x63, 0x63, 0x31, 0x1F, 0x0F, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x08, 0x08, 0x0B, 0x0B, 0x08,
		0x08, 0x08, 0x08, 0x08, 0x09, 0x0B, 0x0B, 0x0B, 0x0B, 0x09, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0B, 0x0B };
/*-----------------------------Демонстрационное Logo----------------------------*/

/*---------------Вывод стартового демонстрационного лого------------------------*/
void ST7920_logo_demonstration(void) {
	for (int i = 0; i < 369; i++) {
		Frame_buffer[i + 265] = solderingiron[i];
	}
	ST7920_Update();
	sprintf(tx_buffer, "Saint Petersburg");
	ST7920_Decode_UTF8(16, 5, 0, tx_buffer);
	sprintf(tx_buffer, "electronics lab.");
	ST7920_Decode_UTF8(18, 6, 0, tx_buffer);
	HAL_Delay(500);
	ST7920_Update();
	HAL_Delay(3000);
	sprintf(tx_buffer, "                ");
	ST7920_Decode_UTF8(18, 6, 0, tx_buffer);
	HAL_Delay(500);
	ST7920_Update();
	sprintf(tx_buffer, "                ");
	ST7920_Decode_UTF8(16, 5, 0, tx_buffer);
	HAL_Delay(500);
	ST7920_Update();
	for (int i = 0; i < 369; i++) {
		Frame_buffer[i + 265] = 0x00;
	}
	HAL_Delay(500);
}
/*---------------Вывод стартового демонстрационного лого------------------------*/
/*================= Демонстрационное лого. Можно вырезать. =====================*/

/*----------------------Функция отправки команды на дисплей------------------------*/
static void ST7920_Send_command(uint8_t Data) {
/// Функция отправки команды на дисплей
/// \param Data - 8 бит данных. DB0 - DB7.
	cs_set();
	uint8_t tx_buffer = 0xF8; //Отправка команды. RW = 0/RS = 0
	HAL_SPI_Transmit_IT(&hspi1, &tx_buffer, 1);
	tx_buffer = Data & 0xF0;  //Разбиваем 8 бит на 2 части. Передаем 7-4 бит.
	HAL_SPI_Transmit_IT(&hspi1, &tx_buffer, 1);
	tx_buffer = (Data << 4); //Разбиваем 8 бит на 2 части. Передаем оставшиеся 3-0 бит.
	HAL_SPI_Transmit_IT(&hspi1, &tx_buffer, 1);
	cs_reset();
}
/*----------------------Функция отправки команды на дисплей------------------------*/

/*----------------------Функция отправки данных на дисплей------------------------*/
static void ST7920_Send_data(uint8_t Data) {
/// Функция отправки данных на дисплей
/// \param Data - 8 бит данных. DB0 - DB7.
	cs_set();
	uint8_t tx_buffer = 0xFA; //Отправка данных. RW = 0/RS = 1
	HAL_SPI_Transmit_IT(&hspi1, &tx_buffer, 1);
	tx_buffer = Data & 0xF0; //Разбиваем 8 бит на 2 части. Передаем 7-4 бит.
	HAL_SPI_Transmit_IT(&hspi1, &tx_buffer, 1);
	tx_buffer = (Data << 4); //Разбиваем 8 бит на 2 части. Передаем оставшиеся 3-0 бит.
	HAL_SPI_Transmit_IT(&hspi1, &tx_buffer, 1);
	cs_reset();
}
/*----------------------Функция отправки данных на дисплей------------------------*/

/*-------------------------Функция инициализации дисплея--------------------------*/
void ST7920_Init(void) {
/// Функция инициализации дисплея
	RST_reset(); //Дернем ножку RST
	HAL_Delay(10);
	RST_set();
	HAL_Delay(40); //Ждем 40 мс

	//Далее все согласно Datasheet://
	uint8_t tx_buffer = 0x30; //Function set
	ST7920_Send_command(tx_buffer);
	HAL_Delay(1);
	ST7920_Send_command(tx_buffer);
	HAL_Delay(1);
	tx_buffer = 0x0C; //D = 1, C = 0, B = 0.
	ST7920_Send_command(tx_buffer);
	HAL_Delay(1);
	tx_buffer = 0x01;
	ST7920_Send_command(tx_buffer); //Display Clean
	HAL_Delay(12);
	tx_buffer = 0x06;
	ST7920_Send_command(tx_buffer); //Cursor increment right no shift
	HAL_Delay(1);
}
/*-------------------------Функция инициализации дисплея--------------------------*/

/*----------------Функция вывода символьного текста на дисплей--------------------*/
void ST7920_Send_symbol_text(uint8_t y, uint8_t x, char *message) {
/// Функция вывода символьного текста на дисплей.
/// Дисплей превращается в матрицу 16*4.
/// \param y - координата по y(от 0 до 3).
/// \param x - координата по x(от 0 до 15).

	switch (y) {
	case 0:
		x |= 0x80;
		break;
	case 1:
		x |= 0x90;
		break;
	case 2:
		x |= 0x88;
		break;
	case 3:
		x |= 0x98;
		break;
	default:
		x |= 0x80;
		break;
	}
	ST7920_Send_command(x);

	for (int i = 0; i < strlen(message); i++) {
		ST7920_Send_data(message[i]);
	}
}
/*----------------Функция вывода символьного текста на дисплей--------------------*/

/*----------------Функция включения/выключения графического режима----------------*/
void ST7920_Graphic_mode(bool enable)   // 1-enable, 0-disable
{
	if (enable) {
		ST7920_Send_command(0x34);  // Т.к. работаем в 8мибитном режиме, то выбираем 0x30 + RE = 1. Переходим в extended instruction.
		HAL_Delay(1);
		ST7920_Send_command(0x36);  // Включаем графический режим
		HAL_Delay(1);
	}

	else if (!enable) {
		ST7920_Send_command(0x30);  // Т.к. работаем в 8мибитном режиме, то выбираем 0x30 + RE = 0. Переходим в basic instruction.
		HAL_Delay(1);
	}
}
/*----------------Функция включения/выключения графического режима----------------*/

/*---------------Функция очистки дисплея в графическом режиме--------------------*/
void ST7920_Clean() {
/// Функция очистки дисплея в графическом режиме
	uint8_t x, y;
	for (y = 0; y < 64; y++) {
		if (y < 32) {
			ST7920_Send_command(0x80 | y);
			ST7920_Send_command(0x80);
		} else {
			ST7920_Send_command(0x80 | (y - 32));
			ST7920_Send_command(0x88);
		}
		for (x = 0; x < 8; x++) {
			ST7920_Send_data(0x00);
			ST7920_Send_data(0x00);
		}
	}
	ST7920_Clean_Frame_buffer();
}
/*---------------Функция очистки дисплея в графическом режиме--------------------*/

/*-------------------Функция вывода изображения на экран дисплея--------------------------*/
void ST7920_Draw_bitmap(const unsigned char *bitmap) {
/// Функция вывода изображения на дисплей
/// Работает с памятью ST7920.
/// \param *bitmap - изображение 128*64. т.е. Буфер из 1024 элементов.
	uint8_t x, y;
	uint16_t i = 0;
	uint8_t Temp, Db;

	for (y = 0; y < 64; y++) {
		for (x = 0; x < 8; x++) {
			if (y < 32) {
				ST7920_Send_command(0x80 | y);				//y(0-31)
				ST7920_Send_command(0x80 | x);				//x(0-8)
			} else {
				ST7920_Send_command(0x80 | (y - 32));		//y(0-31)
				ST7920_Send_command(0x88 | x);				//x(0-8)
			}

			i = ((y / 8) * 128) + (x * 16);
			Db = y % 8;

			Temp = (((bitmap[i] >> Db) & 0x01) << 7) | (((bitmap[i + 1] >> Db) & 0x01) << 6) | (((bitmap[i + 2] >> Db) & 0x01) << 5)
					| (((bitmap[i + 3] >> Db) & 0x01) << 4) | (((bitmap[i + 4] >> Db) & 0x01) << 3) | (((bitmap[i + 5] >> Db) & 0x01) << 2)
					| (((bitmap[i + 6] >> Db) & 0x01) << 1) | (((bitmap[i + 7] >> Db) & 0x01) << 0);
			ST7920_Send_data(Temp);

			Temp = (((bitmap[i + 8] >> Db) & 0x01) << 7) | (((bitmap[i + 9] >> Db) & 0x01) << 6) | (((bitmap[i + 10] >> Db) & 0x01) << 5)
					| (((bitmap[i + 11] >> Db) & 0x01) << 4) | (((bitmap[i + 12] >> Db) & 0x01) << 3) | (((bitmap[i + 13] >> Db) & 0x01) << 2)
					| (((bitmap[i + 14] >> Db) & 0x01) << 1) | (((bitmap[i + 15] >> Db) & 0x01) << 0);

			ST7920_Send_data(Temp);
		}
	}
}
/*-------------------Функция вывода изображения на экран дисплея--------------------------*/

/*---------------------Функция рисования пикселя на экране----------------------------*/
void ST7920_Draw_pixel(uint8_t x, uint8_t y) {
/// Функция рисования точки.
/// param\ x - координата по X(от 0 до 127)
/// paran\ y - координата по Y(от 0 до 63)
	if (y < ST7920_height && x < ST7920_width) {
		Frame_buffer[(x) + ((y / 8) * 128)] |= 0x01 << y % 8;
	}
}
/*---------------------Функция рисования пикселя на экране----------------------------*/

/*---------------------Функция удаления пикселя на экране----------------------------*/
void ST7920_Clean_pixel(uint8_t x, uint8_t y) {
/// Функция удаления точки.
/// param\ x - координата по X(от 0 до 127)
/// paran\ y - координата по Y(от 0 до 63)
	if (y < ST7920_height && x < ST7920_width) {
		Frame_buffer[(x) + ((y / 8) * 128)] &= 0xFE << y % 8;
	}
}
/*---------------------Функция удаления пикселя на экране----------------------------*/

/*---------------------Функция вывода буфера кадра на дисплей------------------------*/
void ST7920_Update(void) {
	/// Функция вывода буфера кадра на дисплей
	/// Подготовьте буфер кадра, перед тем, как обновить изображение
	ST7920_Draw_bitmap(Frame_buffer);
}
/*---------------------Функция вывода буфера кадра на дисплей------------------------*/

/*---------------------Функция вывода символа на дисплей-----------------------------*/
void print_symbol(uint16_t x, uint16_t symbol, bool inversion) {
/// Функция вывода символа на дисплей
/// \param x положение по х (от 0 до 1023)
/// Дисплей поделен по страницам(строка из 8 пикселей)
/// 1 строка: x = 0;
///	2 строка: x = 128;
/// 3 строка: x = 256;
/// 4 строка: x = 384;
/// 5 строка: x = 512;
/// 6 строка: x = 640;
/// 7 строка: x = 786;
/// 8 строка: x = 896;
/// \param symbol - код символа
/// \param inversion - инверсия. 1 - вкл, 0 - выкл.
	for (int i = 0; i <= 8; i++) {
		if (inversion) {
			Frame_buffer[i + x - 1] = ~Font[(symbol * 8) + i];
		} else {
			Frame_buffer[i + x - 1] = Font[(symbol * 8) + i];
		}

	}
}
/*---------------------Функция вывода символа на дисплей-----------------------------*/

/*----------------Функция декодирования UTF-8 в набор символов-----------------*/

void ST7920_Decode_UTF8(uint16_t x, uint8_t y, bool inversion, char *tx_buffer) {
/// Функция декодирования UTF-8 в набор символов и последующее занесение в буфер кадра
/// \param x - координата по х. От 0 до 127
/// \param y - координата по y. от 0 до 7
/// Дисплей поделен по страницам(строка из 8 пикселей)
/// 1 строка: x = 0;
///	2 строка: x = 128;
/// 3 строка: x = 256;
/// 4 строка: x = 384;
/// 5 строка: x = 512;
/// 6 строка: x = 640;
/// 7 строка: x = 786;
/// 8 строка: x = 896;
	x = x + y * 128;
	uint16_t symbol = 0;
	bool flag_block = 0;
	for (int i = 0; i < strlen(tx_buffer); i++) {
		if (tx_buffer[i] < 0xC0) { //Английский текст и символы. Если до русского текста, то [i] <0xD0. Но в font добавлен знак "°"
			if (flag_block) {
				flag_block = 0;
			} else {
				symbol = tx_buffer[i];
				if (inversion) {
					print_symbol(x, symbol - 32, 1); //Таблица UTF-8. Basic Latin. С "пробел" до "z". Инверсия вкл.
				} else {
					print_symbol(x, symbol - 32, 0); //Таблица UTF-8. Basic Latin. С "пробел" до "z". Инверсия выкл.
				}
				x = x + 6;
			}
		}

		else { //Русский текст
			symbol = tx_buffer[i] << 8 | tx_buffer[i + 1];
			if (symbol < 0xD180 && symbol > 0xD081) {
				if (inversion) {
					print_symbol(x, symbol - 53297, 1); //Таблица UTF-8. Кириллица. С буквы "А" до "п". Инверсия вкл.
				} else {
					print_symbol(x, symbol - 53297, 0); //Таблица UTF-8. Кириллица. С буквы "А" до "п". Инверсия выкл.
				}
				x = x + 6;
			} else if (symbol == 0xD081) {
				if (inversion) {
					print_symbol(x, 159, 1); ////Таблица UTF-8. Кириллица. Буква "Ё". Инверсия вкл.
				} else {
					print_symbol(x, 159, 0); ////Таблица UTF-8. Кириллица. Буква "Ё". Инверсия выкл.
				}
				x = x + 6;
			} else if (symbol == 0xD191) {
				if (inversion) {
					print_symbol(x, 160, 1); ////Таблица UTF-8. Кириллица. Буква "ё". Инверсия вкл.
				} else {
					print_symbol(x, 160, 0); ////Таблица UTF-8. Кириллица. Буква "ё". Инверсия выкл.
				}
				x = x + 6;
			} else if (symbol == 0xC2B0) {
				if (inversion) {
					print_symbol(x, 161, 1); ////Таблица UTF-8. Basic Latin. Символ "°". Инверсия вкл.
				} else {
					print_symbol(x, 161, 0); ////Таблица UTF-8. Basic Latin. Символ "°". Инверсия выкл.
				}
				x = x + 6;
			}

			else {
				if (inversion) {
					print_symbol(x, symbol - 53489, 1); //Таблица UTF-8. Кириллица. С буквы "р", начинается сдвиг. Инверсия вкл.
				} else {
					print_symbol(x, symbol - 53489, 0); //Таблица UTF-8. Кириллица. С буквы "р", начинается сдвиг. Инверсия выкл.
				}
				x = x + 6;
			}
			flag_block = 1;
		}
	}
}

/*----------------Функция декодирования UTF-8 в набор символов-----------------*/

/*---------------------Функция инверсии любого места в буфере------------------*/
void ST7920_Inversion(uint16_t x_start, uint16_t x_end) {
/// Функция инверсии любого места в буфере
/// \param x_start - начальная точка по х от 0 до 1024
/// \param x_end - конечная точка по y от 0 до 1024
	for (; x_start < x_end; x_start++) {
		Frame_buffer[x_start] = ~(Frame_buffer[x_start]);
	}
}
/*---------------------Функция инверсии любого места в буфере------------------*/

/*------------------------Функция очистки буфера кадра-------------------------*/
void ST7920_Clean_Frame_buffer(void) {
/// Функция очистки буфера кадра
	memset(Frame_buffer, 0x00, sizeof(Frame_buffer));
}
/*------------------------Функция очистки буфера кадра-------------------------*/

void ST7920_Draw_line(uint8_t x_start, uint8_t y_start, uint8_t x_end, uint8_t y_end) {
	int dx = (x_end >= x_start) ? x_end - x_start : x_start - x_end;
	int dy = (y_end >= y_start) ? y_end - y_start : y_start - y_end;
	int sx = (x_start < x_end) ? 1 : -1;
	int sy = (y_start < y_end) ? 1 : -1;
	int err = dx - dy;

	for (;;) {
		ST7920_Draw_pixel(x_start, y_start);
		if (x_start == x_end && y_start == y_end)
			break;
		int e2 = err + err;
		if (e2 > -dy) {
			err -= dy;
			x_start += sx;
		}
		if (e2 < dx) {
			err += dx;
			y_start += sy;
		}
	}
}

/********************************РАБОТА С ГЕОМЕТРИЧЕСКИМИ ФИГУРАМИ**********************************/

/*--------------------------------Вывести пустотелый прямоугольник---------------------------------*/
void ST7920_Draw_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
/// Вывести пустотелый прямоугольник
/// \param x - начальная точка по оси "x"
/// \param y - начальная точка по оси "y"
/// \param width - ширина прямоугольника
/// \param height - высота прямоугольника

	/*Проверка ширины и высоты*/
	if ((x + width) >= ST7920_width) {
		width = ST7920_width - x;
	}
	if ((y + height) >= ST7920_height) {
		height = ST7920_height - y;
	}

	/*Рисуем линии*/
	ST7920_Draw_line(x, y, x + width, y); /*Верх прямоугольника*/
	ST7920_Draw_line(x, y + height, x + width, y + height); /*Низ прямоугольника*/
	ST7920_Draw_line(x, y, x, y + height); /*Левая сторона прямоугольника*/
	ST7920_Draw_line(x + width, y, x + width, y + height); /*Правая сторона прямоугольника*/
}
/*--------------------------------Вывести пустотелый прямоугольник---------------------------------*/

/*-------------------------------Вывести закрашенный прямоугольник---------------------------------*/
void ST7920_Draw_rectangle_filled(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
/// Вывести закрашенный прямоугольник
/// \param x - начальная точка по оси "x"
/// \param y - начальная точка по оси "y"
/// \param width - ширина прямоугольника
/// \param height - высота прямоугольника

	/*Проверка ширины и высоты*/
	if ((x + width) >= ST7920_width) {
		width = ST7920_width - x;
	}
	if ((y + height) >= ST7920_height) {
		height = ST7920_height - y;
	}

	/*Рисуем линии*/
	for (uint8_t i = 0; i <= height; i++) {
		ST7920_Draw_line(x, y + i, x + width, y + i);
	}
}
/*-------------------------------Вывести закрашенный прямоугольник---------------------------------*/

/*---------------------------------Вывести пустотелую окружность-----------------------------------*/
void ST7920_Draw_circle(uint8_t x, uint8_t y, uint8_t radius) {
/// Вывести пустотелую окружность
/// \param x - точка центра окружности по оси "x"
/// \param y - точка центра окружности по оси "y"
/// \param radius - радиус окружности

	int f = 1 - (int) radius;
	int ddF_x = 1;

	int ddF_y = -2 * (int) radius;
	int x_0 = 0;

	ST7920_Draw_pixel(x, y + radius);
	ST7920_Draw_pixel(x, y - radius);
	ST7920_Draw_pixel(x + radius, y);
	ST7920_Draw_pixel(x - radius, y);

	int y_0 = radius;
	while (x_0 < y_0) {
		if (f >= 0) {
			y_0--;
			ddF_y += 2;
			f += ddF_y;
		}
		x_0++;
		ddF_x += 2;
		f += ddF_x;
		ST7920_Draw_pixel(x + x_0, y + y_0);
		ST7920_Draw_pixel(x - x_0, y + y_0);
		ST7920_Draw_pixel(x + x_0, y - y_0);
		ST7920_Draw_pixel(x - x_0, y - y_0);
		ST7920_Draw_pixel(x + y_0, y + x_0);
		ST7920_Draw_pixel(x - y_0, y + x_0);
		ST7920_Draw_pixel(x + y_0, y - x_0);
		ST7920_Draw_pixel(x - y_0, y - x_0);
	}
}
/*---------------------------------Вывести пустотелую окружность-----------------------------------*/

/*--------------------------------Вывести закрашенную окружность-----------------------------------*/
void ST7920_Draw_circle_filled(int16_t x, int16_t y, int16_t radius) {
/// Вывести закрашенную окружность
/// \param x - точка центра окружности по оси "x"
/// \param y - точка центра окружности по оси "y"
/// \param radius - радиус окружности

	int16_t f = 1 - radius;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * radius;
	int16_t x_0 = 0;
	int16_t y_0 = radius;

	ST7920_Draw_pixel(x, y + radius);
	ST7920_Draw_pixel(x, y - radius);
	ST7920_Draw_pixel(x + radius, y);
	ST7920_Draw_pixel(x - radius, y);
	ST7920_Draw_line(x - radius, y, x + radius, y);

	while (x_0 < y_0) {
		if (f >= 0) {
			y_0--;
			ddF_y += 2;
			f += ddF_y;
		}
		x_0++;
		ddF_x += 2;
		f += ddF_x;

		ST7920_Draw_line(x - x_0, y + y_0, x + x_0, y + y_0);
		ST7920_Draw_line(x + x_0, y - y_0, x - x_0, y - y_0);
		ST7920_Draw_line(x + y_0, y + x_0, x - y_0, y + x_0);
		ST7920_Draw_line(x + y_0, y - x_0, x - y_0, y - x_0);
	}
}
/*--------------------------------Вывести закрашенную окружность-----------------------------------*/

/*-----------------------------------Вывести пустотелый треугольник--------------------------------*/
void ST7920_Draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3) {
/// Вывести пустотелый треугольник
/// \param x_1 - первая точка треугольника. Координата по оси "x"
/// \param y_1 - первая точка треугольника. Координата по оси "y"
/// \param x_2 - вторая точка треугольника. Координата по оси "x"
/// \param y_2 - вторая точка треугольника. Координата по оси "y"
/// \param x_3 - третья точка треугольника. Координата по оси "x"
/// \param y_3 - третья точка треугольника. Координата по оси "y"

	ST7920_Draw_line(x1, y1, x2, y2);
	ST7920_Draw_line(x2, y2, x3, y3);
	ST7920_Draw_line(x3, y3, x1, y1);
}
/*-----------------------------------Вывести пустотелый треугольник--------------------------------*/

/*----------------------------------Вывести закрашенный треугольник--------------------------------*/
void ST7920_Draw_triangle_filled(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3) {
/// Вывести закрашенный треугольник
/// \param x_1 - первая точка треугольника. Координата по оси "x"
/// \param y_1 - первая точка треугольника. Координата по оси "y"
/// \param x_2 - вторая точка треугольника. Координата по оси "x"
/// \param y_2 - вторая точка треугольника. Координата по оси "y"
/// \param x_3 - третья точка треугольника. Координата по оси "x"
/// \param y_3 - третья точка треугольника. Координата по оси "y"

#define ABS(x)   ((x) > 0 ? (x) : -(x))
int16_t deltax = 0;
int16_t deltay = 0;
int16_t x = 0;
int16_t y = 0;
int16_t xinc1 = 0;
int16_t xinc2 = 0;
int16_t yinc1 = 0;
int16_t yinc2 = 0;
int16_t den = 0;
int16_t num = 0;
int16_t numadd = 0;
int16_t numpixels = 0;
int16_t curpixel = 0;

	deltax = ABS(x2 - x1);
	deltay = ABS(y2 - y1);
	x = x1;
	y = y1;

	if (x2 >= x1) {
		xinc1 = 1;
		xinc2 = 1;
	} else {
		xinc1 = -1;
		xinc2 = -1;
	}

	if (y2 >= y1) {
		yinc1 = 1;
		yinc2 = 1;
	} else {
		yinc1 = -1;
		yinc2 = -1;
	}

	if (deltax >= deltay) {
		xinc1 = 0;
		yinc2 = 0;
		den = deltax;
		num = deltax / 2;
		numadd = deltay;
		numpixels = deltax;
	} else {
		xinc2 = 0;
		yinc1 = 0;
		den = deltay;
		num = deltay / 2;
		numadd = deltax;
		numpixels = deltay;
	}

	for (curpixel = 0; curpixel <= numpixels; curpixel++) {
		ST7920_Draw_line(x, y, x3, y3);

		num += numadd;
		if (num >= den) {
			num -= den;
			x += xinc1;
			y += yinc1;
		}
		x += xinc2;
		y += yinc2;
	}
}
/*----------------------------------Вывести закрашенный треугольник--------------------------------*/

/********************************РАБОТА С ГЕОМЕТРИЧЕСКИМИ ФИГУРАМИ**********************************/
