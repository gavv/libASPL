// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/util/MpscQueue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace {

struct TestNode : aspl::util::MpscNode
{
    static std::atomic<int> nodeCount;

    int value = 0;

    TestNode(const TestNode&) = delete;
    TestNode& operator=(const TestNode&) = delete;

    TestNode(int val = 0)
        : value(val)
    {
        nodeCount++;
    }

    ~TestNode()
    {
        nodeCount--;
    }
};

std::atomic<int> TestNode::nodeCount = 0;

} // namespace

struct MpscQueueTest : testing::Test
{
    void SetUp() override
    {
        TestNode::nodeCount = 0;
    }
};

TEST_F(MpscQueueTest, Empty)
{
    aspl::util::MpscQueue<TestNode> queue;

    auto node = queue.PopNode();
    EXPECT_EQ(nullptr, node);
}

TEST_F(MpscQueueTest, PushPop)
{
    aspl::util::MpscQueue<TestNode> queue;

    auto node = std::make_shared<TestNode>(42);
    queue.PushNode(std::move(node));

    auto retNode = queue.PopNode();
    EXPECT_TRUE(retNode);
    EXPECT_EQ(42, retNode->value);

    // queue should be empty now
    EXPECT_FALSE(queue.PopNode());
}

TEST_F(MpscQueueTest, PushPop_Many)
{
    aspl::util::MpscQueue<TestNode> queue;

    // push multiple nodes
    for (int i = 0; i < 5; i++) {
        auto node = std::make_shared<TestNode>(i);
        queue.PushNode(std::move(node));
    }

    // pop them in FIFO order
    for (int i = 0; i < 5; i++) {
        auto retNode = queue.PopNode();
        EXPECT_TRUE(retNode);
        EXPECT_EQ(i, retNode->value);
    }

    // queue should be empty now
    EXPECT_FALSE(queue.PopNode());
}

TEST_F(MpscQueueTest, PushPop_Interleaved)
{
    aspl::util::MpscQueue<TestNode> queue;

    // push one, pop one
    auto node1 = std::make_shared<TestNode>(1);
    queue.PushNode(std::move(node1));

    auto retNode1 = queue.PopNode();
    EXPECT_TRUE(retNode1);
    EXPECT_EQ(1, retNode1->value);

    // push two more
    auto node2 = std::make_shared<TestNode>(2);
    auto node3 = std::make_shared<TestNode>(3);
    queue.PushNode(std::move(node2));
    queue.PushNode(std::move(node3));

    // pop them
    auto retNode2 = queue.PopNode();
    auto retNode3 = queue.PopNode();
    EXPECT_TRUE(retNode2);
    EXPECT_TRUE(retNode3);
    EXPECT_EQ(2, retNode2->value);
    EXPECT_EQ(3, retNode3->value);

    // queue should be empty
    EXPECT_FALSE(queue.PopNode());
}

TEST_F(MpscQueueTest, Ownership_Transfer)
{
    aspl::util::MpscQueue<TestNode> queue;

    EXPECT_EQ(0, TestNode::nodeCount);

    {
        auto node = std::make_shared<TestNode>(100);
        EXPECT_EQ(1, node.use_count());
        EXPECT_EQ(1, TestNode::nodeCount);

        queue.PushNode(std::move(node));

        // node is now moved, but object still exists in queue
        EXPECT_FALSE(node);
        EXPECT_EQ(1, TestNode::nodeCount);
    }

    // object should still exist in queue
    EXPECT_EQ(1, TestNode::nodeCount);

    {
        auto retNode = queue.PopNode();
        EXPECT_TRUE(retNode);
        EXPECT_EQ(100, retNode->value);

        // we're exclusive owner again
        EXPECT_EQ(1, retNode.use_count());
        EXPECT_EQ(1, TestNode::nodeCount);
    }

    // object should be destroyed now
    EXPECT_EQ(0, TestNode::nodeCount);
}

TEST_F(MpscQueueTest, Ownership_Destructor)
{
    {
        aspl::util::MpscQueue<TestNode> queue;

        EXPECT_EQ(0, TestNode::nodeCount);

        // push several nodes
        for (int i = 0; i < 3; i++) {
            auto node = std::make_shared<TestNode>(i);
            queue.PushNode(std::move(node));
        }

        EXPECT_EQ(3, TestNode::nodeCount);

        {
            // pop one, leave two in queue
            auto retNode = queue.PopNode();
            EXPECT_TRUE(retNode);

            EXPECT_EQ(1, retNode.use_count());
            EXPECT_EQ(3, TestNode::nodeCount);
        }

        // popped node is deleted
        EXPECT_EQ(2, TestNode::nodeCount);
    }

    // queue destructor deleted remaining nodes
    EXPECT_EQ(0, TestNode::nodeCount);
}

TEST_F(MpscQueueTest, Ownership_Sharing)
{
    {
        aspl::util::MpscQueue<TestNode> queue;

        EXPECT_EQ(0, TestNode::nodeCount);

        auto node1 = std::make_shared<TestNode>();
        auto node2 = std::make_shared<TestNode>();

        // no std::move - keep ownership
        queue.PushNode(node1);
        queue.PushNode(node2);

        EXPECT_EQ(2, node1.use_count());
        EXPECT_EQ(2, node2.use_count());
        EXPECT_EQ(2, TestNode::nodeCount);

        {
            // remove node1 from queue
            auto retNode = queue.PopNode();
            EXPECT_TRUE(retNode);

            EXPECT_EQ(2, node1.use_count());
            EXPECT_EQ(2, node2.use_count());
            EXPECT_EQ(2, TestNode::nodeCount);
        }

        EXPECT_EQ(1, node1.use_count());
        EXPECT_EQ(2, node2.use_count());
        EXPECT_EQ(2, TestNode::nodeCount);

        // release node2 ownership
        node2.reset();

        EXPECT_EQ(1, node1.use_count());
        EXPECT_EQ(2, TestNode::nodeCount);
    }

    EXPECT_EQ(0, TestNode::nodeCount);
}

TEST_F(MpscQueueTest, Multithreaded_Spsc)
{
    aspl::util::MpscQueue<TestNode> queue;
    const int numIterations = 1000;
    std::atomic<int> totalPushed = 0;

    std::thread producer([&]() {
        for (int i = 0; i < numIterations; i++) {
            auto node = std::make_shared<TestNode>(i);
            queue.PushNode(std::move(node));
            totalPushed++;

            if (i % 10 == 0) {
                std::this_thread::yield();
            }
        }
    });

    // consume in test thread and validate values immediately
    int expectedValue = 0;
    int totalPopped = 0;
    while (totalPopped < numIterations) {
        auto node = queue.PopNode();
        if (node) {
            EXPECT_EQ(expectedValue, node->value)
                << "Value mismatch at position " << totalPopped;
            expectedValue++;
            totalPopped++;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();

    EXPECT_EQ(numIterations, totalPushed.load());
    EXPECT_EQ(numIterations, totalPopped);
    EXPECT_EQ(0, TestNode::nodeCount);
}

TEST_F(MpscQueueTest, Multithreaded_Mpsc)
{
    aspl::util::MpscQueue<TestNode> queue;
    const int numProducers = 4;
    const int nodesPerProducer = 100;
    std::atomic<int> pushCount = 0;

    // start multiple producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; p++) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < nodesPerProducer; i++) {
                auto node = std::make_shared<TestNode>(p * 1000 + i);
                queue.PushNode(std::move(node));
                pushCount++;
            }
        });
    }

    // consume in test thread
    std::vector<int> consumedValues;
    consumedValues.reserve(numProducers * nodesPerProducer);
    int totalConsumed = 0;
    const int totalExpected = numProducers * nodesPerProducer;

    while (totalConsumed < totalExpected) {
        auto node = queue.PopNode();
        if (node) {
            consumedValues.push_back(node->value);
            totalConsumed++;
        } else {
            std::this_thread::yield();
        }
    }

    // wait for all producers to finish
    for (auto& producer : producers) {
        producer.join();
    }

    // verify all nodes were consumed
    EXPECT_EQ(totalExpected, consumedValues.size());
    EXPECT_EQ(totalExpected, pushCount.load());

    // verify all values are present (order may vary due to concurrency)
    std::sort(consumedValues.begin(), consumedValues.end());
    for (int p = 0; p < numProducers; p++) {
        for (int i = 0; i < nodesPerProducer; i++) {
            int expectedValue = p * 1000 + i;
            auto it =
                std::find(consumedValues.begin(), consumedValues.end(), expectedValue);
            EXPECT_NE(consumedValues.end(), it) << "Missing value: " << expectedValue;
        }
    }

    EXPECT_EQ(0, TestNode::nodeCount);
}
