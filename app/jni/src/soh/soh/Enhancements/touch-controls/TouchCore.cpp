#include "TouchCore.h"

#include <algorithm>
#include <cmath>

namespace SohTouch {

namespace {

// How much larger than the drawn shape a control reacts to touches. Thumbs are imprecise.
constexpr float kButtonHitGrow = 1.15f;
constexpr float kTapHitGrow = 1.25f;
constexpr float kStickGrabGrow = 1.6f;

// Floating stick: touches in this part of the screen (on the stick side, below the top strip) grab the stick.
constexpr float kFloatZoneWidth = 0.42f;
constexpr float kFloatZoneTop = 0.20f;

// A look finger that "teleports" further than this in one frame is treated as noise.
constexpr float kMaxLookStep = 0.6f;

struct Info {
    const char* name;
    Kind kind;
};

// Order must match enum Control.
constexpr Info kInfo[kControlCount] = {
    { "Stick", Kind::Stick },   { "A", Kind::Button },      { "B", Kind::Button },      { "CUp", Kind::Button },
    { "CDown", Kind::Button },  { "CLeft", Kind::Button },  { "CRight", Kind::Button }, { "Z", Kind::Button },
    { "R", Kind::Button },      { "L", Kind::Button },      { "Start", Kind::Button },  { "DUp", Kind::Button },
    { "DDown", Kind::Button },  { "DLeft", Kind::Button },  { "DRight", Kind::Button }, { "Menu", Kind::Tap },
    { "Toggle", Kind::Tap },    { "CloseMenu", Kind::Tap },
};

constexpr uint32_t KindBit(Kind k) {
    return 1u << static_cast<uint32_t>(k);
}

constexpr uint32_t kAllKinds = KindBit(Kind::Stick) | KindBit(Kind::Button) | KindBit(Kind::Tap);

bool IsDpad(Control c) {
    return c == Control::DUp || c == Control::DDown || c == Control::DLeft || c == Control::DRight;
}

float Distance(Vec2 a, Vec2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

} // namespace

const char* ControlName(Control c) {
    return Index(c) < kControlCount ? kInfo[Index(c)].name : "";
}

Kind KindOf(Control c) {
    return Index(c) < kControlCount ? kInfo[Index(c)].kind : Kind::Button;
}

Layout Layout::Default(float aspect) {
    // Work in "screen heights": 1.0 is the full height of the screen, so sizes feel the same on every phone.
    // Left-hand controls are measured from the left edge, right-hand ones from the right edge (a - x).
    const float a = std::clamp(aspect, 1.0f, 3.0f);
    Layout layout;
    auto put = [&](Control c, float xUnits, float yUnits, float radius) {
        layout[c].center = { xUnits / a, yUnits };
        layout[c].radius = radius;
    };

    // Left thumb: stick, with Z / L within reach of the index finger above it.
    put(Control::Stick, 0.31f, 0.68f, 0.17f);
    put(Control::Z, 0.14f, 0.16f, 0.080f);
    put(Control::L, 0.34f, 0.12f, 0.065f);

    // Right thumb: A / B at the bottom, C-buttons above them, R in the corner.
    put(Control::A, a - 0.200f, 0.70f, 0.105f);
    put(Control::B, a - 0.405f, 0.77f, 0.078f);
    const float cx = a - 0.27f;
    const float cy = 0.33f;
    const float cs = 0.125f;
    const float cr = 0.068f;
    put(Control::CUp, cx, cy - cs, cr);
    put(Control::CDown, cx, cy + cs, cr);
    put(Control::CLeft, cx - cs, cy, cr);
    put(Control::CRight, cx + cs, cy, cr);
    put(Control::R, a - 0.12f, 0.12f, 0.075f);

    // Top centre strip.
    put(Control::Menu, a * 0.5f - 0.15f, 0.085f, 0.045f);
    put(Control::Start, a * 0.5f, 0.085f, 0.050f);
    put(Control::Toggle, a * 0.5f + 0.15f, 0.085f, 0.045f);

    // Only shown while the game menu is open: out of the way of the menu bar.
    put(Control::CloseMenu, a - 0.12f, 0.86f, 0.060f);

    // Optional D-pad, between the stick and the face buttons.
    const float dx = a - 0.70f;
    const float dy = 0.74f;
    const float ds = 0.095f;
    const float dr = 0.050f;
    put(Control::DUp, dx, dy - ds, dr);
    put(Control::DDown, dx, dy + ds, dr);
    put(Control::DLeft, dx - ds, dy, dr);
    put(Control::DRight, dx + ds, dy, dr);

    return layout;
}

Engine::Engine() {
    Resolve();
}

float Engine::MinDim() const {
    return std::max(1.0f, std::min(mWidth, mHeight));
}

void Engine::SetScreen(float widthPx, float heightPx) {
    if (widthPx < 1.0f || heightPx < 1.0f) {
        return;
    }
    if (widthPx == mWidth && heightPx == mHeight) {
        return;
    }
    mWidth = widthPx;
    mHeight = heightPx;
    Resolve();
}

void Engine::SetLayout(const Layout& layout) {
    mLayout = layout;
    Resolve();
}

void Engine::SetSettings(const Settings& settings) {
    Settings next = settings;
    next.scale = std::clamp(next.scale, 0.3f, 3.0f);
    next.stickDeadzone = std::clamp(next.stickDeadzone, 0.0f, 0.9f);

    // Safe to call every frame: fingers are only dropped when something that moves controls has changed.
    const bool geometryChanged = next.scale != mSettings.scale || next.leftHanded != mSettings.leftHanded ||
                                 next.showDpad != mSettings.showDpad ||
                                 next.floatingStick != mSettings.floatingStick;
    mSettings = next;
    if (geometryChanged) {
        DropBindings();
        Resolve();
    }
}

void Engine::SetMode(Mode mode) {
    if (mode == mMode) {
        return;
    }
    mMode = mode;
    DropBindings();
    Resolve();
}

void Engine::SetBlockedRect(float x0, float y0, float x1, float y1) {
    mHasBlockedRect = true;
    mBlockedRect[0] = std::min(x0, x1);
    mBlockedRect[1] = std::min(y0, y1);
    mBlockedRect[2] = std::max(x0, x1);
    mBlockedRect[3] = std::max(y0, y1);
}

void Engine::ClearBlockedRect() {
    mHasBlockedRect = false;
}

bool Engine::InBlockedRect(Vec2 p) const {
    return mHasBlockedRect && p.x >= mBlockedRect[0] && p.x <= mBlockedRect[2] && p.y >= mBlockedRect[1] &&
           p.y <= mBlockedRect[3];
}

bool Engine::IsVisibleInMode(Control c) const {
    switch (mMode) {
        case Mode::MenuOpen:
            return c == Control::CloseMenu;
        case Mode::Hidden:
            return c == Control::Toggle;
        case Mode::Play:
        case Mode::Passive:
        case Mode::Edit:
            if (c == Control::CloseMenu) {
                return false;
            }
            if (IsDpad(c)) {
                return mSettings.showDpad;
            }
            return true;
    }
    return false;
}

void Engine::Resolve() {
    const float minDim = MinDim();
    for (size_t i = 0; i < kControlCount; ++i) {
        const Control c = static_cast<Control>(i);
        Resolved& g = mGeometry[i];
        float x = mLayout.controls[i].center.x;
        if (mSettings.leftHanded) {
            x = 1.0f - x;
        }
        g.center = { x * mWidth, mLayout.controls[i].center.y * mHeight };
        // UI buttons keep their size so they stay easy to hit however small the user makes the game controls.
        const float scale = KindOf(c) == Kind::Tap ? 1.0f : mSettings.scale;
        g.radius = mLayout.controls[i].radius * scale * minDim;
        g.visible = IsVisibleInMode(c);
        g.active = g.visible && mMode != Mode::Passive;
    }
}

bool Engine::FindControl(Vec2 p, float grow, uint32_t kindMask, Control& out) const {
    float best = 0.0f;
    bool found = false;
    for (size_t i = 0; i < kControlCount; ++i) {
        const Control c = static_cast<Control>(i);
        if ((KindBit(KindOf(c)) & kindMask) == 0) {
            continue;
        }
        const Resolved& g = mGeometry[i];
        if (!g.active) {
            continue;
        }
        const float ratio = Distance(p, g.center) / std::max(1.0f, g.radius);
        if (ratio <= grow && (!found || ratio < best)) {
            best = ratio;
            out = c;
            found = true;
        }
    }
    return found;
}

Engine::Binding Engine::Classify(int64_t id, Vec2 p) {
    Binding b;
    b.id = id;
    b.last = p;

    if (InBlockedRect(p) || mMode == Mode::Passive) {
        return b; // ignored until the finger lifts
    }

    Control c = Control::Count;

    if (mMode == Mode::Edit) {
        if (FindControl(p, 1.0f, kAllKinds, c)) {
            const Resolved& g = mGeometry[Index(c)];
            b.kind = Bind::Drag;
            b.control = c;
            b.grab = { g.center.x - p.x, g.center.y - p.y };
        }
        return b;
    }

    // UI buttons first: they are small and must win over whatever lies underneath.
    if (FindControl(p, kTapHitGrow, KindBit(Kind::Tap), c)) {
        mTaps |= Bit(c);
        return b; // consumed; the finger does nothing else until it lifts
    }

    if (mMode != Mode::Play) {
        return b; // Hidden / MenuOpen: nothing else reacts
    }

    if (FindControl(p, kButtonHitGrow, KindBit(Kind::Button), c)) {
        b.kind = Bind::Button;
        return b;
    }

    if (!mStickActive) {
        if (mSettings.floatingStick) {
            const float zoneWidth = kFloatZoneWidth * mWidth;
            const bool inZone = mSettings.leftHanded ? (p.x > mWidth - zoneWidth) : (p.x < zoneWidth);
            if (inZone && p.y > kFloatZoneTop * mHeight) {
                b.kind = Bind::Stick;
                mStickCenter = p;
                mStickActive = true;
                return b;
            }
        } else {
            const Resolved& stick = mGeometry[Index(Control::Stick)];
            if (stick.active && Distance(p, stick.center) <= stick.radius * kStickGrabGrow) {
                b.kind = Bind::Stick;
                mStickActive = true;
                return b;
            }
        }
    }

    b.kind = Bind::Look;
    return b;
}

void Engine::UpdateStick(Vec2 p) {
    const Resolved& base = mGeometry[Index(Control::Stick)];
    const float radius = std::max(1.0f, base.radius);
    Vec2 center = mSettings.floatingStick ? mStickCenter : base.center;

    Vec2 d{ p.x - center.x, p.y - center.y };
    float len = std::hypot(d.x, d.y);
    if (len > radius) {
        if (mSettings.floatingStick) {
            // The base follows the thumb so it never has to travel back across the whole dial.
            const float k = (len - radius) / len;
            center.x += d.x * k;
            center.y += d.y * k;
            mStickCenter = center;
            d = { p.x - center.x, p.y - center.y };
        } else {
            d.x *= radius / len;
            d.y *= radius / len;
        }
        len = radius;
    }

    const float magnitude = len / radius; // 0..1
    mStickKnob = { d.x / radius, d.y / radius };

    mStickOut = {};
    const float dz = mSettings.stickDeadzone;
    if (magnitude > dz && len > 0.0f) {
        const float scaled = (magnitude - dz) / (1.0f - dz);
        mStickOut.x = (d.x / len) * scaled;
        mStickOut.y = -(d.y / len) * scaled; // screen y grows downwards, the game's grows upwards
    }
}

void Engine::MoveControl(const Binding& b, Vec2 p) {
    const Resolved& g = mGeometry[Index(b.control)];
    const float rx = std::min(g.radius / mWidth, 0.5f);
    const float ry = std::min(g.radius / mHeight, 0.5f);
    float nx = std::clamp((p.x + b.grab.x) / mWidth, rx, 1.0f - rx);
    const float ny = std::clamp((p.y + b.grab.y) / mHeight, ry, 1.0f - ry);
    if (mSettings.leftHanded) {
        nx = 1.0f - nx; // layouts are stored right-handed
    }
    mLayout.controls[Index(b.control)].center = { nx, ny };
    Resolve();
}

void Engine::ReleaseBinding(const Binding& b) {
    switch (b.kind) {
        case Bind::Stick:
            mStickActive = false;
            mStickOut = {};
            mStickKnob = {};
            break;
        case Bind::Drag:
            mLayoutChanged = true;
            break;
        default:
            break;
    }
}

void Engine::DropBindings() {
    for (const Binding& b : mBindings) {
        ReleaseBinding(b);
    }
    mBindings.clear();
    mPressed.fill(false);
}

void Engine::Update(const Finger* fingers, size_t count) {
    // Forget fingers that have lifted.
    for (size_t i = 0; i < mBindings.size();) {
        bool present = false;
        for (size_t f = 0; f < count; ++f) {
            if (fingers[f].id == mBindings[i].id) {
                present = true;
                break;
            }
        }
        if (present) {
            ++i;
        } else {
            ReleaseBinding(mBindings[i]);
            mBindings.erase(mBindings.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }

    std::array<bool, kControlCount> pressedNow{};

    for (size_t f = 0; f < count; ++f) {
        const Vec2 p{ fingers[f].x * mWidth, fingers[f].y * mHeight };

        size_t index = mBindings.size();
        for (size_t i = 0; i < mBindings.size(); ++i) {
            if (mBindings[i].id == fingers[f].id) {
                index = i;
                break;
            }
        }
        if (index == mBindings.size()) {
            mBindings.push_back(Classify(fingers[f].id, p));
        }
        Binding& b = mBindings[index];

        switch (b.kind) {
            case Bind::Ignore:
                break;
            case Bind::Look: {
                const float dx = (p.x - b.last.x) / MinDim();
                const float dy = (p.y - b.last.y) / MinDim();
                if (std::fabs(dx) < kMaxLookStep && std::fabs(dy) < kMaxLookStep) {
                    mLook.x += dx;
                    mLook.y += dy;
                }
                b.last = p;
                break;
            }
            case Bind::Button: {
                // Re-evaluated every frame so a thumb can roll from one button to the next.
                Control c = Control::Count;
                if (FindControl(p, kButtonHitGrow, KindBit(Kind::Button), c)) {
                    pressedNow[Index(c)] = true;
                }
                break;
            }
            case Bind::Stick:
                UpdateStick(p);
                break;
            case Bind::Drag:
                MoveControl(b, p);
                break;
        }
    }

    for (size_t i = 0; i < kControlCount; ++i) {
        if (pressedNow[i] && !mPressed[i]) {
            mEdges |= Bit(static_cast<Control>(i));
        }
    }
    mPressed = pressedNow;
}

void Engine::Reset() {
    DropBindings();
    mStickOut = {};
    mStickKnob = {};
    mStickActive = false;
    mLook = {};
    mEdges = 0;
    mTaps = 0;
}

Vec2 Engine::StickCenterPx() const {
    if (mStickActive && mSettings.floatingStick) {
        return mStickCenter;
    }
    return mGeometry[Index(Control::Stick)].center;
}

Vec2 Engine::TakeLook() {
    const Vec2 out = mLook;
    mLook = {};
    return out;
}

uint32_t Engine::TakePressEdges() {
    const uint32_t out = mEdges;
    mEdges = 0;
    return out;
}

uint32_t Engine::TakeTaps() {
    const uint32_t out = mTaps;
    mTaps = 0;
    return out;
}

bool Engine::TakeLayoutChanged() {
    const bool out = mLayoutChanged;
    mLayoutChanged = false;
    return out;
}

LookOutput MapLook(Vec2 drag, float seconds, const LookParams& params) {
    const float dt = std::max(seconds, 0.001f);
    const float xSign = params.invertX ? -1.0f : 1.0f;
    const float ySign = params.invertY ? -1.0f : 1.0f;
    LookOutput out;

    // Third person. The stick is a rate, so use the finger's speed. With the game's invert options off, stick right
    // looks right and stick up looks up; with "invert Y" on (the game's default) stick up looks down, so compensate.
    const float speedX = (drag.x / dt) * kStickPerHeightPerSecond * params.sensitivity;
    const float speedY = (drag.y / dt) * kStickPerHeightPerSecond * params.sensitivity;
    out.rightStickX = speedX * (params.gameInvertsX ? -1.0f : 1.0f) * xSign;
    out.rightStickY = speedY * (params.gameInvertsY ? 1.0f : -1.0f) * ySign;

    // First person. The game applies these as turn amounts per tick, so use the drag itself. Positive gyro X looks up
    // and positive gyro Y turns left.
    if (!params.rightStickAiming) {
        out.gyroX = -drag.y * kGyroPerHeight * params.sensitivity * ySign;
        out.gyroY = -drag.x * kGyroPerHeight * params.sensitivity * xSign;
    }
    return out;
}

} // namespace SohTouch
