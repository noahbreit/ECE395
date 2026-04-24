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
void tmc9660_app_HoldInResetUntilReady(void);
void tmc9660_app_ReportConditionAndHalt(void);
bool tmc9660_app_VerifyPLLStatus(uint16_t icID);
void tmc9660_app_DebugStatus(uint16_t icID);
void tmc9660_app_DebugGeneralStatus(uint16_t icID);
void tmc9660_app_DebugGeneralErrors(uint16_t icID);
void tmc9660_app_DebugGDRVErrors(uint16_t icID);
bool tmc9660_app_ClearErrorsManual(uint16_t icID, uint32_t sysMask, uint32_t gdrvMask);
void tmc9660_app_EnableDrive(void);
void tmc9660_app_DisableDrive(void);
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
			HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, &byteIn, 1, 50);
			if (status != HAL_OK) {
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

    // Allow Print to goto serial
    // TODO: MUST REMOVE DELAY if ported to RTOS
    HAL_Delay(5);
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

/* * Holds the TMC9660 in hardware reset and waits for a user start command.
 * Note: Uses PA15 (RSTN) and monitors PB9 (FAULTN) per schematic labels.
 */
void tmc9660_app_HoldInResetUntilReady(void) {
    uint8_t user_cmd = 0;
    extern uint8_t UserRxBufferFS[]; // From usbd_cdc_if.c

    // 1. Assert Hardware Reset (Active Low)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

    // 2. Ensure Wake is high (PB8 is DRIVER_WAKE in schematic)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);

    // 3. Ensure Drive_En is low (PB3)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

    UART_Printf("\r\n[SYSTEM] TMC9660 held in RESET to prevent brownout.\r\n");
    UART_Printf("[SYSTEM] Wait for DC link capacitors to stabilize...\r\n");
    UART_Printf("[SYSTEM] Press 's' to release driver and start init: ");

    // 3. Blocking loop until 's' is received via USB CDC
    while (user_cmd != 's') {
        // Simple polling of the CDC receive buffer
        if (UserRxBufferFS[0] != 0) {
            user_cmd = UserRxBufferFS[0];
            UserRxBufferFS[0] = 0; // Clear for next use
        }

        // Optional: Monitor Fault pin (PB9) while waiting
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_RESET) {
            // Add warning if driver indicates a hardware fault during standby
        }

        HAL_Delay(10);
    }

    // 5. Release Reset
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);

    // 6. Delay Block
    HAL_Delay(100); // Allow TMC9660 internal LDOs to ramp up
    UART_Printf("\r\n[SYSTEM] Driver Released. Proceeding to Init...\r\n");
}

/**
 * @brief Protects the system by disabling the driver and reporting status.
 * To be called in Error_Handler or safety-critical loops.
 */
void tmc9660_app_ReportConditionAndHalt(void) {
    // 1. Assert Safe State (Active Off)
    // DRIVER_DRV_EN (PB3): Setting LOW to disable the gate driver bridge
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

    // DRIVER_RSTN (PA15): Pulling LOW to hold the TMC9660 in Hardware Reset
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

    // 2. Read Fault Condition
    GPIO_PinState fault_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9);

    // 3. Serial Reporting
    UART_Printf("\r\n[FATAL] System Halted for Protection.\r\n");
    if (fault_state == GPIO_PIN_RESET) {
        UART_Printf("[STATUS] DRIVER_FAULTN: ACTIVE (Fault Detected)\r\n");
    } else {
        UART_Printf("[STATUS] DRIVER_FAULTN: CLEAR (Software/Logic Error)\r\n");
    }

    // 4. Visual Warning: Solid Red LED
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

/**
 * @brief Verifies that the TMC9660 PLL is locked and correctly configured.
 * Target: 40MHz system clock with internal oscillator.
 * @return bool True if PLL is locked and settings match required 40MHz operation.
 */
bool tmc9660_app_VerifyPLLStatus(uint16_t icID) {
    uint32_t regValue = 0;
    uint32_t dummy = 0;

    // 1. Set the Bootloader address to the Clock Config register
    // Address: 0x00020018
    if (tmc9660_bl_sendCommand(icID, TMC9660_BLCMD_SET_ADDRESS, CONFIG_BOOT_0C_CLK_SEL_INIT, &dummy) != TMC9660_BLSTATUS_OK) {
        return false;
    }

    // 2. Read the 32-bit register value
    if (tmc9660_bl_sendCommand(icID, TMC9660_BLCMD_READ_32, 0, &regValue) != TMC9660_BLSTATUS_OK) {
        return false;
    }

    // 3. Extract Fields using the Library Macros
    uint8_t pll_lock = (uint8_t)field_extract32(regValue, CONFIG_BOOT_0C_CLK_SEL_INIT_PLL_STATUS_FIELD);
    uint8_t out_sel  = (uint8_t)field_extract32(regValue, CONFIG_BOOT_0C_CLK_SEL_INIT_PLL_OUT_SEL_FIELD);
    uint8_t clk_div  = (uint8_t)field_extract32(regValue, CONFIG_BOOT_0C_CLK_SEL_INIT_SYS_CLK_DIV_FIELD);

    // 4. Report Status over Serial
    UART_Printf("\r\n--- PLL HARDWARE CHECK ---\r\n");
    UART_Printf("[STATUS] Lock Bit: %s\r\n", (pll_lock == 1) ? "LOCKED" : "UNLOCKED (FAIL)");
    UART_Printf("[STATUS] OUT_SEL : %d (Expect 1 for PLL)\r\n", out_sel);
    UART_Printf("[STATUS] CLK_DIV : %d (Expect 0 for 40MHz)\r\n", clk_div);

    // 5. Logical Verification
    // Success requires Lock bit active AND System clock routed through the PLL (OUT_SEL=1)
    // and no further division (SYS_CLK_DIV=0)
    if (pll_lock == 1 && out_sel == 1 && clk_div == 0) {
        UART_Printf("[RESULT] Clock stable at 40MHz.\r\n");
        return true;
    }
	UART_Printf("[RESULT] PLL Configuration Failed!\r\n");


    return false;
}

void tmc9660_app_DebugStatus(uint16_t icID) {
    // Read the Status Flags (Parameters 110, 300)
	tmc9660_app_DebugGeneralStatus(icID);
	tmc9660_app_DebugGeneralErrors(icID);
	tmc9660_app_DebugGDRVErrors(icID);
}

/**
 * @brief Reports all active General Status flags.
 * Uses TMC9660_PARAM_GENERAL_STATUS_FLAGS (289).
 */
void tmc9660_app_DebugGeneralStatus(uint16_t icID) {
    uint32_t status = tmc9660_param_getParameter(icID, TMC9660_PARAM_GENERAL_STATUS_FLAGS);
    if (status == 0) return;

    UART_Printf("\r\n--- TMC9660 GENERAL STATUS ---\r\n");
    UART_Printf("[STATUS] Raw: 0x%08lX\r\n", status);

    // --- Regulation Modes ---
    if (status & 0x00000001) UART_Printf(" . REGULATION_STOPPED\r\n");
    if (status & 0x00000002) UART_Printf(" . REGULATION_TORQUE_ACTIVE\r\n");
    if (status & 0x00000004) UART_Printf(" . REGULATION_VELOCITY_ACTIVE\r\n");
    if (status & 0x00000008) UART_Printf(" . REGULATION_POSITION_ACTIVE\r\n");

    // --- Configuration State ---
    if (status & 0x00000010) UART_Printf(" . CONFIG_STORED_SUCCESS\r\n");
    if (status & 0x00000020) UART_Printf(" . CONFIG_LOADED_SUCCESS\r\n");
    if (status & 0x00000040) UART_Printf(" . CONFIG_READ_ONLY\r\n");
    if (status & 0x00000080) UART_Printf(" . TMCL_SCRIPT_READ_ONLY\r\n");

    // --- Motion & Protection ---
    if (status & 0x00000100) UART_Printf(" . BRAKE_CHOPPER_ACTIVE\r\n");
    if (status & 0x00000200) UART_Printf(" . POSITION_REACHED\r\n");
    if (status & 0x00000400) UART_Printf(" . VELOCITY_REACHED\r\n");
    if (status & 0x00000800) UART_Printf(" . ADC_OFFSET_CALIBRATED\r\n");

    // --- Ramper Status ---
    if (status & 0x00001000) UART_Printf(" . RAMPER_LATCHED\r\n");
    if (status & 0x00002000) UART_Printf(" . RAMPER_EVENT_STOP_SWITCH\r\n");
    if (status & 0x00004000) UART_Printf(" . RAMPER_EVENT_STOP_DEVIATION\r\n");
    if (status & 0x00008000) UART_Printf(" . RAMPER_VELOCITY_REACHED\r\n");
    if (status & 0x00010000) UART_Printf(" . RAMPER_POSITION_REACHED\r\n");
    if (status & 0x00020000) UART_Printf(" . RAMPER_SECOND_MOVE_REQUIRED\r\n");

    // --- Monitoring & Braking ---
    if (status & 0x00040000) UART_Printf(" . IIT_1_MONITOR_ACTIVE\r\n");
    if (status & 0x00080000) UART_Printf(" . IIT_2_MONITOR_ACTIVE\r\n");
    if (status & 0x00100000) UART_Printf(" . REFSEARCH_FINISHED\r\n");
    if (status & 0x00200000) UART_Printf(" . Y2_USED_FOR_BRAKING\r\n");

    // --- Hardware Peripheral Availability ---
    if (status & 0x00800000) UART_Printf(" . STEPDIR_INPUT_AVAILABLE\r\n");
    if (status & 0x01000000) UART_Printf(" . RIGHT_REF_SWITCH_AVAILABLE\r\n");
    if (status & 0x02000000) UART_Printf(" . HOME_REF_SWITCH_AVAILABLE\r\n");
    if (status & 0x04000000) UART_Printf(" . LEFT_REF_SWITCH_AVAILABLE\r\n");
    if (status & 0x08000000) UART_Printf(" . ABN2_FEEDBACK_AVAILABLE\r\n");
    if (status & 0x10000000) UART_Printf(" . HALL_FEEDBACK_AVAILABLE\r\n");
    if (status & 0x20000000) UART_Printf(" . ABN1_FEEDBACK_AVAILABLE\r\n");
    if (status & 0x40000000) UART_Printf(" . SPI_FLASH_AVAILABLE\r\n");
    if (status & 0x80000000) UART_Printf(" . I2C_EEPROM_AVAILABLE\r\n");
}

void tmc9660_app_DebugGDRVErrors(uint16_t icID) {
    uint32_t gdrv = tmc9660_param_getParameter(icID, TMC9660_PARAM_GDRV_ERROR_FLAGS);
    if (gdrv == 0) return;

    UART_Printf("\r\n[GDRV FAULT] Raw: 0x%08lX\r\n", gdrv);

    // --- LOW SIDE OVERCURRENT ---
    if (gdrv & 0x00000001) UART_Printf(" ! U_LOW_SIDE_OVERCURRENT\r\n");
    if (gdrv & 0x00000002) UART_Printf(" ! V_LOW_SIDE_OVERCURRENT\r\n");
    if (gdrv & 0x00000004) UART_Printf(" ! W_LOW_SIDE_OVERCURRENT\r\n");
    if (gdrv & 0x00000008) UART_Printf(" ! Y2_LOW_SIDE_OVERCURRENT\r\n");

    // --- LOW SIDE DISCHARGE SHORT ---
    if (gdrv & 0x00000010) UART_Printf(" ! U_LOW_SIDE_DISCHARGE_SHORT\r\n");
    if (gdrv & 0x00000020) UART_Printf(" ! V_LOW_SIDE_DISCHARGE_SHORT\r\n");
    if (gdrv & 0x00000040) UART_Printf(" ! W_LOW_SIDE_DISCHARGE_SHORT\r\n");
    if (gdrv & 0x00000080) UART_Printf(" ! Y2_LOW_SIDE_DISCHARGE_SHORT\r\n");

    // --- LOW SIDE CHARGE SHORT ---
    if (gdrv & 0x00000100) UART_Printf(" ! U_LOW_SIDE_CHARGE_SHORT\r\n");
    if (gdrv & 0x00000200) UART_Printf(" ! V_LOW_SIDE_CHARGE_SHORT\r\n");
    if (gdrv & 0x00000400) UART_Printf(" ! W_LOW_SIDE_CHARGE_SHORT\r\n");
    if (gdrv & 0x00000800) UART_Printf(" ! Y2_LOW_SIDE_CHARGE_SHORT\r\n");

    // --- BOOTSTRAP UNDERVOLTAGE ---
    if (gdrv & 0x00001000) UART_Printf(" ! U_BOOTSTRAP_UNDERVOLTAGE\r\n");
    if (gdrv & 0x00002000) UART_Printf(" ! V_BOOTSTRAP_UNDERVOLTAGE\r\n");
    if (gdrv & 0x00004000) UART_Printf(" ! W_BOOTSTRAP_UNDERVOLTAGE\r\n");
    if (gdrv & 0x00008000) UART_Printf(" ! Y2_BOOTSTRAP_UNDERVOLTAGE\r\n");

    // --- HIGH SIDE OVERCURRENT ---
    if (gdrv & 0x00010000) UART_Printf(" ! U_HIGH_SIDE_OVERCURRENT\r\n");
    if (gdrv & 0x00020000) UART_Printf(" ! V_HIGH_SIDE_OVERCURRENT\r\n");
    if (gdrv & 0x00040000) UART_Printf(" ! W_HIGH_SIDE_OVERCURRENT\r\n");
    if (gdrv & 0x00080000) UART_Printf(" ! Y2_HIGH_SIDE_OVERCURRENT\r\n");

    // --- HIGH SIDE DISCHARGE SHORT ---
    if (gdrv & 0x00100000) UART_Printf(" ! U_HIGH_SIDE_DISCHARGE_SHORT\r\n");
    if (gdrv & 0x00200000) UART_Printf(" ! V_HIGH_SIDE_DISCHARGE_SHORT\r\n");
    if (gdrv & 0x00400000) UART_Printf(" ! W_HIGH_SIDE_DISCHARGE_SHORT\r\n");
    if (gdrv & 0x00800000) UART_Printf(" ! Y2_HIGH_SIDE_DISCHARGE_SHORT\r\n");

    // --- HIGH SIDE CHARGE SHORT  ---
    if (gdrv & 0x01000000) UART_Printf(" ! U_HIGH_SIDE_CHARGE_SHORT\r\n");
    if (gdrv & 0x02000000) UART_Printf(" ! V_HIGH_SIDE_CHARGE_SHORT\r\n");
    if (gdrv & 0x04000000) UART_Printf(" ! W_HIGH_SIDE_CHARGE_SHORT\r\n");
    if (gdrv & 0x08000000) UART_Printf(" ! Y2_HIGH_SIDE_CHARGE_SHORT\r\n");

    // --- GLOBAL GATE DRIVER ERRORS ---
    if (gdrv & 0x20000000) UART_Printf(" ! GDRV_UNDERVOLTAGE (Logic Rail Fail)\r\n");
    if (gdrv & 0x40000000) UART_Printf(" ! GDRV_LOW_VOLTAGE (Marginal Logic Rail)\r\n");
    if (gdrv & 0x80000000) UART_Printf(" ! GDRV_SUPPLY_UNDERVOLTAGE (VEXT1 fail)\r\n");
}

void tmc9660_app_DebugGeneralErrors(uint16_t icID) {
    uint32_t err = tmc9660_param_getParameter(icID, TMC9660_PARAM_GENERAL_ERROR_FLAGS);
    if (err == 0) return;

    UART_Printf("\r\n[SYS ERROR] Raw: 0x%08lX\r\n", err);
    if (err & 0x00000001) UART_Printf(" ! CONFIG_ERROR\r\n");
    if (err & 0x00000002) UART_Printf(" ! TMCL_SCRIPT_ERROR\r\n");
    if (err & 0x00000004) UART_Printf(" ! HOMESWTICH_NOT_FOUND\r\n");
    if (err & 0x00000020) UART_Printf(" ! HALL_ERROR\r\n");
    if (err & 0x00000200) UART_Printf(" ! WATCHDOG_EVENT\r\n");
    if (err & 0x00002000) UART_Printf(" ! EXT_TEMP_EXCEEDED\r\n");
    if (err & 0x00004000) UART_Printf(" ! CHIP_TEMP_EXCEEDED\r\n");
    if (err & 0x00010000) UART_Printf(" ! I2T_1_EXCEEDED\r\n");
    if (err & 0x00020000) UART_Printf(" ! I2T_2_EXCEEDED\r\n");
    if (err & 0x00040000) UART_Printf(" ! EXT_TEMP_WARNING\r\n");
    if (err & 0x00080000) UART_Printf(" ! SUPPLY_OVERVOLTAGE_WARNING\r\n");
    if (err & 0x00100000) UART_Printf(" ! SUPPLY_UNDERVOLTAGE_WARNING\r\n");
    if (err & 0x00200000) UART_Printf(" ! ADC_IN_OVERVOLTAGE\r\n");
    if (err & 0x00400000) UART_Printf(" ! FAULT_RETRY_HAPPENED\r\n");
    if (err & 0x00800000) UART_Printf(" ! FAULT_RETRIES_FAILED\r\n");
    if (err & 0x01000000) UART_Printf(" ! CHIP_TEMP_WARNING\r\n");
    if (err & 0x04000000) UART_Printf(" ! HEARTBEAT_STOPPED\r\n");
}

/**
 * @brief Clears active latching errors using setParameter (SAP) and verifies the result.
 * @param icID The identifier for the IC.
 * @param sysMask Bitmask for General Error Flags (Param 299).
 * @param gdrvMask Bitmask for Gate Driver Error Flags (Param 300).
 * @return bool True if all RWC bits in the provided masks were successfully cleared.
 */
bool tmc9660_app_ClearErrorsManual(uint16_t icID, uint32_t sysMask, uint32_t gdrvMask) {
  // 1. Clear latching General Errors (RWC bits, e.g., 0x20 for HALL_ERROR)
  //
  if (sysMask != 0) {
    tmc9660_param_setParameter(icID, TMC9660_PARAM_GENERAL_ERROR_FLAGS, sysMask);
  }

  // 2. Clear latching Gate Driver Errors (All bits are RWC)
  if (gdrvMask != 0) {
    tmc9660_param_setParameter(icID, TMC9660_PARAM_GDRV_ERROR_FLAGS, gdrvMask);
  }

  // 3. Small delay to allow internal state machines to update
  HAL_Delay(25);

  // 4. Read back and verify
  uint32_t sysActual = tmc9660_param_getParameter(icID, TMC9660_PARAM_GENERAL_ERROR_FLAGS);
  uint32_t gdrvActual = tmc9660_param_getParameter(icID, TMC9660_PARAM_GDRV_ERROR_FLAGS);

  // Note: Bits 0 and 1 of Param 299 (CONFIG/SCRIPT) are Read-Only status indicators
  // and will only clear when the underlying configuration is valid.
  uint32_t remainingSys = sysActual & sysMask;
  uint32_t remainingGdrv = gdrvActual & gdrvMask;

  if (remainingSys == 0 && remainingGdrv == 0) {
    UART_Printf("[CLEAR] Success: Requested latching bits cleared.\r\n");
    return true;
  }

  UART_Printf("[CLEAR] Failed: Flags 0x%08lX (SYS) and 0x%08lX (GDRV) still active.\r\n", remainingSys, remainingGdrv);
  return false;
}

void tmc9660_app_EnableDrive(void) {
	// Set Drive_En is HIGH (PB3)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
}

void tmc9660_app_DisableDrive(void) {
	// Set Drive_En is LOW (PB3)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
}

void ERR_Handler(int line) {
	UART_Printf("\nFailure @ line: %d\n", line);
	tmc9660_app_ReportConditionAndHalt();
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

bool tmc9660_app_setupQBL5704BLDCHallFeedbackTest(uint16_t icID) {
    UART_Printf("\r\n--- Starting QBL5704 Setup ---\r\n");

    // 1. Existing Setup (Motor Type, Pole Pairs, etc.)
    if (!verifyMotorVoltage(icID)) return false;
    tmc9660_param_setParameter(icID, TMC9660_PARAM_MOTOR_TYPE, 3);
    tmc9660_param_setParameter(icID, TMC9660_PARAM_MOTOR_POLE_PAIRS, 2);

    // 2. THE FIX: Configure Gate Drive BBM (Deadtime)
    // BBM_LOW_UVW (235) and BBM_HIGH_UVW (236). Value 60 ≈ 500ns.
    tmc9660_param_setParameter(icID, TMC9660_PARAM_BREAK_BEFORE_MAKE_TIME_LOW_UVW, 60);
    tmc9660_param_setParameter(icID, TMC9660_PARAM_BREAK_BEFORE_MAKE_TIME_HIGH_UVW, 60);

    // 3. THE FIX: Increase Voltage Limit
    // Increasing from 240 to 8000 (Default) to ensure PWM generation.
    tmc9660_param_setParameter(icID, TMC9660_PARAM_OUTPUT_VOLTAGE_LIMIT, 8000);

    // 4. THE FIX: Bootstrap Pre-charge
    // Set COMMUTATION_MODE (4) to mode 1 (LOW_SIDE_ON) momentarily.
    tmc9660_param_setParameter(icID, TMC9660_PARAM_COMMUTATION_MODE, 1);
    HAL_Delay(50);

    // 5. Set final commutation mode
    tmc9660_param_setParameter(icID, TMC9660_PARAM_COMMUTATION_MODE, 6);

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

  // Handle TMC9660 brownout at HV rail startup
  tmc9660_app_HoldInResetUntilReady();

  HAL_Delay(1000);

  UART_Printf("\r\n--- TMC9660 Boot Configuration ---\r\n");
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

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
    // UPDATED TO Internal!!
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_EXT_NOT_INT_FIELD, 0);	/* 0: Internal 15MHz oscillator
  																				 * 1: External clock source selected by EXT_NOT_XTAL.
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
    reg = field_update32(reg, CONFIG_BOOT_0C_CLK_SEL_INIT_RDIV_FIELD, 14);		/* Divider of the PLL input frequency. Must be set to the input frequency in
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
    if (!tmc9660_app_VerifyPLLStatus(tmc9660_id)) {
		UART_Printf("FATAL: PLL Failed to lock at 40MHz. System clock unstable.\r\n");
		ERR_Handler(__LINE__);
	}

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
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO5_ANALOG_EN_FIELD, 1);

    // Ensure GPIO 2, 3, 4 (Halls) are DIGITAL
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO2_ANALOG_EN_FIELD, 0);
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO3_ANALOG_EN_FIELD, 0);
    reg = field_update16(reg, CONFIG_BOOT_0B_GPIO_16_18_INIT_GPIO4_ANALOG_EN_FIELD, 0);


    blStatus = tmc9660_bl_sendCommand(tmc9660_id, TMC9660_BLCMD_WRITE_16, reg, &rVal);
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

    // 11. SOFTWARE ERROR CLEARING
    // Clear latched HALL_ERROR (0x20) and any early GDRV faults
    uint32_t sysMask = 0x00000020;
    uint32_t gdrvMask = 0xFFFFFFFF;
    tmc9660_app_ClearErrorsManual(tmc9660_id, sysMask, gdrvMask);
    HAL_Delay(50);

    // 12. CONFIGURE SAFETY LIMITS
    // Setting non-zero limits clears the R-only CONFIG_ERROR
    tmc9660_param_setParameter(tmc9660_id, TMC9660_PARAM_OUTPUT_VOLTAGE_LIMIT, 8000); // Default 25%
    tmc9660_param_setParameter(tmc9660_id, TMC9660_PARAM_MAX_TORQUE, 1000);           // 1A limit
    tmc9660_param_setParameter(tmc9660_id, TMC9660_PARAM_BREAK_BEFORE_MAKE_TIME_LOW_UVW, 60); // 500ns Deadtime
    tmc9660_param_setParameter(tmc9660_id, TMC9660_PARAM_BREAK_BEFORE_MAKE_TIME_HIGH_UVW, 60);

    // 13. BOOTSTRAP PRE-CHARGE
//    tmc9660_param_setParameter(tmc9660_id, TMC9660_PARAM_COMMUTATION_MODE, 1); // Low-side ON
    HAL_Delay(50);

    // 14. ENABLE HARDWARE DRIVE (PB3)
//    tmc9660_app_EnableDrive();

    // 15. START FOC HALL CONTROL
//    tmc9660_param_setParameter(tmc9660_id, TMC9660_PARAM_COMMUTATION_MODE, 6);
//    UART_Printf("FOC System Ready.\r\n");


//    // UNIT TESTS
//    tmc9660_app_setupQBL5704BLDCHallFeedbackTest(tmc9660_id);
//    bool test = tmc9660_app_alignQBL5704HallOffset(tmc9660_id);
//    if ( test == true ) {
//    	  UART_Printf("Motor Test Setup Success\n");
//    }
//    else
//    {
//  	  UART_Printf("Motor Test Setup Fail\n");
//    }
//
//    test = tmc9660_app_runQBL5704BLDCHallFeedbackTest(tmc9660_id);
//    if ( test == true ) {
//  	  UART_Printf("Motor Test Run Success\n");
//    }
//    else
//    {
//    	  UART_Printf("Motor Test Run Fail\n");
//    }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  tmc9660_app_DebugStatus(tmc9660_id);
	  HAL_Delay(1000);
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_15, GPIO_PIN_RESET);

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

  /*Configure GPIO pins : PA4 PA5 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_15;
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
