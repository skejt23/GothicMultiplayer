#include "client_resources/process_input.h"
#include "shared/event.h"
#include "client_resources/client_events.h"

namespace gmp::gothic {

bool s_prevPressed[MAX_KEYS_AND_CODES + 1] = {};
bool s_pressedThisFrame[MAX_KEYS_AND_CODES + 1] = {};
bool s_toggledThisFrame[MAX_KEYS_AND_CODES + 1] = {};

void ProcessInput(zCInput* zinput) {
    if (!zinput)
        return;

    std::memset(s_pressedThisFrame, 0, sizeof(s_pressedThisFrame));
    std::memset(s_toggledThisFrame, 0, sizeof(s_toggledThisFrame));

    for (int i = 0; i < gmp::gothic::kAllPhysicalCodeCount; ++i) {
        int key_code = gmp::gothic::kAllPhysicalCodes[i];

        bool isPressedNow = zinput->KeyPressed(key_code) != 0;

        bool wasPressedBefore = s_prevPressed[key_code];

        if (isPressedNow != wasPressedBefore) {
            s_toggledThisFrame[key_code] = true;

            if (isPressedNow) {
                EventManager::Instance().TriggerEvent(
                    gmp::client::kEventOnKeyDownName,
                    gmp::client::OnKeyEvent{key_code}
                );
            } else {
                EventManager::Instance().TriggerEvent(
                    gmp::client::kEventOnKeyUpName,
                    gmp::client::OnKeyEvent{key_code}
                );
            }
        }

        s_pressedThisFrame[key_code] = isPressedNow;
    }

    std::memcpy(
        s_prevPressed,
        s_pressedThisFrame,
        sizeof(s_prevPressed)
    );
}

} // namespace gmp::gothic