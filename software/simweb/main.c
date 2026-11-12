/*
 * Raidmaster 403 — simulateur web (WebAssembly, emcc) : le core et l'UI du sim, pilotés par la page
 * mockup/sim/ (ou par simweb/tests/test_web.cjs sous Node).
 *
 * Mêmes sources que le sim PC (core/, ui/, hal_sim/ par hal_simweb/, lecteur des actions.txt
 * sim/script.c), sans SDL : LVGL dessine dans un framebuffer RGBA 480 × 320 que la page copie dans
 * la texture de l'écran du modèle 3D, le son passe par Web Audio, la mémoire par le localStorage.
 *
 * Temps : horloge virtuelle, avancée par pas de 10 ms comme en mode script du sim PC (mêmes
 * résultats, au pas près, pour les expect). La page donne à chaque image le temps réel écoulé ;
 * l'horloge avance de ce temps × la vitesse choisie (×1 pendant l'écran de démarrage, pour voir
 * l'animation). LVGL ne dessine qu'une fois par image (minuterie de rafraîchissement suspendue,
 * lv_refr_now à la fin de l'image) : à ×100, une image couvre 160 pas.
 *
 * Fonctions exportées (préfixe « _ » côté JS) :
 *   sim_start(nmea, vitesse, actions, persistant)  une fois par module ; chaînes vides : trajet
 *                       simulé, sans actions. Renvoie 0, ou 1 trace illisible, 2 actions illisibles.
 *   sim_frame(ms_réels) avance l'horloge et dessine ; sim_run(ms) avance sans attendre ni dessiner
 *                       (tests), sim_render() dessine.
 *   sim_set_rate(r)     vitesse de l'horloge (temps virtuel / temps réel).
 *   sim_button(id, long) id : 0 A, 1 B, 2 +, 3 −, 4 Menu ; long : appui long.
 *   sim_key(touche)     mêmes touches que le sim PC : n nuit, v écran suivant, d signal GPS,
 *                       g récepteur débranché, c boussole débranchée, r rotation sur place,
 *                       p alimentation suivante.
 *   sim_fb()            adresse du framebuffer RGBA ; sim_info() adresse du tableau d'état (doubles,
 *                       voir INFO_*).
 *   sim_save()          sauvegarde (fermeture de la page).
 * Rappels vers la page : Module.onExpect(champ, obtenu, attendu, tolérance, ok, t_ms),
 * Module.onScriptEnd(échecs) ; journal : printf → Module.print.
 *
 * Licence : GPL-3.0.
 */

#include <emscripten.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "app.h"
#include "gps_parser.h"
#include "hal/hal_gps.h"
#include "hal/hal_mag.h"
#include "hal/hal_time.h"
#include "hal_simweb.h"
#include "jingles.h"
#include "lvgl.h"
#include "script.h"
#include "theme.h"
#include "ui.h"
#include "ui_app.h"
#include "ui_input.h"

enum {
    SCREEN_W = 480,
    SCREEN_H = 320,
    BUF_LINES = 40,
    STEP_MS = 10,                 /* pas de l'horloge virtuelle, comme le mode script du sim PC */
    MAX_STEPS_PER_FRAME = 20000,  /* 200 s de temps simulé au plus par image */
};

/* Tableau d'état lu par la page (sim_info). */
enum {
    INFO_T_MS = 0,      /* temps depuis le lancement (horloge virtuelle) */
    INFO_POS_OK,        /* position connue (dernier fix valide reçu du récepteur) */
    INFO_LAT,
    INFO_LON,
    INFO_SPEED,         /* km/h, vitesse lissée du core */
    INFO_COURSE,        /* cap GPS de la dernière RMC, -1 inconnu */
    INFO_FIX,           /* fix accepté par le core */
    INFO_TRIP_A,        /* km */
    INFO_TRIP_B,
    INFO_ODO,
    INFO_SCRIPT,        /* 0 sans actions, 1 en cours, 2 fini */
    INFO_FAILURES,
    INFO_SIGNAL,
    INFO_GPS_PLUGGED,
    INFO_MAG_PLUGGED,
    INFO_SPIN,
    INFO_NIGHT,
    INFO_GPS_PRESENT,
    INFO_SCREEN,        /* ui_screen_id_t */
    INFO_FB_SERIAL,     /* change à chaque image dessinée */
    INFO_HEADING,       /* cap affiché, -1 inconnu */
    INFO_RATE,
    INFO__COUNT
};

static uint16_t     s_lv_buf[SCREEN_W * BUF_LINES];
static uint8_t      s_rgba[SCREEN_W * SCREEN_H * 4];
static uint32_t     s_fb_serial;
static lv_display_t *s_disp;
static app_t        s_app;
static bool         s_on;
static int          s_script;           /* INFO_SCRIPT */
static uint32_t     s_start_ms, s_last_ms;
static double       s_rate = 1.0, s_budget_ms;
static double       s_info[INFO__COUNT];

/* Position pour la carte : les lignes reçues du récepteur, décodées à part du core. */
static gps_parser_t s_map_parser;
static gps_fix_t    s_map_fix;
static bool         s_map_ok;

/* clang-format off */
EM_JS(void, js_expect, (const char *field, double got, double want, double tol, int ok, double t_ms), {
    if (Module.onExpect) Module.onExpect(UTF8ToString(field), got, want, tol, ok !== 0, t_ms);
})

EM_JS(void, js_script_end, (int failures), {
    if (Module.onScriptEnd) Module.onScriptEnd(failures);
})
/* clang-format on */

/* ------------------------------------------------------------------------- */

static void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int w = area->x2 - area->x1 + 1;
    const uint16_t *src = (const uint16_t *)(void *)px_map;
    for (int y = area->y1; y <= area->y2; y++) {
        uint8_t *o = &s_rgba[(y * SCREEN_W + area->x1) * 4];
        for (int x = 0; x < w; x++, o += 4) {
            const uint16_t p = *src++;
            /* même conversion que les captures PPM du sim PC */
            o[0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
            o[1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
            o[2] = (uint8_t)((p & 0x1F) * 255 / 31);
            o[3] = 255;
        }
    }
    if (lv_display_flush_is_last(disp)) s_fb_serial++;
    lv_display_flush_ready(disp);
}

static void on_gps_line(const char *line, size_t len, void *user)
{
    (void)len;
    gps_fix_t fix;
    if (gps_parser_feed(&s_map_parser, line, &fix) && fix.valid) {
        s_map_fix = fix;
        s_map_ok = true;
    }
    app_gps_line((app_t *)user, line, hal_time_ms());
}

/* shot <nom> : capture /out/<nom>.ppm dans le système de fichiers du module. */
static void script_shot(const char *name, void *user)
{
    (void)user;
    lv_refr_now(NULL);
    char path[160];
    snprintf(path, sizeof(path), "/out/%s.ppm", name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "capture : impossible d'écrire %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fwrite(&s_rgba[i * 4], 1, 3, f);
    fclose(f);
    printf("capture : %s\n", path);
}

static void script_expect(const char *field, double got, double want, double tol, bool ok, void *user)
{
    (void)user;
    js_expect(field, got, want, tol, ok, (double)(hal_time_ms() - s_start_ms));
}

static void update_info(void)
{
    app_state_t st;
    app_get_state(&s_app, hal_time_ms(), &st);
    s_info[INFO_T_MS] = (double)(hal_time_ms() - s_start_ms);
    s_info[INFO_POS_OK] = s_map_ok;
    s_info[INFO_LAT] = s_map_fix.lat_deg;
    s_info[INFO_LON] = s_map_fix.lon_deg;
    s_info[INFO_SPEED] = st.trip.speed_kmh;
    s_info[INFO_COURSE] = isnan(s_map_fix.course_deg) ? -1.0 : s_map_fix.course_deg;
    s_info[INFO_FIX] = st.trip.fix_ok;
    s_info[INFO_TRIP_A] = st.trip.trip_a_m / 1000.0;
    s_info[INFO_TRIP_B] = st.trip.trip_b_m / 1000.0;
    s_info[INFO_ODO] = st.trip.odo_m / 1000.0;
    s_info[INFO_SCRIPT] = s_script;
    s_info[INFO_FAILURES] = script_failures();
    s_info[INFO_SIGNAL] = hal_gps_sim_signal();
    s_info[INFO_GPS_PLUGGED] = hal_gps_sim_plugged();
    s_info[INFO_MAG_PLUGGED] = hal_mag_sim_plugged();
    s_info[INFO_SPIN] = hal_mag_sim_spin();
    s_info[INFO_NIGHT] = st.set.night;
    s_info[INFO_GPS_PRESENT] = st.gps_present;
    s_info[INFO_SCREEN] = ui_current_screen();
    s_info[INFO_FB_SERIAL] = s_fb_serial;
    s_info[INFO_HEADING] = isnan(st.heading_deg) ? -1.0 : st.heading_deg;
    s_info[INFO_RATE] = s_rate;
}

/* Un pas de 10 ms, dans l'ordre de la boucle du sim PC. */
static void step(void)
{
    hal_gps_poll();
    mag_vec_t mag;
    if (hal_mag_poll(&mag)) app_mag_sample(&s_app, mag, hal_time_ms());
    app_tick(&s_app, hal_time_ms());

    const uint32_t now = (uint32_t)hal_time_ms();
    const uint32_t dt = now - s_last_ms;
    s_last_ms = now;
    if (dt > 0) ui_tick(dt);

    if (s_script == 1 && !script_step(now - s_start_ms)) {
        s_script = 2;
        const int n = script_failures();
        if (n > 0) printf("script : %d vérification(s) en échec\n", n);
        else       printf("script : terminé, vérifications en ordre\n");
        js_script_end(n);
    }
    lv_timer_handler();
    hal_time_sim_advance_ms(STEP_MS);
}

/* ------------------------------------------------------------------------- */

EMSCRIPTEN_KEEPALIVE
int sim_start(const char *nmea, double speed, const char *actions, int persistent)
{
    if (s_on) return -1;
    hal_time_sim_virtual(true);
    mkdir("/out", 0777);

    lv_init();
    s_disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(s_disp, flush);
    lv_display_set_buffers(s_disp, s_lv_buf, NULL, sizeof(s_lv_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    if (speed < 1.0) speed = 1.0;
    if (!hal_gps_sim_config(nmea[0] != '\0' ? nmea : NULL, speed, false)) {
        fprintf(stderr, "trace illisible : %s\n", nmea);
        return 1;
    }
    hal_storage_web_open(persistent != 0);
    if (actions[0] != '\0') {
        const script_io_t io = {.shot = script_shot, .expect = script_expect};
        if (!script_open(actions, speed, &s_app, &io)) {
            fprintf(stderr, "actions illisibles : %s\n", actions);
            return 2;
        }
        s_script = 1;
    }
    app_init(&s_app, hal_time_ms());
    ui_theme_set_night(s_app.set.night);
    gps_parser_init(&s_map_parser);

    ui_init(s_disp);
    hal_gps_init(on_gps_line, &s_app);
    hal_mag_init();
    ui_app_attach(&s_app);
    jingle_play(&JINGLE_BOOT);      /* avec l'animation de démarrage (perdu sans contexte audio) */
    lv_timer_pause(lv_display_get_refr_timer(s_disp));     /* dessin : une fois par image */

    s_start_ms = s_last_ms = (uint32_t)hal_time_ms();
    s_on = true;
    printf("[sim] GPS : %s (×%g), actions : %s, mémoire : %s\n", nmea[0] ? nmea : "trajet simulé", speed,
           actions[0] ? actions : "aucune", persistent ? "navigateur" : "session");
    update_info();
    return 0;
}

EMSCRIPTEN_KEEPALIVE
void sim_set_rate(double rate)
{
    s_rate = rate > 0.0 ? rate : 1.0;
}

EMSCRIPTEN_KEEPALIVE
void sim_render(void)
{
    if (!s_on) return;
    lv_refr_now(s_disp);
    update_info();
}

EMSCRIPTEN_KEEPALIVE
void sim_frame(double real_ms)
{
    if (!s_on) return;
    if (real_ms > 1000.0) real_ms = 1000.0;    /* onglet en arrière-plan : on ne rattrape pas */
    double rate = s_rate;
    if (ui_current_screen() == UI_SCREEN_BOOT && rate > 1.0) rate = 1.0;
    s_budget_ms += real_ms * rate;
    int n = (int)(s_budget_ms / STEP_MS);
    if (n > MAX_STEPS_PER_FRAME) {
        n = MAX_STEPS_PER_FRAME;
        s_budget_ms = 0.0;
    } else {
        s_budget_ms -= n * (double)STEP_MS;
    }
    for (int i = 0; i < n; i++) step();
    sim_render();
}

EMSCRIPTEN_KEEPALIVE
int sim_run(double ms)
{
    if (!s_on) return -1;
    for (int n = (int)(ms / STEP_MS); n > 0; n--) step();
    update_info();
    return s_script;
}

EMSCRIPTEN_KEEPALIVE
void sim_button(int id, int long_press)
{
    if (!s_on || id < 0 || id >= UI_BTN__COUNT) return;
    ui_input_dispatch((ui_button_t)id, long_press ? UI_BTN_EV_LONGPRESS : UI_BTN_EV_PRESS);
}

EMSCRIPTEN_KEEPALIVE
void sim_key(int key)
{
    if (!s_on) return;
    switch (key) {
        case 'n':
            ui_app_toggle_night();
            break;
        case 'v':
            ui_cycle_screen();
            break;
        case 'p':
            printf("[sim] alimentation : %s\n", hal_power_sim_cycle());
            break;
        case 'g':
            hal_gps_sim_set_plugged(!hal_gps_sim_plugged());
            printf("[sim] récepteur GPS %s\n", hal_gps_sim_plugged() ? "rebranché (9 600 bauds)" : "débranché");
            break;
        case 'c':
            hal_mag_sim_set_plugged(!hal_mag_sim_plugged());
            printf("[sim] boussole %s\n", hal_mag_sim_plugged() ? "rebranchée" : "débranchée");
            break;
        case 'r':
            hal_mag_sim_set_spin(!hal_mag_sim_spin());
            printf("[sim] rotation sur place %s\n", hal_mag_sim_spin() ? "en cours" : "arrêtée");
            break;
        case 'd':
            hal_gps_sim_set_signal(!hal_gps_sim_signal());
            printf("[sim] signal GPS %s\n", hal_gps_sim_signal() ? "rétabli" : "coupé");
            break;
        default:
            break;
    }
    update_info();
}

EMSCRIPTEN_KEEPALIVE
uint8_t *sim_fb(void)
{
    return s_rgba;
}

EMSCRIPTEN_KEEPALIVE
double *sim_info(void)
{
    return s_info;
}

EMSCRIPTEN_KEEPALIVE
void sim_save(void)
{
    if (s_on) app_save(&s_app);
}
