/*
 * Raidmaster 403 — hal_audio sur l'ESP32 : codec ES8311 (composant esp_codec_dev) en I²S standard,
 * ampli NS4150B (board_amp_enable).
 *
 * Une tâche audio (cœur 1, la boucle LVGL étant sur le cœur 0) reçoit les sons par une file. Au
 * premier son, elle allume le codec (ALDO1 du PMIC) et crée le canal I²S et le codec : le
 * démarrage de la carte n'attend pas. Pour chaque son : codec ouvert à la fréquence demandée,
 * ampli allumé, échantillons tirés par blocs et recopiés sur les deux voies, puis un peu de silence
 * pour vider le DMA avant de couper l'ampli et de fermer le codec. Volume :
 * CONFIG_RAIDMASTER_AUDIO_VOLUME. Si le codec ne répond pas, les sons suivants sont refusés.
 *
 * Licence : GPL-3.0.
 */

#include <string.h>

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "board.h"
#include "hal/hal_audio.h"

static const char *TAG = "audio";

#define BLOCK         512U      /* échantillons par bloc (23 ms à 22 050 Hz) */
#define FLUSH_BLOCKS  4U        /* silence final, plus long que la file DMA (6 × 240 trames) */
#define TASK_STACK    4096U
#define TASK_PRIO     5

typedef struct {
    uint32_t          rate;
    hal_audio_fill_fn fill;
    void             *user;
} request_t;

static QueueHandle_t          s_queue;
static i2s_chan_handle_t      s_tx;
static esp_codec_dev_handle_t s_codec;
static volatile bool          s_busy;
static volatile bool          s_failed;
static int16_t                s_mono[BLOCK];
static int16_t                s_stereo[2 * BLOCK];

static esp_err_t codec_init(void)
{
    ESP_RETURN_ON_ERROR(board_pmu_codec_power(), TAG, "ALDO1 (alimentation du codec)");

    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.auto_clear = true;     /* silence si la tâche prend du retard */
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, &s_tx, NULL), TAG, "canal I²S");
    const i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(22050),      /* MCLK = 256 × fs, refixé à chaque ouverture */
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK,
            .bclk = BOARD_I2S_BCLK,
            .ws = BOARD_I2S_WS,
            .dout = BOARD_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std), TAG, "I²S standard");

    audio_codec_i2s_cfg_t i2s_cfg = {.port = I2S_NUM_0, .tx_handle = s_tx};
    audio_codec_i2c_cfg_t i2c_cfg = {.port = I2C_NUM_0, .addr = ES8311_CODEC_DEFAULT_ADDR, .bus_handle = board_i2c_bus()};
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (data_if == NULL || ctrl_if == NULL || gpio_if == NULL) return ESP_ERR_NO_MEM;
    es8311_codec_cfg_t es_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = -1,           /* ampli sur l'expandeur, piloté ici */
        .use_mclk = true,
        .hw_gain = {.pa_voltage = 5.0f, .codec_dac_voltage = 3.3f},
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es_cfg);
    if (codec_if == NULL) return ESP_ERR_NOT_FOUND;     /* ES8311 muet sur l'I²C */
    esp_codec_dev_cfg_t dev_cfg = {.dev_type = ESP_CODEC_DEV_TYPE_OUT, .codec_if = codec_if, .data_if = data_if};
    s_codec = esp_codec_dev_new(&dev_cfg);
    return s_codec != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static void play(const request_t *r)
{
    esp_codec_dev_sample_info_t fs = {.bits_per_sample = 16, .channel = 2, .sample_rate = r->rate};
    /* esp_codec_dev désactive le canal avant de le régler à la fréquence demandée : actif ici, il ne
     * l'est plus depuis la fermeture précédente (sinon erreur « channel has not been enabled »). */
    i2s_channel_enable(s_tx);
    if (esp_codec_dev_open(s_codec, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "ouverture du codec à %lu Hz impossible", (unsigned long)r->rate);
        return;
    }
    esp_codec_dev_set_out_vol(s_codec, CONFIG_RAIDMASTER_AUDIO_VOLUME);
    board_amp_enable(true);
    size_t n;
    do {
        n = r->fill(s_mono, BLOCK, r->user);
        for (size_t i = 0; i < n; i++) s_stereo[2 * i] = s_stereo[2 * i + 1] = s_mono[i];
        memset(s_stereo + 2 * n, 0, (BLOCK - n) * 2 * sizeof(int16_t));
        esp_codec_dev_write(s_codec, s_stereo, (int)sizeof(s_stereo));
    } while (n == BLOCK);
    memset(s_stereo, 0, sizeof(s_stereo));
    for (unsigned k = 0; k < FLUSH_BLOCKS; k++) esp_codec_dev_write(s_codec, s_stereo, (int)sizeof(s_stereo));
    board_amp_enable(false);
    esp_codec_dev_close(s_codec);
}

static void audio_task(void *arg)
{
    (void)arg;
    request_t r;
    for (;;) {
        xQueueReceive(s_queue, &r, portMAX_DELAY);
        if (s_codec == NULL) {
            const esp_err_t err = codec_init();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "son indisponible (%s)", esp_err_to_name(err));
                s_failed = true;
            } else {
                ESP_LOGI(TAG, "codec ES8311 prêt, volume %d", CONFIG_RAIDMASTER_AUDIO_VOLUME);
            }
        }
        if (s_codec != NULL) play(&r);
        s_busy = false;
    }
}

bool hal_audio_play(uint32_t rate, hal_audio_fill_fn fill, void *user)
{
    if (s_failed || s_busy) return false;
    if (s_queue == NULL) {
        s_queue = xQueueCreate(1, sizeof(request_t));
        if (s_queue == NULL ||
            xTaskCreatePinnedToCore(audio_task, "audio", TASK_STACK, NULL, TASK_PRIO, NULL, 1) != pdPASS) {
            ESP_LOGW(TAG, "tâche audio impossible à créer");
            s_failed = true;
            return false;
        }
    }
    const request_t r = {.rate = rate, .fill = fill, .user = user};
    s_busy = true;
    if (xQueueSend(s_queue, &r, 0) != pdTRUE) {
        s_busy = false;
        return false;
    }
    return true;
}

bool hal_audio_busy(void)
{
    return s_busy;
}
