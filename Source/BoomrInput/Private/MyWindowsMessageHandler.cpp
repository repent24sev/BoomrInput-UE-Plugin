#include "MyWindowsMessageHandler.h"
#include "BoomrInput.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include <hidsdi.h>
#include <hidpi.h>

FMouseBuffer FMyWindowsMessageHandler::MouseBuffer = FMouseBuffer();
FHidBuffer FMyWindowsMessageHandler::HidBuffer = FHidBuffer();
double FMyWindowsMessageHandler::MouseSensitivity = 0.0;

FMyWindowsMessageHandler::FMyWindowsMessageHandler()
{
	SetMouseSensitivity(800.0, 14.0);
}

void FMyWindowsMessageHandler::Initialize() {
	if (!FSlateApplication::IsInitialized()) {
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::Initialize: FSlateApplication is not initialized! Cannot register RAWINPUT devices without a valid window handle!"));
		return;
	}
	TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();

	if (!Window.IsValid() || !Window->GetNativeWindow().IsValid()) {
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::Initialize: Active top level window is not valid!"));
		return;
	}
	
	HWND WindowHandle = NULL;
	WindowHandle = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();

	if (WindowHandle == NULL) {
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::Initialize: Failed to get valid window handle from active top level window!"));
		return;
	}
	RAWINPUTDEVICE Rid[2];

	// Joystick
	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x04;
	Rid[0].dwFlags = RIDEV_INPUTSINK;
	Rid[0].hwndTarget = WindowHandle;

	// Gamepad
	Rid[1].usUsagePage = 0x01;
	Rid[1].usUsage = 0x05;
	Rid[1].dwFlags = RIDEV_INPUTSINK;
	Rid[1].hwndTarget = WindowHandle;

	if (!RegisterRawInputDevices(Rid, 2, sizeof(RAWINPUTDEVICE))) {
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to register RAWINPUT devices! Error: %d"), (int32)GetLastError());
	}
}

bool FMyWindowsMessageHandler::ProcessMessage(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam, int32& OutResult) {
	if(msg != WM_INPUT){
		return false;
	}
	UINT Size = 0;
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &Size, sizeof(RAWINPUTHEADER));

	if(Size <= 0){
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get raw input data size!"));
		return false;
	}
	TArray<uint8> Buffer;
	Buffer.AddUninitialized(Size);
	RAWINPUT* Raw = (RAWINPUT*)Buffer.GetData();

	if(!GetRawInputData((HRAWINPUT)lParam, RID_INPUT, Raw, &Size, sizeof(RAWINPUTHEADER))){
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get raw input data!"));
		return false;
	}

	switch (Raw->header.dwType) {
	case RIM_TYPEMOUSE:
	{
		if (Raw->data.mouse.lLastX == 0 && Raw->data.mouse.lLastY == 0) {
			//UE_LOG(LogTemp, Log, TEXT("FMyWindowsMessageHandler::ProcessMessage: Raw Mouse Input: X=%d, Y=%d, WheelDelta=%d"), Raw->data.mouse.lLastX, Raw->data.mouse.lLastY, Raw->data.mouse.usButtonData);
			return false;
		}
		MouseBuffer.X += Raw->data.mouse.lLastX;
		MouseBuffer.Y -= Raw->data.mouse.lLastY;
		MouseBuffer.HasData = true; 
		break;
	}
	case RIM_TYPEHID:
	{
		UINT dwSize = 0;
		GetRawInputDeviceInfo(Raw->header.hDevice, RIDI_PREPARSEDDATA, NULL, &dwSize);

		if(dwSize <= 0){
			UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get preparsed data size!"));
			return false;
		}
		TArray<uint8> PreparsedDataBuffer;
		PreparsedDataBuffer.AddUninitialized(dwSize);
		PHIDP_PREPARSED_DATA pPreparsedData = (PHIDP_PREPARSED_DATA)PreparsedDataBuffer.GetData();

		if(GetRawInputDeviceInfo(Raw->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &dwSize) == (UINT)-1){
			UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get preparsed data!"));
			return false;
		}
		HIDP_CAPS Caps;

		if(HidP_GetCaps(pPreparsedData, &Caps) != HIDP_STATUS_SUCCESS){
			UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get preparsed data caps!"));
			return false;
		}
		USHORT CapsLength = Caps.NumberInputValueCaps;

		if(CapsLength <= 0){
			UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get preparsed data caps length!"));
			return false;
		}
		TArray<HIDP_VALUE_CAPS> ValueCaps;
		ValueCaps.AddUninitialized(CapsLength);

		if(HidP_GetValueCaps(HidP_Input, ValueCaps.GetData(), &CapsLength, pPreparsedData) != HIDP_STATUS_SUCCESS){
			UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get preparsed data value caps!"));
			return false;
		}		
		TArray<float> ParsedAxes;
		for (int32 i = 0; i < CapsLength; ++i) {
			USHORT Usage = ValueCaps[i].IsRange ? ValueCaps[i].Range.UsageMin : ValueCaps[i].NotRange.Usage;
			ULONG Value = 0;
			NTSTATUS Status = HidP_GetUsageValue(
				HidP_Input,
				ValueCaps[i].UsagePage,
				0, // LinkCollection
				Usage,
				&Value,
				pPreparsedData,
				(PCHAR)Raw->data.hid.bRawData,
				Raw->data.hid.dwSizeHid
			);
			if (Status != HIDP_STATUS_SUCCESS) {
				UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: Failed to get usage value for usage %d! Status: 0x%X"), Usage, (int32)Status);
				continue;
			}
			float NormalizedValue = 0.0f;
			LONG Min = ValueCaps[i].LogicalMin;
			LONG Max = ValueCaps[i].LogicalMax;
			if (Max > Min) {
				NormalizedValue = -1.0f + 2.0f * (float)((LONG)Value - Min) / (float)(Max - Min);
			}
			else {
				NormalizedValue = (float)Value;
			}
			ParsedAxes.Add(NormalizedValue);
		}
		if (!ParsedAxes.IsValidIndex(0)) {
			UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::ProcessMessage: No axes parsed from HID input!"));
			return false;
		}
		HidBuffer.Axes = MoveTemp(ParsedAxes);
		HidBuffer.HasData = true;
		break;
	}

	default:
		break;
	}
	return false;
}
void FMyWindowsMessageHandler::SetMouseSensitivity(double MouseDPI, double CmPer360) {
	if (MouseDPI <= 0.0 || CmPer360 <= 0.0) {
		UE_LOG(LogBoomrInput, Warning, TEXT("FMyWindowsMessageHandler::SetMouseSensitivity: Invalid parameters! MouseDPI and CmPer360 must be greater than 0."));
		return;
	}
	MouseSensitivity = 360.0 / MouseDPI / CmPer360 * 2.54;
}

#include "Windows/HideWindowsPlatformTypes.h"
