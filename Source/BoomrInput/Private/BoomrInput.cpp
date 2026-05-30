// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoomrInput.h"

DEFINE_LOG_CATEGORY(LogBoomrInput);

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Windows/WindowsApplication.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "FBoomrInputModule"

void FBoomrInputModule::StartupModule()
{
#if PLATFORM_WINDOWS
	FMyWindowsMessageHandler::Initialize();

	MessageHandler = MakeShared<FMyWindowsMessageHandler>();
	if (FSlateApplication::IsInitialized())
	{
		FWindowsApplication* WinApp = static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get());
		if (WinApp)
		{
			WinApp->AddMessageHandler(*MessageHandler);
		}
	}
#endif
}

void FBoomrInputModule::ShutdownModule()
{
#if PLATFORM_WINDOWS
	if (MessageHandler.IsValid() && FSlateApplication::IsInitialized())
	{
		FWindowsApplication* WinApp = static_cast<FWindowsApplication*>(FSlateApplication::Get().GetPlatformApplication().Get());
		if (WinApp)
		{
			WinApp->RemoveMessageHandler(*MessageHandler);
		}
	}
	MessageHandler.Reset();
#endif 
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBoomrInputModule, BoomrInput)