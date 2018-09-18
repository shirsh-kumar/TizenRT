/* ****************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#ifndef _FMWUP_API_H_
#define _FMWUP_API_H_

//#include <st_things/st_things.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIRMWARE_URI                            "/firmware"

/* save to key manager(ckmc) with update */
#define FIRMWARE_PROPERTY_UPDATE                "update"
#define FIRMWARE_PROPERTY_UPDATE_TIME           "updatetime"
#define FIRMWARE_PROPERTY_DESCRIPTION           "description"
#define FIRMWARE_PROPERTY_STATE                 "state"
#define FIRMWARE_PROPERTY_RESULT                "result"
#define FIRMWARE_PROPERTY_PROGRESS              "progress"

/* save to key manager(ckmc) with pacakage */
#define FIRMWARE_PROPERTY_NEW_VERSION           "newversion"
#define FIRMWARE_PROPERTY_PACKAGE_URI           "packageuri"

/* platform information */
#define FIRMWARE_PROPERTY_CURRENT_VERSION       "version"
#define FIRMWARE_PROPERTY_VENDER                "vender"
#define FIRMWARE_PROPERTY_MODEL                 "model"

#define KEY_MANAGER_MAX_DATA_LENGTH             21
#define KEY_MANAGER_INT_DEFAULT_DATA            "0"
#define KEY_MANAGER_STR_DEFAULT_DATA            " "
#define KEY_MANAGER_BOL_DEFAULT_DATA            "0"

#define FIRMWARE_PROPERTY_UPDATE_INIT           "Init"
#define FIRMWARE_PROPERTY_UPDATE_CHECK          "Check"
#define FIRMWARE_PROPERTY_UPDATE_DOWNLOAD       "Download"
#define FIRMWARE_PROPERTY_UPDATE_UPDATE         "Update"
#define FIRMWARE_PROPERTY_UPDATE_DOWNLOADUPDATE "DownloadUpdate"
#define FOTA_EXTERN __attribute__ ((__visibility__ ("default")))
/**
 * @brief Enumeration for XXX.
 */
typedef enum {
	FMWUP_COMMAND_INIT = 0,					/**< 0: Initial value */
	FMWUP_COMMAND_CHECK,
	FMWUP_COMMAND_DOWNLOAD,
	FMWUP_COMMAND_UPDATE,
	FMWUP_COMMAND_DOWNLOADUPDATE,
	FMWUP_COMMAND_UNKNOWN
} fmwup_update_e;

typedef enum {
	FMWUP_CHECK_RESULT_ERROR = -1,
	FMWUP_CHECK_RESULT_UPDATE_AVAILABLE = 1,
	FMWUP_CHECK_RESULT_ALREADY_CHECKED
} fmwup_check_update_result_e;

FOTA_EXTERN int fmwup_check_firmware(void);

FOTA_EXTERN int fmwup_download_firmware(void);

FOTA_EXTERN int fmwup_update_firmware(void);

#ifdef __cplusplus
}
#endif
#endif							// _FMWUP_API_H_
