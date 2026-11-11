#pragma once

#ifdef __ANDROID__

#include <libultraship/libultraship.h>

namespace SohTouch {

// Presents the on-screen touch controls to the game as one more controller. That way it shares all of the existing pad
// merging (stick processing, simulated input lag, ...) with real controllers and nothing in the game has to know that
// the input came from a touch screen.
class TouchController final : public LUS::Controller {
  public:
    TouchController();

    void ReadDevice(int32_t portIndex) override;
    bool Connected() const override {
        return true;
    }
    bool CanRumble() const override {
        return false;
    }
    bool CanSetLed() const override {
        return false;
    }
    bool CanGyro() const override {
        return false;
    }
    void CreateDefaultBinding(int32_t) override {
    }
    void ClearRawPress() override {
    }
    int32_t ReadRawPress() override {
        return -1;
    }
    const std::string GetButtonName(int32_t, int32_t) override {
        return "Touch";
    }
    int32_t SetRumble(int32_t, bool) override {
        return -1000;
    }
    int32_t SetLedColor(int32_t, Color_RGB8) override {
        return -1000;
    }
};

} // namespace SohTouch

#endif // __ANDROID__
