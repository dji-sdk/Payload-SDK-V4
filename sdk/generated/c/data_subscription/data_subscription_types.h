/**
 * @file    data_subscription_types.h
 * @brief   DataSubscription enums + structs + init functions (DO NOT EDIT).
 */
#ifndef DATA_SUBSCRIPTION_TYPES_H
#define DATA_SUBSCRIPTION_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DATA_SUBSCRIPTION_FREQUENCY_UNSPECIFIED = 0,
    DATA_SUBSCRIPTION_FREQUENCY_1HZ = 1,
    DATA_SUBSCRIPTION_FREQUENCY_5HZ = 5,
    DATA_SUBSCRIPTION_FREQUENCY_10HZ = 10,
    DATA_SUBSCRIPTION_FREQUENCY_50HZ = 50,
} E_DataSubscriptionFrequency;


typedef struct {
  char                        *topic;
  E_DataSubscriptionFrequency frequency;
} T_DataSubscriptionSubscribeTopicRequest;

typedef struct {
  char     *topic;
} T_DataSubscriptionUnsubscribeTopicRequest;

typedef struct {
  char     *topic;
} T_DataSubscriptionGetLatestValueRequest;

typedef struct {
  float q0;
  float q1;
  float q2;
  float q3;
} T_DataSubscriptionQuaternion;

typedef struct {
  float x;
  float y;
  float z;
} T_DataSubscriptionVector3f;

typedef struct {
  float   x;
  float   y;
  float   z;
  int32_t health;
} T_DataSubscriptionVelocity;

typedef struct {
  float altitude;
} T_DataSubscriptionAltitude;

typedef struct {
  float height;
} T_DataSubscriptionHeight;

typedef struct {
  double  longitude;
  double  latitude;
  float   altitude;
  int32_t visible_satellite_number;
} T_DataSubscriptionPositionFused;

typedef struct {
  float   x;
  float   y;
  float   z;
  int32_t x_health;
  int32_t y_health;
  int32_t z_health;
} T_DataSubscriptionPositionVo;

typedef struct {
  int32_t date;
} T_DataSubscriptionGpsDate;

typedef struct {
  int32_t time;
} T_DataSubscriptionGpsTime;

typedef struct {
  int32_t x;
  int32_t y;
  int32_t z;
} T_DataSubscriptionVector3i;

typedef struct {
  float   hdop;
  float   pdop;
  float   fix_state;
  float   vacc;
  float   hacc;
  float   sacc;
  int32_t gps_satellite_number_used;
  int32_t glonass_satellite_number_used;
  int32_t total_satellite_number_used;
  int32_t gps_counter;
} T_DataSubscriptionGpsDetails;

typedef struct {
  int32_t gps_signal_level;
} T_DataSubscriptionGpsSignalLevel;

typedef struct {
  int32_t yaw;
} T_DataSubscriptionRtkYaw;

typedef struct {
  int32_t position_solution_properaty;
} T_DataSubscriptionRtkPositionInfo;

typedef struct {
  int32_t yaw_info;
} T_DataSubscriptionRtkYawInfo;

typedef struct {
  int32_t connected;
} T_DataSubscriptionRtkConnectStatus;

typedef struct {
  int32_t roll;
  int32_t pitch;
  int32_t yaw;
  int32_t throttle;
  int32_t mode;
  int32_t gear;
} T_DataSubscriptionRc;

typedef struct {
  float   pitch;
  float   roll;
  float   yaw;
  float   throttle;
  int32_t logic_connected;
  int32_t sky_connected;
  int32_t app_connected;
  int32_t ground_connected;
} T_DataSubscriptionRcWithFlagData;

typedef struct {
  int32_t status;
} T_DataSubscriptionStatusFlight;

typedef struct {
  int32_t status;
} T_DataSubscriptionStatusDisplayMode;

typedef struct {
  int32_t control_authority;
  int32_t control_authority_change_reason;
} T_DataSubscriptionControlDevice;

typedef struct {
  int32_t current;
  int32_t speed;
  int32_t voltage;
  int32_t temperature;
  int32_t stall;
  int32_t empty;
  int32_t unbalanced;
  int32_t esc_disconnected;
  int32_t temperature_high;
} T_DataSubscriptionEscItem;

typedef struct {
  int32_t impact_in_air;
  int32_t random_fly;
  int32_t height_ctrl_fail;
  int32_t roll_pitch_ctrl_fail;
  int32_t yaw_ctrl_fail;
  int32_t aircraft_is_falling;
  int32_t strong_wind_level1;
  int32_t strong_wind_level2;
  int32_t compass_installation_error;
  int32_t imu_installation_error;
  int32_t esc_temperature_high;
  int32_t at_least_one_esc_disconnected;
  int32_t gps_yaw_error;
} T_DataSubscriptionFlightAnomaly;

typedef struct {
  float   down;
  float   front;
  float   right;
  float   back;
  float   left;
  float   up;
  int32_t down_health;
  int32_t front_health;
  int32_t right_health;
  int32_t back_health;
  int32_t left_health;
  int32_t up_health;
} T_DataSubscriptionAvoidData;

typedef struct {
  int32_t status;
} T_DataSubscriptionHomePointSetStatus;

typedef struct {
  double longitude;
  double latitude;
} T_DataSubscriptionHomePointInfo;

typedef struct {
  int32_t cell_break;
  int32_t self_check_error;
  int32_t battery_closed_reason;
  int32_t bat_soh_state;
  int32_t max_cycle_limit;
  int32_t less_battery;
  int32_t battery_communication_abnormal;
  int32_t has_cell_break;
  int32_t is_battery_embed;
  int32_t heat_state;
  int32_t soc_state;
} T_DataSubscriptionBatteryState;

typedef struct {
  int32_t                        battery_index;
  int32_t                        current_voltage;
  int32_t                        current_electric;
  int32_t                        full_capacity;
  int32_t                        remained_capacity;
  int32_t                        battery_temperature;
  int32_t                        cell_count;
  int32_t                        battery_capacity_percent;
  T_DataSubscriptionBatteryState battery_state;
  int32_t                        SOP;
} T_DataSubscriptionBatterySingleInfo;

typedef struct {
  float x;
  float y;
  float z;
} T_DataSubscriptionGimbalAngles;

typedef struct {
  T_DataSubscriptionQuaternion         quaternion;
  T_DataSubscriptionVector3f           acceleration_ground;
  T_DataSubscriptionVector3f           acceleration_body;
  T_DataSubscriptionVector3f           acceleration_raw;
  T_DataSubscriptionVelocity           velocity;
  T_DataSubscriptionVector3f           angular_rate_fused;
  T_DataSubscriptionVector3f           angular_rate_raw;
  T_DataSubscriptionAltitude           altitude_fused;
  T_DataSubscriptionAltitude           altitude_barometer;
  T_DataSubscriptionAltitude           altitude_of_homepoint;
  T_DataSubscriptionHeight             height_fusion;
  T_DataSubscriptionPositionFused      position_fused;
  T_DataSubscriptionPositionVo         position_vo;
  T_DataSubscriptionGpsDate            gps_date;
  T_DataSubscriptionGpsTime            gps_time;
  T_DataSubscriptionVector3i           gps_position;
  T_DataSubscriptionVector3f           gps_velocity;
  T_DataSubscriptionGpsDetails         gps_details;
  T_DataSubscriptionGpsSignalLevel     gps_signal_level;
  T_DataSubscriptionVector3f           rtk_position;
  T_DataSubscriptionVector3f           rtk_velocity;
  T_DataSubscriptionRtkYaw             rtk_yaw;
  T_DataSubscriptionRtkPositionInfo    rtk_position_info;
  T_DataSubscriptionRtkYawInfo         rtk_yaw_info;
  T_DataSubscriptionRtkConnectStatus   rtk_connect_status;
  T_DataSubscriptionRc                 rc;
  T_DataSubscriptionRcWithFlagData     rc_with_flag_data;
  T_DataSubscriptionStatusFlight       status_flight;
  T_DataSubscriptionStatusDisplayMode  status_displaymode;
  T_DataSubscriptionControlDevice      control_device;
  T_DataSubscriptionVector3i           compass;
  T_DataSubscriptionFlightAnomaly      flight_anomaly;
  T_DataSubscriptionAvoidData          avoid_data;
  T_DataSubscriptionHomePointSetStatus home_point_set_status;
  T_DataSubscriptionHomePointInfo      home_point_info;
  T_DataSubscriptionBatterySingleInfo  battery_single_info_index1;
  T_DataSubscriptionBatterySingleInfo  battery_single_info_index2;
  T_DataSubscriptionGimbalAngles       gimbal_angles;
} T_DataSubscriptionPublishDataPush;


static inline void DataSubscription_InitDataSubscriptionSubscribeTopicRequest(T_DataSubscriptionSubscribeTopicRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionUnsubscribeTopicRequest(T_DataSubscriptionUnsubscribeTopicRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionGetLatestValueRequest(T_DataSubscriptionGetLatestValueRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionQuaternion(T_DataSubscriptionQuaternion *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionVector3f(T_DataSubscriptionVector3f *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionVelocity(T_DataSubscriptionVelocity *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionAltitude(T_DataSubscriptionAltitude *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionHeight(T_DataSubscriptionHeight *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionPositionFused(T_DataSubscriptionPositionFused *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionPositionVo(T_DataSubscriptionPositionVo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionGpsDate(T_DataSubscriptionGpsDate *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionGpsTime(T_DataSubscriptionGpsTime *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionVector3i(T_DataSubscriptionVector3i *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionGpsDetails(T_DataSubscriptionGpsDetails *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionGpsSignalLevel(T_DataSubscriptionGpsSignalLevel *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionRtkYaw(T_DataSubscriptionRtkYaw *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionRtkPositionInfo(T_DataSubscriptionRtkPositionInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionRtkYawInfo(T_DataSubscriptionRtkYawInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionRtkConnectStatus(T_DataSubscriptionRtkConnectStatus *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionRc(T_DataSubscriptionRc *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionRcWithFlagData(T_DataSubscriptionRcWithFlagData *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionStatusFlight(T_DataSubscriptionStatusFlight *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionStatusDisplayMode(T_DataSubscriptionStatusDisplayMode *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionControlDevice(T_DataSubscriptionControlDevice *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionEscItem(T_DataSubscriptionEscItem *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionFlightAnomaly(T_DataSubscriptionFlightAnomaly *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionAvoidData(T_DataSubscriptionAvoidData *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionHomePointSetStatus(T_DataSubscriptionHomePointSetStatus *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionHomePointInfo(T_DataSubscriptionHomePointInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionBatteryState(T_DataSubscriptionBatteryState *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionBatterySingleInfo(T_DataSubscriptionBatterySingleInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionGimbalAngles(T_DataSubscriptionGimbalAngles *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void DataSubscription_InitDataSubscriptionPublishDataPush(T_DataSubscriptionPublishDataPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* DATA_SUBSCRIPTION_TYPES_H */
