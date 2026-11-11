// Host-side unit tests for the touch core.  Build and run:  sh tests/touch-controls/run_tests.sh
#include "TouchCore.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

using namespace SohTouch;

static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++gChecks;                                                               \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++gFailures;                                                         \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                         \
    do {                                                                                              \
        ++gChecks;                                                                                    \
        if (std::fabs((a) - (b)) > (eps)) {                                                           \
            std::printf("FAIL %s:%d  %s (%f) != %s (%f)\n", __FILE__, __LINE__, #a, (double)(a), #b, \
                        (double)(b));                                                                 \
            ++gFailures;                                                                              \
        }                                                                                             \
    } while (0)

static const float W = 2400.0f;
static const float H = 1080.0f;

struct Rig {
    Engine e;
    Rig(float w = W, float h = H) {
        e.SetScreen(w, h);
        e.SetLayout(Layout::Default(w / h));
        e.SetMode(Mode::Play);
        w_ = w;
        h_ = h;
    }
    Finger At(int64_t id, float px, float py) const {
        return Finger{ id, px / w_, py / h_ };
    }
    Finger OnCenter(int64_t id, Control c, float dx = 0, float dy = 0) const {
        const Resolved& g = e.Geometry(c);
        return At(id, g.center.x + dx, g.center.y + dy);
    }
    void Step(std::initializer_list<Finger> fingers) {
        std::vector<Finger> v(fingers);
        e.Update(v.data(), v.size());
    }
    float w_, h_;
};

static void TapAndRelease() {
    Rig r;
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(r.e.IsPressed(Control::A));
    CHECK(!r.e.IsPressed(Control::B));
    CHECK((r.e.TakePressEdges() & Bit(Control::A)) != 0);

    r.Step({ r.OnCenter(1, Control::A) }); // still held: no new edge
    CHECK(r.e.TakePressEdges() == 0);

    r.Step({});
    CHECK(!r.e.IsPressed(Control::A));
}

static void EveryButtonIsReachable() {
    for (Control c : { Control::A, Control::B, Control::CUp, Control::CDown, Control::CLeft, Control::CRight,
                       Control::Z, Control::R, Control::L, Control::Start }) {
        Rig r;
        r.Step({ r.OnCenter(1, c) });
        CHECK(r.e.IsPressed(c));
    }
}

static void StickDeflection() {
    Rig r;
    const float R = r.e.Geometry(Control::Stick).radius;

    r.Step({ r.OnCenter(1, Control::Stick) });
    CHECK(r.e.StickActive());
    CHECK_NEAR(r.e.StickVector().x, 0.0f, 1e-4f);

    r.Step({ r.OnCenter(1, Control::Stick, R * 0.55f, 0) });
    CHECK_NEAR(r.e.StickVector().x, (0.55f - 0.10f) / 0.90f, 1e-3f);
    CHECK_NEAR(r.e.StickVector().y, 0.0f, 1e-4f);

    r.Step({ r.OnCenter(1, Control::Stick, R * 3.0f, 0) }); // far past the rim: clamped
    CHECK_NEAR(r.e.StickVector().x, 1.0f, 1e-3f);

    r.Step({ r.OnCenter(1, Control::Stick, 0, -R * 0.9f) }); // up on screen = +y for the game
    CHECK(r.e.StickVector().y > 0.8f);

    r.Step({ r.OnCenter(1, Control::Stick, R * 0.05f, 0) }); // inside the deadzone
    CHECK(r.e.StickVector().x == 0.0f && r.e.StickVector().y == 0.0f);

    r.Step({});
    CHECK(!r.e.StickActive());
    CHECK(r.e.StickVector().x == 0.0f && r.e.StickVector().y == 0.0f);
}

static void StickFingerNeverPressesButtons() {
    Rig r;
    r.Step({ r.OnCenter(1, Control::Stick) });
    // Drag the same finger across the screen right over the A button.
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(!r.e.IsPressed(Control::A));
    CHECK(r.e.StickActive());
}

static void ThumbRollsBetweenButtons() {
    Rig r;
    r.Step({ r.OnCenter(1, Control::B) });
    CHECK(r.e.IsPressed(Control::B));
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(r.e.IsPressed(Control::A));
    CHECK(!r.e.IsPressed(Control::B));
    r.Step({ r.At(1, W * 0.5f, H * 0.6f) }); // slid off onto empty space: nothing held, not a camera drag either
    CHECK(!r.e.IsPressed(Control::A));
    CHECK(r.e.TakeLook().x == 0.0f);
}

static void LookAccumulatesInScreenHeights() {
    Rig r;
    r.Step({ r.At(7, 1200, 600) });
    CHECK(r.e.TakeLook().x == 0.0f); // first frame has no delta

    r.Step({ r.At(7, 1308, 546) }); // +108 px right, -54 px up
    Vec2 look = r.e.TakeLook();
    CHECK_NEAR(look.x, 0.10f, 1e-4f);
    CHECK_NEAR(look.y, -0.05f, 1e-4f);

    look = r.e.TakeLook(); // read clears the accumulator
    CHECK(look.x == 0.0f && look.y == 0.0f);

    r.Step({ r.At(7, 1308 + 54, 546) });
    r.Step({ r.At(7, 1308 + 108, 546) });
    CHECK_NEAR(r.e.TakeLook().x, 0.10f, 1e-4f); // two frames accumulate before one read
}

static void LookIgnoresTeleports() {
    Rig r;
    r.Step({ r.At(7, 1200, 600) });
    r.Step({ r.At(7, 100, 100) }); // jump of more than half the screen in one frame
    Vec2 look = r.e.TakeLook();
    CHECK(look.x == 0.0f && look.y == 0.0f);
}

static void StickAndLookAtTheSameTime() {
    Rig r;
    const float R = r.e.Geometry(Control::Stick).radius;
    r.Step({ r.OnCenter(1, Control::Stick), r.At(2, 1500, 600) });
    r.Step({ r.OnCenter(1, Control::Stick, R * 0.8f, 0), r.At(2, 1600, 600) });
    CHECK(r.e.StickVector().x > 0.5f);
    CHECK_NEAR(r.e.TakeLook().x, 100.0f / H, 1e-4f);
    // lifting the look finger leaves the stick alone
    r.Step({ r.OnCenter(1, Control::Stick, R * 0.8f, 0) });
    CHECK(r.e.StickActive());
}

static void FloatingStickAnchorsAndFollows() {
    Rig r;
    Settings s;
    s.floatingStick = true;
    r.e.SetSettings(s);
    const float R = r.e.Geometry(Control::Stick).radius;

    // A touch on the left side that is not on a button grabs the stick wherever it lands.
    r.Step({ r.At(1, 700, 800) });
    CHECK(r.e.StickActive());
    CHECK_NEAR(r.e.StickCenterPx().x, 700.0f, 1e-3f);
    CHECK_NEAR(r.e.StickVector().x, 0.0f, 1e-4f);

    r.Step({ r.At(1, 700 + R * 0.55f, 800) });
    CHECK_NEAR(r.e.StickVector().x, (0.55f - 0.10f) / 0.90f, 1e-3f);

    r.Step({ r.At(1, 700 + R * 3.0f, 800) }); // the base follows the thumb
    CHECK_NEAR(r.e.StickVector().x, 1.0f, 1e-3f);
    CHECK_NEAR(r.e.StickCenterPx().x, 700 + R * 2.0f, 1.0f);

    r.Step({});
    CHECK(!r.e.StickActive());
    CHECK_NEAR(r.e.StickCenterPx().x, r.e.Geometry(Control::Stick).center.x, 1e-3f); // back home

    // The right half of the screen is still camera-look.
    r.Step({ r.At(2, 1500, 600) });
    r.Step({ r.At(2, 1560, 600) });
    CHECK(!r.e.StickActive());
    CHECK(r.e.TakeLook().x > 0.0f);
}

static void SecondFingerCannotStealTheStick() {
    Rig r;
    Settings s;
    s.floatingStick = true;
    r.e.SetSettings(s);
    r.Step({ r.At(1, 600, 800), r.At(2, 800, 800) });
    CHECK(r.e.StickActive());
    CHECK_NEAR(r.e.StickCenterPx().x, 600.0f, 1e-3f); // first finger owns it
    r.Step({ r.At(1, 600, 800), r.At(2, 900, 800) });
    CHECK(r.e.TakeLook().x > 0.0f); // the second finger became a look finger
}

static void LeftHandedMirrors() {
    Rig r;
    Settings s;
    s.leftHanded = true;
    r.e.SetSettings(s);
    const Resolved& a = r.e.Geometry(Control::A);
    CHECK(a.center.x < W * 0.5f); // A is on the left now
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(r.e.IsPressed(Control::A));
    const Resolved& st = r.e.Geometry(Control::Stick);
    CHECK(st.center.x > W * 0.5f);
}

static void TapControls() {
    Rig r;
    r.Step({ r.OnCenter(1, Control::Menu) });
    CHECK((r.e.TakeTaps() & Bit(Control::Menu)) != 0);
    r.Step({ r.OnCenter(1, Control::Menu), });
    CHECK(r.e.TakeTaps() == 0); // fires once per touch
    r.Step({ r.At(1, 1000, 100) }); // dragging off it does not turn into a camera drag
    CHECK(r.e.TakeLook().x == 0.0f);
    r.Step({});
    r.Step({ r.OnCenter(2, Control::Toggle) });
    CHECK((r.e.TakeTaps() & Bit(Control::Toggle)) != 0);
}

static void BlockedRectIsIgnored() {
    Rig r;
    r.e.SetBlockedRect(1000, 400, 1400, 700);
    r.Step({ r.At(1, 1100, 500) });
    r.Step({ r.At(1, 1200, 500) });
    CHECK(r.e.TakeLook().x == 0.0f);
    r.e.ClearBlockedRect();
    r.Step({ r.At(2, 1100, 500) });
    r.Step({ r.At(2, 1200, 500) });
    CHECK(r.e.TakeLook().x > 0.0f);
}

static void ModesGateInput() {
    {
        Rig r;
        r.e.SetMode(Mode::MenuOpen);
        r.Step({ r.OnCenter(1, Control::A) });
        CHECK(!r.e.IsPressed(Control::A));
        r.Step({ r.OnCenter(1, Control::A), r.OnCenter(2, Control::CloseMenu) });
        CHECK((r.e.TakeTaps() & Bit(Control::CloseMenu)) != 0);
        CHECK(!r.e.Geometry(Control::A).visible);
        CHECK(r.e.Geometry(Control::CloseMenu).visible);
    }
    {
        Rig r;
        r.e.SetMode(Mode::Hidden);
        r.Step({ r.At(1, 1200, 600) });
        r.Step({ r.At(1, 1300, 600) });
        CHECK(r.e.TakeLook().x == 0.0f); // hidden controls do not look either
        r.Step({ r.OnCenter(2, Control::Toggle) });
        CHECK((r.e.TakeTaps() & Bit(Control::Toggle)) != 0);
    }
    {
        Rig r;
        r.e.SetMode(Mode::Passive);
        r.Step({ r.OnCenter(1, Control::A) });
        CHECK(!r.e.IsPressed(Control::A));
        CHECK(r.e.Geometry(Control::A).visible); // still drawn, just inert
        CHECK(!r.e.Geometry(Control::A).active);
    }
}

static void ModeChangeReleasesFingers() {
    Rig r;
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(r.e.IsPressed(Control::A));
    r.e.SetMode(Mode::MenuOpen);
    CHECK(!r.e.IsPressed(Control::A));
    r.e.SetMode(Mode::Play);
    r.Step({ r.OnCenter(1, Control::A) }); // same finger id is treated as a fresh touch
    CHECK(r.e.IsPressed(Control::A));
}

static void SettingsAreSafeToReapplyEveryFrame() {
    Rig r;
    Settings s;
    r.e.SetSettings(s);
    r.Step({ r.OnCenter(1, Control::A) });
    r.e.SetSettings(s); // identical: must not drop the finger
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(r.e.IsPressed(Control::A));
    s.stickDeadzone = 0.2f; // a deadzone tweak does not move anything either
    r.e.SetSettings(s);
    r.Step({ r.OnCenter(1, Control::A) });
    CHECK(r.e.IsPressed(Control::A));
}

static void EditModeDragsAndClamps() {
    Rig r;
    r.e.SetMode(Mode::Edit);
    const Vec2 before = r.e.GetLayout()[Control::A].center;

    r.Step({ r.OnCenter(1, Control::A) });
    r.Step({ r.OnCenter(1, Control::A, -300, -200) });
    const Vec2 after = r.e.GetLayout()[Control::A].center;
    CHECK_NEAR(after.x, before.x - 300.0f / W, 1e-3f);
    CHECK_NEAR(after.y, before.y - 200.0f / H, 1e-3f);
    CHECK(!r.e.TakeLayoutChanged()); // not until the finger lifts
    r.Step({});
    CHECK(r.e.TakeLayoutChanged());
    CHECK(!r.e.TakeLayoutChanged());
    CHECK(!r.e.IsPressed(Control::A)); // editing never presses anything

    // Dragging far off screen keeps the control fully visible.
    r.Step({ r.OnCenter(2, Control::B) });
    r.Step({ r.At(2, 5000, 5000) });
    const Resolved& g = r.e.Geometry(Control::B);
    CHECK(g.center.x + g.radius <= W + 0.5f);
    CHECK(g.center.y + g.radius <= H + 0.5f);
}

static void EditModeKeepsLayoutRightHanded() {
    Rig r;
    Settings s;
    s.leftHanded = true;
    r.e.SetSettings(s);
    r.e.SetMode(Mode::Edit);
    const float xBefore = r.e.GetLayout()[Control::A].center.x;
    r.Step({ r.OnCenter(1, Control::A) });
    r.Step({ r.OnCenter(1, Control::A, 100, 0) }); // dragged right on screen = left in the stored layout
    CHECK_NEAR(r.e.GetLayout()[Control::A].center.x, xBefore - 100.0f / W, 1e-3f);
    r.Step({});
}

static void ScaleResizesControlsButNotUiButtons() {
    Rig r;
    const float a0 = r.e.Geometry(Control::A).radius;
    const float m0 = r.e.Geometry(Control::Menu).radius;
    Settings s;
    s.scale = 1.5f;
    r.e.SetSettings(s);
    CHECK_NEAR(r.e.Geometry(Control::A).radius, a0 * 1.5f, 0.01f);
    CHECK_NEAR(r.e.Geometry(Control::Menu).radius, m0, 0.01f);
}

static void DpadOnlyWhenEnabled() {
    Rig r;
    CHECK(!r.e.Geometry(Control::DUp).visible);
    Settings s;
    s.showDpad = true;
    r.e.SetSettings(s);
    CHECK(r.e.Geometry(Control::DUp).visible);
    r.Step({ r.OnCenter(1, Control::DLeft) });
    CHECK(r.e.IsPressed(Control::DLeft));
}

static void DefaultLayoutHasNoOverlapsAndStaysOnScreen() {
    const float aspects[] = { 1.6f, 16.0f / 9.0f, 2.0f, 19.5f / 9.0f, 20.0f / 9.0f, 21.0f / 9.0f, 2.4f };
    for (float aspect : aspects) {
        const float h = 1000.0f;
        const float w = h * aspect;
        for (bool left : { false, true }) {
            Rig r(w, h);
            Settings s;
            s.showDpad = true;
            s.leftHanded = left;
            r.e.SetSettings(s);
            for (size_t i = 0; i < kControlCount; ++i) {
                const Resolved& a = r.e.Geometry(static_cast<Control>(i));
                if (!a.visible) {
                    continue;
                }
                CHECK(a.center.x - a.radius >= -0.5f && a.center.x + a.radius <= w + 0.5f);
                CHECK(a.center.y - a.radius >= -0.5f && a.center.y + a.radius <= h + 0.5f);
                for (size_t j = i + 1; j < kControlCount; ++j) {
                    const Resolved& b = r.e.Geometry(static_cast<Control>(j));
                    if (!b.visible) {
                        continue;
                    }
                    const float d = std::hypot(a.center.x - b.center.x, a.center.y - b.center.y);
                    if (d < a.radius + b.radius) {
                        std::printf("overlap at aspect %.2f (left=%d): %s vs %s (dist %.1f < %.1f)\n", aspect, left,
                                    ControlName(static_cast<Control>(i)), ControlName(static_cast<Control>(j)), d,
                                    a.radius + b.radius);
                    }
                    CHECK(d >= a.radius + b.radius);
                }
            }
        }
    }
}

static void NamesAreUniqueAndStable() {
    for (size_t i = 0; i < kControlCount; ++i) {
        const char* a = ControlName(static_cast<Control>(i));
        CHECK(a != nullptr && a[0] != '\0');
        for (size_t j = i + 1; j < kControlCount; ++j) {
            const char* b = ControlName(static_cast<Control>(j));
            bool same = true;
            for (size_t k = 0;; ++k) {
                if (a[k] != b[k]) {
                    same = false;
                    break;
                }
                if (a[k] == '\0') {
                    break;
                }
            }
            CHECK(!same);
        }
    }
    CHECK(KindOf(Control::Stick) == Kind::Stick);
    CHECK(KindOf(Control::A) == Kind::Button);
    CHECK(KindOf(Control::Menu) == Kind::Tap);
}

static void ResetClearsEverything() {
    Rig r;
    const float R = r.e.Geometry(Control::Stick).radius;
    r.Step({ r.OnCenter(1, Control::Stick, R, 0), r.OnCenter(2, Control::A), r.At(3, 1200, 600) });
    r.Step({ r.OnCenter(1, Control::Stick, R, 0), r.OnCenter(2, Control::A), r.At(3, 1250, 600) });
    r.e.Reset();
    CHECK(!r.e.StickActive());
    CHECK(!r.e.IsPressed(Control::A));
    CHECK(r.e.StickVector().x == 0.0f);
    CHECK(r.e.TakeLook().x == 0.0f);
    CHECK(r.e.TakePressEdges() == 0);
}


static void LookMappingSigns() {
    LookParams game; // game defaults: invert X off, invert Y on
    const float dt = 0.05f;

    // Drag right: look right.
    LookOutput o = MapLook({ 0.05f, 0.0f }, dt, game);
    CHECK(o.rightStickX > 0.0f);
    CHECK(o.gyroY < 0.0f); // positive gyro Y turns left, so right is negative
    CHECK_NEAR(o.rightStickY, 0.0f, 1e-6f);

    // Drag up: look up. The game's default "invert Y" makes stick-up look down, so we send stick-down...
    o = MapLook({ 0.0f, -0.05f }, dt, game);
    CHECK(o.rightStickY < 0.0f);
    CHECK(o.gyroX > 0.0f); // ...and positive gyro X looks up.

    // With the game's invert-Y option off the stick is natural, so drag up = stick up.
    LookParams natural = game;
    natural.gameInvertsY = false;
    o = MapLook({ 0.0f, -0.05f }, dt, natural);
    CHECK(o.rightStickY > 0.0f);
    CHECK(o.gyroX > 0.0f); // gyro does not depend on the game's invert options

    // The game's invert-X option is compensated; the gyro path is not affected.
    LookParams invertX = game;
    invertX.gameInvertsX = true;
    o = MapLook({ 0.05f, 0.0f }, dt, invertX);
    CHECK(o.rightStickX < 0.0f);
    CHECK(o.gyroY < 0.0f);

    // The player's own "invert look X" flips horizontal look on both paths, and only horizontal.
    LookParams flippedX = game;
    flippedX.invertX = true;
    o = MapLook({ 0.05f, -0.05f }, dt, flippedX);
    CHECK(o.rightStickX < 0.0f && o.gyroY > 0.0f);
    CHECK(o.rightStickY < 0.0f && o.gyroX > 0.0f);

    // The player's own "invert look Y" flips vertical look on both paths, and only vertical.
    LookParams flipped = game;
    flipped.invertY = true;
    o = MapLook({ 0.05f, -0.05f }, dt, flipped);
    CHECK(o.rightStickX > 0.0f && o.gyroY < 0.0f);
    CHECK(o.rightStickY > 0.0f && o.gyroX < 0.0f);
}

static void LookMappingScaling() {
    LookParams p;
    const LookOutput base = MapLook({ 0.02f, 0.0f }, 0.05f, p);

    // The stick is a rate: the same drag in half the time is twice as fast.
    const LookOutput fast = MapLook({ 0.02f, 0.0f }, 0.025f, p);
    CHECK_NEAR(fast.rightStickX, base.rightStickX * 2.0f, 1e-3f);
    // The gyro channels are an amount: same drag, same turn, whatever the time.
    CHECK_NEAR(fast.gyroY, base.gyroY, 1e-6f);

    // Sensitivity scales both.
    p.sensitivity = 2.0f;
    const LookOutput twice = MapLook({ 0.02f, 0.0f }, 0.05f, p);
    CHECK_NEAR(twice.rightStickX, base.rightStickX * 2.0f, 1e-3f);
    CHECK_NEAR(twice.gyroY, base.gyroY * 2.0f, 1e-6f);

    // A full-height drag turns the first-person view by roughly 100 degrees (one gyro unit is 750 / 65536 * 360 deg).
    LookParams one;
    const LookOutput full = MapLook({ 0.0f, 1.0f }, 0.05f, one);
    const float degrees = std::fabs(full.gyroX) * 750.0f / 65536.0f * 360.0f;
    CHECK(degrees > 90.0f && degrees < 110.0f);

    // No drag, no output; a zero time step cannot divide by zero.
    const LookOutput none = MapLook({ 0.0f, 0.0f }, 0.0f, one);
    CHECK(none.rightStickX == 0.0f && none.rightStickY == 0.0f && none.gyroX == 0.0f && none.gyroY == 0.0f);
    const LookOutput finite = MapLook({ 0.01f, 0.01f }, 0.0f, one);
    CHECK(std::isfinite(finite.rightStickX) && std::isfinite(finite.rightStickY));
}

static void LookMappingRightStickAiming() {
    LookParams p;
    p.rightStickAiming = true; // first person reads the right stick, so the gyro channels must stay silent
    const LookOutput o = MapLook({ 0.05f, -0.05f }, 0.05f, p);
    CHECK(o.rightStickX != 0.0f && o.rightStickY != 0.0f);
    CHECK(o.gyroX == 0.0f && o.gyroY == 0.0f);
}

static void FullSwipeSaturatesTheStickButSlowDragDoesNot() {
    LookParams p;
    // A brisk swipe (1.2 screen heights per second) pushes well past the stick's +-85 range...
    CHECK(MapLook({ 0.06f, 0.0f }, 0.05f, p).rightStickX > 85.0f);
    // ...while a slow, careful drag (0.2 heights per second) stays a gentle fraction of it.
    const float slow = MapLook({ 0.01f, 0.0f }, 0.05f, p).rightStickX;
    CHECK(slow > 10.0f && slow < 40.0f);
}

int main() {
    TapAndRelease();
    EveryButtonIsReachable();
    StickDeflection();
    StickFingerNeverPressesButtons();
    ThumbRollsBetweenButtons();
    LookAccumulatesInScreenHeights();
    LookIgnoresTeleports();
    StickAndLookAtTheSameTime();
    FloatingStickAnchorsAndFollows();
    SecondFingerCannotStealTheStick();
    LeftHandedMirrors();
    TapControls();
    BlockedRectIsIgnored();
    ModesGateInput();
    ModeChangeReleasesFingers();
    SettingsAreSafeToReapplyEveryFrame();
    EditModeDragsAndClamps();
    EditModeKeepsLayoutRightHanded();
    ScaleResizesControlsButNotUiButtons();
    DpadOnlyWhenEnabled();
    DefaultLayoutHasNoOverlapsAndStaysOnScreen();
    NamesAreUniqueAndStable();
    ResetClearsEverything();
    LookMappingSigns();
    LookMappingScaling();
    LookMappingRightStickAiming();
    FullSwipeSaturatesTheStickButSlowDragDoesNot();

    std::printf("%d checks, %d failure(s)\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
