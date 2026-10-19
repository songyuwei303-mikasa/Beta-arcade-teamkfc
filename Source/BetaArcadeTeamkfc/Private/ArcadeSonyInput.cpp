#include "ArcadeSonyInput.h"
#include "ArcadeGamepadMath.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

struct FArcadeSonyInput::FImpl
{
#if PLATFORM_WINDOWS
    IDirectInput8W* Input = nullptr;
    struct FDevice
    {
        GUID Id{};
        bool bReserved = false;
        IDirectInputDevice8W* Handle = nullptr;
        FString Name;
    };
    FDevice Devices[4];
    double NextEnumeration = 0;
    FImpl()
    {
        if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
            reinterpret_cast<void**>(&Input), nullptr)))
            UE_LOG(LogTemp, Warning, TEXT("Arcade: DirectInput unavailable; native Sony support disabled."));
    }
    ~FImpl()
    {
        for (auto& D : Devices) Close(D);
        if (Input) Input->Release();
    }
    static void Close(FDevice& D)
    {
        if (D.Handle) { D.Handle->Unacquire(); D.Handle->Release(); D.Handle = nullptr; }
    }
    static BOOL CALLBACK Enumerate(const DIDEVICEINSTANCEW* Info, void* Context)
    {
        auto& Self = *static_cast<FImpl*>(Context);
        const uint16 Vendor = LOWORD(Info->guidProduct.Data1);
        const uint16 Product = HIWORD(Info->guidProduct.Data1);
        if (Vendor != 0x054c || !(Product == 0x05c4 || Product == 0x09cc || Product == 0x0ba0 || Product == 0x0ce6 || Product == 0x0df2))
            return DIENUM_CONTINUE;
        int32 Slot = INDEX_NONE;
        for (int32 I = 0; I < 4; ++I)
            if (Self.Devices[I].bReserved && IsEqualGUID(Self.Devices[I].Id, Info->guidInstance)) { Slot = I; break; }
        if (Slot == INDEX_NONE)
            for (int32 I = 0; I < 4; ++I) if (!Self.Devices[I].bReserved) { Slot = I; break; }
        if (Slot == INDEX_NONE || Self.Devices[Slot].Handle) return DIENUM_CONTINUE;
        auto& D = Self.Devices[Slot];
        IDirectInputDevice8W* Device = nullptr;
        if (FAILED(Self.Input->CreateDevice(Info->guidInstance, &Device, nullptr))) return DIENUM_CONTINUE;
        if (FAILED(Device->SetDataFormat(&c_dfDIJoystick2)) ||
            FAILED(Device->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
        { Device->Release(); return DIENUM_CONTINUE; }
        for (DWORD Offset : {DWORD(DIJOFS_X), DWORD(DIJOFS_Y)})
        {
            DIPROPRANGE Range{};
            Range.diph.dwSize = sizeof(Range); Range.diph.dwHeaderSize = sizeof(Range.diph);
            Range.diph.dwObj = Offset; Range.diph.dwHow = DIPH_BYOFFSET;
            Range.lMin = -32768; Range.lMax = 32767;
            if (FAILED(Device->SetProperty(DIPROP_RANGE, &Range.diph))) { Device->Release(); return DIENUM_CONTINUE; }
        }
        Device->Acquire();
        D.Handle = Device; D.Id = Info->guidInstance; D.bReserved = true;
        D.Name = Product == 0x0ce6 || Product == 0x0df2 ? TEXT("DualSense") : TEXT("DualShock 4");
        UE_LOG(LogTemp, Display, TEXT("Arcade: Sony slot %d connected (%s, PID %04x)."), Slot + 1, *D.Name, Product);
        return DIENUM_CONTINUE;
    }
#endif
};
FArcadeSonyInput::FArcadeSonyInput() : Impl(MakeUnique<FImpl>()) {}
FArcadeSonyInput::~FArcadeSonyInput() = default;
void FArcadeSonyInput::Poll(FArcadeInputState* Out, float DeadZone)
{
    for (int32 I = 0; I < 4; ++I) Out[I] = FArcadeInputState();
#if PLATFORM_WINDOWS
    if (!Impl->Input) return;
    if (FPlatformTime::Seconds() >= Impl->NextEnumeration)
    {
        Impl->NextEnumeration = FPlatformTime::Seconds() + 1.0;
        Impl->Input->EnumDevices(DI8DEVCLASS_GAMECTRL, &FImpl::Enumerate, Impl.Get(), DIEDFL_ATTACHEDONLY);
    }
    for (int32 I = 0; I < 4; ++I)
    {
        auto& D = Impl->Devices[I];
        if (!D.Handle) continue;
        if (FAILED(D.Handle->Poll())) { D.Handle->Acquire(); D.Handle->Poll(); }
        DIJOYSTATE2 State{};
        if (FAILED(D.Handle->GetDeviceState(sizeof(State), &State)))
        { FImpl::Close(D); continue; }
        auto& P = Out[I];
        P.bConnected = true;
        P.DeviceName = D.Name;
        FVector2D Stick(FMath::Clamp(State.lX / 32767.0, -1.0, 1.0), -FMath::Clamp(State.lY / 32767.0, -1.0, 1.0));
        P.Direction = ArcadeGamepad::Direction(Stick, ArcadeGamepad::POV(State.rgdwPOV[0]), DeadZone);
        // Native Sony Windows HID: square=0, cross=1, circle=2, triangle=3.
        P.bConfirm = ((State.rgbButtons[0] | State.rgbButtons[1]) & 0x80) != 0;
        P.bExit = (State.rgbButtons[2] & 0x80) != 0;
    }
#endif
}
