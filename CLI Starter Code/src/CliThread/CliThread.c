/**************************************************************************/ /**
 * @file      CliThread.c
 * @brief     File for the CLI Thread handler. Uses FREERTOS + CLI
 * @author    Eduardo Garcia
 * @date      2020-02-15

 ******************************************************************************/

/******************************************************************************
 * Includes
 ******************************************************************************/
#include "CliThread.h"
#include "SerialConsole.h"
#include "FreeRTOS.h"
#include "task.h"

/******************************************************************************
 * Defines
 ******************************************************************************/
#define FIRMWARE_VERSION "0.0.1" // Firmware version that can be easily modified

/******************************************************************************
 * Variables
 ******************************************************************************/
static int8_t *const pcWelcomeMessage =
    "FreeRTOS CLI.\r\nType Help to view a list of registered commands.\r\n";

// Clear screen command
const CLI_Command_Definition_t xClearScreen =
    {
        CLI_COMMAND_CLEAR_SCREEN,
        CLI_HELP_CLEAR_SCREEN,
        CLI_CALLBACK_CLEAR_SCREEN,
        CLI_PARAMS_CLEAR_SCREEN};

static const CLI_Command_Definition_t xResetCommand =
    {
        "reset",
        "reset: Resets the device\r\n",
        (const pdCOMMAND_LINE_CALLBACK)CLI_ResetDevice,
        0};

/******************************************************************************
 * Forward Declarations
 ******************************************************************************/
static void FreeRTOS_read(char *character);

// Forward declarations for CLI command functions
BaseType_t CLI_GetVersion(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);
BaseType_t CLI_GetTicks(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString);

/******************************************************************************
 * External Declarations
 ******************************************************************************/
extern SemaphoreHandle_t rxSemaphore; // External semaphore from SerialConsole.c

/******************************************************************************
 * Command Definitions
 ******************************************************************************/
// Version command
static const CLI_Command_Definition_t xVersionCommand =
    {
        "version",
        "version: Displays the firmware version\r\n",
        (const pdCOMMAND_LINE_CALLBACK)CLI_GetVersion,
        0};

// Ticks command
static const CLI_Command_Definition_t xTicksCommand =
    {
        "ticks",
        "ticks: Displays the number of ticks since scheduler start\r\n",
        (const pdCOMMAND_LINE_CALLBACK)CLI_GetTicks,
        0};

/******************************************************************************
 * Callback Functions
 ******************************************************************************/

/******************************************************************************
 * CLI Thread
 ******************************************************************************/

void vCommandConsoleTask(void *pvParameters)
{
    // REGISTER COMMANDS HERE

    FreeRTOS_CLIRegisterCommand(&xClearScreen);
    FreeRTOS_CLIRegisterCommand(&xResetCommand);
    FreeRTOS_CLIRegisterCommand(&xVersionCommand);  // Register version command
    FreeRTOS_CLIRegisterCommand(&xTicksCommand);    // Register ticks command

    uint8_t cRxedChar[2], cInputIndex = 0;
    BaseType_t xMoreDataToFollow;
    /* The input and output buffers are declared static to keep them off the stack. */
    static char pcOutputString[MAX_OUTPUT_LENGTH_CLI], pcInputString[MAX_INPUT_LENGTH_CLI];
    static char pcLastCommand[MAX_INPUT_LENGTH_CLI];
    static bool isEscapeCode = false;
    static char pcEscapeCodes[4];
    static uint8_t pcEscapeCodePos = 0;

    // Any semaphores/mutexes/etc you needed to be initialized, you can do them here

    /* This code assumes the peripheral being used as the console has already
    been opened and configured, and is passed into the task as the task
    parameter.  Cast the task parameter to the correct type. */

    /* Send a welcome message to the user knows they are connected. */
    SerialConsoleWriteString(pcWelcomeMessage);
    char rxChar;
    for (;;)
    {
        /* This implementation reads a single character at a time.  Wait in the
        Blocked state until a character is received. */

        FreeRTOS_read(&cRxedChar);

        if (cRxedChar[0] == '\n' || cRxedChar[0] == '\r')
        {
            /* A newline character was received, so the input command string is
            complete and can be processed.  Transmit a line separator, just to
            make the output easier to read. */
            SerialConsoleWriteString("\r\n");
            // Copy for last command
            isEscapeCode = false;
            pcEscapeCodePos = 0;
            strncpy(pcLastCommand, pcInputString, MAX_INPUT_LENGTH_CLI - 1);
            pcLastCommand[MAX_INPUT_LENGTH_CLI - 1] = 0; // Ensure null termination

            /* The command interpreter is called repeatedly until it returns
            pdFALSE.  See the "Implementing a command" documentation for an
            explanation of why this is. */
            do
            {
                /* Send the command string to the command interpreter.  Any
                output generated by the command interpreter will be placed in the
                pcOutputString buffer. */
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                    pcInputString,        /* The command string.*/
                    pcOutputString,       /* The output buffer. */
                    MAX_OUTPUT_LENGTH_CLI /* The size of the output buffer. */
                );

                /* Write the output generated by the command interpreter to the
                console. */
                // Ensure it is null terminated
                pcOutputString[MAX_OUTPUT_LENGTH_CLI - 1] = 0;
                SerialConsoleWriteString(pcOutputString);

            } while (xMoreDataToFollow != pdFALSE);

            /* All the strings generated by the input command have been sent.
            Processing of the command is complete.  Clear the input string ready
            to receive the next command. */
            cInputIndex = 0;
            memset(pcInputString, 0x00, MAX_INPUT_LENGTH_CLI);
        }
        else
        {
            /* The if() clause performs the processing after a newline character
    is received.  This else clause performs the processing if any other
    character is received. */

            if (true == isEscapeCode)
            {

                if (pcEscapeCodePos < CLI_PC_ESCAPE_CODE_SIZE)
                {
                    pcEscapeCodes[pcEscapeCodePos++] = cRxedChar[0];
                }
                else
                {
                    isEscapeCode = false;
                    pcEscapeCodePos = 0;
                }

                if (pcEscapeCodePos >= CLI_PC_MIN_ESCAPE_CODE_SIZE)
                {

                    // UP ARROW SHOW LAST COMMAND
                    if (strcasecmp(pcEscapeCodes, "oa"))
                    {
                        /// Delete current line and add prompt (">")
                        sprintf(pcInputString, "%c[2K\r>", 27);
                        SerialConsoleWriteString(pcInputString);
                        /// Clear input buffer
                        cInputIndex = 0;
                        memset(pcInputString, 0x00, MAX_INPUT_LENGTH_CLI);
                        /// Send last command
                        strncpy(pcInputString, pcLastCommand, MAX_INPUT_LENGTH_CLI - 1);
                        cInputIndex = (strlen(pcInputString) < MAX_INPUT_LENGTH_CLI - 1) ? strlen(pcLastCommand) : MAX_INPUT_LENGTH_CLI - 1;
                        SerialConsoleWriteString(pcInputString);
                    }

                    isEscapeCode = false;
                    pcEscapeCodePos = 0;
                }
            }
            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */

            else if (cRxedChar[0] == '\r')
            {
                /* Ignore carriage returns. */
            }
            else if (cRxedChar[0] == ASCII_BACKSPACE || cRxedChar[0] == ASCII_DELETE)
            {
                char erase[4] = {0x08, 0x20, 0x08, 0x00};
                SerialConsoleWriteString(erase);
                /* Backspace was pressed.  Erase the last character in the input
                buffer - if there are any. */
                if (cInputIndex > 0)
                {
                    cInputIndex--;
                    pcInputString[cInputIndex] = 0;
                }
            }
            // ESC
            else if (cRxedChar[0] == ASCII_ESC)
            {
                isEscapeCode = true; // Next characters will be code arguments
                pcEscapeCodePos = 0;
            }
            else
            {
                /* A character was entered.  It was not a new line, backspace
                or carriage return, so it is accepted as part of the input and
                placed into the input buffer.  When a n is entered the complete
                string will be passed to the command interpreter. */
                if (cInputIndex < MAX_INPUT_LENGTH_CLI)
                {
                    pcInputString[cInputIndex] = cRxedChar[0];
                    cInputIndex++;
                }

                // Order Echo
                cRxedChar[1] = 0;
                SerialConsoleWriteString(&cRxedChar[0]);
            }
        }
    }
}

/**************************************************************************/ /**
 * @fn			void FreeRTOS_read(char* character)
 * @brief		Blocks the CLI thread until a character is received from the UART
 * @details		This function waits on a semaphore that is given by the UART
 *              receive callback. Once the semaphore is available, it reads a
 *              character from the circular buffer and returns it.
 * @param[out]  character Pointer to store the received character
 * @note        This function uses a blocking semaphore, so the thread will be
 *              suspended until a character is available.
 *****************************************************************************/
static void FreeRTOS_read(char *character)
{
    uint8_t rxChar;
    
    // Wait until a character is available (semaphore given by usart_read_callback)
    xSemaphoreTake(rxSemaphore, portMAX_DELAY);
    
    // Once semaphore is obtained, get the character from the circular buffer
    if (SerialConsoleReadCharacter(&rxChar) != -1) {
        *character = (char)rxChar;
    }
    else {
        // No character available in buffer despite semaphore being given
        // This shouldn't happen, but handle it anyway
        *character = 0;
    }
}

/******************************************************************************
 * CLI Functions - Define here
 ******************************************************************************/

// THIS COMMAND USES vt100 TERMINAL COMMANDS TO CLEAR THE SCREEN ON A TERMINAL PROGRAM LIKE TERA TERM
// SEE http://www.csie.ntu.edu.tw/~r92094/c++/VT100.html for more info
// CLI SPECIFIC COMMANDS
static char bufCli[CLI_MSG_LEN];
BaseType_t xCliClearTerminalScreen(char *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    char clearScreen = ASCII_ESC;
    snprintf(bufCli, CLI_MSG_LEN - 1, "%c[2J", clearScreen);
    snprintf(pcWriteBuffer, xWriteBufferLen, bufCli);
    return pdFALSE;
}

// Example CLI Command. Resets system.
BaseType_t CLI_ResetDevice(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    system_reset();
    return pdFALSE;
}

/**************************************************************************/ /**
 * @fn			BaseType_t CLI_GetVersion(int8_t *pcWriteBuffer, size_t xWriteBufferLen, 
 *                                       const int8_t *pcCommandString)
 * @brief		Displays the firmware version
 * @details		This function is called when the user enters the "version" command
 *              in the CLI. It outputs the firmware version defined at the top of this file.
 * @param[out]  pcWriteBuffer Buffer to write the output to
 * @param[in]   xWriteBufferLen Maximum size of the output buffer
 * @param[in]   pcCommandString Command string (not used in this function)
 * @return      pdFALSE to indicate that the command is complete
 * @note        The firmware version can be changed by modifying the FIRMWARE_VERSION define.
 *****************************************************************************/
BaseType_t CLI_GetVersion(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    snprintf((char *)pcWriteBuffer, xWriteBufferLen, "Firmware Version: %s\r\n", FIRMWARE_VERSION);
    return pdFALSE;
}

/**************************************************************************/ /**
 * @fn			BaseType_t CLI_GetTicks(int8_t *pcWriteBuffer, size_t xWriteBufferLen, 
 *                                     const int8_t *pcCommandString)
 * @brief		Displays the number of ticks since the scheduler was started
 * @details		This function is called when the user enters the "ticks" command
 *              in the CLI. It outputs the current tick count from the FreeRTOS scheduler.
 * @param[out]  pcWriteBuffer Buffer to write the output to
 * @param[in]   xWriteBufferLen Maximum size of the output buffer
 * @param[in]   pcCommandString Command string (not used in this function)
 * @return      pdFALSE to indicate that the command is complete
 * @note        Uses the FreeRTOS xTaskGetTickCount() function to get the tick count.
 *****************************************************************************/
BaseType_t CLI_GetTicks(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    TickType_t currentTicks = xTaskGetTickCount();
    snprintf((char *)pcWriteBuffer, xWriteBufferLen, "Tick count: %lu\r\n", (unsigned long)currentTicks);
    return pdFALSE;
}
