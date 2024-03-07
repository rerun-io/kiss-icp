#pragma once

#include <rerun.hpp>
#include <memory>
#include <string>

namespace kiss_icp {

extern std::unique_ptr<rerun::RecordingStream> rr_rec;

void init_rr_rec(std::string app_id, std::string recording_id);

}