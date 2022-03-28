// Copyright Epic Games, Inc. All Rights Reserved.
#include <string>

#include "my_eos_platform.h"

#if ALLOW_RESERVED_PLATFORM_OPTIONS
#include "ReservedPlatformOptions.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include "Windows/eos_Windows.h"
#endif

#include "../../../eos_secrets.h"

EOS_HPlatform FPlatform::PlatformHandle = nullptr;
bool FPlatform::bIsInit = false;
bool FPlatform::bIsShuttingDown = false;
bool FPlatform::bHasShownCreateFailedError = false;
bool FPlatform::bHasShownInvalidParamsErrors = false;
bool FPlatform::bHasInvalidParamProductId = false;
bool FPlatform::bHasInvalidParamSandboxId = false;
bool FPlatform::bHasInvalidParamDeploymentId = false;
bool FPlatform::bHasInvalidParamClientCreds = false;

const char* GetTempDirectory()
{
#ifdef _WIN32
	static char Buffer[1024] = { 0 };
	if (Buffer[0] == 0)
	{
		GetTempPathA(sizeof(Buffer), Buffer);
	}

	return Buffer;

#elif defined(__APPLE__)
	return "/private/var/tmp";
#else
	return "/var/tmp";
#endif
}

bool FPlatform::Create()
{
	bIsInit = false;

	// Create platform instance
	EOS_Platform_Options PlatformOptions = {};
	PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
	PlatformOptions.bIsServer = false;
	PlatformOptions.EncryptionKey = __ENCRYPTION_KEY__;
	PlatformOptions.OverrideCountryCode = nullptr;
	PlatformOptions.OverrideLocaleCode = nullptr;
	PlatformOptions.Flags = EOS_PF_DISABLE_OVERLAY; // Enable overlay support for D3D9/10 and OpenGL. This sample uses D3D11 or SDL.
	PlatformOptions.CacheDirectory = GetTempDirectory();

	std::string ProductId = __PRODUCT_ID__;
	std::string SandboxId = __SANDBOX_ID__;
	std::string DeploymentId = __DEPLOYMENT_ID__;

	bHasInvalidParamProductId = ProductId.empty() ? true : false;
	bHasInvalidParamSandboxId = SandboxId.empty() ? true : false;
	bHasInvalidParamDeploymentId = DeploymentId.empty() ? true : false;

	PlatformOptions.ProductId = ProductId.c_str();
	PlatformOptions.SandboxId = SandboxId.c_str();
	PlatformOptions.DeploymentId = DeploymentId.c_str();

	std::string ClientId = __CLIENT_ID__;
	std::string ClientSecret = __CLIENT_SECRET__;

	bHasInvalidParamClientCreds = false;
	if (!ClientId.empty() && !ClientSecret.empty())
	{
		PlatformOptions.ClientCredentials.ClientId = ClientId.c_str();
		PlatformOptions.ClientCredentials.ClientSecret = ClientSecret.c_str();
	}
	else if (!ClientId.empty() || !ClientSecret.empty())
	{
		bHasInvalidParamClientCreds = true;
	}
	else
	{
		PlatformOptions.ClientCredentials.ClientId = nullptr;
		PlatformOptions.ClientCredentials.ClientSecret = nullptr;
	}

	if (bHasInvalidParamProductId ||
		bHasInvalidParamSandboxId ||
		bHasInvalidParamDeploymentId ||
		bHasInvalidParamClientCreds)
	{
		return false;
	}

	PlatformOptions.RTCOptions = NULL; //XXX: If you want voicechat, you'll need to set RTCOptions.

#if ALLOW_RESERVED_PLATFORM_OPTIONS
	SetReservedPlatformOptions(PlatformOptions);
#else
	PlatformOptions.Reserved = NULL;
#endif // ALLOW_RESERVED_PLATFORM_OPTIONS

	PlatformHandle = EOS_Platform_Create(&PlatformOptions);

	if (PlatformHandle == nullptr)
	{
		return false;
	}

	bIsInit = true;

	return true;
}

void FPlatform::Release()
{
	bIsInit = false;
	PlatformHandle = nullptr;
	bIsShuttingDown = true;
}

void FPlatform::Update()
{
	if (PlatformHandle)
	{
		EOS_Platform_Tick(PlatformHandle);
	}

	if (!bIsInit && !bIsShuttingDown)
	{
		if (!bHasShownCreateFailedError)
		{
			bHasShownCreateFailedError = true;
			printf("[EOS SDK] Platform Create Failed!\n");
		}
	}

	if (bHasInvalidParamProductId ||
		bHasInvalidParamSandboxId ||
		bHasInvalidParamDeploymentId ||
		bHasInvalidParamClientCreds)
	{
		if (!bHasShownInvalidParamsErrors)
		{
			bHasShownInvalidParamsErrors = true;
			if (bHasInvalidParamProductId)
			{
				printf("[EOS SDK] Product Id is empty, add your product id from Epic Games DevPortal to SampleConstants\n");
			}
			if (bHasInvalidParamSandboxId)
			{
				printf("[EOS SDK] Sandbox Id is empty, add your sandbox id from Epic Games DevPortal to SampleConstants\n");
			}
			if (bHasInvalidParamDeploymentId)
			{
				printf("[EOS SDK] Deployment Id is empty, add your deployment id from Epic Games DevPortal to SampleConstants\n");
			}
			if (bHasInvalidParamClientCreds)
			{
				printf("[EOS SDK] Client credentials are invalid, check clientid and clientsecret in SampleConstants\n");
			}

			printf("FATAL: [EOS SDK] One or more parameters required for EOS_Platform_Create are invalid. Make sure your secrets file has been set up correctly.");
		}
	}
}
