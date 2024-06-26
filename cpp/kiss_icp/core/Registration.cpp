// MIT License
//
// Copyright (c) 2022 Ignacio Vizzo, Tiziano Guadagnino, Benedikt Mersch, Cyrill
// Stachniss.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include "Registration.hpp"

#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <tbb/info.h>
#include <tbb/parallel_reduce.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <rerun.hpp>
#include <sophus/se3.hpp>
#include <sophus/so3.hpp>
#include <tuple>
#include <optional>

#include "Recording.hpp"
#include "VoxelHashMap.hpp"

namespace Eigen {
using Matrix6d = Eigen::Matrix<double, 6, 6>;
using Matrix3_6d = Eigen::Matrix<double, 3, 6>;
using Vector6d = Eigen::Matrix<double, 6, 1>;
}  // namespace Eigen
using Associations = std::vector<std::pair<Eigen::Vector3d, Eigen::Vector3d>>;
using LinearSystem = std::pair<Eigen::Matrix6d, Eigen::Vector6d>;

template <>
struct rerun::CollectionAdapter<rerun::LineStrip3D, Associations> {
    Collection<rerun::LineStrip3D> operator()(const Associations& container) {
        std::vector<rerun::LineStrip3D> strips(container.size());
        for (auto &pair : container) {
            const auto &[src_point, target_point] = pair;
            std::vector<rerun::Position3D> lines{};
            Eigen::Vector3f float_src_point = src_point.cast<float>();
            Eigen::Vector3f float_target_point = target_point.cast<float>();
            lines.push_back(rerun::Vec3D(float_src_point.data()));
            lines.push_back(rerun::Vec3D(float_target_point.data()));
            strips.push_back(rerun::LineStrip3D(lines));
        }
        return Collection<rerun::LineStrip3D>::take_ownership(std::move(strips));
    }

    Collection<rerun::LineStrip3D> operator()(const Associations&& container) {
        std::vector<rerun::LineStrip3D> strips(container.size());
        for (auto &pair : container) {
            const auto &[src_point, target_point] = pair;
            std::vector<rerun::Position3D> lines{};
            Eigen::Vector3f float_src_point = src_point.cast<float>();
            Eigen::Vector3f float_target_point = target_point.cast<float>();
            lines.push_back(rerun::Vec3D(float_src_point.data()));
            lines.push_back(rerun::Vec3D(float_target_point.data()));
            strips.push_back(rerun::LineStrip3D(lines));
        }
        return Collection<rerun::LineStrip3D>::take_ownership(std::move(strips));
    }
};

namespace {
inline double square(double x) { return x * x; }

void TransformPoints(const Sophus::SE3d &T, std::vector<Eigen::Vector3d> &points) {
    std::transform(points.cbegin(), points.cend(), points.begin(),
                   [&](const auto &point) { return T * point; });
}

using Voxel = kiss_icp::VoxelHashMap::Voxel;
std::vector<Voxel> GetAdjacentVoxels(const Voxel &voxel, int adjacent_voxels = 1) {
    std::vector<Voxel> voxel_neighborhood;
    for (int i = voxel.x() - adjacent_voxels; i < voxel.x() + adjacent_voxels + 1; ++i) {
        for (int j = voxel.y() - adjacent_voxels; j < voxel.y() + adjacent_voxels + 1; ++j) {
            for (int k = voxel.z() - adjacent_voxels; k < voxel.z() + adjacent_voxels + 1; ++k) {
                voxel_neighborhood.emplace_back(i, j, k);
            }
        }
    }
    return voxel_neighborhood;
}

std::tuple<Eigen::Vector3d, double> GetClosestNeighbor(const Eigen::Vector3d &point,
                                                       const kiss_icp::VoxelHashMap &voxel_map) {
    // Convert the point to voxel coordinates
    const auto &voxel = voxel_map.PointToVoxel(point);
    // Get nearby voxels on the map
    const auto &query_voxels = GetAdjacentVoxels(voxel);
    // Extract the points contained within the neighborhood voxels
    const auto &neighbors = voxel_map.GetPoints(query_voxels);

    // Find the nearest neighbor
    Eigen::Vector3d closest_neighbor;
    double closest_distance = std::numeric_limits<double>::max();
    std::for_each(neighbors.cbegin(), neighbors.cend(), [&](const auto &neighbor) {
        double distance = (neighbor - point).norm();
        if (distance < closest_distance) {
            closest_neighbor = neighbor;
            closest_distance = distance;
        }
    });
    return std::make_tuple(closest_neighbor, closest_distance);
}

Associations FindAssociations(const std::vector<Eigen::Vector3d> &points,
                              const kiss_icp::VoxelHashMap &voxel_map,
                              double max_correspondance_distance) {
    using points_iterator = std::vector<Eigen::Vector3d>::const_iterator;
    Associations associations;
    associations.reserve(points.size());
    associations = tbb::parallel_reduce(
        // Range
        tbb::blocked_range<points_iterator>{points.cbegin(), points.cend()},
        // Identity
        associations,
        // 1st lambda: Parallel computation
        [&](const tbb::blocked_range<points_iterator> &r, Associations res) -> Associations {
            res.reserve(r.size());
            std::for_each(r.begin(), r.end(), [&](const auto &point) {
                const auto &[closest_neighbor, distance] = GetClosestNeighbor(point, voxel_map);
                if (distance < max_correspondance_distance) {
                    res.emplace_back(point, closest_neighbor);
                }
            });
            return res;
        },
        // 2nd lambda: Parallel reduction
        [](Associations a, const Associations &b) -> Associations {
            a.insert(a.end(),                              //
                     std::make_move_iterator(b.cbegin()),  //
                     std::make_move_iterator(b.cend()));
            return a;
        });

    return associations;
}


LinearSystem BuildLinearSystem(const Associations &associations, double kernel) {
    auto compute_jacobian_and_residual = [](auto association) {
        const auto &[source, target] = association;
        const Eigen::Vector3d residual = source - target;
        Eigen::Matrix3_6d J_r;
        J_r.block<3, 3>(0, 0) = Eigen::Matrix3d::Identity();
        J_r.block<3, 3>(0, 3) = -1.0 * Sophus::SO3d::hat(source);
        return std::make_tuple(J_r, residual);
    };

    auto sum_linear_systems = [](LinearSystem a, const LinearSystem &b) {
        a.first += b.first;
        a.second += b.second;
        return a;
    };

    auto GM_weight = [&](double residual2) { return square(kernel) / square(kernel + residual2); };

    using associations_iterator = Associations::const_iterator;
    const auto &[JTJ, JTr] = tbb::parallel_reduce(
        // Range
        tbb::blocked_range<associations_iterator>{associations.cbegin(), associations.cend()},
        // Identity
        LinearSystem(Eigen::Matrix6d::Zero(), Eigen::Vector6d::Zero()),
        // 1st Lambda: Parallel computation
        [&](const tbb::blocked_range<associations_iterator> &r, LinearSystem J) -> LinearSystem {
            return std::transform_reduce(
                r.begin(), r.end(), J, sum_linear_systems, [&](const auto &association) {
                    const auto &[J_r, residual] = compute_jacobian_and_residual(association);
                    const double w = GM_weight(residual.squaredNorm());
                    return LinearSystem(J_r.transpose() * w * J_r,        // JTJ
                                        J_r.transpose() * w * residual);  // JTr
                });
        },
        // 2nd Lambda: Parallel reduction of the private Jacboians
        sum_linear_systems);

    return {JTJ, JTr};
}
}  // namespace

namespace kiss_icp {

Registration::Registration(int max_num_iteration, double convergence_criterion, int max_num_threads)
    : max_num_iterations_(max_num_iteration),
      convergence_criterion_(convergence_criterion),
      // Only manipulate the number of threads if the user specifies something greater than 0
      max_num_threads_(max_num_threads > 0 ? max_num_threads : tbb::info::default_concurrency()) {
    // This global variable requires static duration storage to be able to manipulate the max
    // concurrency from TBB across the entire class
    static const auto tbb_control_settings = tbb::global_control(
        tbb::global_control::max_allowed_parallelism, static_cast<size_t>(max_num_threads_));
}

Sophus::SE3d Registration::AlignPointsToMap(const std::vector<Eigen::Vector3d> &frame,
                                            const VoxelHashMap &voxel_map,
                                            const Sophus::SE3d &initial_guess,
                                            double max_correspondence_distance,
                                            double kernel) {
    if (voxel_map.Empty()) return initial_guess;

    // Equation (9)
    std::vector<Eigen::Vector3d> source = frame;
    TransformPoints(initial_guess, source);

    // max_correspondence_distance = 1000.0;


    // ICP-loop
    Sophus::SE3d T_icp = Sophus::SE3d();
    int j{0};
    for (; j < max_num_iterations_; ++j) {
        // Equation (10)
        const auto associations = FindAssociations(source, voxel_map, max_correspondence_distance);

        if (rr_rec != nullptr) {
            rr_rec->set_time_sequence("step", algo_step);
            if (j == 0) {
                algo_step += 7;
            }
            algo_step += 1;
            rr_rec->log("world/icp/source", rerun::Points3D(source)
                                                .with_colors(rerun::Color(0, 180, 180))
                                                .with_radii(0.25f));

            rr_rec->log(
                "world/icp/correspondences",
                rerun::LineStrips3D(associations).with_colors(rerun::Color(0, 255, 0)).with_radii(0.15f));

            auto cur_transform = T_icp * initial_guess;
            Sophus::Vector3f float_trans = cur_transform.translation().cast<float>();
            Sophus::Matrix3f float_rot = cur_transform.rotationMatrix().cast<float>();
            rr_rec->log("world/icp/pose", rerun::Transform3D(rerun::Vec3D(float_trans.data()),
                                                             rerun::Mat3x3(float_rot.data())));
        }

        // Equation (11)
        const auto &[JTJ, JTr] = BuildLinearSystem(associations, kernel);
        const Eigen::Vector6d dx = JTJ.ldlt().solve(-JTr);
        const Sophus::SE3d estimation = Sophus::SE3d::exp(dx);
        // Equation (12)
        TransformPoints(estimation, source);
        // Update iterations
        T_icp = estimation * T_icp;
        // Termination criteria
        if (dx.norm() < convergence_criterion_) break;
    }
    // Spit the final transformation
    return T_icp * initial_guess;
}

}  // namespace kiss_icp
