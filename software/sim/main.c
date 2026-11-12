/*
 * Raidmaster 403 — simulateur PC.
 *
 * Fenêtre SDL 480×320 landscape (résolution exacte de l'écran de la carte Waveshare
 * ESP32-S3-Touch-LCD-3.5B), avec LVGL v9 dessinant dedans via un buffer software + SDL_Texture.
 *
 * Pourquoi ce chemin plutôt que LV_USE_SDL=1 : on garde le contrôle total des SDL_Event, ce qui
 * permet de router les touches a/b/+/-/m vers notre ui_input_dispatch() sans combat avec le
 * driver clavier interne de LVGL.
 *
 * Le core (core/app) tourne comme sur l'ESP32 : GPS fourni par hal_sim (rejeu d'un .nmea ou
 * trajet simulé), réglages et distances sauvegardés dans sim-storage.json. Le jingle de démarrage
 * (core/jingles) se joue avec l'animation, par la sortie audio SDL (muet : SDL_AUDIODRIVER=dummy ;
 * jamais en modes script, captures et GIF).
 *
 *   raidmaster-sim [--replay trace.nmea] [--speed N] [--loop] [--storage fichier.json | --no-storage]
 *                  [--script actions.txt --out dossier]
 *     --replay   rejoue un fichier NMEA au rythme de ses horodatages (sinon trajet simulé)
 *     --speed    accélère le temps GPS (1 à 1000, défaut 1)
 *     --loop     rejoue le fichier en boucle
 *     --storage  fichier de sauvegarde (défaut ./sim-storage.json) ; --no-storage : en mémoire
 *     --script   joue des actions horodatées puis quitte (avec SDL_VIDEODRIVER=dummy pour
 *                tourner sans fenêtre) ; une action par ligne, « # » pour commenter :
 *                  <ms> <bouton>   a b plus minus m, en majuscules pour l'appui long (A, PLUS, M…)
 *                  <ms> night      bascule jour / nuit
 *                  <ms> signal     coupe / rétablit le signal GPS
 *                  <ms> gps        débranche / rebranche le récepteur (repart à 9 600 bauds)
 *                  <ms> mag        débranche / rebranche la boussole
 *                  <ms> spin       la voiture tourne sur place (30°/s) / s'arrête de tourner
 *                  <ms> power_cut  coupure d'alimentation : le core redémarre sur la dernière
 *                                  sauvegarde, sans sauvegarder (l'UI et le GPS continuent)
 *                  <ms> shot <nom> capture <dossier>/<nom>.ppm
 *                  <ms> expect <champ> <valeur> [tolérance]
 *                                  champs : trip_a trip_b odo (km), speed avg max (km/h), calib (%),
 *                                  active (0 A, 1 B), fix, night, view (0 raid, 1 balade),
 *                                  hhmm (heure locale, -1 inconnue), alarm, chrono (en marche),
 *                                  gps_err (phrases rejetées : checksum ou champs illisibles),
 *                                  gps (récepteur présent), mag (boussole présente), mag_cal
 *                                  (calibrée), head_src (0 aucun, 1 route GPS, 2 boussole),
 *                                  heading (°, -1 inconnu), magcal (secteurs de 10° parcourus)
 *                  <ms> quit
 *                <ms> : temps depuis le lancement, sur une horloge virtuelle (pas de 10 ms, sans
 *                attente) : résultat déterministe, une minute se joue en une fraction de seconde.
 *                « @<s> » à la place : instant <s> de la trace rejouée (= s × 1000 / --speed ms).
 *                Code de sortie 1 si un expect échoue.
 *
 * Touches clavier (aussi dans le README) :
 *   a / b / + ou = / - / m        → boutons physiques A, B, +, -, Menu (appui court)
 *   Shift + touche                → appui long (Shift + m : entrée / sortie du menu réglages)
 *   n ou Tab                      → bascule mode nuit
 *   v                             → cycle entre écrans raid / balade / menu / calib (debug)
 *   d                             → coupe / rétablit le signal GPS (tunnel)
 *   g                             → débranche / rebranche le récepteur GPS (puck)
 *   c                             → débranche / rebranche la boussole
 *   r                             → la voiture tourne sur place / s'arrête (calibration boussole)
 *   p                             → alimentation suivante : batterie, USB en charge, USB batterie
 *                                   pleine, USB sans batterie, batterie faible (barre d'état)
 *   Échap                         → quitter (sauvegarde)
 *
 * Mode captures (vérification visuelle sans interaction) : RAIDMASTER_SHOTS=<dossier>
 * enchaîne chaque écran avec un état fixe repris de la maquette, écrit <dossier>/<écran>.ppm
 * puis quitte. Ex. : SDL_VIDEODRIVER=dummy RAIDMASTER_SHOTS=/tmp/shots ./raidmaster-sim
 *
 * Mode enregistrement démo : RAIDMASTER_RECORD=<fichier.gif> [RAIDMASTER_RECORD_SCALE=1..4]
 * joue le scénario de démo en temps virtuel et écrit un GIF animé en boucle (voir
 * demo_record.h). Commande à la demande : tools/demo_gif.sh.
 *
 * Licence : GPL-3.0.
 */

#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui.h"
#include "theme.h"
#include "ui_demo.h"
#include "ui_input.h"
#include "screen_boot.h"
#include "screen_menu.h"
#include "demo_record.h"
#include "script.h"
#include "app.h"
#include "jingles.h"
#include "hal/hal_gps.h"
#include "hal/hal_mag.h"
#include "hal/hal_time.h"
#include "hal_sim.h"
#include "ui_app.h"

enum {
    TRIPMASTER_SCREEN_W = 480,
    TRIPMASTER_SCREEN_H = 320,
};

/* Buffer LVGL : partiel (10 lignes), format RGB565 (LV_COLOR_DEPTH 16). On en met un seul,
 * c'est largement suffisant côté PC. */
#define BUF_LINES 40
static uint16_t s_lv_buf[TRIPMASTER_SCREEN_W * BUF_LINES];

static app_t s_app;
static bool  s_app_on = false;      /* core branché (hors modes GIF et captures) */

static void on_gps_line(const char *line, size_t len, void *user)
{
    (void)len;
    app_gps_line((app_t *)user, line, hal_time_ms());
}

static void usage(void)
{
    fprintf(stderr, "usage : raidmaster-sim [--replay trace.nmea] [--speed N] [--loop] "
                    "[--storage fichier.json | --no-storage] [--script actions.txt --out dossier]\n");
}

static SDL_Window   *s_window   = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture  *s_texture  = NULL;

/* Copie complète de l'écran (RGB565), tenue à jour au flush, pour les modes captures et
 * enregistrement. */
static uint16_t s_fb[TRIPMASTER_SCREEN_W * TRIPMASTER_SCREEN_H];

/* ------------------------------------------------------------------------- */
/* LVGL display flush : LVGL nous donne un rectangle à copier dans la texture SDL,
 * puis on présente. On fait un UpdateTexture ciblé par rectangle. */
static void sim_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    SDL_Rect r;
    r.x = area->x1;
    r.y = area->y1;
    r.w = area->x2 - area->x1 + 1;
    r.h = area->y2 - area->y1 + 1;

    /* px_map est au format RGB565, stride = r.w * 2. */
    SDL_UpdateTexture(s_texture, &r, px_map, r.w * (int)sizeof(uint16_t));
    const uint16_t *src = (const uint16_t *)px_map;
    for (int y = 0; y < r.h; y++) {
        memcpy(&s_fb[(r.y + y) * TRIPMASTER_SCREEN_W + r.x], &src[y * r.w],
               (size_t)r.w * sizeof(uint16_t));
    }

    /* On ne fait un RenderPresent qu'au dernier chunk pour éviter le tearing. */
    if (lv_display_flush_is_last(disp)) {
        SDL_RenderClear(s_renderer);
        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
        SDL_RenderPresent(s_renderer);
    }

    lv_display_flush_ready(disp);
}

/* ------------------------------------------------------------------------- */
/* Mode captures. */
static void save_ppm(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "capture : impossible d'écrire %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H);
    for (int i = 0; i < TRIPMASTER_SCREEN_W * TRIPMASTER_SCREEN_H; i++) {
        uint16_t p = s_fb[i];
        uint8_t rgb[3] = {
            (uint8_t)(((p >> 11) & 0x1F) * 255 / 31),
            (uint8_t)(((p >> 5) & 0x3F) * 255 / 63),
            (uint8_t)((p & 0x1F) * 255 / 31),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("capture : %s\n", path);
}

/* État fixe = valeurs de la maquette mode raid jour, avec l'alimentation donnée. */
static void push_mockup_state_ex(uint8_t power, uint8_t battery_pct, bool gps_absent, uint8_t head_src,
                                 uint8_t mag_state);

static void push_mockup_state(uint8_t power, uint8_t battery_pct)
{
    push_mockup_state_ex(power, battery_pct, false, UI_HEAD_SRC_UNSET, UI_MAG_OK);
}

/* Variante : module GPS absent, source du cap, état de la boussole. */
static void push_mockup_state_ex(uint8_t power, uint8_t battery_pct, bool gps_absent, uint8_t head_src,
                                 uint8_t mag_state)
{
    ui_trip_state_t st;
    memset(&st, 0, sizeof(st));
    st.trip_a_km   = 12.345;
    st.trip_b_km   = 3.47;
    st.odo_km      = 48273.2;
    st.speed_kmh   = 68.0f;
    st.avg_kmh     = 42.0f;
    st.heading_deg = 127;
    st.chrono_s    = 14U * 60U + 27U;
    st.fix_type    = UI_FIX_3D;
    st.battery_pct = battery_pct;
    st.power       = power;
    st.t_cabin_c   = 22.0f;
    snprintf(st.time_hhmm, sizeof(st.time_hhmm), "14:23");
    st.gps_absent  = gps_absent;
    st.heading_src = head_src;
    st.mag_state   = mag_state;
    if (gps_absent) {
        st.fix_type = UI_FIX_NONE;
        st.speed_kmh = 0.0f;
        st.heading_unknown = head_src == UI_HEAD_SRC_UNSET;
    }
    ui_push_trip_state(&st);
}

/* Avance le scénario de captures ; renvoie 0 quand c'est fini. */
static int shots_step(const char *dir, uint32_t t_ms)
{
    static int step = 0;
    static uint32_t next = 700;
    if (t_ms < next) return 1;
    switch (step++) {
        case 0:  save_ppm(dir, "1-boot"); break;
        case 1:  ui_demo_stop(); ui_show_screen(UI_SCREEN_RAID); push_mockup_state(UI_PWR_BATTERY, 68); break;
        case 2:  save_ppm(dir, "2-raid"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 3:  save_ppm(dir, "3-balade"); ui_show_screen(UI_SCREEN_MENU);
                 screen_menu_move(+1); break;
        case 4:  save_ppm(dir, "4-menu"); ui_show_screen(UI_SCREEN_CALIB); break;
        case 5:  save_ppm(dir, "5-calib"); ui_show_screen(UI_SCREEN_RAID);
                 ui_theme_toggle_night(); break;
        case 6:  save_ppm(dir, "6-raid-nuit"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 7:  save_ppm(dir, "7-balade-nuit"); ui_theme_toggle_night();
                 ui_show_screen(UI_SCREEN_RAID); push_mockup_state(UI_PWR_CHARGING, 68); break;
        case 8:  save_ppm(dir, "8-raid-charge");
                 push_mockup_state(UI_PWR_EXTERNAL, UI_BATT_UNKNOWN); break;
        case 9:  save_ppm(dir, "9-raid-usb");
                 push_mockup_state_ex(UI_PWR_BATTERY, 68, false, UI_HEAD_SRC_MAG, UI_MAG_OK); break;
        case 10: save_ppm(dir, "10-raid-cap-boussole");
                 push_mockup_state_ex(UI_PWR_BATTERY, 68, true, UI_HEAD_SRC_UNSET, UI_MAG_ABSENT); break;
        case 11: save_ppm(dir, "11-raid-gps-absent"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 12: save_ppm(dir, "12-balade-gps-absent"); ui_show_screen(UI_SCREEN_MAGCAL);
                 push_mockup_state(UI_PWR_BATTERY, 68); break;
        case 13: save_ppm(dir, "13-calib-boussole"); return 0;
        default: return 0;
    }
    /* Après le boot, on attend la fin du timer boot→raid (qui lance la démo) avant de figer
     * l'état : sa durée dépend de la présence de l'animation (screen_boot_duration_ms). */
    next = step == 1 ? screen_boot_duration_ms() + 300U : t_ms + (step == 2 ? 600 : 300);
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Routing clavier SDL → ui_input_dispatch(). */
static void handle_key(const SDL_KeyboardEvent *k, int *running)
{
    bool is_long = (k->keysym.mod & KMOD_SHIFT) != 0;
    SDL_Keycode sym = k->keysym.sym;

    switch (sym) {
        case SDLK_ESCAPE:
            *running = 0;
            break;
        case SDLK_a:
            ui_input_dispatch(UI_BTN_A, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_b:
            ui_input_dispatch(UI_BTN_B, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            ui_input_dispatch(UI_BTN_PLUS, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            ui_input_dispatch(UI_BTN_MINUS, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_m:
            ui_input_dispatch(UI_BTN_MENU, is_long ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
            break;
        case SDLK_n:
        case SDLK_TAB:
            if (s_app_on) ui_app_toggle_night();
            else          ui_theme_toggle_night();
            break;
        case SDLK_v:
            ui_cycle_screen();
            break;
        case SDLK_p:
            printf("[sim] alimentation : %s\n", hal_power_sim_cycle());
            break;
        case SDLK_g:
            hal_gps_sim_set_plugged(!hal_gps_sim_plugged());
            printf("[sim] récepteur GPS %s\n", hal_gps_sim_plugged() ? "rebranché (9 600 bauds)" : "débranché");
            break;
        case SDLK_c:
            hal_mag_sim_set_plugged(!hal_mag_sim_plugged());
            printf("[sim] boussole %s\n", hal_mag_sim_plugged() ? "rebranchée" : "débranchée");
            break;
        case SDLK_r:
            hal_mag_sim_set_spin(!hal_mag_sim_spin());
            printf("[sim] rotation sur place %s\n", hal_mag_sim_spin() ? "en cours" : "arrêtée");
            break;
        case SDLK_d:
            if (s_app_on) {
                hal_gps_sim_set_signal(!hal_gps_sim_signal());
                printf("[sim] signal GPS %s\n", hal_gps_sim_signal() ? "rétabli" : "coupé");
            } else {
                ui_demo_toggle();
            }
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------------- */
/* Mode script : actions horodatées (voir l'en-tête), lues par sim/script.c. */

#define SCRIPT_STEP_MS 10U

static bool        s_script;
static const char *s_script_out = ".";

static void script_shot(const char *name, void *user)
{
    (void)user;
    lv_refr_now(NULL);
    save_ppm(s_script_out, name);
}

/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    const char *replay = NULL, *storage = "sim-storage.json", *script = NULL;
    double speed = 1.0;
    bool loop = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay = argv[++i];
        } else if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc) {
            speed = atof(argv[++i]);
            if (speed < 1.0 || speed > 1000.0) {
                fprintf(stderr, "--speed : 1 à 1000\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--loop") == 0) {
            loop = true;
        } else if (strcmp(argv[i], "--storage") == 0 && i + 1 < argc) {
            storage = argv[++i];
        } else if (strcmp(argv[i], "--no-storage") == 0) {
            storage = NULL;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            s_script_out = argv[++i];
        } else {
            usage();
            return 2;
        }
    }

    if (script != NULL) {
        const script_io_t io = {.shot = script_shot};
        if (!script_open(script, speed, &s_app, &io)) {
            fprintf(stderr, "--script : impossible de lire %s\n", script);
            return 2;
        }
        s_script = true;
        hal_time_sim_virtual(true);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init a echoue : %s\n", SDL_GetError());
        return 1;
    }

    s_window = SDL_CreateWindow(
        "Raidmaster 403 - sim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H,
        SDL_WINDOW_SHOWN);
    if (s_window == NULL) {
        fprintf(stderr, "SDL_CreateWindow a echoue : %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    s_renderer = SDL_CreateRenderer(
        s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (s_renderer == NULL) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (s_renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer a echoue : %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return 1;
    }

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H);
    if (s_texture == NULL) {
        fprintf(stderr, "SDL_CreateTexture a echoue : %s\n", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return 1;
    }

    /* ------- LVGL init ------- */
    lv_init();

    lv_display_t *disp = lv_display_create(TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H);
    if (disp == NULL) {
        fprintf(stderr, "lv_display_create a echoue\n");
        return 1;
    }
    lv_display_set_flush_cb(disp, sim_disp_flush);
    lv_display_set_buffers(disp, s_lv_buf, NULL,
                           sizeof(s_lv_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    /* Core branché comme sur l'ESP32, sauf en modes GIF et captures (états figés de la démo et
     * de la maquette). Initialisé avant l'UI pour appliquer le mode nuit sauvegardé dès le boot. */
    const char *record_env = getenv("RAIDMASTER_RECORD");
    const char *shots_env = getenv("RAIDMASTER_SHOTS");
    if ((record_env == NULL || record_env[0] == '\0') && shots_env == NULL) {
        if (!hal_gps_sim_config(replay, speed, loop)) {
            fprintf(stderr, "--replay : impossible de lire %s\n", replay);
            return 1;
        }
        hal_storage_sim_open(storage);
        app_init(&s_app, hal_time_ms());
        ui_theme_set_night(s_app.set.night);
        s_app_on = true;
    }

    /* UI init (thème, écran boot, timer boot→écran principal). */
    ui_init(disp);
    if (s_app_on) {
        hal_gps_init(on_gps_line, &s_app);
        hal_mag_init();
        ui_app_attach(&s_app);
        if (!s_script) jingle_play(&JINGLE_BOOT);   /* avec l'animation de démarrage */
        printf("[sim] GPS : %s (×%g%s), sauvegarde : %s\n", replay ? replay : "trajet simulé", speed,
               loop ? ", en boucle" : "", storage ? storage : "mémoire");
    }

    /* ------- Mode enregistrement démo (temps virtuel, pas de boucle d'événements) ------- */
    const char *record_path = getenv("RAIDMASTER_RECORD");
    if (record_path != NULL && record_path[0] != '\0') {
        const char *scale_env = getenv("RAIDMASTER_RECORD_SCALE");
        int scale = (scale_env != NULL) ? atoi(scale_env) : 1;
        int rc = demo_record_run(disp, s_fb, TRIPMASTER_SCREEN_W, TRIPMASTER_SCREEN_H,
                                 record_path, scale);
        SDL_DestroyTexture(s_texture);
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return rc;
    }

    /* ------- Boucle principale ------- */
    const char *shots_dir = getenv("RAIDMASTER_SHOTS");


    int running = 1;
    uint32_t start_ms = (uint32_t)hal_time_ms();
    uint32_t last_ms = start_ms;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                handle_key(&ev.key, &running);
            }
        }

        if (s_app_on) {
            hal_gps_poll();
            mag_vec_t mag;
            if (hal_mag_poll(&mag)) app_mag_sample(&s_app, mag, hal_time_ms());
            app_tick(&s_app, hal_time_ms());
        }

        uint32_t now = (uint32_t)hal_time_ms();
        uint32_t dt  = now - last_ms;
        last_ms      = now;
        if (dt > 0) {
            ui_tick(dt);
        }

        if (shots_dir != NULL && !shots_step(shots_dir, now - start_ms)) {
            running = 0;
        }
        if (s_script && !script_step(now - start_ms)) {
            running = 0;
        }

        uint32_t next = lv_timer_handler();
        if (s_script) {
            hal_time_sim_advance_ms(SCRIPT_STEP_MS);    /* horloge virtuelle : pas d'attente */
            continue;
        }
        if (next > 33U) next = 33U;    /* cap à ~30 FPS côté veille. */
        if (next < 1U)  next = 1U;
        SDL_Delay(next);
    }
    if (s_app_on) app_save(&s_app);

    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    if (script_failures() > 0) {
        fprintf(stderr, "script : %d vérification(s) en échec\n", script_failures());
        return 1;
    }
    return 0;
}
