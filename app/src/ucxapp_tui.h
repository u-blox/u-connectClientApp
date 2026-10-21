#ifndef UCXAPP_TUI_H
#define UCXAPP_TUI_H

/* Three-pane text UI (commands, log, events) in 80 columns; PC targets only. */

#include "ucxapp.h"

/** Runs until the user quits; returns 0, or 1 when stdin/stdout are not a terminal. */
int ucxapp_tui_run(ucxapp_t *app);

/** "u-connectClientApp 3.4.0.261020 (built Oct 20 2026 20:28:01)" */
const char *ucxapp_version_text(void);

#endif /* UCXAPP_TUI_H */
