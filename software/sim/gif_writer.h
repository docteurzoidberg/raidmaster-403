/*
 * Raidmaster 403 — encodeur GIF animé minimal pour le simulateur (mode enregistrement démo).
 *
 * Accumule des images RGB565 plein écran, puis écrit un GIF89a en boucle infinie :
 *   - palette globale unique construite sur l'histogramme de toutes les images (couleurs
 *     exactes si ≤ 255 couleurs distinctes, sinon sélection par fréquence + couleur la plus
 *     proche, sans tramage : les aplats restent purs) ;
 *   - une image par changement, réduite au rectangle modifié, pixels inchangés transparents ;
 *   - images identiques consécutives fusionnées (délai cumulé) ;
 *   - option d'agrandissement entier (pixels dupliqués).
 *
 * Les images sont stockées dans un fichier temporaire anonyme (tmpfile(), suit $TMPDIR) pour ne
 * pas tenir des centaines d'images en mémoire.
 *
 * Licence : GPL-3.0.
 */

#ifndef RAIDMASTER_GIF_WRITER_H
#define RAIDMASTER_GIF_WRITER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gif_rec gif_rec_t;

/* Crée un enregistreur pour des images w×h, agrandies ×scale (1..4) à l'écriture. */
gif_rec_t *gif_rec_create(int w, int h, int scale);

/* Ajoute une image RGB565 (w×h, ligne par ligne) affichée delay_cs centièmes de seconde.
 * Renvoie 0 si OK. */
int gif_rec_add_frame(gif_rec_t *rec, const uint16_t *rgb565, unsigned delay_cs);

/* Construit la palette et écrit le GIF. Renvoie 0 si OK. */
int gif_rec_write(gif_rec_t *rec, const char *path);

/* Nombre d'images distinctes et durée totale (cs) accumulées. */
int      gif_rec_frame_count(const gif_rec_t *rec);
unsigned gif_rec_duration_cs(const gif_rec_t *rec);

void gif_rec_free(gif_rec_t *rec);

#ifdef __cplusplus
}
#endif

#endif /* RAIDMASTER_GIF_WRITER_H */
