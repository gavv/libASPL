// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/util/Freelist.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>

namespace {

struct TestNode : aspl::util::FreelistNode
{
    static std::atomic<int> objectCount;

    int value = 0;

    TestNode()
    {
        objectCount++;
    }

    ~TestNode()
    {
        objectCount--;
    }
};

std::atomic<int> TestNode::objectCount = 0;

} // namespace

struct FreelistTest : testing::Test
{
    void SetUp() override
    {
        TestNode::objectCount = 0;
    }
};

TEST_F(FreelistTest, Empty)
{
    aspl::util::Freelist<TestNode> freelist;

    TestNode* node = freelist.GetNode();
    EXPECT_EQ(nullptr, node);

    EXPECT_EQ(0, TestNode::objectCount);
}

TEST_F(FreelistTest, PutGet)
{
    aspl::util::Freelist<TestNode> freelist;

    TestNode node;
    node.value = 42;
    EXPECT_EQ(1, TestNode::objectCount);

    freelist.PutNode(&node);

    {
        TestNode* retNode = freelist.GetNode();
        EXPECT_EQ(&node, retNode);
        EXPECT_EQ(42, retNode->value);
    }

    {
        TestNode* retNode = freelist.GetNode();
        EXPECT_EQ(nullptr, retNode);
    }
}

TEST_F(FreelistTest, PutGet_Many)
{
    aspl::util::Freelist<TestNode> freelist;

    const int numNodes = 5;
    TestNode nodes[numNodes];

    for (int i = 0; i < numNodes; i++) {
        nodes[i].value = i;
        freelist.PutNode(&nodes[i]);
    }

    EXPECT_EQ(numNodes, TestNode::objectCount);

    for (int i = numNodes - 1; i >= 0; i--) {
        TestNode* retNode = freelist.GetNode();
        EXPECT_NE(nullptr, retNode);
        EXPECT_EQ(&nodes[i], retNode);
        EXPECT_EQ(i, retNode->value);
    }

    TestNode* retNode = freelist.GetNode();
    EXPECT_EQ(nullptr, retNode);
}

TEST_F(FreelistTest, PutGet_Many_Loop)
{
    aspl::util::Freelist<TestNode> freelist;

    for (int i = 0; i < 100; i++) {
        TestNode nodes[10];

        for (int j = 0; j < 10; j++) {
            nodes[j].value = i * 10 + j;
            freelist.PutNode(&nodes[j]);
        }

        EXPECT_EQ(10, TestNode::objectCount);

        for (int j = 0; j < 10; j++) {
            TestNode* retNode = freelist.GetNode();
            EXPECT_NE(nullptr, retNode);
        }

        TestNode* retNode = freelist.GetNode();
        EXPECT_EQ(nullptr, retNode);
    }
}

TEST_F(FreelistTest, NoOwnership)
{
    {
        TestNode nodes[3];

        {
            aspl::util::Freelist<TestNode> freelist;

            for (int i = 0; i < 3; i++) {
                freelist.PutNode(&nodes[i]);
            }

            EXPECT_EQ(3, TestNode::objectCount);
        }

        EXPECT_EQ(3, TestNode::objectCount);
    }

    EXPECT_EQ(0, TestNode::objectCount);
}
