//
// Created by 刘文景 on 2021/4/9.
//

#include "helpers/memenv/memenv.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "lsmdb/db.h"
#include "lsmdb/env.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}