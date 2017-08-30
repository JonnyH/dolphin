// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/FIFOWindow.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlaybackAnalyzer.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"

#include "DolphinQt2/QtUtils/QueueOnObject.h"
#include "DolphinQt2/Settings.h"

FIFOWindow::FIFOWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("FIFO Player"));

  CreateWidgets();
  ConnectWidgets();

  UpdateInfo();

  connect(this, &FIFOWindow::EmulationStarted, this, &FIFOWindow::OnEmulationStarted);
  connect(this, &FIFOWindow::EmulationStopped, this, &FIFOWindow::OnEmulationStopped);
  connect(this, &FIFOWindow::EmulationStopped, this, &FIFOWindow::UpdateInfo);

  connect(&Settings::Instance(), &Settings::FIFORecordingDone, this, &FIFOWindow::OnRecordingDone);
  connect(&Settings::Instance(), &Settings::FIFOLogLoaded, this, &FIFOWindow::OnFIFOLoaded);
  connect(&Settings::Instance(), &Settings::FIFOFrameWritten, this, &FIFOWindow::UpdateInfo);

  FifoPlayer::GetInstance().SetFileLoadedCallback([] {
    QueueOnObject(&Settings::Instance(), [] { Settings::Instance().TriggerFIFOLogLoaded(); });
  });
  FifoPlayer::GetInstance().SetFrameWrittenCallback([] {
    QueueOnObject(&Settings::Instance(), [] { Settings::Instance().TriggerFIFOFrameWritten(); });
  });
}

FIFOWindow::~FIFOWindow()
{
  FifoPlayer::GetInstance().SetFileLoadedCallback(nullptr);
  FifoPlayer::GetInstance().SetFrameWrittenCallback(nullptr);
}

void FIFOWindow::CreateWidgets()
{
  auto* layout = new QGridLayout;

  // Info
  auto* info_group = new QGroupBox(tr("File Info"));
  auto* info_layout = new QHBoxLayout;

  m_info_label = new QLabel;
  info_layout->addWidget(m_info_label);
  info_group->setLayout(info_layout);

  info_group->setMinimumHeight(100);

  // Object Range
  auto* object_range_group = new QGroupBox(tr("Object Range"));
  auto* object_range_layout = new QHBoxLayout;

  m_object_range_from = new QSpinBox;
  m_object_range_to = new QSpinBox;

  m_object_range_from->setMinimum(0);
  m_object_range_from->setMaximum(99999);
  m_object_range_to->setMaximum(99999);
  m_object_range_to->setValue(99999);

  object_range_layout->addWidget(new QLabel(tr("From:")));
  object_range_layout->addWidget(m_object_range_from);
  object_range_layout->addWidget(new QLabel(tr("To:")));
  object_range_layout->addWidget(m_object_range_to);
  object_range_group->setLayout(object_range_layout);

  // Frame Range
  auto* frame_range_group = new QGroupBox(tr("Frame Range"));
  auto* frame_range_layout = new QHBoxLayout;

  m_frame_range_from = new QSpinBox;
  m_frame_range_to = new QSpinBox;

  m_frame_range_to->setMinimum(1);
  m_frame_range_from->setMaximum(10);
  m_frame_range_to->setMaximum(10);

  frame_range_layout->addWidget(new QLabel(tr("From:")));
  frame_range_layout->addWidget(m_frame_range_from);
  frame_range_layout->addWidget(new QLabel(tr("To:")));
  frame_range_layout->addWidget(m_frame_range_to);
  frame_range_group->setLayout(frame_range_layout);

  // Playback Options
  auto* playback_group = new QGroupBox(tr("Playback Options"));
  auto* playback_layout = new QGridLayout;
  m_early_memory_updates = new QCheckBox(tr("Early Memory Updates"));

  playback_layout->addWidget(object_range_group, 0, 0);
  playback_layout->addWidget(frame_range_group, 0, 1);
  playback_layout->addWidget(m_early_memory_updates, 1, 0);
  playback_group->setLayout(playback_layout);

  // Recording Options
  auto* recording_group = new QGroupBox(tr("Recording Options"));
  auto* recording_layout = new QHBoxLayout;
  m_frame_record_count = new QSpinBox;

  m_frame_record_count->setMinimum(1);
  m_frame_record_count->setMaximum(3600);
  m_frame_record_count->setValue(3);

  recording_layout->addWidget(new QLabel(tr("Frames to Record:")));
  recording_layout->addWidget(m_frame_record_count);
  recording_group->setLayout(recording_layout);

  // Action Buttons
  m_load = new QPushButton(tr("Load..."));
  m_save = new QPushButton(tr("Save..."));
  m_record = new QPushButton(tr("Record"));

  m_save->setEnabled(false);
  m_record->setEnabled(false);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  layout->addWidget(info_group, 0, 0, 1, -1);
  layout->addWidget(playback_group, 1, 0, 1, -1);
  layout->addWidget(recording_group, 2, 0, 1, -1);
  layout->addWidget(m_load, 3, 0);
  layout->addWidget(m_save, 3, 1);
  layout->addWidget(m_record, 3, 2);
  layout->addWidget(m_button_box, 4, 0, 1, -1);

  setLayout(layout);
}

void FIFOWindow::ConnectWidgets()
{
  connect(m_load, &QPushButton::pressed, this, &FIFOWindow::LoadRecording);
  connect(m_save, &QPushButton::pressed, this, &FIFOWindow::SaveRecording);
  connect(m_record, &QPushButton::pressed, this, &FIFOWindow::ToggleRecording);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &FIFOWindow::reject);
  connect(m_early_memory_updates, &QCheckBox::toggled, this,
          &FIFOWindow::OnEarlyMemoryUpdatesChanged);
  connect(m_frame_range_from, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOWindow::OnFrameFromChanged);
  connect(m_frame_range_to, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOWindow::OnFrameToChanged);

  connect(m_object_range_from, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOWindow::OnObjectFromChanged);
  connect(m_object_range_to, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &FIFOWindow::OnObjectToChanged);
}

void FIFOWindow::LoadRecording()
{
  QString path = QFileDialog::getOpenFileName(this, tr("Open FIFO log"), QString(),
                                              tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  emit LoadFIFO(path);
}

void FIFOWindow::SaveRecording()
{
  QString path = QFileDialog::getSaveFileName(this, tr("Save FIFO log"), QString(),
                                              tr("Dolphin FIFO Log (*.dff)"));

  if (path.isEmpty())
    return;

  FifoDataFile* file = FifoRecorder::GetInstance().GetRecordedFile();

  bool result = file->Save(path.toStdString());

  if (!result)
    QMessageBox::critical(this, tr("Error"), tr("Failed to save FIFO log."));
}

void FIFOWindow::ToggleRecording()
{
  FifoRecorder& recorder = FifoRecorder::GetInstance();

  if (!m_recording)
  {
    // Start recording
    recorder.StartRecording(m_frame_record_count->value(), [] {
      QueueOnObject(&Settings::Instance(), [] { Settings::Instance().TriggerFIFORecordingDone(); });
    });

    m_save->setEnabled(false);
    m_recording = true;
    m_recording_done = false;

    m_record->setText(tr("Stop"));
  }
  else
  {
    recorder.StopRecording();
  }

  UpdateInfo();
}

void FIFOWindow::OnEmulationStarted()
{
  m_record->setEnabled(!m_playing);
  m_load->setEnabled(false);
}

void FIFOWindow::OnEmulationStopped()
{
  m_record->setEnabled(false);
  m_load->setEnabled(true);

  // If we have previously been recording, stop now.
  if (m_recording)
    ToggleRecording();

  if (m_playing)
  {
    m_record->setEnabled(true);
    m_playing = false;
  }
}

void FIFOWindow::OnRecordingDone()
{
  m_recording = false;
  m_recording_done = true;

  UpdateInfo();

  m_save->setEnabled(true);
  m_record->setText(tr("Record"));
}

void FIFOWindow::UpdateInfo()
{
  if (m_playing)
  {
    FifoDataFile* file = FifoPlayer::GetInstance().GetFile();
    m_info_label->setText(
        tr("%1 frame(s)\n%2 object(s)\nCurrent Frame: %3")
            .arg(QString::number(file->GetFrameCount()),
                 QString::number(FifoPlayer::GetInstance().GetFrameObjectCount()),
                 QString::number(FifoPlayer::GetInstance().GetCurrentFrameNum())));
    return;
  }

  if (m_recording)
  {
    m_info_label->setText(tr("Recording..."));
    return;
  }

  if (m_recording_done)
  {
    FifoDataFile* file = FifoRecorder::GetInstance().GetRecordedFile();
    size_t fifo_bytes = 0;
    size_t mem_bytes = 0;

    for (u32 i = 0; i < file->GetFrameCount(); ++i)
    {
      fifo_bytes += file->GetFrame(i).fifoData.size();
      for (const auto& mem_update : file->GetFrame(i).memoryUpdates)
        mem_bytes += mem_update.data.size();
    }

    m_info_label->setText(tr("%1 FIFO bytes\n%2 memory bytes\n%3 frames")
                              .arg(QString::number(fifo_bytes), QString::number(mem_bytes),
                                   QString::number(file->GetFrameCount())));
    return;
  }

  m_info_label->setText(tr("No file loaded / recorded."));
}

void FIFOWindow::OnFIFOLoaded()
{
  m_playing = true;
  m_recording_done = false;

  FifoDataFile* file = FifoPlayer::GetInstance().GetFile();

  auto object_count = FifoPlayer::GetInstance().GetFrameObjectCount();
  auto frame_count = file->GetFrameCount();

  m_frame_range_to->setMaximum(frame_count);
  m_frame_range_to->setValue(frame_count);
  m_object_range_to->setMaximum(FifoPlayer::GetInstance().GetFrameObjectCount());
  m_object_range_to->setValue(object_count);

  // We can't record when we are currently playing back a FIFO Log
  m_record->setEnabled(false);

  UpdateInfo();
}

void FIFOWindow::OnFrameFromChanged(int value)
{
  FifoPlayer& player = FifoPlayer::GetInstance();

  player.SetFrameRangeStart(value);

  m_frame_range_from->setValue(player.GetFrameRangeStart());
  m_frame_range_to->setValue(player.GetFrameRangeEnd());
}

void FIFOWindow::OnFrameToChanged(int value)
{
  FifoPlayer& player = FifoPlayer::GetInstance();
  player.SetFrameRangeEnd(value);

  m_frame_range_from->setValue(player.GetFrameRangeStart());
  m_frame_range_to->setValue(player.GetFrameRangeEnd());
}

void FIFOWindow::OnObjectFromChanged(int value)
{
  FifoPlayer::GetInstance().SetObjectRangeStart(value);
}

void FIFOWindow::OnObjectToChanged(int value)
{
  FifoPlayer::GetInstance().SetObjectRangeEnd(value);
}

void FIFOWindow::OnEarlyMemoryUpdatesChanged(bool enabled)
{
  FifoPlayer::GetInstance().SetEarlyMemoryUpdates(enabled);
}
