#pragma once

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSettings;
class QSpinBox;
class DataReceiver;
class MeasurementModel;
class QString;

class ReceiverControlWidget : public QWidget {
  Q_OBJECT
 public:
  explicit ReceiverControlWidget(DataReceiver* receiver, MeasurementModel* model,
                                 QWidget* parent = nullptr);

  void loadSettings(QSettings* settings);
  void saveSettings(QSettings* settings) const;
  void startReceiving();
  void setProtocol(const QString& protocol);
  void setPort(int port);
  void setMeasurementTitle(const QString& measurementTitle);
  QString receiverProtocolAbbrev() const;
  int port() const;
  QString measurementTitle() const;

 signals:
  void configurationChanged();

 private slots:
  void onStart();
  void onStop();
  void onStatusChanged(bool running, const QString& message);
  void onEquationChanged(const QString& text);
  void onFormatChanged(const QString& text);

 private:
  DataReceiver* receiver_;
  MeasurementModel* model_;

  QComboBox* mode_combo_;
  QSpinBox* port_spin_;
  QLineEdit* title_edit_;
  QLineEdit* equation_edit_;
  QLineEdit* format_edit_;
  QSpinBox* averaging_window_spin_;
  QPushButton* start_button_;
  QPushButton* stop_button_;
  QLabel* status_label_;
};
