#ifndef PMO_TEST_STUB_H
#define PMO_TEST_STUB_H
#include <stddef.h>
typedef struct pmo pmo_t;
typedef enum { PMO_EV_NONE=0, PMO_EV_SELECT, PMO_EV_TOGGLE, PMO_EV_SLIDE, PMO_EV_BACK, PMO_EV_SHOW, PMO_EV_HIDE, PMO_EV_QUIT } pmo_event_kind;
typedef struct { pmo_event_kind kind; char id[49]; int on; int value; } pmo_event_t;
pmo_t *pmo_connect(const char *id);
void pmo_close(pmo_t *o);
int pmo_wait(pmo_t *o, pmo_event_t *ev, int timeout_ms);
int pmo_visible(const pmo_t *o);
void pmo_page_begin(pmo_t *o, const char *title, const char *subtitle);
void pmo_header(pmo_t *o, const char *label);
void pmo_item(pmo_t *o, const char *id, const char *label, const char *value, const char *detail);
void pmo_footer(pmo_t *o, const char *footer);
int pmo_page_end(pmo_t *o);
int pmo_patch_value(pmo_t *o, const char *id, const char *value);
int pmo_patch_detail(pmo_t *o, const char *id, const char *detail);
int pmo_exit(pmo_t *o);
#endif
