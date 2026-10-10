/*
 * Raidmaster 403 — mode enregistrement « démo » (voir demo_record.h).
 *
 * Temps virtuel : chaque image avance l'horloge LVGL de FRAME_MS par pas de STEP_MS
 * (ui_tick + lv_timer_handler), force le rendu (lv_refr_now) puis copie l'écran. Le résultat
 * est donc identique d'une exécution à l'autre et ne dépend pas de la charge de la machine.
 *
 * Données affichées : à la place de ui_demo (dont le cap tourne à 0,5 °/s, invisible sur quelques
 * secondes), le scénario pousse lui-même un ui_trip_state_t vivant, parti des valeurs de la
 * maquette : vitesse qui ondule, Trip A/B et ODO qui avancent, chrono qui tourne, cap qui
 * balance de ±70° comme dans une suite de virages.
 *
 * Licence : GPL-3.0.
 */

#include "demo_record.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gif_writer.h"
#include "screen_menu.h"
#include "theme.h"
#include "ui.h"
#include "ui_demo.h"

enum {
    FRAME_MS     = 100,            /* 10 images/s : aligné sur le rafraîchissement des données */
    STEP_MS      = 5,              /* pas d'avance de l'horloge LVGL dans une image */
    BOOT_MAX_MS  = 20000,          /* garde-fou si le boot ne rend jamais la main */
};

/* Étapes du scénario, durées en ms (le boot dure ce qu'il dure : on attend qu'il passe la main). */
typedef enum {
    PH_BOOT = 0,
    PH_RAID,
    PH_BALADE,
    PH_MENU,
    PH_CALIB,
    PH_RAID_NUIT,
    PH_RAID_JOUR,       /* retour au jour : la boucle du GIF repart sans saut de palette */
    PH_FIN,
} phase_t;

static const uint32_t s_phase_ms[PH_FIN] = {
    [PH_BOOT]      = 0,
    [PH_RAID]      = 5500,
    [PH_BALADE]    = 3500,
    [PH_MENU]      = 4500,
    [PH_CALIB]     = 3000,
    [PH_RAID_NUIT] = 4000,
    [PH_RAID_JOUR] = 1500,
};

static const char *const s_phase_name[PH_FIN + 1] = {
    "boot", "raid", "balade", "menu", "calibration", "raid nuit", "raid jour", "fin",
};

/* Déplacements de la sélection du menu : instant (ms depuis l'entrée dans le menu) → sens. */
static const struct { uint32_t at_ms; int delta; } s_menu_moves[] = {
    {  900, +1 }, { 1500, +1 }, { 2100, +1 }, { 2700, +1 }, { 3500, -1 }, { 4000, -1 },
};

/* ------------------------------------------------------------------------- */
/* Données de démo. */

static ui_trip_state_t s_st;

static void state_init(void)
{
    memset(&s_st, 0, sizeof(s_st));
    s_st.trip_a_km   = 12.345;
    s_st.trip_b_km   = 3.47;
    s_st.odo_km      = 48273.2;
    s_st.speed_kmh   = 68.0f;
    s_st.avg_kmh     = 42.0f;
    s_st.heading_deg = 127;
    s_st.chrono_s    = 14U * 60U + 27U;
    s_st.fix_type    = UI_FIX_3D;
    s_st.battery_pct = 68;
    s_st.t_cabin_c   = 22.0f;
    snprintf(s_st.time_hhmm, sizeof(s_st.time_hhmm), "14:23");
}

/* Avance l'état de dt_ms ; t_ms = temps écoulé depuis l'arrivée sur l'écran raid. */
static void state_step(uint32_t t_ms, uint32_t dt_ms)
{
    const float two_pi = 6.2831853f;
    float ts = (float)t_ms / 1000.0f;

    s_st.speed_kmh = 74.0f + 16.0f * sinf(two_pi * ts / 9.0f) + 5.0f * sinf(two_pi * ts / 2.3f);

    double step_km = (double)s_st.speed_kmh * (double)dt_ms / 3600000.0;
    s_st.trip_a_km += step_km;
    s_st.trip_b_km += step_km;
    s_st.odo_km    += step_km;

    /* Chrono : base de la maquette + temps écoulé (en secondes entières). */
    s_st.chrono_s = 14U * 60U + 27U + t_ms / 1000U;
    s_st.avg_kmh  = (float)(s_st.trip_a_km * 3600.0 / (double)s_st.chrono_s);

    int cap = 127 + (int)lroundf(70.0f * sinf(two_pi * ts / 11.0f));
    s_st.heading_deg = ((cap % 360) + 360) % 360;

    ui_push_trip_state(&s_st);
}

/* ------------------------------------------------------------------------- */

static void enter_phase(phase_t ph)
{
    switch (ph) {
        case PH_RAID:
            /* Le boot a lancé ui_demo : on la coupe et on pousse nos propres données. */
            ui_demo_stop();
            if (ui_current_screen() != UI_SCREEN_RAID) ui_show_screen(UI_SCREEN_RAID);
            break;
        case PH_BALADE:    ui_show_screen(UI_SCREEN_BALADE); break;
        case PH_MENU:      ui_show_screen(UI_SCREEN_MENU);   break;
        case PH_CALIB:     ui_show_screen(UI_SCREEN_CALIB);  break;
        case PH_RAID_NUIT:
            ui_show_screen(UI_SCREEN_RAID);
            if (!ui_theme_is_night()) ui_theme_toggle_night();
            break;
        case PH_RAID_JOUR:
            if (ui_theme_is_night()) ui_theme_toggle_night();
            break;
        default: break;
    }
}

int demo_record_run(lv_display_t *disp, const uint16_t *fb, int w, int h,
                    const char *gif_path, int scale)
{
    gif_rec_t *rec = gif_rec_create(w, h, scale);
    if (rec == NULL) {
        fprintf(stderr, "enregistrement : initialisation impossible (échelle %d ?)\n", scale);
        return 1;
    }

    phase_t  ph = PH_BOOT;
    uint32_t t = 0, ph_t0 = 0, raid_t0 = 0;
    size_t   next_move = 0;
    int      ok = 1;

    /* Image 0 : l'écran de boot tel que ui_init() l'a posé. */
    lv_refr_now(disp);
    ok = gif_rec_add_frame(rec, fb, FRAME_MS / 10) == 0;

    while (ok && ph != PH_FIN) {
        for (uint32_t k = 0; k < FRAME_MS; k += STEP_MS) {
            ui_tick(STEP_MS);
            lv_timer_handler();
        }
        t += FRAME_MS;

        /* Transitions de phase. */
        if (ph == PH_BOOT) {
            if (ui_current_screen() != UI_SCREEN_BOOT || t >= BOOT_MAX_MS) {
                if (t >= BOOT_MAX_MS) fprintf(stderr, "enregistrement : boot trop long, on passe\n");
                printf("enregistrement : boot terminé à %u ms\n", (unsigned)t);
                ph = PH_RAID;
                ph_t0 = raid_t0 = t;
                state_init();
                enter_phase(ph);
            }
        } else if (t - ph_t0 >= s_phase_ms[ph]) {
            ph = (phase_t)(ph + 1);
            ph_t0 = t;
            printf("enregistrement : %s à %u ms\n", s_phase_name[ph], (unsigned)t);
            if (ph == PH_FIN) break;
            enter_phase(ph);
            next_move = 0;
        }

        if (ph != PH_BOOT) {
            if (ph == PH_MENU && next_move < sizeof(s_menu_moves) / sizeof(s_menu_moves[0]) &&
                t - ph_t0 >= s_menu_moves[next_move].at_ms) {
                screen_menu_move(s_menu_moves[next_move].delta);
                next_move++;
            }
            state_step(t - raid_t0, FRAME_MS);
        }

        lv_refr_now(disp);
        ok = gif_rec_add_frame(rec, fb, FRAME_MS / 10) == 0;
    }

    /* Remet le thème de jour (propreté si quelqu'un réutilise la boucle après). */
    if (ui_theme_is_night()) ui_theme_toggle_night();

    if (ok) ok = gif_rec_write(rec, gif_path) == 0;
    if (ok) {
        unsigned cs = gif_rec_duration_cs(rec);
        printf("enregistrement : %s — %d images distinctes, %u.%02u s, %d×%d\n", gif_path,
               gif_rec_frame_count(rec), cs / 100U, cs % 100U, w * scale, h * scale);
    } else {
        fprintf(stderr, "enregistrement : échec d'écriture de %s\n", gif_path);
    }
    gif_rec_free(rec);
    return ok ? 0 : 1;
}
