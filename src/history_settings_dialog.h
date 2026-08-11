#pragma once

#include <QDialog>

#include "history_paths.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QSpinBox;
QT_END_NAMESPACE

class HistoryManager;

class HistorySettingsDialog : public QDialog {
  Q_OBJECT
 public:
  HistorySettingsDialog(const QString& measurement_id, const HistoryConfig& config,
                        const HistoryManager* history, QWidget* parent = nullptr);

  HistoryConfig historyConfig() const;

 private slots:
  void browseForDirectory();
  void openDirectory();

 private:
  QLineEdit* archive_root_edit_;
  QSpinBox* flush_interval_spin_;
  QSpinBox* max_samples_spin_;
};
