#include <rerun.hpp>
#include <memory>
#include <string>
#include <utility>

namespace kiss_icp {

std::unique_ptr<rerun::RecordingStream> rr_rec = nullptr;
int64_t algo_step = 0;

void init_rr_rec(std::string app_id, std::string recording_id) {
    auto local_rec = std::make_unique<rerun::RecordingStream>(rerun::RecordingStream(app_id, recording_id));
    local_rec->connect().exit_on_failure();
    rr_rec = std::move(local_rec);    
}

void set_algo_step(int64_t n) {
    algo_step = n;
}
int64_t get_algo_step() {
    return algo_step;
}

}
