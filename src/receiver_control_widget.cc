#include "receiver_control_widget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include "data_receiver.h"
#include "measurement_model.h"

ReceiverControlWidget::ReceiverControlWidget(DataReceiver* receiver, MeasurementModel* model,
                                             QWidget* parent)
    : QWidget(parent),
      receiver_(receiver),
      model_(model),
      mode_combo_(new QComboBox(this)),
      port_spin_(new QSpinBox(this)),
      title_edit_(new QLineEdit(this)),
      offset_spin_(new QDoubleSpinBox(this)),
      scale_factor_spin_(new QDoubleSpinBox(this)),
      averaging_window_spin_(new QSpinBox(this)),
      start_button_(new QPushButton("Start", this)),
      stop_button_(new QPushButton("Stop", this)),
      status_label_(new QLabel("Stopped", this)) {
  mode_combo_->addItem("UDP", static_cast<int>(DataReceiver::Mode::Udp));
  mode_combo_->addItem("WebSocket", static_cast<int>(DataReceiver::Mode::WebSocket));

  port_spin_->setRange(1, 65535);
  port_spin_->setValue(9000);

  title_edit_->setPlaceholderText("Measurement title");

  offset_spin_->setRange(-1e9, 1e9);
  offset_spin_->setDecimals(6);
  offset_spin_->setValue(model_->offset());

  scale_factor_spin_->setRange(-1e9, 1e9);
  scale_factor_spin_->setDecimals(6);
  scale_factor_spin_->setValue(model_->scaleFactor());

  averaging_window_spin_->setRange(1, 100000);
  averaging_window_spin_->setValue(model_->averagingWindowLength());

  auto* form = new QFormLayout;
  form->addRow("Protocol", mode_combo_);
  form->addRow("Port", port_spin_);
  form->addRow("Title", title_edit_);
  form->addRow("Offset", offset_spin_);
  form->addRow("Scale factor", scale_factor_spin_);
  form->addRow("Avg window (samples)", averaging_window_spin_);

  auto* controls = new QHBoxLayout;
  controls->addWidget(start_button_);
  controls->addWidget(stop_button_);
  controls->addWidget(status_label_, 1);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addLayout(controls);

  connect(start_button_, &QPushButton::clicked, this, &ReceiverControlWidget::onStart);
  connect(stop_button_, &QPushButton::clicked, this, &ReceiverControlWidget::onStop);
  connect(receiver_, &DataReceiver::statusChanged, this, &ReceiverControlWidget::onStatusChanged);

  connect(title_edit_, &QLineEdit::textChanged, model_, &MeasurementModel::setMeasurementTitle);
  connect(offset_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), model_,
          &MeasurementModel::setOffset);
  connect(scale_factor_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), model_,
          &MeasurementModel::setScaleFactor);
  connect(averaging_window_spin_, qOverload<int>(&QSpinBox::valueChanged), model_,
          &MeasurementModel::setAveragingWindowLength);
  connect(mode_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &ReceiverControlWidget::configurationChanged);
  connect(port_spin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ReceiverControlWidget::configurationChanged);
  connect(title_edit_, &QLineEdit::textChanged, this, &ReceiverControlWidget::configurationChanged);
}

void ReceiverControlWidget::loadSettings(QSettings* settings) {
  settings->beginGroup("receiver_control");

  const int mode_value = settings->value("mode", static_cast<int>(DataReceiver::Mode::Udp)).toInt();
  const int mode_index = mode_combo_->findData(mode_value);
  if (mode_index >= 0) {
    mode_combo_->setCurrentIndex(mode_index);
  }

  port_spin_->setValue(settings->value("port", 9000).toInt());

  title_edit_->setText(settings->value("measurementTitle", "").toString());
  offset_spin_->setValue(settings->value("offset", 0.0).toDouble());
  scale_factor_spin_->setValue(settings->value("scaleFactor", 1.0).toDouble());
  averaging_window_spin_->setValue(settings->value("averagingWindowLength", 1).toInt());

  settings->endGroup();
}

void ReceiverControlWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("receiver_control");
  settings->setValue("mode", mode_combo_->currentData().toInt());
  settings->setValue("port", port_spin_->value());
  settings->setValue("measurementTitle", title_edit_->text());
  settings->setValue("offset", offset_spin_->value());
  settings->setValue("scaleFactor", scale_factor_spin_->value());
  settings->setValue("averagingWindowLength", averaging_window_spin_->value());
  settings->endGroup();
}

void ReceiverControlWidget::startReceiving() {
  const auto mode = static_cast<DataReceiver::Mode>(mode_combo_->currentData().toInt());
  receiver_->start(mode, static_cast<quint16>(port_spin_->value()));
}

void ReceiverControlWidget::setProtocol(const QString& protocol) {
  const QString normalized = protocol.trimmed().toUpper();
  int mode_value = static_cast<int>(DataReceiver::Mode::Udp);
  if (normalized == "WS" || normalized == "WEBSOCKET") {
    mode_value = static_cast<int>(DataReceiver::Mode::WebSocket);
  }
  const int mode_index = mode_combo_->findData(mode_value);
  if (mode_index >= 0) {
    mode_combo_->setCurrentIndex(mode_index);
  }
}

void ReceiverControlWidget::setPort(int port) { port_spin_->setValue(port); }

void ReceiverControlWidget::setMeasurementTitle(const QString& measurementTitle) {
  title_edit_->setText(measurementTitle);
}

QString ReceiverControlWidget::receiverProtocolAbbrev() const {
  const auto mode = static_cast<DataReceiver::Mode>(mode_combo_->currentData().toInt());
  return mode == DataReceiver::Mode::Udp ? "UDP" : "WS";
}

int ReceiverControlWidget::port() const { return port_spin_->value(); }

QString ReceiverControlWidget::measurementTitle() const { return title_edit_->text().trimmed(); }

void ReceiverControlWidget::onStart() { startReceiving(); }

void ReceiverControlWidget::onStop() { receiver_->stop(); }

void ReceiverControlWidget::onStatusChanged(bool, const QString& message) {
  status_label_->setText(message);
}
