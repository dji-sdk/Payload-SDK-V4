/**
 * @file    bind_types.h
 * @brief   Bind enums + structs + init functions (DO NOT EDIT).
 */
#ifndef BIND_TYPES_H
#define BIND_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BIND_STATE_IDLE = 0,
    BIND_STATE_CHECKING = 1,
    BIND_STATE_BOUND = 2,
    BIND_STATE_BINDING = 3,
    BIND_STATE_BIND_SUCCESS = 4,
    BIND_STATE_BIND_FAILED = 5,
    BIND_STATE_UNBINDING = 6,
    BIND_STATE_UNBIND_SUCCESS = 7,
    BIND_STATE_UNBIND_FAILED = 8,
} E_BindState;


typedef struct {
  E_BindState bind_state;
  char        *bind_state_desc;
  char        *message;
  int64_t     timestamp_ms;
} T_BindRequired;


static inline void Bind_InitBindRequired(T_BindRequired *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* BIND_TYPES_H */
