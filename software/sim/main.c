/*
 * Raidmaster 403 — simulateur PC.
 *
 * Fenêtre SDL 480×320 landscape (résolution exacte du ST7796 de la carte Waveshare
 * ESP32-S3-Touch-LCD-3.5), avec LVGL v9 dessinant dedans via un buffer software + SDL_Texture.
 *
 * Pourquoi ce chemin plutôt que LV_USE_SDL=1 : on garde le contrôle total des SDL_Event, ce qui
 * permet de router les touches a/b/+/-/m vers notre ui_input_dispatch() sans combat avec le
 * driver clavier interne de LVGL. Fallback acceptable d'après prompts/ui.md (dérogation UI-agent).
 *
 * Le core (core/app) tourne comme sur l'ESP32 : GPS fourni par hal_sim (rejeu d'un .nmea ou
 * trajet simulé), réglages et distances sauvegardés dans sim-storage.json.
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
 *                  <ms> power_cut  coupure d'alimentation : le core redémarre sur la dernière
 *                                  sauvegarde, sans sauvegarder (l'UI et le GPS continuent)
 *                  <ms> shot <nom> capture <dossier>/<nom>.ppm
 *                  <ms> expect <champ> <valeur> [tolérance]
 *                                  champs : trip_a trip_b odo (km), speed avg max (km/h), calib (%),
 *                                  active (0 A, 1 B), fix, night, view (0 raid, 1 balade),
 *                                  hhmm (heure locale, -1 inconnue), alarm, chrono (en marche),
 *                                  gps_err (phrases rejetées : checksum ou champs illisibles)
 *                  <ms> quit
 *                <ms> : temps depuis le lancement, sur une horloge virtuelle (pas de 10 ms, sans
 *                attente) : résultat déterministe, une minute se joue en une fraction de seconde.
 *                « @<s> » à la place : instant <s> de la trace rejouée (= s × 1000 / --speed ms).
 *                Code de sortie 1 si un expect échoue.
 *
 * Touches clavier (voir prompts/ui.md § 4 et prompts/simulation.md § 1) :
 *   a / b / + ou = / - / m        → boutons physiques A, B, +, -, Menu (appui court)
 *   Shift + touche                → appui long (Shift + m : entrée / sortie du menu réglages)
 *   n ou Tab                      → bascule mode nuit
 *   v                             → cycle entre écrans raid / balade / menu / calib (debug)
 *   d                             → coupe / rétablit le signal GPS (tunnel)
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
 * UI-agent exception: integration LVGL (voir prompts/ui.md).
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
#include "app.h"
#include "hal/hal_gps.h"
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

/* État fixe = valeurs de la maquette mode raid jour. */
static void push_mockup_state(void)
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
    st.battery_pct = 68;
    st.t_cabin_c   = 22.0f;
    snprintf(st.time_hhmm, sizeof(st.time_hhmm), "14:23");
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
        case 1:  ui_demo_stop(); ui_show_screen(UI_SCREEN_RAID); push_mockup_state(); break;
        case 2:  save_ppm(dir, "2-raid"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 3:  save_ppm(dir, "3-balade"); ui_show_screen(UI_SCREEN_MENU);
                 screen_menu_move(+1); break;
        case 4:  save_ppm(dir, "4-menu"); ui_show_screen(UI_SCREEN_CALIB); break;
        case 5:  save_ppm(dir, "5-calib"); ui_show_screen(UI_SCREEN_RAID);
                 ui_theme_toggle_night(); break;
        case 6:  save_ppm(dir, "6-raid-nuit"); ui_show_screen(UI_SCREEN_BALADE); break;
        case 7:  save_ppm(dir, "7-balade-nuit"); return 0;
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
/* Mode script : actions horodatées (voir l'en-tête). */

#define SCRIPT_STEP_MS 10U

static FILE       *s_script;
static const char *s_script_out = ".";
static uint32_t    s_script_at;
static char        s_script_cmd[160];
static bool        s_script_pending;
static int         s_script_failures;
static double      s_script_speed = 1.0;    /* --speed, pour les instants « @<s> » */

/* expect <champ> <valeur> [tolérance] sur l'état du core. */
static void script_expect(const char *args)
{
    char field[32];
    double want, tol = 0.0;
    int n = sscanf(args, "%31s %lf %lf", field, &want, &tol);
    if (n < 2) {
        fprintf(stderr, "script : expect mal formé « %s »\n", args);
        s_script_failures++;
        return;
    }
    app_state_t st;
    app_get_state(&s_app, hal_time_ms(), &st);
    double got;
    if      (strcmp(field, "trip_a") == 0) got = st.trip.trip_a_m / 1000.0;
    else if (strcmp(field, "trip_b") == 0) got = st.trip.trip_b_m / 1000.0;
    else if (strcmp(field, "odo") == 0)    got = st.trip.odo_m / 1000.0;
    else if (strcmp(field, "speed") == 0)  got = st.trip.speed_kmh;
    else if (strcmp(field, "calib") == 0)  got = st.trip.calib_pct;
    else if (strcmp(field, "active") == 0) got = st.active;
    else if (strcmp(field, "fix") == 0)    got = st.trip.fix_ok;
    else if (strcmp(field, "night") == 0)  got = st.set.night;
    else if (strcmp(field, "view") == 0)   got = st.set.view;
    else if (strcmp(field, "avg") == 0)    got = st.trip.avg_kmh;
    else if (strcmp(field, "max") == 0)    got = st.trip.max_kmh;
    else if (strcmp(field, "hhmm") == 0)   got = st.local_hh < 0 ? -1 : st.local_hh * 100 + st.local_mm;
    else if (strcmp(field, "alarm") == 0)  got = st.speed_alarm;
    else if (strcmp(field, "chrono") == 0) got = st.trip.chrono_running;
    else if (strcmp(field, "gps_err") == 0)
        got = s_app.parser.stats.bad_checksum + s_app.parser.stats.parse_errors;
    else {
        fprintf(stderr, "script : champ inconnu « %s »\n", field);
        s_script_failures++;
        return;
    }
    const bool ok = fabs(got - want) <= tol + 1e-9;
    printf("expect %-7s %10.4f (attendu %.4f ± %g) %s\n", field, got, want, tol, ok ? "ok" : "ÉCHEC");
    if (!ok) s_script_failures++;
}

static bool script_next(void)
{
    char line[200];
    while (fgets(line, sizeof(line), s_script) != NULL) {
        char *h = strchr(line, '#');
        if (h != NULL) *h = '\0';
        unsigned at;
        double at_s;
        int n = 0;
        if (sscanf(line, " @%lf %n", &at_s, &n) == 1 && line[n] != '\0' && line[n] != '\n') {
            s_script_at = (uint32_t)llround(at_s * 1000.0 / s_script_speed);
            snprintf(s_script_cmd, sizeof(s_script_cmd), "%s", line + n);
            s_script_cmd[strcspn(s_script_cmd, "\r\n")] = '\0';
            return true;
        }
        n = 0;
        if (sscanf(line, "%u %n", &at, &n) == 1 && line[n] != '\0' && line[n] != '\n') {
            s_script_at = at;
            snprintf(s_script_cmd, sizeof(s_script_cmd), "%s", line + n);
            s_script_cmd[strcspn(s_script_cmd, "\r\n")] = '\0';
            return true;
        }
    }
    return false;
}

static void dispatch_named(const char *name, bool *ok)
{
    static const struct { const char *n; ui_button_t b; } map[] = {
        {"a", UI_BTN_A}, {"b", UI_BTN_B}, {"plus", UI_BTN_PLUS}, {"minus", UI_BTN_MINUS}, {"m", UI_BTN_MENU},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcmp(name, map[i].n) == 0) {
            ui_input_dispatch(map[i].b, UI_BTN_EV_PRESS);
            return;
        }
        if (SDL_strcasecmp(name, map[i].n) == 0) {
            ui_input_dispatch(map[i].b, UI_BTN_EV_LONGPRESS);
            return;
        }
    }
    *ok = false;
}

/* Joue les actions échues ; renvoie 0 pour quitter. */
static int script_step(uint32_t t_ms)
{
    while (s_script_pending && t_ms >= s_script_at) {
        char verb[32] = "", arg[128] = "";
        sscanf(s_script_cmd, "%31s %127s", verb, arg);
        bool ok = true;
        if (strcmp(verb, "quit") == 0) {
            return 0;
        } else if (strcmp(verb, "shot") == 0 && arg[0] != '\0') {
            lv_refr_now(NULL);
            save_ppm(s_script_out, arg);
        } else if (strcmp(verb, "expect") == 0) {
            script_expect(s_script_cmd + strlen("expect"));
        } else if (strcmp(verb, "night") == 0) {
            ui_app_toggle_night();
        } else if (strcmp(verb, "signal") == 0) {
            hal_gps_sim_set_signal(!hal_gps_sim_signal());
        } else if (strcmp(verb, "power_cut") == 0) {
            app_init(&s_app, hal_time_ms());
        } else {
            dispatch_named(verb, &ok);
        }
        if (!ok) fprintf(stderr, "script : action inconnue « %s »\n", s_script_cmd);
        s_script_pending = script_next();
    }
    return s_script_pending ? 1 : 0;
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
        s_script = fopen(script, "r");
        if (s_script == NULL) {
            fprintf(stderr, "--script : impossible de lire %s\n", script);
            return 2;
        }
        s_script_speed = speed;
        s_script_pending = script_next();
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
        ui_app_attach(&s_app);
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
        if (s_script != NULL && !script_step(now - start_ms)) {
            running = 0;
        }

        uint32_t next = lv_timer_handler();
        if (s_script != NULL) {
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
    if (s_script_failures > 0) {
        fprintf(stderr, "script : %d vérification(s) en échec\n", s_script_failures);
        return 1;
    }
    return 0;
}
