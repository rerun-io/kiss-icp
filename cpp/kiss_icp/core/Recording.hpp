#pragma once

#include <rerun.hpp>
#include <memory>
#include <string>
#include <cstdint>
#include <vector>

namespace kiss_icp {

extern std::unique_ptr<rerun::RecordingStream> rr_rec;
extern int64_t algo_step;

void init_rr_rec(std::string app_id, std::string recording_id);
void set_algo_step(int64_t n);
int64_t get_algo_step();

}

template <>
struct rerun::CollectionAdapter<rerun::Position3D, std::vector<Eigen::Vector3d>> {

    Collection<rerun::Position3D> operator()(const std::vector<Eigen::Vector3d>& container) {
        std::vector<rerun::Position3D> positions(container.size());
        for (auto &point : container) {
            Eigen::Vector3f pointf = point.cast<float>();
            positions.push_back(rerun::Position3D(rerun::Vec3D(pointf.data())));
        }
        return Collection<rerun::Position3D>::take_ownership(std::move(positions));
    }

    // Do a full copy for temporaries (otherwise the data might be deleted when the temporary is destroyed).
    Collection<rerun::Position3D> operator()(std::vector<Eigen::Vector3d>&& container) {
        std::vector<rerun::Position3D> positions(container.size());
        for (auto &point : container) {
            Eigen::Vector3f pointf = point.cast<float>();
            positions.push_back(rerun::Position3D(rerun::Vec3D(pointf.data())));
        }
        return Collection<rerun::Position3D>::take_ownership(std::move(positions));
    }
};
