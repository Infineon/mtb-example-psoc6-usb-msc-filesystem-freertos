/*****************************************************************************
* File Name: audio_fs.h
*
* Description:
*  This file contains the function prototypes and constants used in
*  the audio_fs.c.
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

#ifndef AUDIO_FS_H_
#define AUDIO_FS_H_

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
* Constants
********************************************************************************/
#define MODE_STEREO         1
#define MODE_MONO           0

#define RECORD_MAX_NUM      10000

/* Constant Names */
#define RECORD_FOLDER_NAME  "PSOC_RECORDS"
#define RECORD_FILE_NAME    "rec_"
#define RECORD_FILE_EXT     "raw"
#define CONFIG_FILE_NAME    "config.txt"

#define RECORD_PATTERN(NAME, EXT)   NAME "*" EXT

/* Default config file content */
#define CONFIG_FILE_TXT     "# Set the sample rate in Hertz\r\n" \
                            "SAMPLE_RATE_HZ=48000\r\n" \
                            "\r\n# Sample mode (stereo, mono)\r\n" \
                            "SAMPLE_MODE=stereo"

/* Default settings, if invalid config file */
#define CONFIG_DEFAULT_SAMPLE_RATE  48000
#define CONFIG_DEFAULT_MODE         MODE_STEREO

#define CONFIG_FILE_SIZE    256u

/* Config Strings */
#define STRING_SAMPLE_RATE  "SAMPLE_RATE_HZ="
#define STRING_SAMPLE_MODE  "SAMPLE_MODE="

/* Drive Label Name */
#define DRIVE_LABEL_NAME    "PSoC Drive"

/*******************************************************************************
* Functions
********************************************************************************/
void audio_fs_init(bool force_format);
void audio_fs_get_config(uint32_t *sample_rate, bool *is_stereo);
bool audio_fs_new_record(void);
bool audio_fs_write(uint8_t *buf, uint32_t len);
void audio_fs_save(void);
void audio_fs_list(void);

#endif /* AUDIO_FS_H_ */