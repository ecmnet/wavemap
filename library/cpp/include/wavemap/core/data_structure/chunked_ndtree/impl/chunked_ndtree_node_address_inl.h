#ifndef WAVEMAP_CORE_DATA_STRUCTURE_CHUNKED_NDTREE_IMPL_CHUNKED_NDTREE_NODE_ADDRESS_INL_H_
#define WAVEMAP_CORE_DATA_STRUCTURE_CHUNKED_NDTREE_IMPL_CHUNKED_NDTREE_NODE_ADDRESS_INL_H_

#include <utility>

namespace wavemap {
template <typename ChunkT>
ChunkedNdtreeNodePtr<ChunkT>::ChunkedNdtreeNodePtr(ChunkT* chunk)
    : node_(chunk ? std::make_optional<NodeRef>(*chunk) : std::nullopt) {}

template <typename ChunkT>
ChunkedNdtreeNodePtr<ChunkT>::ChunkedNdtreeNodePtr(ChunkT* chunk,
                                                   NodeOffsetType offset)
    : node_(chunk ? std::make_optional<NodeRef>(*chunk, offset)
                  : std::nullopt) {}

template <typename ChunkT>
ChunkedNdtreeNodePtr<ChunkT>::ChunkedNdtreeNodePtr(
    const ChunkedNdtreeNodePtr& other)
    : node_(other ? std::make_optional<NodeRef>(other.node_.value())
                  : std::nullopt) {}

template <typename ChunkT>
ChunkedNdtreeNodePtr<ChunkT>::ChunkedNdtreeNodePtr(
    ChunkedNdtreeNodePtr&& other) noexcept
    : node_(other ? std::make_optional<NodeRef>(std::move(other.node_.value()))
                  : std::nullopt) {}

template <typename ChunkT>
ChunkedNdtreeNodePtr<ChunkT>& ChunkedNdtreeNodePtr<ChunkT>::operator=(
    const ChunkedNdtreeNodePtr& other) {
  if (other) {
    node_.emplace(other.node_.value());
  } else {
    reset();
  }
  return *this;
}

template <typename ChunkT>
ChunkedNdtreeNodePtr<ChunkT>& ChunkedNdtreeNodePtr<ChunkT>::operator=(
    ChunkedNdtreeNodePtr&& other) noexcept {
  if (other) {
    node_.emplace(std::move(other.node_.value()));
  } else {
    reset();
  }
  return *this;
}

template <typename ChunkT>
ChunkedNdtreeNodeRef<ChunkT>::ChunkedNdtreeNodeRef(ChunkT& chunk,
                                                   NodeOffsetType offset)
    : chunk_(chunk), offset_(offset) {}

template <typename ChunkT>
ChunkedNdtreeNodeRef<ChunkT>::ChunkedNdtreeNodeRef(
    const ChunkedNdtreeNodeRef& other)
    : ChunkedNdtreeNodeRef(other.chunk_, other.offset_) {}

template <typename ChunkT>
ChunkedNdtreeNodeRef<ChunkT>::ChunkedNdtreeNodeRef(
    ChunkedNdtreeNodeRef&& other) noexcept
    : chunk_(other.chunk_), offset_(other.offset_) {}

template <typename ChunkT>
ChunkedNdtreeNodeRef<ChunkT>::operator ChunkedNdtreeNodeRef<const ChunkT>()
    const {
  return {chunk_, offset_};
}

template <typename ChunkT>
typename ChunkedNdtreeNodeRef<ChunkT>::NodePtr
ChunkedNdtreeNodeRef<ChunkT>::operator&() const {  // NOLINT
  return {&chunk_, offset_};
}

template <typename ChunkT>
bool ChunkedNdtreeNodeRef<ChunkT>::empty() const {
  return !hasAtLeastOneChild() && !hasNonzeroData();
}

template <typename ChunkT>
bool ChunkedNdtreeNodeRef<ChunkT>::hasNonzeroData() const {
  return chunk_.nodeHasNonzeroData(offset_);
}

template <typename ChunkT>
bool ChunkedNdtreeNodeRef<ChunkT>::hasNonzeroData(
    FloatingPoint threshold) const {
  return chunk_.nodeHasNonzeroData(offset_, threshold);
}

template <typename ChunkT>
auto& ChunkedNdtreeNodeRef<ChunkT>::data() const {
  return chunk_.nodeData(offset_);
}

template <typename ChunkT>
bool ChunkedNdtreeNodeRef<ChunkT>::hasAtLeastOneChild() const {
  return chunk_.nodeHasAtLeastOneChild(offset_);
}

template <typename ChunkT>
bool ChunkedNdtreeNodeRef<ChunkT>::hasChild(
    NdtreeIndexRelativeChild child_index) const {
  return chunk_.nodeHasChild(offset_, child_index);
}

template <typename ChunkT>
void ChunkedNdtreeNodeRef<ChunkT>::eraseChild(
    NdtreeIndexRelativeChild child_index) const {
  chunk_.nodeEraseChild(offset_, child_index);
}

template <typename ChunkT>
typename ChunkedNdtreeNodeRef<ChunkT>::NodePtr
ChunkedNdtreeNodeRef<ChunkT>::getChild(
    NdtreeIndexRelativeChild child_index) const {
  if (!hasChild(child_index)) {
    return {nullptr};
  }
  const NodeOffsetType child_offset = computeChildOffset(child_index);
  if (ChunkT::kMaxNodeOffset < child_offset) {
    const NodeOffsetType level_index =
        convert::nodeOffsetToLevelIndex<kDim>(child_offset);
    auto* child_chunk = chunk_.getChild(level_index);
    return {child_chunk, kRootOffset};
  }
  return {&chunk_, child_offset};
}

template <typename ChunkT>
template <typename... DefaultArgs>
ChunkedNdtreeNodeRef<ChunkT> ChunkedNdtreeNodeRef<ChunkT>::getOrAllocateChild(
    NdtreeIndexRelativeChild child_index, DefaultArgs&&... args) const {
  chunk_.nodeSetHasChild(offset_, child_index);
  const NodeOffsetType child_offset = computeChildOffset(child_index);
  if (ChunkT::kMaxNodeOffset < child_offset) {
    const NodeOffsetType level_index =
        convert::nodeOffsetToLevelIndex<kDim>(child_offset);
    auto& child_chunk = chunk_.getOrAllocateChild(
        level_index, std::forward<DefaultArgs>(args)...);
    return {child_chunk, kRootOffset};
  }
  return {chunk_, child_offset};
}

template <typename ChunkT>
IndexElement ChunkedNdtreeNodeRef<ChunkT>::computeDepthIndex() const {
  return convert::nodeOffsetToDepth<kDim>(offset_);
}

template <typename ChunkT>
typename ChunkT::NodeOffsetType
ChunkedNdtreeNodeRef<ChunkT>::computeLevelIndex() const {
  return convert::nodeOffsetToLevelIndex<kDim>(offset_);
}

template <typename ChunkT>
typename ChunkT::NodeOffsetType
ChunkedNdtreeNodeRef<ChunkT>::computeChildOffset(
    NdtreeIndexRelativeChild child_index) const {
  return convert::nodeOffsetToChildOffset<kDim>(offset_, child_index);
}
}  // namespace wavemap

#endif  // WAVEMAP_CORE_DATA_STRUCTURE_CHUNKED_NDTREE_IMPL_CHUNKED_NDTREE_NODE_ADDRESS_INL_H_
