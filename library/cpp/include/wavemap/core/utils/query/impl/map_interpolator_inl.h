#ifndef WAVEMAP_CORE_UTILS_QUERY_IMPL_MAP_INTERPOLATOR_INL_H_
#define WAVEMAP_CORE_UTILS_QUERY_IMPL_MAP_INTERPOLATOR_INL_H_

#include "wavemap/core/indexing/index_conversions.h"
#include "wavemap/core/utils/data/eigen_checks.h"

namespace wavemap::interpolate {
template <typename MapT>
FloatingPoint nearestNeighbor(MapT& map, const Point3D& position) {
  // Compute the index of the cell closest to the query point
  const FloatingPoint cell_width_inv = 1.f / map.getMinCellWidth();
  const auto nearest_index =
      wavemap::convert::pointToNearestIndex(position, cell_width_inv);
  // Lookup and return its value
  return map.getCellValue(nearest_index);
}

template <typename MapT>
FloatingPoint trilinear(MapT& map, const Point3D& position) {
  // Compute the index of the interpolation neighborhood's minimum corner
  const FloatingPoint cell_width = map.getMinCellWidth();
  const FloatingPoint cell_width_inv = 1.f / cell_width;
  const auto min_corner_index =
      wavemap::convert::pointToFloorIndex(position, cell_width_inv);

  // Gather the values of the 8 neighbors
  std::array<FloatingPoint, 8> cube_corners{};
  for (int neighbor_idx = 0; neighbor_idx < 8; ++neighbor_idx) {
    const Index3D offset{bit_ops::is_bit_set(neighbor_idx, 2),
                         bit_ops::is_bit_set(neighbor_idx, 1),
                         bit_ops::is_bit_set(neighbor_idx, 0)};
    cube_corners[neighbor_idx] = map.getCellValue(min_corner_index + offset);
  }

  // Compute the offset between the query point, the min corner and max corner
  const Point3D position_min_corner =
      wavemap::convert::indexToCenterPoint(min_corner_index, cell_width);
  // Offset to min corner
  const Vector3D a = (position - position_min_corner) * cell_width_inv;
  DCHECK_EIGEN_GE(a, Vector3D::Constant(0.f - kEpsilon));
  DCHECK_EIGEN_LE(a, Vector3D::Constant(1.f + kEpsilon));
  // Offset to max corner
  const Vector3D a_comp = 1.f - a.array();
  DCHECK_EIGEN_GE(a_comp, Vector3D::Constant(0.f - kEpsilon));
  DCHECK_EIGEN_LE(a_comp, Vector3D::Constant(1.f + kEpsilon));

  // Interpolate out the first dimension,
  // reducing the cube into a square that contains the query point
  std::array<FloatingPoint, 4> plane_corners{};
  for (int corner_idx = 0; corner_idx < 4; ++corner_idx) {
    plane_corners[corner_idx] = a_comp[0] * cube_corners[corner_idx] +
                                a[0] * cube_corners[corner_idx + 0b100];
  }

  // Interpolate out the second dimension,
  // reducing the square into a line segment that contains the query point
  std::array<FloatingPoint, 2> line_corners{};
  for (int side_idx = 0; side_idx < 2; ++side_idx) {
    line_corners[side_idx] = a_comp[1] * plane_corners[side_idx] +
                             a[1] * plane_corners[side_idx + 0b10];
  }

  // Interpolate out the third dimension,
  // reducing the line segment into a single value
  return a_comp[2] * line_corners[0] + a[2] * line_corners[1];
}
}  // namespace wavemap::interpolate

#endif  // WAVEMAP_CORE_UTILS_QUERY_IMPL_MAP_INTERPOLATOR_INL_H_
