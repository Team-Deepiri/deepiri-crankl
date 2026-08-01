#ifndef CRANKL_GUI_CORE_ARCHIVE_ADAPTER_H
#define CRANKL_GUI_CORE_ARCHIVE_ADAPTER_H

#include "core/ArchiveSnapshot.h"

#include <QMetaType>
#include <QString>

namespace crankl_gui {

// Result of one open attempt. `ok` is false only when the file could not be
// opened as a .crank archive at all (crankl_cran_read itself failed -- bad
// magic, missing file, truncated header).
struct ArchiveOpenResult {
    bool ok = false;
    ArchiveSnapshot snapshot;
    QString errorMessage;
};

class ArchiveAdapter {
  public:
    static ArchiveOpenResult openArchive(const QString &path);
};

} // namespace crankl_gui

Q_DECLARE_METATYPE(crankl_gui::ArchiveOpenResult)

#endif // CRANKL_GUI_CORE_ARCHIVE_ADAPTER_H
