/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l0xx_it.c
  * @brief   Interrupt Service Routines
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ── Ring-buffers RX / TX ────────────────────────────────────────────────── */
#define RX_BUF_SIZE  256
#define TX_BUF_SIZE  512

static volatile uint8_t  rxBuf[RX_BUF_SIZE];
static volatile uint16_t rxHead  = 0;
static volatile uint16_t rxTail  = 0;
static volatile uint16_t rxCount = 0;

static volatile uint8_t  txBuf[TX_BUF_SIZE];
static volatile uint16_t txHead  = 0;
static volatile uint16_t txTail  = 0;
static volatile uint16_t txCount = 0;
volatile uint32_t sysTick_ms;

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1) {}
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */
  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */
  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */
  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */
  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
	sysTick_ms ++;
  /* USER CODE END SysTick_IRQn 0 */

  /* USER CODE BEGIN SysTick_IRQn 1 */
  /* USER CODE END SysTick_IRQn 1 */
}

uint32_t timeur(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */
	return sysTick_ms;
  /* USER CODE END SysTick_IRQn 0 */

  /* USER CODE BEGIN SysTick_IRQn 1 */
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32L0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles USART2 global interrupt / USART2 wake-up interrupt through EXTI line 26.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* ── Overrun ──────────────────────────────────────────────────────────── */
  if (LL_USART_IsActiveFlag_ORE(USART2))
      LL_USART_ClearFlag_ORE(USART2);

  /* ── RX : stocke dans le ring-buffer, le main gère le switch ─────────── */
  if (LL_USART_IsActiveFlag_RXNE(USART2))
  {
      uint8_t c = LL_USART_ReceiveData8(USART2);
      if (rxCount < RX_BUF_SIZE)
      {
          rxBuf[rxHead] = c;
          rxHead  = (rxHead + 1) % RX_BUF_SIZE;
          rxCount++;
      }
  }

  /* ── TX : envoie depuis le ring-buffer ───────────────────────────────── */
  if (LL_USART_IsActiveFlag_TXE(USART2) &&
      LL_USART_IsEnabledIT_TXE(USART2))
  {
      if (txCount > 0)
      {
          LL_USART_TransmitData8(USART2, txBuf[txHead]);
          txHead  = (txHead + 1) % TX_BUF_SIZE;
          txCount--;
      }
      else
      {
          LL_USART_DisableIT_TXE(USART2);
      }
  }

  /* USER CODE END USART2_IRQn 0 */
  /* USER CODE BEGIN USART2_IRQn 1 */
  /* USER CODE END USART2_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief  Enfile des octets dans le ring-buffer TX et démarre l'émission.
  */
void USART2_Send(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        while (txCount >= TX_BUF_SIZE);

        txBuf[txTail] = data[i];
        txTail = (txTail + 1) % TX_BUF_SIZE;

        __disable_irq();
        txCount++;
        __enable_irq();
    }
    LL_USART_EnableIT_TXE(USART2);
}

/**
  * @brief  Lit jusqu'à maxLen octets depuis le ring-buffer RX.
  * @retval Nombre d'octets lus
  */
uint16_t USART2_Read(uint8_t *dst, uint16_t maxLen)
{
    uint16_t read = 0;
    while (read < maxLen && rxCount > 0)
    {
        dst[read++] = rxBuf[rxTail];
        rxTail = (rxTail + 1) % RX_BUF_SIZE;

        __disable_irq();
        rxCount--;
        __enable_irq();
    }
    return read;
}

/**
  * @brief  Retourne le nombre d'octets disponibles en RX.
  */
uint16_t USART2_Available(void)
{
    return rxCount;
}

/* USER CODE END 1 */
