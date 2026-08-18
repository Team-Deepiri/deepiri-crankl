#ifndef CRANKL_GUI_WIDGETS_ARCHIVE_HEALTH_HEADER_H
#define CRANKL_GUI_WIDGETS_ARCHIVE_HEALTH_HEADER_H

#include "core/ArchiveSnapshot.h"

#include <QFrame>

class QLabel;
class QPushButton;
class QToolButton;

namespace crankl_gui {

// The one reusable archive-status widget every archive page embeds (§5.1,
// design turn 2). Fixed four-zone anatomy -- state | identity | counts |
// actions -- in that order, never reordered, never changing height across
// states. The four count cells always render a value; "none", "unavailable"
// and the empty-state em dash are values, not blanks. Read-only, forever:
// no write action is ever added to the actions zone, so pages that write in
// later phases can still safely embed this exact widget.
//
// An empty (default-constructed) snapshot renders the design's 4a
// "NO ARCHIVE" state: neutral rail, greyed identity, em-dash counts, and
// disabled actions -- the header never disappears on pages that keep
// archive context.
class ArchiveHealthHeader : public QFrame {
    Q_OBJECT
  public:
    explicit ArchiveHealthHeader(QWidget *parent = nullptr);

    void setSnapshot(const ArchiveSnapshot &snapshot);
    void setVerifyState(VerifyState state);

    // Compact is driven by the host page's width
    void setCompact(bool compact);

  Q_SIGNALS:
    void verifyRequested();
    void jsonCopyRequested();
    void closeRequested();

  private:
    void buildUi();
    void applyState();
    void applyCompact();

    ArchiveSnapshot m_snapshot;
    VerifyState m_verifyState = VerifyState::Unverified;
    bool m_compact = false;

    QFrame *m_stateRail = nullptr;
    QLabel *m_stateBadge = nullptr;
    QLabel *m_stateTimestamp = nullptr;
    QLabel *m_identityName = nullptr;
    QLabel *m_identityPath = nullptr;

    QLabel *m_countNSlots = nullptr;
    QLabel *m_countDepth = nullptr;
    QLabel *m_countMetadata = nullptr;
    QLabel *m_countHistory = nullptr;

    QPushButton *m_reVerifyButton = nullptr;
    QPushButton *m_copyJsonButton = nullptr;
    QToolButton *m_closeButton = nullptr;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_ARCHIVE_HEALTH_HEADER_H
