#pragma once

#include <QWidget>

#include "udp_relay.h"

class QCheckBox;
class QLabel;
class QSettings;
class QTableWidget;

class RelayWidget : public QWidget {
  Q_OBJECT
 public:
  explicit RelayWidget(UdpRelay* relay, QWidget* parent = nullptr);
  void loadSettings(QSettings* settings);
  void saveSettings(QSettings* settings) const;

 private:
  void addDestination(const UdpRelay::Destination& destination);
  void applyDestinations();
  QVector<UdpRelay::Destination> destinations() const;

  UdpRelay* relay_;
  QCheckBox* enabled_;
  QTableWidget* table_;
  QLabel* status_;
};
