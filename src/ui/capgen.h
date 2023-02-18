#pragma once

// Included in the top so that it includes libTorch first.
#include "core/transcribe.h"
#include <wx/wxprec.h>

// For the platforms without support for precompiled headers.
#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif


namespace capgen {


class Application : public wxApp {
public:
  virtual bool OnInit();
};


class TranscriptionUpdateEvent : public wxThreadEvent {
private:
  int m_progress;

public:
  TranscriptionUpdateEvent(wxEventType eventType, int id, int progress)
    : wxThreadEvent(eventType, id), m_progress(progress)
  {
  }

  int get_progress() const {
    return m_progress;
  }

  virtual wxEvent *Clone() const
  {
    return new TranscriptionUpdateEvent(*this);
  }
};


class TranscriptionWidget : public wxPanel {
public:
  wxStaticText *m_progbar_text;
  wxGauge *m_progbar;
  wxStaticText *m_status_text;

  TranscriptionWidget(wxScrolledWindow *parent_window, const wxString &path);
  void update_progbar(int value);
  void transcription_complete();
  void on_transcription_thread_update(TranscriptionUpdateEvent& event);
  void on_transcription_thread_completion(wxThreadEvent& event);
};


class MainWindow : public wxFrame {
public:
  wxScrolledWindow *m_content_window;
  wxSizer *m_content_sizer = nullptr;
  capgen::Whisper *m_whisper_ptr = nullptr;

  MainWindow();
  TranscriptionWidget *add_transcription_widget(wxString &path);
  void transcribe(TranscriptionWidget *widget, wxString &path);

private:
  void on_audio_add(wxCommandEvent& event);
  void on_video_add(wxCommandEvent& event);
  void on_exit(wxCommandEvent& event);
  void on_about(wxCommandEvent& event);
  void on_close(wxCloseEvent& event);
};


class TranscriptionThread : public wxThread {
public:
  std::string m_path;
  capgen::Whisper *m_whisper_ptr;
  TranscriptionWidget *m_widget;

  TranscriptionThread(std::string &path, capgen::Whisper *whisper_ptr, TranscriptionWidget *widget);
  ~TranscriptionThread();

  virtual void *Entry();
};

} // namespace capen


