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