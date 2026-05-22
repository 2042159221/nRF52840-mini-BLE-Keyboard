#include <mode/mode_selector.h>

#define MODE_USB_24G_BOUNDARY_MV 825
#define MODE_24G_BLE_BOUNDARY_MV 1450
#define MODE_SELECTOR_HYSTERESIS_MV 100

enum kb_mode mode_selector_classify_mv(int32_t mv, enum kb_mode current_mode)
{
    switch (current_mode) {
    case KB_MODE_USB:
        if (mv <= (MODE_USB_24G_BOUNDARY_MV + MODE_SELECTOR_HYSTERESIS_MV)) {
            return KB_MODE_USB;
        }
        break;
    case KB_MODE_24G_RESERVED:
        if (mv >= (MODE_USB_24G_BOUNDARY_MV - MODE_SELECTOR_HYSTERESIS_MV) &&
            mv < (MODE_24G_BLE_BOUNDARY_MV + MODE_SELECTOR_HYSTERESIS_MV)) {
            return KB_MODE_24G_RESERVED;
        }
        break;
    case KB_MODE_BLE:
        if (mv >= (MODE_24G_BLE_BOUNDARY_MV - MODE_SELECTOR_HYSTERESIS_MV)) {
            return KB_MODE_BLE;
        }
        break;
    default:
        break;
    }

    if (mv < MODE_USB_24G_BOUNDARY_MV) {
        return KB_MODE_USB;
    }

    if (mv >= MODE_24G_BLE_BOUNDARY_MV) {
        return KB_MODE_BLE;
    }

    return KB_MODE_24G_RESERVED;
}
