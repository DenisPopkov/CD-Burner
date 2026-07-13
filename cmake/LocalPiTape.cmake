# Optional local Pi defaults (host, etc.). Pi tape is enabled only via -DCASSETTE_ENABLE_PI_TAPE=ON
# or cmake/PiTapeDevOptions.cmake — never auto-enabled from this file.
set(PI_TAPE_DEFAULT_HOST "192.168.1.119" CACHE STRING "Default Pi FTP host for local dev tools")
