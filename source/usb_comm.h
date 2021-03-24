/*****************************************************************************
* File Name: usb_comm.h
*
* Description:
*  This file contains the function prototypes and constants used in
*  the usb_comm.c.
*
* Note:
*
******************************************************************************
* Copyright 2020-2021, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*****************************************************************************/

#ifndef USB_COMM_H_
#define USB_COMM_H_

#include "cy_usb_dev.h"
#include "cy_usb_dev_msc.h"

/*******************************************************************************
* Constants
********************************************************************************/
/* MSC OUT and IN endpoint address */
#define MSC_OUT_ENDPOINT_ADDR   0x02
#define MSC_IN_ENDPOINT_ADDR    0x81
#define MSC_OUT_ENDPOINT        0x02
#define MSC_IN_ENDPOINT         0x01

/*******************************************************************************
* USB Communication Functions
*******************************************************************************/
void     usb_comm_init(void);
void     usb_comm_connect(void);
void     usb_comm_link_fs(uint8_t *fs);
bool     usb_comm_is_ready(void);
void     usb_comm_refresh(void);
void     usb_comm_process(void);


#endif /* USB_COMM_H_ */

/* [] END OF FILE */
