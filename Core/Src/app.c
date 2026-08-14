#include "app.h"
#include "log.h"
#include "dip.h"
#include "rs485_if.h"
#include "main.h"
#include "seg595.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* =========================
 * HW handles
 * ========================= */
extern UART_HandleTypeDef huart1;

/* =========================
 * Config
 * ========================= */
#define RUN_OUT_SEQ_MAX      8
#define RUN_L_COND_MAX       4

#define STATUS_REPLY_ONE_FRAME   1   // 1: STATUS 한 프레임으로 응답
#define STATUS_PERSIST_DONE      1   // DONE 상태 유지(다음 RUN 전까지)

#define MS_PER_SEC           1000u
#define INPUT_DEBOUNCE_MS    5u
#define APP_FRAME_BUDGET     2u

/* =========================
 * Output polarity (relay/mosfet compatibility)
 * 1 = Active High  (GPIO HIGH = ON)
 * 0 = Active Low   (GPIO LOW  = ON)  // 대부분 릴레이 모듈이 여기에 해당
 * ========================= */
#define OUT_ACTIVE_HIGH      1   // 신규 IO 보드: GPIO HIGH = 출력 ON

#if OUT_ACTIVE_HIGH
  #define OUT_ON_LEVEL   GPIO_PIN_SET
  #define OUT_OFF_LEVEL  GPIO_PIN_RESET
#else
  #define OUT_ON_LEVEL   GPIO_PIN_RESET
  #define OUT_OFF_LEVEL  GPIO_PIN_SET
#endif

/* =========================
 * Address / states
 * ========================= */
static uint8_t s_addr = 0;

typedef enum {
  SLV_IDLE = 0,
  SLV_BUSY,
  SLV_DONE
} slv_state_t;

static slv_state_t s_state = SLV_IDLE;
static uint32_t s_start_ms = 0;

static uint16_t s_last_seq = 0;   // 마지막 RUN의 seq(참고용)
static uint16_t s_next_step = 0;  // DONE 시 NEXT (0=정상 다음 step)
static uint8_t  s_last_run_valid = 0;
static uint8_t  s_last_run_cancelled = 0;
static char     s_last_run_k[64] = {0};
static char     s_last_run_v[256] = {0};

/* =========================
 * IO helpers
 * ========================= */
static void io_all_off(void)
{
  HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, OUT_OFF_LEVEL);
  HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, OUT_OFF_LEVEL);
  HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, OUT_OFF_LEVEL);
  HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, OUT_OFF_LEVEL);
  HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, OUT_OFF_LEVEL);
}

static void out_set(uint8_t ch /*1..5*/, uint8_t on)
{
  GPIO_PinState st = on ? OUT_ON_LEVEL : OUT_OFF_LEVEL;
  switch (ch) {
    case 1: HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, st); break;
    case 2: HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, st); break;
    case 3: HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, st); break;
    case 4: HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, st); break;
    case 5: HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, st); break;
    default: break;
  }
}

typedef struct {
  uint8_t last_raw;
  uint32_t raw_changed_ms;
  uint8_t stable_logical;
  uint8_t initialized;
} input_db_t;

static input_db_t s_input_db[5];

static uint8_t in_read_raw_logical(uint8_t ch /*1..5*/)
{
  GPIO_PinState st = GPIO_PIN_RESET;
  switch (ch) {
    case 1: st = HAL_GPIO_ReadPin(POTO1_GPIO_Port, POTO1_Pin); break;
    case 2: st = HAL_GPIO_ReadPin(POTO2_GPIO_Port, POTO2_Pin); break;
    case 3: st = HAL_GPIO_ReadPin(POTO3_GPIO_Port, POTO3_Pin); break;
    case 4: st = HAL_GPIO_ReadPin(POTO4_GPIO_Port, POTO4_Pin); break;
    case 5: st = HAL_GPIO_ReadPin(POTO5_GPIO_Port, POTO5_Pin); break;
    default: return 0u;
  }
  return (st == GPIO_PIN_RESET) ? 1u : 0u;
}

static void input_debounce_init(void)
{
  uint32_t now = HAL_GetTick();
  for (uint8_t i = 0; i < 5u; i++) {
    s_input_db[i].last_raw = in_read_raw_logical((uint8_t)(i + 1u));
    s_input_db[i].raw_changed_ms = now;
    s_input_db[i].stable_logical = 0u;
    s_input_db[i].initialized = 0u;
  }
}

static void input_debounce_tick(void)
{
  uint32_t now = HAL_GetTick();
  for (uint8_t i = 0; i < 5u; i++) {
    input_db_t *db = &s_input_db[i];
    uint8_t raw = in_read_raw_logical((uint8_t)(i + 1u));
    if (raw != db->last_raw) {
      db->last_raw = raw;
      db->raw_changed_ms = now;
      continue;
    }
    if ((uint32_t)(now - db->raw_changed_ms) >= INPUT_DEBOUNCE_MS) {
      if (!db->initialized) {
        db->stable_logical = raw;
        db->initialized = 1u;
      } else if (db->stable_logical != raw) {
        db->stable_logical = raw;
      }
    }
  }
}

static uint8_t in_ready(uint8_t ch)
{
  return (ch >= 1u && ch <= 5u) ? s_input_db[ch - 1u].initialized : 0u;
}

static uint8_t in_read(uint8_t ch /*1..5*/)
{
  return in_ready(ch) ? s_input_db[ch - 1u].stable_logical : 0u;
}

/* =========================
 * Parsing utils
 * ========================= */
static void trim_ws(char *s)
{
  if (!s) return;
  size_t i=0;
  while (s[i] && isspace((unsigned char)s[i])) i++;
  if (i) memmove(s, s+i, strlen(s+i)+1);
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n-1])) { s[n-1]=0; n--; }
}

static int parse_u32_dec(const char *s, uint32_t *out)
{
  if (!s || !out) return -1;
  while (*s && isspace((unsigned char)*s)) s++;
  if (!isdigit((unsigned char)*s)) return -1;
  uint32_t v = 0;
  while (isdigit((unsigned char)*s)) {
    v = v*10u + (uint32_t)(*s - '0');
    s++;
  }
  *out = v;
  return 0;
}

static uint32_t parse_time_to_ms(const char *s, int *ok)
{
  // seconds string (e.g. "3", "0.3", "1.250") -> ms
  // 소수 최대 3자리(1ms 단위). 초과는 절삭.
  if (ok) *ok = 0;
  if (!s) return 0;

  while (*s && isspace((unsigned char)*s)) s++;

  uint32_t sec_int = 0;
  uint32_t frac = 0;
  uint32_t frac_scale = 1;

  const char *p = s;
  if (!isdigit((unsigned char)*p) && *p != '.') return 0;

  // integer
  while (isdigit((unsigned char)*p)) {
    sec_int = sec_int*10u + (uint32_t)(*p - '0');
    p++;
  }

  // fraction
  if (*p == '.') {
    p++;
    uint32_t f = 0;
    uint32_t k = 0;
    while (isdigit((unsigned char)*p) && k < 3) {
      f = f*10u + (uint32_t)(*p - '0');
      p++; k++;
    }
    // 남은 소수자리는 무시(절삭)
    frac = f;
    if (k == 0) frac_scale = 1;
    else if (k == 1) frac_scale = 10;
    else if (k == 2) frac_scale = 100;
    else frac_scale = 1000;
  }

  uint32_t ms = sec_int * 1000u;
  if (frac) {
    if (frac_scale == 10) ms += frac * 100u;
    else if (frac_scale == 100) ms += frac * 10u;
    else if (frac_scale == 1000) ms += frac * 1u;
  }

  if (ok) *ok = 1;
  return ms;
}

static int args_get_kv(const char *args, const char *key, char *out, size_t out_sz)
{
  // args: "K=...;V=..." (세미콜론 구분)
  if (!args || !key || !out || out_sz==0) return -1;
  out[0]=0;

  size_t keylen = strlen(key);
  const char *p = args;
  while (*p) {
    while (*p == ';' || isspace((unsigned char)*p)) p++;
    if (!*p) break;

    const char *eq = strchr(p, '=');
    if (!eq) break;

    size_t klen = (size_t)(eq - p);
    if (klen == keylen && strncmp(p, key, keylen)==0) {
      const char *v = eq + 1;
      const char *end = strchr(v, ';');
      size_t n = end ? (size_t)(end - v) : strlen(v);
      if (n >= out_sz) n = out_sz - 1;
      memcpy(out, v, n);
      out[n]=0;
      trim_ws(out);
      return 0;
    }

    const char *next = strchr(eq+1, ';');
    if (!next) break;
    p = next + 1;
  }
  return -1;
}

typedef enum { K_KIND_NONE=0, K_KIND_R, K_KIND_L, K_KIND_P } k_kind_t;

static int parse_k(const char *k, uint8_t *out_addr, k_kind_t *out_kind, uint8_t *out_idx)
{
  // "n-R1" / "n-L3" / "n-P1"
  if (!k || !out_addr || !out_kind || !out_idx) return -1;

  char tmp[64];
  strncpy(tmp, k, sizeof(tmp)-1);
  tmp[sizeof(tmp)-1]=0;
  trim_ws(tmp);

  char *dash = strchr(tmp, '-');
  if (!dash) return -1;
  *dash = 0;
  char *left = tmp;
  char *right = dash + 1;

  uint32_t a=0;
  if (parse_u32_dec(left, &a)!=0) return -1;
  if (a > 255) return -1;

  if (!right[0]) return -1;

  char kindc = right[0];
  uint32_t idx=0;
  if (parse_u32_dec(right+1, &idx)!=0) return -1;

  k_kind_t kk = K_KIND_NONE;
  if (kindc=='R') kk = K_KIND_R;
  else if (kindc=='L') kk = K_KIND_L;
  else if (kindc=='P') kk = K_KIND_P;
  else return -1;

  if (kk==K_KIND_R && (idx < 1 || idx > 5)) return -1;
  if (kk==K_KIND_L && (idx < 1 || idx > 5)) return -1;
  if (kk==K_KIND_P && (idx < 1 || idx > 255)) return -1;

  *out_addr = (uint8_t)a;
  *out_kind = kk;
  *out_idx  = (uint8_t)idx;
  return 0;
}

/* =========================
 * RUN: Output sequence executor
 * ========================= */
typedef struct {
  uint8_t  is_on;     // 1=ON, 0=OFF
  uint32_t hold_ms;   // 0이면 즉시(시간 유지 없음)
} out_tok_t;

static struct {
  uint8_t  ch;                  // 1..5
  uint8_t  count;               // tokens
  uint8_t  cur;                 // current token index
  out_tok_t tok[RUN_OUT_SEQ_MAX];
  uint32_t phase_start_ms;
  uint8_t  running;
} s_out = {0};

static int parse_out_seq(const char *v, out_tok_t *out, uint8_t *out_cnt)
{
  // "ON3,OFF0.3" / "ON" / "OFF" / "ON3" ...
  if (!v || !out || !out_cnt) return -1;
  *out_cnt = 0;

  char tmp[256];
  strncpy(tmp, v, sizeof(tmp)-1);
  tmp[sizeof(tmp)-1]=0;
  trim_ws(tmp);
  if (!tmp[0]) return -1;

  char *p = tmp;
  while (p && *p) {
    while (*p==',' || isspace((unsigned char)*p)) p++;
    if (!*p) break;

    char *comma = strchr(p, ',');
    if (comma) { *comma = 0; }

    trim_ws(p);
    if (!p[0]) { p = comma ? (comma+1) : NULL; continue; }

    if (*out_cnt >= RUN_OUT_SEQ_MAX) return -1;

    out_tok_t t; memset(&t, 0, sizeof(t));

    if (strncmp(p, "ON", 2)==0) {
      t.is_on = 1;
      const char *ts = p + 2;
      int ok=0;
      uint32_t ms = 0;
      trim_ws((char*)ts);
      if (*ts) { ms = parse_time_to_ms(ts, &ok); if (!ok) return -1; }
      t.hold_ms = ms;
    } else if (strncmp(p, "OFF", 3)==0) {
      t.is_on = 0;
      const char *ts = p + 3;
      int ok=0;
      uint32_t ms = 0;
      trim_ws((char*)ts);
      if (*ts) { ms = parse_time_to_ms(ts, &ok); if (!ok) return -1; }
      t.hold_ms = ms;
    } else {
      return -1;
    }

    out[*out_cnt] = t;
    (*out_cnt)++;

    p = comma ? (comma + 1) : NULL;
  }

  return (*out_cnt > 0) ? 0 : -1;
}

static void out_exec_start(uint8_t ch, const char *v)
{
  memset(&s_out, 0, sizeof(s_out));
  s_out.ch = ch;

  uint8_t cnt=0;
  if (parse_out_seq(v, s_out.tok, &cnt) != 0) {
    log_printf("[RUN][OUT] parse FAIL V=%s\r\n", v ? v : "(null)");
    s_state = SLV_DONE;
    s_next_step = 0;
    return;
  }

  /* 디버그 보강: 실제 토큰 수 확인 */
  log_printf("[RUN][OUT] parsed count=%u V=%s\r\n",
             (unsigned)cnt, v ? v : "(null)");

  s_out.count = cnt;
  s_out.cur = 0;
  s_out.running = 1;
  s_out.phase_start_ms = HAL_GetTick();

  // apply first token immediately
  out_set(s_out.ch, s_out.tok[0].is_on);
  log_printf("[RUN][OUT] ch=%u start tok0=%s hold=%lums\r\n",
             (unsigned)s_out.ch,
             s_out.tok[0].is_on ? "ON":"OFF",
             (unsigned long)s_out.tok[0].hold_ms);

  // 즉시형(hold_ms=0)만 있고 토큰이 1개면 즉시 DONE
  if (s_out.count == 1 && s_out.tok[0].hold_ms == 0) {
    s_state = SLV_DONE;
    s_next_step = 0;
    s_out.running = 0;
  }
}

static void out_exec_tick(void)
{
  if (!s_out.running) return;
  uint32_t now = HAL_GetTick();
  out_tok_t *t = &s_out.tok[s_out.cur];

  if (t->hold_ms == 0) {
    // 즉시 토큰: 다음 토큰이 있으면 즉시 넘어가고, 없으면 DONE
    if (s_out.cur + 1 >= s_out.count) {
      s_out.running = 0;
      s_state = SLV_DONE;
      s_next_step = 0;
      log_printf("[RUN][OUT] DONE (immediate)\r\n");
      return;
    }
    s_out.cur++;
    t = &s_out.tok[s_out.cur];
    out_set(s_out.ch, t->is_on);
    s_out.phase_start_ms = now;
    log_printf("[RUN][OUT] next tok=%u %s hold=%lums\r\n",
               (unsigned)s_out.cur,
               t->is_on ? "ON":"OFF",
               (unsigned long)t->hold_ms);
    return;
  }

  if ((now - s_out.phase_start_ms) >= t->hold_ms) {
    // 다음 토큰으로
    if (s_out.cur + 1 >= s_out.count) {
      s_out.running = 0;
      s_state = SLV_DONE;
      s_next_step = 0;
      log_printf("[RUN][OUT] DONE\r\n");
      return;
    }
    s_out.cur++;
    t = &s_out.tok[s_out.cur];
    out_set(s_out.ch, t->is_on);
    s_out.phase_start_ms = now;
    log_printf("[RUN][OUT] next tok=%u %s hold=%lums\r\n",
               (unsigned)s_out.cur,
               t->is_on ? "ON":"OFF",
               (unsigned long)t->hold_ms);
  }
}

/* =========================
 * RUN: Input condition watcher
 * - NO : OFF 대기 -> ON 되면 이벤트 상태
 * - NC : ON 대기  -> OFF 되면 이벤트 상태
 * - STBYx : 감시 시작 전 대기시간
 * - TOx   : 감시 시작 후 x초 경과 시 DONE;NEXT=step
 * - TIx   : 이벤트 상태가 x초 연속 유지되면 DONE;NEXT=step
 * ========================= */
typedef enum { CTYPE_NONE=0, CTYPE_TO, CTYPE_TI } cond_type_t;
typedef enum { LMODE_NO=0, LMODE_NC=1 } l_mode_t;

typedef struct {
  cond_type_t type;
  uint32_t    t_ms;       // TO/TI in ms
  uint16_t    step;       // NEXT step (0 허용)
} l_cond_t;

static struct {
  uint8_t   ch;               // 1..5
  l_mode_t  mode;             // NO/NC
  uint8_t   cond_cnt;
  l_cond_t  cond[RUN_L_COND_MAX];

  uint32_t  start_ms;         // RUN 시작 시각
  uint32_t  watch_start_ms;   // STBY 종료 후 실제 감시 시작 시각
  uint32_t  standby_ms;       // 감시 시작 전 대기시간
  uint8_t   standby_done;     // 0=STBY 중, 1=감시 중

  uint8_t   prev_in;          // debounced logical input
  uint8_t   ev_prev;          // 이전 이벤트 상태
  uint8_t   ev_now;           // 현재 이벤트 상태

  uint8_t   stable_active;    // 이벤트 연속 유지 계측 중
  uint32_t  stable_start_ms;  // 이벤트 상태 진입 시각
  uint8_t   input_ready_seen; // 최초 debounce 확정은 이벤트가 아닌 기준 상태
  uint8_t   initial_active_suppressed;

  uint8_t   running;
} s_in = {0};

static int parse_step_num(const char *s, uint16_t *out_step)
{
  // "STEP2" -> 2
  if (!s || !out_step) return -1;
  while (*s && isspace((unsigned char)*s)) s++;
  if (strncmp(s, "STEP", 4)!=0) return -1;
  s += 4;

  uint32_t n=0;
  if (parse_u32_dec(s, &n)!=0) return -1;
  if (n > 255) return -1;

  *out_step = (uint16_t)n;
  return 0;
}

static uint8_t is_event_state(l_mode_t mode, uint8_t in_level)
{
  // NO: 이벤트는 ON(1)
  // NC: 이벤트는 OFF(0)
  if (mode == LMODE_NO) return in_level ? 1u : 0u;
  return in_level ? 0u : 1u;
}

static int parse_in_cond_list(const char *v,
                              l_mode_t *out_mode,
                              l_cond_t *out,
                              uint8_t *out_cnt,
                              uint32_t *out_stby_ms)
{
  // 지원:
  // "(NO,TO5,STEP2)"
  // "(NC,TI2,STEP3)"
  // "(NO,STBY1,TO5,STEP2)"
  // "(NC,STBY0.5,TI1.2,STEP7)"
  if (!v || !out_mode || !out || !out_cnt || !out_stby_ms) return -1;

  *out_cnt = 0;
  *out_stby_ms = 0;

  char tmp[256];
  strncpy(tmp, v, sizeof(tmp)-1);
  tmp[sizeof(tmp)-1] = 0;
  trim_ws(tmp);
  if (!tmp[0]) return -1;

  char *p = tmp;
  l_mode_t mode = LMODE_NO;
  uint8_t first_mode_set = 0;

  while (p && *p) {
    while (*p==',' || isspace((unsigned char)*p)) p++;
    if (!*p) break;

    if (*p != '(') return -1;
    char *rpar = strchr(p, ')');
    if (!rpar) return -1;
    *rpar = 0;
    char *inside = p + 1;

    if (*out_cnt >= RUN_L_COND_MAX) return -1;

    char buf[128];
    strncpy(buf, inside, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;

    char *save = NULL;
    char *tok = strtok_r(buf, ",", &save);
    if (!tok) return -1;
    trim_ws(tok);

    l_mode_t mtmp = LMODE_NO;
    if (strcmp(tok, "NO") == 0) mtmp = LMODE_NO;
    else if (strcmp(tok, "NC") == 0) mtmp = LMODE_NC;
    else return -1;

    if (!first_mode_set) {
      mode = mtmp;
      first_mode_set = 1;
    } else {
      // 같은 L에서 NO/NC 혼합 금지
      if (mtmp != mode) return -1;
    }

    l_cond_t c;
    memset(&c, 0, sizeof(c));

    uint16_t step = 0;
    uint8_t got_rule = 0;

    for (;;) {
      tok = strtok_r(NULL, ",", &save);
      if (!tok) break;
      trim_ws(tok);
      if (!tok[0]) continue;

      if (strncmp(tok, "STEP", 4) == 0) {
        if (parse_step_num(tok, &step) != 0) return -1;
        continue;
      }

      if (strncmp(tok, "STBY", 4) == 0) {
        int ok = 0;
        uint32_t ms = parse_time_to_ms(tok + 4, &ok);
        if (!ok) return -1;
        // 여러 cond에 STBY가 섞이면 가장 큰 값 사용
        if (ms > *out_stby_ms) *out_stby_ms = ms;
        continue;
      }

      if (strncmp(tok, "TO", 2) == 0) {
        int ok = 0;
        uint32_t ms = parse_time_to_ms(tok + 2, &ok);
        if (!ok) return -1;
        c.type = CTYPE_TO;
        c.t_ms = ms;
        got_rule = 1;
        continue;
      }

      if (strncmp(tok, "TI", 2) == 0) {
        int ok = 0;
        uint32_t ms = parse_time_to_ms(tok + 2, &ok);
        if (!ok) return -1;
        c.type = CTYPE_TI;
        c.t_ms = ms;
        got_rule = 1;
        continue;
      }

      return -1;
    }

    if (!got_rule) return -1;
    c.step = step; // STEP 없으면 0
    out[*out_cnt] = c;
    (*out_cnt)++;

    p = rpar + 1;
    if (*p == ',') p++;
  }

  *out_mode = mode;
  return (*out_cnt > 0) ? 0 : -1;
}

/* RUN을 ACK하기 전에 K/V 전체를 부작용 없이 검증한다. */
static const char *run_validate(const char *k, const char *v)
{
  uint8_t kaddr = 0;
  uint8_t idx = 0;
  k_kind_t kind = K_KIND_NONE;

  if (parse_k(k, &kaddr, &kind, &idx) != 0) {
    return "BAD_K";
  }
  if (s_addr != 0 && kaddr != s_addr) {
    return "ADDR_MISMATCH";
  }

  if (kind == K_KIND_R) {
    out_tok_t tok[RUN_OUT_SEQ_MAX];
    uint8_t cnt = 0;
    if (parse_out_seq(v, tok, &cnt) != 0) {
      return "BAD_V";
    }
    return NULL;
  }

  if (kind == K_KIND_L) {
    l_mode_t mode = LMODE_NO;
    l_cond_t cond[RUN_L_COND_MAX];
    uint8_t cnt = 0;
    uint32_t stby_ms = 0;
    if (parse_in_cond_list(v, &mode, cond, &cnt, &stby_ms) != 0) {
      return "BAD_V";
    }
    return NULL;
  }

  return "UNSUPPORTED_KIND";
}

static void in_watch_start(uint8_t ch, const char *v)
{
  memset(&s_in, 0, sizeof(s_in));
  s_in.ch = ch;

  uint8_t cnt = 0;
  l_mode_t mode = LMODE_NO;
  uint32_t stby_ms = 0;

  if (parse_in_cond_list(v, &mode, s_in.cond, &cnt, &stby_ms) != 0) {
    log_printf("[RUN][IN] parse FAIL V=%s\r\n", v ? v : "(null)");
    s_state = SLV_DONE;
    s_next_step = 0;
    return;
  }

  s_in.mode = mode;
  s_in.cond_cnt = cnt;
  s_in.standby_ms = stby_ms;

  s_in.start_ms = HAL_GetTick();
  s_in.watch_start_ms = 0;
  s_in.prev_in = in_read(ch);
  s_in.ev_prev = is_event_state(mode, s_in.prev_in);
  s_in.ev_now = s_in.ev_prev;
  s_in.stable_active = 0;
  s_in.stable_start_ms = 0;
  s_in.input_ready_seen = in_ready(ch);

  s_in.standby_done = (stby_ms == 0u) ? 1u : 0u;
  if (s_in.standby_done) {
    s_in.watch_start_ms = s_in.start_ms;
  }

  s_in.running = 1;

  log_printf("[RUN][IN] ch=%u mode=%s cond=%u stby=%lums\r\n",
             (unsigned)ch,
             (mode==LMODE_NO) ? "NO" : "NC",
             (unsigned)cnt,
             (unsigned long)stby_ms);
}

static void in_watch_tick(void)
{
  if (!s_in.running) return;

  uint32_t now = HAL_GetTick();

  if (!in_ready(s_in.ch)) return;
  if (!s_in.input_ready_seen) {
    s_in.prev_in = in_read(s_in.ch);
    s_in.ev_prev = is_event_state(s_in.mode, s_in.prev_in);
    s_in.ev_now = s_in.ev_prev;
    s_in.stable_active = 0;
    s_in.stable_start_ms = 0;
    s_in.input_ready_seen = 1u;
    s_in.initial_active_suppressed = s_in.ev_prev;
    return;
  }

  // 1) STBY 처리
  if (!s_in.standby_done) {
    if ((now - s_in.start_ms) < s_in.standby_ms) {
      return;
    }

    s_in.standby_done = 1;
    s_in.watch_start_ms = now;

    // 감시 시작 시점에서 상태 기준 다시 잡음
    s_in.prev_in = in_read(s_in.ch);
    s_in.ev_prev = is_event_state(s_in.mode, s_in.prev_in);
    s_in.ev_now = s_in.ev_prev;
    s_in.stable_active = 0;
    s_in.stable_start_ms = 0;

    log_printf("[RUN][IN] STBY done -> watch start\r\n");
  }

  // 2) 현재 이벤트 상태 계산
  uint8_t cur = in_read(s_in.ch);
  s_in.ev_now = is_event_state(s_in.mode, cur);

  if (s_in.initial_active_suppressed) {
    s_in.ev_prev = s_in.ev_now;
    s_in.prev_in = cur;
    if (s_in.ev_now) return;
    s_in.initial_active_suppressed = 0u;
  }

  // 이벤트 상태 진입: NO면 OFF->ON, NC면 ON->OFF
  if (!s_in.ev_prev && s_in.ev_now) {
    s_in.stable_active = 1;
    s_in.stable_start_ms = now;
    log_printf("[RUN][IN] EVENT edge\r\n");
  }

  // 이벤트 상태 이탈
  if (s_in.ev_prev && !s_in.ev_now) {
    s_in.stable_active = 0;
    s_in.stable_start_ms = 0;
    log_printf("[RUN][IN] EVENT clear\r\n");
  }

  s_in.ev_prev = s_in.ev_now;
  s_in.prev_in = cur;

  // 3) TI: 이벤트 상태가 연속 유지되면 성공
  for (uint8_t i=0; i<s_in.cond_cnt; i++) {
    if (s_in.cond[i].type != CTYPE_TI) continue;
    if (!s_in.stable_active) continue;

    uint32_t held = now - s_in.stable_start_ms;
    if (held >= s_in.cond[i].t_ms) {
      s_in.running = 0;
      s_state = SLV_DONE;
      s_next_step = s_in.cond[i].step;
      log_printf("[RUN][IN] DONE via TI hold=%lums -> NEXT=%u\r\n",
                 (unsigned long)held,
                 (unsigned)s_next_step);
      return;
    }
  }

  // 4) TI 규칙이 하나도 없고 이벤트 상태 진입 시 즉시 성공
  {
    uint8_t any_ti = 0;
    for (uint8_t i=0; i<s_in.cond_cnt; i++) {
      if (s_in.cond[i].type == CTYPE_TI) {
        any_ti = 1;
        break;
      }
    }

    if (!any_ti && s_in.ev_now) {
      s_in.running = 0;
      s_state = SLV_DONE;
      s_next_step = 0;
      log_printf("[RUN][IN] DONE immediate event NEXT=0\r\n");
      return;
    }
  }

  // 5) TO: 감시 시작 후 지정 시간 경과 시 성공 분기
  {
    uint32_t elapsed = now - s_in.watch_start_ms;
    for (uint8_t i=0; i<s_in.cond_cnt; i++) {
      if (s_in.cond[i].type != CTYPE_TO) continue;
      if (elapsed >= s_in.cond[i].t_ms) {
        s_in.running = 0;
        s_state = SLV_DONE;
        s_next_step = s_in.cond[i].step;
        log_printf("[RUN][IN] DONE via TO -> NEXT=%u\r\n",
                   (unsigned)s_next_step);
        return;
      }
    }
  }
}

/* =========================
 * RUN dispatch
 * ========================= */
static void run_start(uint16_t seq, const char *k, const char *v)
{
  s_last_seq = seq;
  s_next_step = 0;
  s_start_ms = HAL_GetTick();

  uint8_t kaddr = 0, idx = 0;
  k_kind_t kind = K_KIND_NONE;

  if (parse_k(k, &kaddr, &kind, &idx) != 0) {
    log_printf("[RUN] K parse FAIL K=%s\r\n", k ? k : "(null)");
    s_state = SLV_IDLE;
    s_next_step = 0;
    return;
  }

  /* K 주소 불일치 시 실행하지 않고 IDLE 유지 */
  if (s_addr != 0 && kaddr != s_addr) {
    log_printf("[RUN] K addr mismatch K=%u my=%u -> IGNORE\r\n",
               (unsigned)kaddr, (unsigned)s_addr);
    s_state = SLV_IDLE;
    s_next_step = 0;
    return;
  }

  s_state = SLV_BUSY;

  if (kind == K_KIND_R) {
    out_exec_start(idx, v);
  } else if (kind == K_KIND_L) {
    in_watch_start(idx, v);
  } else {
    log_printf("[RUN] kind=P not supported on IO slave\r\n");
    s_state = SLV_IDLE;
    s_next_step = 0;
  }
}

static void run_tick(void)
{
  if (s_state != SLV_BUSY) return;
  out_exec_tick();
  in_watch_tick();
}

/* =========================
 * STATUS string
 * ========================= */
static const char* state_str(void)
{
  switch (s_state) {
    case SLV_IDLE: return "IDLE";
    case SLV_BUSY: return "BUSY";
    case SLV_DONE: return "DONE";
    default: return "UNK";
  }
}

/* =========================
 * RS485 command handlers
 * ========================= */
static void run_context_clear(void)
{
  memset(&s_out, 0, sizeof(s_out));
  memset(&s_in, 0, sizeof(s_in));
  s_state = SLV_IDLE;
  s_start_ms = 0;
  s_next_step = 0;
}

static void handle_all_off(uint8_t req_addr, uint16_t seq)
{
  io_all_off();
  run_context_clear();
  if (s_last_run_valid) s_last_run_cancelled = 1;

  if (req_addr != 0) {
    (void)rs485_if_send(s_addr, seq, "ACK", "");
  }
  log_printf("[IO] ALL_OFF (from addr=%u)\r\n", (unsigned)req_addr);
}

static void handle_m_stop(uint8_t req_addr, uint16_t seq)
{
  io_all_off();
  run_context_clear();
  if (s_last_run_valid) s_last_run_cancelled = 1;

  if (req_addr != 0) {
    (void)rs485_if_send(s_addr, seq, "ACK", "");
  }
  log_printf("[IO] M_STOP -> ALL_OFF (from addr=%u)\r\n", (unsigned)req_addr);
}

static void handle_run(uint8_t req_addr, uint16_t seq, const char *args)
{
  if (req_addr == 0) return;

  char k[64] = {0};
  char v[256] = {0};

  if (args_get_kv(args, "K", k, sizeof(k)) != 0 ||
      args_get_kv(args, "V", v, sizeof(v)) != 0) {
    log_printf("[RUN] args parse FAIL args=%s\r\n", args ? args : "(null)");
    (void)rs485_if_send(s_addr, seq, "NACK", "BAD_ARGS");
    return;
  }

  /* 동일 transaction 재전송은 기존 실행을 다시 시작하지 않는다. */
  if (s_last_run_valid && seq == s_last_seq) {
    if (strcmp(k, s_last_run_k) != 0 || strcmp(v, s_last_run_v) != 0) {
      log_printf("[RUN] seq conflict seq=%u K=%s V=%s\r\n",
                 (unsigned)seq, k, v);
      (void)rs485_if_send(s_addr, seq, "NACK", "SEQ_CONFLICT");
      return;
    }

    if (s_last_run_cancelled) {
      log_printf("[RUN] cancelled retry blocked seq=%u\r\n", (unsigned)seq);
      (void)rs485_if_send(s_addr, seq, "NACK", "CANCELLED");
      return;
    }

    log_printf("[RUN] duplicate ACK seq=%u state=%s\r\n",
               (unsigned)seq, state_str());
    (void)rs485_if_send(s_addr, seq, "ACK", "");
    return;
  }

  /* BUSY 중 다른 transaction은 기존 실행을 유지하고 거부한다. */
  if (s_state == SLV_BUSY) {
    log_printf("[RUN] BUSY ignore seq=%u K=%s V=%s\r\n",
               (unsigned)seq, k, v);
    (void)rs485_if_send(s_addr, seq, "NACK", "BUSY");
    return;
  }

  /* 새 RUN은 K 주소/종류와 R/L의 V 전체를 검증한 뒤에만 ACK한다. */
  const char *nack_reason = run_validate(k, v);
  if (nack_reason != NULL) {
    log_printf("[RUN] validate FAIL seq=%u reason=%s K=%s V=%s\r\n",
               (unsigned)seq, nack_reason, k, v);
    (void)rs485_if_send(s_addr, seq, "NACK", nack_reason);
    return;
  }

  /* 이전 R/L 실행기의 잔여 상태를 제거하고 새 transaction을 기록한다. */
  run_context_clear();
  s_last_seq = seq;
  strncpy(s_last_run_k, k, sizeof(s_last_run_k)-1);
  s_last_run_k[sizeof(s_last_run_k)-1] = 0;
  strncpy(s_last_run_v, v, sizeof(s_last_run_v)-1);
  s_last_run_v[sizeof(s_last_run_v)-1] = 0;
  s_last_run_valid = 1;
  s_last_run_cancelled = 0;

  (void)rs485_if_send(s_addr, seq, "ACK", "");

  run_start(seq, k, v);
  log_printf("[RUN] start seq=%u K=%s V=%s\r\n", (unsigned)seq, k, v);
}

static void handle_status(uint8_t req_addr, uint16_t seq)
{
  if (req_addr == 0) return;

#if STATUS_REPLY_ONE_FRAME
  char a[96];
  if (s_state == SLV_DONE) {
    snprintf(a, sizeof(a), "DONE;NEXT=%u", (unsigned)s_next_step);
  } else {
    snprintf(a, sizeof(a), "%s", state_str());
  }
  (void)rs485_if_send(s_addr, seq, "STATUS", a);
#else
  (void)rs485_if_send(s_addr, seq, "ACK", "");
  char a[96];
  if (s_state == SLV_DONE) snprintf(a, sizeof(a), "DONE;NEXT=%u", (unsigned)s_next_step);
  else snprintf(a, sizeof(a), "%s", state_str());
  (void)rs485_if_send(s_addr, seq, "STATUS", a);
#endif

#if !STATUS_PERSIST_DONE
  if (s_state == SLV_DONE) { s_state = SLV_IDLE; s_next_step = 0; }
#endif
}

/* =========================
 * App entry
 * ========================= */
void app_init(void)
{
  dip_state_t d = dip_read();
  s_addr = d.addr;

  seg595_init();
  seg595_show_u8(s_addr);

  log_printf("[SLAVE] init OK addr=%u raw=0x%02X\r\n", (unsigned)s_addr, (unsigned)d.raw);
  if (s_addr == 0) {
    log_printf("[SLAVE] WARN addr=0 reserved. Set DIP to 1..15\r\n");
  }

  rs485_if_init(&huart1, DE_485_GPIO_Port, DE_485_Pin);
  input_debounce_init();

  io_all_off();
  s_state = SLV_IDLE;
  s_start_ms = 0;
  s_last_seq = 0;
  s_next_step = 0;
  s_last_run_valid = 0;
  s_last_run_cancelled = 0;
  s_last_run_k[0] = 0;
  s_last_run_v[0] = 0;

  log_printf("[SLAVE] OUT_ACTIVE_HIGH=%u (ON=%s)\r\n",
             (unsigned)OUT_ACTIVE_HIGH,
#if OUT_ACTIVE_HIGH
             "HIGH"
#else
             "LOW"
#endif
  );

  log_printf("[SLAVE] RUN uses K=n-Rx / n-Lx, args: K=...;V=...\r\n");
  log_printf("[SLAVE] broadcast addr=0 enabled; NO-REPLY on broadcast\r\n");
}

void app_loop(void)
{
  input_debounce_tick();
  run_tick();

  rs485_diag_t diag;
  rs485_if_get_diag(&diag, true);
  if (diag.rx_overrun_pending || diag.rx_oversize_pending) {
    log_printf("[RS485_RX] overrun=%lu oversize=%lu\r\n",
               (unsigned long)diag.rx_overrun_count,
               (unsigned long)diag.rx_oversize_count);
  }
  if (diag.uart_error_pending) {
    log_printf("[UART1] err=0x%08lX ORE=%lu FE=%lu NE=%lu PE=%lu rearm_fail=%lu\r\n",
               (unsigned long)diag.uart_last_error,
               (unsigned long)diag.uart_ore_count,
               (unsigned long)diag.uart_fe_count,
               (unsigned long)diag.uart_ne_count,
               (unsigned long)diag.uart_pe_count,
               (unsigned long)diag.uart_rx_rearm_fail_count);
  }
  if (diag.tx_error_pending) {
    log_printf("[RS485_TX] fail=%lu tc_timeout=%lu\r\n",
               (unsigned long)diag.tx_fail_count,
               (unsigned long)diag.tx_tc_timeout_count);
  }

  rs485_req_t req;
  rs485_parse_result_t res;

  uint8_t frame_count = 0;
  while (frame_count < APP_FRAME_BUDGET && rs485_if_poll(&req, &res))
  {
    frame_count++;
    if (res == RS485_MSG_BADCRC) {
      log_printf("[RS485_RX] BADCRC\r\n");
      continue;
    }
    if (res == RS485_MSG_BADFMT) {
      log_printf("[RS485_RX] BADFMT\r\n");
      continue;
    }
    if (res != RS485_MSG_OK) {
      log_printf("[RS485_RX] PARSE_ERR=%d\r\n", (int)res);
      continue;
    }

    log_printf("[RS485_RX] OK addr=%u seq=%u cmd=%s args=%s\r\n",
               (unsigned)req.addr,
               (unsigned)req.seq,
               req.cmd,
               req.args);

    /* 주소 정책:
     * - 내 주소(s_addr) 또는 브로드캐스트(addr==0)만 처리
     * - DIP addr=0인 보드는 브로드캐스트만 수용
     */
    if (s_addr == 0) {
      if (req.addr != 0) {
        log_printf("[RS485_RX] IGNORE addr=%u my=%u (broadcast only)\r\n",
                   (unsigned)req.addr, (unsigned)s_addr);
        continue;
      }
    } else {
      if (req.addr != s_addr && req.addr != 0) {
        log_printf("[RS485_RX] IGNORE addr=%u my=%u\r\n",
                   (unsigned)req.addr, (unsigned)s_addr);
        continue;
      }
    }

    if (strcmp(req.cmd, "ALL_OFF") == 0) {
      handle_all_off(req.addr, req.seq);
      continue;
    }

    if (strcmp(req.cmd, "M_STOP") == 0) {
      handle_m_stop(req.addr, req.seq);
      continue;
    }

    if (strcmp(req.cmd, "RUN") == 0) {
      handle_run(req.addr, req.seq, req.args);
      continue;
    }

    if (strcmp(req.cmd, "STATUS") == 0) {
      handle_status(req.addr, req.seq);
      continue;
    }

    /* 지원하지 않는 명령은 ACK로 덮지 말고 NACK */
    if (req.addr != 0) {
      log_printf("[RS485_RX] UNKNOWN cmd=%s -> NACK\r\n", req.cmd);
      (void)rs485_if_send(s_addr, req.seq, "NACK", "UNKNOWN_CMD");
    }
  }
}
