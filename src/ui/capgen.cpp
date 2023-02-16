// Included in the top so that it includes libTorch first.
#include "transcribe.h"
#include <wx/wxprec.h>

// For the platforms without support for precompiled headers.
#ifndef WX_PRECOMP
  #include <wx/wx.h>
#endif

#include <functional>


class CapgenApplication : public wxApp {
public:
  virtual bool OnInit();
};

// Registers our application so that the main function calls into our
// entry point.
wxIMPLEMENT_APP(CapgenApplication);


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


class TranscriptionPanel : public wxPanel {
public:
  wxStaticText *m_progbar_text;
  wxGauge *m_progbar;
  wxStaticText *m_status_text;

  TranscriptionPanel(wxScrolledWindow *parent_window, const wxString &path);
  void update_progbar(int value);
  void transcription_complete();
  void OnThreadUpdate(TranscriptionUpdateEvent& event);
  void OnThreadCompletion(wxThreadEvent& event);
};


// We need to create main window to render when we initialize.
class MainWindow : public wxFrame {
public:
  wxScrolledWindow *m_content_window;
  wxSizer *m_content_sizer = new wxBoxSizer(wxVERTICAL);
  capgen::Whisper m_whisper = capgen::Whisper();

  MainWindow();
  TranscriptionPanel *add_transcription_panel(wxString &path);
  void transcribe(TranscriptionPanel *panel, wxString &path);

private:
  // Event handlers, which do not need to be private. They receive event
  // as their parameter.
  void OnExit(wxCommandEvent& event);
  void OnAbout(wxCommandEvent& event);
  void OnFileAdd(wxCommandEvent& event);
};


// Called upon startup.
bool CapgenApplication::OnInit() {
  MainWindow *main_window = new MainWindow();
  main_window->Show();
  return true;
}

enum {
  ID_file_add = 1
};


wxDEFINE_EVENT(wxEVT_COMMAND_THREAD_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_THREAD_UPDATE, TranscriptionUpdateEvent);


TranscriptionPanel::TranscriptionPanel(wxScrolledWindow *parent_window, const wxString &path)
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

  // Provide event bindings for thread
  Bind(wxEVT_COMMAND_THREAD_UPDATE, &TranscriptionPanel::OnThreadUpdate, this, wxID_ANY);
  Bind(wxEVT_COMMAND_THREAD_COMPLETED, &TranscriptionPanel::OnThreadCompletion, this, wxID_ANY);
}

void TranscriptionPanel::OnThreadCompletion(wxThreadEvent &event) {
  wxFrame *frame = (wxFrame *)(this->GetGrandParent());
  if (frame)
    frame->GetToolBar()->EnableTool(ID_file_add, true);
  transcription_complete();
}

void TranscriptionPanel::OnThreadUpdate(TranscriptionUpdateEvent &event) {
  update_progbar(event.get_progress());
}

void TranscriptionPanel::update_progbar(int percentage) {
  m_progbar->SetValue(percentage);
  std::string label = std::string(" ") + std::to_string(percentage) + std::string("%");
  m_progbar_text->SetLabelText(label);
}

void TranscriptionPanel::transcription_complete() {
  m_status_text->SetLabelText("Status: Transcription complete!");
  m_status_text->SetForegroundColour(wxColour(0, 255, 0));
}

TranscriptionPanel *MainWindow::add_transcription_panel(wxString &path) {
  TranscriptionPanel *panel = new TranscriptionPanel(m_content_window, path);
  m_content_sizer->PrependSpacer(20);
  m_content_sizer->Prepend(panel, 0, wxEXPAND);
  // Force the sizer to re-render.
  m_content_sizer->Layout();
  m_content_sizer->FitInside(m_content_window);
  return panel;
}


class TranscriptionThread : public wxThread {
public:
  std::string m_path;
  capgen::Whisper *m_whisper;
  TranscriptionPanel *m_panel;

  TranscriptionThread(std::string &path, capgen::Whisper *whisper, TranscriptionPanel *panel)
    : wxThread(wxTHREAD_DETACHED), m_path(path), m_whisper(whisper), m_panel(panel)
  {
  }

  ~TranscriptionThread() {
    std::cout << "[UI]: Deleting Transcription thread" << std::endl;
  }

  void *Entry() {
    std::function<void(int)> update_cb = [this](int progress) {
      wxQueueEvent(m_panel, new TranscriptionUpdateEvent(wxEVT_COMMAND_THREAD_UPDATE, wxID_ANY, progress));
    };

    if (!TestDestroy()) {
      capgen::transcribe(m_path, *m_whisper, update_cb);
    }
    wxQueueEvent(m_panel, new wxThreadEvent(wxEVT_COMMAND_THREAD_COMPLETED));
    return (void *)0;
  }
};


void MainWindow::transcribe(TranscriptionPanel *panel, wxString &path) {
  std::string path_string = path.ToStdString();
  TranscriptionThread *t_thread = new TranscriptionThread(path_string, &m_whisper, panel);
  if (t_thread->Run() != wxTHREAD_NO_ERROR) {
    wxLogError("Transcription thread failed to run");
    delete t_thread;
  }
  std::cout << "[UI]: Run Transcription thread" << std::endl;
}

MainWindow::MainWindow()
  : wxFrame(NULL, wxID_ANY, "Capgen", wxDefaultPosition, wxSize(600, 650))
{
  wxMenu *menu_home = new wxMenu;
  menu_home->Append(wxID_EXIT, "Exit", "Exit the program");
  wxMenu *menu_help = new wxMenu;
  menu_help->Append(wxID_ABOUT);
  wxMenuBar *menu_bar = new wxMenuBar;
  menu_bar->Append(menu_home, "&File");
  menu_bar->Append(menu_help, "&Help");
  SetMenuBar(menu_bar);

  wxBitmap add_file("add.svg", wxBITMAP_TYPE_ANY);
  // The parameter `256` exists only because it allows the toolbar icons text to
  // appear. It should be removed in the future once a better way is found.
  wxToolBar *toolbar = CreateToolBar(256);
  toolbar->AddTool(ID_file_add, "Transcribe file", add_file);
  toolbar->Realize();

  // Manages all the content.
  m_content_window = new wxScrolledWindow(this);
  m_content_window->SetBackgroundColour(wxColour(60, 60, 60));
  m_content_window->AlwaysShowScrollbars();
  m_content_window->SetSizer(m_content_sizer);
  m_content_window->SetScrollbars(0, 20, 0, 100);
  
  Centre();

  Bind(wxEVT_MENU, &MainWindow::OnAbout, this, wxID_ABOUT);
  Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
  Bind(wxEVT_TOOL, &MainWindow::OnFileAdd, this, ID_file_add);
}

void MainWindow::OnFileAdd(wxCommandEvent& event) {
  wxFileDialog file_dialog(
    this, "Select file to transcribe", "", "", 
    "Media files (*.mp3)|*.mp3|(*.m4a)|*.m4a|(*.mp4)|*.mp4|(*.mkv)|*.mkv", 
    wxFD_OPEN | wxFD_FILE_MUST_EXIST,
    wxDefaultPosition,
    wxSize(400, 300)
    );
  if (file_dialog.ShowModal() == wxID_CANCEL)
    return;
  auto path = file_dialog.GetPath();
  TranscriptionPanel *panel = add_transcription_panel(path);
  GetToolBar()->EnableTool(ID_file_add, false);
  transcribe(panel, path);
}

void MainWindow::OnExit(wxCommandEvent& event) {
  Close(true);
}

void MainWindow::OnAbout(wxCommandEvent& event) {
  wxMessageBox(
    "Capgen is a free automatic captions generator for audio and videos.",
    "About capgen",
    wxOK | wxICON_INFORMATION
  );
}
