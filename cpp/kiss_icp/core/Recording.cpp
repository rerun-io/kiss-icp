#include <rerun.hpp>
#include <memory>
#include <string>
#include <utility>

namespace kiss_icp {

std::unique_ptr<rerun::RecordingStream> rr_rec = nullptr;

void init_rr_rec(std::string app_id, std::string recording_id) {
    auto local_rec = std::make_unique<rerun::RecordingStream>(rerun::RecordingStream(app_id, recording_id));
    local_rec->connect().exit_on_failure();
    rr_rec = std::move(local_rec);    
}

}
