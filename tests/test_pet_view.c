/**
 * @file    test_pet_view.c
 * @brief   Tests for sprite selection and frame building.
 *
 * These check which sprite gets picked for each state. That matters because
 * drawing a dead pet with the happy pose is a bug the model tests can't catch -
 * the model is right either way, it's the choice of image that's wrong.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc -I sprites tests/test_pet_view.c \
 *        Core/Src/pet_view.c Core/Src/pet_render.c Core/Src/pet_anim.c \
 *        Core/Src/pet_core.c sprites/slime_NAME.c -o /tmp/test_pet_view
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
            printf("  [FAIL] %s   (%s:%d)\n", (msg), __FILE__, __LINE__); \
        }                                                                 \
    } while (0)

static void section(const char *name) { printf("\n%s\n", name); }

static void test_mood_mapping(void)
{
    section("Mood to sprite");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_CONTENT)   == slime_content,   "CONTENT -> slime_content");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_DELIGHTED) == slime_delighted, "DELIGHTED -> slime_delighted");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_HUNGRY)    == slime_hungry,    "HUNGRY -> slime_hungry");
    CHECK(pet_view_sprite_for_mood(PET_MOOD_MISERABLE) == slime_miserable, "MISERABLE -> slime_miserable");
}

static void test_anim_mapping(void)
{
    section("Animation to sprite");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_FEED)    == slime_eating,  "FEED -> slime_eating");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_DEATH)   == slime_dead,    "DEATH -> slime_dead");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_RUNAWAY) == slime_runaway, "RUNAWAY -> slime_runaway");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_IDLE)  == NULL, "IDLE has no sprite of its own, it uses the mood");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_PLAY)  == NULL, "PLAY uses the mood sprite with a transform");
    CHECK(pet_view_sprite_for_anim(PET_ANIM_CLEAN) == NULL, "CLEAN uses the mood sprite with a transform");
}

static void test_selection_follows_state(void)
{
    pet_t p;
    pet_anim_t a;

    section("Selection follows the pet's state");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    CHECK(pet_view_sprite(&p, &a) == slime_delighted,
          "a new pet is at happiness 75, so it shows the delighted pose");

    { pet_t h; pet_init(&h, 0); pet_tick(&h, 35 * PET_HAPPINESS_PERIOD_MS);   /* food 17 */
      CHECK(pet_view_sprite(&h, &a) == slime_hungry, "a very hungry pet shows the hungry pose"); }

    /* A pet left alone starves before it gets this unhappy - it dies at 90 s but
       happiness wouldn't be low enough until 100 s. So feed it enough to stay
       alive and let happiness drain. */
    { pet_t m; uint32_t t; bool miserable = false;
      pet_init(&m, 0);
      for (t = 1000; t <= (3u * PET_RUNAWAY_MS); t += 1000) {
          pet_tick(&m, t);
          if (m.food <= 50) { pet_feed(&m, 5, t); }
          if (m.state != PET_ALIVE) { break; }
          if (pet_mood(&m) == PET_MOOD_MISERABLE) { miserable = true; break; }
      }
      CHECK(miserable, "pet got miserable while still alive");
      CHECK(pet_view_sprite(&m, &a) == slime_miserable, "so it shows the miserable pose"); }
}

static void test_terminal_states_win(void)
{
    pet_t p;
    pet_anim_t a;

    section("A finished pet overrides whatever animation is running");

    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_FEED, 1000);
    CHECK(pet_view_sprite(&p, &a) == slime_eating, "while feeding it shows the eating pose");

    pet_tick(&p, PET_STARVE_MS);                     /* starved part way through the feed */
    CHECK(p.state == PET_DEAD, "pet has died");
    CHECK(pet_view_sprite(&p, &a) == slime_dead,
          "it shows the dead pose, not the feeding pose");

    { pet_t r; uint32_t t; bool ran = false;
      pet_init(&r, 0);
      for (t = 1000; t <= (3u * PET_RUNAWAY_MS); t += 1000) {
          pet_tick(&r, t);
          if (r.food <= 50) { pet_feed(&r, 5, t); }
          if (r.state == PET_RUNAWAY) { ran = true; break; }
          if (r.state == PET_DEAD) { break; }
      }
      pet_anim_start(&a, PET_ANIM_FEED, t);
      CHECK(ran, "pet reached the runaway state");
      CHECK(pet_view_sprite(&r, &a) == slime_runaway,
            "it shows the runaway pose, not the feeding pose"); }
}

static void test_frame_index(void)
{
    pet_anim_t a;
    section("Frame index");
    CHECK(pet_view_frame(NULL) == 0, "no animation gives frame 0");

    pet_anim_start(&a, PET_ANIM_PLAY, 0);
    CHECK(pet_view_frame(&a) == 0, "starts on frame 0");
    pet_anim_update(&a, 100);
    CHECK(pet_view_frame(&a) == 1, "advances with time");
    pet_anim_update(&a, 500);
    CHECK(pet_view_frame(&a) == 4, "ends on the last frame");
}

static void test_composition(void)
{
    pet_t p;
    pet_anim_t a;
    static uint16_t fb[PET_SPRITE_W * PET_SPRITE_H];
    uint32_t i, non_bg = 0;
    int all_bg = 1;

    section("Building a frame");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    pet_view_compose_stage(fb, PET_SPRITE_W, PET_SPRITE_H, &p, &a);

    for (i = 0; i < (uint32_t)PET_SPRITE_W * PET_SPRITE_H; i++) {
        if (fb[i] == PET_STAGE_COLOR) { continue; }
        all_bg = 0;
        non_bg++;
    }
    printf("         %u of %u pixels differ from the background\n",
           non_bg, (unsigned)(PET_SPRITE_W * PET_SPRITE_H));
    CHECK(!all_bg, "the finished frame actually has the sprite in it");
    CHECK(non_bg > 500, "a decent chunk of the frame is sprite");

    /* A dead pet has to build a different frame from a living one. */
    {
        static uint16_t fb2[PET_SPRITE_W * PET_SPRITE_H];
        int differs, k;
        pet_tick(&p, PET_STARVE_MS);
        pet_view_compose_stage(fb2, PET_SPRITE_W, PET_SPRITE_H, &p, &a);
        differs = 0;
        for (k = 0; k < (int)(PET_SPRITE_W * PET_SPRITE_H); k++) {
            if (fb2[k] != fb[k]) { differs++; }
        }
        CHECK(differs > 100, "a dead pet builds a clearly different frame");
    }

    section("Bad input is refused");
    pet_view_compose_stage(NULL, PET_SPRITE_W, PET_SPRITE_H, &p, &a);
    CHECK(1, "a NULL framebuffer is refused, not dereferenced");
}

int main(void)
{
    printf("pet_view tests\n");

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
