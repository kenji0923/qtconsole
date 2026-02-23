#include "numeric_ratio_widget.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

#include "measurement_model.h"

NumericRatioWidget::NumericRatioWidget(MeasurementModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      value_label_(new QLabel("0.000", this)),
      ratio_bar_(new QProgressBar(this)),
      max_spin_(new QDoubleSpinBox(this)) {
  value_label_->setAlignment(Qt::AlignCenter);
  value_label_->setStyleSheet("font-size: 56px; font-weight: 700;");

  ratio_bar_->setRange(0, 1000);
  ratio_bar_->setTextVisible(true);
  ratio_bar_->setFormat("%p%");
  ratio_bar_->setStyleSheet(
      "QProgressBar { border: 1px solid #777; border-radius: 4px; text-align: center; }"
      "QProgressBar::chunk { background-color: #2E8B57; }");

  max_spin_->setRange(0.0001, 1e9);
  max_spin_->setDecimals(4);
  max_spin_->setValue(model_->reference_max());

  auto* form = new QFormLayout;
  form->addRow("Reference max", max_spin_);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(value_label_, 1);
  layout->addWidget(ratio_bar_);
  layout->addLayout(form);

  connect(model_, &MeasurementModel::sample_updated, this, &NumericRatioWidget::on_sample_updated);
  connect(max_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &NumericRatioWidget::on_reference_max_changed);
}

void NumericRatioWidget::on_sample_updated(double value, double ratio, qint64) {
  value_label_->setText(QString::number(value, 'f', 3));
  ratio_bar_->setValue(static_cast<int>(ratio * 1000.0));
}

void NumericRatioWidget::on_reference_max_changed(double value) {
  model_->set_reference_max(value);
}
