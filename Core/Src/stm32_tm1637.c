#include "stm32f4xx_hal.h"
#include "stm32_tm1637.h"

/* === Pin mapping: PC0 = CLK, PC1 = DIO === */
#define TM_CLK_PORT GPIOC
#define TM_DIO_PORT GPIOC
#define TM_CLK_PIN  GPIO_PIN_0
#define TM_DIO_PIN  GPIO_PIN_1
#define TM_CLK_EN() __HAL_RCC_GPIOC_CLK_ENABLE()
#define TM_DIO_EN() __HAL_RCC_GPIOC_CLK_ENABLE()

/* 7-seg map */
static const uint8_t SEG[] = {
  0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,  // 0..7
  0x7f,0x6f,0x77,0x7c,0x39,0x5e,0x79,0x71,  // 8..9, A..F
  0x00                                       // blank
};

/* --- tiny µs delay (tolerant enough for TM1637) --- */
static void dly_us(unsigned us){
  for(; us>0; --us){
    for(volatile int i=0;i<12;i++){ __asm__ __volatile__("nop"); }
  }
}

/* --- GPIO helpers --- */
static inline void CLK_H(void){ HAL_GPIO_WritePin(TM_CLK_PORT, TM_CLK_PIN, GPIO_PIN_SET); }
static inline void CLK_L(void){ HAL_GPIO_WritePin(TM_CLK_PORT, TM_CLK_PIN, GPIO_PIN_RESET); }
static inline void DIO_H(void){ HAL_GPIO_WritePin(TM_DIO_PORT, TM_DIO_PIN, GPIO_PIN_SET); }
static inline void DIO_L(void){ HAL_GPIO_WritePin(TM_DIO_PORT, TM_DIO_PIN, GPIO_PIN_RESET); }

static void DIO_to_in(void){
  GPIO_InitTypeDef g={0};
  g.Pin   = TM_DIO_PIN;
  g.Mode  = GPIO_MODE_INPUT;
  g.Pull  = GPIO_NOPULL;                 // rely on module pull-ups
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TM_DIO_PORT, &g);
}
static void DIO_to_outOD(void){
  GPIO_InitTypeDef g={0};
  g.Pin   = TM_DIO_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_OD;
  g.Pull  = GPIO_NOPULL;                 // IMPORTANT at 5V
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TM_DIO_PORT, &g);
}

/* --- bus primitives --- */
static void start(void){
  CLK_H(); DIO_H(); dly_us(3);
  DIO_L();          dly_us(3);
  CLK_L();          dly_us(3);
}
static void stop_(void){
  CLK_L(); DIO_L(); dly_us(3);
  CLK_H();          dly_us(3);
  DIO_H();          dly_us(3);
}
static uint8_t readAck(void){
  uint8_t ack;
  CLK_L();
  DIO_to_in(); DIO_H(); dly_us(3);      // release line
  CLK_H(); dly_us(3);
  ack = (uint8_t)HAL_GPIO_ReadPin(TM_DIO_PORT, TM_DIO_PIN); // 0 = ACK
  CLK_L();
  DIO_to_outOD();
  return ack;
}
static void writeByte(uint8_t b){
  for(int i=0;i<8;i++){
    CLK_L();
    (b & 0x01) ? DIO_H() : DIO_L();
    dly_us(3);
    CLK_H(); dly_us(3);
    b >>= 1;
  }
  (void)readAck();
}

/* --- API --- */
void tm1637Init(void){
  TM_CLK_EN(); TM_DIO_EN();

  /* idle high */
  CLK_H(); DIO_H();

  GPIO_InitTypeDef g={0};
  g.Pin   = TM_CLK_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_OD;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TM_CLK_PORT, &g);

  g.Pin   = TM_DIO_PIN;
  g.Mode  = GPIO_MODE_OUTPUT_OD;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TM_DIO_PORT, &g);

  tm1637SetBrightness(3);
}

void tm1637SetBrightness(int8_t level){
  uint8_t cmd = 0x80;                       // display OFF
  if(level >= 0){
    if(level > 7) level = 7;
    cmd = (uint8_t)(0x88 | (level & 7));    // display ON + brightness
  }
  start(); writeByte(cmd); stop_();
}

void tm1637DisplayDecimal(int value, int showColon){
  uint8_t d[4];
  int v = value; if(v < 0) v = -v;

  /* right-aligned, zero-padded */
  for(int i=0;i<4;i++){ d[i] = SEG[v % 10]; v /= 10; }
  if(showColon) d[1] |= 0x80;               // many modules use bit7 of digit1 for colon

  /* data cmd: auto-increment */
  start(); writeByte(0x40); stop_();
  /* address cmd: start at 0 */
  start(); writeByte(0xC0);
  /* send MS digit first */
  writeByte(d[3]); writeByte(d[2]); writeByte(d[1]); writeByte(d[0]);
  stop_();
}
