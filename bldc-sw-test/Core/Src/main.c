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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include "string.h"
#include "tmc/ic/TMC9660/TMC9660.h"
#include "tmc/ic/TMC9660/TMC9660_BL_HW_Abstraction.h"
#include "tmc/ic/TMC9660/TMC9660_PARAM_HW_Abstraction.h"
#include <stdio.h>
#include <stdarg.h>
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
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint16_t tmc9660_id = 0;
int32_t chipID = 0;
uint8_t tmc_detected = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
bool tmc9660_app_GotoBootMode(uint16_t icID);
void tmc9660_UART_WriteRead(uint8_t channel, uint8_t *data, size_t writeLength, size_t readLength);
void UART_Printf(const char* fmt, ...);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Implementation of mandatory timestamp callback */
uint32_t tmc_getMicrosecondTimestamp(void)
{
    /* Return a value in microseconds.
       This example assumes a 80MHz SystemCoreClock. */
    return (uint32_t)(HAL_GetTick() * 1000);
}

/* Mandatory bus type callback */
TMC9660BusType tmc9660_getBusType(uint16_t icID)
{
    return TMC9660_BUS_UART;
}

/* Mandatory bus address callback */
TMC9660BusAddresses tmc9660_getBusAddresses(uint16_t icID)
{
    TMC9660BusAddresses addr;
    addr.device = 1;   // Default device address
    addr.host = 255;   // Default host address
    return addr;
}

/* Mandatory physical UART transport callback */
bool tmc9660_readWriteUART(uint16_t icID, uint8_t *data, size_t writeLength, size_t readLength)
{
	/*
	 * typedef enum
		{
		  HAL_OK       = 0x00,
		  HAL_ERROR    = 0x01,
		  HAL_BUSY     = 0x02,
		  HAL_TIMEOUT  = 0x03
		} HAL_StatusTypeDef;
	 */
	// 1. Transmit the command
	if (HAL_UART_Transmit(&huart1, data, (uint16_t)writeLength, 100) != HAL_OK) return false;

	// 2. Receive the reply
	if (readLength > 0)
	{
		uint8_t byteIn = 0;

		// --- THE FIX: Line-Turnaround Glitch Filter ---
		// Read bytes one by one until we get a non-zero byte.
		// This silently discards the 0x00 glitch and captures the true start byte (e.g., 255).
		do {
			if (HAL_UART_Receive(&huart1, &byteIn, 1, 50) != HAL_OK) {
				return false; // Timeout
			}
		} while (byteIn == 0x00);

		// Store the first valid byte (255) at the correct index (0)
		data[0] = byteIn;

		// Read the remaining bytes of the datagram (which will now include the CRC!)
		if (readLength > 1) {
			if (HAL_UART_Receive(&huart1, &data[1], (uint16_t)(readLength - 1), 100) != HAL_OK) {
				return false;
			}
		}
	}

	return true;
}
/* Debug Helper: Print formatted strings to USART2 (PC Terminal) */
void UART_Printf(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) CDC_Transmit_FS((uint8_t*)buf, len);
}

void Test_TMC9660_Status(int32_t status) {
    TMC9660ParamStatus test = (TMC9660ParamStatus)status;

    switch (test) {
        case TMC9660_PARAMSTATUS_CHKERROR:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_CHKERROR (Checksum error during communication)\r\n");
            break;
        case TMC9660_PARAMSTATUS_INVALID_CMD:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_INVALID_CMD (Invalid command number)\r\n");
            break;
        case TMC9660_PARAMSTATUS_WRONG_TYPE:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_WRONG_TYPE (Invalid type number)\r\n");
            break;
        case TMC9660_PARAMSTATUS_INVALID_VALUE:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_INVALID_VALUE (Invalid value)\r\n");
            break;
        case TMC9660_PARAMSTATUS_CMD_NOT_AVAILABLE:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_CMD_NOT_AVAILABLE (Command currently not available)\r\n");
            break;
        case TMC9660_PARAMSTATUS_CMD_LOAD_ERROR:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_CMD_LOAD_ERROR (Failed to load command into script memory)\r\n");
            break;
        case TMC9660_PARAMSTATUS_MAX_EXCEEDED:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_MAX_EXCEEDED (Maximum exceeded)\r\n");
            break;
        case TMC9660_PARAMSTATUS_CMD_DOWNLOAD_NOT_POSSIBLE:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_CMD_DOWNLOAD_NOT_POSSIBLE (Script memory loading not available)\r\n");
            break;
        case TMC9660_PARAMSTATUS_OK:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_OK (Success)\r\n");
            break;
        case TMC9660_PARAMSTATUS_CMD_LOADED:
            UART_Printf("STATUS: TMC9660_PARAMSTATUS_CMD_LOADED (Command successfully loaded into script memory)\r\n");
            break;
        default:
            UART_Printf("STATUS: UNKNOWN (Code: %ld)\r\n", status);
            break;
    }
}

void ERR_Handler(int line) {
	UART_Printf("\nFailure @ line: %d\n", line);
	while(1) {};
}

bool tmc9660_app_GotoBootMode(uint16_t icID) {
	// assuming tmc9660_id global set
	bool ret = false;
	uint32_t rVal = 0;
	TMC9660BlStatus blStatus;

	// 1. Try returning from Register Mode
	// TODO: improve TMC-API to return device status byte reply
    tmc9660_reg_returnToBootloader(icID);
    HAL_Delay(100); // Delay for possible Init

    // 2. Try returning from Parameter Mode
    // TODO: improve TMC-API to return device status byte reply
    tmc9660_param_returnToBootloader(icID);
    HAL_Delay(100); // Delay for possible Init

	// 3. Confirm Boot Mode
	blStatus = tmc9660_bl_sendCommand(icID, TMC9660_BLCMD_GET_INFO, 0, &rVal);
	if ( blStatus == TMC9660_BLSTATUS_OK ) {
		ret = true;
	}

	return ret;
}

/**
 * @brief Converts TMC9660 raw voltage units (100mV) to Millivolts (mV).
 * @param volt100mV Raw value from IC (e.g., 80 for 8.0V).
 * @return uint32_t Equivalent value in millivolts (e.g., 8000).
 */
static inline uint32_t tmc9660_app_volt100mVtoMV(uint32_t volt100mV) {
    return volt100mV * 100;
}

/* Helper: Step 1 - Verify Motor Supply Voltage >= 8V */
static bool verifyMotorVoltage(uint16_t icID) {
    // 1. Get raw parameter (Units: 100mV)
	// TODO: prefer to expose TMC9660ParamStatus device status reply in TMC-API
    uint32_t raw_voltage = tmc9660_param_getParameter(icID, TMC9660_PARAM_SUPPLY_VOLTAGE);

    // 2. Convert to Millivolts (mV)
    uint32_t voltage_mv = tmc9660_app_volt100mVtoMV(raw_voltage);

    // 3. Perform threshold check (8000mV)
    if (voltage_mv < 8000) {
        UART_Printf("Error: Voltage is %ld mV (threshold 8000 mV)\r\n", voltage_mv);
        return false;
    }

    return true;
}

/* Helper: Step 2 - Set Motor Type to BLDC */
static bool setupMotorType(uint16_t icID) {
    // 3 = 3-Phase BLDC/PMSM
	// TODO: prefer to expose TMC9660ParamStatus device status reply in TMC-API
    return tmc9660_param_setParameter(icID, TMC9660_PARAM_MOTOR_TYPE, 3);
}

/* Helper: Step 3 - Set Pole Pairs */
static bool setupPolePairs(uint16_t icID) {
    // QBL5704-94-04-032 has 4 poles = 2 pole pairs
	// TODO: prefer to expose TMC9660ParamStatus device status reply in TMC-API
    return tmc9660_param_setParameter(icID, TMC9660_PARAM_MOTOR_POLE_PAIRS, 2);
}

/* Helper: Step 4 - Set Commutation Mode to FOC Hall */
static bool setupCommutationMode(uint16_t icID) {
    // 6 = FOC based on Hall Sensor feedback
	// TODO: prefer to expose TMC9660ParamStatus device status reply in TMC-API
    return tmc9660_param_setParameter(icID, TMC9660_PARAM_COMMUTATION_MODE, 6);
}

/**
 * @brief Configures the TMC9660 for a Hall-based FOC test with the QBL5704 motor.
 * @param icID The identifier for the IC.
 * @return bool True if all steps passed, False otherwise.
 */
bool tmc9660_app_setupQBL5704BLDCHallFeedbackTest(uint16_t icID) {
	bool test;
    UART_Printf("\r\n--- Starting QBL5704 Setup ---\r\n");

    // Step 1: Voltage Verification
    test = verifyMotorVoltage(icID);
    if (test == false) {
        UART_Printf("Error: Supply voltage too low (< 8V). Check VS power.\r\n");
        return false;
    }
    UART_Printf("1. Voltage Check: OK\r\n");

    // Step 2: Configure Motor Type
    test = setupMotorType(icID);
    if (test == false) {
        UART_Printf("Error: Failed to set Motor Type.\r\n");
        return false;
    }
    UART_Printf("2. Motor Type: BLDC\r\n");

    // Step 3: Configure Pole Pairs
    test = setupPolePairs(icID);
    if (test == false) {
        UART_Printf("Error: Failed to set Pole Pairs.\r\n");
        return false;
    }
    UART_Printf("3. Pole Pairs: 2\r\n");

    // Step 4: Configure Commutation
    test = setupCommutationMode(icID);
    if (test == false) {
        UART_Printf("Error: Failed to set Commutation Mode.\r\n");
        return false;
    }
    UART_Printf("4. Commutation: FOC Hall\r\n");

    UART_Printf("--- QBL5704 Setup Successful ---\r\n");
    return true;
}

/**
 * @brief Performs electrical alignment using Open Loop Current Mode (4).
 * @param icID The identifier for the IC.
 * @return bool True if alignment was successful.
 */
bool tmc9660_app_alignQBL5704HallOffset(uint16_t icID) {
    uint32_t hall_angle_at_zero = 0;

    UART_Printf("\r\n--- Starting Hall Alignment (Current Mode 4) ---\r\n");

    // 1. Enter Open Loop Current Mode (4)
    tmc9660_param_setParameter(icID, TMC9660_PARAM_COMMUTATION_MODE, 4);

    // 2. Set the target electrical angle to 0
    tmc9660_param_setParameter(icID, TMC9660_PARAM_OPENLOOP_ANGLE, 0);

    // 3. Apply alignment current (e.g., 800 units for QBL5704)
    // This locks the rotor to the magnetic 0° position.
    tmc9660_param_setParameter(icID, TMC9660_PARAM_OPENLOOP_CURRENT, 800);

    // 4. Wait for the rotor to physically settle at the magnetic pole
    HAL_Delay(1200);

    // 5. Read the raw Hall electrical angle
    hall_angle_at_zero = tmc9660_param_getParameter(icID, TMC9660_PARAM_HALL_PHI_E);

    UART_Printf("Rotor Locked at 0 deg. Hall Angle Reported: %ld\r\n", hall_angle_at_zero);

    // 6. Set the Offset to bridge the gap between Sensor 0 and Magnetic 0
    tmc9660_param_setParameter(icID, TMC9660_PARAM_HALL_PHI_E_OFFSET, hall_angle_at_zero);

    // 7. Cleanup: Disable current and return to a safe state
    tmc9660_param_setParameter(icID, TMC9660_PARAM_OPENLOOP_CURRENT, 0);
    tmc9660_param_setParameter(icID, TMC9660_PARAM_COMMUTATION_MODE, 0);

    UART_Printf("Offset Programmed. IC now calibrated for QBL5704.\r\n");
    return true;
}


/**
 * @brief Simple test for the QBL5704 motor.
 * Sets safety limits and applies a 200mA torque command.
 */
bool tmc9660_app_runQBL5704BLDCHallFeedbackTest(uint16_t icID) {
	// 0. Setup Necessary Parameter Fields
	// Side Effect: COMMUTATION_MODE=FOC_HALL(6),
	bool test = tmc9660_app_setupQBL5704BLDCHallFeedbackTest(icID);
	if (test == false) {
		return false;
	}

    // 1. Set Maximum Current & Torque Limits (ID 6)
    // In FOC, these are controlled by the same parameter (Max Iq).
    // Setting to 1000 units (Assuming 1.0A / 1000mA limit).
    tmc9660_param_setParameter(icID, TMC9660_PARAM_MAX_TORQUE, 1000);

    // 2. Set Maximum Voltage Limit (ID 5)
    // Setting to 240 units (24.0V limit @ 100mV per unit).
    tmc9660_param_setParameter(icID, TMC9660_PARAM_OUTPUT_VOLTAGE_LIMIT, 240);

    // 3. Set Target Torque (ID 104)
    // Command a constant 200mA torque producing current (Iq).
    // This will cause the motor to spin if the Hall alignment is correct.
    tmc9660_param_setParameter(icID, TMC9660_PARAM_TARGET_TORQUE, 200);
    HAL_Delay(5000);
    tmc9660_param_setParameter(icID, TMC9660_PARAM_TARGET_TORQUE, 0);

    return true;
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
  MX_USB_DEVICE_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  uint32_t rVal = 0;
  TMC9660BlStatus blStatus;
  uint32_t reg = 0;

  HAL_Delay(2000);

  UART_Printf("\r\n--- TMC9660 Boot Configuration ---\r\n");

  // Can burn these to One-Time-Programming Memory, but not necessary
    // Instead, just run following code block on boot
    // Note: TMC9660's LDOs will lag according to SS_VEXTx_FIELD
    // Note: Fields in Steps #1-#5 are from TMC9660 Datasheet
    // Note: Fields in Steps #6 are from TMC9660 Parameter Mode Guide

    bool bootInit = tmc9660_app_GotoBootMode(tmc9660_id);
    if (bootInit == false) {
  	  UART_Printf("Failed to enter Boot Mode\n");
  	  ERR_Handler(__LINE__);
    }

    // Select CONFIG Bank 5
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_BANK, 5, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
    	  ERR_Handler(__LINE__);
    }

    // 1. LDO & POWER
    // VEXT1=5.0V(3), Slope=3ms(0), VEXT2=3.3V(2), Slope=3ms(0), ShortFault=false(0)
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_00_POWER, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_00_POWER_VEXT1_FIELD, 3); 				/* 0: LDO disabled
  																				 * 1: 2.5V
  																				 * 2: 3.3V
  																				 * 3: 5.0V <--
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_00_POWER_SS_VEXT1_FIELD, 0);			/* 0: 3ms <--
  																				 * 1: 1.5ms
  																				 * 2: 0.75ms
  																				 * 3: 0.37ms
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_00_POWER_VEXT2_FIELD, 2);				/* 0: LDO disabled
  																				 * 1: 2.5V
  																				 * 2: 3.3V <--
  																				 * 3: 5.0V
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_00_POWER_SS_VEXT2_FIELD, 0);			/* 0: 3ms	<--
  																				 * 1: 1.5ms
  																				 * 2: 0.75ms
  																				 * 3: 0.37ms
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_00_POWER_LDO_SHORT_FAULT_FIELD, 0);		// Default: 0
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_00_POWER, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
    	  ERR_Handler(__LINE__);
    }
    // 2. UART & ADDRESSES
    // ChipAddr=1, HostAddr=255
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_01_ADDRESS, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
    	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_01_DEVICE_ADDRESS_FIELD, 1);			// Default: 1
    reg = field_update16(reg, CONFIG_BOOT_01_MASTER_ADDRESS_FIELD, 255);			// Default: 255
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_01_ADDRESS, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 3. INTERFACE CONFIG
    // UART Enable (Disable=0), SPI Slave Disable(1), TX=GPIO6(0), RX=GPIO7(0), Baud=auto16(7), SCK=GPIO11(1)
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_03_BOOT_INTERFACE, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_03_INTERFACE_BL_DISABLE_UART_FIELD, 0); // Default: 0
    reg = field_update16(reg, CONFIG_BOOT_03_INTERFACE_BL_DISABLE_SPI_FIELD, 1);	// Default: 0
    reg = field_update16(reg, CONFIG_BOOT_03_INTERFACE_BL_UART_TX_FIELD, 0);		/* 0: GPIO6 <--
  																				 * 1: GPIO0
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_03_INTERFACE_BL_UART_RX_FIELD, 0);		/* 0: GPIO7	<--
  																				 * 1: GPIO0
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_03_INTERFACE_BL_UART_BAUDRATE_FIELD, 7);/* 0: 9600
  																				 * 1: 19200
  																				 * 2: 38400
  																				 * 3: 57600
  																				 * 4: 115200
  																				 * 5: 1000000
  																				 * 6: auto8
  																				 * 7: auto16 <--
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_03_INTERFACE_BL_SPI0_SCK_FIELD, 1);		/* 0: GPIO6
  																				 * 1: GPIO11 <--
  																				 * Note: This is bit is only required for
  																				 * the SPI bootloader communication if it
  																				 * uses SPI0 (BL_SPI_SELECT=0)
  																				 */
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_03_BOOT_INTERFACE, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 4. SPI FLASH
    // Enable=true, CS=GPIO12, Freq=10MHz(3 @ 40MHz Sys)
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_05_FLASH, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_05_SPI_FLASH_EN_FIELD, 1);				// Default: 0
    reg = field_update16(reg, CONFIG_BOOT_05_FLASH_SPI_CS_SW_FIELD, 12);			/* assuming this field is SPI_FLASH_CS from datasheet
  																				 * assign GPIO pin to SPI_FLASH chip select
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_05_FLASH_SPI_DIV_FIELD, 3);				/* assuming this field is SPI_FLASH_FREQ from datasheet
  																				 * default .toml file sets SPI_FLASH_FREQ to 10MHz
  																				 * and datasheet shows: SPI_FLASH_FREQ=3 -> 10MHz
  																				 */
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_05_FLASH, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 5. CLOCK & PLL
    // ExtOsc(1), 16MHz(3), Boost=false(0), SysFreq=40MHz(0), RDIV=15(16MHz-1)
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_0C_CLK_SEL_INIT, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_32, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    // TODO: Custom board will have to use Internal Oscillator
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_EXT_NOT_INT_FIELD, 1);	/* 0: Internal 15MHz oscillator
  																				 * 1: External clock source selected by EXT_NOT_XTAL.
  																				 */
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_XTAL_CFG_FIELD, 3);		/* 0: RESERVED
  																				 * 1: 8MHz
  																				 * 2: RESERVED
  																				 * 3: 16MHz
  																				 * 4: RESERVED
  																				 * 5: 24MHz-25MHz
  																				 * 6: 32MHz
  																				 * 7: RESERVED
  																				 */
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_EXT_NOT_XTAL_FIELD, 0); /* 0: External oscillator
  																				 * 1: External clock
  																				 */
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_XTAL_BOOST_FIELD, 0);	// Default: 0
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_PLL_OUT_SEL_FIELD, 1);	/* 0. Use the internal oscillator
  																				 * 1: Use the PLL
  																				 * 2: RESERVED
  																				 * 3: RESERVED
  																				 */
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_RDIV_FIELD, 15);		/* Divider of the PLL input frequency. Must be set to the input frequency in
  																				 * MHz minus one.
  																				 * The internal oscillator has a frequency of 15 MHz. For Internal oscillator
  																				 * input (EXT_NOT_INT=0), RDIV must be set to 14.
  																				 * Default: 14
  																				 */
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_SYS_CLK_DIV_FIELD, 0);  /* 0: 40MHz
  																				 * 1: RESERVED
  																				 * 2: RESERVED
  																				 * 3: 15MHz
  																				 * Note: Only change this setting during the workaround for Erratum 1:
  																				 * Bootloader OTP_BURN Command.
  																				 */
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_0C_CLK_SEL_INIT, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_32, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    HAL_Delay(50); // PLL stabilization
    // TODO: read PLL_STATUS reg to confirm configuration correctness
    /* Note: Running the motor control system is only supported with system clock configured to 40 MHz (PLL_OUT_SEL=1
     * 	   with a valid PLL configuration and SYS_CLK_DIV=0).
     * Note: The clock configuration should be written with a single WRITE_32 or WRITE_32_INC command.
     */

    // 6. APP CONFIG 0 (HALL)
    // Enable=true, U=2, V=3, W=4
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_10_APP_CONFIG_0, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_10_APP_CONFIG_0_HALL_ENABLE_FIELD, 1);  // Default: 0
    reg = field_update16(reg, CONFIG_BOOT_10_APP_CONFIG_0_HALL_UX_FIELD, 0);		/* 0: GPIO2	<--
  																				 * 1: GPIO7
  																				 * 2: GPIO9
  																				 * 3: RESERVED
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_10_APP_CONFIG_0_HALL_V_FIELD, 0);		/* 0: GPIO3	<--
  																				 * 1: GPIO15
  																				 * 2: RESERVED
  																				 * 3: RESERVED
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_10_APP_CONFIG_0_HALL_WY_FIELD, 0);		/* 0: GPIO4	<--
  																				 * 1: GPIO8
  																				 * 2: GPIO10
  																				 * 3: RESERVED
  																				 */
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_10_APP_CONFIG_0, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 7. APP CONFIG 2 (WATCHDOG)
    // Enable=true(Disable=0), Timeout=2000ms(7)
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_12_APP_CONFIG_2, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_12_APP_CONFIG_2_WDG_DISABLE_FIELD, 1);	// Default: 0
  //      reg = field_update16(reg, CONFIG_BOOT_12_APP_CONFIG_2_WDG_TIMEOUT_FIELD, 7); // removed, could not find documentation
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_12_APP_CONFIG_2, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 8. GPIO INIT (GPIO 5, 17, 18)
    // GPIO17/18: Input, Pulldown. GPIO5: Analog.
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_0B_GPIO_16_18_INIT, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO17_OUT_EN_FIELD, 0); // Input
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO17_PD_FIELD, 1);     // Pulldown
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO18_OUT_EN_FIELD, 0); // Input
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO18_PD_FIELD, 1);     // Pulldown
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO5_ANALOG_EN_FIELD, 1); // GPIO5 Analog
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_0B_GPIO_16_18_INIT, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 9. BOOTSTRAP
    // Mode=Param(2), StartApp=true(1)
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_04_BOOTSTRAP, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_READ_16, 0, &reg);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    reg = field_update16(reg, CONFIG_BOOT_04_BOOTSTRAP_BOOT_APP_FIELD, 2);		// I assume this field is BOOT_MODE from datasheet
  																				/* 0: RESERVED
  																				 * 1: Register mode
  																				 * 2: Parameter mode
  																				 * 3: RESERVED
  																				 */
    reg = field_update16(reg, CONFIG_BOOT_04_BOOTSTRAP_LOAD_ROM_CODE_FIELD, 1);	// I assume this field is START_MOTOR_CTRL from datasheet
  																				// Default: 0
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_04_BOOTSTRAP, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }
    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
    if (blStatus != TMC9660_BLSTATUS_OK) {
  	  ERR_Handler(__LINE__);
    }

    // 10. FINAL BOOT
    UART_Printf("Soft-Config Applied. Transitioning to Parameter Mode...\r\n");
    // Note: confused whether BL cmd: CONFIG_BOOT_04_BOOTSTRAP_LOAD_ROM_CODE_FIELD or if param cmd: TMC9660_CMD_BOOT will start program
    HAL_Delay(200);

    uint32_t mV_voltage_motor = tmc9660_param_getParameter(tmc9660_id, TMC9660_PARAM_SUPPLY_VOLTAGE);
    if (mV_voltage_motor > 0) {
  	  tmc_detected = true;
  	  UART_Printf("SUCCESS: IC Active in Parameter Mode.\r\n");
  	  UART_Printf("VM: %d.%d\n", mV_voltage_motor / 10, mV_voltage_motor % 10); // unit conversion 100mV -> 1V
    }

    bool test = tmc9660_app_alignQBL5704HallOffset(tmc9660_id);
    if ( test == true ) {
    	  UART_Printf("Motor Test Setup Success\n");
    }
    else
    {
  	  UART_Printf("Motor Test Setup Fail\n");
    }

    // REMOVE FOR BOARD TESTING
  //  test = tmc9660_app_runQBL5704BLDCHallFeedbackTest(tmc9660_id);
  //  if ( test == true ) {
  //	  UART_Printf("Motor Test Run Success\n");
  //  }
  //  else
  //  {
  //  	  UART_Printf("Motor Test Run Fail\n");
  //  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  if (tmc_detected == true)
	  	{
	  	  /* Heartbeat: Slow toggle if chip was found */
	  	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
	  	  HAL_Delay(500);
	  	}
	  	else
	  	{
	  	  /* Error: Fast blink if chip was not detected */
	  	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
	  	  HAL_Delay(100);
	  	}
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
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
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_12|GPIO_PIN_3
                          |GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB12 PB3
                           PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_12|GPIO_PIN_3
                          |GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA9 PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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

#ifdef  USE_FULL_ASSERT
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
