# r29b (test build after r29a)

Fix: Save Project and Save Sample could show **Internal** again. External is now enforced for as long as the window is open (the native code was resetting the flag after the first few frames). When this happens the log gets a `SAVE_RESEED` line.

Everything else is as in [r29a](RELEASE_NOTES-r29a.md).

[Русский](RELEASE_NOTES-r29b.ru.md)
