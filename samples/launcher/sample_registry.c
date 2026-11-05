/**
 * @file    sample_registry.c
 * @brief   Sample registry implementation — include sample_list.inc twice to get the full table. Pure ISO C.
 */
#include "sample_registry.h"
#include <string.h>

/* 1) Declare the extern symbols of all listed samples (defined by each sample file's trailing SAMPLE_REGISTER). */
#define SAMPLE(id) extern const SampleEntry sample_##id;
#include "sample_list.inc"
#undef SAMPLE

/* 2) Collect them into a pointer table (order = sample_list.inc line order = menu order). */
static const SampleEntry *const g_tbl[] = {
#define SAMPLE(id) &sample_##id,
#include "sample_list.inc"
#undef SAMPLE
};

int sample_registry_count(void)
{
    return (int)(sizeof(g_tbl) / sizeof(g_tbl[0]));
}

const SampleEntry *sample_registry_get(int i)
{
    return (i >= 0 && i < sample_registry_count()) ? g_tbl[i] : 0;
}

const SampleEntry *sample_registry_find(const char *name)
{
    if (!name) return 0;
    for (int i = 0; i < sample_registry_count(); ++i)
        if (strcmp(g_tbl[i]->name, name) == 0)
            return g_tbl[i];
    return 0;
}
