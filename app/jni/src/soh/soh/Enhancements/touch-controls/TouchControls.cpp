#ifdef __ANDROID__

#include "TouchControls.h"

#include <algorithm>
#include <cmath>

#include <SDL2/SDL.h>
#include <ImGui/imgui.h>

#include <libultraship/libultraship.h>

#include "TouchController.h"
#include "TouchControlsBridge.h"
#include "TouchOverlay.h"
#include "TouchOverlayWindow.h"
#include "port/android/AndroidImpl.h"
#include "public/bridge/consolevariablebridge.h"

namespace SohTouch {

namespace {

constexpr int GAMEPAD_RECHECK_FRAMES = 30;
constexpr float kMaxAxis = MAX_AXIS_RANGE;

int8_t ToAxis(float value) {
    return static_cast<int8_t>(std::lround(std::clamp(value, -kMaxAxis, kMaxAxis)));
}

struct ButtonBit {
    Control control;
    int32_t n64;
};

constexpr ButtonBit kButtons[] = {
    { Control::A, BTN_A },         { Control::B, BTN_B },         { Control::Z, BTN_Z },
    { Control::R, BTN_R },         { Control::L, BTN_L },         { Control::Start, BTN_START },
    { Control::CUp, BTN_CUP },     { Control::CDown, BTN_CDOWN }, { Control::CLeft, BTN_CLEFT },
    { Control::CRight, BTN_CRIGHT }, { Control::DUp, BTN_DUP },   { Control::DDown, BTN_DDOWN },
    { Control::DLeft, BTN_DLEFT }, { Control::DRight, BTN_DRIGHT },
};

} // namespace

TouchControls& TouchControls::GetInstance() {
    static TouchControls instance;
    return instance;
}

TouchControls::TouchControls() = default;

TouchControls::~TouchControls() {
    // SDL may already be shut down by the time statics are destroyed.
    if (mGyro != nullptr && SDL_WasInit(SDL_INIT_SENSOR) != 0) {
        SDL_SensorClose(reinterpret_cast<SDL_Sensor*>(mGyro));
    }
    if (mHaptic != nullptr && SDL_WasInit(SDL_INIT_HAPTIC) != 0) {
        SDL_HapticClose(reinterpret_cast<SDL_Haptic*>(mHaptic));
    }
}

void TouchControls::ToggleSettingsWindow() {
    mSettingsOpen = !mSettingsOpen;
}

std::shared_ptr<LUS::GuiMenuBar> TouchControls::GetMenuBar() const {
    const auto context = LUS::Context::GetInstance();
    if (context == nullptr) {
        return nullptr;
    }
    const auto window = context->GetWindow();
    if (window == nullptr) {
        return nullptr;
    }
    const auto gui = window->GetGui();
    return gui != nullptr ? gui->GetMenuBar() : nullptr;
}

void TouchControls::InitIfNeeded(float aspect) {
    if (mInitialised) {
        return;
    }
    mInitialised = true;

    mSettings = Prefs::Load();
    Layout layout = Layout::Default(aspect);
    mLayoutCustom = Prefs::HasCustomLayout();
    Prefs::LoadLayout(layout);
    mLayoutAspect = aspect;

    std::lock_guard<std::mutex> lock(mMutex);
    mEngine.SetLayout(layout);
    mEngine.SetSettings(mSettings.ToEngine());
}

bool TouchControls::DetermineMode(bool menuOpen, Mode& mode) const {
    if (!mSettings.enabled) {
        return false;
    }
    if (mSettings.autoHide && mGamepadPresent) {
        return false;
    }
    if (mEditing) {
        mode = Mode::Edit;
    } else if (menuOpen) {
        mode = Mode::MenuOpen;
    } else if (mSettingsOpen) {
        mode = Mode::Passive;
    } else if (mSettings.hidden) {
        mode = Mode::Hidden;
    } else {
        mode = Mode::Play;
    }
    return true;
}

void TouchControls::RefreshGamepadPresence() {
    if (mFrame++ % GAMEPAD_RECHECK_FRAMES != 0) {
        return;
    }
    bool present = false;
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count && !present; ++i) {
        present = SDL_IsGameController(i) == SDL_TRUE;
    }
    mGamepadPresent = present;
}

void TouchControls::PollFingers() {
    mFingers.clear();
    const int devices = SDL_GetNumTouchDevices();
    for (int d = 0; d < devices; ++d) {
        const SDL_TouchID touchId = SDL_GetTouchDevice(d);
        if (SDL_GetTouchDeviceType(touchId) == SDL_TOUCH_DEVICE_INVALID) {
            continue;
        }
        const int fingers = SDL_GetNumTouchFingers(touchId);
        for (int i = 0; i < fingers; ++i) {
            const SDL_Finger* finger = SDL_GetTouchFinger(touchId, i);
            if (finger == nullptr) {
                continue;
            }
            // Finger ids are only unique per device, so fold the device in.
            const int64_t id = (static_cast<int64_t>(d) << 48) ^ static_cast<int64_t>(finger->id);
            mFingers.push_back({ id, finger->x, finger->y });
        }
    }
}

void TouchControls::Draw() {
    const ImGuiIO& io = ImGui::GetIO();
    const float width = io.DisplaySize.x;
    const float height = io.DisplaySize.y;
    if (width < 1.0f || height < 1.0f) {
        return;
    }
    const float aspect = width / height;

    InitIfNeeded(aspect);
    RefreshGamepadPresence();

    const auto menuBar = GetMenuBar();
    const bool menuOpen = menuBar != nullptr && menuBar->IsVisible();

    Mode mode = Mode::Play;
    const bool shown = DetermineMode(menuOpen, mode);
    if (!shown) {
        mEditing = false;
    }

    Engine snapshot; // drawn from a copy so the game tick never waits on rendering
    if (shown && !mDefaultsApplied) {
        mDefaultsApplied = true;
        Prefs::ApplyRecommendedGameDefaultsOnce();
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mShown = shown;
        if (!shown) {
            mEngine.Reset();
            mActive = false;
        } else {
            if (!mLayoutCustom && std::fabs(aspect - mLayoutAspect) > 0.01f) {
                mEngine.SetLayout(Layout::Default(aspect));
                mLayoutAspect = aspect;
            }
            mEngine.SetScreen(width, height);
            mEngine.SetSettings(mSettings.ToEngine());
            mEngine.SetMode(mode);
            if (mode == Mode::Edit && mHasToolbarRect) {
                mEngine.SetBlockedRect(mToolbarRect[0], mToolbarRect[1], mToolbarRect[2], mToolbarRect[3]);
            } else {
                mEngine.ClearBlockedRect();
            }

            PollFingers();
            mEngine.Update(mFingers.data(), mFingers.size());
            mActive = mode == Mode::Play;
            HandleEngineEvents();
        }
        snapshot = mEngine;
    }

    if (shown) {
        DrawControls(snapshot, mSettings.opacity);
        if (mode == Mode::Edit) {
            HandleEditToolbar(width, height);
        }
    }
    DrawSettingsWindow();
}

// Runs under mMutex, right after the engine has processed this frame's fingers.
void TouchControls::HandleEngineEvents() {
    const uint32_t taps = mEngine.TakeTaps();
    if ((taps & Bit(Control::Menu)) != 0) {
        if (const auto bar = GetMenuBar()) {
            bar->ToggleVisibility();
        }
    }
    if ((taps & Bit(Control::CloseMenu)) != 0) {
        if (const auto bar = GetMenuBar()) {
            bar->Hide();
        }
    }
    if ((taps & Bit(Control::Toggle)) != 0) {
        mSettings.hidden = !mSettings.hidden;
        Prefs::Save(mSettings);
        CVarSave();
    }

    if (mEngine.TakePressEdges() != 0) {
        Vibrate();
    }
    mEngine.TakeLayoutChanged(); // positions are saved when editing is finished, not on every drag
}

void TouchControls::BeginEdit() {
    mEditing = true;
    mHasToolbarRect = false;
    if (const auto bar = GetMenuBar()) {
        bar->Hide(); // the menu bar would cover the top of the screen while arranging controls
    }
}

void TouchControls::EndEdit() {
    Layout layout;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        layout = mEngine.GetLayout();
    }
    Prefs::SaveLayout(layout);
    mLayoutCustom = true;
    mEditing = false;
    mSettingsOpen = true; // back to where the player came from
}

void TouchControls::ResetLayout(float aspect) {
    Prefs::ClearLayout();
    mLayoutCustom = false;
    mLayoutAspect = aspect;
    std::lock_guard<std::mutex> lock(mMutex);
    mEngine.SetLayout(Layout::Default(aspect));
}

void TouchControls::HandleEditToolbar(float screenWidth, float screenHeight) {
    switch (DrawEditToolbar(screenWidth, screenHeight, mToolbarRect)) {
        case EditAction::Done:
            EndEdit();
            break;
        case EditAction::Reset:
            ResetLayout(screenWidth / screenHeight);
            break;
        case EditAction::None:
            break;
    }
    mHasToolbarRect = true;
}

void TouchControls::DrawSettingsWindow() {
    if (!mSettingsOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(560, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Touch Controls", &mSettingsOpen)) {
        UserSettings edited = mSettings;
        const SettingsActions act = DrawSettingsBody(edited);

        if (act.changed) {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mSettings = edited;
            }
            Prefs::Save(edited);
        }
        if (act.flush) {
            CVarSave();
        }
        if (act.startEdit) {
            mSettingsOpen = false;
            BeginEdit();
        }
        if (act.resetLayout) {
            const ImGuiIO& io = ImGui::GetIO();
            ResetLayout(io.DisplaySize.x / std::max(1.0f, io.DisplaySize.y));
        }
    }
    ImGui::End();
}

void TouchControls::ReadToPad(OSContPad* pad, int32_t portIndex) {
    if (pad == nullptr) {
        return;
    }
    if (mController == nullptr) {
        mController = std::make_unique<TouchController>();
    }
    mController->ReadToPad(pad, portIndex);
}

TouchControls::TickInput TouchControls::ConsumeTickInput() {
    TickInput out;
    Vec2 look;
    float dt;
    bool active;
    UserSettings settings;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const uint64_t now = SDL_GetTicks64();
        dt = std::clamp(static_cast<float>(now - mLastTickMs) / 1000.0f, 0.008f, 0.25f);
        mLastTickMs = now;

        look = mEngine.TakeLook(); // always drained, so a drag made while a menu was open never replays later
        if (!mShown) {
            return out;
        }
        active = mActive;
        settings = mSettings;

        if (active) {
            for (const ButtonBit& b : kButtons) {
                if (mEngine.IsPressed(b.control)) {
                    out.buttons |= b.n64;
                }
            }
            const Vec2 stick = mEngine.StickVector();
            out.stickX = ToAxis(stick.x * kMaxAxis);
            out.stickY = ToAxis(stick.y * kMaxAxis);
        }
    }

    if (active) {
        ApplyLook(look, dt, settings, out);
    }
    // Phone motion does not depend on the on-screen buttons, so it keeps working while they are hidden.
    if (settings.gyroAiming) {
        float gx = 0.0f;
        float gy = 0.0f;
        ReadGyro(settings.gyroSensitivity, gx, gy);
        out.gyroX += gx;
        out.gyroY += gy;
    }
    return out;
}

// Gathers the game options that affect camera direction and lets the core map the drag (see MapLook).
void TouchControls::ApplyLook(Vec2 look, float dt, const UserSettings& settings, TickInput& out) {
    LookParams params;
    params.sensitivity = settings.lookSensitivity;
    params.invertX = settings.invertLookX;
    params.invertY = settings.invertLookY;
    params.gameInvertsX = CVarGetInteger("gInvertXAxis", 0) != 0;
    params.gameInvertsY = CVarGetInteger("gInvertYAxis", 1) != 0;
    params.rightStickAiming = CVarGetInteger("gRightStickAiming", 0) != 0;

    const LookOutput mapped = MapLook(look, dt, params);
    out.rightStickX = ToAxis(mapped.rightStickX);
    out.rightStickY = ToAxis(mapped.rightStickY);
    out.gyroX = mapped.gyroX;
    out.gyroY = mapped.gyroY;
}

// Phone motion for first-person aiming. Mirrors what SDLController does for a gamepad that uses the phone's gyro, so the
// axes line up with the game the same way.
void TouchControls::ReadGyro(float sensitivity, float& gyroX, float& gyroY) {
    if (mGyro == nullptr && !mGyroTried) {
        mGyroTried = true;
        SDL_InitSubSystem(SDL_INIT_SENSOR);
        for (int i = 0; i < SDL_NumSensors(); ++i) {
            if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_GYRO) {
                mGyro = reinterpret_cast<_SDL_Sensor*>(SDL_SensorOpen(i));
                break;
            }
        }
    }
    if (mGyro == nullptr) {
        return;
    }

    SDL_SensorUpdate();
    float data[3] = { 0.0f, 0.0f, 0.0f };
    if (SDL_SensorGetData(reinterpret_cast<SDL_Sensor*>(mGyro), data, 3) < 0) {
        return;
    }
    LUS::Android::adjustGyro(data);

    // Sensors have a small constant offset. Follow it slowly while the phone is nearly still so it cancels out.
    constexpr float kStillThreshold = 0.04f; // rad/s
    constexpr float kBiasRate = 0.02f;
    for (int i = 0; i < 2; ++i) {
        const float delta = data[i] - mGyroBias[i];
        if (std::fabs(delta) < kStillThreshold) {
            mGyroBias[i] += delta * kBiasRate;
        }
    }

    gyroX = (data[0] - mGyroBias[0]) * sensitivity;
    gyroY = (data[1] - mGyroBias[1]) * sensitivity;
}

void TouchControls::Vibrate() {
    if (!mSettings.haptics) {
        return;
    }
    if (mHaptic == nullptr && !mHapticTried) {
        mHapticTried = true;
        SDL_InitSubSystem(SDL_INIT_HAPTIC);
        if (SDL_NumHaptics() > 0) {
            SDL_Haptic* haptic = SDL_HapticOpen(0); // the phone's vibrator is the first haptic device on Android
            if (haptic != nullptr && SDL_HapticRumbleInit(haptic) != 0) {
                SDL_HapticClose(haptic);
                haptic = nullptr;
            }
            mHaptic = reinterpret_cast<_SDL_Haptic*>(haptic);
        }
    }
    if (mHaptic != nullptr) {
        SDL_HapticRumblePlay(reinterpret_cast<SDL_Haptic*>(mHaptic), 0.5f, 18);
    }
}

TouchOverlayWindow::TouchOverlayWindow() : LUS::GuiWindow("", true, "Touch Controls Overlay") {
}

void TouchOverlayWindow::DrawElement() {
    TouchControls::GetInstance().Draw();
}

} // namespace SohTouch

extern "C" void TouchControls_ReadToPad(OSContPad* pad) {
    SohTouch::TouchControls::GetInstance().ReadToPad(pad, 0);
}

#endif // __ANDROID__
