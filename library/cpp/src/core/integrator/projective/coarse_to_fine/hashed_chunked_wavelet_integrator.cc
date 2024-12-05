#include "wavemap/core/integrator/projective/coarse_to_fine/hashed_chunked_wavelet_integrator.h"

#include <algorithm>
#include <memory>
#include <stack>
#include <utility>

#include <wavemap/core/utils/profile/profiler_interface.h>

namespace wavemap {
void HashedChunkedWaveletIntegrator::updateMap() {
  ProfilerZoneScoped;
  // Update the range image intersector
  {
    ProfilerZoneScopedN("updateRangeImageIntersector");
    range_image_intersector_ = std::make_shared<RangeImageIntersector>(
        posed_range_image_, projection_model_, *measurement_model_,
        config_.min_range, config_.max_range);
  }

  // Find all the indices of blocks that need updating
  BlockList blocks_to_update;
  {
    ProfilerZoneScopedN("selectBlocksToUpdate");
    const auto [fov_min_idx, fov_max_idx] =
        getFovMinMaxIndices(posed_range_image_->getOrigin());
    for (const auto& block_index :
         Grid(fov_min_idx.position, fov_max_idx.position)) {
      recursiveTester(OctreeIndex{fov_min_idx.height, block_index},
                      blocks_to_update);
    }
  }

  // Make sure the to-be-updated blocks are allocated
  for (const auto& block_index : blocks_to_update) {
    occupancy_map_->getOrAllocateBlock(block_index);
  }

  // Update it with the threadpool
  for (const auto& block_index : blocks_to_update) {
    thread_pool_->add_task([this, block_index]() {
      if (auto* block = occupancy_map_->getBlock(block_index); block) {
        updateBlock(*block, block_index);
      }
    });
  }
  thread_pool_->wait_all();
}

std::pair<OctreeIndex, OctreeIndex>
HashedChunkedWaveletIntegrator::getFovMinMaxIndices(
    const Point3D& sensor_origin) const {
  const int height = 1 + std::max(static_cast<int>(std::ceil(std::log2(
                                      config_.max_range / min_cell_width_))),
                                  tree_height_);
  const OctreeIndex fov_min_idx = convert::indexAndHeightToNodeIndex<3>(
      convert::pointToFloorIndex<3>(
          sensor_origin - Vector3D::Constant(config_.max_range),
          min_cell_width_inv_) -
          occupancy_map_->getBlockSize(),
      height);
  const OctreeIndex fov_max_idx = convert::indexAndHeightToNodeIndex<3>(
      convert::pointToCeilIndex<3>(
          sensor_origin + Vector3D::Constant(config_.max_range),
          min_cell_width_inv_) +
          occupancy_map_->getBlockSize(),
      height);
  return {fov_min_idx, fov_max_idx};
}

void HashedChunkedWaveletIntegrator::updateBlock(
    HashedChunkedWaveletOctree::Block& block,
    const HashedChunkedWaveletOctree::BlockIndex& block_index) {
  ProfilerZoneScoped;
  block.setNeedsPruning();
  block.setLastUpdatedStamp();

  bool block_needs_thresholding = block.getNeedsThresholding();
  const OctreeIndex root_node_index{tree_height_, block_index};
  updateNodeRecursive(block.getRootNode(), root_node_index,
                      block.getRootScale(), block_needs_thresholding);
  block.setNeedsThresholding(block_needs_thresholding);
}

void HashedChunkedWaveletIntegrator::updateNodeRecursive(  // NOLINT
    HashedChunkedWaveletIntegrator::OctreeType::NodeRefType node,
    const OctreeIndex& node_index, FloatingPoint& node_value,
    bool& block_needs_thresholding) {
  // Decompress child values
  auto& node_details = node.data();
  auto child_values = HashedChunkedWaveletOctreeBlock::Transform::backward(
      {node_value, node_details});

  // Handle each child
  for (NdtreeIndexRelativeChild relative_child_idx = 0;
       relative_child_idx < OctreeIndex::kNumChildren; ++relative_child_idx) {
    const OctreeIndex child_index =
        node_index.computeChildIndex(relative_child_idx);
    FloatingPoint& child_value = child_values[relative_child_idx];

    // Test whether it is fully occupied; free or unknown; or fully unknown
    const AABB<Point3D> W_child_aabb =
        convert::nodeIndexToAABB(child_index, min_cell_width_);
    const UpdateType update_type =
        range_image_intersector_->determineUpdateType(
            W_child_aabb, posed_range_image_->getRotationMatrixInverse(),
            posed_range_image_->getOrigin());

    // If we're fully in unknown space,
    // there's no need to evaluate this node or its children
    if (update_type == UpdateType::kFullyUnobserved) {
      continue;
    }

    // We can also stop here if the cell will result in a free space update
    // (or zero) and the map is already saturated free
    if (update_type != UpdateType::kPossiblyOccupied &&
        child_value < min_log_odds_shrunk_) {
      continue;
    }

    // Test if the worst-case error for the intersection type at the current
    // resolution falls within the acceptable approximation error
    const FloatingPoint child_width = W_child_aabb.width<0>();
    const Point3D W_child_center =
        W_child_aabb.min + Vector3D::Constant(child_width / 2.f);
    const Point3D C_child_center =
        posed_range_image_->getPoseInverse() * W_child_center;
    const FloatingPoint d_C_child =
        projection_model_->cartesianToSensorZ(C_child_center);
    const FloatingPoint bounding_sphere_radius =
        kUnitCubeHalfDiagonal * child_width;
    if (measurement_model_->computeWorstCaseApproximationError(
            update_type, d_C_child, bounding_sphere_radius) <
        config_.termination_update_error) {
      const FloatingPoint sample = computeUpdate(C_child_center);
      child_value += sample;
      block_needs_thresholding = true;
      continue;
    }

    // Since the approximation error would still be too big, refine
    auto child_node = node.getOrAllocateChild(relative_child_idx);
    auto& child_details = child_node.data();

    // If we're at the leaf level, directly compute the update
    if (child_index.height <= termination_height_ + 1) {
      updateLeavesBatch(child_index, child_value, child_details);
    } else {
      // Otherwise, recurse
      DCHECK_GE(child_index.height, 0);
      updateNodeRecursive(child_node, child_index, child_value,
                          block_needs_thresholding);
    }
  }

  // Compress
  const auto [new_value, new_details] =
      HashedChunkedWaveletOctreeBlock::Transform::forward(child_values);
  node_details = new_details;
  node_value = new_value;
}
}  // namespace wavemap
