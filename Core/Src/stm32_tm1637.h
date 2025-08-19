#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void tm1637Init(void);
void tm1637DisplayDecimal(int value, int showColon);  // colon: 0/1
void tm1637SetBrightness(int8_t level);               // 0..7; -1 = OFF

#ifdef __cplusplus
}
#endif
