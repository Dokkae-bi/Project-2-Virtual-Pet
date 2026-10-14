/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : X-NUCLEO-GFX01M2 LCD demo for the NUCLEO-L496ZG-P
  *
  * This demo drives the 2.2" ILI9341 QVGA display on the X-NUCLEO-GFX01M2
  * expansion board over SPI1 and cycles through a few simple graphics demos:
  * color bars, filled/outlined shapes, text, and a live joystick (B1)
  * readout. See Drivers/BSP/GFX01M2 for the small display driver used here.
  *
  * WIRING - the GFX01M2 plugs directly onto the top of this board's CN11/CN12
  * ST Morpho headers. NOTE: ST's own datasheet (DB4236) only lists Nucleo-64
  * boards as compatible with the GFX01M2 - the NUCLEO-L496ZG-P (Nucleo-144)
  * is not officially validated, so UM2750 has no pin table for this exact
  * combination. Pins were instead cross-referenced against this board's own
  * schematic (mb1312-l4xxzx-a03): RESET=PA1, CS=PA9, DC=PB10, SPI1 SCK/MISO/
  * MOSI=PA5/PA6/PA7. See Drivers/BSP/GFX01M2/gfx01m2_conf.h for the full
  * derivation and the PA9/USB_VBUS_Pin sharing note.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ili9341.h"
#include "gfx01m2_conf.h"

/* USER CODE BEGIN Includes */
/* Pet modules. Layering is described in README.md. */
#include "pet_ctl.h"
#include "pet_ui.h"
#include "pet_screen.h"
#include "pet_shake.h"
#include "mpu6050.h"
#include "pet_pot.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
/* I2C1 for the MPU-6050 shake sensor. Kept in a USER CODE block because the
   .ioc does not know about I2C1, so CubeMX would otherwise delete it. */
I2C_HandleTypeDef hi2c1;

/* Pet state. static because the stack is only 2 KB. */
static pet_ctl_t g_pet;
static pet_ui_t  g_ui;

/* How much the pet gets fed, set by the potentiometer in Pot_Poll().
   pet_ui_set_portion() puts the same value in the menu, so what the player sees
   is what the pet actually gets. */
static uint8_t g_portion = PET_FEED_PORTION_MIN;

/* Potentiometer on PF3 (ADC3_IN6). Kept in a USER CODE block for the same
   reason as hi2c1: the .ioc does not know about ADC3. */
#define POT_SAMPLE_MS    20u     /* 50 Hz: faster than a hand turns a knob */
#define POT_TIMEOUT_MS   2u      /* one conversion takes ~7 us */
ADC_HandleTypeDef hadc3;
static pet_pot_t  g_pot;
static uint32_t   g_pot_next_ms = 0u;

/* MPU-6050 shake sensor. LD2 shows whether it is answering. */
#define SHAKE_SAMPLE_MS  10u     /* 100 Hz, matches the sensor's output rate */
#define SHAKE_RETRY_MS   1000u   /* how often to look for a missing sensor */
#define SHAKE_AWAKE_MS   1000u   /* how often to check it hasn't silently reset */
static pet_shake_t g_shake;
static bool        g_shake_online   = false;
static uint32_t    g_shake_next_ms  = 0u;
static uint32_t    g_shake_awake_ms = 0u;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static void MX_I2C1_Init(void);
static pet_input_t Input_Read(void);
static bool Shake_Poll(uint32_t now);
static void Shake_Offline(uint32_t now);
static void I2C1_Recover(void);
static void MX_ADC3_Init(void);
static void Pot_Supply_Init(void);
static void Pot_Poll(uint32_t now);
/* USER CODE END PFP */

/* Joystick (B1) bit flags returned by Joystick_Read() */
#define JOY_LEFT_MASK   (1U << 0)
#define JOY_CENTER_MASK (1U << 1)
#define JOY_DOWN_MASK   (1U << 2)
#define JOY_RIGHT_MASK  (1U << 3)
#define JOY_UP_MASK     (1U << 4)

static uint8_t Joystick_Read(void)
{
  uint8_t state = 0;

  /* All five directions are active low (see gfx01m2_conf.h) */
  if (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET)     { state |= JOY_LEFT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_CENTER_GPIO_Port, JOY_CENTER_Pin) == GPIO_PIN_RESET) { state |= JOY_CENTER_MASK; }
  if (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET)     { state |= JOY_DOWN_MASK; }
  if (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET)   { state |= JOY_RIGHT_MASK; }
  if (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET)         { state |= JOY_UP_MASK; }

  return state;
}

/* USER CODE BEGIN 0 */
/**
  * @brief  Turn the joystick into one logical event for pet_ui.
  *
  * Reports a press only on the transition from released to held. This loop runs
  * far faster than anyone can tap, so returning the held state every pass would
  * act on a single press hundreds of times.
  *
  * LEFT and RIGHT are mapped for completeness; pet_ui reserves them and does
  * nothing with them yet.
  *
  * @retval PET_INPUT_NONE when nothing new was pressed.
  */
static pet_input_t Input_Read(void)
{
  static uint8_t previous = 0U;

  const uint8_t now     = Joystick_Read();
  const uint8_t pressed = (uint8_t)(now & (uint8_t)(~previous));
  pet_input_t   event   = PET_INPUT_NONE;

  previous = now;

  if      ((pressed & JOY_UP_MASK)     != 0U) { event = PET_INPUT_UP;     }
  else if ((pressed & JOY_DOWN_MASK)   != 0U) { event = PET_INPUT_DOWN;   }
  else if ((pressed & JOY_LEFT_MASK)   != 0U) { event = PET_INPUT_LEFT;   }
  else if ((pressed & JOY_RIGHT_MASK)  != 0U) { event = PET_INPUT_RIGHT;  }
  else if ((pressed & JOY_CENTER_MASK) != 0U) { event = PET_INPUT_SELECT; }

  return event;
}

/**
  * @brief  Sample the MPU-6050 when one is due and report a completed shake.
  *
  * A missing or unplugged sensor is not an error for the pet: it just keeps
  * running without shake input, LD2 goes off, and the sensor is looked for again
  * once a second, so plugging it back in recovers without a reset.
  *
  * Two quieter failures are handled too. A glitch on the wires can leave the
  * I2C bus looking busy forever, so every failure also frees the bus and
  * restarts I2C1 (I2C1_Recover). And a sensor that loses power for a moment
  * comes back asleep, reading zeros while still answering, so once a second it
  * is asked whether it is still awake.
  *
  * @retval true on the one call where a shake completes.
  */
static bool Shake_Poll(uint32_t now)
{
  if ((int32_t)(now - g_shake_next_ms) < 0)
  {
    return false;
  }

  if (!g_shake_online)
  {
    if (mpu6050_init(&hi2c1) != HAL_OK)
    {
      Shake_Offline(now);
      return false;
    }
    g_shake_online   = true;
    g_shake_next_ms  = now + SHAKE_SAMPLE_MS;
    g_shake_awake_ms = now + SHAKE_AWAKE_MS;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    pet_shake_init(&g_shake);
    return false;
  }

  if ((int32_t)(now - g_shake_awake_ms) >= 0)
  {
    if (mpu6050_check_awake(&hi2c1) != HAL_OK)
    {
      Shake_Offline(now);
      return false;
    }
    g_shake_awake_ms = now + SHAKE_AWAKE_MS;
  }

  int16_t accel[3];
  if (mpu6050_read_accel(&hi2c1, accel) != HAL_OK)
  {
    Shake_Offline(now);
    return false;
  }

  g_shake_next_ms = now + SHAKE_SAMPLE_MS;
  return pet_shake_update(&g_shake, accel[0], accel[1], accel[2], now);
}

/**
  * @brief  Mark the sensor missing, turn LD2 off, and get the bus ready to look
  *         for it again in SHAKE_RETRY_MS.
  */
static void Shake_Offline(uint32_t now)
{
  g_shake_online  = false;
  g_shake_next_ms = now + SHAKE_RETRY_MS;
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
  I2C1_Recover();
}

/* Roughly 20 us at 56.8 MHz: well over the 5 us an I2C half-clock needs. */
static void I2C1_BusDelay(void)
{
  for (volatile uint32_t n = 0u; n < 300u; n++) { }
}

/**
  * @brief  Free a stuck I2C bus and restart I2C1.
  *
  * HAL clears its own state after a failed transfer but leaves the peripheral
  * enabled, and its BUSY flag only clears on a STOP. A START with no STOP (a
  * wire glitch, or a reset in the middle of a read) therefore wedges it, and
  * every later call waits out HAL's 25 ms busy timeout and fails. A slave
  * caught mid-byte can also hold SDA low indefinitely.
  *
  * The standard fix (I2C spec, "bus clear"): take PB8/PB9 as plain open-drain
  * GPIO, clock SCL nine times so any slave finishes its byte and lets go of SDA,
  * send a STOP, then re-initialise I2C1, which puts the pins back on AF4.
  * Takes well under a millisecond and runs at most once per SHAKE_RETRY_MS.
  */
static void I2C1_Recover(void)
{
  (void)HAL_I2C_DeInit(&hi2c1);

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_SET);
  GPIO_InitStruct.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  for (uint32_t i = 0u; i < 9u; i++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);   /* SCL low  */
    I2C1_BusDelay();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);     /* SCL high */
    I2C1_BusDelay();
  }

  /* STOP: SDA rises while SCL is high. */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
  I2C1_BusDelay();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
  I2C1_BusDelay();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
  I2C1_BusDelay();

  MX_I2C1_Init();
}

/**
  * @brief I2C1 Initialization Function - the MPU-6050 shake sensor.
  *        SCL = PB8, SDA = PB9, 100 kHz standard mode.
  *
  * TIMING 0x60420F13 is the RM0351 100 kHz table entry (SCLL 0x13, SCLH 0x0F,
  * SDADEL 2, SCLDEL 4) with PRESC raised from 3 to 6. That keeps the prescaled
  * clock near 4 MHz from PCLK1 = 28.4 MHz (28.4 / 7 = 4.06 MHz).
  *
  * HAL_I2C_Init only fails on bad parameters, never because the sensor is
  * missing, so unplugging the GY-521 can't land in Error_Handler from here.
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance              = I2C1;
  hi2c1.Init.Timing           = 0x60420F13;
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Powers the potentiometer from two GPIO pins, so it needs no wires to
  *        the 3V3 and GND pins: PF11 drives one end high and PF15 the other
  *        end low. A 10 kOhm pot draws 3.3 V / 10 kOhm = 0.33 mA, far below
  *        what a pin can supply. The wiper is read on PF3 (see MX_ADC3_Init).
  * @retval None
  */
static void Pot_Supply_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOF_CLK_ENABLE();

  /* Set the levels before the pins become outputs, so neither glitches. */
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_15, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = GPIO_PIN_11 | GPIO_PIN_15;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

/**
  * @brief ADC3 Initialization Function - the feeding-portion potentiometer.
  *        PF3 = ADC3_IN6, 12-bit, single software-triggered conversion.
  *
  * Clock: SYSCLK 56.8 MHz / 4 = 14.2 MHz. Sampling 92.5 cycles (6.5 us) is
  * plenty for a 10 kOhm pot; a conversion is (92.5 + 12.5) / 14.2 MHz = 7.4 us.
  * Calibrated once at start-up, as RM0351 requires before the first conversion.
  * @retval None
  */
static void MX_ADC3_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc3.Instance                   = ADC3;
  hadc3.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV4;
  hadc3.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc3.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc3.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait      = DISABLE;
  hadc3.Init.ContinuousConvMode    = DISABLE;
  hadc3.Init.NbrOfConversion       = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  hadc3.Init.OversamplingMode      = DISABLE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel      = ADC_CHANNEL_6;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  sConfig.SingleDiff   = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset       = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Read the potentiometer when a reading is due and update the portion.
  *
  * The portion goes to both the menu (shown next to Feed) and the Feed action,
  * so the two can't disagree. pet_pot smooths the reading and adds hysteresis,
  * so the menu only redraws when the knob has really moved.
  *
  * A failed conversion keeps the last portion: Feed still works, it just can't
  * be resized until the ADC answers again. A disconnected wiper leaves PF3
  * floating, which reads as a random portion - check the wiring if the number
  * wanders on its own.
  */
static void Pot_Poll(uint32_t now)
{
  if ((int32_t)(now - g_pot_next_ms) < 0)
  {
    return;
  }
  g_pot_next_ms = now + POT_SAMPLE_MS;

  if (HAL_ADC_Start(&hadc3) != HAL_OK)
  {
    return;
  }
  if (HAL_ADC_PollForConversion(&hadc3, POT_TIMEOUT_MS) == HAL_OK)
  {
    const uint8_t portion = pet_pot_update(&g_pot, (uint16_t)HAL_ADC_GetValue(&hadc3));
    if (portion != g_portion)
    {
      g_portion = portion;
      pet_ui_set_portion(&g_ui, portion);
    }
  }
  (void)HAL_ADC_Stop(&hadc3);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();

  /* Initialize the LCD */
  ILI9341_Init(&hspi1);

  /* USER CODE BEGIN 2 */
  MX_I2C1_Init();
  Pot_Supply_Init();
  MX_ADC3_Init();
  pet_pot_init(&g_pot);

  pet_ctl_init(&g_pet, HAL_GetTick());
  pet_ui_init(&g_ui);
  pet_ui_set_portion(&g_ui, g_portion);
  pet_screen_init(&g_pet.pet, &g_pet.anim, &g_ui);
  /* USER CODE END 2 */

  /*
   * Main loop. Deliberately no HAL_Delay in here: every part of the pet is
   * driven by elapsed time, so blocking would freeze its clock and the
   * animation. Nothing is redrawn unless it actually changed, so spinning
   * freely costs nothing on the SPI bus.
   */
  while (1)
  {
    /* USER CODE BEGIN WHILE */
    const uint32_t now = HAL_GetTick();

    pet_ctl_tick(&g_pet, now);

    /* Before the input is handled, so a Feed this pass uses the knob's
       current position. */
    Pot_Poll(now);

    const pet_ui_result_t result = pet_ui_handle(&g_ui, Input_Read(), &g_pet.pet);

    if (result.restart)
    {
      pet_ctl_restart(&g_pet, now);
    }
    else if (result.action != PET_ACTION_NONE)
    {
      (void)pet_ctl_action(&g_pet, result.action, g_portion, now);
    }

    /* Shaking the MPU-6050 plays with the pet, the same as choosing Play from
       the menu. Ignored while the menu is open, so it can't act behind it. A
       dead or runaway pet refuses it inside pet_ctl_action. Polled every pass
       so a shake made with the menu open is dropped rather than queued. */
    if (Shake_Poll(now) && (pet_ui_screen(&g_ui) == PET_SCREEN_PET))
    {
      (void)pet_ctl_action(&g_pet, PET_ACTION_PLAY, 1u, now);
    }

    pet_screen_update(&g_pet.pet, &g_pet.anim, &g_ui);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* USER CODE END 3 */
  }
}

/**
  * @brief SPI1 Initialization Function
  *        SCK = PA5, MISO = PA6, MOSI = PA7 - connects to the GFX01M2 LCD.
  * @retval None
  */
static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  /* PCLK2 is now ~56.8 MHz (HCLK was previously quartered by a stray AHB
   * DIV4 - see SystemClock_Config). /32 keeps the same ~1.775 MHz target
   * that gave comfortable margin over the ILI9341's 10 MHz write ceiling
   * on this jumper-wired, unsupported-board setup. */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 71;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  /* DIV4 here previously left HCLK at 56.8/4 = 14.2 MHz despite the PLL
   * being configured for 56.8 MHz - DIV1 makes HCLK match the PLL output. */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /* HCLK is now ~56.8 MHz - VOS Range 1 requires 3 wait states for
   * 48-64 MHz (RM0351), not the 0 WS that was only valid up to 16 MHz. */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin, GPIO_PIN_RESET);

  /* LCD CS/RESET idle high, DC state does not matter until the first transfer */
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_OverCurrent_Pin SMPS_PG_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin|SMPS_PG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : USB_PowerSwitchOn_Pin SMPS_V1_Pin SMPS_EN_Pin SMPS_SW_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin|SMPS_V1_Pin|SMPS_EN_Pin|SMPS_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CS_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DC is on a different port than LCD_CS (GPIOB vs GPIOA) - must be a separate HAL_GPIO_Init call */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin (X-NUCLEO-GFX01M2) */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_LEFT_Pin JOY_DOWN_Pin JOY_RIGHT_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_LEFT_Pin|JOY_DOWN_Pin|JOY_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : JOY_CENTER_Pin JOY_UP_Pin (X-NUCLEO-GFX01M2 joystick) */
  GPIO_InitStruct.Pin = JOY_CENTER_Pin|JOY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
