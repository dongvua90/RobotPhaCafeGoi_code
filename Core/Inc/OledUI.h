/*
 * OledUI.h
 *
 *  Created on: Jun 15, 2026
 *      Author: Pika
 */

#ifndef INC_OLEDUI_H_
#define INC_OLEDUI_H_

#include "main.h"

extern const unsigned char icon_cafe_full_42x41 [];
extern const unsigned char icon_cafe_mid_42x41 [];
extern const unsigned char icon_cafe_low_42x41 [];

void Ui_RenderBoot();
int HomeScreen();
void UI_Brewing(uint8_t step);
void BrewCompleteScreen();
void PacketEditorScreen();
void WarningScreen(WarningState_t warning);


#endif /* INC_OLEDUI_H_ */
