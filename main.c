/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   HW16 final stable version
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  MOTOR_STATE_FWD = 0,
  MOTOR_STATE_OFF1,
  MOTOR_STATE_REV,
  MOTOR_STATE_OFF2
} motor_state_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define INA219_ADDR         (0x40 << 1)
#define INA219_REG_CONFIG   0x00
#define INA219_REG_SHUNT    0x01
#define INA219_REG_BUS      0x02

#define PWM_PERIOD          2400
#define PRINT_INTERVAL_MS   100

#define PWM_TEST_DUTY 900
#define STATE_TIME_MS 300
#define ADC_MIN_LIMIT 380
#define ADC_MAX_LIMIT 2600
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
COM_InitTypeDef BspCOMInit;
ADC_HandleTypeDef hadc1;
FDCAN_HandleTypeDef hfdcan1;
I2C_HandleTypeDef hi2c2;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
volatile uint8_t  g_tick_1ms = 0;
volatile uint32_t g_ms_count = 0;

uint16_t g_adc_val = 0;
uint16_t g_cfg_val = 0;
int16_t  g_shunt_raw = 0;
uint16_t g_bus_raw = 0;
int32_t  g_shunt_mV_x100 = 0;
int32_t  g_current_mA_x100 = 0;

motor_state_t g_motor_state = MOTOR_STATE_FWD;
uint16_t g_state_timer_ms = 0;

uint8_t g_fault_stop = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */
uint16_t read_adc(void);

HAL_StatusTypeDef writeINA219(uint8_t reg, uint16_t value);
HAL_StatusTypeDef readINA219(uint8_t reg, uint16_t *value);
void init_ina219(void);

int16_t read_ina219_shunt_raw(void);
uint16_t read_ina219_bus_raw(void);
int32_t read_ina219_shunt_mV_x100(void);
int32_t read_ina219_current_mA_x100(void);

void motor_forward(uint16_t duty);
void motor_reverse(uint16_t duty);
void motor_off(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint16_t read_adc(void)
{
  uint16_t adc_val = 0;

  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
  adc_val = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);

  return adc_val;
}

HAL_StatusTypeDef writeINA219(uint8_t reg, uint16_t value)
{
  uint8_t data[3];
  data[0] = reg;
  data[1] = (value >> 8) & 0xFF;
  data[2] = value & 0xFF;
  return HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDR, data, 3, 100);
}

HAL_StatusTypeDef readINA219(uint8_t reg, uint16_t *value)
{
  uint8_t data[2];
  HAL_StatusTypeDef status;

  status = HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDR, &reg, 1, 100);
  if (status != HAL_OK) return status;

  status = HAL_I2C_Master_Receive(&hi2c2, INA219_ADDR, data, 2, 100);
  if (status != HAL_OK) return status;

  *value = ((uint16_t)data[0] << 8) | data[1];
  return status;
}

void init_ina219(void)
{
  uint16_t config = 0x399F;

  printf("before INA219 init\r\n");
  if (writeINA219(INA219_REG_CONFIG, config) != HAL_OK)
  {
    printf("INA219 write failed\r\n");
    Error_Handler();
  }
  printf("INA219 init OK\r\n");
}

int16_t read_ina219_shunt_raw(void)
{
  uint16_t raw = 0;
  if (readINA219(INA219_REG_SHUNT, &raw) != HAL_OK)
  {
    return 0;
  }
  return (int16_t)raw;
}

uint16_t read_ina219_bus_raw(void)
{
  uint16_t raw = 0;
  if (readINA219(INA219_REG_BUS, &raw) != HAL_OK)
  {
    return 0;
  }
  return raw;
}

/* 1 raw count = 0.01 mV */
int32_t read_ina219_shunt_mV_x100(void)
{
  return (int32_t)read_ina219_shunt_raw();
}

/* current in 0.01 mA, assuming 0.1 ohm shunt */
int32_t read_ina219_current_mA_x100(void)
{
  int32_t shunt_mV_x100 = read_ina219_shunt_mV_x100();
  return shunt_mV_x100 * 10;
}

void motor_forward(uint16_t duty)
{
  if (duty > PWM_PERIOD) duty = PWM_PERIOD;
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_PERIOD);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_PERIOD - duty);
}

void motor_reverse(uint16_t duty)
{
  if (duty > PWM_PERIOD) duty = PWM_PERIOD;
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_PERIOD - duty);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_PERIOD);
}

void motor_off(void)
{
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PWM_PERIOD);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, PWM_PERIOD);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  printf("\r\n============================\r\n");
  printf("HW16 final stable version\r\n");
  printf("ADC on A0 / PA0\r\n");
  printf("INA219 on I2C2 (SDA=D12, SCL=D11)\r\n");
  printf("TIM1 PWM enabled\r\n");

  init_ina219();

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  motor_off();

  HAL_TIM_Base_Start_IT(&htim2);

  printf("TIM2 interrupt started\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (g_tick_1ms)
    {
      g_tick_1ms = 0;

      /* sensor read */
      g_adc_val = read_adc();
      (void)readINA219(INA219_REG_CONFIG, &g_cfg_val);
      (void)readINA219(INA219_REG_SHUNT, (uint16_t *)&g_shunt_raw);
      (void)readINA219(INA219_REG_BUS, &g_bus_raw);

      g_shunt_mV_x100 = (int32_t)g_shunt_raw;
      g_current_mA_x100 = g_shunt_mV_x100 * 10;

      /* safety limit */
      if ((g_adc_val < ADC_MIN_LIMIT) || (g_adc_val > ADC_MAX_LIMIT))
      {
        g_fault_stop = 1;
        motor_off();
      }
      else
      {
        g_fault_stop = 0;

        /* state machine */
        g_state_timer_ms++;
        if (g_state_timer_ms >= STATE_TIME_MS)
        {
          g_state_timer_ms = 0;
          g_motor_state = (motor_state_t)((g_motor_state + 1) % 4);
        }

        switch (g_motor_state)
        {
          case MOTOR_STATE_FWD:
            motor_forward(PWM_TEST_DUTY);
            break;

          case MOTOR_STATE_OFF1:
            motor_off();
            break;

          case MOTOR_STATE_REV:
            motor_reverse(PWM_TEST_DUTY);
            break;

          case MOTOR_STATE_OFF2:
          default:
            motor_off();
            break;
        }
      }

      /* print every 100 ms */
      if ((g_ms_count % PRINT_INTERVAL_MS) == 0)
      {
        if (g_fault_stop)
        {
          printf("[FAULT] ADC=%u | limits=(%d,%d)\r\n",
                 g_adc_val, ADC_MIN_LIMIT, ADC_MAX_LIMIT);
        }
        else
        {
          printf("[STATE=%d] ADC=%u | CFG=0x%04X | SHUNT raw=%d | BUS raw=0x%04X | Shunt=%ld.%02ld mV | Current=%ld.%02ld mA\r\n",
                 g_motor_state,
                 g_adc_val,
                 g_cfg_val,
                 g_shunt_raw,
                 g_bus_raw,
                 g_shunt_mV_x100 / 100,
                 (g_shunt_mV_x100 >= 0 ? g_shunt_mV_x100 % 100 : (-g_shunt_mV_x100) % 100),
                 g_current_mA_x100 / 100,
                 (g_current_mA_x100 >= 0 ? g_current_mA_x100 % 100 : (-g_current_mA_x100) % 100));
        }
      }
    }
  }
  /* USER CODE END WHILE */
}

/**
  * @brief Period elapsed callback in non blocking mode
  * @param htim TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim2)
  {
    g_tick_1ms = 1;
    g_ms_count++;
    BSP_LED_Toggle(LED1);
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_1CYCLE_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_FDCAN1_Init(void)
{
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = ENABLE;
  hfdcan1.Init.TransmitPause = ENABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 1;
  hfdcan1.Init.NominalSyncJumpWidth = 12;
  hfdcan1.Init.NominalTimeSeg1 = 35;
  hfdcan1.Init.NominalTimeSeg2 = 12;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 6;
  hfdcan1.Init.DataTimeSeg1 = 17;
  hfdcan1.Init.DataTimeSeg2 = 6;
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10805D88;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM1_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2399;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim1);
}

static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 47;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    BSP_LED_Toggle(LED2);
    HAL_Delay(200);
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  while (1)
  {
  }
}
#endif
