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
static int casaJa(const char *a, const char *b) { return !strcasecmp(a, b) || (!strcasecmp(a, "jpn") && !strcasecmp(b, "ja")); }
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

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
  // 4a. Owner's log 4 (18/11): a fansub English ASS (1368 points) against a
  //     CR English ASS (395) and a French ASS (673): no count gate any more,
  //     English first, the pair over 2x apart needs 0.90.
  { DoaLegInfo a[3], b[3];
    int ia[4], ib[4], n;
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    strcpy(a[0].lingua, "eng"); strcpy(a[0].codec, "ass"); strcpy(a[0].nome, "Signs & Songs"); a[0].forcada = 1; a[0].n = 1029;
    strcpy(a[1].lingua, "eng"); strcpy(a[1].codec, "ass"); strcpy(a[1].nome, "Dialogue"); a[1].n = 1368; a[1].falas = 610;
    strcpy(a[2].lingua, "jpn"); strcpy(a[2].codec, "ass"); a[2].n = 500;
    strcpy(b[0].lingua, "fre"); strcpy(b[0].codec, "ass"); b[0].n = 673;
    strcpy(b[1].lingua, "eng"); strcpy(b[1].codec, "ass"); b[1].n = 395;
    strcpy(b[2].lingua, "jpn"); strcpy(b[2].codec, "ass"); b[2].n = 480;
    n = doadorleg_pares(a, 3, b, 3, ling_casa, ia, ib, 4);
    assert(n == 2 && ia[0] == 1 && ib[0] == 1 && ia[1] == 2 && ib[1] == 2);
    assert(doadorleg_conf_min(&a[1], &b[1], 0) == 0.0);   // 610 / 395 lines: within 2x
    a[1].falas = 1368;
    assert(doadorleg_conf_min(&a[1], &b[1], 0) > 0.89 && doadorleg_conf_min(&a[2], &b[2], 0) == 0.0);
    assert(doadorleg_conf_min(&a[2], &b[2], 1) > 0.89);
    n = doadorleg_pares_cruzados(a, 3, b, 3, ia, ib, 4);
    assert(n == 4 && !(ia[0] == 0 || ia[1] == 0)); }
  // 4d. The bursts: an ASS line drawn in pieces counts once, SRT lines 20 ms apart too.
  { double t[] = { 1.0, 1.05, 1.10, 1.20, 2.0, 2.01, 3.0, -1.0, 4.0 };
    assert(doa_falas(t, 9, 1) == 5 && doa_falas(t, 9, 0) == 7); }
  // 4e. Fansub vs CR on the same episode (owner's log 4): speech lines, the
  //     fansub's in three layers (one start), a third of them split in two,
  //     karaoke in the OP/ED and typeset signs; CR's with some lines joined,
  //     its own timing jitter, 1.75 s later. The plain match fails; the phrase
  //     starts find it. Unrelated dialogue stays refused.
  { static double fa[8000], dn[8000], fala[2000];
    int nfa = 0, ndn = 0, nl = 0, ok;
    double x = 100.0, cf = 0;
    semente = 4242;
    while (x < 1400.0 && nl < 2000) { fala[nl++] = x; x += aleat() < 0.05 ? 8.0 + aleat() * 20.0 : 1.2 + aleat() * 3.5; }
    for (i = 0; i < nl; i++) {
      for (int l = 0; l < 3; l++) fa[nfa++] = fala[i] + l * 0.01 + aleat() * 0.04;
      if (aleat() < 0.3) fa[nfa++] = fala[i] + 1.0 + aleat();
    }
    for (x = 0.0; x < 90.0; x += 2.0) for (int q = 0; q < 6; q++) fa[nfa++] = x + q * 0.02;      // OP karaoke
    for (x = 1320.0; x < 1410.0; x += 2.0) for (int q = 0; q < 6; q++) fa[nfa++] = x + q * 0.02; // ED karaoke
    for (int q = 0; q < 120; q++) fa[nfa++] = 100.0 + aleat() * 1300.0;                          // signs
    for (i = 0; i < nl; i++) {
      if (i && aleat() < 0.15) continue;   // joined with the line before
      dn[ndn++] = fala[i] + 1.75 + (aleat() - 0.5) * 0.2;
    }
    qsort(fa, (size_t)nfa, sizeof *fa, cmpd); qsort(dn, (size_t)ndn, sizeof *dn, cmpd);
    printf("fansub vs CR: %d points (%d lines) / %d points (%d lines)\n", nfa, doa_falas(fa, nfa, 1), ndn, doa_falas(dn, ndn, 1));
    ok = doadorleg_alinhar_v(fa, nfa, 1, "eng", dn, ndn, 1, "eng", 700.0, 1, &k, &des, &cf, m, sizeof m, nunca, NULL);
    printf("fansub vs CR, plain: %s (%s)\n", ok ? "found" : "not found", m);
    ok = doadorleg_alinhar_par(fa, nfa, 1, "eng", dn, ndn, 1, "eng", 700.0, 0.0, 1, (double)doa_falas(dn, ndn, 1) / doa_falas(fa, nfa, 1),
                               &k, &des, &cf, m, sizeof m, nunca, NULL);
    printf("fansub vs CR, with phrase starts: %s k %.6f desloc %.3f confidence %.2f\n", ok ? "found" : "not found", k, des, cf);
    assert(ok && k == 1.0 && fabs(des - (-1.75)) < 0.15 && cf >= 0.90);
    semente = 777;
    ndn = dialogo(dn, 4000, 1400.0);
    ok = doadorleg_alinhar_par(fa, nfa, 1, "eng", dn, ndn, 1, "eng", 700.0, 0.0, 1, 0.5, &k, &des, &cf, m, sizeof m, nunca, NULL);
    printf("unrelated with phrase starts: %s\n", ok ? "FOUND (wrong)" : m);
    assert(!ok);
    { double o[8]; double t[] = { 1.0, 2.0, 3.6, 4.0, 7.0, -1.0, 9.4 };
      assert(doadorleg_frases(t, 7, 2.5, o) == 3 && o[0] == 1.0 && o[1] == 3.6 && o[2] == 7.0); } }
  // 4f. Which audio the match compares (owner 18/11, doa_match_escolher).
  { DoaMatchFaixa fx[6];
    DoaMatchEscolha e;
    memset(fx, 0, sizeof fx);
    // file: 0 jpn flac, 1 eng ac3; donor: 2 eng eac3 (playing), 3 jpn aac
    fx[0] = (DoaMatchFaixa){ 0, 1, 1, 2, "jpn", "flac" }; fx[1] = (DoaMatchFaixa){ 0, 2, 1, 6, "eng", "ac3" };
    fx[2] = (DoaMatchFaixa){ 1, 1, 1, 6, "eng", "eac3" }; fx[3] = (DoaMatchFaixa){ 1, 2, 1, 2, "jpn", "aac" };
    // the original on both sides: the donor's jpn (not the playing one)
    assert(doa_match_escolher(fx, 4, 2, "ja", "", "", 0, 0, casaJa, &e) && e.como == DOA_MC_ORIGINAL && e.siA == 1 && e.siD == 2 && e.siA2 == -1);
    // a donor without jpn: two file tracks
    assert(doa_match_escolher(fx, 3, 2, "ja", "", "", 0, 0, casaJa, &e) && e.como == DOA_MC_DUAS && e.siA == 2 && e.siA2 == 1 && e.siD == -1);
    printf("dual: %s\n", e.porque);
    // ...light: the original only
    assert(doa_match_escolher(fx, 3, 2, "ja", "", "", 0, 1, casaJa, &e) && e.como == DOA_MC_ORIG_LEVE && e.siA == 1 && e.siA2 == -1);
    // picked by hand: the file's eng; the donor's eng plays
    assert(doa_match_escolher(fx, 4, 2, "ja", "eng", "ac3", 6, 0, casaJa, &e) && e.como == DOA_MC_PEDIDA && e.siA == 2 && e.siD == -1);
    // picked jpn: the donor's jpn read alongside
    assert(doa_match_escolher(fx, 4, 2, "", "jpn", "flac", 2, 0, casaJa, &e) && e.como == DOA_MC_PEDIDA && e.siA == 1 && e.siD == 2);
    // no original language known: as before, the playing track's language
    assert(doa_match_escolher(fx, 4, 2, "", "", "", 0, 0, casaJa, &e) && e.como == DOA_MC_LINGUA && e.siA == 2 && e.siA2 == -1);
    // a pick the file lacks: Auto
    assert(doa_match_escolher(fx, 4, 2, "ja", "kor", "", 0, 0, casaJa, &e) && e.como == DOA_MC_ORIGINAL);
    // an Italian donor on a jpn/eng file with its own eng: the shared language
    fx[2] = (DoaMatchFaixa){ 1, 1, 1, 6, "ita", "eac3" }; fx[3] = (DoaMatchFaixa){ 1, 2, 1, 2, "eng", "aac" };
    assert(doa_match_escolher(fx, 4, 2, "", "", "", 0, 0, casaJa, &e) && e.como == DOA_MC_COMUM && e.siA == 2 && e.siD == 2);
    // ...with the original known but not in the donor: still the shared language
    assert(doa_match_escolher(fx, 4, 2, "ja", "", "", 0, 0, casaJa, &e) && e.como == DOA_MC_COMUM && e.siA == 2 && e.siD == 2);
    // the donor not playing: nothing
    assert(!doa_match_escolher(fx, 4, 0, "ja", "", "", 0, 0, casaJa, &e)); }
  // 4g. Two comparisons added: a candidate both see wins over one only one sees.
  { double oa[3] = { -2.2, -3.0, 1.0 }, sa[3] = { 0.70, 0.30, 0.10 }, ob[2] = { -3.02, 4.0 }, sb[2] = { 0.80, 0.12 };
    double o[8], md[8], a1[8], a2[8];
    int n = doa_match_somar(oa, sa, 3, ob, sb, 2, 0.10, o, md, a1, a2, 8);
    assert(n == 4 && fabs(o[0] + 3.0) < 0.05 && fabs(md[0] - 0.55) < 1e-9 && a1[0] == 0.30 && a2[0] == 0.80);
    assert(fabs(o[1] + 2.2) < 1e-9 && a2[1] == 0.0);
    assert(doa_match_somar(NULL, NULL, 0, ob, sb, 2, 0.1, o, md, a1, a2, 8) == 2 && md[0] == 0.40); }
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
  puts("doadorleg: shift, 24/23.976 speed, unrelated refused, pairs (English first, no count gate), fansub vs CR and the line passed");
  return 0;
}
