#include "core/exceptions.h"
#include "capgen.h"
#include "core/log.h"

#include <wx/app.h> 
#include <wx/gdicmn.h>
#include <wx/imagpng.h>
#include <wx/webrequest.h>
#include <wx/wfstream.h>

// A c++ file containing raw png data for icons. It is meant to be included like this
// and is therefore not compiled separately.
#include "icons"
#include "logo.xpm"

// Registers our application's entry point.
wxIMPLEMENT_APP(capgen::Application);


// Called upon startup.
bool capgen::Application::OnInit()
{
    // Allows loading of toolbar icons from embedded PNG data.
    wxImage::AddHandler(new wxPNGHandler);

    capgen::MainWindow *main_window = new capgen::MainWindow();
    main_window->Show();
    CG_LOG_MINFO("Application has started");

    return true;
}


capgen::MainWindow::MainWindow()
  : wxFrame(NULL, wxID_ANY, "Capgen", wxDefaultPosition, wxSize(600, 800)),
    m_app(wxGetApp()), m_content_sizer(new wxBoxSizer(wxVERTICAL))
{
    SetIcon(wxICON(s_CAPGEN_LOGO));
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
    Centre();
    // DRAG & DROP
    SetDropTarget(new MediaFileDragAndDropTarget(this));

    // Panel to contain toolbar, options toolbar and scrolled window for transcription widgets.
    wxPanel *main_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, this->GetSize());
    main_panel->SetAutoLayout(true);
    main_panel->SetBackgroundColour(wxColour(75, 75, 75));
    wxBoxSizer *main_panel_sizer = new wxBoxSizer(wxVERTICAL);
    main_panel->SetSizer(main_panel_sizer);

    // Toolbars
    wxPanel *toolbar = create_toolbar(main_panel);
    wxPanel *options_toolbar = create_options_toolbar(main_panel);

    // Transcription widgets container.
    m_content_window = new wxScrolledWindow(main_panel, wxID_ANY, wxDefaultPosition, wxSize(600, 635));
    m_content_window->SetBackgroundColour(main_panel->GetBackgroundColour());
    m_content_window->AlwaysShowScrollbars();
    m_content_window->SetSizer(m_content_sizer);
    m_content_window->SetScrollbars(0, 20, 0, 100);
    create_default_trx_widget(m_content_window);

    main_panel_sizer->Add(toolbar, 0, wxALL | wxGROW);
    main_panel_sizer->Add(options_toolbar, 0, wxBOTTOM | wxGROW, 10);
    main_panel_sizer->Add(m_content_window, 0, wxALL | wxGROW);

    Bind(wxEVT_BUTTON, &MainWindow::on_about, this, ID_about_btn);
    Bind(wxEVT_BUTTON, &MainWindow::on_audio_add, this, ID_audio_add);
    Bind(wxEVT_BUTTON, &MainWindow::on_video_add, this, ID_video_add);
    Bind(wxEVT_CHOICE, &MainWindow::on_model_choice_update, this, ID_model_selector);
}

void capgen::MainWindow::add_trx_widget(std::filesystem::path media_filepath)
{
    capgen::TranscriptionWidget *trx_widget = new capgen::TranscriptionWidget(this, m_content_window, media_filepath);
    m_content_sizer->PrependSpacer(10);
    m_content_sizer->Prepend(trx_widget, 0, wxGROW | wxLEFT | wxRIGHT, 20);
    // Re-render the sizer.
    m_content_sizer->Layout();
    m_content_sizer->FitInside(m_content_window);

    // Remove the default trx widget if it is available.
    hide_default_trx_widget();
    if (m_trx_thread_is_running)
        m_trx_widgets_queue.push(trx_widget);
    else
        m_trx_thread_is_running = trx_widget->launch_transcription_task();
}

void capgen::MainWindow::notify_current_trx_finished()
{
    m_trx_thread_is_running = false;
    if (m_trx_widgets_queue.size() > 0) {
        m_trx_thread_is_running = m_trx_widgets_queue.front()->launch_transcription_task();
        m_trx_widgets_queue.pop();
    }
}

void capgen::MainWindow::create_default_trx_widget(wxScrolledWindow *parent_window)
{
    m_default_trx_widget = new wxPanel(parent_window, wxID_ANY, wxDefaultPosition, wxSize(300, 200));
    m_default_trx_widget->Centre(wxHORIZONTAL);
    wxBoxSizer *default_trx_widget_sizer = new wxBoxSizer(wxVERTICAL);  // DRAG FILES HERE TO TRANSCRIBE
    m_default_trx_widget->SetSizer(default_trx_widget_sizer);
    wxButton *drag_and_drop = new wxButton(m_default_trx_widget,
        wxID_ANY, "Drag audio or video files to transcribe.", wxDefaultPosition, wxSize(300, 100), wxBORDER_NONE);
    drag_and_drop->SetBitmap(wxBITMAP_PNG(capgen::s_DRAG_AND_DROP_ICON), wxTOP);
    drag_and_drop->SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    drag_and_drop->SetBackgroundColour(m_content_window->GetBackgroundColour());
    default_trx_widget_sizer->AddSpacer(100);
    default_trx_widget_sizer->Add(drag_and_drop, 0);
}

wxPanel *capgen::MainWindow::create_toolbar(wxPanel *parent_window)
{
    // A custom toolbar developed to overcome the limitations of the `wxToolBar` such as inability
    // to set background color.
    wxColour toolbar_bg(35, 35, 35);
    wxColour toolbar_fg(255, 255, 255);
    wxPanel *toolbar = new wxPanel(parent_window, wxID_ANY, wxDefaultPosition, wxSize(600, 100));
    toolbar->SetMinSize(wxSize(600, 100));
    toolbar->SetBackgroundColour(toolbar_bg);
    wxBoxSizer *toolbar_sizer = new wxBoxSizer(wxHORIZONTAL);
    toolbar->SetSizer(toolbar_sizer);

    wxButton *audio_trx_btn = new wxButton(toolbar, ID_audio_add, "Transcribe Audio", wxDefaultPosition, wxSize(120, 100), wxBORDER_NONE);
    audio_trx_btn->SetBitmap(wxBITMAP_PNG(capgen::s_AUDIO_ICON), wxTOP);
    audio_trx_btn->SetBackgroundColour(toolbar_bg);
    audio_trx_btn->SetForegroundColour(toolbar_fg);

    wxButton *video_trx_btn = new wxButton(toolbar, ID_video_add, "Transcribe Video", wxDefaultPosition, wxSize(120, 100), wxBORDER_NONE);
    video_trx_btn->SetBitmap(wxBITMAP_PNG(capgen::s_VIDEO_ICON), wxTOP);
    video_trx_btn->SetBackgroundColour(toolbar_bg);
    video_trx_btn->SetForegroundColour(toolbar_fg);

    wxButton *about_btn = new wxButton(toolbar, ID_about_btn, "About Capgen", wxDefaultPosition, wxSize(120, 100), wxBORDER_NONE);
    about_btn->SetBitmap(wxBITMAP_PNG(capgen::s_ABOUT_ICON), wxTOP);
    about_btn->SetBackgroundColour(toolbar_bg);
    about_btn->SetForegroundColour(toolbar_fg);

    toolbar_sizer->Add(audio_trx_btn, 0, wxGROW);
    toolbar_sizer->Add(video_trx_btn, 0, wxGROW | wxLEFT, 10);
    toolbar_sizer->AddStretchSpacer();
    toolbar_sizer->Add(about_btn, 0, wxGROW);

    // We use connect instead of bind function because the bind function cannot bind a mouse event
    // to a specific component such as button.
    audio_trx_btn->Connect(ID_audio_add, wxEVT_ENTER_WINDOW, wxMouseEventHandler(MainWindow::on_toolbar_btn_hover));
    audio_trx_btn->Connect(ID_audio_add, wxEVT_LEAVE_WINDOW, wxMouseEventHandler(MainWindow::on_toolbar_btn_leave));
    video_trx_btn->Connect(ID_video_add, wxEVT_ENTER_WINDOW, wxMouseEventHandler(MainWindow::on_toolbar_btn_hover));
    video_trx_btn->Connect(ID_video_add, wxEVT_LEAVE_WINDOW, wxMouseEventHandler(MainWindow::on_toolbar_btn_leave));
    about_btn->Connect(ID_about_btn, wxEVT_ENTER_WINDOW, wxMouseEventHandler(MainWindow::on_toolbar_btn_hover));
    about_btn->Connect(ID_about_btn, wxEVT_LEAVE_WINDOW, wxMouseEventHandler(MainWindow::on_toolbar_btn_leave));

    return toolbar;
}

wxPanel *capgen::MainWindow::create_options_toolbar(wxPanel *parent_window)
{
    wxPanel *options_panel = new wxPanel(parent_window, wxID_ANY, wxDefaultPosition, wxSize(600, 75));
    options_panel->SetBackgroundColour(wxColour(45, 45, 45));
    wxBoxSizer *options_panel_sizer = new wxBoxSizer(wxHORIZONTAL);
    options_panel->SetSizer(options_panel_sizer);

    // Task selector
    m_task_choices = new wxChoice(options_panel, wxID_ANY, wxDefaultPosition, wxSize(180, 28));
    m_task_choices->Append("English");
    m_task_choices->Append("Detected language");
    m_task_choices->Append("Translate to English");
    m_task_choices->Select(0);
    wxBoxSizer *task_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText *captions_text = new wxStaticText(options_panel, wxID_ANY, "TRANSCRIPTION LANGUAGE");
    captions_text->SetForegroundColour(wxColor(255, 255, 255));
    task_sizer->Add(captions_text, wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL));
    task_sizer->AddSpacer(6);
    task_sizer->Add(m_task_choices);

    // Model selector
    m_model_choices = new wxChoice(options_panel, ID_model_selector, wxDefaultPosition, wxSize(150, 28));
    m_model_choices->AppendString("tiny");
    m_model_choices->AppendString("base");
    m_model_choices->AppendString("small");
    // TODO: Allow models to be sorted and use enums to refer to models instead of strings.
    std::string default_model_name = m_app.models_manager.get_default_model_name();
    if (default_model_name == "tiny")
        m_model_choices->Select(0);
    else if (default_model_name == "base")
        m_model_choices->Select(1);
    else if (default_model_name == "small")
        m_model_choices->Select(2);
    wxBoxSizer *model_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText *model_text = new wxStaticText(options_panel, wxID_ANY, "MODEL");
    model_text->SetForegroundColour(wxColor(255, 255, 255));
    model_sizer->Add(model_text, wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL));
    model_sizer->AddSpacer(6);
    model_sizer->Add(m_model_choices);

    // Decoding method selector
    m_decoding_choices = new wxChoice(options_panel, wxID_ANY, wxDefaultPosition, wxSize(180, 28));
    m_decoding_choices->Append("Best quality");
    m_decoding_choices->Append("Fastest transcription");
    m_decoding_choices->Select(0);
    wxBoxSizer *decoding_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText *decoding_text = new wxStaticText(options_panel, wxID_ANY, "TRANSCRIPT OPTIONS");
    decoding_text->SetForegroundColour(wxColor(255, 255, 255));
    decoding_sizer->Add(decoding_text, wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL));
    decoding_sizer->AddSpacer(6);
    decoding_sizer->Add(m_decoding_choices);

    options_panel_sizer->Add(task_sizer, 0, wxGROW | wxLEFT | wxTOP, 10);
    options_panel_sizer->Add(model_sizer, 0, wxGROW | wxLEFT | wxTOP, 10);
    options_panel_sizer->Add(decoding_sizer, 0, wxGROW | wxLEFT | wxTOP, 10);

    return options_panel;
}

void capgen::MainWindow::hide_default_trx_widget()
{
    if (m_default_trx_widget)
        m_default_trx_widget->Hide();
}

void capgen::MainWindow::on_about(wxCommandEvent& evt)
{
    wxButton *btn = (wxButton*)evt.GetEventObject();
    btn->SetBackgroundColour(wxColour(35, 35, 35));

    //  TODO: Add developer info.
    wxMessageBox(
        "Capgen is a free automatic captions generator for audio and videos."
        " It utilizes Whisper neural networks released by OpenAI to generate"
        " accurate transcriptions with timestamps. All of the Capgen's code"
        " can be found at https://github.com/iangitonga/capgenx",
        "About capgen",
        wxOK | wxICON_INFORMATION
    );
}

void capgen::MainWindow::on_audio_add(wxCommandEvent& evt)
{
    wxButton *btn = (wxButton*)evt.GetEventObject();
    btn->SetBackgroundColour(wxColour(35, 35, 35));
    // TODO: Add more formats.
    wxFileDialog file_dialog(
        this, "Select audio file to transcribe", "", "",
        wxFileSelectorDefaultWildcardStr, 
        wxFD_OPEN | wxFD_FILE_MUST_EXIST,
        wxDefaultPosition,
        wxSize(400, 300)
    );
    if (file_dialog.ShowModal() == wxID_CANCEL)
        return;
    std::filesystem::path media_filepath(file_dialog.GetPath().ToStdString());
    add_trx_widget(media_filepath);
}

void capgen::MainWindow::on_model_choice_update(wxCommandEvent &event)
{
    std::string selected = m_model_choices->GetStringSelection().ToStdString();
    int selected_idx = m_model_choices->GetSelection();
    if (!(m_app.models_manager.model_is_registered(selected)))
    {
        capgen::ModelInfo target_model_info = m_app.models_manager.get_model_info(selected);
        ModelDownloadDialog download_dlg(this, target_model_info);
        download_dlg.ShowModal();
        // Check if the model was downloaded and if not, revert to the default model.
        m_app.models_manager.reload_registered_models();
        if (!(m_app.models_manager.model_is_registered(selected)))
        {
            std::string default_model_name = m_app.models_manager.get_default_model_name();
            if (default_model_name == "tiny")
                m_model_choices->Select(0);
            else if (default_model_name == "base")
                m_model_choices->Select(1);
            else if (default_model_name == "small")
                m_model_choices->Select(2);
            else
                m_model_choices->Select(64); // Force selection of none.
        }
    }
}

void capgen::MainWindow::on_toolbar_btn_hover(wxMouseEvent &evt)
{
    wxButton *btn = (wxButton*)evt.GetEventObject();
    btn->SetBackgroundColour(wxColour(65, 65, 65));
}

void capgen::MainWindow::on_toolbar_btn_leave(wxMouseEvent &evt)
{
    wxButton *btn = (wxButton*)evt.GetEventObject();
    btn->SetBackgroundColour(wxColour(35, 35, 35));
}

void capgen::MainWindow::on_video_add(wxCommandEvent& evt)
{
    wxButton *btn = (wxButton*)evt.GetEventObject();
    btn->SetBackgroundColour(wxColour(35, 35, 35));
    wxFileDialog file_dialog(
        this, "Select video file to transcribe", "", "", 
        wxFileSelectorDefaultWildcardStr, 
        wxFD_OPEN | wxFD_FILE_MUST_EXIST,
        wxDefaultPosition,
        wxSize(400, 300)
    );
    if (file_dialog.ShowModal() == wxID_CANCEL)
        return;
    std::filesystem::path media_filepath(file_dialog.GetPath().ToStdString());
    add_trx_widget(media_filepath);
}

bool capgen::MediaFileDragAndDropTarget::OnDropFiles(wxCoord, wxCoord, const wxArrayString& filepaths)
{
    uint32_t n_files_dropped = filepaths.GetCount();

    for (uint32_t i = 0; i < n_files_dropped; i++)
    {
        std::filesystem::path media_filepath(filepaths[i].ToStdString());
        m_main_window->add_trx_widget(media_filepath);
    }
    return true;
}


capgen::TranscriptionWidget::TranscriptionWidget(MainWindow *main_window,
                                                 wxScrolledWindow *parent_window,
                                                 std::filesystem::path media_filepath)
  : wxPanel(parent_window, wxID_ANY), m_main_window(main_window), m_media_filepath(media_filepath), m_timer(this, ID_timer)
{
    this->SetBackgroundColour(wxColour(40, 40, 40));

    wxFont default_font = wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    wxFont bold_font = wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    std::string file_desc_path = m_media_filepath.filename().string();
    // Truncate if the path is too long so it fits into the window.
    if (file_desc_path.length() > 50)
        file_desc_path = file_desc_path.substr(0, file_desc_path.length() + 50) + "...";
    m_file_text = new wxStaticText(this, wxID_ANY, file_desc_path);
    m_file_text->SetForegroundColour(wxColour(255, 255, 255));
    m_file_text->SetFont(bold_font); 
    m_out_fpath_text = new wxStaticText(this, wxID_ANY, "");
    m_out_fpath_text->SetFont(default_font);
    m_out_fpath_text->SetForegroundColour(wxColour(50, 235, 25));

    wxBoxSizer *status_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_status_text = new wxStaticText(this, wxID_ANY, "Status: Queued");
    m_status_text->SetFont(default_font);
    m_status_text->SetForegroundColour(wxColour(50, 235, 25));

    m_progbar_text = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition);
    m_progbar_text->SetFont(default_font);
    m_progbar_text->SetForegroundColour(wxColour(50, 235, 25));

    status_sizer->Add(m_status_text, 0);
    status_sizer->Add(m_progbar_text, 0, wxLEFT, 335);

    // Transcription progress bar.
    m_progbar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(500, 8), wxGA_SMOOTH | wxGA_HORIZONTAL);

    // Widget sizer.
    wxBoxSizer *main_sizer = new wxBoxSizer(wxVERTICAL);
    const int border_size = 30;
    main_sizer->AddSpacer(20);
    main_sizer->Add(m_file_text, 0, wxGROW | wxLEFT, border_size);
    main_sizer->AddSpacer(15);
    main_sizer->Add(status_sizer, 0, wxGROW | wxLEFT, border_size);
    main_sizer->AddSpacer(10);
    main_sizer->Add(m_progbar, 0, wxLEFT, border_size);
    main_sizer->Add(m_out_fpath_text, 0, wxGROW | wxLEFT, border_size);
    main_sizer->AddSpacer(25);
    this->SetSizerAndFit(main_sizer);

    // Provide event bindings for thread events.
    Bind(EVT_TRX_THREAD_LAUNCH, &TranscriptionWidget::on_trx_thread_launch, this, wxID_ANY);
    Bind(EVT_TRX_THREAD_START, &TranscriptionWidget::on_trx_thread_start, this, wxID_ANY);
    Bind(EVT_TRX_THREAD_UPDATE, &TranscriptionWidget::on_trx_thread_update, this, wxID_ANY);
    Bind(EVT_TRX_THREAD_COMPLETED, &TranscriptionWidget::on_trx_thread_completion, this,wxID_ANY);
    Bind(EVT_TRX_THREAD_FAILED, &TranscriptionWidget::on_trx_thread_fail, this, wxID_ANY);
    Bind(EVT_TRX_THREAD_MEDIA_DECODING_FAILED, &TranscriptionWidget::on_trx_thread_media_decode_fail, this, wxID_ANY);
    Bind(wxEVT_TIMER, &TranscriptionWidget::on_timer_update, this, ID_timer);
}

void capgen::TranscriptionWidget::on_trx_thread_completion(wxThreadEvent &event)
{
    m_file_text->SetForegroundColour(wxColour(50, 235, 25));
    m_progbar_text->SetLabelText("");
    m_progbar->Hide();
    m_status_text->Hide();

    std::string out_fpath = m_media_filepath.replace_extension("srt").filename().string();
    if (out_fpath.length() > 32)
        out_fpath = out_fpath.substr(0, out_fpath.length() + 32) + "...";
    std::string label_text = std::string("Transcription file: ") + out_fpath;
    m_out_fpath_text->SetLabelText(label_text);
    m_main_window->notify_current_trx_finished();
}

void capgen::TranscriptionWidget::on_trx_thread_fail(wxThreadEvent &event)
{
    m_timer.Stop();
    m_progbar->SetValue(0);
    m_progbar->Hide();
    m_progbar_text->SetLabelText("");
    // TODO: Set red color.
    m_status_text->SetForegroundColour(wxColour(235, 25, 25));
    m_status_text->SetLabelText("Status: Transcription failed!");
    wxLogError("An unexpected error occurred during transcription process.");
    m_main_window->notify_current_trx_finished();
}

void capgen::TranscriptionWidget::on_trx_thread_media_decode_fail(wxThreadEvent &evt) {
    m_timer.Stop();
    m_progbar->SetValue(0);
    m_progbar->Hide();
    m_progbar_text->SetLabelText("");
    // TODO: Set red color.
    m_status_text->SetForegroundColour(wxColour(235, 25, 25));
    m_status_text->SetLabelText("Audio or video decoding failed!");
    wxLogError("Media file (%s) could not be decoded.", m_media_filepath.c_str());
    m_main_window->notify_current_trx_finished();    
}

void capgen::TranscriptionWidget::on_trx_thread_launch(wxThreadEvent &event)
{
    m_timer.Start(100);
    m_status_text->SetLabelText("Preparing to transcribe...");
    m_progbar->Pulse();
}

void capgen::TranscriptionWidget::on_trx_thread_start(wxThreadEvent &event)
{
    m_timer.Stop();
    m_status_text->SetLabelText("Transcribing...");
    m_progbar_text->SetLabelText("0%");
    m_progbar->SetValue(0);
}

void capgen::TranscriptionWidget::on_trx_thread_update(TranscriptionUpdateEvent &event)
{
    float percentage = event.get_progress();
    m_progbar->SetValue(percentage);
    std::string label = std::string("") + std::to_string((int)percentage) + std::string("%");
    m_progbar_text->SetLabelText(label);
}

void capgen::TranscriptionWidget::on_timer_update(wxTimerEvent &event) 
{
    m_progbar->Pulse();
}

bool capgen::TranscriptionWidget::launch_transcription_task()
{
    std::string selected_task = m_main_window->get_selected_task();
    capgen::ModelType model_type;
    capgen::TranscriptionTask trx_task;
    capgen::TranscriptionDecoder decoder = m_main_window->get_selected_decoder();
    if (selected_task == "English") {
        model_type = capgen::ModelType::English;
        trx_task = capgen::TranscriptionTask::Transcribe;
    }
    else
    {
        model_type = capgen::ModelType::Multilingual;
        if (selected_task == "Detected language")
            trx_task = capgen::TranscriptionTask::Transcribe;
        else
            trx_task = capgen::TranscriptionTask::Translate;
    }
    std::string selected_model = m_main_window->get_selected_model();
    if (selected_model == "")
    {
        wxLogInfo("Please select the model to use in the models selector below the toolbar..");
        return false;
    }

    TranscriptionThread *trx_thread = new TranscriptionThread(this, m_media_filepath, selected_model, model_type, trx_task, decoder);
    if (trx_thread->Run() != wxTHREAD_NO_ERROR)
    {
        CG_LOG_ERROR("Transcription thread failed to run for media file: %s", m_media_filepath.c_str());
        delete trx_thread;
        wxLogError("Something went wrong and transcription could not be done. Try restarting the application.");
        return false;
    }
    CG_LOG_INFO("Transcription thread running for media file: %s", m_media_filepath.c_str());
    return true;
}

capgen::TranscriptionThread::TranscriptionThread(TranscriptionWidget *widget,
                                                 std::filesystem::path media_filepath,
                                                 std::string &model_name,
                                                 capgen::ModelType model_type,
                                                 TranscriptionTask task,
                                                 TranscriptionDecoder decoder)
  : wxThread(wxTHREAD_DETACHED), m_media_filepath(media_filepath), m_model_name(model_name),
    m_model_type(model_type), m_widget(widget), m_trx_task(task), m_decoder(decoder)
  {}

capgen::TranscriptionThread::~TranscriptionThread() 
{
    CG_LOG_MINFO("Deleted Transcription thread");
}

// Code run by transcription thread. The transcription thread does not run any code
// that directly changes the UI. It is not safe to change UI from any thread other
// than the main thread. So, instead of interfering with UI, the transcription worker
// thread queue's the necessary events which the main thread receives and updates
// the UI. The thread queing code is implemented by wxWidgets to be thread-safe. 
void *capgen::TranscriptionThread::Entry()
{
    std::function<void()> trx_start_callback = [this]() {
        wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_START));
    };

    std::function<void(float)> trx_update_callback = [this](float progress) {
        wxQueueEvent(m_widget, new TranscriptionUpdateEvent(EVT_TRX_THREAD_UPDATE, wxID_ANY, progress));
    };    

    if (!TestDestroy())
    {
        wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_LAUNCH));
        try
        {
            Application& app = wxGetApp();
            auto model = app.models_manager.get_model(m_model_name, m_model_type);
            capgen::transcribe(m_media_filepath, model, m_trx_task, m_decoder, trx_start_callback, trx_update_callback);
            wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_COMPLETED));
        }
        catch (MediaDecodingException e)
        {
            wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_MEDIA_DECODING_FAILED));
            CG_LOG_MERROR(e.what());
        }
        catch (const std::exception &e)
        {
            wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_FAILED));
            CG_LOG_MERROR(e.what());
        }
    }

    return (void *)0;
}

capgen::ModelDownloadDialog::ModelDownloadDialog(wxWindow *parent, const ModelInfo &model_info)
   : wxDialog(parent, wxID_ANY, "Download model", wxDefaultPosition, wxSize(300, 300)), m_model_info(model_info)
{
    wxFont text_font = wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_MEDIUM);
    const std::string model_name_s = std::string("Model Name: ") + model_info.name;
    wxStaticText *model_name_text = new wxStaticText(this, wxID_ANY, model_name_s);
    model_name_text->SetFont(text_font);
    const std::string model_dl_size_s = std::string("Download Size: ") + std::to_string(m_model_info.dl_size_mb) + "MB";
    wxStaticText *model_dl_size_text = new wxStaticText(this, wxID_ANY, model_dl_size_s);
    model_dl_size_text->SetFont(text_font);
    const std::string memory_usage_s = std::string("Memory Usage: ") + std::to_string(m_model_info.mem_usage_mb) + "MB";
    wxStaticText *memory_usage_text = new wxStaticText(this, wxID_ANY, memory_usage_s);
    memory_usage_text->SetFont(text_font);

    const std::string dl_status_s = std::string("Download Progress: (0MB/") + std::to_string(m_model_info.dl_size_mb) + "MB)";
    m_dl_status = new wxStaticText(this, wxID_ANY, dl_status_s);
    m_dl_status->SetFont(text_font.Bold());
    m_dl_gauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(280, 20));
    m_dl_btn = new wxButton(this, ID_model_dl_btn, "DOWNLOAD");
    m_dl_btn->SetFont(text_font);

    wxButton *dl_close_btn = new wxButton(this, ID_model_dl_close_btn, "CLOSE");
    dl_close_btn->SetFont(text_font.Bold());
    dl_close_btn->SetBackgroundColour(wxColour(216, 0, 0));

    m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->Add(model_name_text, 0, wxTOP | wxLEFT, 10);
    m_sizer->Add(model_dl_size_text, 0, wxTOP | wxLEFT, 10);
    m_sizer->Add(memory_usage_text, 0, wxTOP | wxLEFT, 10);
    m_sizer->AddSpacer(60);
    m_sizer->AddSpacer(40); // Extra space that accounts for hidden status and gauge. See below.
    m_sizer->Add(m_dl_status, 0, wxLEFT, 10);
    m_sizer->Add(m_dl_gauge, 0, wxLEFT, 10);
    m_sizer->Add(m_dl_btn, 0,  wxALIGN_CENTER_HORIZONTAL | wxTOP, 10);
    m_sizer->Add(dl_close_btn, 0,  wxALIGN_CENTER_HORIZONTAL | wxTOP, 10);
    SetSizer(m_sizer);

    // Download status items hidden until user clicks download button.
    m_sizer->Hide(5); // Download status text
    m_sizer->Hide(6); // Download status gauge
    m_sizer->Hide(8); // Close button.
    m_sizer->Layout();

    Bind(wxEVT_BUTTON, &ModelDownloadDialog::on_dl_btn_click, this, ID_model_dl_btn);
    Bind(wxEVT_BUTTON, &ModelDownloadDialog::on_dl_btn_close_click, this, ID_model_dl_close_btn);
}

void capgen::ModelDownloadDialog::on_dl_btn_click(wxCommandEvent &evt)
{
    m_dl_btn->Disable();
    m_sizer->Hide(4); // Extra padding space.
    m_sizer->Show((size_t)5);  // Download status text
    m_sizer->Show((size_t)6);  // Download status gauge
    m_sizer->Layout();

    download_model();
}

void capgen::ModelDownloadDialog::on_dl_btn_close_click(wxCommandEvent &evt)
{
    Close();
}

void capgen::ModelDownloadDialog::download_model()
{
    CG_LOG_INFO("Model download started for model=%s, url=%s", m_model_name.c_str(), m_model_info.url);
    wxWebRequest request = wxWebSession::GetDefault().CreateRequest(this, m_model_info.url);
    // Allows progress monitoring of the download process.
    request.SetStorage(wxWebRequest::Storage::Storage_None);
    if (!request.IsOk())
    {
        wxLogError("An internet connection could not be established. Check your internet connection status.");
        CG_LOG_MERROR("Model download request is not OK");
        return;
    }

    std::string out_fpath = std::string("./assets/models/") + m_model_name + ".zip";
    std::shared_ptr<wxFileOutputStream> out_stream = std::make_shared<wxFileOutputStream>(out_fpath);
    if (!out_stream->IsOk())
    {
        wxLogError("A file to store the downloaded model could not be created at %s.", out_fpath);
        CG_LOG_ERROR("Failed to open model download output file: %s", out_fpath.c_str());
        return;
    }

    // Bind state event
    Bind(wxEVT_WEBREQUEST_STATE, [request, out_fpath, this](wxWebRequestEvent& evt) {
        switch (evt.GetState()) 
        {
            case wxWebRequest::State_Completed:
            {
                CG_LOG_MINFO("Model download complete");
                m_dl_status->SetLabelText("Download Complete!");
                m_sizer->Hide((size_t)7);  // Download button.
                m_sizer->Show((size_t)8);  // Close button.
                std::string archive_outdir = std::string("./assets/models/") + m_model_name + "/";
                if (wxMkdir(archive_outdir))
                {
                    capgen::extract_downloaded_model(out_fpath, archive_outdir);
                    m_dl_gauge->SetValue(100);
                    wxRemoveFile(out_fpath);
                } 
                else
                {
                  wxLogError("A directory to store the models be created at %s.", archive_outdir);
                  CG_LOG_ERROR("Failed to create output directory for downloaded model at %s", archive_outdir.c_str());
                }
                break;
            }

            case wxWebRequest::State_Failed: 
            {
                m_dl_btn->Enable();
                wxLogError("An internet connection could not be established. Check your internet connection status.");
                CG_LOG_ERROR("Connection Error: %s", evt.GetErrorDescription().ToStdString().c_str());
                break;
            }
        }
    });

    Bind(wxEVT_WEBREQUEST_DATA, [request, out_stream, this](wxWebRequestEvent& event) {
        out_stream->Write(event.GetDataBuffer(), event.GetDataSize());
        int total_dl_size = m_model_info.dl_size_mb;
        wxString progress_msg;
        progress_msg.Printf("Download progress: (%dMB/%dMB)", capgen::b_to_mb(request.GetBytesReceived()), total_dl_size);
        m_dl_status->SetLabelText(progress_msg);
        float progress = ((float)request.GetBytesReceived() / capgen::mb_to_b(total_dl_size)) * 90.0f;
        m_dl_gauge->SetValue(progress);
    });

    // Start the request
    request.Start();
}
