#pragma once

#include "core/model.h"

#include <string>
#include <memory>
#include <vector>


namespace capgen {

/// @brief Performs model management tasks during runtime which include:
///  ~ Storing a list of downloaded models.
///  ~ Loading the downloaded models from disk to memory for inference.
class ModelsManager {
public:
  ModelsManager();
  std::shared_ptr<capgen::Whisper> get_model(std::string &name, capgen::ModelType model_type);
  const std::vector<std::string> &get_registered_models() const;
  bool model_is_registered(const std::string &name) const;
  void reload_registered_models();
  void register_downloaded_models();
  int get_registered_models_length() const;
  std::string get_default_model_name() const;

private:
  const std::string m_models_basepath =  "./assets/models/";
  std::vector<std::string> m_registered_models;
  // Models loaded in memory.
  std::vector<std::shared_ptr<capgen::Whisper>> m_loaded_models;
};


int b_to_mb(int bytes);
float mb_to_b(float mb);

/// @brief Extracts downloaded models in zip format.
/// @param archive_fname Path to the the archive.
/// @param archive_outdir Directory to extract in.
void extract_downloaded_model(const std::string& archive_path, const std::string &archive_outdir);

} // namespace capgen