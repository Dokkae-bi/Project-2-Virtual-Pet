/**
 ******************************************************************************
 * @file    test_pet_view.c
 * @brief   Host test suite for sprite selection and frame composition.
 *
 * Asserting which artwork gets chosen for each state is what stops a dead pet
 * being drawn as a happy one - a bug that is invisible in a unit test of the
 * model alone, because the model is correct either way.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc -I sprites tests/test_pet_view.c \
 *        Core/Src/pet_view.c Core/Src/pet_render.c Core/Src/pet_anim.c \
 *        Core/Src/pet_core.c sprites/slime_&lt;name&gt;.c -o /tmp/test_pet_view
 ******************************************************************************
 */
#include <stdio.h>

#include "pet_view.h"

#include "slime_content.h"
#include "slime_delighted.h"
#include "slime_hungry.h"
#include "slime_miserable.h"
#include "slime_eating.h"
#include "slime_dead.h"
#include "slime_runaway.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            g_pass++;                                                     \
            printf("  [PASS] %s\n", (msg));                               \
        } else {                                                          \
            g_fail++;                                                     \
            printf("  [FAIL] %s   (%s:%d)\n", (msg), __FILE__, __LINE__);  \
        }                                                                 \
    } while (0)

static void section(const char *name) { printf("\n%s\n", name); }

/* --------------------------------------------------------------------------- */

static void test_mood_mapping(void)
{
    section("Mood to artwork mapping");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_CONTENT)   == slime_content,   "content uses the idle pose");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_DELIGHTED) == slime_delighted, "delighted uses the happy pose");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_HUNGRY)    == slime_hungry,    "hungry uses the hungry pose");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_MISERABLE) == slime_miserable, "miserable uses the sad pose");
}

static void test_anim_mapping(void)
{
    section("Animation to artwork mapping");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_FEED)    == slime_eating,  "feeding has its own pose");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_DEATH)   == slime_dead,    "death has its own pose");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_RUNAWAY) == slime_runaway, "runaway has its own pose");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_IDLE)  == NULL, "idle defers to the mood sprite");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_PLAY)  == NULL, "play is a transform of the mood sprite");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_CLEAN) == NULL, "clean is a transform of the mood sprite");
}

static void test_selection_follows_state(void)
{
    pet_t p;
    pet_anim_t a;

    section("Selection follows the pet's actual state");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    CHECK(pet_view_sprite(&p, &a) == slime_delighted,
          "a freshly initialised pet (happiness 75) shows the delighted pose");

    { pet_t h; pet_init(&h, 0); pet_tick(&h, 70000);   /* hunger 83 */
      CHECK(pet_view_sprite(&h, &a) == slime_hungry, "a very hungry pet shows the hungry pose"); }

    /* A neglected pet starves before it can get this unhappy: with the documented
     * rates it dies at 90 s while happiness would not hit the miserable
     * threshold until 100 s. So misery has to be reached deliberately, by
     * feeding enough to stay alive while happiness drains. */
    { pet_t m; uint32_t t; bool miserable = false;
      pet_init(&m, 0);
      for (t = 1000; t <= 600000; t += 1000) {
          pet_tick(&m, t);
          if (m.hunger >= 50) { pet_feed(&m, 5, t); }
          if (m.state != PET_ALIVE) { break; }
          if (pet_mood(&m) == PET_MOOD_MISERABLE) { miserable = true; break; }
      }
      CHECK(miserable, "the pet reached the miserable mood while still alive");
      CHECK(pet_view_sprite(&m, &a) == slime_miserable, "a miserable pet shows the sad pose"); }
}

static void test_terminal_states_win(void)
{
    pet_t p;
    pet_anim_t a;

    section("A finished life overrides whatever animation is in flight");

    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_FEED, 1000);
    CHECK(pet_view_sprite(&p, &a) == slime_eating, "while feeding, the eating pose is shown");

    pet_tick(&p, 90000);                     /* starved mid-feed */
    CHECK(p.state == PET_DEAD, "the pet has died");
    CHECK(pet_view_sprite(&p, &a) == slime_dead,
          "the dead pose is shown, not the feeding pose");

    { pet_t r; uint32_t t; bool ran = false;
      pet_init(&r, 0);
      for (t = 1000; t <= 600000; t += 1000) {
          pet_tick(&r, t);
          if (r.hunger >= 50) { pet_feed(&r, 5, t); }
          if (r.state == PET_RUNAWAY) { ran = true; break; }
          if (r.state == PET_DEAD) { break; }
      }
      pet_anim_start(&a, PET_ANIM_FEED, t);
      CHECK(ran, "the pet reached the runaway state");
      CHECK(pet_view_sprite(&r, &a) == slime_runaway,
            "the runaway pose is shown, not the feeding pose"); }
}

static void test_frame_index(void)
{
    pet_anim_t a;
    section("Frame index reporting");
    CHECK(pet_view_frame(NULL) == 0, "no animation reports frame 0");

    pet_anim_start(&a, PET_ANIM_PLAY, 0);
    CHECK(pet_view_frame(&a) == 0, "the first frame of play");
    pet_anim_update(&a, 100);
    CHECK(pet_view_frame(&a) == 1, "advances with time");
    pet_anim_update(&a, 500);
    CHECK(pet_view_frame(&a) == 4, "ends on the last frame");
}

static void test_composition(void)
{
    pet_t p;
    pet_anim_t a;
    /* Large enough to hold one stage-sized sprite. */
    static uint16_t fb[PET_SPRITE_W * PET_SPRITE_H];
    uint32_t i, non_bg = 0;
    int all_bg = 1;

    section("Frame composition");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    pet_view_compose_stage(fb, PET_SPRITE_W, PET_SPRITE_H, &p, &a);

    for (i = 0; i < (uint32_t)PET_SPRITE_W * PET_SPRITE_H; i++) {
        if (fb[i] == PET_STAGE_COLOR) { continue; }
        all_bg = 0;
        non_bg++;
    }
    printf("         %u of %u pixels differ from the stage colour\n",
           non_bg, (unsigned)(PET_SPRITE_W * PET_SPRITE_H));
    CHECK(!all_bg, "the composed frame actually contains the sprite");
    CHECK(non_bg > 500, "a meaningful fraction of the frame is sprite");

    /* A dead pet must compose differently from a living one. */
    {
        static uint16_t fb2[PET_SPRITE_W * PET_SPRITE_H];
        int differs, k;
        pet_tick(&p, 90000);
        pet_view_compose_stage(fb2, PET_SPRITE_W, PET_SPRITE_H, &p, &a);
        differs = 0;
        for (k = 0; k < (int)(PET_SPRITE_W * PET_SPRITE_H); k++) {
            if (fb2[k] != fb[k]) { differs++; }
        }
        CHECK(differs > 100, "a dead pet composes a visibly different frame");
    }

    section("Composition refuses bad input");
    pet_view_compose_stage(NULL, PET_SPRITE_W, PET_SPRITE_H, &p, &a);
    CHECK(1, "a NULL framebuffer is refused rather than dereferenced");
}

/* --------------------------------------------------------------------------- */

int main(void)
{
    printf("pet_view host test suite\n");

    test_mood_mapping();
    test_anim_mapping();
    test_selection_follows_state();
    test_terminal_states_win();
    test_frame_index();
    test_composition();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
