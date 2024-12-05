#include "wavemap_ros/map_operations/crop_map_operation.h"

#include <memory>
#include <string>
#include <utility>

#include <wavemap/core/map/hashed_blocks.h>
#include <wavemap/core/map/hashed_chunked_wavelet_octree.h>
#include <wavemap/core/map/hashed_wavelet_octree.h>

namespace wavemap {
DECLARE_CONFIG_MEMBERS(CropMapOperationConfig,
                      (once_every)
                      (body_frame)
                      (remove_blocks_beyond_distance));

bool CropMapOperationConfig::isValid(bool verbose) const {
  bool all_valid = true;

  all_valid &= IS_PARAM_GT(once_every, 0.f, verbose);
  all_valid &= IS_PARAM_NE(body_frame, "", verbose);
  all_valid &= IS_PARAM_GT(remove_blocks_beyond_distance, 0.f, verbose);

  return all_valid;
}

CropMapOperation::CropMapOperation(const CropMapOperationConfig& config,
                                   MapBase::Ptr occupancy_map,
                                   std::shared_ptr<TfTransformer> transformer,
                                   std::string world_frame)
    : MapOperationBase(std::move(occupancy_map)),
      config_(config.checkValid()),
      transformer_(std::move(transformer)),
      world_frame_(std::move(world_frame)) {}

bool CropMapOperation::shouldRun(const ros::Time& current_time) {
  return config_.once_every < (current_time - last_run_timestamp_).toSec();
}

void CropMapOperation::run(bool force_run) {
  const ros::Time current_time = ros::Time::now();
  if (!force_run && !shouldRun(current_time)) {
    return;
  }
  last_run_timestamp_ = current_time;

  // If the map is empty, there's no work to do
  if (occupancy_map_->empty()) {
    return;
  }

  const auto T_W_B = transformer_->lookupTransform(
      world_frame_, config_.body_frame, current_time);
  if (!T_W_B) {
    ROS_WARN_STREAM(
        "Could not look up center point for map cropping. TF lookup of "
        "body_frame \""
        << config_.body_frame << "\" w.r.t. world_frame \"" << world_frame_
        << "\" at time " << current_time << " failed.");
    return;
  }

  const IndexElement tree_height = occupancy_map_->getTreeHeight();
  const FloatingPoint min_cell_width = occupancy_map_->getMinCellWidth();
  const Point3D t_W_B = T_W_B->getPosition();

  auto indicator_fn = [tree_height, min_cell_width, &config = config_, &t_W_B](
                          const Index3D& block_index, const auto& /*block*/) {
    const auto block_node_index = OctreeIndex{tree_height, block_index};
    const auto block_aabb =
        convert::nodeIndexToAABB(block_node_index, min_cell_width);
    const FloatingPoint d_B_block = block_aabb.minDistanceTo(t_W_B);
    return config.remove_blocks_beyond_distance < d_B_block;
  };

  if (auto* hashed_wavelet_octree =
          dynamic_cast<HashedWaveletOctree*>(occupancy_map_.get());
      hashed_wavelet_octree) {
    hashed_wavelet_octree->eraseBlockIf(indicator_fn);
  } else if (auto* hashed_chunked_wavelet_octree =
                 dynamic_cast<HashedChunkedWaveletOctree*>(
                     occupancy_map_.get());
             hashed_chunked_wavelet_octree) {
    hashed_chunked_wavelet_octree->eraseBlockIf(indicator_fn);
  } else {
    ROS_WARN(
        "Map cropping is only supported for hash-based map data structures.");
  }
}
}  // namespace wavemap
