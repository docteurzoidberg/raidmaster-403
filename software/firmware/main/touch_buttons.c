/*
 * Raidmaster 403 — boutons de banc (voir touch_buttons.h).
 *
 * Mêmes évènements que les touches du sim : appui court au relâchement, appui long dès 800 ms
 * de maintien (prompts/ui.md § 4.1). Le relâchement n'est pris qu'après deux lectures « pas de
 * contact » de suite (40 ms), pour qu'une lecture manquée ne fasse pas un appui court.
 *
 * Licence : GPL-3.0.
 */

#include "touch_buttons.h"

#include <inttypes.h>
#include <stdbool.h>

#include "esp_log.h"

#include "board.h"
#include "ui_input.h"

static const char *TAG = "touch_buttons";

#define POLL_MS     20U
#define LONG_MS     800U
#define RELEASE_N   2
#define BANDS       5

static const ui_button_t s_band_btn[BANDS] = {UI_BTN_A, UI_BTN_B, UI_BTN_MINUS, UI_BTN_PLUS, UI_BTN_MENU};
static const char *const s_band_name[BANDS] = {"A", "B", "-", "+", "Menu"};

static uint64_t s_next_poll_ms;
static bool     s_down;
static bool     s_long_sent;
static int      s_band;
static int      s_up_reads;
static uint64_t s_down_ms;

void touch_buttons_poll(uint64_t now_ms)
{
    if (now_ms < s_next_poll_ms) return;
    s_next_poll_ms = now_ms + POLL_MS;

    int32_t x, y;
    const bool pressed = board_touch_read(&x, &y);
    if (pressed) {
        s_up_reads = 0;
        if (!s_down) {
            s_down = true;
            s_long_sent = false;
            s_down_ms = now_ms;
            s_band = (int)(x * BANDS / BOARD_UI_W);
            if (s_band < 0) s_band = 0;
            if (s_band >= BANDS) s_band = BANDS - 1;
            ESP_LOGI(TAG, "tactile (%" PRId32 ", %" PRId32 ") → bouton %s", x, y, s_band_name[s_band]);
        } else if (!s_long_sent && now_ms - s_down_ms >= LONG_MS) {
            s_long_sent = true;
            ui_input_dispatch(s_band_btn[s_band], UI_BTN_EV_LONGPRESS);
        }
    } else if (s_down && ++s_up_reads >= RELEASE_N) {
        s_down = false;
        if (!s_long_sent) ui_input_dispatch(s_band_btn[s_band], UI_BTN_EV_PRESS);
    }
}
