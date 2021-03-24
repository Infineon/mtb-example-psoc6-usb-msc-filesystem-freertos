/*****************************************************************************
* File Name: audio_fs.c
*
* Description:
*  This file provides the source code to implement the audio related storage
*  on the file system.
*
* Note:
*
******************************************************************************
* Copyright 2021, Cypress Semiconductor Corporation (an Infineon company) or
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
#include "audio_fs.h"
#include "ff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
* Global variables
********************************************************************************/
static BYTE work[FF_MAX_SS];
static const char config_content[CONFIG_FILE_SIZE] = CONFIG_FILE_TXT;
static uint32_t file_record_num = 1;
static char filename[32];  
FATFS fs;
FIL current_fp;

/*******************************************************************************
* Function Name: audio_fs_init
********************************************************************************
* Summary:
*   Mounts the file system and manage the config file and records folder. It 
*   formats the memory, if necessary.
*
*******************************************************************************/
void audio_fs_init(bool force_format)
{
    FRESULT result;
    FIL   fp;
    const MKFS_PARM fs_param =
    {
        .fmt = FM_FAT32,  /* Format option */
        .n_fat = 1,       /* Number of FATs */
        .align = 0,
        .n_root = 0,
        .au_size = 0
    };

    if (force_format)
    {
        printf("\n\rFormatting file system... ");
        result = f_mkfs("", &fs_param, work, FF_MAX_SS);
        if (result == FR_OK)
        {
            f_mount(&fs, "", 1);
            f_setlabel(DRIVE_LABEL_NAME);
            printf("done!\n\r");
        }
        else
        {
            printf("not able to create a file system!\n\r");
            return;
        }
    }

    /* Attempt to mount the external memory */
    result = f_mount(&fs, "", 1);

    switch (result)
    {
        case FR_NO_FILESYSTEM:
            /* No file system, create a FAT system */
            printf("\n\rNo file system, creating one... ");
            result = f_mkfs("", &fs_param, work, FF_MAX_SS);
            if (result == FR_OK)
            {
                f_mount(&fs, "", 1);
                f_setlabel(DRIVE_LABEL_NAME);
                printf("done!\n\r");             
            }
            else
            {
                printf("not able to create a file system!\n\r");
                return;
            }
            break;
        case FR_NOT_READY:
            printf("\r\nSD Card not present! Insert one to the SD card slot\n\r");
            break;
        default:
            break;
    }

    /* Check for the config file */
    result = f_open(&fp, CONFIG_FILE_NAME, FA_OPEN_EXISTING | FA_WRITE | FA_READ);

    if (result == FR_NO_FILE)
    {
        printf("\n\rCreating a new %s file... ", CONFIG_FILE_NAME);

        /* Create a new file */
        result = f_open(&fp, CONFIG_FILE_NAME, FA_CREATE_NEW | FA_WRITE | FA_READ);

        if (result == FR_OK)
        {
            UINT count;
            result = f_write(&fp, config_content, sizeof(config_content), &count);

            if (result == FR_OK)
            {
                printf("done!\n\r");
            }
            else
            {
                printf("failed to write to the file!\n\r");
            }
        }
        else
        {
            printf("failed to create the file!\n\r");
        }
    }
    f_close(&fp);

    /* Create the records folder */
    result = f_mkdir(RECORD_FOLDER_NAME);

    if ((result != FR_EXIST) && (result != FR_OK))
    {
        printf("\n\rNot able to create %s folder!\n\r", RECORD_FOLDER_NAME);
    }
}

/*******************************************************************************
* Function Name: audio_fs_get_config
********************************************************************************
* Summary:
*   Return the config file information.
*
* Parameters:
*   sample_rate = frame rate in Hertz
*   is_stereo = true if stereo, false if mono
*
*******************************************************************************/
void audio_fs_get_config(uint32_t *sample_rate, bool *is_stereo)
{
    FRESULT result;
    FIL fp;
    char line[32];
    char *str;

    result = f_open(&fp, CONFIG_FILE_NAME, FA_OPEN_EXISTING | FA_READ);

    /* Load the default values */
    *sample_rate = CONFIG_DEFAULT_SAMPLE_RATE;
    *is_stereo   = CONFIG_DEFAULT_MODE; 

    if (result == FR_OK)
    {
        /* Read each line in the file */
        while (f_gets(line, sizeof(line), &fp))
        {
            /* Check if has the SAMPLE_RATE info */
            str = strstr(line, STRING_SAMPLE_RATE);
            if (str != NULL)
            {
                *sample_rate = strtol(line + sizeof(STRING_SAMPLE_RATE) - 1, &str, 10);                   
            }     
            else
            {
                /* Check if has the SAMPLE_MODE info */
                str = strstr(line, STRING_SAMPLE_MODE);
                if (str != NULL)
                {
                    str = strstr(line, "mono");
                    if (str != NULL)
                    {
                        *is_stereo = false;
                    }
                    else
                    {
                        *is_stereo = true;
                    }                  
                }              
            }                 
        }
    }
    else
    {
        printf("Error opening file!\n\r");
    }
    
    printf("SAMPLE_RATE = %lu\n\r", (unsigned long) *sample_rate); 
    printf("SAMPLE_MODE = %s\n\r", (*is_stereo) ? "stereo" : "mono");

    f_close(&fp);
}

/*******************************************************************************
* Function Name: audio_fs_new_record
********************************************************************************
* Summary:
*   Create a new file to record audio data. The filename is based on the last
*   file record number.
*
* Return:
*   Return true if success, false if error.
*
*******************************************************************************/
bool audio_fs_new_record(void)
{
    FRESULT result;
      
    do {
        /* Build the filename */
        sprintf(filename, "%s/%s%.4lu.raw", RECORD_FOLDER_NAME, RECORD_FILE_NAME, (unsigned long) file_record_num);

        /* Attempt to open */
        result = f_open(&current_fp, filename, FA_CREATE_NEW | FA_WRITE);

        if (result == FR_EXIST)
        {
            f_close(&current_fp);
        }

        file_record_num++;
    } 
    while ((result == FR_EXIST) && (file_record_num != RECORD_MAX_NUM));
    
    if (result != FR_OK)
    {
        printf("Can't create a new record\n\r");
        f_close(&current_fp);
        return false;
    }

    return true;
}

/*******************************************************************************
* Function Name: audio_fs_write
********************************************************************************
* Summary:
*   Write some audio data to the open file record.
*
* Parameters:
*   buf = pointer to the buffer
*   len = length of the buffer
*
* Return:
*   Return true if success, false if error.
*
*******************************************************************************/
bool audio_fs_write(uint8_t *buf, uint32_t len)
{
    FRESULT result;
    UINT count;

    result = f_write(&current_fp, buf, len, &count);
    result |= f_sync(&current_fp);

    if ((result != FR_OK) || (count != len))
    {
        printf("Error writing to the record!\n\r");
        f_close(&current_fp);
        return false;
    }

    return true;
}

/*******************************************************************************
* Function Name: audio_fs_save
********************************************************************************
* Summary:
*   Close the file.
*
*******************************************************************************/
void audio_fs_save(void)
{
    printf("File created: %s\n\r", filename);
    f_close(&current_fp);
}

/*******************************************************************************
* Function Name: audio_fs_list
********************************************************************************
* Summary:
*   List all files in the record folder. Update the latest record number.
*
*******************************************************************************/
void audio_fs_list(void)
{
    FRESULT result;
    FILINFO fno;
    DIR dir;
    char *str;
    uint32_t record_num;

    printf("\n\rList of records:\n\r");

    result = f_findfirst(&dir, &fno, RECORD_FOLDER_NAME, 
                                     RECORD_PATTERN(RECORD_FILE_NAME, RECORD_FILE_EXT));

    if (fno.fname[0] == 0)
    {
        printf("<Empty>\n\r");
        file_record_num = 1;
    }

    while ((result == FR_OK) && (fno.fname[0]))
    {
        /* Update the latest record */
        record_num = strtol(fno.fname + sizeof(RECORD_FILE_NAME) - 1, &str, 10);

        /* Find the latest record */
        if (file_record_num < record_num)
        {
            file_record_num = record_num;
        }

        printf("%s\n\r", fno.fname);
        result = f_findnext(&dir, &fno);       
    }

    f_closedir(&dir);
}