# Sleep / Night — Design Spec

Menu-driven sleep for the virtual pet: the player puts the slime to bed; while
asleep happiness recovers and food still drains slowly; the player must wake it
explicitly. No auto-wake. No new artwork required for v1.

**Status:** implemented 2026-10-14  
**Approach:** sleep flag on `pet_t` (not a new `pet_state_t`)

---

## Goals

- Add a clear **Sleep** care action that changes pacing and presentation.
- Keep the happiness/food **tradeoff** honest: rest helps mood, still need to feed.
- Reuse existing input paths: menu via `pet_ui`, Play via shake → `pet_ctl_action`.
- Stay host-testable: all rules in HAL-free modules with new checks in `tests/`.

## Non-goals (v1)

- Auto night from the game clock.
- New sleep sprite / Zzz artwork (text or tint only).
- PWM buzzer sleep lullaby.
- Persisting sleep across reset.
- Changing the age clock (age still advances while sleeping).

---

## Player-facing behaviour

| Situation | Behaviour |
|---|---|
| Menu → **Sleep** | Pet goes to sleep (alive only, not already sleeping). |
| Asleep, pet screen, **SELECT** | Opens menu; stays asleep. |
| Asleep, menu → **Feed / Play / Clean** | Wake, then apply that action (same effects/animations as today). |
| Asleep, menu → **Sleep** | **Wake only** (no Feed/Play/Clean effect). Label may stay `"Sleep"` or show `"Wake"` while asleep — prefer **`"Wake"`** when `pet_is_sleeping` so the row is honest. |
| Asleep, menu → **BACK** (when B1 wired) | Close menu; stay asleep. |
| Asleep, pet screen, **shake** | Wake, then Play (same as menu Play). |
| Asleep | No auto-wake after N hours. |
| Death / runaway while asleep | Sleep clears; existing ending screens apply. |
| Restart after ending | Awake, normal start stats. |

### Stat rules while sleeping

| Stat | Behaviour | Tuning knobs (`pet_core.h`) |
|---|---|---|
| Food | Continues to decay, **slower** than awake | e.g. `PET_SLEEP_FOOD_MS_PER_UNIT` (suggest 2× awake `PET_FOOD_MS_PER_UNIT`) |
| Happiness | **Increases** toward max on a recover period | e.g. `PET_SLEEP_HAPPINESS_MS_PER_UNIT` (suggest ~1500–2500 ms per +1) |
| Clamps | Still `[PET_STAT_MIN, PET_STAT_MAX]` | existing |
| Terminal | Food → 0 still → `PET_DEAD`; happiness → 0 still → `PET_RUNAWAY` (unlikely while recovering, but food death in bed is intentional) | existing checks after tick |

Awake decay rates unchanged.

---

## Architecture

```
pet_ui  --Sleep / care-->  pet_ctl_action / pet_ctl_wake_then
                |                |
                v                v
            pet_core          pet_anim
         (sleeping flag,    (idle while asleep;
          sleep tick rates)  action anims after wake)

main.c Shake_Poll --if sleeping--> wake + PET_ACTION_PLAY
pet_screen --if sleeping--> night tint + optional "Sleeping" line
```

### Why a flag, not `PET_SLEEPING` state

`pet_state_t` is terminal life outcomes (`ALIVE` / `DEAD` / `RUNAWAY`). Sleep is a
mode of an alive pet. A `bool sleeping` (or small mode enum) keeps death/runaway
logic simple and matches `pet_is_interactive()` = alive.

---

## API / data changes

### `pet_core`

```c
/* On pet_t */
bool sleeping;

/* New action */
PET_ACTION_SLEEP   /* in pet_action_t, before PET_ACTION_COUNT */

bool pet_sleep(pet_t *pet, uint32_t now_ms);   /* true if entered sleep */
void pet_wake(pet_t *pet, uint32_t now_ms);    /* clear sleeping; no-op if awake */
bool pet_is_sleeping(const pet_t *pet);
```

- `pet_init` / `pet_restart`: `sleeping = false`.
- `pet_feed` / `pet_play` / `pet_clean`: if sleeping, **wake first** then apply
  (so model stays consistent if called directly), or refuse until woken —
  **prefer wake-then-apply inside these functions** so every caller gets the same
  semantics (UI and shake).
- `pet_sleep`: refuse if not interactive or already sleeping.
- Entering death/runaway in `pet_tick`: set `sleeping = false`.
- `pet_mood` while sleeping: keep using happiness/food thresholds (recovering pet
  can look content/delighted); presentation layer may still tint the stage night.

### `pet_ctl`

- Handle `PET_ACTION_SLEEP`: `pet_sleep` + return to/start idle animation
  (optional: future `PET_ANIM_SLEEP`; v1 = idle is enough).
- Feed/Play/Clean unchanged at the switch level if core wakes-then-applies.
- Optional helper `pet_ctl_wake(ctl, now_ms)` for clarity; not required if core owns wake.

### `pet_ui`

- Add Sleep to `UI_ITEMS[]` (label `"Sleep"` — font-safe).
- While `pet_is_sleeping(pet)`:
  - Pet screen SELECT → open menu (same as awake).
  - Menu SELECT on Feed/Play/Clean → return that action (core wakes then applies).
  - Fourth row label becomes `"Wake"`; selecting it closes the menu and signals
    wake only (e.g. `result.wake = true` or a dedicated flag on
    `pet_ui_result_t` — **not** `PET_ACTION_SLEEP` again). Keep four rows so HUD
    layout stays stable.
- When awake, fourth row is `"Sleep"` and commits `PET_ACTION_SLEEP`.
- Dead/runaway behaviour unchanged (menu locked, SELECT = restart).

### `pet_anim` / `pet_view` / `pet_screen`

- v1: no new animation id required; idle + night presentation.
- `pet_screen`: when sleeping and menu closed, draw night panel/stage tint
  (darker `PET_HUD` colours or fill stage region) and a short status line
  e.g. `"Sleeping"` (must pass `test_ui_text` charset).
- Meters still update when food/happiness change.

### `main.c`

- Shake path today: `pet_ctl_action(..., PLAY)` if pet screen.
- Keep that; if core Play wakes-then-applies, shake automatically becomes
  wake + Play. No special-case required beyond ensuring shake is ignored with
  menu open (already true).

---

## Tuning defaults (starting point)

| Define | Suggested | Notes |
|---|---|---|
| `PET_SLEEP_FOOD_MS_PER_UNIT` | `2400u` (2× 1200) | Half hunger rate vs awake |
| `PET_SLEEP_HAPPINESS_MS_PER_UNIT` | `2000u` | +1 happiness / 2 s while asleep |

Exact values are knobs; host tests should use the defines, not magic numbers.

---

## Testing

| Suite | New / extended checks |
|---|---|
| `test_pet_core` | sleep enter/refuse; wake; food slow + happiness up; starve in sleep; wake-then-feed/play/clean; restart clears sleep |
| `test_pet_ctl` | SLEEP action; Play after sleep wakes; animation returns idle |
| `test_pet_ui` | Sleep in menu; asleep SELECT opens menu; Feed from asleep menu commits FEED; Wake row wakes without another care action |
| `test_ui_text` | `"Sleep"` / `"Sleeping"` drawable |
| `test_pet_hud` | only if new layout strings/helpers |

Update README/TODO check totals when counts change.

---

## Implementation order

1. `pet_core` flag + rates + `pet_sleep` / `pet_wake` + wake-then-apply on care actions + tests  
2. `pet_ctl` + `PET_ACTION_SLEEP` + tests  
3. `pet_ui` menu + asleep behaviour + tests  
4. `pet_screen` / HUD night presentation + `test_ui_text`  
5. Manual: shake wake+Play on hardware when available  
6. Docs: README actions table, `docs/pet-design.md` status row  

---

## Risks

| Risk | Mitigation |
|---|---|
| Starving in sleep surprises players | Intentional tradeoff; show food meter still moving; optional later warning flash |
| Four-item menu crowding on 240×320 | Existing row height; verify in `pet_hud` / on panel |
| `pet_ui_item_count` context complexity | Keep fixed four items; fourth row toggles Sleep ↔ Wake |
| CubeMX / `main.c` churn | Shake needs no new branch if core owns wake-on-Play |
| SPI cost of full-stage tint every frame | Tint only on sleep enter / wake / first draw; reuse dirty flags in `g_drawn` |

---

## Open points (resolved)

| Question | Decision |
|---|---|
| How to start sleep? | Menu action Sleep |
| Stats while asleep? | Happiness recovers; food drains slowly |
| Auto-wake? | No |
| SELECT while asleep? | Opens menu (stay asleep) |
| Care from menu while asleep? | Wake then apply |
| Shake while asleep? | Wake + Play |
| Pure wake (no care action)? | Fourth menu row becomes Wake while asleep |
| Model shape? | `bool sleeping` on `pet_t` |

---

## Success criteria

- [x] Player can Sleep from the menu and see a night/sleeping presentation.
- [x] While asleep, happiness rises and food falls (slower), both clamped.
- [x] Feed/Play/Clean from the asleep menu wake then apply; shake Play wakes then plays.
- [x] SELECT on pet screen while asleep opens the menu without waking.
- [x] Asleep menu Wake clears sleep without applying Feed/Play/Clean; death/runaway/restart clear sleep.
- [x] Host tests cover the above; no `HAL_Delay` introduced.
