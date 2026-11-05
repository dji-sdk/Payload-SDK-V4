/* sample_list.inc —— single source of truth for the registry (committed to the repo, hand-editable by any build system).
 *
 * Rules:
 *   • Keep the SAMPLE(<id>) line for whichever sample you want, comment it out otherwise.
 *   • For each <id> listed here, its .c (including the trailing SAMPLE_REGISTER(<id>,...))
 *     must be compiled into the link, or linking reports undefined symbol `sample_<id>` (this is a useful error).
 *   • Conversely: compiling a sample's .c without listing it here just keeps it out of the menu, no error.
 *   • Line order = menu order.
 *
 * Adding a new sample = add a line here + write its .c + (CMake users) add one sample_add line in CMakeLists.
 *
 * Note: this file is included twice by sample_registry.c (first for extern declarations, then to populate the table),
 *     so do not put anything other than SAMPLE(...) here.
 */
SAMPLE(custom_small_data)
SAMPLE(data_subscription)
SAMPLE(efts)
SAMPLE(mop_channel)
SAMPLE(power)
SAMPLE(time_sync)
SAMPLE(upgrade)
SAMPLE(widget)
