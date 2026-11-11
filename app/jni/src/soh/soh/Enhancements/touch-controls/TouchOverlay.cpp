#ifdef __ANDROID__

#include "TouchOverlay.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include <ImGui/imgui.h>

#include "public/bridge/consolevariablebridge.h"

namespace SohTouch {

namespace {

ImVec4 BaseColor(Control c) {
    switch (c) {
        case Control::A:
            return ImVec4(0.16f, 0.38f, 0.86f, 1.0f); // N64 blue
        case Control::B:
            return ImVec4(0.18f, 0.66f, 0.30f, 1.0f); // N64 green
        case Control::CUp:
        case Control::CDown:
        case Control::CLeft:
        case Control::CRight:
            return ImVec4(0.96f, 0.80f, 0.15f, 1.0f); // N64 yellow
        case Control::Start:
            return ImVec4(0.86f, 0.20f, 0.20f, 1.0f); // N64 red
        case Control::Z:
        case Control::L:
        case Control::R:
            return ImVec4(0.60f, 0.60f, 0.66f, 1.0f);
        case Control::DUp:
        case Control::DDown:
        case Control::DLeft:
        case Control::DRight:
            return ImVec4(0.64f, 0.64f, 0.70f, 1.0f);
        default:
            return ImVec4(0.86f, 0.86f, 0.92f, 1.0f);
    }
}

ImU32 WithAlpha(const ImVec4& c, float alpha) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, std::clamp(alpha, 0.0f, 1.0f)));
}

// Glyphs are drawn as strokes rather than text: they stay crisp at any size and do not depend on the loaded fonts.
// Points are in -1..1 and scaled by `size` around `center`.
void Stroke(ImDrawList* dl, ImVec2 center, float size, std::initializer_list<ImVec2> points, ImU32 colour,
            float thickness) {
    ImVec2 buffer[12];
    int count = 0;
    for (const ImVec2& p : points) {
        if (count == 12) {
            break;
        }
        buffer[count++] = ImVec2(center.x + p.x * size, center.y + p.y * size);
    }
    dl->AddPolyline(buffer, count, colour, ImDrawFlags_None, thickness);
}

// Filled triangle pointing in direction (dx, dy).
void Arrow(ImDrawList* dl, ImVec2 center, float size, float dx, float dy, ImU32 colour) {
    const ImVec2 forward(dx * size, dy * size);
    const ImVec2 side(-dy * size * 0.85f, dx * size * 0.85f);
    const ImVec2 tip(center.x + forward.x, center.y + forward.y);
    const ImVec2 baseA(center.x - forward.x * 0.6f + side.x, center.y - forward.y * 0.6f + side.y);
    const ImVec2 baseB(center.x - forward.x * 0.6f - side.x, center.y - forward.y * 0.6f - side.y);
    dl->AddTriangleFilled(tip, baseA, baseB, colour);
}

void DrawGlyph(ImDrawList* dl, Control c, ImVec2 center, float radius, ImU32 ink, float thickness, bool controlsHidden) {
    const float s = radius * 0.46f;
    switch (c) {
        case Control::A:
            Stroke(dl, center, s, { ImVec2(-0.8f, 0.9f), ImVec2(0.0f, -0.9f), ImVec2(0.8f, 0.9f) }, ink, thickness);
            Stroke(dl, center, s, { ImVec2(-0.45f, 0.35f), ImVec2(0.45f, 0.35f) }, ink, thickness);
            break;
        case Control::B:
            Stroke(dl, center, s,
                   { ImVec2(-0.6f, 0.9f), ImVec2(-0.6f, -0.9f), ImVec2(0.2f, -0.9f), ImVec2(0.6f, -0.6f),
                     ImVec2(0.6f, -0.3f), ImVec2(0.2f, 0.0f), ImVec2(-0.6f, 0.0f) },
                   ink, thickness);
            Stroke(dl, center, s,
                   { ImVec2(0.2f, 0.0f), ImVec2(0.7f, 0.3f), ImVec2(0.7f, 0.65f), ImVec2(0.25f, 0.9f),
                     ImVec2(-0.6f, 0.9f) },
                   ink, thickness);
            break;
        case Control::Z:
            Stroke(dl, center, s, { ImVec2(-0.7f, -0.9f), ImVec2(0.7f, -0.9f), ImVec2(-0.7f, 0.9f), ImVec2(0.7f, 0.9f) },
                   ink, thickness);
            break;
        case Control::L:
            Stroke(dl, center, s, { ImVec2(-0.5f, -0.9f), ImVec2(-0.5f, 0.9f), ImVec2(0.6f, 0.9f) }, ink, thickness);
            break;
        case Control::R:
            Stroke(dl, center, s,
                   { ImVec2(-0.6f, 0.9f), ImVec2(-0.6f, -0.9f), ImVec2(0.2f, -0.9f), ImVec2(0.6f, -0.6f),
                     ImVec2(0.6f, -0.3f), ImVec2(0.2f, 0.0f), ImVec2(-0.6f, 0.0f) },
                   ink, thickness);
            Stroke(dl, center, s, { ImVec2(0.0f, 0.0f), ImVec2(0.65f, 0.9f) }, ink, thickness);
            break;
        case Control::Start:
            Arrow(dl, center, s * 1.1f, 1.0f, 0.0f, ink); // play triangle
            break;
        case Control::CUp:
            Arrow(dl, center, s * 1.15f, 0.0f, -1.0f, ink);
            break;
        case Control::CDown:
            Arrow(dl, center, s * 1.15f, 0.0f, 1.0f, ink);
            break;
        case Control::CLeft:
            Arrow(dl, center, s * 1.15f, -1.0f, 0.0f, ink);
            break;
        case Control::CRight:
            Arrow(dl, center, s * 1.15f, 1.0f, 0.0f, ink);
            break;
        case Control::DUp:
            Arrow(dl, center, s * 1.1f, 0.0f, -1.0f, ink);
            break;
        case Control::DDown:
            Arrow(dl, center, s * 1.1f, 0.0f, 1.0f, ink);
            break;
        case Control::DLeft:
            Arrow(dl, center, s * 1.1f, -1.0f, 0.0f, ink);
            break;
        case Control::DRight:
            Arrow(dl, center, s * 1.1f, 1.0f, 0.0f, ink);
            break;
        case Control::Menu:
            for (float y : { -0.65f, 0.0f, 0.65f }) {
                Stroke(dl, center, s, { ImVec2(-0.85f, y), ImVec2(0.85f, y) }, ink, thickness);
            }
            break;
        case Control::Toggle:
            // An eye; crossed out while the controls are hidden.
            dl->AddCircle(center, s * 0.95f, ink, 24, thickness);
            dl->AddCircleFilled(center, s * 0.35f, ink, 16);
            if (controlsHidden) {
                Stroke(dl, center, s, { ImVec2(-0.9f, 0.9f), ImVec2(0.9f, -0.9f) }, ink, thickness);
            }
            break;
        case Control::CloseMenu:
            Stroke(dl, center, s, { ImVec2(-0.75f, -0.75f), ImVec2(0.75f, 0.75f) }, ink, thickness * 1.2f);
            Stroke(dl, center, s, { ImVec2(0.75f, -0.75f), ImVec2(-0.75f, 0.75f) }, ink, thickness * 1.2f);
            break;
        default:
            break;
    }
}

void DrawButton(ImDrawList* dl, Control c, const Resolved& g, bool pressed, bool editing, bool controlsHidden,
                float opacity) {
    const ImVec2 center(g.center.x, g.center.y);
    const float radius = g.radius * (pressed ? 0.94f : 1.0f);
    const ImVec4 base = BaseColor(c);
    const bool yellow = c == Control::CUp || c == Control::CDown || c == Control::CLeft || c == Control::CRight;

    dl->AddCircleFilled(center, radius, WithAlpha(base, opacity * (pressed ? 0.95f : 0.40f)), 40);
    dl->AddCircle(center, radius, WithAlpha(base, opacity + 0.20f), 40, std::max(2.0f, g.radius * 0.05f));

    const ImVec4 inkColour = yellow ? ImVec4(0.10f, 0.10f, 0.12f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const float inkAlpha = yellow ? std::min(1.0f, opacity + 0.35f) : std::min(1.0f, opacity + 0.30f);
    DrawGlyph(dl, c, center, g.radius, WithAlpha(inkColour, inkAlpha), std::max(2.5f, g.radius * 0.075f),
              controlsHidden);

    if (editing) {
        dl->AddCircle(center, radius + 5.0f, IM_COL32(255, 255, 255, 230), 40, 3.0f);
    }
}

void DrawStick(ImDrawList* dl, const Engine& engine, bool editing, float opacity) {
    const Resolved& g = engine.Geometry(Control::Stick);
    const bool active = engine.StickActive();
    const bool floating = engine.GetSettings().floatingStick;

    // A floating stick is only a faint hint where it will appear until the thumb lands.
    const float baseAlpha = (floating && !active && !editing) ? opacity * 0.45f : opacity;
    const Vec2 c = engine.StickCenterPx();
    const ImVec2 center(c.x, c.y);
    const float radius = g.radius;
    const ImVec4 grey(0.78f, 0.78f, 0.84f, 1.0f);

    dl->AddCircleFilled(center, radius, WithAlpha(grey, baseAlpha * 0.28f), 48);
    dl->AddCircle(center, radius, WithAlpha(grey, baseAlpha + 0.20f), 48, std::max(2.0f, radius * 0.03f));
    dl->AddCircle(center, radius * 0.5f, WithAlpha(grey, baseAlpha * 0.35f), 32, 1.5f);

    const float knobRadius = radius * 0.42f;
    const float travel = radius - knobRadius;
    const Vec2 k = engine.StickKnob();
    const ImVec2 knob(center.x + k.x * travel, center.y + k.y * travel);
    dl->AddCircleFilled(knob, knobRadius, WithAlpha(grey, baseAlpha * (active ? 0.85f : 0.55f)), 40);
    dl->AddCircle(knob, knobRadius, WithAlpha(grey, baseAlpha + 0.25f), 40, std::max(2.0f, radius * 0.03f));

    if (editing) {
        dl->AddCircle(center, radius + 5.0f, IM_COL32(255, 255, 255, 230), 48, 3.0f);
    }
}

} // namespace

void DrawControls(const Engine& engine, float opacity) {
    ImDrawList* dl = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const bool editing = engine.GetMode() == Mode::Edit;
    const bool controlsHidden = engine.GetMode() == Mode::Hidden;
    opacity = std::clamp(opacity, 0.1f, 1.0f);

    for (size_t i = 0; i < kControlCount; ++i) {
        const Control c = static_cast<Control>(i);
        const Resolved& g = engine.Geometry(c);
        if (!g.visible) {
            continue;
        }
        if (c == Control::Stick) {
            DrawStick(dl, engine, editing, opacity);
        } else {
            DrawButton(dl, c, g, engine.IsPressed(c), editing, controlsHidden, opacity);
        }
    }
}

EditAction DrawEditToolbar(float screenWidth, float screenHeight, float blocked[4]) {
    EditAction action = EditAction::None;

    ImGui::SetNextWindowPos(ImVec2(screenWidth * 0.5f, screenHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##TouchLayoutEditor", nullptr, flags)) {
        ImGui::TextUnformatted("Drag the controls to move them");
        if (ImGui::Button("Done")) {
            action = EditAction::Done;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset layout")) {
            action = EditAction::Reset;
        }
        const ImVec2 pos = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        blocked[0] = pos.x;
        blocked[1] = pos.y;
        blocked[2] = pos.x + size.x;
        blocked[3] = pos.y + size.y;
    }
    ImGui::End();

    return action;
}

SettingsActions DrawSettingsBody(UserSettings& s) {
    SettingsActions act;

    auto check = [&](const char* label, bool& value) {
        if (ImGui::Checkbox(label, &value)) {
            act.changed = true;
            act.flush = true;
        }
    };
    // Values change continuously while dragging; the config file is only written once the slider is let go.
    auto slider = [&](const char* label, float& value, float lo, float hi, const char* format) {
        if (ImGui::SliderFloat(label, &value, lo, hi, format)) {
            act.changed = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            act.flush = true;
        }
    };

    check("Show touch controls", s.enabled);
    check("Hide when a controller is connected", s.autoHide);

    ImGui::Separator();
    ImGui::TextUnformatted("Layout");
    slider("Opacity", s.opacity, 0.15f, 1.0f, "%.2f");
    slider("Size", s.scale, 0.6f, 1.6f, "%.2f");
    check("Left-handed layout", s.leftHanded);
    check("Floating analog stick", s.floatingStick);
    check("Show D-Pad", s.showDpad);
    check("Vibrate on button press", s.haptics);
    if (ImGui::Button("Edit layout")) {
        act.startEdit = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset layout")) {
        act.resetLayout = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Camera");
    ImGui::TextWrapped("Drag on any empty part of the screen to look around.");
    slider("Look sensitivity", s.lookSensitivity, 0.25f, 4.0f, "%.2f");
    check("Invert look X", s.invertLookX);
    check("Invert look Y", s.invertLookY);

    // These two are the game's own options; they are exposed here because touch-look needs them.
    bool freeCamera = CVarGetInteger("gFreeCamera", 0) != 0;
    if (ImGui::Checkbox("Free camera (needed to look around outside first person)", &freeCamera)) {
        CVarSetInteger("gFreeCamera", freeCamera ? 1 : 0);
        act.flush = true;
    }
    float cameraSpeed = CVarGetFloat("gThirdPersonCameraSensitivityX", 1.0f);
    if (ImGui::SliderFloat("Third-person camera speed", &cameraSpeed, 0.5f, 5.0f, "%.1f")) {
        CVarSetFloat("gThirdPersonCameraSensitivityX", cameraSpeed);
        CVarSetFloat("gThirdPersonCameraSensitivityY", cameraSpeed);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        act.flush = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Aiming");
    check("Gyro aiming (tilt the phone in first person)", s.gyroAiming);
    if (s.gyroAiming) {
        slider("Gyro sensitivity", s.gyroSensitivity, 0.25f, 4.0f, "%.2f");
    }

    return act;
}

} // namespace SohTouch

#endif // __ANDROID__
