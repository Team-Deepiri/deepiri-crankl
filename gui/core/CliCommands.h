#ifndef CRANKL_GUI_CORE_CLI_COMMANDS_H
#define CRANKL_GUI_CORE_CLI_COMMANDS_H

#include "core/CranklJob.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace crankl_gui {

// Builders for the `crankl <command> ...` invocations the GUI runs as
// subprocess jobs. The returned list contains only the tokens after the
// command name; JobManager prepends the resolved executable and the command.
//
// Mirror of the CLI surface in src/cli/main.cpp (kept read-only), so these
// must stay in lockstep with it.

QString cliCommandName(JobType type);

QStringList packArgs(const QString &input, const QString &output);
QStringList turnArgs(const QString &input, const QString &output, int steps, double lr);
QStringList finetuneArgs(const QString &input, const QString &output, int steps, double lr);
QStringList peelArgs(const QString &input, const QString &output, int layers);
QStringList compareArgs(const QString &archiveA, const QString &archiveB);
QStringList inspectArgs(const QString &path);

// Extracts the last JSON object that parses from a captured stdout buffer
// (compare/inspect/finetune --json print one object). Returns nullopt when
// nothing parses, so callers never misread plain-text output as structured.
std::optional<QJsonObject> extractJsonFromOutput(const QString &output);

} // namespace crankl_gui

#endif // CRANKL_GUI_CORE_CLI_COMMANDS_H