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

#define DL_PARES   4        // pairs tried, best first
#define DL_MAXPTS  6000     // points per track (the engine takes up to 8000 cues)
#define DL_FIM_MAX 4.0      // an estimated end: the next start, at most this after its own
// Pairs in two different languages (owner 17/11: the match compares when lines
// start, and translations of one release mostly follow the same speech): after
// the shared-language ones, and only with this much confidence. The same bar
// for a pair whose line counts are over 2x apart (owner 18/11: no count gate).
#define DL_CONF_CRUZADO 0.90

static int casaPadrao(const char *a, const char *b) { return a[0] && b[0] && !strcasecmp(a, b); }
static int contem(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}
static int ehSdh(const DoaLegInfo *l) { return contem(l->nome, "sdh") || contem(l->nome, "hearing") || contem(l->nome, "cc"); }
static int ehAss(const DoaLegInfo *l) { return !strcmp(l->codec, "ass") || !strcmp(l->codec, "ssa"); }

// The lines a track has once its bursts are one (DoaLegInfo.falas; n when
// not counted).
static int falasDe(const DoaLegInfo *l) { return l->falas > 0 ? l->falas : l->n; }
static int ehIngles(const char *l) {
  return !strcasecmp(l, "en") || !strcasecmp(l, "eng") || !strncasecmp(l, "en-", 3) || !strncasecmp(l, "en_", 3);
}
double doadorleg_razao(const DoaLegInfo *a, const DoaLegInfo *b) {
  int x = falasDe(a), y = falasDe(b);
  if (x <= 0 || y <= 0) return 0;
  return x > y ? (double)y / x : (double)x / y;
}
// NO LINE-COUNT GATE (owner 18/11, log 4: a fansub's English ASS, 1368
// points, against a CR English ASS, 395, and a French ASS, 673, made "0
// pairs"): every pair of full tracks is tried, the closest line counts (after
// the bursts are merged) first; the match's own confidence decides, and a
// pair over 2x apart needs DL_CONF_LONGE (doadorleg_conf_min).
// ENGLISH FIRST (owner 18/11, "line up the English vs English subtitles and
// adjust from there"): an English pair before any other shared language.
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
      r = doadorleg_razao(&a[i], &b[j]);   // 1 = as many lines
      c[n].i = i; c[n].j = j;
      c[n].p = (ehIngles(a[i].lingua) || ehIngles(b[j].lingua) ? 100.0 : 0.0) + r * 10.0 +
               (ehSdh(&a[i]) == ehSdh(&b[j]) ? 5.0 : 0.0) + (!ehAss(&a[i]) && !ehAss(&b[j]) ? 2.0 : 0.0) +
               (!a[i].lingua[0] ? -3.0 : 0.0);
      n++;
    }
  for (i = 1; i < n; i++) for (k = i; k > 0 && c[k].p > c[k - 1].p; k--) { t = c[k]; c[k] = c[k - 1]; c[k - 1] = t; }
  for (i = 0; i < n && i < max; i++) { ia[i] = c[i].i; ib[i] = c[i].j; }
  return n < max ? n : max;
}

int doadorleg_pares_cruzados(const DoaLegInfo *a, int na, const DoaLegInfo *b, int nb, int *ia, int *ib, int max) {
  struct { int i, j; double p; } c[64], t;
  int n = 0, i, j, k;
  for (i = 0; i < na; i++)
    for (j = 0; j < nb && n < 64; j++) {
      double r;
      if (a[i].forcada || b[j].forcada) continue;
      r = doadorleg_razao(&a[i], &b[j]);
      c[n].i = i; c[n].j = j;
      // Closest line counts first; SDH with SDH; SRT before ASS (typeset ASS
      // splits a line into many starts); English on one side next.
      c[n].p = r * 10.0 + (ehSdh(&a[i]) == ehSdh(&b[j]) ? 3.0 : 0.0) + (!ehAss(&a[i]) && !ehAss(&b[j]) ? 2.0 : 0.0) +
               (ehIngles(a[i].lingua) || ehIngles(b[j].lingua) ? 1.0 : 0.0);
      n++;
    }
  for (i = 1; i < n; i++) for (k = i; k > 0 && c[k].p > c[k - 1].p; k--) { t = c[k]; c[k] = c[k - 1]; c[k - 1] = t; }
  for (i = 0; i < n && i < max; i++) { ia[i] = c[i].i; ib[i] = c[i].j; }
  return n < max ? n : max;
}
double doadorleg_conf_min(const DoaLegInfo *a, const DoaLegInfo *b, int cruzado) {
  return cruzado || doadorleg_razao(a, b) < 0.5 ? DL_CONF_CRUZADO : 0.0;
}

int doadorleg_reta(double t1, double d1, double t2, double d2, double *k, double *desloc) {
  return doadorleg_reta_v(t1, d1, t2, d2, 0, k, desloc);
}
// ONLY REAL SPEEDS (owner's log 9, Fight Club French, 18/11): two releases of
// one film run at the same speed or at a film/TV ratio, nothing in between.
// Two subtitle tracks written by different people drift a few hundred ms
// against each other, and the two-point line read that as +354 ppm: a 2 s
// drift by the end of the film, and a needless reopen at the "new speed".
// The measured slope snaps to the nearest real ratio (within 800 ppm) or the
// line is refused; with the same frame rate in both containers it is 1. The
// offset is taken at t1 (the playhead), where the viewer is.
int doadorleg_reta_v(double t1, double d1, double t2, double d2, int mesmaVel, double *k, double *desloc) {
  static const double CONH[] = { 1.0, 24000.0 / 23976.0, 25.0 / 24.0, 25000.0 / 23976.0,
                                 23976.0 / 24000.0, 24.0 / 25.0, 23976.0 / 25000.0 };
  double kk, melhor = 0, dist = 1e9;
  unsigned i;
  if (!isfinite(d1) || !isfinite(d2) || fabs(d2 - d1) < 30.0) return 0;
  kk = (t2 - t1) / (d2 - d1);
  if (kk < 0.95 || kk > 1.05) return 0;
  for (i = 0; i < (mesmaVel ? 1u : sizeof CONH / sizeof *CONH); i++)
    if (fabs(kk - CONH[i]) < dist) { dist = fabs(kk - CONH[i]); melhor = CONH[i]; }
  if (dist > (mesmaVel ? 0.003 : 0.0008)) return 0;
  if (dist > 0.00005)
    printf("[doadorleg] measured speed %.6f is no real ratio: %.6f used%s\n", kk, melhor, mesmaVel ? " (same frame rate in both files)" : "");
  *k = melhor;
  *desloc = t1 - melhor * d1;
  return 1;
}

// A track's start times as a timing-only document: each line runs to the next
// start (at most DL_FIM_MAX). ASS: a burst (karaoke, a typeset sign drawn in
// pieces) keeps its first point only.
static LegendaDocumento *documento(const double *t, int n, int ass, const char *lingua, const char *quem, double *span, double j0, double j1) {
  LegendaCue *v = calloc((size_t)n, sizeof *v);
  LegendaDocumentoInfo info;
  LegendaDocumento *d;
  int i, m = 0;
  if (!v) return NULL;
  for (i = 0; i < n; i++) {
    double ini = t[i], fim;
    if (ini < 0 || (m && ini - v[m - 1].inicio < (ass ? DOA_SURTO_ASS : DOA_SURTO_SRT))) continue;
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
  // A window (the file's side only): the engine proves the offset inside it
  // (a cut elsewhere does not matter), speed 1, +-10 s.
  if (j1 > j0) { info.flags |= LEGENDA_DOC_JANELA; info.janelaInicio = j0; info.janelaFim = j1; }
  if (span) { span[0] = m ? v[0].inicio : 0; span[1] = m ? v[m - 1].inicio : 0; }
  d = m >= 20 ? legenda_documento_de_cues(v, m, &info) : NULL;
  free(v);
  return d;
}

typedef struct {
  int ia[DL_PARES], ib[DL_PARES], n, cruzado[DL_PARES], nCruz;
  DoaLegInfo a[DOA_LEG_MAX], b[DOA_LEG_MAX];
  double *ta[DL_PARES], *tb[DL_PARES]; int na[DL_PARES], nb[DL_PARES];
  double pos;
  int mesmaVel;          // both containers at one frame rate (doa_diferenca): speed 1
  int semCorte;          // the two files are as long (doa_diferenca's cut within 1 s): the whole-episode phrase pass may run
  unsigned ger;
} Pedido;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint geracao;
static int estado;               // 0 nothing, 1 working, 2 found, 3 not
static double rk, rd, rConf;
static char rMotivo[160];

static int cancelado(void *u) { return (unsigned)(uintptr_t)u != atomic_load(&geracao); }

static void terminar(unsigned ger, int achou, double k, double d, double conf, const char *motivo) {
  pthread_mutex_lock(&trava);
  if (ger == atomic_load(&geracao)) {
    estado = achou ? 2 : 3; rk = k; rd = d; rConf = conf;
    snprintf(rMotivo, sizeof rMotivo, "%s", motivo ? motivo : "");
  }
  pthread_mutex_unlock(&trava);
}

static int alinharMin(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double confMin, double j0, double j1, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                      int (*cancelar)(void *), void *u);
int doadorleg_alinhar(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double *k, double *desloc, char *motivo, size_t mn, int (*cancelar)(void *), void *u) {
  return alinharMin(ta, na, assA, la, tb, nb, assB, lb, pos, 0.0, 0, 0, 0, k, desloc, NULL, motivo, mn, cancelar, u);
}
int doadorleg_alinhar_v(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                        double pos, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                        int (*cancelar)(void *), void *u) {
  return alinharMin(ta, na, assA, la, tb, nb, assB, lb, pos, 0.0, 0, 0, mesmaVel, k, desloc, conf, motivo, mn, cancelar, u);
}
static int alinharMin(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                      double pos, double confMin, double j0, double j1, int mesmaVel, double *k, double *desloc, double *conf, char *motivo, size_t mn,
                      int (*cancelar)(void *), void *u) {
  double sa[2], sb[2];
  LegendaDocumento *ref = documento(ta, na, assA, la, "file", sa, j0, j1);
  LegendaDocumento *doc = documento(tb, nb, assB, lb, "donor", sb, 0, 0);
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
  if (r.estado == AUTOSYNC_ACCEPTED && r.confianca < confMin) {
    snprintf(motivo, mn, "confidence %.2f under the %.2f this pair needs", r.confianca, confMin);
    printf("[doadorleg] confidence %.2f is under the %.2f this pair needs (two languages or line counts over 2x apart), not used\n", r.confianca, confMin);
  } else if (r.estado == AUTOSYNC_ACCEPTED) {
    // Two points of the file's time, from the playhead on (another cut: the
    // piece the viewer is in), mapped to the donor's.
    double t1s = pos > sa[0] + 60 ? pos : sa[0] + (sa[1] - sa[0]) * 0.25, t2s = t1s + 600.0, d1, d2;
    if (t2s > sa[1]) { t2s = t1s; t1s = t2s - 600.0; }
    if (t1s < sa[0]) t1s = sa[0];
    d1 = mapa ? autosync_mapa_tempo(mapa, t1s) : t1s + r.offsetMs / 1000.0;
    d2 = mapa ? autosync_mapa_tempo(mapa, t2s) : t2s + r.offsetMs / 1000.0;
    if (d1 > AUTOSYNC_SEM_LEGENDA / 2 && d2 > AUTOSYNC_SEM_LEGENDA / 2 && doadorleg_reta_v(t1s, d1, t2s, d2, mesmaVel, k, desloc)) achou = 1;
    else if (!mapa) { *k = 1.0; *desloc = -r.offsetMs / 1000.0; achou = 1; }
    else snprintf(motivo, mn, "accepted, but no real speed around %.0f s", pos);
    if (achou && conf) *conf = r.confianca;
    if (achou)
      printf("[doadorleg] the file's %.1f s = the donor's %.3f s, %.1f s = %.3f s: file = %.6f x donor %+.3f s\n", t1s, d1, t2s, d2, *k, *desloc);
  } else snprintf(motivo, mn, "%s", autosync_motivo(r.motivo));
  fflush(stdout);
  autosync_mapa_liberar(mapa);
  legenda_documento_liberar(ref); legenda_documento_liberar(doc);
  return achou;
}

// PHRASE STARTS (owner 18/11, log 4: a fansub's ASS against CR's): when the
// two tracks were written apart (one splits a line in two, carries typeset
// signs or karaoke, the other joins lines), their starts disagree too often
// for the engine. What both still share is where a phrase starts after a
// pause: both tracks keep only starts DL_FRASE_S after the one kept before,
// and 0.90 confidence is needed.
// AROUND THE PLAYHEAD FIRST (owner 18/11, log 6: a CR release against a
// fansub refused as "region_disagreement", and the whole-episode phrase pass
// gave a wrong +4774 ms in two pieces that the 0.90 bar only just stopped):
// the file's side is a window of +-DL_JANELA_S around the playhead, so a cut
// elsewhere in the episode does not matter (the offset is needed where the
// viewer is; another cut is the follow's). Synthetic fansub/CR timelines with
// a cut: found 65-90 %, never wrong; the whole-episode pass was wrong in 3 of
// ~300 runs with a cut. It runs only when the lengths agree (semCorte).
#define DL_FRASE_S  2.5
#define DL_JANELA_S 300.0
int doadorleg_frases(const double *t, int n, double gap, double *o) {
  int i, m = 0;
  double u = -1e18;
  for (i = 0; i < n; i++) {
    if (t[i] < 0 || t[i] - u < gap) continue;
    o[m++] = t[i]; u = t[i];
  }
  return m;
}
int doadorleg_alinhar_par(const double *ta, int na, int assA, const char *la, const double *tb, int nb, int assB, const char *lb,
                          double pos, double confMin, int mesmaVel, double razao, int semCorte, double *k, double *desloc,
                          double *conf, char *motivo, size_t mn, int (*cancelar)(void *), void *u) {
  double *fa, *fb, cm = confMin > DL_CONF_CRUZADO ? confMin : DL_CONF_CRUZADO;
  int achou = alinharMin(ta, na, assA, la, tb, nb, assB, lb, pos, confMin, 0, 0, mesmaVel, k, desloc, conf, motivo, mn, cancelar, u), ma, mb;
  if (achou || (cancelar && cancelar(u)) || !(assA || assB || razao < 0.8)) return achou;
  fa = malloc(sizeof *fa * (size_t)(na > 0 ? na : 1)); fb = malloc(sizeof *fb * (size_t)(nb > 0 ? nb : 1));
  if (fa && fb) {
    char m1[64], m2[64] = "", m3[64] = "";
    double j0, j1, ini = na ? ta[0] : 0, fim = na ? ta[na - 1] : 0;
    snprintf(m1, sizeof m1, "%s", motivo);
    ma = doadorleg_frases(ta, na, DL_FRASE_S, fa); mb = doadorleg_frases(tb, nb, DL_FRASE_S, fb);
    // The window: +-DL_JANELA_S around the playhead, inside the track, 2*DL_JANELA_S wide when it fits.
    j0 = pos - DL_JANELA_S; j1 = pos + DL_JANELA_S;
    if (j0 < ini) { j1 += ini - j0; j0 = ini; }
    if (j1 > fim) { j0 -= j1 - fim; j1 = fim; }
    if (j0 < ini) j0 = ini;
    printf("[doadorleg] phrase starts only (%.1f s apart) around the playhead (%.0f-%.0f s of the file): file %d / donor %d, confidence 0.90 needed\n",
           DL_FRASE_S, j0, j1, ma, mb);
    achou = j1 - j0 >= 120.0 &&
            alinharMin(fa, ma, 0, la, fb, mb, 0, lb, pos, cm, j0, j1, mesmaVel, k, desloc, conf, m2, sizeof m2, cancelar, u);
    if (!achou && j1 - j0 < 120.0) snprintf(m2, sizeof m2, "the track is too short for a window");
    if (!achou && !(cancelar && cancelar(u))) {
      if (semCorte) {
        printf("[doadorleg] phrase starts over the whole episode (the two files are as long): confidence 0.90 needed\n");
        achou = alinharMin(fa, ma, 0, la, fb, mb, 0, lb, pos, cm, 0, 0, mesmaVel, k, desloc, conf, m3, sizeof m3, cancelar, u);
      } else snprintf(m3, sizeof m3, "not tried (the lengths differ: another cut)");
    }
    if (!achou) snprintf(motivo, mn, "%.40s; around the playhead: %.40s; whole: %.40s", m1, m2, m3);
  }
  free(fa); free(fb);
  return achou;
}

// THE POINTS OF A FAILED PAIR (owner 18/11): both tracks' index starts in the
// log, so a real pair can be replayed on the host (tests/doadorleg.c reads
// them back). Centiseconds, the first absolute, then deltas, base 36, "."
// between; DL_PONTOS_LINHA values a line.
#define DL_PONTOS_LINHA 240
void doadorleg_pontos_log(const char *quem, int k, const DoaLegInfo *l, const double *t, int n) {
  static const char dig[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  int partes = (n + DL_PONTOS_LINHA - 1) / DL_PONTOS_LINHA, pt, i;
  long ant = 0;
  printf("[doadorleg] points %s %d: %s %s \"%.30s\" n=%d\n", quem, k, l && l->lingua[0] ? l->lingua : "und", l ? l->codec : "?",
         l ? l->nome : "", n);
  for (pt = 0; pt < partes; pt++) {
    char b[DL_PONTOS_LINHA * 8 + 1];
    size_t o = 0;
    for (i = pt * DL_PONTOS_LINHA; i < n && i < (pt + 1) * DL_PONTOS_LINHA; i++) {
      long c = lround(t[i] * 100.0), d = c - ant;
      char tmp[16];
      int m = 0;
      ant = c;
      if (d < 0) { b[o++] = '-'; d = -d; }
      do { tmp[m++] = dig[d % 36]; d /= 36; } while (d && m < 15);
      while (m) b[o++] = tmp[--m];
      b[o++] = '.';
    }
    if (o) o--;
    b[o] = 0;
    printf("[doadorleg] points %s %d part %d/%d: %s\n", quem, k, pt + 1, partes, b);
  }
  fflush(stdout);
}
// One "points ... part" line back: 1 read (*n points added after the *n already in t).
int doadorleg_pontos_ler(const char *linha, char *quem, size_t nq, int *k, int *parte, double *t, int max, int *n) {
  const char *p = strstr(linha, "[doadorleg] points ");
  char q[16];
  int kk, pp, tot, usado = 0;
  long c = 0;
  if (!p || sscanf(p, "[doadorleg] points %15s %d part %d/%d: %n", q, &kk, &pp, &tot, &usado) < 4 || !usado) return 0;
  p += usado;
  if (pp == 1) *n = 0;
  else if (*n > 0) c = lround(t[*n - 1] * 100.0);
  while (*p && *p != '\n' && *n < max) {
    long d = 0;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p && *p != '.' && *p != '\n') { d = d * 36 + (*p <= '9' ? *p - '0' : *p - 'a' + 10); p++; }
    c += neg ? -d : d;
    t[(*n)++] = c / 100.0;
    if (*p == '.') p++;
  }
  if (quem && nq) snprintf(quem, nq, "%s", q);
  if (k) *k = kk;
  if (parte) *parte = pp;
  return 1;
}

static void *fio(void *u) {
  Pedido *p = u;
  int i, achou = 0;
  double k = 1, d = 0, conf = 0;
  char motivo[160] = "no pair of subtitle tracks in the same language";
  for (i = 0; i < p->n && !achou && !cancelado((void *)(uintptr_t)p->ger); i++) {
    const DoaLegInfo *a = &p->a[p->ia[i]], *b = &p->b[p->ib[i]];
    char m[120] = "";
    double cm = doadorleg_conf_min(a, b, p->cruzado[i]);
    printf("[doadorleg] pair %d%s: file %s %s \"%.30s\" (%d lines) / donor %s %s \"%.30s\" (%d lines)%s\n", i + 1,
           p->cruzado[i] ? " (two languages)" : "", a->lingua[0] ? a->lingua : "und", a->codec, a->nome, a->falas,
           b->lingua[0] ? b->lingua : "und", b->codec, b->nome, b->falas,
           cm > 0 && !p->cruzado[i] ? " (line counts over 2x apart: confidence 0.90 needed)" : "");
    achou = doadorleg_alinhar_par(p->ta[i], p->na[i], ehAss(a), a->lingua, p->tb[i], p->nb[i], ehAss(b), b->lingua, p->pos,
                                  cm, p->mesmaVel, doadorleg_razao(a, b), p->semCorte, &k, &d, &conf, m, sizeof m, cancelado,
                                  (void *)(uintptr_t)p->ger);
    if (!achou) snprintf(motivo, sizeof motivo, "pair %d (%s %s / %s %s): %s", i + 1, a->lingua, a->codec, b->lingua, b->codec, m);
  }
  if (!achou && !cancelado((void *)(uintptr_t)p->ger)) {
    // For the host replay: where the viewer was, the lengths, every pair's tracks once.
    int j, q, ja;
    printf("[doadorleg] points: at %.1f s, same frame rate %d, as long %d, %d pair(s)\n", p->pos, p->mesmaVel, p->semCorte, p->n);
    for (j = 0; j < p->n; j++) {
      for (q = 0, ja = 0; q < j; q++) if (p->ia[q] == p->ia[j]) ja = 1;
      if (!ja) doadorleg_pontos_log("file", p->ia[j], &p->a[p->ia[j]], p->ta[j], p->na[j]);
      for (q = 0, ja = 0; q < j; q++) if (p->ib[q] == p->ib[j]) ja = 1;
      if (!ja) doadorleg_pontos_log("donor", p->ib[j], &p->b[p->ib[j]], p->tb[j], p->nb[j]);
    }
  }
  terminar(p->ger, achou, k, d, conf, achou ? "" : motivo);
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
  if (!p) { terminar(ger, 0, 1, 0, 0, "out of memory"); return; }
  p->ger = ger; p->pos = pos;
  { int corte = 0, vel = 1, ppm = 0;
    int ok = doa_diferenca(&corte, &vel, &ppm);
    p->mesmaVel = ok && !vel; p->semCorte = ok && abs(corte) <= 1; }
  na = doa_legendas(0, p->a, DOA_LEG_MAX);
  nb = doa_legendas(1, p->b, DOA_LEG_MAX);
  // Shared-language pairs first (English first), then pairs in two
  // languages fill what is left of DL_PARES.
  p->n = doadorleg_pares(p->a, na, p->b, nb, ling_casa, p->ia, p->ib, DL_PARES);
  for (i = 0; i < p->n; i++) p->cruzado[i] = 0;
  if (p->n < DL_PARES && na && nb) {
    int ca[DL_PARES * 4], cb[DL_PARES * 4], nc = doadorleg_pares_cruzados(p->a, na, p->b, nb, ca, cb, DL_PARES * 4), j, q;
    for (j = 0; j < nc && p->n < DL_PARES; j++) {
      int ja = 0;
      if (p->a[ca[j]].lingua[0] && p->b[cb[j]].lingua[0] &&
          (ling_casa(p->a[ca[j]].lingua, p->b[cb[j]].lingua) || ling_casa(p->b[cb[j]].lingua, p->a[ca[j]].lingua))) continue;
      for (q = 0; q < p->n; q++) if (p->ia[q] == ca[j] && p->ib[q] == cb[j]) ja = 1;
      if (ja) continue;
      p->ia[p->n] = ca[j]; p->ib[p->n] = cb[j]; p->cruzado[p->n++] = 1; p->nCruz++;
    }
  }
  printf("[doadorleg] subtitle match: the file has %d indexed text track(s), the donor %d: %d pair(s) to try%s\n", na, nb, p->n,
         p->nCruz ? " (pairs in two languages last, confidence 0.90 needed)" : "");
  fflush(stdout);
  for (i = 0; i < p->n; i++) {
    p->ta[i] = malloc(sizeof(double) * DL_MAXPTS); p->tb[i] = malloc(sizeof(double) * DL_MAXPTS);
    p->na[i] = p->ta[i] ? doa_legenda_tempos(0, p->ia[i], p->ta[i], DL_MAXPTS) : 0;
    p->nb[i] = p->tb[i] ? doa_legenda_tempos(1, p->ib[i], p->tb[i], DL_MAXPTS) : 0;
  }
  if (!p->n) {
    terminar(ger, 0, 1, 0, 0, na && nb ? "no pair of full subtitle tracks (only forced or sign tracks)" : !na ? "the file's subtitles are not in its index"
                                                                         : "the donor's subtitles are not in its index");
    free(p);
    return;
  }
  if (pthread_create(&t, NULL, fio, p) != 0) {
    terminar(ger, 0, 1, 0, 0, "no thread");
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
int doadorleg_resultado(double *k, double *desloc, double *conf, char *motivo, size_t n) {
  int v = 0;
  pthread_mutex_lock(&trava);
  if (estado == 2 || estado == 3) {
    v = estado == 2 ? 1 : -1;
    if (k) *k = rk;
    if (desloc) *desloc = rd;
    if (conf) *conf = rConf;
    if (motivo && n) snprintf(motivo, n, "%s", rMotivo);
    estado = 0;
  }
  pthread_mutex_unlock(&trava);
  return v;
}
