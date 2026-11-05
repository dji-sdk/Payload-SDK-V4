/**
 * @file    power_publisher.c    (self-contained)
 *
 *   For the device side / mock: pushes messages to the event/osd/state topics.
 *   The topic is built inline at CreatePublisher time according to the session's topic_style.
 *   Pushes are uniformly wrapped in a {seq, method, data} envelope (method gets the V2 cap prefix via build_method).
 *   The method carries the V2 cap prefix via build_method.
 */
#include "power_publisher.h"
#include "power_topics.h"
#include "power_json.h"
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *KIND_SUFFIX_PUB_V1[THING_MODEL_TOPIC_KIND_MAX] = {
    "service", "service_reply",
    "properties/get", "properties/get_reply",
    "properties/set", "properties/set_reply",
    "event", "osd", "state",
};

static const char *KIND_SUFFIX_PUB_V2[THING_MODEL_TOPIC_KIND_MAX] = {
    "services", "services_reply",
    "property/get", "property/get_reply",
    "property/set", "property/set_reply",
    "events", "osd", "state",
};

static void build_topic_pub(char *out, size_t cap, E_ThingModelTopicStyle style,
                            const char *sn, E_ThingModelTopicKind kind)
{
    if (style == THING_MODEL_TOPIC_STYLE_V2)
        /* V2 fixed owner=product, without the cap segment, and no leading / before thing */
        snprintf(out, cap, "thing/product/%s/%s",
                 sn, KIND_SUFFIX_PUB_V2[kind]);
    else
        snprintf(out, cap, "/thing/%s/%s/%s/%s",
                 POWER_TOPIC_OWNER, sn,
                 POWER_TOPIC_CAP_NAME, KIND_SUFFIX_PUB_V1[kind]);
}

/* payload.method string: V2 adds the cap prefix, V1 uses the bare name (aligned with client / subscriber-side expect_method). */
static void build_method(char *out, size_t cap, E_ThingModelTopicStyle style,
                         const char *base)
{
    if (style == THING_MODEL_TOPIC_STYLE_V2)
        snprintf(out, cap, "%s_%s", POWER_TOPIC_CAP_NAME, base);
    else
        snprintf(out, cap, "%s", base);
}

struct Power_Publisher {
    ThingModelClientSession_t *session;
    E_ThingModelTopicStyle     style;
    char topic_event[256];
    char topic_osd[256];
    char topic_state[256];
};

Power_Publisher_t *Power_CreatePublisher(ThingModelClientSession_t *session)
{
    if (!session) return NULL;
    Power_Publisher_t *p = ThingModelMem_Calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->session = session;
    E_ThingModelTopicStyle style = ThingModelClientSession_GetTopicStyle(session);
    p->style = style;
    const char *sn = ThingModelClientSession_GetSn(session);
    build_topic_pub(p->topic_event, sizeof(p->topic_event), style, sn, THING_MODEL_TOPIC_EVENT);
    build_topic_pub(p->topic_osd,   sizeof(p->topic_osd),   style, sn, THING_MODEL_TOPIC_OSD);
    build_topic_pub(p->topic_state, sizeof(p->topic_state), style, sn, THING_MODEL_TOPIC_STATE);
    return p;
}

void Power_DestroyPublisher(Power_Publisher_t *p) { ThingModelMem_Free(p); }

static T_ThingModelReturnCode pub_raw(ThingModelClientSession_t *s, const char *topic, char *payload)
{
    if (!payload) return THING_MODEL_RC_OUT_OF_MEMORY;
    T_ThingModelReturnCode rc = ThingModelClientSession_Publish(s, topic, payload, 0);
    ThingModelMem_Free(payload);
    return rc;
}

