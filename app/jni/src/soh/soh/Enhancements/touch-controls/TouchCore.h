#pragma once

// Pure, dependency-free logic behind the on-screen touch controls: layout, hit-testing and the mapping from raw
// fingers to buttons / analog stick / camera-look. Nothing in here knows about SDL, ImGui or the game, which keeps it
// portable and unit-testable on a desktop (see tests/touch_core_test.cpp).

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace SohTouch {

enum class Control : uint8_t {
    Stick,
    A,
    B,
    CUp,
    CDown,
    CLeft,
    CRight,
    Z,
    R,
    L,
    Start,
    DUp,
    DDown,
    DLeft,
    DRight,
    Menu,      // opens the game menu
    Toggle,    // hides / shows the on-screen controls
    CloseMenu, // closes the game menu (only offered while it is open)
    Count
};

constexpr size_t kControlCount = static_cast<size_t>(Control::Count);

constexpr size_t Index(Control c) {
    return static_cast<size_t>(c);
}

constexpr uint32_t Bit(Control c) {
    return 1u << static_cast<uint32_t>(c);
}

enum class Kind : uint8_t {
    Stick,  // analog stick
    Button, // held while a finger is on it
    Tap     // fires once when touched (menu / UI buttons)
};

// Stable identifier used when persisting the layout ("A", "CUp", ...).
const char* ControlName(Control c);
Kind KindOf(Control c);

enum class Mode : uint8_t {
    Play,     // normal gameplay: everything reacts
    MenuOpen, // the game menu is open: only CloseMenu reacts
    Passive,  // controls are drawn but ignore touches (settings window is open)
    Hidden,   // the user hid the controls: only Toggle reacts
    Edit      // layout editing: touches drag controls around
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct ControlLayout {
    Vec2 center;         // normalised: 0..1 of the screen width / height (right-handed layout)
    float radius = 0.1f; // in screen heights, before the user's size scale is applied
};

struct Layout {
    std::array<ControlLayout, kControlCount> controls{};

    // Sensible layout for a landscape screen of the given aspect ratio (width / height).
    static Layout Default(float aspect);

    ControlLayout& operator[](Control c) {
        return controls[Index(c)];
    }
    const ControlLayout& operator[](Control c) const {
        return controls[Index(c)];
    }
};

struct Settings {
    float scale = 1.0f; // multiplies every control radius
    bool leftHanded = false;
    bool floatingStick = false; // the stick appears wherever the thumb lands
    bool showDpad = false;
    float stickDeadzone = 0.10f; // 0..0.9 of the stick radius
};

// One finger as reported by the platform: id is stable while the finger is down, x/y are normalised 0..1.
struct Finger {
    int64_t id = 0;
    float x = 0.0f;
    float y = 0.0f;
};

// A control resolved to pixels for the current screen / settings / mode.
struct Resolved {
    Vec2 center;
    float radius = 0.0f;
    bool visible = false;
    bool active = false; // reacts to touches
};

class Engine {
  public:
    Engine();

    void SetScreen(float widthPx, float heightPx);
    void SetLayout(const Layout& layout);
    void SetSettings(const Settings& settings);
    void SetMode(Mode mode);
    // Touches that start inside this rectangle (pixels) are ignored, e.g. a dialog drawn above the controls.
    void SetBlockedRect(float x0, float y0, float x1, float y1);
    void ClearBlockedRect();

    // Feed the fingers currently touching the screen. Call once per rendered frame.
    void Update(const Finger* fingers, size_t count);
    // Forget every finger and output (focus loss, controls turned off, ...).
    void Reset();

    const Layout& GetLayout() const {
        return mLayout;
    }
    const Settings& GetSettings() const {
        return mSettings;
    }
    Mode GetMode() const {
        return mMode;
    }
    const Resolved& Geometry(Control c) const {
        return mGeometry[Index(c)];
    }

    bool IsPressed(Control c) const {
        return mPressed[Index(c)];
    }
    // Stick output in -1..1 with the deadzone applied; +y is up (the game's convention).
    Vec2 StickVector() const {
        return mStickOut;
    }
    // Knob offset in -1..1 for drawing; +y is down (screen convention).
    Vec2 StickKnob() const {
        return mStickKnob;
    }
    bool StickActive() const {
        return mStickActive;
    }
    // Centre of the stick base right now (differs from the layout while a floating stick is being held).
    Vec2 StickCenterPx() const;

    // Camera-look drag accumulated since the last call, in screen heights (+x right, +y down).
    Vec2 TakeLook();
    // Controls that went down since the last call (bitmask of Bit(Control)).
    uint32_t TakePressEdges();
    // Tap controls touched since the last call (bitmask of Bit(Control)).
    uint32_t TakeTaps();
    // True once after an edit-mode drag has finished.
    bool TakeLayoutChanged();

  private:
    enum class Bind : uint8_t { Ignore, Look, Button, Stick, Drag };

    struct Binding {
        int64_t id = 0;
        Bind kind = Bind::Ignore;
        Vec2 last;                         // previous position in pixels (Look)
        Vec2 grab;                         // centre minus finger at touch-down (Drag)
        Control control = Control::Count;  // dragged control (Drag)
    };

    void Resolve();
    bool IsVisibleInMode(Control c) const;
    bool FindControl(Vec2 p, float grow, uint32_t kindMask, Control& out) const;
    Binding Classify(int64_t id, Vec2 p);
    void UpdateStick(Vec2 p);
    void MoveControl(const Binding& b, Vec2 p);
    void ReleaseBinding(const Binding& b);
    void DropBindings();
    bool InBlockedRect(Vec2 p) const;
    float MinDim() const;

    float mWidth = 1920.0f;
    float mHeight = 1080.0f;
    Layout mLayout = Layout::Default(1920.0f / 1080.0f);
    Settings mSettings;
    Mode mMode = Mode::Play;
    std::array<Resolved, kControlCount> mGeometry{};

    bool mHasBlockedRect = false;
    float mBlockedRect[4] = { 0, 0, 0, 0 };

    std::vector<Binding> mBindings;
    std::array<bool, kControlCount> mPressed{};

    Vec2 mStickCenter;
    Vec2 mStickOut;
    Vec2 mStickKnob;
    bool mStickActive = false;

    Vec2 mLook;
    uint32_t mEdges = 0;
    uint32_t mTaps = 0;
    bool mLayoutChanged = false;
};

// ---------------------------------------------------------------------------------------------------------------------
// Camera look: how a finger drag becomes the two ways the game can be steered.
//
//   * the right stick  - orbits the camera when the game's "free camera" option is on (third person)
//   * the gyro channels - turn the view while aiming in first person (slingshot, bow, C-up look, ...)
//
// The game reads each one only in its own situation, so feeding both never double-turns. The exception is the game's
// "right stick aiming" option, which makes first person read the right stick too; the gyro channels stay silent then.
//
// Signs: dragging right looks right and dragging up looks up, whatever the player chose for the game's own camera-invert
// options (those are compensated for here). The player's "Invert look X / Y" preferences flip the result back.
// ---------------------------------------------------------------------------------------------------------------------

// Right-stick units per (screen height / second) of finger speed, at sensitivity 1.
constexpr float kStickPerHeightPerSecond = 100.0f;
// First-person "gyro" units per screen height dragged, at sensitivity 1. One unit is about 4.1 degrees in the game, so
// a drag across the full height of the screen turns the view by a little under 100 degrees.
constexpr float kGyroPerHeight = 24.0f;

struct LookParams {
    float sensitivity = 1.0f;
    bool invertX = false;          // the player's own preferences
    bool invertY = false;
    bool gameInvertsX = false;     // the game's "invert camera X" option
    bool gameInvertsY = true;      // the game's "invert camera Y" option (on by default in the game)
    bool rightStickAiming = false; // the game's "right stick aiming" option
};

struct LookOutput {
    float rightStickX = 0.0f; // stick units, not yet limited to the stick's range
    float rightStickY = 0.0f; // +y = stick up, like the game
    float gyroX = 0.0f;
    float gyroY = 0.0f;
};

// `drag` is the finger movement since the last call in screen heights (+x right, +y down) and `seconds` the time that
// took.
LookOutput MapLook(Vec2 drag, float seconds, const LookParams& params);

} // namespace SohTouch
