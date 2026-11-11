#ifdef __ANDROID__

#include "TouchController.h"

#include "TouchControls.h"

namespace SohTouch {

TouchController::TouchController() : LUS::Controller(-1) {
    mGuid = "TouchControls";
    mControllerName = "Touch Controls";
}

void TouchController::ReadDevice(int32_t portIndex) {
    const TouchControls::TickInput in = TouchControls::GetInstance().ConsumeTickInput();

    GetPressedButtons(portIndex) = in.buttons;
    GetLeftStickX(portIndex) = in.stickX;
    GetLeftStickY(portIndex) = in.stickY;
    GetRightStickX(portIndex) = in.rightStickX;
    GetRightStickY(portIndex) = in.rightStickY;
    GetGyroX(portIndex) = in.gyroX;
    GetGyroY(portIndex) = in.gyroY;
}

} // namespace SohTouch

#endif // __ANDROID__
