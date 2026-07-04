#include "receiver_control_widget.h"

#include <QComboBox>
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
      equation_edit_(new QLineEdit(this)),
      format_edit_(new QLineEdit(this)),
      averaging_window_spin_(new QSpinBox(this)),
      start_button_(new QPushButton("Start", this)),
      stop_button_(new QPushButton("Stop", this)),
      status_label_(new QLabel("Stopped", this)) {
  mode_combo_->addItem("UDP", static_cast<int>(DataReceiver::Mode::Udp));
  mode_combo_->addItem("WebSocket", static_cast<int>(DataReceiver::Mode::WebSocket));

  port_spin_->setRange(1, 65535);
  port_spin_->setValue(9000);

  title_edit_->setPlaceholderText("Measurement title");

  equation_edit_->setText(model_->equation());
  equation_edit_->setPlaceholderText("x  (e.g. sqrt(x)*2 + sin(x))");
  equation_edit_->setToolTip(
      "Transform applied to each raw sample. Variable x is the raw value.\n"
      "Arithmetic and functions: exp, sin, cos, tan, sqrt, pow, log, abs, ...");

  format_edit_->setText(model_->format());
  format_edit_->setPlaceholderText("%.3f");
  format_edit_->setToolTip(
      "printf-style display format with one float conversion (e/E/f/F/g/G),\n"
      "e.g. \"%.3f\", \"%8.2e\", \"%.1f V\".");

  averaging_window_spin_->setRange(1, 100000);
  averaging_window_spin_->setValue(model_->averagingWindowLength());

  auto* form = new QFormLayout;
  form->addRow("Protocol", mode_combo_);
  form->addRow("Port", port_spin_);
  form->addRow("Title", title_edit_);
  form->addRow("Equation f(x)", equation_edit_);
  form->addRow("Format", format_edit_);
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
  connect(equation_edit_, &QLineEdit::textChanged, this,
          &ReceiverControlWidget::onEquationChanged);
  connect(format_edit_, &QLineEdit::textChanged, this, &ReceiverControlWidget::onFormatChanged);
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

  QString equation = settings->value("equation").toString();
  if (equation.isEmpty() &&
      (settings->contains("offset") || settings->contains("scaleFactor"))) {
    // Migrate legacy linear transform (scale*x + offset) to an equation.
    const double scale = settings->value("scaleFactor", 1.0).toDouble();
    const double offset = settings->value("offset", 0.0).toDouble();
    equation = QStringLiteral("%1*x + %2").arg(scale).arg(offset);
  }
  if (equation.isEmpty()) {
    equation = QStringLiteral("x");
  }
  equation_edit_->setText(equation);
  format_edit_->setText(settings->value("format", "%.3f").toString());

  averaging_window_spin_->setValue(settings->value("averagingWindowLength", 1).toInt());

  settings->endGroup();
}

void ReceiverControlWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("receiver_control");
  settings->setValue("mode", mode_combo_->currentData().toInt());
  settings->setValue("port", port_spin_->value());
  settings->setValue("measurementTitle", title_edit_->text());
  settings->setValue("equation", equation_edit_->text());
  settings->setValue("format", format_edit_->text());
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

namespace {
const char* kInvalidStyle = "QLineEdit { background-color: #5a1e1e; color: #ffd5d5; }";
}  // namespace

void ReceiverControlWidget::onEquationChanged(const QString& text) {
  model_->setEquation(text);
  const bool ok = model_->equationValid();
  equation_edit_->setStyleSheet(ok ? QString() : kInvalidStyle);
  equation_edit_->setToolTip(ok ? QString("Variable x is the raw value.")
                                : QString("Invalid equation: %1").arg(model_->equationError()));
}

void ReceiverControlWidget::onFormatChanged(const QString& text) {
  model_->setFormat(text);
  const bool ok = model_->formatValid();
  format_edit_->setStyleSheet(ok ? QString() : kInvalidStyle);
  format_edit_->setToolTip(ok ? QString("printf-style format, e.g. %.3f")
                              : QString("Invalid format: use one float conversion, e.g. %.3f"));
}
