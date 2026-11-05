/**
 * @file    mop_channel_types.h
 * @brief   MopChannel enums + structs + init functions (DO NOT EDIT).
 */
#ifndef MOP_CHANNEL_TYPES_H
#define MOP_CHANNEL_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROLE_SERVER = 0,
    ROLE_CLIENT = 1,
} E_Role;

typedef enum {
    RELIABILITY_RELIABLE = 0,
    RELIABILITY_UNRELIABLE = 1,
} E_Reliability;

typedef enum {
    CHANNEL_STATE_UNKNOWN = 0,
    CHANNEL_STATE_ACTIVE = 1,
    CHANNEL_STATE_CLOSED = 2,
} E_ChannelState;

typedef enum {
    TARGET_DEVICE_TYPE_UNKNOWN = 0,
    TARGET_DEVICE_TYPE_MSDK = 1,
    TARGET_DEVICE_TYPE_OSDK = 2,
    TARGET_DEVICE_TYPE_PSDK = 3,
} E_TargetDeviceType;


typedef struct {
  E_Role             role;
  E_Reliability      reliability;
  uint32_t           channel_id;
  E_TargetDeviceType target_device_type;
  uint32_t           target_address;
} T_MopEnableRequest;

typedef struct {
  int32_t  result;
  char     *result_message;
  uint32_t channel_id;
  char     *state;
  char     *ip;
  uint32_t port;
} T_MopEnableReply;

typedef struct {
  uint32_t channel_id;
} T_MopDisableRequest;

typedef struct {
  uint32_t       channel_id;
  E_ChannelState state;
} T_ChannelEventPush;


static inline void MopChannel_InitMopEnableRequest(T_MopEnableRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void MopChannel_InitMopEnableReply(T_MopEnableReply *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void MopChannel_InitMopDisableRequest(T_MopDisableRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void MopChannel_InitChannelEventPush(T_ChannelEventPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* MOP_CHANNEL_TYPES_H */
