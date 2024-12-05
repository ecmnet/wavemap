#include "wavemap/core/integrator/projective/projective_integrator.h"

#include <wavemap/core/utils/data/eigen_checks.h>
#include <wavemap/core/utils/profile/profiler_interface.h>

namespace wavemap {
DECLARE_CONFIG_MEMBERS(ProjectiveIntegratorConfig,
                      (min_range)
                      (max_range)
                      (max_update_resolution)
                      (termination_update_error));

bool ProjectiveIntegratorConfig::isValid(bool verbose) const {
  bool is_valid = true;

  is_valid &= IS_PARAM_GT(min_range, 0.f, verbose);
  is_valid &= IS_PARAM_GT(max_range, 0.f, verbose);
  is_valid &= IS_PARAM_LT(min_range, max_range, verbose);
  is_valid &= IS_PARAM_GE(max_update_resolution, 0.f, verbose);
  is_valid &= IS_PARAM_GT(termination_update_error, 0.f, verbose);

  return is_valid;
}

void ProjectiveIntegrator::integrate(const PosedPointcloud<>& pointcloud) {
  ProfilerZoneScoped;
  if (!isPoseValid(pointcloud.getPose())) {
    return;
  }
  importPointcloud(pointcloud);
  updateMap();
}

void ProjectiveIntegrator::integrate(const PosedImage<>& range_image) {
  ProfilerZoneScoped;
  CHECK_NOTNULL(projection_model_);
  if (range_image.getDimensions() != projection_model_->getDimensions()) {
    LOG(WARNING) << "Dimensions of range image"
                 << print::eigen::oneLine(range_image.getDimensions())
                 << " do not match projection model"
                 << print::eigen::oneLine(projection_model_->getDimensions())
                 << ". Ignoring integration request.";
    return;
  }
  if (!isPoseValid(range_image.getPose())) {
    return;
  }
  importRangeImage(range_image);
  updateMap();
}

void ProjectiveIntegrator::importPointcloud(
    const PosedPointcloud<>& pointcloud) {
  ProfilerZoneScoped;
  // Reset the posed range image and the beam offset image
  posed_range_image_->resetToInitialValue();
  posed_range_image_->setPose(pointcloud.getPose());
  beam_offset_image_->resetToInitialValue();

  // Import all the points
  for (const auto& C_point : pointcloud.getPointsLocal()) {
    // Filter out noisy points and compute point's range
    if (!isMeasurementValid(C_point)) {
      continue;
    }

    // Calculate the range image index
    const auto sensor_coordinates =
        projection_model_->cartesianToSensor(C_point);

    const auto [range_image_index, beam_to_pixel_offset] =
        projection_model_->imageToNearestIndexAndOffset(
            sensor_coordinates.image);
    if (!posed_range_image_->isIndexWithinBounds(range_image_index)) {
      // Prevent out-of-bounds access
      continue;
    }

    // Add the point to the range image, if multiple points hit the same image
    // pixel, keep the closest point
    const FloatingPoint range = sensor_coordinates.depth;
    const FloatingPoint old_range_value =
        posed_range_image_->at(range_image_index);
    if (old_range_value < config_.min_range || range < old_range_value) {
      posed_range_image_->at(range_image_index) = range;
      beam_offset_image_->at(range_image_index) = beam_to_pixel_offset;
    }
  }
}

void ProjectiveIntegrator::importRangeImage(
    const PosedImage<>& range_image_input) {
  ProfilerZoneScoped;
  CHECK_NOTNULL(posed_range_image_);
  CHECK_EIGEN_EQ(range_image_input.getDimensions(),
                 posed_range_image_->getDimensions());
  *posed_range_image_ = range_image_input;
  beam_offset_image_->resetToInitialValue();
}
}  // namespace wavemap
