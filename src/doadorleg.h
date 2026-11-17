/* AUDIO FROM ANOTHER SOURCE: THE SUBTITLE MATCH, first level of the sync
 * (owner 17/11/2026).
 *
 * The donor's audio is placed against the picture by its subtitles first:
 * an embedded text subtitle track of the playing file and one in the same
 * language of the donor, both as their seek index lists them (the start of
 * every indexed block, doa_legendas: no byte more is downloaded), are aligned
 * by the subtitle sync engine of subtitle-rewrite (autosync_alinhar, the one
 * the add-on subtitles use, starts-only mode LEGENDA_DOC_FIM_ESTIMADO). The
 * result (file = k * donor + desloc) is applied at once; the audio match then
 * refines it inside +-1 s over ~40 s of playback (doa_dopasuj_fino).
 *
 * Runs on its own thread (the engine is bounded to 16 s of work); the main
 * thread polls doadorleg_resultado. */
#ifndef NV_DOADORLEG_H
#define NV_DOADORLEG_H
#include <stddef.h>
#include "dts/doador_tpk.h"

/* Starts for the session now playing (the donor's track plays): `pos` the
 * playhead (s, the file's time), for another cut the piece around it. */
void doadorleg_iniciar(double pos);
void doadorleg_cancelar(void);
/* 0 nothing yet / not started, 1 found (*k, *desloc), -1 not possible or not
 * sure (why in `motivo`). Each outcome is returned once. */
int  doadorleg_resultado(double *k, double *desloc, char *motivo, size_t n);

/* Pure parts, for the test. The pairs worth trying (file track, donor track),
 * best first: same language, then the same kind (SDH with SDH, forced never),
 * the point counts closest, SRT before ASS. Returns how many (up to max). */
int  doadorleg_pares(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb,
                     int (*casa)(const char *, const char *), int *ia, int *ib, int max);
/* When no language is shared: pairs in two languages (forced never, line
 * counts within 2x), best first. The match then needs confidence 0.90. */
int  doadorleg_pares_cruzados(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb, int *ia, int *ib, int max);
/* Start times -> a line from the two-point map. `donorDe(t)` = the donor's
 * time at the file's t. Fills k (file seconds per donor second) and desloc. */
int  doadorleg_reta(double t1, double d1, double t2, double d2, double *k, double *desloc);
/* One pair (start times of the file's track and of the donor's): 1 found. */
int  doadorleg_alinhar(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                       double pos, double *k, double *desloc, char *motivo, size_t mn, int (*cancelar)(void *), void *u);
#endif
