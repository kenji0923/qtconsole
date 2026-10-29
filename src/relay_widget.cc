#include "relay_widget.h"

#include <algorithm>
#include <functional>

#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

RelayWidget::RelayWidget(UdpRelay* relay, QWidget* parent)
    : QWidget(parent),
      relay_(relay),
      enabled_(new QCheckBox("Enable relay", this)),
      table_(new QTableWidget(0, 4, this)),
      status_(new QLabel("Add destinations, then click Apply.", this)) {
  table_->setHorizontalHeaderLabels({"Enabled", "Host", "Port", "Status"});
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  auto* add = new QPushButton("Add", this);
  auto* remove = new QPushButton("Remove selected", this);
  auto* apply = new QPushButton("Apply", this);
  auto* buttons = new QHBoxLayout;
  buttons->addWidget(enabled_);
  buttons->addStretch();
  buttons->addWidget(add);
  buttons->addWidget(remove);
  buttons->addWidget(apply);
  auto* layout = new QVBoxLayout(this);
  auto* description = new QLabel(
      "Forward incoming User Datagram Protocol (UDP) datagrams unchanged to every enabled "
      "destination. Host names and address literals are supported.", this);
  description->setWordWrap(true);
  layout->addWidget(description);
  layout->addWidget(table_);
  layout->addLayout(buttons);
  layout->addWidget(status_);
  connect(enabled_, &QCheckBox::toggled, relay_, &UdpRelay::setEnabled);
  connect(add, &QPushButton::clicked, this, [this] {
    addDestination({"", 9001, true});
    applyDestinations();
  });
  connect(remove, &QPushButton::clicked, this, [this] {
    const auto rows = table_->selectionModel()->selectedRows();
    QList<int> indices;
    for (const auto& row : rows) {
      indices.append(row.row());
    }
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    for (int row : indices) {
      table_->removeRow(row);
    }
    applyDestinations();
  });
  connect(apply, &QPushButton::clicked, this, &RelayWidget::applyDestinations);
  connect(table_, &QTableWidget::itemChanged, this, [this] {
    status_->setText("Changes pending. Click Apply to update destinations.");
  });
  connect(relay_, &UdpRelay::destinationStatusChanged, this,
          [this](int index, const QString& status) {
    if (index < table_->rowCount()) {
      const QSignalBlocker blocker(table_);
      table_->item(index, 3)->setText(status);
    }
  });
}

void RelayWidget::addDestination(const UdpRelay::Destination& destination) {
  const QSignalBlocker blocker(table_);
  const int row = table_->rowCount();
  table_->insertRow(row);
  auto* enabled = new QTableWidgetItem;
  enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
  enabled->setCheckState(destination.enabled ? Qt::Checked : Qt::Unchecked);
  table_->setItem(row, 0, enabled);
  table_->setItem(row, 1, new QTableWidgetItem(destination.host));
  auto* port = new QSpinBox(table_);
  port->setRange(1, 65535);
  port->setValue(destination.port);
  table_->setCellWidget(row, 2, port);
  connect(port, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
    status_->setText("Changes pending. Click Apply to update destinations.");
  });
  auto* status = new QTableWidgetItem;
  status->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  table_->setItem(row, 3, status);
}

QVector<UdpRelay::Destination> RelayWidget::destinations() const {
  QVector<UdpRelay::Destination> result;
  for (int row = 0; row < table_->rowCount(); ++row) {
    const auto* port = qobject_cast<QSpinBox*>(table_->cellWidget(row, 2));
    result.append({table_->item(row, 1)->text().trimmed(),
                   static_cast<quint16>(port->value()),
                   table_->item(row, 0)->checkState() == Qt::Checked});
  }
  return result;
}

void RelayWidget::applyDestinations() {
  relay_->setDestinations(destinations());
  status_->setText("Destinations applied. Relay runs while enabled and receiving UDP.");
}

void RelayWidget::loadSettings(QSettings* settings) {
  enabled_->setChecked(false);
  table_->setRowCount(0);
  settings->beginGroup("relay");
  const int count = settings->beginReadArray("destinations");
  for (int i = 0; i < count; ++i) {
    settings->setArrayIndex(i);
    addDestination({settings->value("host").toString(),
                    static_cast<quint16>(qBound(1, settings->value("port", 9001).toInt(), 65535)),
                    settings->value("enabled", true).toBool()});
  }
  settings->endArray();
  applyDestinations();
  enabled_->setChecked(settings->value("enabled", false).toBool());
  settings->endGroup();
}

void RelayWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("relay");
  settings->setValue("enabled", enabled_->isChecked());
  const auto targets = destinations();
  settings->beginWriteArray("destinations", targets.size());
  for (int i = 0; i < targets.size(); ++i) {
    settings->setArrayIndex(i);
    settings->setValue("host", targets[i].host);
    settings->setValue("port", targets[i].port);
    settings->setValue("enabled", targets[i].enabled);
  }
  settings->endArray();
  settings->endGroup();
}
