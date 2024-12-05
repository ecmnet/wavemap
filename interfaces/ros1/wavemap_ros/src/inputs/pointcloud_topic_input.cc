#include "wavemap_ros/inputs/pointcloud_topic_input.h"

#include <memory>
#include <string>
#include <utility>

#include <cv_bridge/cv_bridge.h>
#include <opencv2/core/eigen.hpp>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <sensor_msgs/point_cloud_conversion.h>
#include <wavemap/core/data_structure/image.h>
#include <wavemap/core/integrator/projective/projective_integrator.h>
#include <wavemap/core/utils/profile/profiler_interface.h>
#include <wavemap_ros_conversions/time_conversions.h>

namespace wavemap {
DECLARE_CONFIG_MEMBERS(PointcloudTopicInputConfig,
                      (topic_name)
                      (topic_type)
                      (topic_queue_length)
                      (measurement_integrator_names)
                      (processing_retry_period)
                      (max_wait_for_pose)
                      (sensor_frame_id)
                      (time_offset)
                      (undistort_motion)
                      (num_undistortion_interpolation_intervals_per_cloud)
                      (projected_range_image_topic_name)
                      (undistorted_pointcloud_topic_name));

bool PointcloudTopicInputConfig::isValid(bool verbose) const {
  bool all_valid = true;

  all_valid &= IS_PARAM_NE(topic_name, "", verbose);
  all_valid &= IS_PARAM_TRUE(topic_type.isValid(), verbose);
  all_valid &= IS_PARAM_GT(topic_queue_length, 0, verbose);
  all_valid &= IS_PARAM_FALSE(measurement_integrator_names.empty(), verbose);
  all_valid &= IS_PARAM_GT(processing_retry_period, 0.f, verbose);
  all_valid &= IS_PARAM_GE(max_wait_for_pose, 0.f, verbose);
  all_valid &= IS_PARAM_GT(num_undistortion_interpolation_intervals_per_cloud,
                           0, verbose);

  return all_valid;
}

PointcloudTopicInput::PointcloudTopicInput(
    const PointcloudTopicInputConfig& config,
    std::shared_ptr<Pipeline> pipeline,
    std::shared_ptr<TfTransformer> transformer, std::string world_frame,
    ros::NodeHandle nh, ros::NodeHandle nh_private)
    : RosInputBase(config, std::move(pipeline), transformer,
                   std::move(world_frame), nh, nh_private),
      config_(config.checkValid()),
      pointcloud_undistorter_(
          transformer,
          config_.num_undistortion_interpolation_intervals_per_cloud) {
  // Subscribe to the pointcloud input
  registerCallback(config_.topic_type, [this, &nh](auto callback_ptr) {
    pointcloud_sub_ = nh.subscribe(
        config_.topic_name, config_.topic_queue_length, callback_ptr, this);
  });

  // Advertise the range image and undistorted pointcloud publishers if enabled
  if (!config_.projected_range_image_topic_name.empty()) {
    image_transport::ImageTransport it_private(nh_private);
    projected_range_image_pub_ = it_private.advertise(
        config_.projected_range_image_topic_name, config_.topic_queue_length);
  }
  if (!config_.undistorted_pointcloud_topic_name.empty()) {
    undistorted_pointcloud_pub_ =
        nh_private.advertise<sensor_msgs::PointCloud2>(
            config_.undistorted_pointcloud_topic_name,
            config_.topic_queue_length);
  }
}

void PointcloudTopicInput::callback(
    const sensor_msgs::PointCloud2& pointcloud_msg) {
  ProfilerZoneScoped;
  // Skip empty clouds
  const size_t num_points = pointcloud_msg.height * pointcloud_msg.width;
  if (num_points == 0) {
    ROS_WARN_STREAM("Skipping empty pointcloud with timestamp "
                    << pointcloud_msg.header.stamp << ".");
    return;
  }

  // Get the index of the x field, and assert that the y and z fields follow
  auto x_field_iter = std::find_if(
      pointcloud_msg.fields.cbegin(), pointcloud_msg.fields.cend(),
      [](const sensor_msgs::PointField& field) { return field.name == "x"; });
  if (x_field_iter == pointcloud_msg.fields.end()) {
    ROS_WARN("Received pointcloud with missing field x");
    return;
  } else if ((++x_field_iter)->name != "y") {
    ROS_WARN("Received pointcloud with missing or out-of-order field y");
    return;
  } else if ((++x_field_iter)->name != "z") {
    ROS_WARN("Received pointcloud with missing or out-of-order field z");
    return;
  }

  // Convert to our generic stamped pointcloud format
  const uint64_t stamp_nsec = convert::rosTimeToNanoSeconds(
      pointcloud_msg.header.stamp + ros::Duration(config_.time_offset));
  std::string sensor_frame_id = config_.sensor_frame_id.empty()
                                    ? pointcloud_msg.header.frame_id
                                    : config_.sensor_frame_id;
  undistortion::StampedPointcloud stamped_pointcloud{
      stamp_nsec, std::move(sensor_frame_id), num_points};

  // Load the points with time information if undistortion is enabled
  bool loaded = false;
  sensor_msgs::PointCloud2ConstIterator<float> pos_it(pointcloud_msg, "x");
  if (config_.undistort_motion) {
    // NOTE: Livox pointclouds are not handled here but in their own callback.
    switch (config_.topic_type) {
      case PointcloudTopicType::kOuster:
        if (hasField(pointcloud_msg, "t")) {
          sensor_msgs::PointCloud2ConstIterator<uint32_t> t_it(pointcloud_msg,
                                                               "t");
          for (; pos_it != pos_it.end(); ++pos_it, ++t_it) {
            stamped_pointcloud.emplace(pos_it[0], pos_it[1], pos_it[2], *t_it);
          }
          loaded = true;
        } else {
          ROS_WARN_STREAM("Pointcloud topic type is set to \""
                          << config_.topic_type.toStr()
                          << "\", but message has no time field \"t\". Will "
                             "not be undistorted.");
        }
        break;
      default:
        ROS_WARN_STREAM(
            "Pointcloud undistortion is enabled, but not yet supported for "
            "topic type \""
            << config_.topic_type.toStr() << "\". Will not be undistorted.");
    }
  }

  // If undistortion is disabled or loading failed, only load positions
  if (!loaded) {
    for (; pos_it != pos_it.end(); ++pos_it) {
      stamped_pointcloud.emplace(pos_it[0], pos_it[1], pos_it[2], 0);
    }
  }

  // Add it to the integration queue
  pointcloud_queue_.emplace(std::move(stamped_pointcloud));
}

#ifdef LIVOX_AVAILABLE
void PointcloudTopicInput::callback(
    const livox_ros_driver2::CustomMsg& pointcloud_msg) {
  ProfilerZoneScoped;
  // Skip empty clouds
  if (pointcloud_msg.points.empty()) {
    ROS_WARN_STREAM("Skipping empty pointcloud with timestamp "
                    << pointcloud_msg.header.stamp << ".");
    return;
  }

  // Convert to our generic stamped pointcloud format
  const uint64_t stamp_nsec =
      pointcloud_msg.timebase +
      static_cast<int32_t>(config_.time_offset * 1000000000.0);
  std::string sensor_frame_id = config_.sensor_frame_id.empty()
                                    ? pointcloud_msg.header.frame_id
                                    : config_.sensor_frame_id;
  undistortion::StampedPointcloud stamped_pointcloud{
      stamp_nsec, std::move(sensor_frame_id), pointcloud_msg.points.size()};
  for (const auto& point : pointcloud_msg.points) {
    stamped_pointcloud.emplace(point.x, point.y, point.z, point.offset_time);
  }

  // Add it to the integration queue
  pointcloud_queue_.emplace(std::move(stamped_pointcloud));
}
#endif

void PointcloudTopicInput::processQueue() {
  ProfilerZoneScoped;
  while (!pointcloud_queue_.empty()) {
    auto& oldest_msg = pointcloud_queue_.front();

    // Drop messages if they're older than max_wait_for_pose
    if (config_.max_wait_for_pose <
        convert::nanoSecondsToSeconds(pointcloud_queue_.back().getEndTime() -
                                      oldest_msg.getStartTime())) {
      ROS_WARN_STREAM(
          "Max waiting time of "
          << config_.max_wait_for_pose
          << "s exceeded for pointcloud with frame \""
          << oldest_msg.getSensorFrame() << "\" and time interval ["
          << oldest_msg.getStartTime() << ", " << oldest_msg.getEndTime()
          << "] vs newest cloud end time "
          << pointcloud_queue_.back().getEndTime() << ". Dropping cloud.");
      pointcloud_queue_.pop();
      continue;
    }

    // Undistort the pointcloud if appropriate
    PosedPointcloud<> posed_pointcloud;
    if (config_.undistort_motion) {
      const auto undistortion_result =
          pointcloud_undistorter_.undistortPointcloud(
              oldest_msg, posed_pointcloud, world_frame_);
      if (undistortion_result != PointcloudUndistorter::Result::kSuccess) {
        const uint64_t start_time = oldest_msg.getStartTime();
        const uint64_t end_time = oldest_msg.getEndTime();
        switch (undistortion_result) {
          case PointcloudUndistorter::Result::kEndTimeNotInTfBuffer:
            // Try to get this pointcloud's pose again at the next iteration
            return;
          case PointcloudUndistorter::Result::kStartTimeNotInTfBuffer:
            ROS_WARN_STREAM(
                "Pointcloud end pose is available but start pose at time "
                << start_time
                << " is not (or no longer). Skipping pointcloud.");
            break;
          case PointcloudUndistorter::Result::kIntermediateTimeNotInTfBuffer:
            ROS_WARN_STREAM(
                "Could not buffer all transforms for pointcloud spanning time "
                "interval ["
                << start_time << ", " << end_time
                << "]. This should never happen. Skipping pointcloud.");
            break;
          default:
            ROS_WARN("Unknown pointcloud undistortion error.");
        }

        pointcloud_queue_.pop();
        continue;
      }
    } else {
      // Get the pointcloud's pose
      const auto T_W_C = transformer_->lookupTransform(
          world_frame_, oldest_msg.getSensorFrame(),
          convert::nanoSecondsToRosTime(oldest_msg.getTimeBase()));
      if (!T_W_C) {
        // Try to get this pointcloud's pose again at the next iteration
        return;
      }

      // Convert to a posed pointcloud
      posed_pointcloud = PosedPointcloud<>(*T_W_C);
      posed_pointcloud.resize(oldest_msg.getPoints().size());
      for (unsigned point_idx = 0; point_idx < oldest_msg.getPoints().size();
           ++point_idx) {
        posed_pointcloud[point_idx] =
            oldest_msg.getPoints()[point_idx].position;
      }
    }

    // Integrate the pointcloud
    ROS_DEBUG_STREAM("Inserting pointcloud with "
                     << posed_pointcloud.size()
                     << " points. Remaining pointclouds in queue: "
                     << pointcloud_queue_.size() - 1 << ".");
    integration_timer_.start();
    pipeline_->runPipeline(config_.measurement_integrator_names,
                           posed_pointcloud);
    integration_timer_.stop();
    ROS_DEBUG_STREAM("Integrated new pointcloud in "
                     << integration_timer_.getLastEpisodeDuration()
                     << "s. Total integration time: "
                     << integration_timer_.getTotalDuration() << "s.");

    // Publish debugging visualizations
    publishProjectedRangeImageIfEnabled(
        convert::nanoSecondsToRosTime(oldest_msg.getMedianTime()),
        posed_pointcloud);
    publishUndistortedPointcloudIfEnabled(
        convert::nanoSecondsToRosTime(oldest_msg.getMedianTime()),
        posed_pointcloud);
    ProfilerFrameMarkNamed("Pointcloud");

    // Remove the pointcloud from the queue
    pointcloud_queue_.pop();
  }
}

bool PointcloudTopicInput::hasField(const sensor_msgs::PointCloud2& msg,
                                    const std::string& field_name) {
  return std::any_of(msg.fields.cbegin(), msg.fields.cend(),
                     [&field_name = std::as_const(field_name)](
                         const sensor_msgs::PointField& field) {
                       return field.name == field_name;
                     });
}

void PointcloudTopicInput::publishProjectedRangeImageIfEnabled(
    const ros::Time& /*stamp*/, const PosedPointcloud<>& /*posed_pointcloud*/) {
  ProfilerZoneScoped;
  if (config_.projected_range_image_topic_name.empty() ||
      projected_range_image_pub_.getNumSubscribers() <= 0) {
    return;
  }

  // TODO(victorr): Reimplement this
  //  auto projective_integrator =
  //      std::dynamic_pointer_cast<ProjectiveIntegrator>(integrators_.front());
  //  if (projective_integrator) {
  //    const auto& range_image = projective_integrator->getPosedRangeImage();
  //    if (range_image) {
  //    }
  //  }
  //
  //  cv_bridge::CvImage cv_image;
  //  cv_image.header.stamp = stamp;
  //  cv_image.encoding = "32FC1";
  //  cv::eigen2cv(range_image.getData(), cv_image.image);
  //  projected_range_image_pub_.publish(cv_image.toImageMsg());
}

void PointcloudTopicInput::publishUndistortedPointcloudIfEnabled(
    const ros::Time& stamp, const PosedPointcloud<>& undistorted_pointcloud) {
  ProfilerZoneScoped;
  if (config_.undistorted_pointcloud_topic_name.empty() ||
      undistorted_pointcloud_pub_.getNumSubscribers() <= 0) {
    return;
  }

  sensor_msgs::PointCloud pointcloud_msg;
  pointcloud_msg.header.stamp = stamp;
  pointcloud_msg.header.frame_id = world_frame_;

  pointcloud_msg.points.reserve(undistorted_pointcloud.size());
  for (const auto& point : undistorted_pointcloud.getPointsGlobal()) {
    auto& point_msg = pointcloud_msg.points.emplace_back();
    point_msg.x = point.x();
    point_msg.y = point.y();
    point_msg.z = point.z();
  }

  sensor_msgs::PointCloud2 pointcloud2_msg;
  sensor_msgs::convertPointCloudToPointCloud2(pointcloud_msg, pointcloud2_msg);
  undistorted_pointcloud_pub_.publish(pointcloud2_msg);
}
}  // namespace wavemap
