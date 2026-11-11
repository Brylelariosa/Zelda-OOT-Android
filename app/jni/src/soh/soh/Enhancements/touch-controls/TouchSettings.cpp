#include "TouchSettings.h"

#include <string>

#include "public/bridge/consolevariablebridge.h"

namespace SohTouch {

namespace {

constexpr const char* kEnabled = "gTouch.Enabled";
constexpr const char* kAutoHide = "gTouch.AutoHide";
constexpr const char* kHidden = "gTouch.Hidden";
constexpr const char* kOpacity = "gTouch.Opacity";
constexpr const char* kScale = "gTouch.Scale";
constexpr const char* kLeftHanded = "gTouch.LeftHanded";
constexpr const char* kFloatingStick = "gTouch.FloatingStick";
constexpr const char* kShowDpad = "gTouch.ShowDpad";
constexpr const char* kHaptics = "gTouch.Haptics";
constexpr const char* kLookSensitivity = "gTouch.LookSensitivity";
constexpr const char* kInvertLookX = "gTouch.InvertLookX";
constexpr const char* kInvertLookY = "gTouch.InvertLookY";
constexpr const char* kGyroAiming = "gTouch.GyroAiming";
constexpr const char* kGyroSensitivity = "gTouch.GyroSensitivity";
constexpr const char* kDefaultsApplied = "gTouch.DefaultsApplied";
constexpr const char* kLayoutCustom = "gTouch.Layout.Custom";

// Layout values are always within 0..1, so a negative number reliably means "never saved".
constexpr float kUnsetFloat = -1.0f;

std::string LayoutKey(Control c, const char* axis) {
    return std::string("gTouch.Layout.") + ControlName(c) + "." + axis;
}

bool GetBool(const char* name, bool fallback) {
    return CVarGetInteger(name, fallback ? 1 : 0) != 0;
}

void SetBool(const char* name, bool value) {
    CVarSetInteger(name, value ? 1 : 0);
}

} // namespace

Settings UserSettings::ToEngine() const {
    Settings s;
    s.scale = scale;
    s.leftHanded = leftHanded;
    s.floatingStick = floatingStick;
    s.showDpad = showDpad;
    return s;
}

namespace Prefs {

UserSettings Load() {
    const UserSettings d; // defaults
    UserSettings s;
    s.enabled = GetBool(kEnabled, d.enabled);
    s.autoHide = GetBool(kAutoHide, d.autoHide);
    s.hidden = GetBool(kHidden, d.hidden);
    s.opacity = CVarGetFloat(kOpacity, d.opacity);
    s.scale = CVarGetFloat(kScale, d.scale);
    s.leftHanded = GetBool(kLeftHanded, d.leftHanded);
    s.floatingStick = GetBool(kFloatingStick, d.floatingStick);
    s.showDpad = GetBool(kShowDpad, d.showDpad);
    s.haptics = GetBool(kHaptics, d.haptics);
    s.lookSensitivity = CVarGetFloat(kLookSensitivity, d.lookSensitivity);
    s.invertLookX = GetBool(kInvertLookX, d.invertLookX);
    s.invertLookY = GetBool(kInvertLookY, d.invertLookY);
    s.gyroAiming = GetBool(kGyroAiming, d.gyroAiming);
    s.gyroSensitivity = CVarGetFloat(kGyroSensitivity, d.gyroSensitivity);
    return s;
}

void Save(const UserSettings& s) {
    SetBool(kEnabled, s.enabled);
    SetBool(kAutoHide, s.autoHide);
    SetBool(kHidden, s.hidden);
    CVarSetFloat(kOpacity, s.opacity);
    CVarSetFloat(kScale, s.scale);
    SetBool(kLeftHanded, s.leftHanded);
    SetBool(kFloatingStick, s.floatingStick);
    SetBool(kShowDpad, s.showDpad);
    SetBool(kHaptics, s.haptics);
    CVarSetFloat(kLookSensitivity, s.lookSensitivity);
    SetBool(kInvertLookX, s.invertLookX);
    SetBool(kInvertLookY, s.invertLookY);
    SetBool(kGyroAiming, s.gyroAiming);
    CVarSetFloat(kGyroSensitivity, s.gyroSensitivity);
}

bool HasCustomLayout() {
    return GetBool(kLayoutCustom, false);
}

void LoadLayout(Layout& layout) {
    if (!HasCustomLayout()) {
        return;
    }
    for (size_t i = 0; i < kControlCount; ++i) {
        const Control c = static_cast<Control>(i);
        const float x = CVarGetFloat(LayoutKey(c, "X").c_str(), kUnsetFloat);
        const float y = CVarGetFloat(LayoutKey(c, "Y").c_str(), kUnsetFloat);
        if (x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f) {
            layout[c].center = { x, y };
        }
    }
}

void SaveLayout(const Layout& layout) {
    for (size_t i = 0; i < kControlCount; ++i) {
        const Control c = static_cast<Control>(i);
        CVarSetFloat(LayoutKey(c, "X").c_str(), layout[c].center.x);
        CVarSetFloat(LayoutKey(c, "Y").c_str(), layout[c].center.y);
    }
    SetBool(kLayoutCustom, true);
    CVarSave();
}

void ClearLayout() {
    for (size_t i = 0; i < kControlCount; ++i) {
        const Control c = static_cast<Control>(i);
        CVarClear(LayoutKey(c, "X").c_str());
        CVarClear(LayoutKey(c, "Y").c_str());
    }
    CVarClear(kLayoutCustom);
    CVarSave();
}

void ApplyRecommendedGameDefaultsOnce() {
    if (CVarGetInteger(kDefaultsApplied, 0) != 0) {
        return;
    }

    // Free camera lets the right stick (our touch-look) orbit the camera; without it touch-look does nothing outside
    // of first-person aiming. The vanilla camera speed is slow for a swipe, so give it a bit more.
    constexpr int32_t kUnsetInt = -1;
    constexpr float kUnsetCameraSpeed = -999.0f;
    if (CVarGetInteger("gFreeCamera", kUnsetInt) == kUnsetInt) {
        CVarSetInteger("gFreeCamera", 1);
    }
    for (const char* name : { "gThirdPersonCameraSensitivityX", "gThirdPersonCameraSensitivityY" }) {
        if (CVarGetFloat(name, kUnsetCameraSpeed) == kUnsetCameraSpeed) {
            CVarSetFloat(name, 2.0f);
        }
    }

    CVarSetInteger(kDefaultsApplied, 1);
    CVarSave();
}

} // namespace Prefs
} // namespace SohTouch
