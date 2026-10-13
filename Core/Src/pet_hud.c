/**
 * @file    pet_hud.c
 * @brief   HUD layout maths. See pet_hud.h.
 */
#include "pet_hud.h"

uint16_t pet_hud_meter_fill_px(int16_t value, uint16_t track_w)
{
    if (value < PET_STAT_MIN) { value = PET_STAT_MIN; }
    if (value > PET_STAT_MAX) { value = PET_STAT_MAX; }

    /* Integer maths on purpose: no float here, and the multiply happens before
       the divide so a 160 px bar doesn't lose resolution to rounding. */
    return (uint16_t)(((uint32_t)value * track_w) / PET_STAT_MAX);
}

uint16_t pet_hud_meter_color(pet_meter_t which, int16_t value)
{
    if (value < PET_STAT_MIN) { value = PET_STAT_MIN; }
    if (value > PET_STAT_MAX) { value = PET_STAT_MAX; }

    if (which == PET_METER_HUNGER) {
        /* High hunger is bad, so the bands are the other way round. */
        if (value <= PET_HUD_BAD_AT)  { return PET_HUD_COL_GOOD; }
        if (value <= PET_HUD_GOOD_AT) { return PET_HUD_COL_WARN; }
        return PET_HUD_COL_BAD;
    }

    if (value >= PET_HUD_GOOD_AT) { return PET_HUD_COL_GOOD; }
    if (value >= PET_HUD_BAD_AT)  { return PET_HUD_COL_WARN; }
    return PET_HUD_COL_BAD;
}

uint16_t pet_hud_menu_row_y(uint8_t index)
{
    const uint8_t n = pet_ui_item_count();
    if (n == 0u) { return PET_HUD_MENU_Y; }
    if (index >= n) { index = (uint8_t)(index % n); }

    return (uint16_t)(PET_HUD_MENU_Y + (uint16_t)index * PET_HUD_MENU_ROW_H);
}
