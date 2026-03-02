// File Logger for Trading Terminal
// Provides file + stderr logging with thread-safe message handler.
// Call setupFileLogging() once at startup, cleanupFileLogging() at shutdown.

#ifndef FILELOGGER_H
#define FILELOGGER_H

/// Install Qt message handler that logs to file + stderr.
/// Creates a timestamped log file in a `logs/` directory.
void setupFileLogging();

/// Close the log file and release resources.
void cleanupFileLogging();

#endif // FILELOGGER_H
