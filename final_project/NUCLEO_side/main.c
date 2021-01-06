//
//  ******************************************************************************
//  @file    main.c
//  @author  CPL (Pavel Paces, based on STM examples and HAL library)
//  @version V0.0
//  @date    02-November-2016
//  @brief   Adafruit 802 display shield and serial line over ST-Link example
//           Nucleo STM32F401RE USART2 (Tx PA.2, Rx PA.3)
//
//  ******************************************************************************
//

//
// Basic framework
#include "stm32f4xx.h"
#include "stm32f4xx_nucleo.h"

#include "stm32_adafruit_lcd.h"

#include <string.h>
#include <stdio.h> // only used for sscanf


/* Definition for I2Cx clock resources */
/*
#define I2Cx                            I2C1
#define I2Cx_CLK_ENABLE()               __HAL_RCC_I2C1_CLK_ENABLE()
#define I2Cx_SDA_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define I2Cx_SCL_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()

#define I2Cx_FORCE_RESET()              __HAL_RCC_I2C1_FORCE_RESET()
#define I2Cx_RELEASE_RESET()            __HAL_RCC_I2C1_RELEASE_RESET()
*/

/* Definition for I2Cx Pins */
/*
#define I2Cx_SCL_PIN                    GPIO_PIN_8
#define I2Cx_SCL_GPIO_PORT              GPIOB
#define I2Cx_SDA_PIN                    GPIO_PIN_9
#define I2Cx_SDA_GPIO_PORT              GPIOB
#define I2Cx_SCL_SDA_AF                 GPIO_AF4_I2C1
*/

#define I2C_ADDRESS        0x0F


typedef enum
{
  SHIELD_NOT_DETECTED = 0,
  SHIELD_DETECTED
}ShieldStatus;

/* I2C handler declaration */
I2C_HandleTypeDef I2cHandle;
I2C_InitTypeDef  I2C_InitStructure;



#define TFTSHIELD_I2C_ADDR (0x2E << 1)

#define TFTSHIELD_RESET_PIN (1ul << 3)


#define TFTSHIELD_BUTTON_UP_PIN 6
#define TFTSHIELD_BUTTON_UP (1UL << TFTSHIELD_BUTTON_UP_PIN)

#define TFTSHIELD_BUTTON_DOWN_PIN 9
#define TFTSHIELD_BUTTON_DOWN (1UL << TFTSHIELD_BUTTON_DOWN_PIN)

#define TFTSHIELD_BUTTON_LEFT_PIN 8
#define TFTSHIELD_BUTTON_LEFT (1UL << TFTSHIELD_BUTTON_LEFT_PIN)

#define TFTSHIELD_BUTTON_RIGHT_PIN 5
#define TFTSHIELD_BUTTON_RIGHT (1UL << TFTSHIELD_BUTTON_RIGHT_PIN)

#define TFTSHIELD_BUTTON_IN_PIN 7
#define TFTSHIELD_BUTTON_IN (1UL << TFTSHIELD_BUTTON_IN_PIN)

#define TFTSHIELD_BUTTON_1_PIN 10
#define TFTSHIELD_BUTTON_1 (1UL << TFTSHIELD_BUTTON_1_PIN)

#define TFTSHIELD_BUTTON_2_PIN 11
#define TFTSHIELD_BUTTON_2 (1UL << TFTSHIELD_BUTTON_2_PIN)

#define TFTSHIELD_BUTTON_3_PIN 14
#define TFTSHIELD_BUTTON_3 (1UL << TFTSHIELD_BUTTON_3_PIN)

#define TFTSHIELD_BUTTON_ALL (TFTSHIELD_BUTTON_UP | TFTSHIELD_BUTTON_DOWN | TFTSHIELD_BUTTON_LEFT \
                            | TFTSHIELD_BUTTON_RIGHT | TFTSHIELD_BUTTON_IN | TFTSHIELD_BUTTON_1 \
                            | TFTSHIELD_BUTTON_2 | TFTSHIELD_BUTTON_3)



UART_HandleTypeDef hUART2;

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* User may add here some code to deal with this error */

  while(1)
  {
  }
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 84000000
  *            HCLK(Hz)                       = 84000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            HSI Frequency(Hz)              = 16000000
  *            PLL_M                          = 16
  *            PLL_N                          = 336
  *            PLL_P                          = 4
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale2 mode
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /* Enable HSI Oscillator and activate PLL with HSI as source */
  RCC_OscInitStruct.OscillatorType          = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState                = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue     = 6; //0x10;
  RCC_OscInitStruct.PLL.PLLState            = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource           = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM                = 16;
  RCC_OscInitStruct.PLL.PLLN                = 336;
  RCC_OscInitStruct.PLL.PLLP                = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ                = 7;
  if( HAL_RCC_OscConfig( &RCC_OscInitStruct ) != HAL_OK )
  {
    Error_Handler();
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  RCC_ClkInitStruct.ClockType               = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource            = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider           = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider          = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider          = RCC_HCLK_DIV1;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /*
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  */

}

/**
  * @brief  Serial port settings
  * @param  None
  * @retval None
  */
static void initUSART( void )
{
    GPIO_InitTypeDef GPIO_InitStruct;

    __GPIOA_CLK_ENABLE();
    __USART2_CLK_ENABLE();

    // **USART2 GPIO Configuration
    // PA2     ------> USART2_TX
    // PA3     ------> USART2_RX

    GPIO_InitStruct.Pin         = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode        = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull        = GPIO_NOPULL;
    GPIO_InitStruct.Speed       = GPIO_SPEED_LOW;
    GPIO_InitStruct.Alternate   = GPIO_AF7_USART2;
    HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );


    hUART2.Instance             = USART2;
    hUART2.Init.BaudRate        = 9600;//115200;
    hUART2.Init.WordLength      = UART_WORDLENGTH_8B;
    hUART2.Init.StopBits        = UART_STOPBITS_1;
    hUART2.Init.Parity          = UART_PARITY_NONE;
    hUART2.Init.Mode            = UART_MODE_TX_RX;
    hUART2.Init.HwFlowCtl       = UART_HWCONTROL_NONE;
    hUART2.Init.OverSampling    = UART_OVERSAMPLING_16;
    if (HAL_UART_Init( &hUART2 ) != HAL_OK)
    {
        Error_Handler();
    }

} // END void initUSART( void )


#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @brief  Initialize I2C
  * @param  None
  * @retval None
  */
static void i2c_init(void)
{

    GPIO_InitTypeDef GPIO_InitStruct;

    __GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin         = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode        = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull        = GPIO_PULLUP;
    GPIO_InitStruct.Speed       = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate   = GPIO_AF4_I2C1;
    HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );

    // Peripheral clock enable
    __HAL_RCC_I2C1_CLK_ENABLE();

    I2cHandle.Instance             = I2C1;
    I2cHandle.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    I2cHandle.Init.ClockSpeed      = 100*1000;
    I2cHandle.Init.OwnAddress1     = I2C_ADDRESS;
    I2cHandle.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    I2cHandle.Init.OwnAddress2     = 0xFF;
    I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    if(HAL_I2C_Init(&I2cHandle) != HAL_OK) {
          // Initialization Error
          Error_Handler();
    }

} // END static void i2c_init(void)


/**
  * @brief  Check the availability of adafruit 1.8" TFT shield on top of STM32NUCLEO
  *         board. This is done by reading the state of IO PF.03 pin (mapped to
  *         JoyStick available on adafruit 1.8" TFT shield). If the state of PF.03
  *         is high then the adafruit 1.8" TFT shield is available.
  * @param  None
  * @retval SHIELD_DETECTED: 1.8" TFT shield is available
  *         SHIELD_NOT_DETECTED: 1.8" TFT shield is not available
  */
static ShieldStatus TFT_ShieldDetect(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct;

  /* Enable GPIO clock */
  NUCLEO_ADCx_GPIO_CLK_ENABLE();

  GPIO_InitStruct.Pin = NUCLEO_ADCx_GPIO_PIN ;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(NUCLEO_ADCx_GPIO_PORT , &GPIO_InitStruct);

  if(HAL_GPIO_ReadPin(NUCLEO_ADCx_GPIO_PORT, NUCLEO_ADCx_GPIO_PIN) != 0)
  {
    return SHIELD_DETECTED;
  }
  else
  {
    return SHIELD_NOT_DETECTED;
  }
}


/**
  * @brief  Check the availability of adafruit 1.8" TFT shield V2 on top of STM32NUCLEO
  *         board. This is done by writing I2C bus. If it fails the shield is not present
  * @param  None
  * @retval SHIELD_DETECTED: 1.8" TFT shield V2 is available
  *         SHIELD_NOT_DETECTED: 1.8" TFT shield V2 is not available
  */
static ShieldStatus TFT_V2_ShieldDetect(void)
{

    uint8_t i2c_d [3];
    uint8_t status = 0;
    uint32_t timeout = 200;


    i2c_d[0] = 0x00;
    i2c_d[1] = 0x7F;
    i2c_d[2] = 0xFF;

    do {
        status = HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 3, 1000);
    } while ( (status != HAL_OK) && (timeout--) );


    if ( status || !timeout ) {
        return SHIELD_NOT_DETECTED;
    }

    return SHIELD_DETECTED;

}


/**
  * @brief  Returns the Joystick key pressed of adafruit 1.8" TFT shield V2.
  * @retval JOYState_TypeDef: Code of the Joystick key pressed.
  */
JOYState_TypeDef TFT_V2_Shield_JOY_GetState(void)
{
    JOYState_TypeDef state;
    uint8_t i2c_d[4];
    uint32_t btn_state;

    i2c_d[0] = 0x01;
    i2c_d[1] = 0x04;

    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 2, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }
    while(HAL_I2C_Master_Receive(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 4, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }

    btn_state = ~( (uint32_t)i2c_d[0] << 24 | (uint32_t)i2c_d[1] << 16 | (uint32_t)i2c_d[2] << 8 | (uint32_t)i2c_d[3] );

    if( btn_state & TFTSHIELD_BUTTON_DOWN ) {
        state = JOY_DOWN;
    } else if ( btn_state & TFTSHIELD_BUTTON_RIGHT ) {
        state = JOY_RIGHT;
    } else if ( btn_state & TFTSHIELD_BUTTON_IN ) {
        state = JOY_SEL;
    } else if ( btn_state & TFTSHIELD_BUTTON_UP ) {
        state = JOY_UP;
    } else if ( btn_state & TFTSHIELD_BUTTON_LEFT ) {
        state = JOY_LEFT;
    } else {
        state = JOY_NONE;
    }

    return state;
}




/**
  * @brief  Initialize SeeSaw chip on adafruit 1.8" TFT shield V2
  * @param  None
  * @retval None
  */
static void TFT_V2_ShiedInit(void)
{
    uint8_t i2c_d [6];

    // reset all seesaw regs to defaults
    i2c_d[0] = 0x00;
    i2c_d[1] = 0x7F;
    i2c_d[2] = 0xFF;
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 3, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }
    HAL_Delay(500);

    // read ID
#if 0
    i2c_d[0] = 0x00;
    i2c_d[1] = 0x01;
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 2, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }
    while(HAL_I2C_Master_Receive(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 1, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }
#endif


    // enable backlight
    i2c_d[0] = 0x08;
    i2c_d[1] = 0x01;
    i2c_d[2] = 0x00;
    i2c_d[3] = 0xFF;
    i2c_d[4] = 0xFF;
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 5, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }


    // set reset pin as output
    i2c_d[0] = 0x01;
    i2c_d[1] = 0x02;
    i2c_d[2] = (uint8_t)(TFTSHIELD_RESET_PIN >> 24);
    i2c_d[3] = (uint8_t)(TFTSHIELD_RESET_PIN >> 16);
    i2c_d[4] = (uint8_t)(TFTSHIELD_RESET_PIN >> 8);
    i2c_d[5] = (uint8_t)(TFTSHIELD_RESET_PIN);
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 6, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }


    // LCD reset
    i2c_d[0] = 0x01;
    i2c_d[1] = 0x05;
    i2c_d[2] = (uint8_t)(TFTSHIELD_RESET_PIN >> 24);
    i2c_d[3] = (uint8_t)(TFTSHIELD_RESET_PIN >> 16);
    i2c_d[4] = (uint8_t)(TFTSHIELD_RESET_PIN >> 8);
    i2c_d[5] = (uint8_t)TFTSHIELD_RESET_PIN;
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 6, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }

    HAL_Delay(20);

    // set seesaw pins as inputs - buttons and JOY
    i2c_d[0] = 0x01;
    i2c_d[1] = 0x03;
    i2c_d[2] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 24);
    i2c_d[3] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 16);
    i2c_d[4] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 8);
    i2c_d[5] = (uint8_t)(TFTSHIELD_BUTTON_ALL);
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 6, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }

    // set pin val to high, buttons are active in low level
    i2c_d[0] = 0x01;
    i2c_d[1] = 0x05;
    i2c_d[2] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 24);
    i2c_d[3] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 16);
    i2c_d[4] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 8);
    i2c_d[5] = (uint8_t)(TFTSHIELD_BUTTON_ALL);
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 6, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }

    // enable internal PULLUPs
    i2c_d[0] = 0x01;
    i2c_d[1] = 0x0B;
    i2c_d[2] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 24);
    i2c_d[3] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 16);
    i2c_d[4] = (uint8_t)(TFTSHIELD_BUTTON_ALL >> 8);
    i2c_d[5] = (uint8_t)(TFTSHIELD_BUTTON_ALL);
    while(HAL_I2C_Master_Transmit(&I2cHandle, (uint16_t)TFTSHIELD_I2C_ADDR, i2c_d, 6, 1000) != HAL_OK) {
        if (HAL_I2C_GetError(&I2cHandle) != HAL_I2C_ERROR_AF) {
            Error_Handler();
        }
    }

}



#define TFTSHIELD_VERSION 2
#define cRECV_MAX 255

void commandMessage(char * command, char * message)
{
	char msg[cRECV_MAX];
	sprintf(msg, "%s:%s\r\n", command, message);
    HAL_UART_Transmit( &hUART2, (uint8_t*)msg, strlen(msg), 0xFFFF);
}

void commandHandler(char * command)
{
	int color[9] = {LCD_COLOR_BLACK, LCD_COLOR_GREY, LCD_COLOR_BLUE,
					LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_CYAN,
					LCD_COLOR_MAGENTA, LCD_COLOR_YELLOW, LCD_COLOR_WHITE};

	//commandMessage("CMD", command);

    // identification using *IDN?
    if(!strcmp(command, "*IDN?"))
    {
    	//char msg[cRECV_MAX] = "NUCLEO F446RE: author\r\n";
        //HAL_UART_Transmit( &hUART2, (uint8_t*)msg, strlen(msg), 0xFFFF);
    	commandMessage("NUCLEO F4xxRE", "author");
        return;
    }
    //button BUTTON?
    else if(!strcmp(command, "BUTTON?"))
    {

    	uint32_t button = BSP_PB_GetState( BUTTON_USER );

    	char msg[cRECV_MAX];
    	sprintf(msg, "BUTTON %d\r\n", (int)button);
        HAL_UART_Transmit( &hUART2, (uint8_t*)msg, strlen(msg), 0xFFFF);
    	return;
    }
    // LED
    else if(strstr(command, "LED") == command)
    {
    	if(strcmp(&command[3], " ON") == 0)
    	{
    		BSP_LED_On( LED2 );
    	}
    	else if(strcmp(&command[3], " OFF") == 0)
    	{
    		BSP_LED_Off( LED2 );
    	}
    	else{
    		commandMessage("ERROR", command);
    	}
    	return;
    }
    //draw block
    else if(strstr(command, "DRAW") == command)
    {
    	int params[4] = {0,0,0,0};
    	char strBuf[cRECV_MAX];

    	if(sscanf(command, "DRAW:SETTEXTCOLOR %d", &params[0]) == 1)
    	{
    		BSP_LCD_SetTextColor(color[params[0]]);
    	}
    	else if(sscanf(command, "DRAW:CLEAR %d", &params[0]) == 1)
    	{
        	BSP_LCD_Clear(color[params[0]]);
        	return;
    	}
    	else if(sscanf(command, "DRAW:PIXEL %d,%d,%d", &params[0], &params[1], &params[2]) == 3)
    	{
        	BSP_LCD_DrawPixel(params[0], params[1],color[params[2]]);
    	}
    	else if(sscanf(command, "DRAW:LINE %d,%d,%d,%d", &params[0], &params[1], &params[2], &params[3]) == 4)
    	{
        	BSP_LCD_DrawLine(params[0], params[1], params[2], params[3]);
    	}
    	else if(sscanf(command, "DRAW:CIRCLE %d,%d,%d", &params[0], &params[1], &params[2]) == 3)
    	{
        	BSP_LCD_DrawCircle(params[0], params[1], params[2]);
    	}
    	else if(sscanf(command, "DRAW:SETFONT %d", &params[0]) == 1)
    	{
        	sFONT * font[] = {&Font8, &Font12, &Font16, &Font20, &Font24};
        	BSP_LCD_SetFont(font[params[0]/4-2]);
    	}
    	else if(sscanf(command, "DRAW:TEXT %d,%d,%[^\n]s", &params[0], &params[1], strBuf) == 3) // strBuf includes last int
    	{
    		//commandMessage("DEBUG_TEXT", strBuf);
        	char * strText = strBuf;
        	if(strstr(strBuf+1, ",") == NULL){
        		commandMessage("ERROR", command);
        		return;
        	}
        	while(strstr(strText+1, ",") != NULL){ // while a comma can still be found
        		strText = strstr(strText+1, ","); // find the next one - text can contain commas
        	}
        	//location of strBuf is now at the last comma
        	strText[0] = 0; // null terminate for strText
        	if(sscanf(strText, ",%d", &params[2]) == 0){ // extract last int from input
        		commandMessage("ERROR", command);
        		return;
        	}

        	Line_ModeTypdef mode[3] = {CENTER_MODE, RIGHT_MODE, LEFT_MODE};
        	BSP_LCD_DisplayStringAt(params[0], params[1], (uint8_t*)strBuf, mode[params[2]]);
    	}

    	else
    	{
    		commandMessage("ERROR", command);
    	}
    	return;
    }
    else
    {
		commandMessage("UNKNOWN", command);
    }
} // end commandHandler


/**
  * @brief  main function
  * @param  None
  * @retval None
  */
void main(void)
{
    // timer settings
    uint32_t uiTicks, uiSec;
    // display tmp variables
    uint32_t uiDispCentX, uiDispCentY;
    // joystick operation
    JOYState_TypeDef oJoyState;

    // serial port i/o and status

    uint8_t chArr[cRECV_MAX];
    chArr[0] = 0;
    int iChArr = 0;
	#define iDEF_LINE 1
    int iDefLine = iDEF_LINE;

    HAL_StatusTypeDef oRecvStatus;


    // data reception
    uint32_t uiSerRecv;

    //
    // initialization of variables
    uiSec = 0;
    uiSerRecv = 0;

    //
    // System init
    HAL_Init();
    // Configure the System clock to 84 MHz
    SystemClock_Config();
    // serial port
    initUSART();

	#if (TFTSHIELD_VERSION == 2)
	  /* I2C init, 100kHz, 7b addressing */
	  i2c_init();

	  LCD_IO_Init();
	#endif

	#ifndef test

	#if (TFTSHIELD_VERSION == 1)
	  if(TFT_ShieldDetect() == SHIELD_DETECTED) {

		(void)BSP_JOY_Init();

	#elif (TFTSHIELD_VERSION == 2)
	  if (TFT_V2_ShieldDetect() == SHIELD_DETECTED) {
		TFT_V2_ShiedInit();
	#endif
		/* light the LED od TFTSHIELD presence */
		//LED1_On();

		/* Initialize the LCD */
	//  BSP_LCD_Init();

	  }
	#endif

	//  TFT_V2_ShiedInit();
	//  BSP_LCD_Init();

	// Nucleo User LED init
	//BSP_LED_Init( LED2 );
	// and usage

	//  BSP_LED_Off( LED2 );
	//  HAL_Delay( 1000 );
	//  BSP_LED_On( LED2 );
	//  HAL_Delay( 1000 );
	//  BSP_LED_Toggle( LED2 );

		// Nucleo user button init
	//  BSP_PB_Init( BUTTON_USER, BUTTON_MODE_GPIO );

		// Adafruit joystick init
	//  (void)BSP_JOY_Init();

    //
    // Adafruit LCD init
    BSP_LCD_Init();
    uiDispCentX = BSP_LCD_GetXSize()/2;
    uiDispCentY = BSP_LCD_GetYSize()/2;

    //
    // some drawings
    BSP_LCD_Clear( LCD_COLOR_WHITE );
    BSP_LCD_SetFont( &Font8 );
    BSP_LCD_SetTextColor( LCD_COLOR_BLACK );
    //BSP_LCD_SetFont( &Font8 );
    //BSP_LCD_SetTextColor( LCD_COLOR_BLACK );
    //BSP_LCD_DisplayStringAtLine( 1, (uint8_t *)" Welcome to Nucleo! " );

    while(1) // main loop
    {

        // Receive serial data
    	// The requirements are not exact, so I decided to do such an implementation:
    	// Text is displayed until end of buffer reached
    	// If end of buffer reached, text is displayed on new line and buffer is reset
    	// If ENTER key is pressed or out of space, display is cleared
        oRecvStatus = HAL_UART_Receive( &hUART2, &chArr[iChArr], 1, 100 );
        if( oRecvStatus == HAL_OK )
        {
            if ( iChArr >= cRECV_MAX - 1 ) // would be out of bounds otherwise
            { // just resets; no smart handling implemented yet
            	chArr[0] = 0;
            	iChArr = 0;
            }

        	uiSerRecv = (uint32_t)chArr[iChArr]; // store copy of char for display evaluation
            chArr[++iChArr] = 0; // null-terminate

    	    //commandMessage("BUFFER", chArr); //TODO debug thing
            if(iChArr > 2) // ignore too short commands
            {
            	if((chArr[iChArr-2] == '\r') &&
            	    chArr[iChArr-1] == '\n')
            	{
            	    chArr[iChArr-2] = 0;
            		commandHandler(chArr);
            		iChArr = 0;
            	}

            }

        } // END if( oRecvStatus == HAL_OK )

        // Note: leaving compatibility for both displays, my version is 2.

        // Joystick reading example
		#if (TFTSHIELD_VERSION == 1)
        	oJoyState = BSP_JOY_GetState();
		#elif (TFTSHIELD_VERSION == 2)
        	oJoyState = TFT_V2_Shield_JOY_GetState();
		#endif

        switch( oJoyState )
        {
        default:
        case JOY_NONE:
            break;
        case JOY_SEL:
            {
                //BSP_LCD_Clear( LCD_COLOR_WHITE );
                //BSP_LED_Toggle( LED2 );
                //char *msg = "EVENT:JOY_SEL\r\n";
                //HAL_UART_Transmit( &hUART2, (uint8_t*)msg, strlen(msg), 0xFFFF);
            	commandMessage("EVENT", "JOY_SEL");

            }
            break;
        case JOY_DOWN:
            {
            	//BSP_LCD_SetTextColor( LCD_COLOR_BLUE );
                //BSP_LCD_DrawLine( uiDispCentX, uiDispCentY, uiDispCentX, uiDispCentY + 40 );
                //BSP_LCD_DrawCircle(BSP_LCD_GetXSize() - 15, BSP_LCD_GetYSize() - 15, 10);
            	commandMessage("EVENT", "JOY_DOWN");
            }
            break;
        case JOY_LEFT:
            {
            	commandMessage("EVENT", "JOY_LEFT");
            }
            break;
        case JOY_RIGHT:
            {
            	commandMessage("EVENT", "JOY_RIGHT");
            }
            break;
        case JOY_UP:
        	{
            	commandMessage("EVENT", "JOY_UP");
        	}
            break;
        } // end switch( oJoyState )


        // prints current input from serial
        // debug function
        if( uiSerRecv != 0 ) // if any input given
        {
            //BSP_LCD_SetFont( &Font20 );
            //BSP_LCD_SetTextColor( LCD_COLOR_BLACK );
            BSP_LCD_DisplayStringAtLine( iDefLine, chArr);
        }
        //
        // Button reading - the button is hidden under the display
        // uiButtState = BSP_PB_GetState( BUTTON_USER );

        /*
        // Timming - second counter
        if( (HAL_GetTick() - uiTicks) > 1000)
        {
            uint8_t chArr[40];
            uiTicks = HAL_GetTick();
            uiSec++;

            BSP_LCD_SetFont( &Font20 );
            BSP_LCD_SetTextColor( LCD_COLOR_RED );
            sprintf( (char *)chArr, (char *)"  %d   ", (int)uiSec );
            BSP_LCD_DisplayStringAtLine( 14, chArr);
        } // END if( (HAL_GetTick() - uiTicks) > 1000)
        */

        /*
        // string manipulation example
        if( uiSerRecv == 0x30 )
        {
            int iResult;
            char chArrTmpData[] = "DRAW:CIRCLE 64,80,30";

            // !! has to be here
            uiSerRecv = 0;

            iResult = strstr(chArrTmpData, "DRAW");
            if( iResult )
            {
                int iXs, iYs, iRad;
                int iResult;

                iResult = sscanf( chArrTmpData, "DRAW:CIRCLE %d,%d,%d", &iXs, &iYs, &iRad );
                if( iResult > 2 )
                {
                    BSP_LCD_DrawCircle( iXs, iYs, iRad );
                }
            }
        } // END string manipulation example
        */

    } // END while(1)

} // END main
