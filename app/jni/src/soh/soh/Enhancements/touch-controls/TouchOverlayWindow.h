#pragma once

#ifdef __ANDROID__

#include <libultraship/libultraship.h>

namespace SohTouch {

// An always-visible GUI window. Its only job is to put TouchControls::Draw() into the game's per-frame GUI loop, which
// runs every registered window each frame (SohGui registers it next to the other windows).
class TouchOverlayWindow final : public LUS::GuiWindow {
  public:
    TouchOverlayWindow();

  protected:
    void InitElement() override {
    }
    void DrawElement() override;
    void UpdateElement() override {
    }
};

} // namespace SohTouch

#endif // __ANDROID__
