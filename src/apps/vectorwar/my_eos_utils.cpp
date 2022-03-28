#include <string>
#include <iostream>

#include "my_eos_utils.h"
#include "my_eos_platform.h"

#include "eos_init.h"
#include "eos_logging.h"

#include "../../../eos_secrets.h"

/**
* Callback function to use for EOS SDK log messages
*
* @param InMsg - A structure representing data for a log message
*/
void EOS_CALL _EOSSDKLoggingCallback(const EOS_LogMessage* InMsg)
{
	std::cout << "[EOS SDK] " << InMsg->Category << ": " << InMsg->Message << std::endl;
}

void _EOS_InitPlatform()
{
	printf("[EOS SDK] Initializing ...\n");

	// Init EOS SDK
	EOS_InitializeOptions SDKOptions = {};
	SDKOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
	SDKOptions.AllocateMemoryFunction = nullptr;
	SDKOptions.ReallocateMemoryFunction = nullptr;
	SDKOptions.ReleaseMemoryFunction = nullptr;
	SDKOptions.ProductName = "VectorWar";
	SDKOptions.ProductVersion = "1.0";
	SDKOptions.Reserved = nullptr;
	SDKOptions.SystemInitializeOptions = nullptr;
	SDKOptions.OverrideThreadAffinity = nullptr;

	EOS_EResult InitResult = EOS_Initialize(&SDKOptions);
	if (InitResult != EOS_EResult::EOS_Success)
	{
		printf("[EOS SDK] Init Failed!\n");
		return;
	}

	printf("[EOS SDK] Initialized. Setting Logging Callback ...\n");
	EOS_EResult SetLogCallbackResult = EOS_Logging_SetCallback(&_EOSSDKLoggingCallback);
	if (SetLogCallbackResult != EOS_EResult::EOS_Success)
	{
		printf("[EOS SDK] Set Logging Callback Failed!\n");
	}
	else
	{
		printf("[EOS SDK] Logging Callback Set\n");
		EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Verbose);
	}

	const bool bCreateSuccess = FPlatform::Create();
	if (!bCreateSuccess)
	{
		printf("[EOS SDK] Platform Create failed ...\n");
	}
}
