/**
 * @file    speaker_types.h
 * @brief   Speaker enums + structs + init functions (DO NOT EDIT).
 */
#ifndef SPEAKER_TYPES_H
#define SPEAKER_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  int32_t work_mode;
} T_SpeakerSetWorkModeRequest;

typedef struct {
  int32_t play_mode;
} T_SpeakerSetPlayModeRequest;

typedef struct {
  int32_t volume;
} T_SpeakerSetVolumeRequest;

typedef struct {
  int32_t  file_type;
  char     *uuid;
  char     *file_name;
  int32_t  decode_bitrate;
} T_SpeakerTransStartRequest;

typedef struct {
  int32_t            packet_index;
  int32_t            offset;
  T_ThingModelBytes  data;
} T_SpeakerTransDataRequest;

typedef struct {
  char     *md5;
} T_SpeakerTransFinishRequest;

typedef struct {
  int32_t  system_state;
  int32_t  work_mode;
  int32_t  play_mode;
  int32_t  play_volume;
  char     *play_file_name;
  char     *play_file_uuid;
} T_SpeakerStatePush;


static inline void Speaker_InitSpeakerSetWorkModeRequest(T_SpeakerSetWorkModeRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Speaker_InitSpeakerSetPlayModeRequest(T_SpeakerSetPlayModeRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Speaker_InitSpeakerSetVolumeRequest(T_SpeakerSetVolumeRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Speaker_InitSpeakerTransStartRequest(T_SpeakerTransStartRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Speaker_InitSpeakerTransDataRequest(T_SpeakerTransDataRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Speaker_InitSpeakerTransFinishRequest(T_SpeakerTransFinishRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Speaker_InitSpeakerStatePush(T_SpeakerStatePush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* SPEAKER_TYPES_H */
