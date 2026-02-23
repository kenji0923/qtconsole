#include "receiver_control_widget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "data_receiver.h"

ReceiverControlWidget::ReceiverControlWidget(DataReceiver* receiver, QWidget* parent)
    : QWidget(parent),
      receiver_(receiver),
      mode_combo_(new QComboBox(this)),
      port_spin_(new QSpinBox(this)),
      start_button_(new QPushButton("Start", this)),
      stop_button_(new QPushButton("Stop", this)),
      status_label_(new QLabel("Stopped", this)) {
  mode_combo_->addItem("UDP", static_cast<int>(DataReceiver::Mode::Udp));
  mode_combo_->addItem("WebSocket", static_cast<int>(DataReceiver::Mode::WebSocket));

  port_spin_->setRange(1, 65535);
  port_spin_->setValue(9000);

  auto* form = new QFormLayout;
  form->addRow("Protocol", mode_combo_);
  form->addRow("Port", port_spin_);

  auto* controls = new QHBoxLayout;
  controls->addWidget(start_button_);
  controls->addWidget(stop_button_);
  controls->addWidget(status_label_, 1);

  auto* layout = new QHBoxLayout(this);
  layout->addLayout(form);
  layout->addLayout(controls, 1);

  connect(start_button_, &QPushButton::clicked, this, &ReceiverControlWidget::on_start);
  connect(stop_button_, &QPushButton::clicked, this, &ReceiverControlWidget::on_stop);
  connect(receiver_, &DataReceiver::status_changed, this,
          &ReceiverControlWidget::on_status_changed);
}

void ReceiverControlWidget::on_start() {
  const auto mode = static_cast<DataReceiver::Mode>(mode_combo_->currentData().toInt());
  receiver_->start(mode, static_cast<quint16>(port_spin_->value()));
}

void ReceiverControlWidget::on_stop() { receiver_->stop(); }

void ReceiverControlWidget::on_status_changed(bool, const QString& message) {
  status_label_->setText(message);
}
