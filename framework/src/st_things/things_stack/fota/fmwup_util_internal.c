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

#include <fota_api/fmwup_api.h>
#include "fmwup_util_internal.h"
//#include "fmwup_util_http.h"
#include "fmwup_util_data.h"
#include <pthread.h>
#include <errno.h>
#include "utils/things_rtos_util.h"
#include "utils/things_malloc.h"
#include "logging/things_logger.h"

#define TAG "[things_fota]"

#define TIMEOUT_SEC 2

static int g_thread_running = 0;

static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _cond = PTHREAD_COND_INITIALIZER;
extern char * g_token_URL;
extern char * g_new_version;

int fmwup_get_data(things_resource_s *target_resource)
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);

	things_representation_s *rep = NULL;

	if (target_resource->rep == NULL) {
		rep = things_create_representation_inst(NULL);
		target_resource->things_set_representation(target_resource, rep);
	} else {
		rep = target_resource->rep;
	}

	fmwup_data_s *fmwup_data;
	fmwup_data = fmwup_data_get_properties();

	char *update;
	update = fmwup_data_get_update_string(fmwup_data->update);

	rep->things_set_value(rep, FIRMWARE_PROPERTY_UPDATE, update);
	rep->things_set_value(rep, FIRMWARE_PROPERTY_UPDATE_TIME, fmwup_data->update_time);
	rep->things_set_int_value(rep, FIRMWARE_PROPERTY_STATE, (int64_t)fmwup_data->state);
	rep->things_set_int_value(rep, FIRMWARE_PROPERTY_RESULT, (int64_t)fmwup_data->result);

	rep->things_set_value(rep, FIRMWARE_PROPERTY_NEW_VERSION, fmwup_data->new_version);
	rep->things_set_value(rep, FIRMWARE_PROPERTY_PACKAGE_URI, fmwup_data->package_uri);

	rep->things_set_value(rep, FIRMWARE_PROPERTY_CURRENT_VERSION, fmwup_data->current_version);
	rep->things_set_value(rep, FIRMWARE_PROPERTY_VENDER, fmwup_data->manufacturer);
	rep->things_set_value(rep, FIRMWARE_PROPERTY_MODEL, fmwup_data->model_name);

	THINGS_LOG_V(TAG, "firmware_version = %s", fmwup_data->current_version);
	THINGS_LOG_V(TAG, "vendor_id = %s", fmwup_data->manufacturer);
	THINGS_LOG_V(TAG, "model_number = %s", fmwup_data->model_name);

	things_free(fmwup_data->update_time);
	things_free(fmwup_data->new_version);
	things_free(fmwup_data->package_uri);
	things_free(fmwup_data->current_version);
	things_free(fmwup_data->manufacturer);
	things_free(fmwup_data->model_name);
	things_free(fmwup_data);

	fmwup_internal_propagate_cond_signal();

	return OC_EH_OK;
}

int fmwup_set_data(things_resource_s *target_res)
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);

	OCEntityHandlerResult eh_result = OC_EH_ERROR;

	things_representation_s *rep = NULL;

	if (target_res->rep != NULL) {
		rep = target_res->rep;
	} else {
		THINGS_LOG_E(TAG, "fmwup_set_data is NULL");
		return OC_EH_ERROR;
	}

	int update;
	char *update_str;
	char *new_version;
	char *package_uri;

	rep->things_get_value(rep, FIRMWARE_PROPERTY_UPDATE, &update_str);
	update = fmwup_data_get_update_int64(update_str);

	if (fmwup_data_set_property_int64(FIRMWARE_PROPERTY_UPDATE, update) != 0) {
		THINGS_LOG_V(TAG, "fmwup_data_set_property [%s] failed", FIRMWARE_PROPERTY_UPDATE);
	}

	THINGS_LOG_V(TAG, "update : %d\n", update);
	fmwup_internal_update_command(update);


	return OC_EH_OK;
}

int fmwup_check_firmware_upgraded(void)
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);

	char *temp_state = fmwup_data_get_property(FIRMWARE_PROPERTY_STATE);
	if (!temp_state) {
		THINGS_LOG_E(TAG, "failed to get state property");
		return FMWUP_ERROR_OPERATION_FAILED;
	}

	int state = FMWUP_STATE_IDLE;
	state = atoi(temp_state);
	things_free(temp_state);

	if (state == FMWUP_STATE_UPDATING) {
		THINGS_LOG_V(TAG, "***Firmware Upgrade Done***");

		// TODO :: It should be checked and set result value according to upgrade result
		/* Initialize - new platform information - */
		char *new_version = fmwup_data_get_property(FIRMWARE_PROPERTY_NEW_VERSION);
		if (new_version) {
			fmwup_data_set_property(FIRMWARE_PROPERTY_CURRENT_VERSION, new_version);
			things_free(new_version);
		} else {
			THINGS_LOG_E(TAG, "failed get new version");
		}

		/* Initialize - new firmware information - */
		key_manager_reset_data();

		fmwup_internal_propagate_resource(FMWUP_STATE_IDLE, FMWUP_RESULT_SUCCESS, false);

	} else {
		THINGS_LOG_V(TAG, "is not updating, state[%d]", state);
		return FMWUP_ERROR_OPERATION_FAILED;
	}

	return FMWUP_ERROR_NONE;
}


void fmwup_internal_propagate_timed_wait()
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);

	struct timeval now;			/* start time to wait    */
	struct timespec timeout;	/* timeout value in waiting function */
	struct timezone zone;
	pthread_mutex_lock(&_lock);

	gettimeofday(&now, &zone);
	timeout.tv_sec = now.tv_sec + TIMEOUT_SEC;
	timeout.tv_nsec = now.tv_usec * 1000;
	/* timeval = microsecond */
	/* timespec = nanosecond */
	/* 1 nano = 1000 micro */

	int rc = pthread_cond_timedwait(&_cond, &_lock, &timeout);
	switch (rc) {
	case 0:
		break;
	case ETIMEDOUT:
		THINGS_LOG_E(TAG, "timeout %d second", TIMEOUT_SEC);
		break;
	default:
		THINGS_LOG_E(TAG, "failed pthread_cond_timedwait [%d]", rc);
		break;
	}
	pthread_mutex_unlock(&_lock);

	return;
}

void fmwup_internal_propagate_cond_signal()
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);
	pthread_mutex_lock(&_lock);
	pthread_cond_signal(&_cond);
	pthread_mutex_unlock(&_lock);
	return;
}

void fmwup_internal_propagate_resource(fmwup_state_e state, fmwup_result_e result, bool wait_flag)
{
	THINGS_LOG_E(TAG, "Propagate state[%d], result[%d]", state, result);

	int ret = 0;

	fmwup_data_set_property_int64(FIRMWARE_PROPERTY_STATE, (int64_t) state);
	fmwup_data_set_property_int64(FIRMWARE_PROPERTY_RESULT, (int64_t) result);

	ret = st_things_notify_observers(FIRMWARE_URI);
	if (ST_THINGS_ERROR_NONE != ret) {
		THINGS_LOG_E(TAG, "Failed st_things_notify_observers [%d]", ret);
		return;
	}

	if (wait_flag) {
		fmwup_internal_propagate_timed_wait();
	}

	if (result != FMWUP_RESULT_INIT) {
		fmwup_data_set_property_int64(FIRMWARE_PROPERTY_STATE, (int64_t) FMWUP_STATE_IDLE);
		fmwup_data_set_property_int64(FIRMWARE_PROPERTY_RESULT, (int64_t) FMWUP_RESULT_INIT);
	}
	return;
}

void _handle_update_command(int64_t update_type)
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);

	int result = FMWUP_RESULT_INIT;

	char *temp_state = fmwup_data_get_property(FIRMWARE_PROPERTY_STATE);
	int state = 0;

	if (!temp_state) {
		THINGS_LOG_E(TAG, "failed get property");
		result = FMWUP_RESULT_UPDATE_FAILED;
		goto _END_OF_FUNC_;
	}
	state = atoi(temp_state);
	THINGS_LOG_D(TAG, "state : %d, update_type : %d", state, update_type);

	if ((state == FMWUP_STATE_IDLE) && (update_type == FMWUP_COMMAND_CHECK)) {
		if (g_token_URL == NULL) {
			int ret = fmwup_check_firmware();
			THINGS_LOG_V(TAG, "***CHECKING FOR FIRMWARE UPDATE ***");
			if (ret == FMWUP_CHECK_RESULT_ERROR) {
				THINGS_LOG_E(TAG, "fmwup_check_firmware failed");
				result = FMWUP_RESULT_CONNECTION_LOST;
				goto _END_OF_FUNC_;
			} else if (ret == FMWUP_CHECK_RESULT_ALREADY_CHECKED) {
				THINGS_LOG_V(TAG, "***FIRMWARE ALREADY UPDATED***");
				result = FMWUP_RESULT_INIT;
				fmwup_internal_propagate_resource(FMWUP_STATE_IDLE, FMWUP_RESULT_INIT, false);
			} else if (ret == FMWUP_CHECK_RESULT_UPDATE_AVAILABLE) {
				fmwup_data_set_property(FIRMWARE_PROPERTY_NEW_VERSION, g_new_version);
				fmwup_data_set_property(FIRMWARE_PROPERTY_PACKAGE_URI, g_token_URL);
				fmwup_internal_propagate_resource(FMWUP_STATE_IDLE, FMWUP_RESULT_INIT, false);
			}
		} else {
			THINGS_LOG_V(TAG, "***UPDATE ALREADY CHECKED***");
			result = FMWUP_RESULT_INIT;
			fmwup_internal_propagate_resource(FMWUP_STATE_IDLE, FMWUP_RESULT_INIT, false);
		}
		THINGS_LOG_V(TAG, "***FIRMWARE UPDATE CHECK COMPLETE***");

	}

	if ((state == FMWUP_STATE_IDLE) && (update_type == FMWUP_COMMAND_DOWNLOAD || update_type == FMWUP_COMMAND_DOWNLOADUPDATE)) {
		if (!g_token_URL) {
			THINGS_LOG_E(TAG, "Wrong state, Please check function usage");
			result = FMWUP_RESULT_UPDATE_UNSUPPORTED_PROTOCOL;
			goto _END_OF_FUNC_;
		}
		THINGS_LOG_V(TAG, "***Downloading image***");
		state = FMWUP_STATE_DOWNLOADING;
		fmwup_internal_propagate_resource(FMWUP_STATE_DOWNLOADING, FMWUP_RESULT_INIT, false);

		if (fmwup_download_firmware() < 0) {
			THINGS_LOG_E(TAG, "fmwup_http_download_file failed");
			result = FMWUP_RESULT_INVALID_URI;
			goto _END_OF_FUNC_;
		}

		THINGS_LOG_V(TAG, "*** Firmware image downloaded ***");
		state = FMWUP_STATE_DOWNLOADED;
		fmwup_internal_propagate_resource(FMWUP_STATE_DOWNLOADED, FMWUP_RESULT_INIT, false);
	}

	sleep(2);

	if ((state == FMWUP_STATE_DOWNLOADED) && (update_type == FMWUP_COMMAND_UPDATE || update_type == FMWUP_COMMAND_DOWNLOADUPDATE )) {
		state = FMWUP_STATE_UPDATING;
		fmwup_internal_propagate_resource(FMWUP_STATE_UPDATING, FMWUP_RESULT_INIT, false);

		key_manager_save_data();

		if (fmwup_update_firmware() < 0) {
			THINGS_LOG_E(TAG, "fmwup_update_firmware failed");
			result = FMWUP_RESULT_UPDATE_FAILED;
			goto _END_OF_FUNC_;
		}

	}

_END_OF_FUNC_:
	if (result != FMWUP_RESULT_INIT) {
		THINGS_LOG_E(TAG, "propagate printf[%d]", result);
		state = FMWUP_STATE_IDLE;
		fmwup_internal_propagate_resource(FMWUP_STATE_IDLE, result, false);
	}

	return;
}

void *_update_worker(void *data)
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);

	int64_t exec_type = *(int64_t *) data;
	_handle_update_command(exec_type);
	things_free(data);

	pthread_mutex_lock(&_lock);
	g_thread_running = 0;
	pthread_mutex_unlock(&_lock);

	return;
}

int fmwup_internal_update_command(int64_t update_type)
{
	THINGS_LOG_D(TAG, THINGS_FUNC_ENTRY);
	pthread_mutex_lock(&_lock);

	if (g_thread_running) {
		/* Allow only one running thread */
		pthread_mutex_unlock(&_lock);
		THINGS_LOG_E(TAG, "duplicated request");
		return FMWUP_ERROR_NONE;
	} else {
		g_thread_running = 1;
	}

	pthread_mutex_unlock(&_lock);

	int64_t *update = (int64_t *) things_calloc(1, sizeof(int64_t));
	if (!update) {
		THINGS_LOG_E(TAG, "Memory allocation error!");
		return FMWUP_ERROR_MEMORY_ERROR;
	}

	*update = update_type;

	pthread_t fota_thread_handler;
	if (pthread_create_rtos(&fota_thread_handler, NULL, _update_worker, (void *)update, THGINS_STACK_FOTA_UPDATE_THREAD) != 0) {
		THINGS_LOG_E(TAG, "Create thread is failed.");
		return FMWUP_ERROR_OPERATION_FAILED;
	}

	return FMWUP_ERROR_NONE;
}

int fmwup_initialize(void)
{
	printf("Entry");

	if (key_manager_init() < 0) {
		printf("fail key_manager_init()");
		return -1;
	}

	return 0;
}

