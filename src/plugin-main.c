/*
 * PFL Preview — OBS Studio Plugin
 * Copyright (C) 2026 DABLO
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "pt-BR")

void pfl_dock_initialize(void);
void pfl_dock_shutdown(void);
void pfl_dock_update_preview_scene(void);

static void pfl_frontend_event(
    enum obs_frontend_event event,
    void *private_data)
{
    (void)private_data;

    if (event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED) {

        obs_log(
            LOG_INFO,
            "[PFL Preview] EVENTO: Preview mudou");

        pfl_dock_update_preview_scene();

    } else if (event == OBS_FRONTEND_EVENT_EXIT) {

        pfl_dock_shutdown();
    }
}

bool obs_module_load(void)
{
    obs_log(
        LOG_INFO,
        "[PFL Preview] Carregando plugin — versão %s",
        PLUGIN_VERSION);

    pfl_dock_initialize();

    obs_frontend_add_event_callback(
        pfl_frontend_event,
        NULL);

    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_event_callback(
        pfl_frontend_event,
        NULL);

    pfl_dock_shutdown();

    obs_log(
        LOG_INFO,
        "[PFL Preview] Plugin descarregado");
}