# Composant principal du firmware : boucle (main.c) et boutons de banc sur le tactile.
idf_component_register(
    SRCS main.c touch_buttons.c
    INCLUDE_DIRS "."
    PRIV_REQUIRES core ui hal_esp32 esp_timer
)
