/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \brief
* Main core (CM33 Secure) application for the EPU Interrupt example.
* The main core initialises the UART console, configures the PPCA signal
* routing, boots PPCA Core 0, and then accepts duty-cycle values (0-100)
* typed by the user.  Values are written to a shared-memory location that
* PPCA Core 0 reads to update the PWM compare register via an EPU interrupt.
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/
#include "cy_pdl.h"
#include "cycfg.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* Debug UART variables */
static cy_stc_scb_uart_context_t    DEBUG_UART_context; /* UART context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj; /* Debug UART HAL object */

/* These are the addresses where the core0 and core1 images are located. */
#define CORE0_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca0_nvm_C_S_START    //0x12030000
#define CORE1_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca1_nvm_C_S_START    //0x12038000
#define PPCA0_IMAGE_SIZE       CYMEM_CM33_0_S_ppca0_code_SIZE
#define PPCA1_IMAGE_SIZE       CYMEM_CM33_0_S_ppca1_code_SIZE

/* Shared memory addresses for inter-core communication */
/* PPCA cores write to these locations, main core reads from them */
/* Variables located in M4 shared memory space (16KB at 0x20040000-0x20043FFF from PPCA view) */
/* Main core accesses PPCA memory through PPCA peripheral base with memory windows: */
/* M1 (CPU0 data): 0x53020000, M3 (CPU1 data): 0x53040000, M4 (shared): 0x53050000 */
#define PPCA_CPU0_M4_VAR_ADDRESS   0x53050400  /* Written by PPCA Core 0 */



/* Variable for reading strings from user. */
char read_string[128] = {0};
int32_t read_status = 0;
int32_t read_value = 0;

/* Pointer to the M4 shared memory location used to pass the duty cycle (0-100)
   to PPCA Core 0; the EPU inside Core 0 detects writes to this address */
uint32_t *ppca_core0_var = (uint32_t *)PPCA_CPU0_M4_VAR_ADDRESS;

int main(void)
{
    cy_rslt_t result;
    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize and enable the debug UART peripheral */
    Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);

    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config,
                                &DEBUG_UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* retarget IO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable the PPCA configuration block and connect EPU output to pin */
    Cy_PPCA_Enable(CNFG_PPCA_INOUT_HW);
    Cy_PPCA_CNFG_PPCA_Output_Selector(CNFG_PPCA_INOUTCNFG_HW, &CNFG_PPCA_INOUT_ppcaOutConfig);

    /* Write the initial duty cycle (50%) into shared memory so PPCA Core 0
       can read it as soon as it boots and sets up the PWM */
    *ppca_core0_var = 50;

    /* Clear terminal screen and print application banner */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: EPU interrupt\r\n");
    printf("************************************************************\r\n\n");

    /* Enable global interrupts before booting PPCA core */
    __enable_irq();

    /* Boot PPCA Core 0 – it will configure the EPU interrupt and PWM */
    Cy_System_Init_CPU0((void*)CORE0_IMAGE_ADDRESS, PPCA0_IMAGE_SIZE);
    Cy_System_Init_CPU1((void*)CORE1_IMAGE_ADDRESS, PPCA1_IMAGE_SIZE);

    printf("PPCA CM33 cores Boot called\r\n");
    printf("Give a value from 0 to 100 and press 'Enter'\r\n\n");

    for (;;)
    {
        /* Block until the user types a value and presses Enter */
        read_status = scanf("%120s", &read_string[0]);
        if(0<read_status)
        {
             /* Convert the string input to an integer */
             read_value = atoi((const char *)read_string);
             if(read_value >= 0 && read_value <= 100)
             {
                 printf("\033[2K\r");  /* Erase the current terminal line */
                 printf("Your Duty Cycle is = %d \r\n", (int)read_value);
                 printf("\033[2K\r");  /* Erase the current terminal line */
                 printf("%d\r\n", (int)read_value);
                 printf("\033[A");
                 printf("\033[A");
                 /* Write the new duty cycle to shared memory;
                    the EPU interrupt on PPCA Core 0 will pick it up and
                    update the PWM compare register */
                 *ppca_core0_var = read_value;
             }
             else
             {
                 printf("\033[2K\r");  /* Erase the current terminal line */
                 printf("Please enter duty cycle between 0 and 100\r\n");
                 printf("\033[2K\r");  /* Erase the current terminal line */
                 printf("%d\r\n", (int)read_value);
                 printf("\033[A");
                 printf("\033[A");
             }
        }
        else
        {
            /* scanf returned no valid items – prompt the user again */
            printf("\033[2K\r");  /* Erase the current terminal line */
            printf("Give duty cycle from 0 to 100\r\n");
            printf("\033[A");
            printf("\033[A");
        }
    }
}

