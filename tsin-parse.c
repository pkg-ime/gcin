#include <string.h>
#include "gcin.h"
#include "pho.h"
#include "tsin.h"
#include "gcin-conf.h"
#include <math.h>
#include "tsin-parse.h"

#define DBG (0)

int tsin_parse_recur(int start, TSIN_PARSE *out,
                     short *r_match_phr_N, short *r_no_match_ch_N)
{
  int plen;
  double bestscore = -1;
  int bestusecount = 0;
  *r_match_phr_N = 0;
  *r_no_match_ch_N = c_len - start;


  for(plen=1; start + plen <= c_len && plen <= MAX_PHRASE_LEN; plen++) {
#if DBG
    dbg("---- aa st:%d hh plen:%d ", start, plen);utf8_putchar(chpho[start].ch); dbg("\n");
#endif
    if (plen > 1 && (chpho[start+plen-1].flag & FLAG_CHPHO_PHRASE_USER_HEAD))
      break;

    phokey_t pp[MAX_PHRASE_LEN + 1];
    int sti, edi;
    TSIN_PARSE pbest[MAX_PH_BF_EXT+1];
#define MAXV 1000
    int maxusecount = 5-MAXV;
    int remlen;
    short match_phr_N=0, no_match_ch_N = plen;

    bzero(pbest, sizeof(TSIN_PARSE) * c_len);

    pbest[0].len = plen;
    pbest[0].start = start;
    int i, ofs;
    for(ofs=i=0; i < plen; i++)
      ofs += utf8cpy(pbest[0].str + ofs, chpho[start + i].ch);

#if DBG
    dbg("st:%d hh plen:%d ", start, plen);utf8_putchar(chpho[start].ch); dbg("\n");
#endif

    extract_pho(start, plen, pp);

    if (!tsin_seek(pp, plen, &sti, &edi)) {
#if 1
      if (plen > 1)
        break;
#endif
      goto next;
    }

    for (;sti < edi; sti++) {
      phokey_t mtk[MAX_PHRASE_LEN];
      char mtch[MAX_PHRASE_LEN*CH_SZ+1];
      char match_len;
      usecount_t usecount;

      load_tsin_entry(sti, &match_len, &usecount, mtk, mtch);

      if (match_len < plen)
        continue;

      if (check_fixed_mismatch(start, mtch, plen))
        continue;

      if (usecount < 0)
        usecount = 0;

      int i;
      for(i=0;i < plen;i++)
        if (mtk[i]!=pp[i])
          break;

      if (i < plen)
        continue;

      if (match_len > plen) {
        continue;
      }

      if (usecount <= maxusecount)
        continue;

      pbest[0].len = plen;
      maxusecount = usecount;
      utf8cpyN(pbest[0].str, mtch, plen);
      pbest[0].flag |= FLAG_TSIN_PARSE_PHRASE;

      match_phr_N = 1;
      no_match_ch_N = 0;
#if DBG
      utf8_putcharn(mtch, plen);
      dbg("   plen %d usecount:%d  ", plen, usecount);
        utf8_putcharn(mtch, plen);
      dbg("\n");
#endif
    }


next:
    if (!(chpho[start].ch[0] & 0x80) && !match_phr_N)
      no_match_ch_N = 0;

    remlen = c_len - (start + plen);


    if (remlen) {
      int next = start + plen;
      CACHE *pca;

      short smatch_phr_N, sno_match_ch_N;
      int uc;

      if (pca = cache_lookup(next)) {
        uc = pca->usecount;
        smatch_phr_N = pca->match_phr_N;
        sno_match_ch_N = pca->no_match_ch_N;
        memcpy(&pbest[1], pca->best, (c_len - next) * sizeof(TSIN_PARSE));
      } else {
        uc = tsin_parse_recur(next, &pbest[1], &smatch_phr_N, &sno_match_ch_N);
//        dbg("   gg %d\n", smatch_phr_N);
        add_cache(next, uc, &pbest[1], smatch_phr_N, sno_match_ch_N, c_len);
      }

      match_phr_N += smatch_phr_N;
      no_match_ch_N += sno_match_ch_N;
      maxusecount += uc;
    }


    double score = log(maxusecount + MAXV) /
      (pow(match_phr_N, 10)+ 1.0E-6) / (pow(no_match_ch_N, 20) + 1.0E-6);

#if DBG
    dbg("st:%d plen:%d zz muse:%d ma:%d noma:%d  score:%.4e %.4e\n", start, plen,
        maxusecount, match_phr_N, no_match_ch_N, score, bestscore);
#endif
    if (score > bestscore) {
#if DBG
      dbg("is best org %.4e\n", bestscore);
#endif
      bestscore = score;
      memcpy(out, pbest, sizeof(TSIN_PARSE) * (c_len - start));

#if DBG
      dbg("    str:%d  ", start);
      int i;
      for(i=0;  i < c_len - start; i++) {
        utf8_putcharn(out[i].str, out[i].len);
      }
      dbg("\n");
#endif

      bestusecount = maxusecount;
      *r_match_phr_N = match_phr_N;
      *r_no_match_ch_N = no_match_ch_N;
    }
  }

  if (bestusecount < 0)
    bestusecount = 0;

  return bestusecount;
}

void disp_ph_sta_idx(int idx);


void tsin_parse()
{
  TSIN_PARSE out[MAX_PH_BF_EXT+1];
  bzero(out, sizeof(out));

  int i, ofsi;

  if (c_len <= 1)
    return;

  init_cache(c_len);

  short smatch_phr_N, sno_match_ch_N;
  tsin_parse_recur(0, out, &smatch_phr_N, &sno_match_ch_N);
#if 0
  puts("vvvvvvvvvvvvvvvv");
  for(i=0;  i < c_len; i++) {
    printf("%d:", out[i].len);
    utf8_putcharn(out[i].str, out[i].len);
  }
  dbg("\n");
#endif

  for(i=0; i < c_len; i++)
    chpho[i].flag &= ~FLAG_CHPHO_PHRASE_HEAD;

  for(ofsi=i=0; out[i].len; i++) {
    int j, ofsj;
    int psta = ofsi;

    if (out[i].flag & FLAG_TSIN_PARSE_PHRASE)
        chpho[ofsi].flag |= FLAG_CHPHO_PHRASE_HEAD;

    for(ofsj=j=0; j < out[i].len; j++) {
      ofsj += u8cpy(chpho[ofsi].ch, &out[i].str[ofsj]);

      if (out[i].flag & FLAG_TSIN_PARSE_PHRASE)
#if 1
        chpho[ofsi].psta = psta;

#else
        chpho[ofsi].psta = out[i].start;
#endif
      ofsi++;
    }
  }

  int ph_sta_idx = ph_sta;
  if (chpho[c_len-1].psta>=0 && c_len - chpho[c_len-1].psta > 1) {
    ph_sta_idx = chpho[c_len-1].psta;
  }

#if 1
  disp_ph_sta_idx(ph_sta_idx);
#endif

#if 0
  for(i=0;i<c_len;i++)
    utf8_putchar(chpho[i].ch);
  puts("");
#endif

  free_cache();
}
