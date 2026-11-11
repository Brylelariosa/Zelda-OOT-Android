#pragma once

#include "TouchCore.h"

namespace SohTouch {

// Everything the player can tune about the on-screen controls. Persisted through console variables ("gTouch.*") so it
// lands in the same config file as the rest of the game's settings.
struct UserSettings {
    bool enabled = true;
    bool autoHide = true; // hide the controls while a game controller is connected
    bool hidden = false;  // the player tapped the hide button
    float opacity = 0.55f;
    float scale = 1.0f;
    bool leftHanded = false;
    bool floatingStick = false;
    bool showDpad = false;
    bool haptics = true;
    float lookSensitivity = 1.0f;
    bool invertLookX = false;
    bool invertLookY = false;
    bool gyroAiming = false;
    float gyroSensitivity = 1.0f;

    Settings ToEngine() const;
};

namespace Prefs {

UserSettings Load();
// Writes every value to its console variable. Does not flush to disk; call CVarSave() for that.
void Save(const UserSettings& settings);

// Customised control positions. Controls the player never moved keep their default position.
bool HasCustomLayout();
void LoadLayout(Layout& layout);
void SaveLayout(const Layout& layout); // flushes to disk
void ClearLayout();                    // flushes to disk

// On first run, switch on the game options that make touch-look useful (free camera, camera speed) unless the player
// already chose a value. Never touches anything again afterwards.
void ApplyRecommendedGameDefaultsOnce();

} // namespace Prefs
} // namespace SohTouch
