#pragma once

#include "CoreMinimal.h"

#include "Windows/WindowsSystemIncludes.h"
#include "Windows/WindowsApplication.h"

struct FMouseBuffer {
	long X;
	long Y;
	bool HasData;
	void Reset() {
		X = 0;
		Y = 0;
		HasData = false;
	}
};

struct FHidBuffer {
	TArray<float> Axes;
	bool HasData;
	void Reset() {
		Axes.Empty();
		HasData = false;
	}
};

class BOOMRINPUT_API FMyWindowsMessageHandler : public IWindowsMessageHandler
{
public:
	FMyWindowsMessageHandler();

	static void Initialize();
	
	virtual bool ProcessMessage(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam, int32& OutResult) override;

	static void SetMouseSensitivity(double MouseDPI, double CmPer360);

	static double MouseSensitivity;
	static FMouseBuffer MouseBuffer;
	static FHidBuffer HidBuffer;
};
