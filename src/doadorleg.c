// See doadorleg.h.
#include "doadorleg.h"
#include "autosync.h"
#include "legenda.h"
#include "linguas.h"
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DL_PARES   3        // pairs tried, best first
#define DL_MAXPTS  6000     // points per track (the engine takes up to 8000 cues)
#define DL_FIM_MAX 4.0      // an estimated end: the next start, at most this after its own

static int casaPadrao(const char *a, const char *b) { return a[0] && b[0] && !strcasecmp(a, b); }
static int contem(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}
static int ehSdh(const DoaLegInfo *l) { return contem(l->nome, "sdh") || contem(l->nome, "hearing") || contem(l->nome, "cc"); }
static int ehAss(const DoaLegInfo *l) { return !strcmp(l->codec, "ass") || !strcmp(l->codec, "ssa"); }

int doadorleg_pares(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb,
                    int (*casa)(const char *, const char *), int *ia, int *ib, int max) {
  struct { int i, j; double p; } c[64], t;
  int n = 0, i, j, k;
  if (!casa) casa = casaPadrao;
  for (i = 0; i < na; i++)
    for (j = 0; j < nb && n < 64; j++) {
      double r;
      if (a[i].forcada || b[j].forcada) continue;
      // The same language (an undetermined one only with an undetermined one).
      if (a[i].lingua[0] || b[j].lingua[0]) {
        if (!a[i].lingua[0] || !b[j].lingua[0] || !(casa(a[i].lingua, b[j].lingua) || casa(b[j].lingua, a[i].lingua))) continue;
      }
      r = a[i].n > b[j].n ? (double)b[j].n / a[i].n : (double)a[i].n / b[j].n;   // 1 = as many lines
      if (r < 0.5) continue;   // one of them is not the whole dialogue
      c[n].i = i; c[n].j = j;
      c[n].p = r * 10.0 + (ehSdh(&a[i]) == ehSdh(&b[j]) ? 5.0 : 0.0) + (!ehAss(&a[i]) && !ehAss(&b[j]) ? 2.0 : 0.0) +
               (!a[i].lingua[0] ? -3.0 : 0.0);
      n++;
    }
  for (i = 1; i < n; i++) for (k = i; k > 0 && c[k].p > c[k - 1].p; k--) { t = c[k]; c[k] = c[k - 1]; c[k - 1] = t; }
  for (i = 0; i < n && i < max; i++) { ia[i] = c[i].i; ib[i] = c[i].j; }
  return n < max ? n : max;
}

int doadorleg_reta(double t1, double d1, double t2, double d2, double *k, double *desloc) {
  static const double CONH[] = { 24000.0 / 23976.0, 25.0 / 24.0, 25000.0 / 23976.0 };
  double kk;
  unsigned i;
  if (!isfinite(d1) || !isfinite(d2) || fabs(d2 - d1) < 30.0) return 0;
  kk = (t2 - t1) / (d2 - d1);
  if (kk < 0.95 || kk > 1.05) return 0;
  // The same speed within the lines' own precision, or a known film/TV ratio.
  if (fabs(kk - 1.0) < 0.0003) kk = 1.0;
  for (i = 0; i < sizeof CONH / sizeof *CONH; i++) {
    if (fabs(kk - CONH[i]) < 0.0003) kk = CONH[i];
    if (fabs(kk - 1.0 / CONH[i]) < 0.0003) kk = 1.0 / CONH[i];
  }
  *k = kk;
  *desloc = ((t1 - kk * d1) + (t2 - kk * d2)) / 2.0;
  return 1;
}

// A track's start times as a timing-only document: each line runs to the next
// start (at most DL_FIM_MAX). ASS: a burst (karaoke, a typeset sign drawn in
// pieces) keeps its first point only.
static LegendaDocumento *documento(const double *t, int n, int ass, const char *lingua, const char *quem, double *span) {
  LegendaCue *v = calloc((size_t)n, sizeof *v);
  LegendaDocumentoInfo info;
  LegendaDocumento *d;
  int i, m = 0;
  if (!v) return NULL;
  for (i = 0; i < n; i++) {
    double ini = t[i], fim;
    if (ini < 0 || (m && ini - v[m - 1].inicio < (ass ? 0.15 : 0.02))) continue;
    fim = i + 1 < n ? t[i + 1] - 0.05 : ini + 2.0;
    if (fim > ini + DL_FIM_MAX) fim = ini + DL_FIM_MAX;
    if (fim < ini + 0.3) fim = ini + 0.3;
    v[m].inicio = ini; v[m].fim = fim;
    snprintf(v[m].texto, sizeof v[m].texto, "line %05d", m);
    m++;
  }
  memset(&info, 0, sizeof info);
  snprintf(info.idioma, sizeof info.idioma, "%s", lingua);
  snprintf(info.origem, sizeof info.origem, "%s index", quem);
  snprintf(info.identidade, sizeof info.identidade, "doador:%s:%d", quem, m);
  info.flags = LEGENDA_DOC_COMPLETO | LEGENDA_DOC_FIM_ESTIMADO;
  if (span) { span[0] = m ? v[0].inicio : 0; span[1] = m ? v[m - 1].inicio : 0; }
  d = m >= 20 ? legenda_documento_de_cues(v, m, &info) : NULL;
  free(v);
  return d;
}

typedef struct {
  int ia[DL_PARES], ib[DL_PARES], n;
  DoaLegInfo a[DOA_LEG_MAX], b[DOA_LEG_MAX];
  double *ta[DL_PARES], *tb[DL_PARES]; int na[DL_PARES], nb[DL_PARES];
  double pos;
  unsigned ger;
} Pedido;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint geracao;
static int estado;               // 0 nothing, 1 working, 2 found, 3 not
static double rk, rd;
static char rMotivo[160];

static int cancelado(void *u) { return (unsigned)(uintptr_t)u != atomic_load(&geracao); }

static void terminar(unsigned ger, int achou, double k, double d, const char *motivo) {
  pthread_mutex_lock(&trava);
  if (ger == atomic_load(&geracao)) {
    estado = achou ? 2 : 3; rk = k; rd = d;
    snprintf(rMotivo, sizeof rMotivo, "%s", motivo ? motivo : "");
  }
  pthread_mutex_unlock(&trava);
}

int doadorleg_alinhar(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double *k, double *desloc, char *motivo, size_t mn, int (*cancelar)(void *), void *u) {
  double sa[2], sb[2];
  LegendaDocumento *ref = documento(ta, na, assA, la, "file", sa);
  LegendaDocumento *doc = documento(tb, nb, assB, lb, "donor", sb);
  AutoSyncConfig cfg = autosync_config(AUTOSYNC_THOROUGH);
  AutoSyncMapa *mapa = NULL;
  AutoSyncResultado r;
  struct timespec t0, t1;
  int achou = 0;
  if (!ref || !doc) {
    snprintf(motivo, mn, "too few lines");
    legenda_documento_liberar(ref); legenda_documento_liberar(doc);
    return 0;
  }
  clock_gettime(CLOCK_MONOTONIC, &t0);
  cfg.manterMs = 0;   // even a small offset counts: the audio refines from it
  r = autosync_alinhar(doc, ref, &cfg, cancelar, u, &mapa);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  printf("[doadorleg] file %d points (%.0f-%.0f s) / donor %d points (%.0f-%.0f s): %s (%s, offset %d ms, scale %.6f, pieces %d, "
         "confidence %.2f, coverage %.2f) in %.0f ms\n", na, sa[0], sa[1], nb, sb[0], sb[1],
         r.estado == AUTOSYNC_ACCEPTED ? "ACCEPTED" : "not accepted", autosync_motivo(r.motivo), r.offsetMs, r.escala, r.trechos,
         r.confianca, r.cobertura, (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6);
  if (r.estado == AUTOSYNC_ACCEPTED) {
    // Two points of the file's time, from the playhead on (another cut: the
    // piece the viewer is in), mapped to the donor's.
    double t1s = pos > sa[0] + 60 ? pos : sa[0] + (sa[1] - sa[0]) * 0.25, t2s = t1s + 600.0, d1, d2;
    if (t2s > sa[1]) { t2s = t1s; t1s = t2s - 600.0; }
    if (t1s < sa[0]) t1s = sa[0];
    d1 = mapa ? autosync_mapa_tempo(mapa, t1s) : t1s + r.offsetMs / 1000.0;
    d2 = mapa ? autosync_mapa_tempo(mapa, t2s) : t2s + r.offsetMs / 1000.0;
    if (d1 > AUTOSYNC_SEM_LEGENDA / 2 && d2 > AUTOSYNC_SEM_LEGENDA / 2 && doadorleg_reta(t1s, d1, t2s, d2, k, desloc)) achou = 1;
    else if (!mapa) { *k = 1.0; *desloc = -r.offsetMs / 1000.0; achou = 1; }
    else snprintf(motivo, mn, "accepted, but no straight line around %.0f s", pos);
    if (achou)
      printf("[doadorleg] the file's %.1f s = the donor's %.3f s, %.1f s = %.3f s: file = %.6f x donor %+.3f s\n", t1s, d1, t2s, d2, *k, *desloc);
  } else snprintf(motivo, mn, "%s", autosync_motivo(r.motivo));
  fflush(stdout);
  autosync_mapa_liberar(mapa);
  legenda_documento_liberar(ref); legenda_documento_liberar(doc);
  return achou;
}

static void *fio(void *u) {
  Pedido *p = u;
  int i, achou = 0;
  double k = 1, d = 0;
  char motivo[160] = "no pair of subtitle tracks in the same language";
  for (i = 0; i < p->n && !achou && !cancelado((void *)(uintptr_t)p->ger); i++) {
    const DoaLegInfo *a = &p->a[p->ia[i]], *b = &p->b[p->ib[i]];
    char m[120] = "";
    printf("[doadorleg] pair %d: file %s %s \"%.30s\" / donor %s %s \"%.30s\"\n", i + 1, a->lingua[0] ? a->lingua : "und", a->codec,
           a->nome, b->lingua[0] ? b->lingua : "und", b->codec, b->nome);
    achou = doadorleg_alinhar(p->ta[i], p->na[i], ehAss(a), a->lingua, p->tb[i], p->nb[i], ehAss(b), b->lingua, p->pos, &k, &d,
                              m, sizeof m, cancelado, (void *)(uintptr_t)p->ger);
    if (!achou) snprintf(motivo, sizeof motivo, "pair %d (%s %s / %s %s): %s", i + 1, a->lingua, a->codec, b->lingua, b->codec, m);
  }
  terminar(p->ger, achou, k, d, achou ? "" : motivo);
  for (i = 0; i < p->n; i++) { free(p->ta[i]); free(p->tb[i]); }
  free(p);
  return NULL;
}

void doadorleg_iniciar(double pos) {
  Pedido *p = calloc(1, sizeof *p);
  int na, nb, i;
  pthread_t t;
  unsigned ger = atomic_fetch_add(&geracao, 1) + 1;
  pthread_mutex_lock(&trava); estado = 1; rMotivo[0] = 0; pthread_mutex_unlock(&trava);
  if (!p) { terminar(ger, 0, 1, 0, "out of memory"); return; }
  p->ger = ger; p->pos = pos;
  na = doa_legendas(0, p->a, DOA_LEG_MAX);
  nb = doa_legendas(1, p->b, DOA_LEG_MAX);
  p->n = doadorleg_pares(p->a, na, p->b, nb, ling_casa, p->ia, p->ib, DL_PARES);
  printf("[doadorleg] subtitle match: the file has %d indexed text track(s), the donor %d: %d pair(s) to try\n", na, nb, p->n);
  fflush(stdout);
  for (i = 0; i < p->n; i++) {
    p->ta[i] = malloc(sizeof(double) * DL_MAXPTS); p->tb[i] = malloc(sizeof(double) * DL_MAXPTS);
    p->na[i] = p->ta[i] ? doa_legenda_tempos(0, p->ia[i], p->ta[i], DL_MAXPTS) : 0;
    p->nb[i] = p->tb[i] ? doa_legenda_tempos(1, p->ib[i], p->tb[i], DL_MAXPTS) : 0;
  }
  if (!p->n) {
    terminar(ger, 0, 1, 0, na && nb ? "no subtitle language both have" : !na ? "the file's subtitles are not in its index"
                                                                         : "the donor's subtitles are not in its index");
    free(p);
    return;
  }
  if (pthread_create(&t, NULL, fio, p) != 0) {
    terminar(ger, 0, 1, 0, "no thread");
    for (i = 0; i < p->n; i++) { free(p->ta[i]); free(p->tb[i]); }
    free(p);
    return;
  }
  pthread_detach(t);
}
void doadorleg_cancelar(void) {
  atomic_fetch_add(&geracao, 1);
  pthread_mutex_lock(&trava); estado = 0; pthread_mutex_unlock(&trava);
}
int doadorleg_resultado(double *k, double *desloc, char *motivo, size_t n) {
  int v = 0;
  pthread_mutex_lock(&trava);
  if (estado == 2 || estado == 3) {
    v = estado == 2 ? 1 : -1;
    if (k) *k = rk;
    if (desloc) *desloc = rd;
    if (motivo && n) snprintf(motivo, n, "%s", rMotivo);
    estado = 0;
  }
  pthread_mutex_unlock(&trava);
  return v;
}
