#include <gtest/gtest.h>

#include "wavemap_2d/common.h"
#include "wavemap_2d/indexing/index.h"
#include "wavemap_2d/iterator/subtree_iterator.h"
#include "wavemap_2d/test/fixture_base.h"

namespace wavemap_2d {
class SubtreeIteratorTest : public FixtureBase {
 protected:
  using NodeType = Node<int>;
  using NodePtrType = NodeType*;

  static std::unique_ptr<NodeType> demoTree() {
    auto root_node = std::make_unique<NodeType>(1);
    auto child_0 = root_node->allocateChild(0, 2);
    auto child_1 = root_node->allocateChild(1, 3);

    child_0->allocateChild(0, 4);
    auto child_01 = child_0->allocateChild(1, 5);
    child_0->allocateChild(2, 6);
    auto child_11 = child_1->allocateChild(1, 7);
    auto child_13 = child_1->allocateChild(3, 8);

    child_01->allocateChild(2, 9);
    child_01->allocateChild(3, 10);
    child_11->allocateChild(0, 11);
    child_11->allocateChild(1, 12);
    child_11->allocateChild(2, 13);
    child_11->allocateChild(3, 14);
    child_13->allocateChild(2, 15);

    return root_node;
  }
};

TEST_F(SubtreeIteratorTest, DepthFirstPreorderTraversal) {
  std::unique_ptr<Node<int>> root_node = demoTree();
  const std::vector<int> expected_nodes{1, 2,  4,  5,  9,  10, 6, 3,
                                        7, 11, 12, 13, 14, 8,  15};

  int idx = 0;
  for (NodePtrType node_ptr :
       Subtree<NodePtrType, TraversalOrder::kDepthFirstPreorder>(
           root_node.get())) {
    EXPECT_EQ(node_ptr->data(), expected_nodes[idx++]);
  }
  EXPECT_EQ(idx, expected_nodes.size());
}

TEST_F(SubtreeIteratorTest, DepthFirstPostorderTraversal) {
  std::unique_ptr<Node<int>> root_node = demoTree();
  const std::vector<int> expected_nodes{4,  9,  10, 5,  6, 2, 11, 12,
                                        13, 14, 7,  15, 8, 3, 1};

  int idx = 0;
  for (NodePtrType node_ptr :
       Subtree<NodePtrType, TraversalOrder::kDepthFirstPostorder>(
           root_node.get())) {
    //    LOG(INFO) << "Got: " << node_ptr->data() << " while expecting "
    //              << expected_nodes[idx++];
    EXPECT_EQ(node_ptr->data(), expected_nodes[idx++]);
  }
  EXPECT_EQ(idx, expected_nodes.size());
}
TEST_F(SubtreeIteratorTest, BreadthFirstTraversal) {
  std::unique_ptr<Node<int>> root_node = demoTree();
  const std::vector<int> expected_nodes{1, 2,  3,  4,  5,  6,  7, 8,
                                        9, 10, 11, 12, 13, 14, 15};

  int idx = 0;
  for (NodePtrType node_ptr :
       Subtree<NodePtrType, TraversalOrder::kBreadthFirst>(root_node.get())) {
    EXPECT_EQ(node_ptr->data(), expected_nodes[idx++]);
  }
  EXPECT_EQ(idx, expected_nodes.size());
}
}  // namespace wavemap_2d
