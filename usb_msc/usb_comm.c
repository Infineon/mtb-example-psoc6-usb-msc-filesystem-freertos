/*****************************************************************************
* File Name: usb_comm.c
*
* Description:
*  This file provides the source code to implement the USB Mass Storage
*  class requests.
*
* Note:
*
******************************************************************************
* Copyright (2020), Cypress Semiconductor Corporation.
******************************************************************************
* This software is owned by Cypress Semiconductor Corporation (Cypress) and is
* protected by and subject to worldwide patent protection (United States and
* foreign), United States copyright laws and international treaty provisions.
* Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the
* Cypress Source Code and derivative works for the sole purpose of creating
* custom software in support of licensee product to be used only in conjunction
* with a Cypress integrated circuit as specified in the applicable agreement.
* Any reproduction, modification, translation, compilation, or representation of
* this software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes without further notice to the
* materials described herein. Cypress does not assume any liability arising out
* of the application or use of any product or circuit described herein. Cypress
* does not authorize its products for use as critical components in life-support
* systems where a malfunction or failure may reasonably be expected to result in
* significant injury to the user. The inclusion of Cypress' product in a life-
* support systems application implies that the manufacturer assumes all risk of
* such use and in doing so indemnifies Cypress against all charges. Use may be
* limited by and subject to the applicable Cypress software license agreement.
*****************************************************************************/
#include <stdio.h>
#include "usb_comm.h"
#include "usb_scsi.h"

#include "cy_sysint.h"
#include "cyhal.h"
#include "cycfg.h"
#include "cycfg_usbdev.h"

#include "sd_card.h"

#include "FreeRTOS.h"
#include "task.h"

/*******************************************************************************
* Constants
*******************************************************************************/
#define USB_COMM_DEVICE_ID          0
#define USB_COMM_SUSPEND_COUNT      3
#define USB_COMM_CBW_FLAG_DIR_IN    0x80
#define USB_COMM_CBS_PHASE_ERROR    0x02
#define USB_COMM_TIMEOUT            2000

/***************************************************************************
* USB Interrupt Handlers
***************************************************************************/
static void usb_high_isr(void);
static void usb_medium_isr(void);
static void usb_low_isr(void);
void usb_timer_handler(void *arg, cyhal_timer_event_t event);

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* USB Interrupt Configuration */
const cy_stc_sysint_t usb_high_interrupt_cfg =
{
    .intrSrc = (IRQn_Type) usb_interrupt_hi_IRQn,
    .intrPriority = 5U,
};
const cy_stc_sysint_t usb_medium_interrupt_cfg =
{
    .intrSrc = (IRQn_Type) usb_interrupt_med_IRQn,
    .intrPriority = 6U,
};
const cy_stc_sysint_t usb_low_interrupt_cfg =
{
    .intrSrc = (IRQn_Type) usb_interrupt_lo_IRQn,
    .intrPriority = 7U,
};

/* USBDEV context variables */
cy_stc_usbfs_dev_drv_context_t  usb_drvContext;
cy_stc_usb_dev_context_t        usb_devContext;
cy_stc_usb_dev_msc_context_t    usb_mscContext;

/* USB MSC specific variables */
uint8_t msc_lun = 0;
uint8_t msc_reset = 0;

/* USB Timer variables */
cyhal_timer_t usb_timer;
cyhal_timer_cfg_t usb_timer_cfg =
{
    .is_continuous = true,
    .period        = 10000
};

/* Mass storage device interfaces */
cy_stc_mass_storage_dev_t disk_fops = {
  sd_card_is_connected,
  sd_card_init,
  sd_card_sector_size,
  sd_card_max_sector_num,
  sd_card_total_mem_bytes,
  sd_card_read,
  sd_card_write,
};

volatile bool usb_suspended = false;
volatile uint32_t usb_idle_counter = 0;

uint8_t *usb_fs = NULL;

extern uint8_t forceOS;
extern uint8_t statusFileTimer;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
static void usb_comm_msc_out_ep_cb(USBFS_Type *base, uint32_t endpointAddr, uint32_t errorType, struct cy_stc_usbfs_dev_drv_context *context);
static void usb_comm_msc_in_ep_cb(USBFS_Type *base, uint32_t endpointAddr, uint32_t errorType, struct cy_stc_usbfs_dev_drv_context *context);
static cy_en_usb_dev_status_t usb_msc_request_received (cy_stc_usb_dev_control_transfer_t *transfer, void *classContext, cy_stc_usb_dev_context_t *devContext);
static cy_en_usb_dev_status_t usb_msc_request_completed(cy_stc_usb_dev_control_transfer_t *transfer, void *classContext, cy_stc_usb_dev_context_t *devContext);
static uint8 is_command_block_wrapper_valid(cy_stc_usb_dev_msc_context_t *context);
static void usb_high_isr(void);
static void usb_medium_isr(void);
static void usb_low_isr(void);
void usb_timer_handler(void *arg, cyhal_timer_event_t event);

/*******************************************************************************
* Function Name: usb_comm_timeout_handler
********************************************************************************
* Summary:
*   USB timeout handler based on FreeRTOS.
*
*******************************************************************************/
static int32_t usb_comm_timeout_handler(int32_t ms)  
{  
     vTaskDelay(1/portTICK_PERIOD_MS);  
     return --ms;  
} 

/*******************************************************************************
* Function Name: usb_comm_init
********************************************************************************
* Summary:
*   Initializes the USBFS hardware block and its interrupts.
*
*******************************************************************************/
void usb_comm_init(void)
{
    /* Init the USB Block */
    Cy_USB_Dev_Init(CYBSP_USBDEV_HW,
                    &CYBSP_USBDEV_config,
                    &usb_drvContext,
                    &usb_devices[USB_COMM_DEVICE_ID],
                    &usb_devConfig,
                    &usb_devContext);

    /* Init the Mass Storage Device Class */
    Cy_USB_Dev_Msc_Init(NULL,
                        &usb_mscContext,
                        &usb_devContext);
                        
    /* Add Storage callbacks for Mass Storage Device Class */
    usb_mscContext.p_user_data = &disk_fops;
    usb_mscContext.block_size = ((cy_stc_mass_storage_dev_t *)usb_mscContext.p_user_data)->get_block_size();
    usb_mscContext.block_num = ((cy_stc_mass_storage_dev_t *)usb_mscContext.p_user_data)->get_block_num();
    usb_mscContext.mem_size = ((cy_stc_mass_storage_dev_t *)usb_mscContext.p_user_data)->get_mem_size();

    /* Register MSC data endpoint callbacks */
    Cy_USBFS_Dev_Drv_RegisterEndpointCallback(CYBSP_USBDEV_HW, MSC_IN_ENDPOINT, usb_comm_msc_in_ep_cb, &usb_drvContext);
    Cy_USBFS_Dev_Drv_RegisterEndpointCallback(CYBSP_USBDEV_HW, MSC_OUT_ENDPOINT, usb_comm_msc_out_ep_cb, &usb_drvContext);

    /* Register Mass Storage Callbacks */
    Cy_USB_Dev_Msc_RegisterUserCallback(usb_msc_request_received, usb_msc_request_completed, &usb_mscContext);

    /* Initialize the USB interrupts */
    Cy_SysInt_Init(&usb_high_interrupt_cfg,   &usb_high_isr);
    Cy_SysInt_Init(&usb_medium_interrupt_cfg, &usb_medium_isr);
    Cy_SysInt_Init(&usb_low_interrupt_cfg,    &usb_low_isr);

    /* Init the timer to detect USB activity */
    cyhal_timer_init(&usb_timer, NC, NULL);
    cyhal_timer_configure(&usb_timer, &usb_timer_cfg);
    cyhal_timer_register_callback(&usb_timer, usb_timer_handler, NULL);
    cyhal_timer_enable_event(&usb_timer, CYHAL_TIMER_IRQ_TERMINAL_COUNT, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Overwrite the timeout handler */
    Cy_USB_Dev_OverwriteHandleTimeout(usb_comm_timeout_handler, &usb_devContext);
}

/*******************************************************************************
* Function Name: usb_comm_connect
********************************************************************************
* Summary:
*   Starts USB enumeration.
*
*******************************************************************************/
void usb_comm_connect(void)
{
    /* Enable the USB interrupts */
    NVIC_EnableIRQ(usb_high_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_medium_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_low_interrupt_cfg.intrSrc);

    /* Make device appear on the bus. This function call is blocking,
       it waits till the device enumerates */
    Cy_USB_Dev_Connect(true, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);

    /* Start the internal timer to check for activity */
    cyhal_timer_start(&usb_timer);

    /* Enable the OUT Endpoint */
    Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
}

/*******************************************************************************
* Function Name: usb_comm_is_ready
********************************************************************************
* Summary:
*   Verifies if the USB is enumerated.
*
*******************************************************************************/
bool usb_comm_is_ready(void)
{
    return (Cy_USB_Dev_GetConfiguration(&usb_devContext));
}

/*******************************************************************************
* Function Name: usb_comm_refresh
********************************************************************************
* Summary:
*   Force the OS to reenumerate.
*
*******************************************************************************/
void usb_comm_refresh(void)
{
    forceOS = true;
    statusFileTimer = 0;
}

/*******************************************************************************
* Function Name: usb_comm_process
********************************************************************************
* Summary:
*   Process any pending requests from USB.
*
*******************************************************************************/
void usb_comm_process(void)
{
    /* Storage device Status */
    if(!((cy_stc_mass_storage_dev_t *)usb_mscContext.p_user_data)->is_connected()) 
    {
        storageRemovedFlag = true;
    } else 
    {
        storageRemovedFlag = false;
    }

    /* Check the USB configuration */
    if(Cy_USB_Dev_IsConfigurationChanged(&usb_devContext)) 
    {
        usb_mscContext.state = CY_USB_DEV_MSC_READY_STATE;
        /* Enable the OUT Endpoint */
        Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, &usb_devContext);
    }
    
}

/*******************************************************************************
* Function Name: usb_comm_msc_out_ep_cb
********************************************************************************
* Summary:
*   This is the MSC OUT endpoint handler callback.
*
*******************************************************************************/
static void usb_comm_msc_out_ep_cb(USBFS_Type *base, uint32_t endpointAddr, uint32_t errorType, struct cy_stc_usbfs_dev_drv_context *context)
{
    uint32_t actCount = 0;
    uint32_t tempVar = 0;
    cy_en_usb_dev_status_t status = CY_USB_DEV_REQUEST_NOT_HANDLED;

    if(endpointAddr != MSC_OUT_ENDPOINT) {
        return;
    }
     /* Read the data from the OUT endpoint */
    if(CY_USB_DEV_SUCCESS != Cy_USB_Dev_ReadEpNonBlocking(MSC_OUT_ENDPOINT, usb_mscContext.out_buffer, 
                                CY_USB_DEV_MSC_EP_BUF_SIZE, &actCount, context->devConext)) {
        return;
    }
    /* Command Block Wrapper (CBW) */
    if(CY_USB_DEV_MSC_READY_STATE == usb_mscContext.state) {
        if(actCount != CY_USB_DEV_MSC_CMD_BLOCK_SIZE) {
            /* Stall OUT endpoint */
            Cy_USBFS_Dev_Drv_StallEndpoint(base, MSC_OUT_ENDPOINT, context);
            return;
        }
        /* Check the CBW data  */
        if(CY_USB_DEV_SUCCESS != is_command_block_wrapper_valid(&usb_mscContext)) {
            Cy_USBFS_Dev_Drv_StallEndpoint(base, MSC_OUT_ENDPOINT, context);
            Cy_USBFS_Dev_Drv_StallEndpoint(base, MSC_IN_ENDPOINT, context);
            return;
        }
        usb_mscContext.cmd_status.tag = usb_mscContext.cmd_block.tag;
        usb_mscContext.cmd_status.data_residue = usb_mscContext.cmd_block.data_transfer_length;

        /* The number of bytes of transfer data is 0 */
        if(usb_mscContext.cmd_block.data_transfer_length == 0) {
            switch (usb_mscContext.cmd_block.cmd[0]) {
                case CY_USB_DEV_MSC_SCSI_TEST_UNIT_READY:
                    status = usb_scsi_test_unit_ready();
                    break;
                case CY_USB_DEV_MSC_SCSI_MEDIA_REMOVAL:
                    status = usb_scsi_prevent_media_removal(usb_mscContext.cmd_block.cmd[4]);
                    break;
                case CY_USB_DEV_MSC_SCSI_START_STOP_UNIT:
                    status = usb_scsi_start_stop_unit(usb_mscContext.cmd_block.cmd[4]);
                    break;
                default:
                    break;
            }
            usb_mscContext.cmd_status.status = status;
            /* Send CSW */
            if(CY_USB_DEV_SUCCESS == Cy_USB_Dev_WriteEpNonBlocking(MSC_IN_ENDPOINT, (const uint8_t *)&(usb_mscContext.cmd_status), CY_USB_DEV_MSC_CMD_STATUS_SIZE, context->devConext)) {
                usb_mscContext.state = CY_USB_DEV_MSC_STATUS_TRANSPORT;
            }
            Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, context->devConext);
            return;
        }

        /*  Start a reading on OUT endpoint */
        Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, context->devConext);

        /* Data-In from the device to the host */
        if ((usb_mscContext.cmd_block.flags & USB_COMM_CBW_FLAG_DIR_IN) == USB_COMM_CBW_FLAG_DIR_IN) {
            switch (usb_mscContext.cmd_block.cmd[0]) {
                /* SCSI Read command (10) */
                case CY_USB_DEV_MSC_SCSI_READ10:
                    tempVar = ((usb_mscContext.cmd_block.cmd[2] << 24) | (usb_mscContext.cmd_block.cmd[3] << 16) | (usb_mscContext.cmd_block.cmd[4] << 8) | (usb_mscContext.cmd_block.cmd[5]));
                    usb_mscContext.start_location = tempVar * MSC_BLOCKSIZE;
                    tempVar = ((usb_mscContext.cmd_block.cmd[7] << 8) | (usb_mscContext.cmd_block.cmd[8]));
                    usb_mscContext.bytes_to_transfer = tempVar * MSC_BLOCKSIZE;
                    usb_mscContext.dev_data_len = 0;
                    if (usb_mscContext.cmd_block.data_transfer_length != usb_mscContext.bytes_to_transfer) {
                        usb_mscContext.cmd_status.status = USB_COMM_CBS_PHASE_ERROR;
                        /* Stall OUT endpoint */
                        Cy_USBFS_Dev_Drv_StallEndpoint(base, MSC_OUT_ENDPOINT, context);
                        usb_mscContext.state = CY_USB_DEV_MSC_READY_STATE;
                        return;
                    }
                    status = usb_scsi_read_10(&usb_mscContext);
                    break;
                case CY_USB_DEV_MSC_SCSI_REQUEST_SENSE:
                    status = usb_scsi_request_sense(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_INQUIRY:
                    status = usb_scsi_inquiry(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SENSE6:
                    status = usb_scsi_mode_sense_6(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SENSE10:
                    status = usb_scsi_mode_sense_10(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_READ_CAPACITY:
                    status = usb_scsi_read_capacity(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_READ_FORMAT_CAPACITIES:
                    status = usb_scsi_read_format_capacities(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_FORMAT_UNIT:
                    status = usb_scsi_format_unit(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SELECT6:
                    status = usb_scsi_mode_select_6(&usb_mscContext);
                    break;

                case CY_USB_DEV_MSC_SCSI_MODE_SELECT10:
                    status = usb_scsi_mode_select_10(&usb_mscContext);
                    break;

                default:
                    usb_mscContext.state = CY_USB_DEV_MSC_READY_STATE;
                    break;
            }
            if (status == CY_USB_DEV_SUCCESS) {
                /* Send command data */
                if(CY_USB_DEV_SUCCESS == Cy_USB_Dev_WriteEpNonBlocking(MSC_IN_ENDPOINT, usb_mscContext.in_buffer, usb_mscContext.packet_in_size, context->devConext)) {
                    usb_mscContext.state = CY_USB_DEV_MSC_DATA_IN;
                }
                usb_mscContext.cmd_status.status = status;
            }
        /* Data-Out from host to the device */
        } else {
            /* SCSI Write (10) or Verify (10) */
            if((usb_mscContext.cmd_block.cmd[0] == CY_USB_DEV_MSC_SCSI_WRITE10) || (usb_mscContext.cmd_block.cmd[0] == CY_USB_DEV_MSC_SCSI_VERIFY10)) {
                /* Get the block address */
                tempVar = ((usb_mscContext.cmd_block.cmd[2] << 24) | (usb_mscContext.cmd_block.cmd[3] << 16) | (usb_mscContext.cmd_block.cmd[4] << 8) | (usb_mscContext.cmd_block.cmd[5]));
                usb_mscContext.start_location = tempVar * MSC_BLOCKSIZE;
                /* Get the block num */
                tempVar = ((usb_mscContext.cmd_block.cmd[7] << 8) | (usb_mscContext.cmd_block.cmd[8]));
                usb_mscContext.bytes_to_transfer = tempVar * MSC_BLOCKSIZE;
                usb_mscContext.dev_data_len = 0;
                /* Check the transfer length */
                if(usb_mscContext.cmd_block.data_transfer_length != usb_mscContext.bytes_to_transfer) {
                    usb_mscContext.cmd_status.status = USB_COMM_CBS_PHASE_ERROR;
                    /* Stall OUT endpoint */
                    Cy_USBFS_Dev_Drv_StallEndpoint(base, MSC_OUT_ENDPOINT, context);
                    return;
                }
                usb_mscContext.state = CY_USB_DEV_MSC_DATA_OUT;
            }
        }
    /* Data OUT transfer */
    } else if(CY_USB_DEV_MSC_DATA_OUT == usb_mscContext.state) {
        usb_mscContext.packet_out_size = actCount;
        if(CY_USB_DEV_MSC_SCSI_WRITE10 == usb_mscContext.cmd_block.cmd[0]) {
            usb_scsi_write_10(&usb_mscContext);
        } else if(CY_USB_DEV_MSC_SCSI_VERIFY10 == usb_mscContext.cmd_block.cmd[0]) {
            usb_scsi_verify_10(&usb_mscContext);
        }
        /* Transfer completed, Send CSW */
        if (usb_mscContext.state == CY_USB_DEV_MSC_STATUS_TRANSPORT) {
            if(CY_USB_DEV_SUCCESS == Cy_USB_Dev_WriteEpNonBlocking(MSC_IN_ENDPOINT, (const uint8_t *)&(usb_mscContext.cmd_status), CY_USB_DEV_MSC_CMD_STATUS_SIZE, context->devConext)) {
                usb_mscContext.state = CY_USB_DEV_MSC_STATUS_TRANSPORT;
            }
        }
        Cy_USB_Dev_StartReadEp(MSC_OUT_ENDPOINT, context->devConext);
    }
}

/*******************************************************************************
* Function Name: usb_comm_msc_in_ep_cb
********************************************************************************
* Summary:
*   The MSC IN endpoint data handler callback.
*
*******************************************************************************/
static void usb_comm_msc_in_ep_cb(USBFS_Type *base, uint32_t endpointAddr, uint32_t errorType, struct cy_stc_usbfs_dev_drv_context *context)
{
    cy_en_usb_dev_status_t status = CY_USB_DEV_REQUEST_NOT_HANDLED;

    if(MSC_IN_ENDPOINT != (endpointAddr&0x7F)) {
        return;
    }

    /* Send the CSW completed */
    if(CY_USB_DEV_MSC_STATUS_TRANSPORT == usb_mscContext.state) 
    {
        usb_mscContext.state = CY_USB_DEV_MSC_READY_STATE;
    /* Send the data completed */
    } else if(CY_USB_DEV_MSC_DATA_IN == usb_mscContext.state) {
        if(CY_USB_DEV_MSC_SCSI_READ10 != usb_mscContext.cmd_block.cmd[0]) {
            usb_mscContext.cmd_status.data_residue -= usb_mscContext.packet_in_size;
            /* Send CSW */
            if(CY_USB_DEV_SUCCESS == Cy_USB_Dev_WriteEpNonBlocking(MSC_IN_ENDPOINT, (const uint8_t *)&(usb_mscContext.cmd_status), 
                                                                   CY_USB_DEV_MSC_CMD_STATUS_SIZE, context->devConext)) {
                usb_mscContext.state = CY_USB_DEV_MSC_STATUS_TRANSPORT;
            }
        } else {
            usb_mscContext.cmd_status.data_residue -= usb_mscContext.packet_in_size;
            usb_mscContext.start_location += usb_mscContext.packet_in_size;
            usb_mscContext.bytes_to_transfer -= usb_mscContext.packet_in_size;
            if (usb_mscContext.bytes_to_transfer != 0) {
                status = usb_scsi_read_10(&usb_mscContext);
                if (status == CY_USB_DEV_SUCCESS) {
                    /* Send command data */
                    if(CY_USB_DEV_SUCCESS == Cy_USB_Dev_WriteEpNonBlocking(MSC_IN_ENDPOINT, usb_mscContext.in_buffer, 
                                                                usb_mscContext.packet_in_size, context->devConext)) {
                        //TODO...
                    }
                }
                usb_mscContext.cmd_status.status = status;
            } else {
                /* All IN data send completed, send CSW */
                if(CY_USB_DEV_SUCCESS == Cy_USB_Dev_WriteEpNonBlocking(MSC_IN_ENDPOINT, (const uint8_t *)&(usb_mscContext.cmd_status), 
                                                                       CY_USB_DEV_MSC_CMD_STATUS_SIZE, context->devConext)) {
                    usb_mscContext.state = CY_USB_DEV_MSC_STATUS_TRANSPORT;
                }
            }
        }
    }
}

/*******************************************************************************
* Function Name: is_command_block_wrapper_valid
********************************************************************************
* Summary:
*   Check for validity of the Command Block Wrapper (CBW).
*
* Parameters:
*   context: USB MSC context
*
* Return:
*   Success if valid.
* 
*******************************************************************************/
static uint8 is_command_block_wrapper_valid(cy_stc_usb_dev_msc_context_t *context)
{
    cy_en_usb_dev_status_t validStatus = CY_USB_DEV_BAD_PARAM;
    uint8 index = 0;

    for(index = 0; index < CY_USB_DEV_MSC_CMD_BLOCK_SIZE; index++)
    {
        /* Copy all contents from EP buffer to structure. May replace with DMA,
         * need to validate if efficiency can be increased in that way. */
        *((uint8 *)&context->cmd_block + index) = context->out_buffer[index];
    }

    if(context->cmd_block.signature == MSC_CBW_SIGNATURE)
    {
        if(context->cmd_block.lun <= 0)
        {
            if((context->cmd_block.length > 0) && (context->cmd_block.length <= CY_USB_DEV_MSC_CMD_SIZE))
            {
                /* Validate all conditions of section 6.6.1 of MSC spec. */
                validStatus = CY_USB_DEV_SUCCESS;
            }
        }
    }
    return(validStatus);
}

/*******************************************************************************
* Function Name: usb_msc_request_received
********************************************************************************
* Summary:
*   Callback implementation for the MSC Request Received.
*
* Parameters:
*   transfer: contain information about the transfer
*   classContext: pointer to the class context
*   devContext: USB device context
*
* Return:
*   Success if supported request received.
*
*******************************************************************************/
static cy_en_usb_dev_status_t usb_msc_request_received(cy_stc_usb_dev_control_transfer_t *transfer, void *classContext, cy_stc_usb_dev_context_t *devContext)
{
    cy_en_usb_dev_status_t retStatus = CY_USB_DEV_REQUEST_NOT_HANDLED;

    if (transfer->setup.bmRequestType.type == CY_USB_DEV_CLASS_TYPE)
    {
        switch (transfer->setup.bRequest)
        {
            case CY_USB_DEV_MSC_GET_MAX_LUN:
                transfer->remaining = 0x01;
                transfer->ptr = &msc_lun;
                retStatus = CY_USB_DEV_SUCCESS;
                break;
            case CY_USB_DEV_MSC_RESET:
                transfer->notify = true;
                transfer->ptr = &msc_reset;
                transfer->remaining = 0x01;
                retStatus = CY_USB_DEV_SUCCESS;
                break;
            default:
                break;
        }
    }

    return retStatus;
}


/*******************************************************************************
* Function Name: usb_msc_request_completed
********************************************************************************
* Summary:
*   Callback implementation for MSC Msc Request Completed. Not used in this
*   example.
*
* Parameters:
*   transfer: contain information about the transfer
*   classContext: pointer to the class context
*   devContext: USB device context
*
* Return:
*   Not handled. 
*
*******************************************************************************/
static cy_en_usb_dev_status_t usb_msc_request_completed(cy_stc_usb_dev_control_transfer_t *transfer,
                                                         void *classContext,
                                                         cy_stc_usb_dev_context_t *devContext)
{
    cy_en_usb_dev_status_t retStatus = CY_USB_DEV_REQUEST_NOT_HANDLED;

    return retStatus;
}

/*******************************************************************************
* Function Name: usb_timer_handler
********************************************************************************
* Summary:
*   Internal interrupt handler for the USB.
*
* Parameters:
*   arg: not used
*   event: not used
*
***************************************************************************/
void usb_timer_handler(void *arg, cyhal_timer_event_t event)
{
    usb_scsi_serve_timeout();

    if (0u != Cy_USBFS_Dev_Drv_CheckActivity(CYBSP_USBDEV_HW))
    {
        usb_idle_counter = 0;
    }
    else
    {
        /* Check for suspend condition on USB */
        if (usb_idle_counter < USB_COMM_SUSPEND_COUNT)
        {
            /* Counter idle time before detect suspend condition */
            usb_idle_counter++;
        }
        else
        {
            usb_suspended = true;
        }
    }
}

/***************************************************************************
* Function Name: usb_high_isr
********************************************************************************
* Summary:
*  This function process the high priority USB interrupts.
*
***************************************************************************/
static void usb_high_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USBDEV_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseHi(CYBSP_USBDEV_HW),
                               &usb_drvContext);
}


/***************************************************************************
* Function Name: usb_medium_isr
********************************************************************************
* Summary:
*  This function process the medium priority USB interrupts.
*
***************************************************************************/
static void usb_medium_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USBDEV_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseMed(CYBSP_USBDEV_HW),
                               &usb_drvContext);
}


/***************************************************************************
* Function Name: usb_low_isr
********************************************************************************
* Summary:
*  This function process the low priority USB interrupts.
*
**************************************************************************/
static void usb_low_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USBDEV_HW,
                               Cy_USBFS_Dev_Drv_GetInterruptCauseLo(CYBSP_USBDEV_HW),
                               &usb_drvContext);
}
