#pragma once

// Included in the top so that it includes libTorch first.
#include "core/transcribe.h"
#include "utils.h"
// #include <wx/wxprec.h>

// For the platforms without support for precompiled headers.
#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif

#include <memory>
#include <vector>
#include <filesystem>


namespace capgen {

class Application : public wxApp {
public:
  virtual bool OnInit();

  ModelsManager m_models_manager = ModelsManager();
};


// IDs for app widgets.
enum {
  ID_audio_add = 1,
  ID_video_add,
  ID_transcribe_btn,
  ID_timer,
  ID_model_selector,
  ID_model_dl_btn,
  ID_model_dl_close_btn
};


class MainWindow : public wxFrame {
public:
  wxScrolledWindow *m_content_window;
  wxSizer *m_content_sizer = nullptr;

  MainWindow();
  void add_trx_widget(wxString &path);

private:
  void on_about(wxCommandEvent &evt);
  void on_audio_add(wxCommandEvent &evt);
  void on_exit(wxCommandEvent &evt);
  void on_video_add(wxCommandEvent &evt);
};


class TranscriptionUpdateEvent : public wxThreadEvent {
public:
  TranscriptionUpdateEvent(wxEventType eventType, int id, int progress)
    : wxThreadEvent(eventType, id), m_progress(progress)
  {}

  int get_progress() const { return m_progress; }

  virtual wxEvent *Clone() const { return new TranscriptionUpdateEvent(*this); }

private:
  int m_progress;
};


class TranscriptionWidget : public wxPanel {
public:
  Application& m_app;
  MainWindow *m_main_window;
  wxStaticText *m_progbar_text;
  wxGauge *m_progbar;
  wxStaticText *m_status_text;
  wxChoice *m_model_choices;
  wxChoice *m_task_choices;
  std::string m_audio_filepath;
  // Timer allows us to display indeterminate progress bar when loading the model. 
  wxTimer m_timer;

  TranscriptionWidget(MainWindow *main_window, wxScrolledWindow *parent_window, const wxString &path);
  void on_model_choice_update(wxCommandEvent &evt);
  void on_timer_update(wxTimerEvent &evt);
  void on_trx_btn_click(wxCommandEvent &evt);
  void on_trx_model_load(wxThreadEvent &evt);
  void on_trx_thread_completion(wxThreadEvent& evt);
  void on_trx_thread_start(wxThreadEvent& evt);
  void on_trx_thread_update(TranscriptionUpdateEvent& evt);
};


class TranscriptionThread : public wxThread {
public:
  std::string m_audio_path;
  std::string m_model_name;
  TranscriptionWidget *m_widget;
  int m_trx_task;

  TranscriptionThread(std::string &path, std::string &model_name, TranscriptionWidget *widget, int task);
  ~TranscriptionThread();
  virtual void *Entry();
};

// Transcription thread events.
wxDEFINE_EVENT(EVT_TRX_THREAD_START, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_MODEL_LOADED, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_UPDATE, TranscriptionUpdateEvent);


// Model download wizard
class ModelDownloadDialog : public wxDialog {
public:
  wxStaticText *m_dl_status;
  wxButton *m_dl_btn;
  wxGauge *m_dl_gauge;
  wxBoxSizer *m_sizer;
  std::string m_model_name;
  ModelInfo m_model_info;

  ModelDownloadDialog(wxWindow *parent, const std::string &model_name);

private:
  void on_dl_btn_click(wxCommandEvent &evt);
  void on_dl_btn_close_click(wxCommandEvent &evt);
  void download_model();
};

} // namespace capen

