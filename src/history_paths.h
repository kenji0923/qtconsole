#pragma once

#include <QDate>
#include <QString>

struct HistoryConfig {
  QString archive_root;
  int flush_interval_sec = 30;
  int max_samples = 500000;
};

QString defaultHistoryRoot();
QString historyProfileKey(const QString& measurement_id);
QString sanitizeHistoryComponent(const QString& text);
QString historyDirectoryPath(const QString& archive_root, const QString& measurement_id,
                             const QDate& date);
QString historyFilePath(const QString& archive_root, const QString& measurement_id,
                        const QDate& date);
