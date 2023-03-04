#include "utils.h"
#include "core/log.h"

#include <filesystem>

#include "wx/archive.h"
#include "wx/wfstream.h"

capgen::ModelsManager::ModelsManager() {
  register_downloaded_models();
}

std::shared_ptr<capgen::Whisper> capgen::ModelsManager::get_model(std::string &name, capgen::ModelType model_type) {
  for (const auto &model_ptr : m_loaded_models)
    if (model_ptr->m_name == name && model_ptr->m_model_type == model_type) {
      return model_ptr;
    }
  
  // If model is not loaded, load it into memory.
  m_loaded_models.push_back(std::make_shared<capgen::Whisper>(name, model_type));
  m_registered_models.push_back(name);
  return m_loaded_models.back();
}

const std::vector<std::string> &capgen::ModelsManager::get_registered_models() const {
  return m_registered_models;
}

int capgen::ModelsManager::get_registered_models_length() const {
  return m_registered_models.size();
}

std::string capgen::ModelsManager::get_default_model_name() const {
  std::string default_model = "base";
  if (model_is_registered(default_model))
    return default_model;
  if (m_registered_models.size() == 0)
    return "";
  return m_registered_models.front();
}

bool capgen::ModelsManager::model_is_registered(const std::string& name) const {
  for (const auto &model_name : m_registered_models)
    if (model_name == name)
      return true;
  return false;
}

void capgen::ModelsManager::register_downloaded_models() {
  const std::string default_model_archive = "./assets/models/tiny.zip";
  for (const auto &entry : std::filesystem::directory_iterator(m_models_basepath))
    if (entry.is_directory()) {
      const std::string model_dir = entry.path().string();
      m_registered_models.push_back(std::string(model_dir.begin() + m_models_basepath.length(), model_dir.end()));
    }
}

void capgen::ModelsManager::reload_registered_models() {
  for (const auto &entry : std::filesystem::directory_iterator(m_models_basepath))
  if (entry.is_directory()) {
    const std::string model_dir = entry.path().string();
    if (!model_is_registered(model_dir))
      m_registered_models.push_back(std::string(model_dir.begin() + m_models_basepath.length(), model_dir.end()));
  }
}


int capgen::b_to_mb(int bytes) {
  return (int)((float)bytes / 1000000.0f);
}

float capgen::mb_to_b(float mb) {
  return mb * 1000000.0f;
}

static bool copy_stream_data(wxInputStream& input_stream, wxOutputStream& output_stream, wxFileOffset size) {
  uint8_t *buf[128 * 1024];
  int read_size = 128 * 1024;
  wxFileOffset copied_data = 0;
  for (;;) {
    if (size != -1 && copied_data + read_size > size)
      read_size = size - copied_data;
    input_stream.Read(buf, read_size);
    size_t actually_read = input_stream.LastRead();
    output_stream.Write(buf, actually_read);
    if (output_stream.LastWrite() != actually_read) {
      CG_LOG_MERROR("Archive decompressor failed to write sata to output stream.");
      return false;
    }
    if (size == -1) {
      if (input_stream.Eof())
        break;
    } else {
      copied_data += actually_read;
      if (copied_data >= size)
        break;
    }
  }
  return true;
}

void capgen::extract_downloaded_model(const std::string& archive_path, const std::string &archive_outdir) {
  auto factory = wxArchiveClassFactory::Find(archive_path, wxSTREAM_FILEEXT);
  if (!factory) {
    CG_LOG_ERROR("Archive factory not found for file: %s", archive_path.c_str());
    return;
  }
  wxFileInputStream file_input_stream(archive_path);
  if (!file_input_stream.IsOk()) {
    CG_LOG_ERROR("Archive decompressor failed to open input file: %s", archive_path.c_str());
    return;
  }
  std::unique_ptr<wxArchiveInputStream> archive_stream(factory->NewStream(file_input_stream));
  CG_LOG_INFO("Extracting model from file: %s", archive_path.c_str());
  for (wxArchiveEntry* entry = archive_stream->GetNextEntry(); entry; entry = archive_stream->GetNextEntry()) {
    CG_LOG_INFO("Extracting file: %s", entry->GetName().ToStdString().c_str());
    std::string output_fname = archive_outdir + entry->GetName().ToStdString();
    wxTempFileOutputStream output_file_stream(output_fname);
    if (!copy_stream_data(*archive_stream, output_file_stream, entry->GetSize())) {
      CG_LOG_ERROR("Archive decompressor failed to copy to output file: %s", output_fname.c_str());
      return;
    }
    output_file_stream.Commit();
  }
  CG_LOG_INFO("Extracted to %s", archive_outdir.c_str());
}