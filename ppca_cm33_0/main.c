/***************************************************************************//**
* \file main.c
* \version 1.0
*
* \brief
* PPCA Core 0 application for the EPU Interrupt example. Configures an EPU
* T1 unit to detect shared-memory writes from the main CM33 core. An EPU IRQ 0
* interrupt fires on each write and updates the PWM1 compare register with the
* new duty cycle percentage (0-100) stored in the shared M4 memory location.
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
#include <stdio.h>

#define LOOP_DELAY_MS 1000

/* Shared memory addresses in M4 shared memory space (0x20040000-0x20043FFF) */
/* All cores can access M4 shared memory for inter-core communication */
#define PPCA_CPU0_M4_VAR_ADDRESS 0x20040400  /* Used by this core (CPU0) */

#define PERCENT_CONV 100

volatile uint32_t usr_inp;
uint32_t period0;
uint32_t *var = (uint32_t *)PPCA_CPU0_M4_VAR_ADDRESS;

const cy_stc_sysint_t combiner0_Interrupt_Config =
{
        .intrSrc = EPU_BLK_IRQ_EPU_0, //running on PPCA core value is 5
        .intrPriority = 2
};

/*******************************************************************************
* Function Name: combiner0_Interrupt_Handler
********************************************************************************
* Summary:
* EPU IRQ 0 interrupt handler for PPCA Core 0. Fires when the EPU combiner
* detects a write to the shared-memory duty cycle location by the main core.
* Reads the pre-converted compare value and writes it to the PWM double-buffer
* register, then triggers a compare-register swap on the next period boundary.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void combiner0_Interrupt_Handler(void)
{
    /* Clear the EPU IRQ 0 interrupt flag so it can fire again */
    Cy_PPCA_EPU_ClearInterrupt(EPU_BLK_EPU_IRQ0_HW);
    /* Stage the new compare value into the PWM double-buffer register;
       the swap happens at the next PWM period boundary */
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM1_HW, PWM1_NUM, usr_inp);

    /* Trigger compare swap with its buffer values */
    Cy_TCPWM_TriggerCaptureOrSwap_Single(PWM1_HW, PWM1_NUM);
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for PPCA Core 0. It:
*    1. Enables the EPU and configures a T1 unit to detect shared-memory writes
*    2. Routes the T1 output through combiner 0 to EPU IRQ 0
*    3. Initialises and starts PWM1 whose compare register is updated by the ISR
*    4. Continuously converts the user-supplied percentage to a raw PWM count
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{

     /* Enable global interrupts so the EPU interrupt can be serviced */
     __enable_irq();

     /* --- EPU Configuration --- */
     /* Grant this PPCA core exclusive access to the EPU block */
     Cy_PPCA_EPU_EnableExclusiveAccess(EPU_BLK_HW, true);
     /* Enable the EPU block */
     Cy_PPCA_EPU_Enable(EPU_BLK_HW);

     /* Configure T1 processing unit 0 to detect the shared-memory write
        event that the main core generates when a new duty cycle is typed */
     Cy_PPCA_EPU_PU_T1_Configure(put1_0_HW, put1_0_INDEX, &put1_0_put1_config);
     /* Enable the PU Type 1 */
     Cy_PPCA_EPU_PU_T1_Enable(put1_0_HW, put1_0_INDEX, put1_0_ENABLE_MODE);
     /* Wire the T1 output through combiner 0 to EPU IRQ 0 */
     Cy_PPCA_EPU_Combo_Configure(combiner0_HW, combiner0_INDEX, &combiner0_combo_config);

     /* Register and enable the EPU interrupt handler */
     Cy_SysInt_Init(&combiner0_Interrupt_Config, &combiner0_Interrupt_Handler);
     NVIC_EnableIRQ((IRQn_Type) combiner0_Interrupt_Config.intrSrc);

     /* Select the interrupt source and unmask it in the EPU */
     Cy_PPCA_EPU_InterruptSourceSelect(EPU_BLK_EPU_IRQ0_HW, false, epuIrqSrc0);
     Cy_PPCA_EPU_SetInterruptMask(EPU_BLK_EPU_IRQ0_HW);

     /* Initialise and start the PWM; its compare value will be updated
        dynamically by the EPU interrupt handler each time the user changes
        the duty cycle via the main core console */
     Cy_TCPWM_PWM_Init(PWM1_HW, PWM1_NUM, &PWM1_config);
     Cy_TCPWM_PWM_Enable(PWM1_HW, PWM1_NUM);
     Cy_TCPWM_TriggerStart_Single(PWM1_HW, PWM1_NUM);

     for(;;)
     {
          /* Read the current PWM period register value */
          period0 = Cy_TCPWM_PWM_GetPeriod0(PWM1_HW, PWM1_NUM);
          /* Convert the user-supplied percentage (0-100) to a raw compare
             count based on the current period: count = (period / 100) * % */
          usr_inp = (period0/PERCENT_CONV) * (*var);
          Cy_SysLib_Delay(LOOP_DELAY_MS);
     }
}

