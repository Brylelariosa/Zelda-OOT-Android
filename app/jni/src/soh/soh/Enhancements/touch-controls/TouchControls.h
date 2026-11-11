#pragma once

#ifdef __ANDROID__

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "TouchCore.h"
#include "TouchSettings.h"
#include "libultraship/libultra/controller.h"

struct _SDL_Haptic;
struct _SDL_Sensor;

namespace LUS {
class GuiMenuBar;
} // namespace LUS

namespace SohTouch {

class TouchController;

// Android on-screen controls. Two hooks connect it to the engine:
//   * Draw()      is called once per rendered frame from inside the ImGui frame: it reads the touch screen, updates the
//                 controls and draws them.
//   * ReadToPad() is called once per game tick from padmgr.c (through TouchControls_ReadToPad) and merges the controls
//                 into player 1's pad.
class TouchControls {
  public:
    static TouchControls& GetInstance();

    void Draw();
    void ReadToPad(OSContPad* pad, int32_t portIndex);

    // Opens or closes the "Touch Controls" settings window (wired to a button in the game's settings menu).
    void ToggleSettingsWindow();
    bool IsSettingsWindowOpen() const {
        return mSettingsOpen;
    }

    // What the controls ask of the game for one tick; consumed by TouchController.
    struct TickInput {
        int32_t buttons = 0; // BTN_* bits
        int8_t stickX = 0;
        int8_t stickY = 0;
        int8_t rightStickX = 0;
        int8_t rightStickY = 0;
        float gyroX = 0.0f;
        float gyroY = 0.0f;
    };
    TickInput ConsumeTickInput();

  private:
    TouchControls();
    ~TouchControls();
    TouchControls(const TouchControls&) = delete;
    TouchControls& operator=(const TouchControls&) = delete;

    void InitIfNeeded(float aspect);
    bool DetermineMode(bool menuOpen, Mode& mode) const;
    std::shared_ptr<LUS::GuiMenuBar> GetMenuBar() const;
    void RefreshGamepadPresence();
    void PollFingers();
    void HandleEngineEvents();
    void DrawSettingsWindow();
    void HandleEditToolbar(float screenWidth, float screenHeight);
    void BeginEdit();
    void EndEdit();
    void ResetLayout(float aspect);
    static void ApplyLook(Vec2 look, float dt, const UserSettings& settings, TickInput& out);
    void ReadGyro(float sensitivity, float& gyroX, float& gyroY);
    void Vibrate();

    std::mutex mMutex; // guards the members below it that the game tick also reads
    Engine mEngine;
    UserSettings mSettings;
    bool mShown = false;  // the touch controls are switched on and not hidden by a connected controller
    bool mActive = false; // buttons, stick and look are live (Play mode): their input reaches the game
    uint64_t mLastTickMs = 0;

    // Render-thread only.
    bool mInitialised = false;
    bool mSettingsOpen = false;
    bool mEditing = false;
    bool mLayoutCustom = false;
    float mLayoutAspect = 0.0f;
    float mToolbarRect[4] = { 0, 0, 0, 0 };
    bool mHasToolbarRect = false;
    bool mGamepadPresent = false;
    uint32_t mFrame = 0;
    std::vector<Finger> mFingers;
    bool mDefaultsApplied = false;

    std::unique_ptr<TouchController> mController;

    _SDL_Haptic* mHaptic = nullptr;
    bool mHapticTried = false;
    _SDL_Sensor* mGyro = nullptr;
    bool mGyroTried = false;
    float mGyroBias[2] = { 0.0f, 0.0f };
};

} // namespace SohTouch

#endif // __ANDROID__
