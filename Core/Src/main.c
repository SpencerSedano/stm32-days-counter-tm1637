/* Minimal main for Nucleo-F446RE + TM1637 on PC0/PC1 */
#include "stm32f4xx_hal.h"
#include "stm32_tm1637.h"

/* --- Prototypes --- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);

/* RTC */
static RTC_HandleTypeDef hrtc;

/* --- Simple day-count helpers --- */
static int daysFromCivil(int y, unsigned m, unsigned d){
  y -= (m <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2)/5 + d - 1;
  unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;
  return era * 146097 + (int)doe - 1;
}
static int getDaysSinceStart(void){
  RTC_TimeTypeDef t; RTC_DateTypeDef d;
  HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
  int start = daysFromCivil(2023, 1, 10);
  int now   = daysFromCivil(2000 + d.Year, d.Month, d.Date);
  return now - start;
}

/* --- main --- */
int main(void){
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_RTC_Init();

  /* (Optional) precise µs delay via DWT if you want to switch driver later
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
  */

  tm1637Init();
  tm1637SetBrightness(5);

  /* quick probe */
  tm1637DisplayDecimal(8888, 0); HAL_Delay(300);
  tm1637DisplayDecimal(0,    0); HAL_Delay(300);
  tm1637DisplayDecimal(1234, 1); HAL_Delay(600);

  while(1){
    int days = getDaysSinceStart();
    tm1637DisplayDecimal(days % 10000, 0);
    HAL_Delay(500);
  }
}

/* --- Clocks @ 84 MHz from HSI + PLL; LSE on for RTC --- */
void SystemClock_Config(void){
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSIState       = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSEState       = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM       = 16;
  RCC_OscInitStruct.PLL.PLLN       = 336;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ       = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }
}

/* --- GPIO: just ensure port clocks for C are enabled (driver takes care of pin mode) --- */
static void MX_GPIO_Init(void){
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Idle high early (open-drain 'SET' = release high via pull-up) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);

  /* You can also init PC13 (user button) and PA5 (LD2) if you need them; not required here. */
}

/* --- RTC using LSE, 24h --- */
static void MX_RTC_Init(void){
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv   = 127;
  hrtc.Init.SynchPrediv    = 255;
  hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK) { Error_Handler(); }

  /* Always set a known date/time here (you can remove this after first boot if you want persistence) */
  RTC_DateTypeDef d = {0}; RTC_TimeTypeDef t = {0};
  d.Year=25; d.Month=RTC_MONTH_AUGUST; d.Date=19; d.WeekDay=RTC_WEEKDAY_SATURDAY;
  t.Hours=12; t.Minutes=0; t.Seconds=0;
  HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
  HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
}

/* --- Error handler --- */
void Error_Handler(void){
  __disable_irq();
  while(1){}
}
