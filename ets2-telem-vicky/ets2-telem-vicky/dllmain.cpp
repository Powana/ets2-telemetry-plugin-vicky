// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

/**
 * @brief Simple logger.
 *
 * Writes the output into file inside the current directory.
 */

 // Windows stuff.

#ifdef _WIN32
#  define WINVER 0x0500
#  define _WIN32_WINNT 0x0500
#  include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

// SDK

#include "scssdk_telemetry.h"
#include "eurotrucks2/scssdk_eut2.h"
#include "eurotrucks2/scssdk_telemetry_eut2.h"
#include "amtrucks/scssdk_ats.h"
#include "amtrucks/scssdk_telemetry_ats.h"
#include <exception>

#define UNUSED(x)

HANDLE serialHandle;

/**
 * @brief Logging support.
 */
FILE* log_file = NULL;

/**
 * @brief Tracking of paused state of the game.
 */
bool output_paused = true;

/**
 * @brief Should we print the data header next time
 * we are printing the data?
 */
bool print_header = false;


/**
 * @brief Function writting message to the game internal log.
 */
scs_log_t game_log = NULL;

// Management of the log file.

bool init_log(void)
{
	if (log_file) {
		return true;
	}
	fopen_s(&log_file, "telemetry_ben.log", "wt");
	if (!log_file) {
		return false;
	}
	fprintf(log_file, "Log opened\n");
	return true;
}

void finish_log(void)
{
	if (!log_file) {
		return;
	}
	fprintf(log_file, "Log ended\n");
	fclose(log_file);
	log_file = NULL;
}

void log_print(const char* const text, ...)
{
	if (!log_file) {
		return;
	}
	va_list args;
	va_start(args, text);
	vfprintf(log_file, text, args);
	va_end(args);
}

void log_line(const char* const text, ...)
{
	if (!log_file) {
		return;
	}
	va_list args;
	va_start(args, text);
	vfprintf(log_file, text, args);
	fprintf(log_file, "\n");
	va_end(args);
}

/// <summary>
/// Pin mappings for arduino
/// </summary>
char p_front_lblinker  = 13;
char p_front_rblinker  = 12;
char p_back_lblinker   = 7;
char p_back_rblinker   = 6;
char p_lblinkers[] = { p_front_lblinker, p_back_lblinker, '\0' };
char p_rblinkers[] = { p_front_rblinker, p_back_rblinker, '\0' };

char p_low_beam[] = { 10, '\0' };
char p_high_beam[] = { 9, '\0' };
char p_pos_light[] = { 8, '\0' };

char p_front_daytime_running = 11;
char p_back_daytime_running = 5;
char p_daytime_running[] = { p_front_daytime_running, p_back_daytime_running, '\0' };

char p_parking[] = { 99, '\0' }; // There is no parking light
char p_brake[]   = { 4, '\0' };
char p_reverse[] = { 2, '\0'};

char p_back_edge_lamp[] = { 3, '\0' };



bool arduino_init(const char* port_name="\\\\.\\COM3") {
	try {
		serialHandle = CreateFileA(port_name, GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

		// Do some basic settings
		DCB serialParams = { 0 };
		serialParams.DCBlength = sizeof(serialParams);

		GetCommState(serialHandle, &serialParams);
		serialParams.BaudRate = 9600;
		serialParams.ByteSize = 8;
		serialParams.StopBits = TWOSTOPBITS;
		serialParams.Parity = NOPARITY;
		SetCommState(serialHandle, &serialParams);

		// Set timeouts
		COMMTIMEOUTS timeout = { 0 };
		timeout.WriteTotalTimeoutConstant = 50;
		timeout.WriteTotalTimeoutMultiplier = 10;

		SetCommTimeouts(serialHandle, &timeout);

		return 0;
	}
	catch (std::exception& e) {
		log_line("Arduino init exception:");
		log_line(e.what());
		return 1;
	}

}

void arduino_write_pin(char pins[], char val) {
	int i = 0;
	do {
		WriteFile(serialHandle, &pins[i], 1, NULL, NULL);
		WriteFile(serialHandle, &val, 1, NULL, NULL);
		WriteFile(serialHandle, ";", 1, NULL, NULL);
		i++;
	} while (pins[i] != '\0');
	
}

// Handling of individual channels.

SCSAPI_VOID telemetry_light_handler(const scs_string_t name, const scs_u32_t index, const scs_value_t* const value, const scs_context_t context)
{

	assert(value);
	assert(value->type == SCS_VALUE_TYPE_bool);
	assert(context);

	// Match game behaviour with high-beams


	arduino_write_pin((char *)context, value->value_bool.value);
}

/**
 * @brief Telemetry API initialization function.
 *
 * See scssdk_telemetry.h
 */
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params)
{
	// We currently support only one version.

	if (version != SCS_TELEMETRY_VERSION_1_01) {
		return SCS_RESULT_unsupported;
	}

	const scs_telemetry_init_params_v101_t* const version_params = static_cast<const scs_telemetry_init_params_v101_t*>(params);
	if (!init_log()) {
		version_params->common.log(SCS_LOG_TYPE_error, "Unable to initialize the log file");
		return SCS_RESULT_generic_error;
	}

	// Check application version. Note that this example uses fairly basic channels which are likely to be supported
	// by any future SCS trucking game however more advanced application might want to at least warn the user if there
	// is game or version they do not support.

	log_line("Game '%s' %u.%u", version_params->common.game_id, SCS_GET_MAJOR_VERSION(version_params->common.game_version), SCS_GET_MINOR_VERSION(version_params->common.game_version));

	if (strcmp(version_params->common.game_id, SCS_GAME_ID_EUT2) == 0) {

		// Below the minimum version there might be some missing features (only minor change) or
		// incompatible values (major change).

		const scs_u32_t MINIMAL_VERSION = SCS_TELEMETRY_EUT2_GAME_VERSION_1_00;
		if (version_params->common.game_version < MINIMAL_VERSION) {
			log_line("WARNING: Too old version of the game, some features might behave incorrectly");
		}

		// Future versions are fine as long the major version is not changed.

		const scs_u32_t IMPLEMENTED_VERSION = SCS_TELEMETRY_EUT2_GAME_VERSION_CURRENT;
		if (SCS_GET_MAJOR_VERSION(version_params->common.game_version) > SCS_GET_MAJOR_VERSION(IMPLEMENTED_VERSION)) {
			log_line("WARNING: Too new major version of the game, some features might behave incorrectly");
		}
	}
	else if (strcmp(version_params->common.game_id, SCS_GAME_ID_ATS) == 0) {

		// Below the minimum version there might be some missing features (only minor change) or
		// incompatible values (major change).

		const scs_u32_t MINIMAL_VERSION = SCS_TELEMETRY_ATS_GAME_VERSION_1_00;
		if (version_params->common.game_version < MINIMAL_VERSION) {
			log_line("WARNING: Too old version of the game, some features might behave incorrectly");
		}

		// Future versions are fine as long the major version is not changed.

		const scs_u32_t IMPLEMENTED_VERSION = SCS_TELEMETRY_ATS_GAME_VERSION_CURRENT;
		if (SCS_GET_MAJOR_VERSION(version_params->common.game_version) > SCS_GET_MAJOR_VERSION(IMPLEMENTED_VERSION)) {
			log_line("WARNING: Too new major version of the game, some features might behave incorrectly");
		}
	}
	else {
		log_line("WARNING: Unsupported game, some features or values might behave incorrectly");
	}

	// Set up serial communication with Arduino unit.

	if (arduino_init()) {
		version_params->common.log(SCS_LOG_TYPE_error, "Unable to connect to arduino, make sure it is connected to COM3");
		return SCS_RESULT_generic_error;
	}

	// Register for channels. The channel might be missing if the game does not support
	// it (SCS_RESULT_not_found) or if does not support the requested type
	// (SCS_RESULT_unsupported_type).

	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_rblinker,	SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_rblinkers);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_lblinker,	SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_lblinkers);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_low_beam,	SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_low_beam);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_high_beam,	SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_high_beam);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_engine_enabled,	SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_daytime_running);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_parking,		SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_back_edge_lamp);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_brake,		SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_brake);
	version_params->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_light_reverse,		SCS_U32_NIL, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_none, telemetry_light_handler, &p_reverse);

	// Remember the function we will use for logging.

	game_log = version_params->common.log;
	game_log(SCS_LOG_TYPE_message, "Initializing telemetry plugin - vicky");

	// Initially the game is paused.

	output_paused = true;
	return SCS_RESULT_ok;
}

/**
 * @brief Telemetry API deinitialization function.
 *
 * See scssdk_telemetry.h
 */
SCSAPI_VOID scs_telemetry_shutdown(void)
{
	// Any cleanup needed. The registrations will be removed automatically
	// so there is no need to do that manually.
	CloseHandle(serialHandle);
	game_log = NULL;
	finish_log();
}

// Cleanup

#ifdef _WIN32
BOOL APIENTRY DllMain(
	HMODULE module,
	DWORD  reason_for_call,
	LPVOID reseved
)
{
	if (reason_for_call == DLL_PROCESS_DETACH) {
		finish_log();
		CloseHandle(serialHandle);
	}
	return TRUE;
}
#endif

#ifdef __linux__
void __attribute__((destructor)) unload(void)
{
	finish_log();
}
#endif
