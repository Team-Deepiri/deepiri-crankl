#include "widgets/NavigationList.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QSettings>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {

constexpr int DestinationRole = Qt::UserRole + 1;
const char *const kSettingsKey = "nav/currentDestination";

QString destinationLabel(NavDestination d) {
    switch (d) {
    case NavDestination::Home:
        return QObject::tr("Home");
    case NavDestination::Inspect:
        return QObject::tr("Inspect");
    case NavDestination::Compare:
        return QObject::tr("Compare");
    case NavDestination::Pack:
        return QObject::tr("Pack");
    case NavDestination::Optimize:
        return QObject::tr("Optimize");
    case NavDestination::History:
        return QObject::tr("History");
    case NavDestination::Bind:
        return QObject::tr("Bind");
    case NavDestination::Forward:
        return QObject::tr("Forward");
    case NavDestination::Unpack:
        return QObject::tr("Unpack");
    case NavDestination::Pipeline:
        return QObject::tr("Pipeline");
    case NavDestination::AdvancedMathLab:
        return QObject::tr("Advanced Math Lab");
    case NavDestination::Jobs:
        return QObject::tr("Jobs");
    case NavDestination::Settings:
        return QObject::tr("Settings");
    case NavDestination::Help:
        return QObject::tr("Help");
    }
    return {};
}

} // namespace

NavigationList::NavigationList(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("NavigationList"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("NavigationListView"));
    m_list->setFrameShape(QListWidget::NoFrame);
    m_list->setSelectionMode(QListWidget::SingleSelection);
    layout->addWidget(m_list);

    addGroupLabel(tr("WORKSPACE"));
    addDestinationRow(NavDestination::Home, destinationLabel(NavDestination::Home), true);
    addDestinationRow(NavDestination::Inspect, destinationLabel(NavDestination::Inspect), true);
    addDestinationRow(NavDestination::Compare, destinationLabel(NavDestination::Compare), false);

    addGroupLabel(tr("PROCESS"));
    addDestinationRow(NavDestination::Pack, destinationLabel(NavDestination::Pack), false);
    addDestinationRow(NavDestination::Optimize, destinationLabel(NavDestination::Optimize), false);
    addDestinationRow(NavDestination::History, destinationLabel(NavDestination::History), false);
    addDestinationRow(NavDestination::Bind, destinationLabel(NavDestination::Bind), false);
    addDestinationRow(NavDestination::Forward, destinationLabel(NavDestination::Forward), false);
    addDestinationRow(NavDestination::Unpack, destinationLabel(NavDestination::Unpack), false);
    addDestinationRow(NavDestination::Pipeline, destinationLabel(NavDestination::Pipeline), false);

    addGroupLabel(tr("DEVELOPER"));
    addDestinationRow(NavDestination::AdvancedMathLab,
                       destinationLabel(NavDestination::AdvancedMathLab), false);
    addDestinationRow(NavDestination::Jobs, destinationLabel(NavDestination::Jobs) + QStringLiteral(" · 0"),
                       true);
    m_jobsItem = m_list->item(m_list->count() - 1);
    addDestinationRow(NavDestination::Settings, destinationLabel(NavDestination::Settings), true);
    addDestinationRow(NavDestination::Help, destinationLabel(NavDestination::Help), true);

    // currentItemChanged (not itemClicked) so programmatic selection via
    // setCurrentDestination() -- e.g. Home's "Inspect details" button --
    // activates the page exactly like a real click does.
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item, QListWidgetItem *) {
                if (!item || !(item->flags() & Qt::ItemIsEnabled))
                    return;
                const auto destination =
                    static_cast<NavDestination>(item->data(DestinationRole).toInt());
                QSettings().setValue(QString::fromLatin1(kSettingsKey),
                                     static_cast<int>(destination));
                Q_EMIT destinationActivated(destination);
            });

    restorePersistedDestination();
}

void NavigationList::addGroupLabel(const QString &text) {
    auto *item = new QListWidgetItem(text);
    item->setFlags(Qt::NoItemFlags);
    QFont groupFont(QStringLiteral("IBM Plex Mono"));
    groupFont.setStyleHint(QFont::Monospace);
    groupFont.setPointSizeF(8.0);
    groupFont.setWeight(QFont::DemiBold);
    groupFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
    item->setFont(groupFont);
    item->setForeground(QColor(0x5d, 0x66, 0x6c));
    m_list->addItem(item);
}

void NavigationList::addDestinationRow(NavDestination destination, const QString &label, bool enabled) {
    auto *item = new QListWidgetItem(label);
    item->setData(DestinationRole, static_cast<int>(destination));
    item->setFlags(enabled ? (Qt::ItemIsSelectable | Qt::ItemIsEnabled) : Qt::NoItemFlags);
    if (!enabled)
        item->setToolTip(tr("Not available yet."));
    m_list->addItem(item);
}

void NavigationList::setJobsCount(int count) {
    if (m_jobsItem)
        m_jobsItem->setText(destinationLabel(NavDestination::Jobs) +
                             QStringLiteral(" · %1").arg(count));
}

NavDestination NavigationList::currentDestination() const {
    if (auto *item = m_list->currentItem())
        return static_cast<NavDestination>(item->data(DestinationRole).toInt());
    return NavDestination::Home;
}

void NavigationList::setCurrentDestination(NavDestination destination) {
    for (int i = 0; i < m_list->count(); ++i) {
        auto *item = m_list->item(i);
        if ((item->flags() & Qt::ItemIsSelectable) &&
            static_cast<NavDestination>(item->data(DestinationRole).toInt()) == destination) {
            m_list->setCurrentItem(item);
            return;
        }
    }
}

void NavigationList::restorePersistedDestination() {
    const QSettings settings;
    const auto saved = static_cast<NavDestination>(
        settings.value(QString::fromLatin1(kSettingsKey), static_cast<int>(NavDestination::Home))
            .toInt());
    setCurrentDestination(saved);
}

} // namespace crankl_gui
