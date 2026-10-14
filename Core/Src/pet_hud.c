/**
 * @file    pet_hud.c
 * @brief   HUD layout maths. See pet_hud.h.
 */
#include "pet_hud.h"

#include <stdio.h>

uint16_t pet_hud_meter_fill_px(int16_t value, uint16_t track_w)
{
    if (value < PET_STAT_MIN) { value = PET_STAT_MIN; }
    if (value > PET_STAT_MAX) { value = PET_STAT_MAX; }

    /* Integer maths on purpose: no float here, and the multiply happens before
       the divide so a 160 px bar doesn't lose resolution to rounding. */
    return (uint16_t)(((uint32_t)value * track_w) / PET_STAT_MAX);
}

uint16_t pet_hud_meter_color(int16_t value)
{
    if (value < PET_STAT_MIN) { value = PET_STAT_MIN; }
    if (value > PET_STAT_MAX) { value = PET_STAT_MAX; }

    if (value >= PET_HUD_GOOD_AT) { return PET_HUD_COL_GOOD; }
    if (value >= PET_HUD_BAD_AT)  { return PET_HUD_COL_WARN; }
    return PET_HUD_COL_BAD;
}

void pet_hud_age_text(char *buf, size_t size, const pet_t *pet)
{
    if ((buf == NULL) || (size == 0u)) { return; }
    (void)snprintf(buf, size, "Day %lu  %02uh", (unsigned long)pet_age_day(pet),
                   (unsigned)pet_age_hour(pet));
}

uint16_t pet_hud_centered_x(uint16_t chars)
{
    const uint32_t width = (uint32_t)chars * 6u * PET_HUD_TEXT_SCALE;
    return (width >= PET_HUD_SCREEN_W) ? 0u : (uint16_t)((PET_HUD_SCREEN_W - width) / 2u);
}

uint16_t pet_hud_menu_row_y(uint8_t index)
{
    const uint8_t n = pet_ui_item_count();
    if (n == 0u) { return PET_HUD_MENU_Y; }
    if (index >= n) { index = (uint8_t)(index % n); }

    return (uint16_t)(PET_HUD_MENU_Y + (uint16_t)index * PET_HUD_MENU_ROW_H);
}
