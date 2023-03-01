// Included in the top so that it includes libTorch first.
#include "capgen.h"

#include <wx/app.h> 
#include <wx/statline.h>
#include <wx/webrequest.h>
#include <wx/wfstream.h>

#include <wx/gdicmn.h>
#include <wx/imagpng.h>

// A c++ file containing raw png data for toolbar icons. It is meant to be included like this
// and is therefore not compiled separately.
#include "toolbar_icons"

// Registers our application's entry point.
wxIMPLEMENT_APP(capgen::Application);


// Called upon startup.
bool capgen::Application::OnInit() {
  // Allows loading of toolbar icons from PNG data.
  wxImage::AddHandler(new wxPNGHandler);

  capgen::MainWindow *main_window = new capgen::MainWindow();
  main_window->Show();
  std::cout << "[UI]: Main window has rendered.\n";
  return true;
}


capgen::MainWindow::MainWindow()
  : wxFrame(NULL, wxID_ANY, "Capgen", wxDefaultPosition, wxSize(600, 700)),
    m_content_sizer(new wxBoxSizer(wxVERTICAL))
{
  this->SetMinSize(wxSize(600, 700));
  // Menu bar
  wxMenu *menu_home = new wxMenu;
  menu_home->Append(wxID_EXIT, "Exit", "Exit the program");
  wxMenu *menu_help = new wxMenu;
  menu_help->Append(wxID_ABOUT);
  wxMenuBar *menu_bar = new wxMenuBar;
  menu_bar->Append(menu_home, "&File");
  menu_bar->Append(menu_help, "&Help");
  SetMenuBar(menu_bar);

  // Toolbar
  // The parameter `256` exists only because it allows the toolbar icons text to
  // appear. It should be removed in the future once a better way is found.
  wxToolBar *toolbar = CreateToolBar(256);
  toolbar->AddTool(ID_audio_add, "Transcribe audio", wxBITMAP_PNG(capgen::s_AUDIO_ICON));
  toolbar->AddTool(ID_video_add, "Transcribe video", wxBITMAP_PNG(capgen::s_VIDEO_ICON));
  toolbar->Realize();

  // Content manager.
  m_content_window = new wxScrolledWindow(this);
  m_content_window->SetBackgroundColour(wxColour(60, 60, 60));
  m_content_window->AlwaysShowScrollbars();
  m_content_window->SetSizer(m_content_sizer);
  m_content_window->SetScrollbars(0, 20, 0, 100);
  
  Centre();

  Bind(wxEVT_MENU, &MainWindow::on_about, this, wxID_ABOUT);
  Bind(wxEVT_MENU, &MainWindow::on_exit, this, wxID_EXIT);
  Bind(wxEVT_TOOL, &MainWindow::on_audio_add, this, ID_audio_add);
  Bind(wxEVT_TOOL, &MainWindow::on_video_add, this, ID_video_add);
}

void capgen::MainWindow::on_about(wxCommandEvent& evt) {
  //  TODO: Add developer info.
  wxMessageBox(
    "Capgen is a free automatic captions generator for audio and videos.",
    "About capgen",
    wxOK | wxICON_INFORMATION
  );
}

void capgen::MainWindow::on_audio_add(wxCommandEvent& evt) {
  // TODO: Add more formats.
  wxFileDialog file_dialog(
    this, "Select audio file to transcribe", "", "", 
    "Audio files (*.mp3;*.mp2;*.m4a;*.wav;*.flac;*.aac;*.webm;*.ogg)|*.mp3;*.mp2;*.m4a;*.wav;*.flac;*aac;*.webm;*.ogg", 
    wxFD_OPEN | wxFD_FILE_MUST_EXIST,
    wxDefaultPosition,
    wxSize(400, 300)
    );
  if (file_dialog.ShowModal() == wxID_CANCEL)
    return;
  auto path = file_dialog.GetPath();
  add_trx_widget(path);
  GetToolBar()->EnableTool(ID_audio_add, false);
  GetToolBar()->EnableTool(ID_video_add, false);
}

void capgen::MainWindow::on_exit(wxCommandEvent& evt) {
  Close(true);
}

void capgen::MainWindow::on_video_add(wxCommandEvent& evt) {
  // TODO: Add more formats.
  wxFileDialog file_dialog(
    this, "Select video file to transcribe", "", "", 
    "Video files (*.mp4;*.mkv;*.avi;*.ts)|*.mp4;*.mkv;*.avi;*.ts", 
    wxFD_OPEN | wxFD_FILE_MUST_EXIST,
    wxDefaultPosition,
    wxSize(400, 300)
    );
  if (file_dialog.ShowModal() == wxID_CANCEL)
    return;
  auto path = file_dialog.GetPath();
  add_trx_widget(path);
  GetToolBar()->EnableTool(ID_audio_add, false);
  GetToolBar()->EnableTool(ID_video_add, false);
}

void capgen::MainWindow::add_trx_widget(wxString &path) {
  capgen::TranscriptionWidget *widget = new capgen::TranscriptionWidget(this, m_content_window, path);
  m_content_sizer->PrependSpacer(20);
  m_content_sizer->Prepend(widget, 0, wxEXPAND);
  // Re-render the sizer.
  m_content_sizer->Layout();
  m_content_sizer->FitInside(m_content_window);
}


capgen::TranscriptionWidget::TranscriptionWidget(MainWindow *main_window, wxScrolledWindow *parent_window, const wxString &path)
  : m_app(wxGetApp()), m_main_window(main_window), wxPanel(parent_window, wxID_ANY), m_audio_filepath(path.ToStdString()), m_timer(this, ID_timer)
{
  this->SetBackgroundColour(wxColour(40, 40, 40));

  // Top separator line.
  wxStaticLine *top_line = new wxStaticLine(this, wxID_ANY, wxDefaultPosition);
  top_line->SetBackgroundColour(wxColour(65, 65, 65));

  // Task selector
  m_task_choices = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(150, 31));
  m_task_choices->Append("Transcribe");
  m_task_choices->Append("Translate to English");
  m_task_choices->Select(0);
  wxBoxSizer *task_sizer = new wxBoxSizer(wxVERTICAL);
  task_sizer->Add(new wxStaticText(this, wxID_ANY, "TASK"),
                  wxSizerFlags());
  task_sizer->AddSpacer(6);
  task_sizer->Add(m_task_choices);

  // Model selector
  m_model_choices = new wxChoice(this, ID_model_selector, wxDefaultPosition, wxSize(150, 31));
  for (const auto &models_kv : capgen::MODELS)
    m_model_choices->AppendString(models_kv.first);
  // TODO: Select medium by default.
  m_model_choices->Select(0);
  wxBoxSizer *model_sizer = new wxBoxSizer(wxVERTICAL);
  model_sizer->Add(new wxStaticText(this, wxID_ANY, "MODEL"),
                   wxSizerFlags().Align(wxALIGN_CENTER_HORIZONTAL));
  model_sizer->AddSpacer(6);
  model_sizer->Add(m_model_choices);

  // Selectors sizer
  wxSizer *selectors_sizer = new wxBoxSizer(wxHORIZONTAL);
  selectors_sizer->Add(task_sizer);
  selectors_sizer->Add(model_sizer, 0, wxLEFT, 20);

  // Separates selectors and status text.
  wxStaticLine *selectors_line = new wxStaticLine(this, wxID_ANY, wxDefaultPosition);
  selectors_line->SetBackgroundColour(wxColour(55, 55, 55));

  // Status text
  wxFont text_font = wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_MEDIUM);
  wxString file_desc = wxString("File: ") + path;
  wxStaticText *file_text = new wxStaticText(this, wxID_ANY, file_desc);
  file_text->SetFont(text_font);
  m_status_text = new wxStaticText(this, wxID_ANY, "");
  m_status_text->SetFont(text_font);

  // Transcription progress bar.
  wxSizer *progbar_sizer = new wxBoxSizer(wxHORIZONTAL);
  m_progbar_text = new wxStaticText(this, wxID_ANY, " 0%", wxDefaultPosition, wxSize(50, 20));
  wxFont m_progbar_font = wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
  m_progbar_text->SetFont(m_progbar_font);
  m_progbar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(400, 20), wxGA_SMOOTH | wxGA_HORIZONTAL);
  progbar_sizer->Add(m_progbar, 0);
  progbar_sizer->Add(m_progbar_text, 0, wxTOP, 8);

  // Transcribe button.
  wxButton *transcribe_btn = new wxButton(this, ID_transcribe_btn, "TRANSCRIBE");
  wxFont transcribe_btn_font = wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
  transcribe_btn->SetFont(transcribe_btn_font);

  // Widget sizer.
  wxBoxSizer *main_sizer = new wxBoxSizer(wxVERTICAL);

  main_sizer->Add(top_line, 0, wxGROW);
  main_sizer->AddSpacer(9);
  main_sizer->Add(selectors_sizer, 0, wxGROW | wxLEFT, 15);
  main_sizer->AddSpacer(9);
  main_sizer->Add(selectors_line, 0, wxGROW);
  main_sizer->AddSpacer(9);
  main_sizer->Add(file_text, 0, wxGROW | wxLEFT, 15);
  main_sizer->AddSpacer(4);
  main_sizer->Add(m_status_text, 0, wxGROW | wxLEFT, 15);
  main_sizer->AddSpacer(6);
  main_sizer->Add(progbar_sizer, 0, wxGROW | wxLEFT, 15);
  main_sizer->AddSpacer(6);
  main_sizer->Add(transcribe_btn, 0, wxLEFT, 15);
  main_sizer->AddSpacer(20);
  this->SetSizerAndFit(main_sizer);

  Bind(wxEVT_BUTTON, &TranscriptionWidget::on_trx_btn_click, this, ID_transcribe_btn);
  // Provide event bindings for thread events.
  Bind(EVT_TRX_THREAD_START, &TranscriptionWidget::on_trx_thread_start, this, wxID_ANY);
  Bind(EVT_TRX_THREAD_MODEL_LOADED, &TranscriptionWidget::on_trx_model_load, this, wxID_ANY);
  Bind(EVT_TRX_THREAD_UPDATE, &TranscriptionWidget::on_trx_thread_update, this, wxID_ANY);
  Bind(EVT_TRX_THREAD_COMPLETED, &TranscriptionWidget::on_trx_thread_completion, this,wxID_ANY);
  Bind(wxEVT_TIMER, &TranscriptionWidget::on_timer_update, this, ID_timer);
  Bind(wxEVT_CHOICE, &TranscriptionWidget::on_model_choice_update, this, ID_model_selector);
}

void capgen::TranscriptionWidget::on_model_choice_update(wxCommandEvent &event) {
  std::string selected = m_model_choices->GetStringSelection().ToStdString();
  if (!(m_app.m_models_manager.model_is_registered(selected))) {
    ModelDownloadDialog download_dlg(this, selected);
    download_dlg.ShowModal();
    // Check if the model was downloaded and if not, revert to the default model.
    m_app.m_models_manager.reload_registered_models();
    if (!(m_app.m_models_manager.model_is_registered(selected)))
      m_model_choices->Select(0);
  } 
}

void capgen::TranscriptionWidget::on_timer_update(wxTimerEvent &event) {
  m_progbar->Pulse();
}

void capgen::TranscriptionWidget::on_trx_btn_click(wxCommandEvent &event) {
  // TODO: string operations not ideal. Replace with a TranscriptionOptions object that
  // uses enum values for settings. Status: Transcribing
  std::string selected_task = m_task_choices->GetStringSelection().ToStdString();
  int task;
  if (selected_task == "Transcribe")
    task = capgen::Tokenizer::transcribe;
  else
    task = capgen::Tokenizer::translate;
  std::string selected_model = m_model_choices->GetStringSelection().ToStdString();
  // Transcribe
  TranscriptionThread *t_thread = new TranscriptionThread(m_audio_filepath, selected_model, this, task);
  if (t_thread->Run() != wxTHREAD_NO_ERROR) {
    wxLogError("[UI]: Transcription thread failed to run");
    delete t_thread;
  }
  std::cout << "[UI]: Run Transcription thread" << std::endl;
}

void capgen::TranscriptionWidget::on_trx_model_load(wxThreadEvent &event) {
  m_timer.Stop();
  m_status_text->SetLabelText("Status: Transcribing ...");
  m_progbar->SetValue(0);
}


void capgen::TranscriptionWidget::on_trx_thread_completion(wxThreadEvent &event) {
  m_main_window->GetToolBar()->EnableTool(ID_audio_add, true);
  m_main_window->GetToolBar()->EnableTool(ID_video_add, true);
  m_status_text->SetLabelText("Status: Transcription complete!");
  m_status_text->SetForegroundColour(wxColour(0, 255, 0));
}

void capgen::TranscriptionWidget::on_trx_thread_start(wxThreadEvent &event) {
  m_timer.Start(100);
  m_status_text->SetLabelText("Status: Loading the model ...");
  m_progbar->Pulse();
}

void capgen::TranscriptionWidget::on_trx_thread_update(TranscriptionUpdateEvent &event) {
  float percentage = event.get_progress();
  m_progbar->SetValue(percentage);
  std::string label = std::string(" ") + std::to_string((int)percentage) + std::string("%");
  m_progbar_text->SetLabelText(label);
}

capgen::TranscriptionThread::TranscriptionThread(std::string &path,
                                                 std::string &model_name,
                                                 TranscriptionWidget *widget,
                                                 int task)
  : wxThread(wxTHREAD_DETACHED), m_audio_path(path), m_model_name(model_name), m_widget(widget), m_trx_task(task)
  {}

capgen::TranscriptionThread::~TranscriptionThread() {
  std::cout << "[UI]: Deleting Transcription thread" << std::endl;
}

// Code run by transcription thread. The transcription thread does not run any code
// that directly changes the UI. It is not safe to change UI from any thread other
// than the main thread. So, instead of interfering with UI, the transcription worker
// thread queue's the necessary events which the main thread receives and updates
// the UI. The thread queing code is implemented by wxWidgets to be thread-safe. 
void *capgen::TranscriptionThread::Entry() {
  std::function<void(float)> update_cb = [this](float progress) {
    wxQueueEvent(m_widget, new TranscriptionUpdateEvent(EVT_TRX_THREAD_UPDATE, wxID_ANY, progress));
  };

  if (!TestDestroy()) {
    wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_START));
    try {
      auto model = m_widget->m_app.m_models_manager.get_model(m_model_name);
      wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_MODEL_LOADED));
      capgen::transcribe(m_audio_path, model, m_trx_task, update_cb);
    } catch (const std::exception &e) {
      std::cout << e.what() << std::endl;
    }
  }
  wxQueueEvent(m_widget, new wxThreadEvent(EVT_TRX_THREAD_COMPLETED));
  return (void *)0;
}


capgen::ModelDownloadDialog::ModelDownloadDialog(wxWindow *parent, const std::string &model_name)
   : wxDialog(parent, wxID_ANY, "Download model", wxDefaultPosition, wxSize(300, 300)), m_model_name(model_name),
     m_model_info(capgen::MODELS.at(model_name))
{
  // SetMinSize(wxSize(200, 500));
  wxFont text_font = wxFont(11, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_MEDIUM);
  const std::string model_name_s = std::string("Model Name: ") + m_model_name;
  wxStaticText *model_name_text = new wxStaticText(this, wxID_ANY, model_name_s);
  model_name_text->SetFont(text_font);
  const std::string model_dl_size_s = std::string("Download Size: ") + std::to_string(m_model_info.dl_size_mb) + "MB";
  wxStaticText *model_dl_size_text = new wxStaticText(this, wxID_ANY, model_dl_size_s);
  model_dl_size_text->SetFont(text_font);
  const std::string model_disk_size_s = std::string("Disk Size: ") + std::to_string(m_model_info.disk_size_mb) + "MB";
  wxStaticText *model_disk_size_text = new wxStaticText(this, wxID_ANY, model_disk_size_s);
  model_disk_size_text->SetFont(text_font);
  const std::string memory_usage_s = std::string("Memory Usage: ") + std::to_string(m_model_info.mem_usage_mb) + "MB";
  wxStaticText *memory_usage_text = new wxStaticText(this, wxID_ANY, memory_usage_s);
  memory_usage_text->SetFont(text_font);

  const std::string dl_status_s = std::string("Download Progress: (0MB/") + std::to_string(m_model_info.dl_size_mb) + "MB)";
  m_dl_status = new wxStaticText(this, wxID_ANY, dl_status_s);
  m_dl_status->SetFont(text_font.Bold());
  // m_dl_status->SetForegroundColour(wxColour(0, 250, 0));
  m_dl_gauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(280, 20));
  m_dl_btn = new wxButton(this, ID_model_dl_btn, "DOWNLOAD");
  m_dl_btn->SetFont(text_font);

  wxButton *dl_close_btn = new wxButton(this, ID_model_dl_close_btn, "CLOSE");
  dl_close_btn->SetFont(text_font.Bold());
  dl_close_btn->SetBackgroundColour(wxColour(216, 0, 0));

  m_sizer = new wxBoxSizer(wxVERTICAL);
  m_sizer->Add(model_name_text, 0, wxTOP | wxLEFT, 10);
  m_sizer->Add(model_dl_size_text, 0, wxTOP | wxLEFT, 10);
  m_sizer->Add(model_disk_size_text, 0, wxTOP | wxLEFT, 10);
  m_sizer->Add(memory_usage_text, 0, wxTOP | wxLEFT, 10);
  m_sizer->AddSpacer(60);
  m_sizer->AddSpacer(40); // Extra space that accounts for hidden status and gauge. See below.
  m_sizer->Add(m_dl_status, 0, wxLEFT, 10);
  m_sizer->Add(m_dl_gauge, 0, wxLEFT, 10);
  m_sizer->Add(m_dl_btn, 0,  wxALIGN_CENTER_HORIZONTAL | wxTOP, 10);
  m_sizer->Add(dl_close_btn, 0,  wxALIGN_CENTER_HORIZONTAL | wxTOP, 10);
  SetSizer(m_sizer);

  // Download status items hidden until user clicks download button.
  m_sizer->Hide(6); // Download status text
  m_sizer->Hide(7); // Download status gauge
  m_sizer->Hide(9); // Close button.
  m_sizer->Layout();

  Bind(wxEVT_BUTTON, &ModelDownloadDialog::on_dl_btn_click, this, ID_model_dl_btn);
  Bind(wxEVT_BUTTON, &ModelDownloadDialog::on_dl_btn_close_click, this, ID_model_dl_close_btn);
}

void capgen::ModelDownloadDialog::on_dl_btn_click(wxCommandEvent &evt) {
  m_dl_btn->Disable();
  m_sizer->Hide(5); // Extra padding space.
  m_sizer->Show((size_t)6);  // Download status text
  m_sizer->Show((size_t)7);  // Download status gauge
  m_sizer->Layout();

  download_model();
}

void capgen::ModelDownloadDialog::on_dl_btn_close_click(wxCommandEvent &evt) {
  Close();
}

void capgen::ModelDownloadDialog::download_model() {
  wxWebRequest request = wxWebSession::GetDefault().CreateRequest(this, m_model_info.url);
  // Allows progress monitoring of the download process.
  request.SetStorage(wxWebRequest::Storage::Storage_None);
  if (!request.IsOk()) {
    std::cout << "REQ is not OK.\n";
    return;
  }

  std::string out_fpath = std::string("./assets/models/") + m_model_name + ".zip";
  std::shared_ptr<wxFileOutputStream> out_stream = std::make_shared<wxFileOutputStream>(out_fpath);
  if (!out_stream->IsOk()) {
    std::cout << "[ERROR]: Failed to open output file.\n";
    return;
  }

  // Bind state event
  Bind(wxEVT_WEBREQUEST_STATE, [request, out_fpath, this](wxWebRequestEvent& evt) {
    switch (evt.GetState()) {
      case wxWebRequest::State_Completed: {
        std::cout << "Downloaded\n";
        m_dl_status->SetLabelText("Download Complete!");
        m_sizer->Hide((size_t)8);  // Download button.
        m_sizer->Show((size_t)9);  // Close button.
        std::string archive_outdir = std::string("./assets/models/") + m_model_name + "/";
        if (wxMkdir(archive_outdir)) {
          capgen::extract_downloaded_model(out_fpath, archive_outdir);
          m_dl_gauge->SetValue(100);
          wxRemoveFile(out_fpath);
        } else {
          std::cout << "Failed to make directory\n";
        }
        break;
      }
      // Request failed
      case wxWebRequest::State_Failed: {
        m_dl_btn->Enable();
        wxLogError("Error: %s", evt.GetErrorDescription());
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
