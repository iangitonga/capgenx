#pragma once

#include "core/transcribe.h"
#include "utils.h"

#include <wx/wxprec.h>
// For the platforms without support for precompiled headers.
#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif
#include <wx/dnd.h>

#include <filesystem>
#include <memory>
#include <vector>
#include <queue>


namespace capgen {

class Application : public wxApp
{
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
    ID_timer,
    ID_model_selector,
    ID_model_dl_btn,
    ID_model_dl_close_btn
};


// Forward declaration to allow `m_trx_widgets_queue` definition in MainWindow.
class TranscriptionWidget;


class MainWindow : public wxFrame
{
public:
    MainWindow();
    void add_trx_widget(std::filesystem::path media_filepath);
    void notify_current_trx_finished();
    std::string get_selected_task() const { return m_task_choices->GetStringSelection().ToStdString(); }
    std::string get_selected_model() const { return m_model_choices->GetStringSelection().ToStdString(); }

private:
    Application& m_app;
    wxChoice *m_model_choices;
    wxChoice *m_task_choices;
    wxScrolledWindow *m_content_window;
    wxSizer *m_content_sizer;
    wxPanel *m_default_trx_widget;

    // Indicates whether a transcription thread is currently running.
    bool m_trx_thread_is_running = false;
    // Stores transcription widgets which hold transcription threads to be run in the future
    // in case a transcription thread is currently running.
    std::queue<TranscriptionWidget*> m_trx_widgets_queue;

    void create_default_trx_widget(wxScrolledWindow *parent_window);
    wxPanel *create_toolbar(wxPanel *parent_window);
    wxPanel *create_options_toolbar(wxPanel *parent_window);
    void hide_default_trx_widget();
    void on_about(wxCommandEvent &evt);
    void on_audio_add(wxCommandEvent &evt);
    void on_model_choice_update(wxCommandEvent &evt);
    void on_toolbar_btn_hover(wxMouseEvent &evt);
    void on_toolbar_btn_leave(wxMouseEvent &evt);
    void on_video_add(wxCommandEvent &evt);
};


class MediaFileDragAndDropTarget : public wxFileDropTarget
{
public:
    MediaFileDragAndDropTarget(capgen::MainWindow *main_window) : m_main_window(main_window) {}
    bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames);

private:
    capgen::MainWindow *m_main_window;
};


class TranscriptionUpdateEvent : public wxThreadEvent
{
public:
    TranscriptionUpdateEvent(wxEventType eventType, int id, int progress)
        : wxThreadEvent(eventType, id), m_progress(progress)
    {}
    int get_progress() const { return m_progress; }
    wxEvent *Clone() const { return new TranscriptionUpdateEvent(*this); }

private:
    int m_progress;
};


class TranscriptionWidget : public wxPanel
{
public:
    TranscriptionWidget(MainWindow *main_window, wxScrolledWindow *parent_window, std::filesystem::path media_filepath);
    bool launch_transcription_task();

private:
    MainWindow *m_main_window;
    std::filesystem::path m_media_filepath;
    wxGauge *m_progbar;
    wxStaticText *m_progbar_text;
    wxStaticText *m_out_fpath_text;
    wxStaticText *m_status_text;
    // Timer allows us to display indeterminate progress bar when waiting for model loading
    // and audio decoding. 
    wxTimer m_timer;

    void on_trx_thread_completion(wxThreadEvent& evt);
    void on_trx_thread_fail(wxThreadEvent &evt);
    void on_trx_thread_media_decode_fail(wxThreadEvent &evt);
    void on_trx_thread_launch(wxThreadEvent& evt);
    void on_trx_thread_start(wxThreadEvent &evt);
    void on_trx_thread_update(TranscriptionUpdateEvent& evt);
    void on_timer_update(wxTimerEvent &evt);
};


class TranscriptionThread : public wxThread
{
public:
    TranscriptionThread(TranscriptionWidget *widget,
                        std::filesystem::path m_media_filepath,
                        std::string &model_name,
                        ModelType model_type,
                        TranscriptionTask task);
    ~TranscriptionThread();
    virtual void *Entry();

private:
    TranscriptionWidget *m_widget;
    std::filesystem::path m_media_filepath;
    std::string m_model_name;
    ModelType m_model_type;
    TranscriptionTask m_trx_task;
};

// Transcription thread events.
wxDEFINE_EVENT(EVT_TRX_THREAD_LAUNCH, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_START, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_UPDATE, TranscriptionUpdateEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_FAILED, wxThreadEvent);
wxDEFINE_EVENT(EVT_TRX_THREAD_MEDIA_DECODING_FAILED, wxThreadEvent);

// Model download wizard
class ModelDownloadDialog : public wxDialog
{
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
