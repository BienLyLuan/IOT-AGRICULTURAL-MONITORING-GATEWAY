/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stdlib.h"
#include "stdbool.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Struct data LoRa
typedef struct {
		uint8_t header;
    uint8_t nodeID;
		uint8_t len;	
    int16_t temperature;
    int16_t humidity;
		uint8_t Vbat;
		uint16_t Sensor2[7];
		uint8_t status;
		uint16_t crc;	
} __attribute__((packed)) MyData;
// Khoi tao Packet Tx LoRa
MyData TxPacket;
// Enum LoRa
typedef enum {
    LORA_OK,
    LORA_ERR_BUSY,    	
    LORA_ERR_UART,    
} LoRa_Status_t;
// Enum Modbus RTU
typedef enum {
	MB_OK,
	MB_TIMEOUT,
	MB_ERR_ID,
	MB_ERR_FUNC,
	MB_ERR_LEN,
	MB_ERR_CRC,
	MB_ERR_BYTECOUNT,
} MB_Status_t; 
MB_Status_t Status_SHT31;
MB_Status_t Status_NPK;
// enum state sensor
typedef enum {
	MB_STATE_IDLE,
	MB_STATE_SEND_SENSOR,
	MB_STATE_WAIT,
	MB_STATE_PROCESS,
} MB_state_t;
MB_state_t state1 = MB_STATE_IDLE;
MB_state_t state2 = MB_STATE_IDLE;
// enum state Lora
typedef enum {
    LORA_STATE_IDLE,
    LORA_STATE_SET_MODE,
    LORA_STATE_WAIT_SETMODE,
    LORA_STATE_SENDING,
		LORA_STATE_WAIT_TX,
} LoRa_State_t;
LoRa_State_t lora_state = LORA_STATE_IDLE;
/* USER CODE END PTD */
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// define LoRa
// define config lora
#define LORA_FREQ_433MHZ 				0x17      // Kenh tan so 433Mhz, kenh thu 23 
#define LORA_MODE_FIXED 				0xC4			
#define LORA_GATEWAY_ADDR 			0x0000		// Addr dung de cau hinh cho node o che do Fixed
#define MY_LORA_ADDR 						0x0001		// ---
#define MY_LORA_ID        			0x01			// ID cho Frame data 
// define mode lora 
#define LORA_MODE_NORMAL 				0
#define LORA_MODE_WAKEUP  			1
#define LORA_MODE_SLEEP 				2         // Power saving 
#define LORA_MODE_CONFIG 				3					// Che do de cau hinh hoac cung le che do sleep
// define Modbus
#define READ_HOLDING_REGISTER 	0x03
#define READ_INPUT_REGISTER   	0x04
// define sensor NPK
#define NPK_ID 	 								0x01
#define NPK_START_ADDR 					0x0000
#define NPK_QUANTIY_REG_READ 		0x0007
//define sensor SHT31
#define SHT31_ID 								0x02
#define SHT31_START_ADDR 				0x0000
#define SHT31_QUANTITY_REG_READ 0x0002
// define tick Schedule 
#define READ_SHT31_INTERVAL 		60000 	//1M
#define READ_NPK_INTERVAL 			60000 	//1M
#define SEND_LORA_INTERVAL  		60000 	//1M
#define READ_BATTERY_INTERVAL 	300000 	//5M
#define CHECK_ERR_INTERVAL    	2000  	//2s
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
// Modbus RTU
uint8_t TxBufferModbus[8];
uint8_t RxBufferModbus[128];
volatile uint16_t rx_len = 0;
volatile bool flag = false;
volatile bool mutex = true;
// Buffer Data Sensor 
uint16_t RxSHT31[2];
uint16_t RxNPK[7];
// Battery 
float battery_voltage = 0.0f;
uint16_t battery_mv = 0;
// count check 
uint32_t cnt1 = 0;
uint32_t cnt2 = 0;
uint8_t sht31_err = 0;
uint8_t NPK_err = 0;
// Tick Schedule 
uint32_t tick_SHT31 = 0;
uint32_t tick_NPK = 0;
uint32_t tick_LoRa =0;
uint32_t tick_Battery = 0;
uint32_t tick_CheckErr = 0;
// Timeout Schedule 
uint32_t TimeoutSHT31 = 0;
uint32_t TimeoutNPK = 0;
uint32_t TimeoutLoRa = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//************************************************************************************************************************
//****************************************************SHARED**************************************************************
//************************************************************************************************************************
// printf
//int fputc(int ch, FILE *f)
//{
//    HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 100);
//    return ch;
//}
// crc
uint16_t Modbus_CRC16(uint8_t *buf, int len) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
// timer
bool Is_Timer_Expired(uint32_t *lastTick, uint32_t interval) {
    uint32_t now = HAL_GetTick();
    if (now - *lastTick >= interval) {
        *lastTick += interval;
        return true;
    }
    return false;
}
// check Error
void CheckErr() {
		TxPacket.status = 0;
		if (sht31_err >3) {
			TxPacket.status |= 1<<0;
		}
		else {
			TxPacket.status &= ~(1<<0);
		}
		if (NPK_err >3) {
			TxPacket.status |=(1<<1);
		}
		else {
			TxPacket.status &=~(1<<1);
		}
}
//*********************************************************************************************************************
//**************************************************BUZZER***************************************************************
//*********************************************************************************************************************
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        static uint8_t buzzer_state = 0;
        if (TxPacket.status != 0) {
            buzzer_state = !buzzer_state; 
        }
        else {
            buzzer_state = 0; 
        }
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, buzzer_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
//*********************************************************************************************************************
//**************************************************LORA***************************************************************
//*********************************************************************************************************************
// Ham set mode LoRa
LoRa_Status_t Lora_SetMode(uint8_t mode) {
    uint32_t timeout = HAL_GetTick();
    while(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - timeout > 1000) return LORA_ERR_BUSY; 
    }
    switch (mode) {
        case 0: // NORMAL: M0=0, M1=0
            HAL_GPIO_WritePin(LORA_M0_GPIO_Port,LORA_M0_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_RESET);
            break;
        case 1: // WAKEUP: M0=1, M1=0
            HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_RESET);
            break;
        case 2: // SLEEP: M0=0, M1=1
            HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_SET);
            break;
        case 3: // CONFIG: M0=1, M1=1
            HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_SET);
            break;
    }
    HAL_Delay(40);
    timeout = HAL_GetTick();
    while(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - timeout > 1000) return LORA_ERR_BUSY; 
    }
		return LORA_OK;
}
// Ham config LoRa
LoRa_Status_t Lora_Config(UART_HandleTypeDef *huart, uint8_t addrH, uint8_t addrL, uint8_t chan) {
    Lora_SetMode(LORA_MODE_CONFIG); // config 
		HAL_Delay(100);
    uint8_t config_cmd[6];
    config_cmd[0] = 0xC0;    // Ghi va luu vinh vien (Save to Flash)
    config_cmd[1] = addrH;   // Dia chi cao cua Node STM32
    config_cmd[2] = addrL;   // Dia chi thap cua Node STM32
    config_cmd[3] = 0x1A;    // 9600bps, 8N1, Air 2.4k voi 0x1A, neu air la 0.3 thi la 0x18
    config_cmd[4] = chan;    // Kenh tan so
		config_cmd[5] = LORA_MODE_FIXED;
    HAL_UART_Transmit(huart, config_cmd, 6, 100);
    HAL_Delay(100);
    uint32_t timeout = HAL_GetTick();
    while(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - timeout > 1000) return LORA_ERR_BUSY; 
    }
    Lora_SetMode(LORA_MODE_NORMAL);
		return LORA_OK;
}
// Ham send data LoRa qua UART1, goi trong ham SendDataViaLoRa
LoRa_Status_t Lora_SendData(UART_HandleTypeDef *huart, uint8_t targetH, uint8_t targetL, uint8_t targetChan, MyData *data) {
    // Chuan bi data
    data->len = sizeof(MyData) - 5;   //  Tru di 5 byte cau hinh 
    data->header = 0xAA;						 	//  Gan 1 so bat ky lam header, o day chon 0xAA
    data->nodeID = MY_LORA_ID; 						 	//  Gan 1 ID bat ky, o day chon 0x01 cho node Input
    data->crc = Modbus_CRC16((uint8_t*)data, sizeof(MyData) - 2);
//    // Wait AUX 
//    uint32_t timeout = HAL_GetTick();
//    while(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET) {
//        if (HAL_GetTick() - timeout > 1000) return LORA_ERR_BUSY; 
//    }
    // Gui dia chi Gateway va Channel
    uint8_t addr_header[3] = {targetH, targetL, targetChan};
    if (HAL_UART_Transmit(huart, addr_header, 3, 100) != HAL_OK) return LORA_ERR_UART;
    // Gui data
    if (HAL_UART_Transmit(huart, (uint8_t*)data, sizeof(MyData), 500) != HAL_OK) return LORA_ERR_UART;
//    // Wait AUX
//    timeout = HAL_GetTick();
//    while(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET) {
//        if (HAL_GetTick() - timeout > 1000) return LORA_ERR_BUSY;
//    }
    return LORA_OK;
}
// State Machine LoRa
void StateMachineLoRa() {
	switch (lora_state) {
		case LORA_STATE_IDLE:
			if (Is_Timer_Expired(&tick_LoRa, SEND_LORA_INTERVAL)) {
				Lora_SetMode(LORA_MODE_NORMAL);
				lora_state = LORA_STATE_SENDING;
			}
			break;
		 case LORA_STATE_SENDING:
        Lora_SendData(&huart1, (LORA_GATEWAY_ADDR>>8)&0xFF,(LORA_GATEWAY_ADDR&0xFF), LORA_FREQ_433MHZ, &TxPacket);
        TimeoutLoRa = HAL_GetTick();
        lora_state = LORA_STATE_WAIT_TX; 
      break;
		 case LORA_STATE_WAIT_TX:
        if (HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_SET) {
            Lora_SetMode(LORA_MODE_CONFIG);  // dua ve ngu sau 
            lora_state = LORA_STATE_IDLE;
         }
        else if (HAL_GetTick() - TimeoutLoRa > 300) {
					//timeout
					lora_state = LORA_STATE_IDLE;
        }
       break;
	}
}
//****************************************************************************************************************************
//*************************************************MODBUS*********************************************************************
//****************************************************************************************************************************
// send request
void ModbusSendRequest(uint8_t id, uint8_t function, uint16_t start_addr, uint16_t quantity) {
	TxBufferModbus[0] = id;
	TxBufferModbus[1] = function;
	TxBufferModbus[2] = (start_addr >>8) & 0xFF;
	TxBufferModbus[3] = start_addr & 0xFF;
	TxBufferModbus[4] = (quantity >>8) & 0xFF;
	TxBufferModbus[5] = quantity & 0xFF;
	uint16_t crc = Modbus_CRC16(TxBufferModbus,6);
	TxBufferModbus[6] = crc & 0xFF;
	TxBufferModbus[7] = (crc>>8) & 0xFF;
	HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin,GPIO_PIN_SET);
	HAL_UART_Transmit(&huart2,TxBufferModbus,8,900);
	while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET);
  HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET); 
}
// callback Rx IDLE
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
      rx_len = Size;
			flag = true;
			HAL_GPIO_TogglePin(LED_USER_GPIO_Port,LED_USER_Pin);
      HAL_UARTEx_ReceiveToIdle_IT(&huart2, RxBufferModbus, sizeof(RxBufferModbus));
    }
}
// Parse Response
MB_Status_t ModbusParseResponse(uint8_t *rxbuf, uint16_t rxlen, uint8_t id, uint8_t function, uint16_t *rxdatadest) {
	if ( rxlen < 5) return MB_ERR_LEN;
	uint8_t bytecount = rxbuf[2];
	uint16_t framelen = bytecount + 5;
	if ( rxlen != framelen) return MB_ERR_LEN;
	if ( rxbuf[0] != id || rxbuf[1] != function) return MB_ERR_ID;
	if (bytecount % 2 != 0) return MB_ERR_BYTECOUNT;
	// check CRC
	uint16_t CalCRC = Modbus_CRC16(rxbuf,framelen - 2);
	uint16_t RecCRC = rxbuf[framelen - 2] | (rxbuf[framelen - 1]<<8);
	if (CalCRC != RecCRC) return MB_ERR_CRC;
	// copy
	for (int i =0; i < bytecount /2; i++) {
			rxdatadest[i] = (rxbuf[3 + i * 2] << 8) | rxbuf[4 + i * 2];
	}
	return MB_OK;
}
// state machine sht31
void StateMachineSHT31() {
	switch (state1) {
		case MB_STATE_IDLE:
			if (Is_Timer_Expired(&tick_SHT31,READ_SHT31_INTERVAL) && mutex == true) {
				mutex = false;
				state1 = MB_STATE_SEND_SENSOR;
			}
			break;
		case MB_STATE_SEND_SENSOR:
			ModbusSendRequest(SHT31_ID,READ_HOLDING_REGISTER,SHT31_START_ADDR,SHT31_QUANTITY_REG_READ);
			TimeoutSHT31 = HAL_GetTick();
			state1 = MB_STATE_WAIT;
			break;
		case MB_STATE_WAIT:
			if (flag) {
				flag = false;
				state1 = MB_STATE_PROCESS;
			}
			else if ( HAL_GetTick() - TimeoutSHT31 > 250 ) {
				//timeout
				Status_SHT31 = MB_TIMEOUT;
				sht31_err++;
				mutex = true;
				state1 = MB_STATE_IDLE;
			}
			break;
		case MB_STATE_PROCESS:
				Status_SHT31 = ModbusParseResponse(RxBufferModbus,rx_len,SHT31_ID,READ_HOLDING_REGISTER,RxSHT31);
				if (Status_SHT31 == MB_OK) {
					cnt1++;
					sht31_err = 0;
					TxPacket.temperature = (int16_t)RxSHT31[0];
					TxPacket.humidity = (int16_t)RxSHT31[1];
				} else {
					sht31_err++;
				}
				mutex = true;
				rx_len = 0;
				state1 = MB_STATE_IDLE;
				break;
	} 
}
// state machine NPK
void StateMachineNPK() {
	switch (state2) {
		case MB_STATE_IDLE:
			if (Is_Timer_Expired(&tick_NPK,READ_NPK_INTERVAL) && mutex == true) {
				mutex = false;
				state2 = MB_STATE_SEND_SENSOR;
			}
			break;
		case MB_STATE_SEND_SENSOR:
			ModbusSendRequest(NPK_ID,READ_HOLDING_REGISTER,NPK_START_ADDR,NPK_QUANTIY_REG_READ);
			TimeoutNPK = HAL_GetTick();
			state2 = MB_STATE_WAIT;
			break;
		case MB_STATE_WAIT:
			if (flag) {
				flag = false;
				state2 = MB_STATE_PROCESS;
			}
			else if ( HAL_GetTick() - TimeoutNPK > 300 ) {
				//timeout
				Status_NPK = MB_TIMEOUT;
				NPK_err++;
				mutex = true;
				state2 = MB_STATE_IDLE;
			}
			break;
		case MB_STATE_PROCESS:
					Status_NPK = ModbusParseResponse(RxBufferModbus,rx_len,NPK_ID,READ_HOLDING_REGISTER,RxNPK);
					if (Status_NPK == MB_OK) {
						cnt2++;
						NPK_err =0;
						for (int i = 0; i < 7; i++) {
							TxPacket.Sensor2[i] = RxNPK[i];
						}
					} else {
						NPK_err++;
					}
				mutex = true;
				rx_len = 0;
				state2 = MB_STATE_IDLE;
				break;
	} 
}
//***********************************************************************************************************************
//******************************************************READ BATTERY*****************************************************
//***********************************************************************************************************************
// Ham doc ADC
uint32_t ADC_Read(uint32_t channel, uint8_t samples) {	
    uint32_t sum = 0;
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
    // Lay mau trung binh
    for (uint8_t i = 0; i < samples; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            sum += HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }
    return (sum / samples);
}
// Ham tinh dung luong pin 
uint8_t BatteryPercent(uint16_t voltage_mv)
{
    const uint16_t v[] = {4200,4100,4000,3900,3800,3700,3600,3500,3400,3300};
    const uint8_t  p[] = {100,90,80,70,60,50,40,30,20,0};
    int i;
    if (voltage_mv >= v[0]) return 100;
    if (voltage_mv <= v[9]) return 0;
    for (i = 0; i < 9; i++)
    {
        if (voltage_mv <= v[i] && voltage_mv > v[i+1])
        {
            return p[i+1] +
                   (voltage_mv - v[i+1]) * (p[i] - p[i+1]) /
                   (v[i] - v[i+1]);
        }
    }
    return 0;
}
// Ham doc phan tram pin
void ReadBattery(void) {
    //  �oc tong 16 lan
    uint32_t adc_raw_sum = ADC_Read(ADC_CHANNEL_6, 16);      
    uint32_t vref_raw_sum = ADC_Read(ADC_CHANNEL_VREFINT, 16);
    if (vref_raw_sum > 0) {
        // Tinh trung binh
        float adc_avg = (float)adc_raw_sum;
        float vref_avg = (float)vref_raw_sum;
        float vdd_actual = (1.2f * 4095.0f) / vref_avg;
        //Tinh dien ap Pin thuc te
        battery_voltage = (adc_avg * vdd_actual / 4095.0f) * 2.0f;
        battery_mv = (uint16_t)(battery_voltage * 1000.0f);
        TxPacket.Vbat = BatteryPercent(battery_mv);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

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
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
	// Enable IC SP485
	HAL_GPIO_WritePin(EN_RS485_GPIO_Port,EN_RS485_Pin, GPIO_PIN_SET);
	// Enable DIR ban dau
	HAL_GPIO_WritePin(DIR_GPIO_Port,DIR_Pin,GPIO_PIN_RESET);
	// Enable LoRa, vi se su dung che do tiet kiem pin 
	HAL_GPIO_WritePin(LORA_EN_GPIO_Port,LORA_EN_Pin,GPIO_PIN_SET);
	// Config LoRa
	Lora_Config(&huart1, (MY_LORA_ADDR >> 8) & 0xFF, (MY_LORA_ADDR & 0xFF), LORA_FREQ_433MHZ);
	// Enable Rx IDLE UART2
	HAL_UARTEx_ReceiveToIdle_IT(&huart2, RxBufferModbus,sizeof(RxBufferModbus));
	// Enable TIM2, dung de lam coi bao dong 
	HAL_TIM_Base_Start_IT(&htim2);
	// Set TxPacket ve 0 ban dau 
	memset(&TxPacket, 0, sizeof(TxPacket));
	// Khoi tao tick tac vu, va cho chay lan dau 
	tick_SHT31 = HAL_GetTick() - (READ_SHT31_INTERVAL - 1000);
	tick_NPK = HAL_GetTick() - (READ_NPK_INTERVAL - 2000);
	tick_Battery = HAL_GetTick() - (READ_BATTERY_INTERVAL - 3000);
	tick_LoRa = HAL_GetTick() -(SEND_LORA_INTERVAL - 5000);
	tick_CheckErr = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		StateMachineSHT31();	
		StateMachineNPK();	
		StateMachineLoRa();
		if (Is_Timer_Expired(&tick_Battery,READ_BATTERY_INTERVAL)) {
			ReadBattery();
		}
		if (Is_Timer_Expired(&tick_CheckErr,CHECK_ERR_INTERVAL)) {
			//CheckErr();
		}
//		if ( Is_Timer_Expired(&tick_LoRa,10000)) {
//				Lora_SendData(&huart1, (LORA_GATEWAY_ADDR>>8)&0xFF,(LORA_GATEWAY_ADDR&0xFF), LORA_FREQ_433MHZ, &TxPacket);
//		}
		
//					if (Is_Timer_Expired(&tick1,3000)) {
//						// sht20
//						ModbusSendRequest(0x01,0x04,0x0001,0x0002);
//					}
//					if (Is_Timer_Expired(&tick2,3000)) {
//						// sht31
//						ModbusSendRequest(0x02,0x03,0x0000,0x0007);
//					}	
//		if (flag) {
//				if (RxBuffer[0] == 0x01) {
//					cnt1++;
//					ModbusParseResponse(RxBuffer,0x04,0x01,0x04,RxData1);
//					HAL_Delay(20);
//				}
//				
//					if (RxBuffer[0] == 0x02) {
//					cnt2++;
//					// stm32f1
//					ModbusParseResponse(RxBuffer,0x0E,0x02,0x03,RxData2);
//					HAL_Delay(20);
//				}
//				rx_len = 0;
//				flag = false;
//		}
		
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV4;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4999;
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
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, EN_RS485_Pin|DIR_Pin|LORA_M1_Pin|LORA_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED_USER_Pin|BUZZER_Pin|LORA_M0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : EN_RS485_Pin DIR_Pin LORA_M1_Pin LORA_EN_Pin */
  GPIO_InitStruct.Pin = EN_RS485_Pin|DIR_Pin|LORA_M1_Pin|LORA_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_USER_Pin BUZZER_Pin LORA_M0_Pin */
  GPIO_InitStruct.Pin = LED_USER_Pin|BUZZER_Pin|LORA_M0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LORA_AUX_Pin */
  GPIO_InitStruct.Pin = LORA_AUX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(LORA_AUX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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