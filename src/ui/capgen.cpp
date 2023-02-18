// Included in the top so that it includes libTorch first.
#include "capgen.h"


// Registers our application's entry point.
wxIMPLEMENT_APP(capgen::Application);


// Called upon startup.
bool capgen::Application::OnInit() {
  capgen::MainWindow *main_window = new capgen::MainWindow();
  main_window->Show();
  return true;
}

enum {
  ID_audio_add = 1,
  ID_video_add
};

capgen::MainWindow::MainWindow()
  : wxFrame(NULL, wxID_ANY, "Capgen", wxDefaultPosition, wxSize(600, 650)), m_content_sizer(new wxBoxSizer(wxVERTICAL))
{
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
  wxBitmap audio_icon("./assets/audio-icon.svg", wxBITMAP_TYPE_ANY);
  wxBitmap video_icon("./assets/video-icon.svg", wxBITMAP_TYPE_ANY);
  // The parameter `256` exists only because it allows the toolbar icons text to
  // appear. It should be removed in the future once a better way is found.
  wxToolBar *toolbar = CreateToolBar(256);
  toolbar->AddTool(ID_audio_add, "Transcribe audio", audio_icon);
  toolbar->AddTool(ID_video_add, "Transcribe video", video_icon);
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
  Bind(wxEVT_CLOSE_WINDOW, &MainWindow::on_close, this, wxID_EXIT);
}

void capgen::MainWindow::on_audio_add(wxCommandEvent& event) {
  wxFileDialog file_dialog(
    this, "Select audio file to transcribe", "", "", 
    "Audio files (*.mp3)|*.mp3", 
    wxFD_OPEN | wxFD_FILE_MUST_EXIST,
    wxDefaultPosition,
    wxSize(400, 300)
    );
  if (file_dialog.ShowModal() == wxID_CANCEL)
    return;
  auto path = file_dialog.GetPath();
  capgen::TranscriptionWidget *widget = add_transcription_widget(path);
  GetToolBar()->EnableTool(ID_audio_add, false);
  GetToolBar()->EnableTool(ID_video_add, false);
  transcribe(widget, path);
}

void capgen::MainWindow::on_video_add(wxCommandEvent& event) {
  wxFileDialog file_dialog(
    this, "Select video file to transcribe", "", "", 
    "Video files (*.m4a)|*.m4a|(*.mp4)|*.mp4|(*.mkv)|*.mkv", 
    wxFD_OPEN | wxFD_FILE_MUST_EXIST,
    wxDefaultPosition,
    wxSize(400, 300)
    );
  if (file_dialog.ShowModal() == wxID_CANCEL)
    return;
  auto path = file_dialog.GetPath();
  capgen::TranscriptionWidget *widget = add_transcription_widget(path);
  GetToolBar()->EnableTool(ID_audio_add, false);
  GetToolBar()->EnableTool(ID_video_add, false);
  transcribe(widget, path);
}

capgen::TranscriptionWidget *capgen::MainWindow::add_transcription_widget(wxString &path) {
  capgen::TranscriptionWidget *widget = new capgen::TranscriptionWidget(m_content_window, path);
  m_content_sizer->PrependSpacer(20);
  m_content_sizer->Prepend(widget, 0, wxEXPAND);
  // Force the sizer to re-render.
  m_content_sizer->Layout();
  m_content_sizer->FitInside(m_content_window);
  return widget;
}

void capgen::MainWindow::transcribe(capgen::TranscriptionWidget *widget, wxString &path) {
  std::string path_string = path.ToStdString();
  // Load the model on-demand
  if (m_whisper_ptr == nullptr)
    m_whisper_ptr = new capgen::Whisper();
  TranscriptionThread *t_thread = new TranscriptionThread(path_string, m_whisper_ptr, widget);
  if (t_thread->Run() != wxTHREAD_NO_ERROR) {
    wxLogError("[UI]: Transcription thread failed to run");
    delete t_thread;
  }
  std::cout << "[UI]: Run Transcription thread" << std::endl;
}

void capgen::MainWindow::on_exit(wxCommandEvent& event) {
  Close(true);
}

void capgen::MainWindow::on_about(wxCommandEvent& event) {
  wxMessageBox(
    "Capgen is a free automatic captions generator for audio and videos.",
    "About capgen",
    wxOK | wxICON_INFORMATION
  );
}

void capgen::MainWindow::on_close(wxCloseEvent& event) {
  if (m_whisper_ptr)
    delete m_whisper_ptr;
}


wxDEFINE_EVENT(wxEVT_COMMAND_TRANSCRIPTION_THREAD_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_TRANSCRIPTION_THREAD_UPDATE, capgen::TranscriptionUpdateEvent);


capgen::TranscriptionWidget::TranscriptionWidget(wxScrolledWindow *parent_window, const wxString &path)
  : wxPanel(parent_window, wxID_ANY)
{
  this->SetBackgroundColour(wxColour(40, 40, 40));
  wxBoxSizer *panel_sizer = new wxBoxSizer(wxVERTICAL);
  wxFont text_font = wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
  wxString file_desc = wxString("File: ") + path;
  wxStaticText *file_text = new wxStaticText(this, wxID_ANY, file_desc);
  file_text->SetFont(text_font);
  wxStaticText *language_text = new wxStaticText(this, wxID_ANY, "Language: English");
  language_text->SetFont(text_font);
  m_status_text = new wxStaticText(this, wxID_ANY, "Status: Transcribing ...");
  m_status_text->SetFont(text_font);

  wxSizer *progbar_sizer = new wxBoxSizer(wxHORIZONTAL);
  m_progbar_text = new wxStaticText(this, wxID_ANY, " 0%", wxDefaultPosition, wxSize(50, 20));
  m_progbar_text->SetFont(text_font);
  m_progbar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(400, 20), wxGA_SMOOTH | wxGA_HORIZONTAL);
  progbar_sizer->Add(m_progbar, 0);
  progbar_sizer->Add(m_progbar_text, 0, wxTOP, 8);

  panel_sizer->Add(file_text, 0, wxGROW | wxLEFT | wxTOP, 15);
  panel_sizer->AddSpacer(9);
  panel_sizer->Add(language_text, 0, wxGROW | wxLEFT, 15);
  panel_sizer->AddSpacer(9);
  panel_sizer->Add(m_status_text, 0, wxGROW | wxLEFT, 15);
  panel_sizer->AddSpacer(9);
  panel_sizer->Add(progbar_sizer, 0, wxGROW | wxLEFT, 15);
  panel_sizer->AddSpacer(20);
  this->SetSizer(panel_sizer);

  // Provide event bindings for thread events.
  Bind(wxEVT_COMMAND_TRANSCRIPTION_THREAD_UPDATE,
       &TranscriptionWidget::on_transcription_thread_update,
       this,
       wxID_ANY
  );
  Bind(wxEVT_COMMAND_TRANSCRIPTION_THREAD_COMPLETED,
       &TranscriptionWidget::on_transcription_thread_completion,
       this,
       wxID_ANY
  );
}

void capgen::TranscriptionWidget::on_transcription_thread_completion(wxThreadEvent &event) {
  wxFrame *frame = (wxFrame *)(this->GetGrandParent());
  if (frame) {
    frame->GetToolBar()->EnableTool(ID_audio_add, true);
    frame->GetToolBar()->EnableTool(ID_video_add, true);
  }
  transcription_complete();
}

void capgen::TranscriptionWidget::on_transcription_thread_update(TranscriptionUpdateEvent &event) {
  update_progbar(event.get_progress());
}

void capgen::TranscriptionWidget::update_progbar(int percentage) {
  m_progbar->SetValue(percentage);
  std::string label = std::string(" ") + std::to_string(percentage) + std::string("%");
  m_progbar_text->SetLabelText(label);
}

void capgen::TranscriptionWidget::transcription_complete() {
  m_status_text->SetLabelText("Status: Transcription complete!");
  m_status_text->SetForegroundColour(wxColour(0, 255, 0));
}

capgen::TranscriptionThread::TranscriptionThread(std::string &path, capgen::Whisper *whisper_ptr, TranscriptionWidget *widget)
  : wxThread(wxTHREAD_DETACHED), m_path(path), m_whisper_ptr(whisper_ptr), m_widget(widget)
{}


capgen::TranscriptionThread::~TranscriptionThread() {
  std::cout << "[UI]: Deleting Transcription thread" << std::endl;
}

// Code run by transcription thread. The transcription thread does not run any code
// that directly changes the UI. It is not safe to change UI from any thread other
// than the main thread. So, instead of interfering with UI, the transcription worker
// thread queue's the necessary events that then the main thread receives and updates
// the UI. The thread queing code is implemented by wxWidgets to be thread-safe. A
// potential source of danger is the fact that we pass a pointer to the model from
// the main thread to the worker thread and we access the model object from the worker
// thread without locks. But since we never access the model from the main thread, that
// should not be a big issue for now. Although, if one kills the application while the
// worker thread is running, we could delete the model while the worker thread is using it.
// The application would crash but for now, we can assume that our users are sane. 
void *capgen::TranscriptionThread::Entry() {
  std::function<void(int)> update_cb = [this](int progress) {
    wxQueueEvent(m_widget, new TranscriptionUpdateEvent(wxEVT_COMMAND_TRANSCRIPTION_THREAD_UPDATE, wxID_ANY, progress));
  };

  if (!TestDestroy()) {
    capgen::transcribe(m_path, *m_whisper_ptr, update_cb);
  }
  wxQueueEvent(m_widget, new wxThreadEvent(wxEVT_COMMAND_TRANSCRIPTION_THREAD_COMPLETED));
  return (void *)0;
}
