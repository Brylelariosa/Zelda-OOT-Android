#pragma once

// Everything that is drawn with ImGui for the touch controls: the controls themselves, the layout-editing toolbar and
// the settings window contents. Must be called from inside an active ImGui frame.

#ifdef __ANDROID__

#include "TouchCore.h"
#include "TouchSettings.h"

namespace SohTouch {

// Draws the controls above everything else in the frame.
void DrawControls(const Engine& engine, float opacity);

enum class EditAction { None, Done, Reset };

// Small panel shown while the layout is being edited. `blocked` receives its rectangle (x0, y0, x1, y1 in pixels) so
// touches on the panel are not mistaken for drags.
EditAction DrawEditToolbar(float screenWidth, float screenHeight, float blocked[4]);

struct SettingsActions {
    bool changed = false;     // a touch-control value changed: apply it
    bool flush = false;       // a value was committed: write the config file
    bool startEdit = false;   // "Edit layout" was pressed
    bool resetLayout = false; // "Reset layout" was pressed
};

// The widgets inside the settings window (the caller owns Begin/End).
SettingsActions DrawSettingsBody(UserSettings& settings);

} // namespace SohTouch

#endif // __ANDROID__
