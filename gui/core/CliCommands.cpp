#include "core/CliCommands.h"

#include <QJsonDocument>
#include <QStringView>

namespace crankl_gui {

QString cliCommandName(JobType type) {
    switch (type) {
    case JobType::Pack:
        return QStringLiteral("pack");
    case JobType::Turn:
        return QStringLiteral("turn");
    case JobType::Finetune:
        return QStringLiteral("finetune");
    case JobType::Peel:
        return QStringLiteral("peel");
    case JobType::Compare:
        return QStringLiteral("compare");
    case JobType::Inspect:
        return QStringLiteral("inspect");
    case JobType::OpenArchive:
        return QString();
    }
    return {};
}

QStringList packArgs(const QString &input, const QString &output) {
    // --shape omitted: the CLI auto-derives ceil(float_count / 64).
    return {QStringLiteral("--input"), input, QStringLiteral("-o"), output};
}

QStringList turnArgs(const QString &input, const QString &output, int steps, double lr) {
    return {QStringLiteral("--input"), input,
            QStringLiteral("-o"),      output,
            QStringLiteral("--steps"), QString::number(steps),
            QStringLiteral("--lr"),    QString::number(lr, 'g', 10)};
}

QStringList finetuneArgs(const QString &input, const QString &output, int steps, double lr) {
    // --json so the GUI can parse a structured result summary.
    return {QStringLiteral("--input"), input,
            QStringLiteral("-o"),      output,
            QStringLiteral("--steps"), QString::number(steps),
            QStringLiteral("--lr"),    QString::number(lr, 'g', 10),
            QStringLiteral("--json")};
}

QStringList peelArgs(const QString &input, const QString &output, int layers) {
    return {QStringLiteral("--input"), input,
            QStringLiteral("-o"),      output,
            QStringLiteral("--layers"), QString::number(layers)};
}

QStringList compareArgs(const QString &archiveA, const QString &archiveB) {
    return {archiveA, archiveB, QStringLiteral("--json")};
}

QStringList inspectArgs(const QString &path) {
    return {path, QStringLiteral("--json")};
}

std::optional<QJsonObject> extractJsonFromOutput(const QString &output) {
    std::optional<QJsonObject> parsed;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || !trimmed.startsWith(QLatin1Char('{')))
            continue;
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject())
            parsed = doc.object();
    }
    return parsed;
}

} // namespace crankl_gui