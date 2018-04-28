/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l4xx_hal.h"

/* USER CODE BEGIN Includes */
/* vim: set ai et ts=4 sw=4: */

// #include "ice40_config_uncompressed.h"
#include "ice40_config_compressed.h"

#include "ssd1306_tests.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

void blink_status_led(int num) {
    // blink status LED (SS) `num` times
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    HAL_Delay(200);

    for(int i = 0; i < num; i++) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_15); // on
        HAL_Delay(200);
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_15); // off
        HAL_Delay(200);
    }
}

// Write to SPI in multiple chunks (HAL_SPI_Transmit length is limited to 16 bits)
// Copy-pasted from iceboot
int spi_write(uint8_t *p, uint32_t len) {
    int ret;
    uint16_t n;

    ret = HAL_OK;
    n = 0x8000;
    while (len > 0) {
        if (len < n)
            n = len;
        ret = HAL_SPI_Transmit(&hspi1, p, n, HAL_MAX_DELAY);
        if (ret != HAL_OK)
            return ret;
        len -= n;
        p += n;
    }
    return ret;
}

// 0 = OK , < 0 = error
int ice40_configure_uncompressed(const unsigned char* bin, unsigned int bin_size) {
    // SS = LOW
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

    // CRESET = LOW
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    // Keep CRESET low for at least 200 nsec
    HAL_Delay(1);
    // CRESET = HIGH
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    // Give ICE40 at least 800 usec to clear internal configuration memory
    HAL_Delay(850);

    // check CDONE is LOW
    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) != GPIO_PIN_RESET) {
        return -1;
    }

    // send the configuration over SPI
    int res = spi_write((uint8_t*)bin, bin_size);
    if(res != HAL_OK) {
        return -2;
    }

    // send at least 49 dummy bits
    unsigned char dummy[7] = {0};
    res = spi_write(dummy, sizeof(dummy));
    if(res != HAL_OK) {
        return -3;
    }

    // check CDONE is HIGH
    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) != GPIO_PIN_SET) {
        return -4;
    }

    return 0;
}

// 0 = OK , < 0 = error
int ice40_configure_compressed(const unsigned char* bin, unsigned int bin_size) {
    // SS = LOW
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

    // CRESET = LOW
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    // Keep CRESET low for at least 200 nsec
    HAL_Delay(1);
    // CRESET = HIGH
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    // Give ICE40 at least 800 usec to clear internal configuration memory
    HAL_Delay(850);

    // check CDONE is LOW
    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) != GPIO_PIN_RESET) {
        return -1;
    }

    uint32_t offset = 0;
    uint8_t zero_buff[128] = { 0 };
    uint8_t curr_code;
    int res;

    while(offset < bin_size) {
        curr_code = (uint8_t)(bin[offset++]);

        if((curr_code & (1 << 7)) == 0) { // zeros
            uint8_t zeros_cnt = (curr_code & 0x7F) + 1;
            res = spi_write(zero_buff, zeros_cnt);
            if(res != HAL_OK) {
                return -2;
            }
        } else if((curr_code & 0xC0) == 0x80) { // even more zeros
            if(offset == bin_size) { // end of data while reading extra code
                return -3;
            }

            uint8_t extra_code = (uint8_t)(bin[offset++]);
            uint16_t zeros_cnt = (((uint16_t)(curr_code & 0x3F)) << 8) | (uint16_t)extra_code;
            zeros_cnt += 129;

            while(zeros_cnt > 0) {
                uint8_t nwrite = zeros_cnt > sizeof(zero_buff) ? sizeof(zero_buff) : zeros_cnt;
                res = spi_write(zero_buff, nwrite);
                if(res != HAL_OK) {
                    return -4;
                }
                zeros_cnt -= nwrite;
            }
        } else { // (curr_code & 0xC0) == 0xC0, regular bytes
            uint8_t regular_cnt = (curr_code & 0x3F) + 1;
            if(offset + regular_cnt > bin_size) {
                return -5;
            }

            res = spi_write((uint8_t*)(bin + offset), regular_cnt);
            if(res != HAL_OK) {
                return -6;
            }
            offset += regular_cnt;
        }
    }

    // send at least 49 dummy bits
    unsigned char dummy[7] = {0};
    res = spi_write(dummy, sizeof(dummy));
    if(res != HAL_OK) {
        return -7;
    }

    // check CDONE is HIGH
    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) != GPIO_PIN_SET) {
        return -8;
    }

    return 0;
}

void loop() {
    ssd1306_TestAll();
    HAL_Delay(5*1000);
}

void init() {
    // notify of configuration start
    blink_status_led(2); 

    // int code = ice40_configure_uncompressed(main_bin, main_bin_len);
    int code = ice40_configure_compressed(main_bin_enc2, main_bin_enc2_len);
    
    // notify of configuration end (code == 0) or error (code < 0)
    blink_status_led(2 - code); 

    if(code < 0) {
        HAL_Delay(HAL_MAX_DELAY);
    }
}

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  init();
  while (1)
  {
    loop();
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the main internal regulator output voltage 
    */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA10 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler_Debug */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
