// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSpinBox;

class FIFOWindow : public QDialog
{
  Q_OBJECT
public:
  explicit FIFOWindow(QWidget* parent = nullptr);
  ~FIFOWindow();

signals:
  void EmulationStarted();
  void EmulationStopped();
  void LoadFIFO(const QString& path);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadRecording();
  void SaveRecording();
  void ToggleRecording();

  void OnEmulationStarted();
  void OnEmulationStopped();
  void OnFrameFromChanged(int value);
  void OnFrameToChanged(int value);
  void OnObjectFromChanged(int value);
  void OnObjectToChanged(int value);
  void OnEarlyMemoryUpdatesChanged(bool enabled);
  void OnRecordingDone();
  void OnFIFOLoaded();

  void UpdateInfo();

  QLabel* m_info_label;
  QPushButton* m_load;
  QPushButton* m_save;
  QPushButton* m_record;
  QSpinBox* m_frame_range_from;
  QSpinBox* m_frame_range_to;
  QSpinBox* m_frame_record_count;
  QSpinBox* m_object_range_from;
  QSpinBox* m_object_range_to;
  QCheckBox* m_early_memory_updates;
  QDialogButtonBox* m_button_box;

  bool m_recording = false;
  bool m_recording_done = false;
  bool m_playing = false;
};
