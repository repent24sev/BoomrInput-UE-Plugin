// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "MyWindowsMessageHandler.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBoomrInput, Log, All);


class FBoomrInputModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if PLATFORM_WINDOWS
	TSharedPtr<FMyWindowsMessageHandler> MessageHandler;
#endif

};
