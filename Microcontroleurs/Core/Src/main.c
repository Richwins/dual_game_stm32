/* USER CODE BEGIN Header */
/**
 * @file    main.c
 * @brief   Boucle principale – lecture joystick ADC, chronomètre, gestion UART et LEDs.
 * @ingroup grp_micro_main
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32l0xx_ll_usart.h"
#include "stm32l0xx_ll_adc.h"
#include "stm32l0xx_ll_rcc.h"
#include "stm32l0xx_ll_bus.h"
#include "stm32l0xx_ll_cortex.h"
#include "stm32l0xx_ll_pwr.h"
#include "stm32l0xx_it.h"
#include "stm32l0xx_ll_utils.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t  rxData[256];   /**< Buffer de réception UART temporaire                      */
uint16_t rxLen = 0;     /**< Nombre d'octets reçus dans rxData                        */
uint8_t  cpt   = 0;     /**< Compteur de LEDs éteintes (0 = toutes allumées)          */
uint16_t vx    = 0;     /**< Valeur brute ADC axe X du joystick (PA0, 0–4095)         */
uint16_t vy    = 0;     /**< Valeur brute ADC axe Y du joystick (PA1, 0–4095)         */
uint32_t lastTick = 0;  /**< Dernier tick où 'T' a été envoyé (en ms via timeur())    */
uint32_t timeu = 0;     /**< Timestamp courant retourné par timeur()                  */
uint8_t  btnPressed = 0;/**< État du bouton PA4 (1 = pressé, anti-rebond)             */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
uint16_t ADC_ReadChannel(uint32_t channel);
void Blink(GPIO_TypeDef* GPIOx, uint32_t pin, uint8_t count);
void ResetGameState(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Lit la valeur ADC 12 bits d'un canal donné.
 * @details Réinitialise le périphérique ADC, configure le canal demandé en
 *          mode single-shot, lance la calibration, active l'ADC puis retourne
 *          la valeur convertie sur 12 bits (0–4095).
 * @param  channel  Canal ADC LL à convertir (ex: LL_ADC_CHANNEL_0 pour PA0,
 *                  LL_ADC_CHANNEL_1 pour PA1).
 * @return Valeur ADC 12 bits (0 = 0 V, 4095 = 3,3 V).
 */
uint16_t ADC_ReadChannel(uint32_t channel) {
	/* Reset complet du peripheral ADC */
	LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_ADC1);
	LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_ADC1);

	/* Reconfigure l'ADC */
	LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
	LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
	LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_MODE_NONE);
	LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
	LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
	LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);
	LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
	LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_39CYCLES_5);

	/* Ajoute uniquement le canal voulu */
	LL_ADC_REG_SetSequencerChAdd(ADC1, channel);

	/* Calibration */
	LL_ADC_StartCalibration(ADC1);
	while (LL_ADC_IsCalibrationOnGoing(ADC1))
		;

	/* Active */
	LL_ADC_Enable(ADC1);
	while (!LL_ADC_IsActiveFlag_ADRDY(ADC1))
		;

	/* Convertit */
	LL_ADC_ClearFlag_EOC(ADC1);
	LL_ADC_REG_StartConversion(ADC1);
	uint32_t timeout = 1000000;
	while (!LL_ADC_IsActiveFlag_EOC(ADC1) && timeout--)
		;

	return LL_ADC_REG_ReadConversionData12(ADC1);
}

/**
 * @brief  Fait clignoter une LED un certain nombre de fois.
 * @param  GPIOx  Port GPIO de la LED (ex: GPIOB, GPIOC).
 * @param  pin    Broche LL de la LED (ex: LL_GPIO_PIN_6).
 * @param  count  Nombre de clignotements (chaque cycle = 500 ms).
 */
void Blink(GPIO_TypeDef* GPIOx, uint32_t pin, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        LL_GPIO_ResetOutputPin(GPIOx, pin);
        LL_mDelay(250);
        LL_GPIO_SetOutputPin(GPIOx, pin);
        LL_mDelay(250);
        LL_GPIO_ResetOutputPin(GPIOx, pin);
    }
}

/**
 * @brief  Réinitialise l'état du jeu en fin de manche.
 * @details Fait clignoter le buzzer (PC0) 3 fois, puis rallume
 *          les 3 LEDs de vie (PA9, PC7, PB6) et remet le compteur cpt à 0.
 */
void ResetGameState(void) {
	Blink(GPIOC, LL_GPIO_PIN_0, 3);
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_9);
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_7);
	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_6);
	cpt = 0;
}


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */
	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

	/* SysTick_IRQn interrupt configuration */
	NVIC_SetPriority(SysTick_IRQn, 3);

	/* USER CODE BEGIN Init */
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */
	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_ADC_Init();
	/* USER CODE BEGIN 2 */
	LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_9);
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_7);
	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_6);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {

		LL_mDelay(50);
		timeu = timeur();
		if ((timeu - lastTick) >= 1000) {
			lastTick = timeu;
			USART2_Send((uint8_t*) "T\n", 2);
		}
		/* ── Lecture joystick ────────────────────────────────── */
		vx = ADC_ReadChannel(LL_ADC_CHANNEL_0); /* PA0 = axe X */
		vy = ADC_ReadChannel(LL_ADC_CHANNEL_1); /* PA1 = axe Y */

		if (vx < 1900) {
			char buf[64];
			snprintf(buf, sizeof(buf), "DOWN\n");
			USART2_Send((uint8_t*) buf, strlen(buf));

		}
		if (vx > 2200) {
			char buf[64];
			snprintf(buf, sizeof(buf), "UP\n");
			USART2_Send((uint8_t*) buf, strlen(buf));

		}
		if (vy < 1900) {
			char buf[64];
			snprintf(buf, sizeof(buf), "RIGHT\n");
			USART2_Send((uint8_t*) buf, strlen(buf));

		}
		if (vy > 2200) {
			char buf[64];
			snprintf(buf, sizeof(buf), "LEFT\n");
			USART2_Send((uint8_t*) buf, strlen(buf));

		}

		/* ── Réception UART ──────────────────────────────────── */
		if (USART2_Available() > 0) {
			uint8_t c;
			USART2_Read(&c, 1);

			switch (c) {

			case 'L':
				if (cpt == 0) {
					Blink(GPIOB, LL_GPIO_PIN_6, 3);
				}
				if (cpt == 1) {
					Blink(GPIOC, LL_GPIO_PIN_7, 3);
				}
				if (cpt == 2) {
					Blink(GPIOA, LL_GPIO_PIN_9, 3);
				}
				cpt++;
				break;

			case 'B':
				ResetGameState();
				break;

			default:
				break;
			}
		}

		/* ── Bouton PA4 (actif haut) ─────────────────────────── */
		if (LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_4) == 1) {
			if (btnPressed == 0) {        // front montant seulement
				btnPressed = 1;
				char buf[32];
				snprintf(buf, sizeof(buf), "1\n");
				USART2_Send((uint8_t*) buf, 2);
			}
		} else {
			btnPressed = 0;               // doigt levé → réarme
		}

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END WHILE */
	/* USER CODE END 3 */
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
	while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0) {
	}
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
	while (LL_PWR_IsActiveFlag_VOS() != 0) {
	}
	LL_RCC_HSI_Enable();

	/* Wait till HSI is ready */
	while (LL_RCC_HSI_IsReady() != 1) {

	}
	LL_RCC_HSI_SetCalibTrimming(16);
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

	/* Wait till System clock is ready */
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {

	}

	LL_Init1msTick(16000000);
	LL_SYSTICK_EnableIT();
	LL_SetSystemCoreClock(16000000);
	LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	__disable_irq();
	while (1)
		;
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
