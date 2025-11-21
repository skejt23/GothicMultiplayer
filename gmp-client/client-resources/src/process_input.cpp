#include "client_resources/process_input.h"
#include "shared/event.h"
#include "client_resources/client_events.h"

namespace gmp::gothic {

bool s_prevPressed[MAX_KEYS_AND_CODES + 1] = {};
bool s_pressedThisFrame[MAX_KEYS_AND_CODES + 1] = {};
bool s_toggledThisFrame[MAX_KEYS_AND_CODES + 1] = {};

// Call this once per frame
void ProcessInput(zCInput* zinput) {
    if (!zinput)
        return;

    // Clear current-frame snapshots
    std::memset(s_pressedThisFrame, 0, sizeof(s_pressedThisFrame));
    std::memset(s_toggledThisFrame, 0, sizeof(s_toggledThisFrame));

    for (int i = 0; i < gmp::gothic::kAllPhysicalCodeCount; ++i) {
        int key_code = gmp::gothic::kAllPhysicalCodes[i];

        // Current physical state from engine
        bool isPressedNow = zinput->KeyPressed(key_code) != 0;

        // Compare with previous frame
        bool wasPressedBefore = s_prevPressed[key_code];

        if (isPressedNow != wasPressedBefore) {
            // It changed this frame ? toggled
            s_toggledThisFrame[key_code] = true;

            if (isPressedNow) {
                // went UP ? DOWN
                EventManager::Instance().TriggerEvent(
                    gmp::client::kEventOnKeyDownName,
                    gmp::client::OnKeyEvent{key_code}
                );
            } else {
                // went DOWN ? UP
                EventManager::Instance().TriggerEvent(
                    gmp::client::kEventOnKeyUpName,
                    gmp::client::OnKeyEvent{key_code}
                );
            }
        }

        // Save for this frame snapshot
        s_pressedThisFrame[key_code] = isPressedNow;
    }

    // After processing all keys, remember state for next frame
    std::memcpy(
        s_prevPressed,
        s_pressedThisFrame,
        sizeof(s_prevPressed)
    );
}

} // namespace gmp::gothic