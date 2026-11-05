/* sample_autostart.inc — Linux auto-start policy (which services come up on boot).
 *
 * This is LAUNCH POLICY, deliberately kept separate from the samples: a sample declares whether it
 * *is* a service (SAMPLE_REGISTER_SERVICE), but which services are auto-started — and with what args
 * (e.g. a distinct HTTP port so two services don't collide on :8000) — is decided here, per platform.
 *
 * Format:  AUTOSTART(name, argc, argv_array_or_NULL)
 *   • name  must be a SAMPLE_KIND_SERVICE registered in sample_list.inc.
 *   • argv  extra tokens appended after argv[0]=name; use string literals (they must outlive the run).
 *
 * The launcher includes this once, under the interactive/headless front door, on POSIX only.
 * RTOS has its own composition in launcher_task.c and does not use this file.
 */

/* widget: keeps the documented default HTTP file-server port :8000. */
AUTOSTART(widget, 0, NULL)

/* upgrade: resident firmware-upgrade service. Its HTTP firmware port defaults to :8001 so it
 * coexists with widget's :8000 (override with THING_MODEL_UPGRADE_HTTP_PORT). By default the
 * upgrade is STAGE-ONLY (no binary overwrite, no reboot) — real install is an opt-in deploy
 * integration, see samples/upgrade/deploy/.
 * NOTE: because the widget service above already owns get_file_list, the launcher sets
 * THING_MODEL_UPGRADE_WIDGET=0 before autostart to suppress upgrade's fallback responder. If you
 * remove the widget line above, set THING_MODEL_UPGRADE_WIDGET=1 so DA2 still lists the device. */
AUTOSTART(upgrade, 0, NULL)
