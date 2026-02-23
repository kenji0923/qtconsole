#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QDoubleSpinBox;
class QSettings;
class QToolButton;
class QWidget;
class MeasurementModel;

class NumericRatioWidget : public QWidget {
  Q_OBJECT
 public:
  explicit NumericRatioWidget(MeasurementModel* model, QWidget* parent = nullptr);
  void loadSettings(QSettings* settings);
  void saveSettings(QSettings* settings) const;

 private slots:
  void onSampleUpdated(double raw_value, double processed_value, double averaged_value,
                       double ratio, qint64 timestamp_ms);
  void onReferenceControlsToggled(bool enabled);
  void onReferenceMinChanged(double value);
  void onReferenceMaxChanged(double value);

 private:
  MeasurementModel* model_;
  QLabel* value_label_;
  QProgressBar* ratio_bar_;
  QToolButton* reference_toggle_button_;
  QWidget* reference_controls_widget_;
  QDoubleSpinBox* min_spin_;
  QDoubleSpinBox* max_spin_;
};
