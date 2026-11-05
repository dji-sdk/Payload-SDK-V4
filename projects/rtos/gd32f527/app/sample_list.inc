/* app/sample_list.inc — RTOS-side sample list.
 *
 * Relationship with projects/linux/sample_list.inc:
 *   Linux's full list compiles many samples: camera/efts/power/time_sync/widget/custom_small_data/data_subscription
 *   etc.; the RTOS GD32 firmware only compiles the 3 that sample_registry.c needs to see:
 *     - custom_small_data       (can run standalone, shares one source with Linux, #if platform split)
 *     - data_subscription       (can run standalone, same as above; needs a real flight controller to receive pushes)
 *     - rtos_all                (RTOS-only combined sample, shares a session and concurrently runs the above two)
 *   AUTORUN defaults to rtos_all, directly starting the two business tasks concurrently.
 *
 * CMakeLists adds this directory (app/) to the include path before samples/launcher, so that
 * the `#include "sample_list.inc"` in samples/launcher/sample_registry.c resolves to here.
 *
 * 3 things to do to add a new sample to the RTOS firmware:
 *   1. Add the corresponding demo.c to add_executable in CMakeLists.txt (first confirm it compiles on RTOS, usually needs
 *      to be isolated by #if __linux__/__unix__/__APPLE__ to split off the POSIX sections)
 *   2. Add add_library(thing_model_gen_<cap> ...) to CMakeLists.txt
 *   3. Add one line SAMPLE(<cap>) to this file
 *
 * Note: to run a certain sample standalone rather than through rtos_all, change the
 *     SAMPLE_AUTORUN macro in CMakeLists to that sample name (e.g. "data_subscription").
 */
SAMPLE(custom_small_data)
SAMPLE(data_subscription)
SAMPLE(rtos_all)
