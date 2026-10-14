/**
 * @file    pet_ctl.c
 * @brief   Action handling. See pet_ctl.h.
 */
#include "pet_ctl.h"

#include <stddef.h>

void pet_ctl_init(pet_ctl_t *ctl, uint32_t now_ms)
{
    if (ctl == NULL) { return; }

    pet_init(&ctl->pet, now_ms);
    pet_anim_start(&ctl->anim, PET_ANIM_IDLE, now_ms);
}

void pet_ctl_tick(pet_ctl_t *ctl, uint32_t now_ms)
{
    if (ctl == NULL) { return; }

    pet_tick(&ctl->pet, now_ms);
    pet_anim_update(&ctl->anim, now_ms);

    /* An action animation has played out, so drop back to idle. Only while the
       pet is alive - a finished pet is showing DEATH or RUNAWAY, which
       pet_anim_select() picks over whatever this holds. */
    if (pet_anim_finished(&ctl->anim) && (ctl->anim.id != PET_ANIM_IDLE) &&
        pet_is_interactive(&ctl->pet)) {
        pet_anim_start(&ctl->anim, PET_ANIM_IDLE, now_ms);
    }
}

bool pet_ctl_action(pet_ctl_t *ctl, pet_action_t action, uint8_t portion,
                    uint32_t now_ms)
{
    bool accepted;

    if (ctl == NULL) { return false; }

    switch (action) {
        case PET_ACTION_FEED:  accepted = pet_feed(&ctl->pet, portion, now_ms); break;
        case PET_ACTION_PLAY:  accepted = pet_play(&ctl->pet, now_ms);          break;
        case PET_ACTION_CLEAN: accepted = pet_clean(&ctl->pet, now_ms);         break;
        case PET_ACTION_SLEEP: accepted = pet_sleep(&ctl->pet, now_ms);         break;
        default:               return false;
    }

    if (accepted) {
        /* Sleep stays on idle; care actions play their one-shots. */
        pet_anim_start(&ctl->anim, pet_anim_for_action(action), now_ms);
    }
    return accepted;
}

void pet_ctl_wake(pet_ctl_t *ctl, uint32_t now_ms)
{
    if (ctl == NULL) { return; }
    pet_wake(&ctl->pet, now_ms);
    pet_anim_start(&ctl->anim, PET_ANIM_IDLE, now_ms);
}

void pet_ctl_restart(pet_ctl_t *ctl, uint32_t now_ms)
{
    if (ctl == NULL) { return; }

    pet_restart(&ctl->pet, now_ms);
    pet_anim_start(&ctl->anim, PET_ANIM_RESTART, now_ms);
}
