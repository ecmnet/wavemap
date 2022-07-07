#ifndef WAVEMAP_2D_INDEXING_INDEX_HASHES_H_
#define WAVEMAP_2D_INDEXING_INDEX_HASHES_H_

#include <wavemap_common/common.h>

namespace wavemap {
struct VoxbloxIndexHash {
  static constexpr size_t sl = 17191;

  size_t operator()(const Index2D& index) const {
    return index.x() + index.y() * sl;
  }
};

// NOTE: This hash function is introduced in K. Museth, “VDB: High-resolution
//       sparse volumes with dynamic topology,” ACM Trans. Graph., 2013.
struct VDBIndexHash {
  // TODO(victorr): Pick a better estimate for this value
  // NOTE: kLog2N should equal log_2 of the estimated hash map size
  static constexpr size_t kLog2N = 16u;

  size_t operator()(const Index2D& index) const {
    return static_cast<size_t>(((1 << kLog2N) - 1) &
                               (index.x() * 73856093 ^ index.y() * 19349663));
  }
};
}  // namespace wavemap

#endif  // WAVEMAP_2D_INDEXING_INDEX_HASHES_H_
