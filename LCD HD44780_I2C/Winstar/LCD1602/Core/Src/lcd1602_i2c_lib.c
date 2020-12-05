/*
 * lcd1602_i2c_lib.c
 *
 *  Библиотека для дисплеев фирмы "Winstar"
 *  Created on: Nov 24, 2020
 *      Authors: Oleg Volkov & Konstantin Golinskiy
 *      Создать свой символ: https://www.quinapalus.com/hd44780udg.html
 */

#include "lcd1602_i2c_lib.h"
/*-----------------------------------Настройки----------------------------------*/

#define Adress 0x27 << 1                //Адрес устройства.
extern I2C_HandleTypeDef hi2c1;         //Шина I2C.
bool backlight = true;             //Начальная установка для подсветки вкл/выкл.
char lcd1602_tx_buffer[40] = { 0, }; //глобальный буфер данных. В него записывается текст.
uint8_t global_buffer = 0; //глобальная переменная байта данных, отправляемая дисплею.

/*----Русский алфавит----*/
/*Чего нет, то заменяется латинскими буквами*/

#define RU_B 0xA0  //Б
#define RU_G 0xA1  //Г
#define RU_D 0xE0  //Д
#define RU_Eo 0xA2 //Ё
#define RU_Zh 0xA3 //Ж
#define RU_Z 0xA4  //З
#define RU_I 0xA5 //И
#define RU_J 0xA6 //Й
#define RU_L 0xA7 //Л
#define RU_P 0xA8 //П
#define RU_U 0xA9 //У
#define RU_F 0xAA //Ф
#define RU_Ts 0xD6 //Ц
#define RU_Ch 0xAB //Ч
#define RU_Sh 0xAC //Ш
#define RU_Shch 0xE2 //Щ
#define RU_TZ 0xAD //Ъ
#define RU_Y 0xAE //Ы
#define RU_MZ 0x62 //Ь
#define RU_EE 0xAF //Э
#define RU_Yu 0xB0 //Ю
#define RU_Ya 0xB1 //Я

/*Чего нет, то заменяется латинскими буквами*/

#define RU_b 0xB2 //б
#define RU_v 0xB3 //в
#define RU_g 0xB4 //г
#define RU_d 0xE3 //д
#define RU_eo 0xB5 //ё
#define RU_zh 0xB6 //ж
#define RU_z 0xB7 //з
#define RU_i 0xB8 //и
#define RU_j 0xB9 //й
#define RU_k 0xBA //к
#define RU_l 0xBB //л
#define RU_m 0xBC //м
#define RU_n 0xBD //н
#define RU_p 0xBE //п
#define RU_t 0xBF //т
#define RU_f 0xAA //ф
#define RU_ts 0xE5 //ц
#define RU_ch 0xC0 //ч
#define RU_sh 0xC1 //ш
#define RU_shch 0xE6 //щ
#define RU_tz 0xC2 //ъ
#define RU_y 0xC3 //ы
#define RU_mz 0xC4 //ь
#define RU_ee 0xC5 //э
#define RU_yu 0xC6 //ю
#define RU_ya 0xC7 //я

/*----Русский алфавит----*/
/*-----------------------------------Настройки----------------------------------*/

/*============================Вспомогательные функции============================*/
/*-------------Функция для отправки данных при инициализации дисплея-------------*/
/// Функция предназначена для отправки байта данных по шине i2c. Содержит в себе Delay. Без него инициализация дисплея не проходит.
/// \param *init_Data - байт, например 0x25, где 2 (0010) это DB7-DB4 или DB3-DB0, а 5(0101) это сигналы LED, E, RW, RS соответственно
static void lcd1602_Send_init_Data(uint8_t *init_Data) {
	if (backlight) {
		*init_Data |= 0x08; //Включить подсветку
	} else {
		*init_Data &= ~0x08; //Выключить подсветку
	}
	*init_Data |= 0x04; // Устанавливаем стробирующий сигнал E в 1
	HAL_I2C_Master_Transmit(&hi2c1, Adress, init_Data, 1, 10);
	HAL_Delay(5);
	*init_Data &= ~0x04; // Устанавливаем стробирующий сигнал E в 0
	HAL_I2C_Master_Transmit(&hi2c1, Adress, init_Data, 1, 10);
	HAL_Delay(5);
}
/*-------------Функция для отправки данных при инициализации дисплея-------------*/

/*--------------------Функция отправки байта информации на дисплей---------------*/
/// Функция отправки байта информации на дисплей
/// \param Data - Байт данныйх
static void lcd1602_Write_byte(uint8_t Data) {
	HAL_I2C_Master_Transmit(&hi2c1, Adress, &Data, 1, 10);
}
/*--------------------Функция отправки байта информации на дисплей---------------*/

/*----------------------Функция отправки пол байта информации--------------------*/
/// Функция отправки пол байта информации
/// \*param Data - байт данных
static void lcd1602_Send_cmd(uint8_t Data) {
	Data <<= 4;
	lcd1602_Write_byte(global_buffer |= 0x04); // Устанавливаем стробирующий сигнал E в 1
	lcd1602_Write_byte(global_buffer | Data); // Отправляем в дисплей полученный и сдвинутый байт
	lcd1602_Write_byte(global_buffer &= ~0x04);	// Устанавливаем стробирующий сигнал E в 0.
}
/*----------------------Функция отправки пол байта информации--------------------*/

/*----------------------Функция отправки байта данных----------------------------*/
/// Функция отправки байта данных на дисплей
/// \param Data - байт данных
/// \param mode - отправка команды. 1 - RW = 1(отправка данных). 0 - RW = 0(отправка команды).
static void lcd1602_Send_data_symbol(uint8_t Data, uint8_t mode) {
	if (mode == 0) {
		lcd1602_Write_byte(global_buffer &= ~0x01); // RS = 0
	} else {
		lcd1602_Write_byte(global_buffer |= 0x01); // RS = 1
	}
	uint8_t MSB_Data = 0;
	MSB_Data = Data >> 4; // Сдвигаем полученный байт на 4 позичии и записываем в переменную
	lcd1602_Send_cmd(MSB_Data);	// Отправляем первые 4 бита полученного байта
	lcd1602_Send_cmd(Data);	   // Отправляем последние 4 бита полученного байта
}
/*----------------------Функция отправки байта данных----------------------------*/

/*----------------------Основная функция для отправки данных---------------------*/
/// Функция предназначена для отправки байта данных по шине i2c
/// \param *init_Data - байт, например 0x25, где 2 (0010) это DB7-DB4 или DB3-DB0, а 5(0101) это сигналы LED, E, RW, RS соответственно
static void lcd1602_Send_data(uint8_t *Data) {

	if (backlight) {
		*Data |= 0x08;
	} else {
		*Data &= ~0x08;
	}
	*Data |= 0x04; // устанавливаем стробирующий сигнал E в 1
	HAL_I2C_Master_Transmit(&hi2c1, Adress, Data, 1, 10);
	*Data &= ~0x04; // устанавливаем стробирующий сигнал E в 0
	HAL_I2C_Master_Transmit(&hi2c1, Adress, Data, 1, 10);
}

/*----------------------Основная функция для отправки данных---------------------*/
/*============================Вспомогательные функции============================*/

/*-------------------------Функция инициализации дисплея-------------------------*/
/// Функция инициализации дисплея
void lcd1602_Init(void) {
	/*========Power on========*/
	uint8_t tx_buffer = 0x30;
	/*========Wait for more than 15 ms after Vcc rises to 4.5V========*/
	HAL_Delay(15);
	/*========BF can not be checked before this instruction.========*/
	/*========Function set ( Interface is 8 bits long.========*/
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Wait for more 4.1 ms========*/
	HAL_Delay(5);
	/*========BF can not be checked before this instruction.========*/
	/*========Function set ( Interface is 8 bits long.========*/
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Wait for more 100 microsec========*/
	HAL_Delay(1);
	/*========BF can not be checked before this instruction.========*/
	/*========Function set ( Interface is 8 bits long.========*/
	lcd1602_Send_init_Data(&tx_buffer);

	/*========Включаем 4х-битный интерфейс========*/
	tx_buffer = 0x20;
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Включаем 4х-битный интерфейс========*/

	/*======2 строки, шрифт 5х11======*/
	tx_buffer = 0x20;
	lcd1602_Send_init_Data(&tx_buffer);
	tx_buffer = 0x80;
	lcd1602_Send_init_Data(&tx_buffer);
	/*======2 строки, шрифт 5х11======*/

	/*========Выключить дисплей========*/
	tx_buffer = 0x00;
	lcd1602_Send_init_Data(&tx_buffer);
	tx_buffer = 0x80;
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Выключить дисплей========*/

	/*========Очистить дисплей========*/
	tx_buffer = 0x00;
	lcd1602_Send_init_Data(&tx_buffer);
	tx_buffer = 0x10;
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Очистить дисплей========*/

	/*========Режим сдвига курсора========*/
	tx_buffer = 0x00;
	lcd1602_Send_init_Data(&tx_buffer);
	tx_buffer = 0x30;
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Режим сдвига курсора========*/

	/*========Инициализация завершена. Включить дисплей========*/
	tx_buffer = 0x00;
	lcd1602_Send_init_Data(&tx_buffer);
	tx_buffer = 0xC0;
	lcd1602_Send_init_Data(&tx_buffer);
	/*========Инициализация завершена. Включить дисплей========*/
}

/*-------------------------Функция инициализации дисплея-------------------------*/

/*-------------------------Функция вывода символа на дисплей---------------------*/
/// Функция вывода символа на дисплей
/// \param* symbol - символ в кодировке utf-8
void lcd1602_Print_symbol(uint8_t symbol) {
	uint8_t command;
	command = ((symbol & 0xf0) | 0x09); //Формирование верхнего полубайта в команду для дисплея
	lcd1602_Send_data(&command);
	command = ((symbol & 0x0f) << 4) | 0x09; //Формирование нижнего полубайта в команду для дисплея
	lcd1602_Send_data(&command);
}
/*-------------------------Функция вывода символа на дисплей---------------------*/

/*-------------------------Функция вывода текста на дисплей----------------------*/
/// Функция вывода символа на дисплей
/// \param *message - массив, который отправляем на дисплей.
/// Максимальная длина сообщения - 40 символов.
void lcd1602_Print_text(char *message) {
	for (int i = 0; i < strlen(message); i++) {
		lcd1602_Print_symbol(message[i]);
	}
}
/*-------------------------Функция вывода текста на дисплей----------------------*/

/*-------------------Функция установки курсора для вывода текста----------------*/
/// Функция установки курсора для вывода текста на дисплей
/// \param x - координата по оси x. от 0 до 39.
/// \param y - координата по оси y. от 0 до 3.
/// Видимая область:
/// Для дисплеев 1602 max x = 15, max y = 1.
/// Для дисплеев 2004 max x = 19, max y = 3.
void lcd1602_SetCursor(uint8_t x, uint8_t y) {
	uint8_t command, adr;
	if (y > 3)
		y = 3;
	if (x > 39)
		x = 39;
	if (y == 0) {
		adr = x;
	}
	if (y == 1) {
		adr = x + 0x40;
	}
	if (y == 2) {
		adr = x + 0x14;
	}
	if (y == 3) {
		adr = x + 0x54;
	}
	command = ((adr & 0xf0) | 0x80);
	lcd1602_Send_data(&command);

	command = (adr << 4);
	lcd1602_Send_data(&command);

}
/*-------------------Функция установки курсора для вывода текста----------------*/

/*------------------------Функция перемещения текста влево-----------------------*/
/// Функция перемещения текста влево
/// Если ее повторять с периодичностью, получится бегущая строка
void lcd1602_Move_to_the_left(void) {
	uint8_t command;
	command = 0x18;
	lcd1602_Send_data(&command);

	command = 0x88;
	lcd1602_Send_data(&command);
}
/*------------------------Функция перемещения текста влево-----------------------*/

/*------------------------Функция перемещения текста вправо----------------------*/
/// Функция перемещения текста вправо
/// Если ее повторять с периодичностью, получится бегущая строка
void lcd1602_Move_to_the_right(void) {
	uint8_t command;
	command = 0x18;
	lcd1602_Send_data(&command);

	command = 0xC8;
	lcd1602_Send_data(&command);
}
/*------------------------Функция перемещения текста вправо----------------------*/

/*---------------------Функция включения/выключения подсветки--------------------*/
/// Булевая функция включения/выключения подсветки
/// \param state - состояние подсветки.
/// 1 - вкл. 0 - выкл.
void lcd1602_Backlight(bool state) {
	if (state) {
		backlight = true;
	} else {
		backlight = false;
	}
}
/*---------------------Функция включения/выключения подсветки--------------------*/

/*---------------------Функция создания своего символа-------------------------- */
/// Функция создания своего собственного символа и запись его в память.
/// \param *my_Symbol - массив с символом
/// \param memory_adress - номер ячейки: от 1 до 8. Всего 8 ячеек.
void lcd1602_Create_symbol(uint8_t *my_Symbol, uint8_t memory_adress) {
	lcd1602_Send_data_symbol(((memory_adress * 8) | 0x40), 0);
	for (uint8_t i = 0; i < 8; i++) {
		lcd1602_Send_data_symbol(my_Symbol[i], 1); // Записываем данные побайтово в память
	}
}
/*---------------------Функция создания своего символа-------------------------- */

/*-------------------------Функция очистки дисплея-------------------------------*/

void lcd1602_Clean(void) {
/// Аппаратная функция очистки дисплея.
/// Удаляет весь текст, возвращает курсор в начальное положение.
	uint8_t tx_buffer = 0x00;
	lcd1602_Send_init_Data(&tx_buffer);
	tx_buffer = 0x10;
	lcd1602_Send_init_Data(&tx_buffer);

}
/*-------------------------Функция очистки дисплея-------------------------------*/

void lcd1602_Clean_Text(void) {
/// Альтернативная функция очистки дисплея
/// Заполняет все поле памяти пробелами
/// Работает быстрее, чем lcd1602_Clean, но в отличии от нее не возвращает курсор в начальное положение
	lcd1602_SetCursor(0, 0);
	lcd1602_Print_text("                                        ");
	lcd1602_SetCursor(0, 1);
	lcd1602_Print_text("                                        ");
}
