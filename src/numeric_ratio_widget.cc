#include "numeric_ratio_widget.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSettings>
#include <QVBoxLayout>

#include "measurement_model.h"

NumericRatioWidget::NumericRatioWidget(MeasurementModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      value_label_(new QLabel("0.000", this)),
      ratio_bar_(new QProgressBar(this)),
      min_spin_(new QDoubleSpinBox(this)),
      max_spin_(new QDoubleSpinBox(this)) {
  value_label_->setAlignment(Qt::AlignCenter);
  value_label_->setStyleSheet("font-size: 56px; font-weight: 700;");

  ratio_bar_->setRange(0, 1000);
  ratio_bar_->setTextVisible(true);
  ratio_bar_->setFormat("%p%");
  ratio_bar_->setStyleSheet(
      "QProgressBar { border: 1px solid #777; border-radius: 4px; text-align: center; }"
      "QProgressBar::chunk { background-color: #2E8B57; }");

  min_spin_->setRange(-1e9, 1e9);
  min_spin_->setDecimals(4);
  min_spin_->setValue(model_->referenceMin());

  max_spin_->setRange(0.0001, 1e9);
  max_spin_->setDecimals(4);
  max_spin_->setValue(model_->referenceMax());

  auto* form = new QFormLayout;
  form->addRow("Reference min", min_spin_);
  form->addRow("Reference max", max_spin_);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(value_label_, 1);
  layout->addWidget(ratio_bar_);
  layout->addLayout(form);

  connect(model_, &MeasurementModel::sampleUpdated, this, &NumericRatioWidget::onSampleUpdated);
  connect(min_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &NumericRatioWidget::onReferenceMinChanged);
  connect(max_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &NumericRatioWidget::onReferenceMaxChanged);
}

void NumericRatioWidget::loadSettings(QSettings* settings) {
  settings->beginGroup("numeric_ratio");
  min_spin_->setValue(settings->value("reference_min", model_->referenceMin()).toDouble());
  max_spin_->setValue(settings->value("reference_max", model_->referenceMax()).toDouble());
  settings->endGroup();
}

void NumericRatioWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("numeric_ratio");
  settings->setValue("reference_min", min_spin_->value());
  settings->setValue("reference_max", max_spin_->value());
  settings->endGroup();
}

void NumericRatioWidget::onSampleUpdated(double, double, double averaged_value, double ratio,
                                         qint64) {
  value_label_->setText(QString::number(averaged_value, 'f', 3));
  ratio_bar_->setValue(static_cast<int>(ratio * 1000.0));
}

void NumericRatioWidget::onReferenceMaxChanged(double value) {
  model_->setReferenceMax(value);
  if (model_->referenceMax() != value) {
    max_spin_->setValue(model_->referenceMax());
  }
}

void NumericRatioWidget::onReferenceMinChanged(double value) {
  model_->setReferenceMin(value);
  if (model_->referenceMin() != value) {
    min_spin_->setValue(model_->referenceMin());
  }
}
