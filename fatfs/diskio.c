/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various existing       */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"            /* Obtains integer types */
#include "diskio.h"        /* Declarations of disk functions */
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

/* Definitions of physical drive number for each drive */
#define DEV_SD        0    /* Example: Map SD to physical drive 0 */

/* Pins connected to the SDHC block */
#define cmd                 P12_4
#define clk                 P12_5
#define dat0                P13_0
#define dat1                P13_1
#define dat2                P13_2
#define dat3                P13_3
/* dat4 to dat7 are reserved for future use and should be NC */
#define dat4                NC
#define dat5                NC
#define dat6                NC
#define dat7                NC
#define card_detect         NC
#define emmc_reset          NC
#define io_volt_sel         NC
#define card_if_pwren       NC
#define card_mech_writeprot NC
#define led_ctl             NC
#define custom_card_detect  P13_5

bool SD_INIT = false;

cyhal_sdhc_t sdhc_obj;

__USED bool Cy_SD_Host_IsCardConnected(SDHC_Type const *base)
{
    /* P13_5 reads 0 when card detected, 1 when card not detected */
    return cyhal_gpio_read(custom_card_detect) ? false : true;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
    BYTE pdrv        /* Physical drive number to identify the drive */
)
{
    DSTATUS stat = 0;

    switch (pdrv) {
        case DEV_SD :

            if (false == Cy_SD_Host_IsCardConnected(NULL))
            {
                stat |= STA_NODISK;
            }

            if(false == SD_INIT)
            {
                stat |= STA_NOINIT;
            }

            return stat;
    }

    return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    BYTE pdrv                /* Physical drive number to identify the drive */
)
{
    DSTATUS stat = RES_OK;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    const cyhal_sdhc_config_t sdhc_config = {
        .enableLedControl = false,
        .lowVoltageSignaling = false,
        .isEmmc   = false,
        .busWidth = 4,
    };

    switch (pdrv) {
    case DEV_SD :

        if (SD_INIT == false)
        {
            /* Initialize the custom Card Detect pin */
            result |= cyhal_gpio_init(custom_card_detect, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, true);

            /* Initialize the SD Card interface */
            result |= cyhal_sdhc_init(&sdhc_obj, &sdhc_config, cmd, clk,
                                     dat0, dat1, dat2, dat3, dat4, dat5, dat6, dat7,
                                     card_detect, io_volt_sel,
                                     card_if_pwren,
                                     card_mech_writeprot, led_ctl, emmc_reset);

            if (result != CY_RSLT_SUCCESS)
            {
                stat |= STA_NOINIT;
                return stat;
            }
        }

        SD_INIT = true;

        return stat;
    }
    return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
    BYTE pdrv,        /* Physical drive number to identify the drive */
    BYTE *buff,        /* Data buffer to store read data */
    LBA_t sector,    /* Start sector in LBA */
    UINT count        /* Number of sectors to read */
)
{
    DRESULT res;
    cy_rslt_t result;

    switch (pdrv) {
        case DEV_SD :
            {
                result = cyhal_sdhc_read(&sdhc_obj, sector, buff, &count);
                res = (result == CY_RSLT_SUCCESS) ? RES_OK : RES_ERROR;
                return res;
            }
    }

    return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
    BYTE pdrv,            /* Physical drive number to identify the drive */
    const BYTE *buff,    /* Data to be written */
    LBA_t sector,        /* Start sector in LBA */
    UINT count            /* Number of sectors to write */
)
{
    DRESULT res;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    switch (pdrv) {
        case DEV_SD :
            {
                result = cyhal_sdhc_write(&sdhc_obj, sector, buff, &count);

                res = (result == CY_RSLT_SUCCESS) ? RES_OK : RES_ERROR;
                return res;
            }
    }

    return RES_PARERR;
}

#endif


DWORD get_fattime (void)
{
    // TODO: Add RTC?
    return (40 << 25) | (1 << 21) | (1 << 16) | (0 << 11) | (0 << 5) | (0 << 0);
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
    BYTE pdrv,        /* Physical drive number (0..) */
    BYTE cmd,        /* Control code */
    void *buff        /* Buffer to send/receive control data */
)
{
    switch (pdrv) {
    case DEV_SD :

        switch (cmd) {
            case GET_SECTOR_SIZE :     // Get R/W sector size (WORD)
                *(WORD *) buff = 512;
            break;
            case GET_BLOCK_SIZE :      // Get erase block size in unit of sector (DWORD)
                *(DWORD *) buff = 32;
            break;
            case GET_SECTOR_COUNT :
                *(DWORD *) buff = sdhc_obj.context.maxSectorNum;
            break;
            case CTRL_SYNC :
            break;
        }

        return RES_OK;
    }

    return RES_PARERR;
}

