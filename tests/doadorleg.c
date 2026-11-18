// The donor's subtitle match (doadorleg.c) on synthetic index timelines:
// the same dialogue in two releases, the donor shifted and/or at another
// speed, with a few lines only one release has. The engine is the subtitle
// sync's (autosync.c).
#include "../src/doadorleg.h"
#include "../src/mkvfix.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

char *rede_baixar_bin(const char *url, int segundos, long *n) { (void)url; (void)segundos; if (n) *n = 0; return NULL; }
const char *i18n(const char *s) { return s; }
int ling_casa(const char *a, const char *b) { return !strcasecmp(a, b) || (!strcasecmp(a, "eng") && !strcasecmp(b, "en")); }
static int nunca(void *u) { (void)u; return 0; }

static unsigned semente = 12345;
static double aleat(void) { semente = semente * 1103515245u + 12345u; return (semente >> 8 & 0xffff) / 65536.0; }

// Dialogue: lines 1-6 s apart, pauses of 10-40 s now and then.
static int dialogo(double *t, int max, double fim) {
  int n = 0;
  double x = 30.0;
  while (x < fim && n < max) {
    t[n++] = x;
    x += aleat() < 0.06 ? 10.0 + aleat() * 30.0 : 1.0 + aleat() * 5.0;
  }
  return n;
}

int main(void) {
  static double f[4000], d[4000];
  int nf = dialogo(f, 4000, 6000.0), nd, i;
  double k, des;
  char m[160];
  // 1. The same edit, the donor 2.480 s later (its own lines at t + 2.48), a
  //    few lines missing in each.
  for (i = 0, nd = 0; i < nf; i++) if (i % 37 != 5) d[nd++] = f[i] + 2.48;
  assert(doadorleg_alinhar(f, nf, 0, "en", d, nd, 0, "eng", 1200.0, &k, &des, m, sizeof m, nunca, NULL));
  printf("shift: k %.6f desloc %.3f\n", k, des);
  assert(k == 1.0 && fabs(des - (-2.48)) < 0.06);
  // 2. The donor at 23.976 against a 24.000 picture (Fight Club, 17/11):
  //    donor time = (file time) * 1.001, plus 1.2 s.
  for (i = 0, nd = 0; i < nf; i++) d[nd++] = f[i] * (24000.0 / 23976.0) + 1.2;
  assert(doadorleg_alinhar(f, nf, 0, "en", d, nd, 0, "en", 2500.0, &k, &des, m, sizeof m, nunca, NULL));
  printf("speed: k %.6f desloc %.3f\n", k, des);
  assert(fabs(k - 23976.0 / 24000.0) < 1e-6);
  // At 2543 s the donor's time maps back onto the file's.
  { double df = 2543.0 * (24000.0 / 23976.0) + 1.2; assert(fabs(k * df + des - 2543.0) < 0.1); }
  // 3. Unrelated dialogue: refused.
  semente = 999;
  nd = dialogo(d, 4000, 6000.0);
  assert(!doadorleg_alinhar(f, nf, 0, "en", d, nd, 0, "en", 1200.0, &k, &des, m, sizeof m, nunca, NULL));
  printf("unrelated: %s\n", m);
  // 4. Pairs: same language only, forced never, SRT first, SDH with SDH.
  { DoaLegInfo a[3], b[3];
    int ia[3], ib[3], n;
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    strcpy(a[0].lingua, "en"); strcpy(a[0].codec, "subrip"); a[0].n = 1000;
    strcpy(a[1].lingua, "en"); strcpy(a[1].codec, "subrip"); strcpy(a[1].nome, "SDH"); a[1].n = 1300;
    strcpy(a[2].lingua, "fr"); strcpy(a[2].codec, "subrip"); a[2].n = 900;
    strcpy(b[0].lingua, "eng"); strcpy(b[0].codec, "subrip"); strcpy(b[0].nome, "SDH"); b[0].n = 1310;
    strcpy(b[1].lingua, "eng"); strcpy(b[1].codec, "ass"); b[1].forcada = 1; b[1].n = 40;
    strcpy(b[2].lingua, "de"); strcpy(b[2].codec, "subrip"); b[2].n = 950;
    n = doadorleg_pares(a, 3, b, 3, ling_casa, ia, ib, 3);
    assert(n == 2 && ia[0] == 1 && ib[0] == 0 && ia[1] == 0 && ib[1] == 0); }
  // 4b. No shared language: pairs in two languages, forced never, closest counts first.
  { DoaLegInfo a[2], b[2];
    int ia[3], ib[3], n;
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    strcpy(a[0].lingua, "fr"); strcpy(a[0].codec, "subrip"); a[0].n = 1000;
    strcpy(a[1].lingua, "fr"); strcpy(a[1].codec, "subrip"); a[1].forcada = 1; a[1].n = 30;
    strcpy(b[0].lingua, "it"); strcpy(b[0].codec, "ass"); b[0].n = 1900;
    strcpy(b[1].lingua, "en"); strcpy(b[1].codec, "subrip"); b[1].n = 1100;
    assert(doadorleg_pares(a, 2, b, 2, ling_casa, ia, ib, 3) == 0);
    n = doadorleg_pares_cruzados(a, 2, b, 2, ia, ib, 3);
    assert(n == 2 && ia[0] == 0 && ib[0] == 1 && ib[1] == 0); }
  // 4c. What a file is from its first bytes (the donor server opens MKV/MP4).
  { unsigned char b[400];
    memset(b, 0, sizeof b);
    b[0] = 0x1A; b[1] = 0x45; b[2] = 0xDF; b[3] = 0xA3;
    assert(mkvfix_conteiner_de(b, sizeof b) == MKVFIX_CONT_MKV);
    memset(b, 0, sizeof b); memcpy(b + 4, "ftyp", 4);
    assert(mkvfix_conteiner_de(b, sizeof b) == MKVFIX_CONT_MP4);
    memset(b, 0, sizeof b); b[0] = 0x47; b[188] = 0x47;
    assert(mkvfix_conteiner_de(b, sizeof b) == MKVFIX_CONT_TS && !mkvfix_conteiner_doador_ok(MKVFIX_CONT_TS));
    memset(b, 0, sizeof b); b[4] = 0x47; b[196] = 0x47;
    assert(mkvfix_conteiner_de(b, sizeof b) == MKVFIX_CONT_M2TS);
    assert(mkvfix_conteiner_doador_ok(MKVFIX_CONT_DESCONHECIDO) && mkvfix_conteiner_doador_ok(MKVFIX_CONT_MP4)); }
  // 5. The line: two points, snapped to the known ratio.
  assert(doadorleg_reta(100.0, 100.1, 700.0, 700.7, &k, &des) && fabs(k - 23976.0 / 24000.0) < 1e-9);
  assert(!doadorleg_reta(100.0, 100.0, 110.0, 110.0, &k, &des));
  // 5b. Owner's log 9 (Fight Club French, 18/11): a +354 ppm slope from two
  //     subtitle tracks by different people is no real speed: 1, and the
  //     offset at the playhead (t1), not the average of the two points.
  assert(doadorleg_reta(1827.6, 1825.438, 2427.6, 2425.225, &k, &des) && k == 1.0 && fabs(des - 2.162) < 1e-6);
  //     The same frame rate in both containers: only 1, even near 24/23.976.
  assert(doadorleg_reta_v(100.0, 100.1, 700.0, 700.7, 1, &k, &des) && k == 1.0);
  //     Neither 1 nor a film/TV ratio (2000 ppm): refused.
  assert(!doadorleg_reta(100.0, 100.0, 700.0, 701.2, &k, &des));
  puts("doadorleg: shift, 24/23.976 speed, unrelated refused, pairs and the line passed");
  return 0;
}
