/* A native allocation failure must return to the C caller's cleanup path.
 * There is no global event-factory override or alternate command executor. */
extern "C" int step_try_call(void (*function)(void *),void *context) noexcept
{
    try {
        function(context);
        return 1;
    } catch (...) { return 0; }
}
