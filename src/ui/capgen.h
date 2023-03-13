#pragma once

#include "core/transcribe.h"
#include "utils.h"

#include <wx/wxprec.h>
// For the platforms without support for precompiled headers.
#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif

#include <filesystem>
#include <memory>
#include <vector>


namespace capgen {

class Application : public wxApp {
public:
    ModelsManager models_manager = ModelsManager();

    virtual bool OnInit();
};


// IDs for app widgets.
enum 
{
    ID_audio_add = 1,
    ID_video_add,
    ID_about_btn,
    ID_transcribe_btn,
    ID_timer,
    ID_model_selector,
    ID_model_dl_btn,
    ID_model_dl_close_btn
};


class MainWindow : public wxFrame {
public:
    MainWindow();
    void add_trx_widget(std::filesystem::path media_filepath);
    Application &app() { return m_app; }
    std::string get_selected_task() const { return m_task_choices->GetStringSelection().ToStdString(); }
    std::string get_selected_model() const { return m_model_choices->GetStringSelection().ToStdString(); }

    void enable_trx_buttons() const 
    {
        m_audio_btn->Enable();
        m_video_btn->Enable();
    }

    void disable_trx_buttons() const
    {
        m_audio_btn->Disable();
        m_video_btn->Disable();
    }

private:
    Application& m_app;
    wxPanel *m_main_panel;
    wxPanel *m_toolbar;
    wxButton *m_audio_btn;
    wxButton *m_video_btn;
    wxChoice *m_model_choices;
    wxChoice *m_task_choices;
    wxScrolledWindow *m_content_window;
    wxSizer *m_content_sizer;

    void on_toolbar_btn_hover(wxMouseEvent &evt);
    void on_toolbar_btn_leave(wxMouseEvent &evt);
    void on_audio_add(wxCommandEvent &evt);
    void on_video_add(wxCommandEvent &evt);
    void on_model_choice_update(wxCommandEvent &evt);
    void on_about(wxCommandEvent &evt);
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
    TranscriptionWidget(MainWindow *main_window, wxScrolledWindow *parent_window, std::filesystem::path media_filepath);
    MainWindow *get_main_window() const { return m_main_window; }

private:
    MainWindow *m_main_window;
    wxStaticText *m_progbar_text;
    wxStaticText *m_out_fpath_text;
    wxGauge *m_progbar;
    wxStaticText *m_status_text;
    wxButton *m_transcribe_btn;
    std::filesystem::path m_media_filepath;
    // Timer allows us to display indeterminate progress bar when loading the model. 
    wxTimer m_timer;

    void on_timer_update(wxTimerEvent &evt);
    void on_trx_btn_click(wxCommandEvent &evt);
    void on_trx_model_load(wxThreadEvent &evt);
    void on_trx_thread_completion(wxThreadEvent& evt);
    void on_trx_thread_start(wxThreadEvent& evt);
    void on_trx_thread_update(TranscriptionUpdateEvent& evt);
    void on_trx_thread_fail(wxThreadEvent &evt);
};


class TranscriptionThread : public wxThread {
public:
    std::filesystem::path m_media_filepath;
    std::string m_model_name;
    ModelType m_model_type;
    TranscriptionWidget *m_widget;
    TranscriptionTask m_trx_task;

    TranscriptionThread(std::filesystem::path m_media_filepath,
                        std::string &model_name,
                        ModelType model_type,
                        TranscriptionWidget *widget,
                        TranscriptionTask task);
    ~TranscriptionThread();
    virtual void *Entry();
};

// Transcription thread events.
wxDEFINE_EVENT(EVT_TRX_THREAD_START, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_MODEL_LOADED, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_UPDATE, TranscriptionUpdateEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_FAILED, wxThreadEvent);

// Model download wizard
class ModelDownloadDialog : public wxDialog {
public:
    ModelDownloadDialog(wxWindow *parent, const ModelInfo &model_info);

private:
    wxStaticText *m_dl_status;
    wxButton *m_dl_btn;
    wxGauge *m_dl_gauge;
    wxBoxSizer *m_sizer;
    std::string m_model_name;
    ModelInfo m_model_info;

    void on_dl_btn_click(wxCommandEvent &evt);
    void on_dl_btn_close_click(wxCommandEvent &evt);
    void download_model();
};

} // namespace capen
