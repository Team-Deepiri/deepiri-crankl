#include "widgets/MetricsPanel.h"

#include "widgets/StyleUtil.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {

struct RowSpec {
    const char *key;
    const char *tooltip; // 5b texts, verbatim
};

// Fixed order matching crankl_archive_metrics_t field order.
const RowSpec kRows[] = {
    {"n_slots", QT_TRANSLATE_NOOP("MetricsPanel",
                                   "Number of crank words in the archive. One slot holds one "
                                   "64-bit word, decoding to an 8×8 block.")},
    {"depth_min", QT_TRANSLATE_NOOP("MetricsPanel",
                                     "Lowest encoding depth across all slots.")},
    {"depth_max", QT_TRANSLATE_NOOP("MetricsPanel",
                                     "Highest encoding depth across all slots.")},
    {"scalar_mean", QT_TRANSLATE_NOOP("MetricsPanel",
                                       "Mean of all decoded scalar values across the archive.")},
    {"scalar_abs_mean",
     QT_TRANSLATE_NOOP("MetricsPanel",
                       "Mean absolute magnitude — insensitive to sign cancellation.")},
    {"trit_density",
     QT_TRANSLATE_NOOP("MetricsPanel", "Fraction of non-zero trits across all crank words.")},
    {"trit_entropy",
     QT_TRANSLATE_NOOP("MetricsPanel", "Shannon entropy of the trit distribution, in bits.")},
    {"clifford_energy",
     QT_TRANSLATE_NOOP("MetricsPanel", "Summed squared multivector norm over every slot.")},
    {"beta1_proxy",
     QT_TRANSLATE_NOOP("MetricsPanel", "A proxy for first-order cycle structure — not a computed "
                                        "Betti number. Do not read as beta1.")},
};

} // namespace

MetricsPanel::MetricsPanel(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("MetricsPanel"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    for (const RowSpec &spec : kRows) {
        auto *row = makeCard(QStringLiteral("MetricRow"), this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 7, 0, 7);
        rowLayout->setSpacing(8);

        const QString tooltip = tr(spec.tooltip);
        auto *key = new QLabel(QString::fromLatin1(spec.key), row);
        key->setObjectName(QStringLiteral("MetricKey"));
        key->setToolTip(tooltip);
        auto *value = new QLabel(QStringLiteral("—"), row);
        value->setObjectName(QStringLiteral("MetricValue"));
        value->setToolTip(tooltip);
        // Fix the value height so skeleton/em-dash rows match loaded rows
        // exactly -- no reflow when values land (5a).
        value->setFixedHeight(14);
        key->setFixedHeight(14);

        rowLayout->addWidget(key);
        rowLayout->addStretch(1);
        rowLayout->addWidget(value);
        layout->addWidget(row);
        m_rows.push_back({key, value});
    }

    m_proxyCallout =
        new QLabel(tr("beta1_proxy is a proxy, not a computed Betti number."), this);
    m_proxyCallout->setObjectName(QStringLiteral("ProxyCallout"));
    m_proxyCallout->setWordWrap(true);
    layout->addSpacing(10);
    layout->addWidget(m_proxyCallout);

    m_emptyNote = new QLabel(
        tr("These nine fields are what inspect reports — shown here so you know what you'll "
           "get before opening a file. Values appear as soon as an archive is read."),
        this);
    m_emptyNote->setObjectName(QStringLiteral("MetricsEmptyNote"));
    m_emptyNote->setWordWrap(true);
    layout->addWidget(m_emptyNote);

    applyState(State::Empty);
}

void MetricsPanel::setMetrics(const ArchiveMetrics &metrics) {
    const QStringList values = {
        QString::number(metrics.nSlots),
        QString::number(metrics.depthMin),
        QString::number(metrics.depthMax),
        QString::number(metrics.scalarMean, 'f', 6),
        QString::number(metrics.scalarAbsMean, 'f', 6),
        QString::number(metrics.tritDensity, 'f', 4),
        QString::number(metrics.tritEntropy, 'f', 4),
        QString::number(metrics.cliffordEnergy, 'e', 2),
        QString::number(metrics.beta1Proxy, 'f', 4),
    };
    for (int i = 0; i < m_rows.size(); ++i)
        m_rows[i].value->setText(values[i]);
    applyState(State::Loaded);
}

void MetricsPanel::setPending() {
    for (const Row &row : m_rows)
        row.value->setText(QString());
    applyState(State::Pending);
}

void MetricsPanel::clear() {
    for (const Row &row : m_rows)
        row.value->setText(QStringLiteral("—"));
    applyState(State::Empty);
}

void MetricsPanel::applyState(State state) {
    const QString stateName = state == State::Loaded    ? QStringLiteral("loaded")
                               : state == State::Pending ? QStringLiteral("pending")
                                                          : QStringLiteral("empty");
    for (int i = 0; i < m_rows.size(); ++i) {
        setStyleProperty(m_rows[i].key, "state", stateName);
        setStyleProperty(m_rows[i].value, "state", stateName);
        // beta1_proxy's key renders amber when loaded (3a/5b).
        if (i == m_rows.size() - 1)
            setStyleProperty(m_rows[i].key, "amber", state == State::Loaded);
    }
    m_proxyCallout->setVisible(state == State::Loaded);
    m_emptyNote->setVisible(state == State::Empty);
}

} // namespace crankl_gui
