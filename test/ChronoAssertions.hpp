// Copyright (c) libASPL authors
// Licensed under MIT

#include <gtest/gtest.h>

#include <chrono>

// CHRONO_ASSERT_XXX

#define CHRONO_ASSERT_EQ(a, b) \
    ASSERT_EQ((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_ASSERT_NE(a, b) \
    ASSERT_NE((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_ASSERT_LT(a, b) \
    ASSERT_LT((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_ASSERT_LE(a, b) \
    ASSERT_LE((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_ASSERT_GT(a, b) \
    ASSERT_GT((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_ASSERT_GE(a, b) \
    ASSERT_GE((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_EXPECT_EQ(a, b) \
    EXPECT_EQ((a).time_since_epoch().count(), (b).time_since_epoch().count())

// CHRONO_EXPECT_XXX

#define CHRONO_EXPECT_NE(a, b) \
    EXPECT_NE((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_EXPECT_LT(a, b) \
    EXPECT_LT((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_EXPECT_LE(a, b) \
    EXPECT_LE((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_EXPECT_GT(a, b) \
    EXPECT_GT((a).time_since_epoch().count(), (b).time_since_epoch().count())

#define CHRONO_EXPECT_GE(a, b) \
    EXPECT_GE((a).time_since_epoch().count(), (b).time_since_epoch().count())
