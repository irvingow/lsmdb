//
// Created by 刘文景 on 2021/4/9.
//

#include "helpers/memenv/memenv.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "lsmdb/env.h"
#include "util/test_util.h"

namespace lsmdb {

class MemEnvTest : public testing::Test {
 public:
  MemEnvTest() : env_(NewMemEnv(Env::Default())) {}
  ~MemEnvTest() override { delete env_; }

  Env* env_;
};

TEST_F(MemEnvTest, Basics) {
  uint64_t file_size;
  WritableFile* writable_file;
  std::vector<std::string> children;

  ASSERT_LSMDB_OK(env_->CreateDir("/dir"));

  // Check that the directory is empty.
  ASSERT_TRUE(!env_->FileExists("/dir/non_existent"));
  ASSERT_TRUE(!env_->GetFileSize("/dir/non_existent", &file_size).ok());
  ASSERT_LSMDB_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0, children.size());

  // Create a file.
  ASSERT_LSMDB_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_LSMDB_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0, file_size);
  delete writable_file;

  // Check that the file exists.
  ASSERT_TRUE(env_->FileExists("/dir/f"));
  ASSERT_LSMDB_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0, file_size);
  ASSERT_LSMDB_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(1, children.size());
  ASSERT_EQ("f", children[0]);

  // Write to the file.
  ASSERT_LSMDB_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_LSMDB_OK(writable_file->Append("abc"));
  delete writable_file;

  // Check that append works.
  ASSERT_LSMDB_OK(env_->NewAppendableFile("/dir/f", &writable_file));
  ASSERT_LSMDB_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(3, file_size);
  ASSERT_LSMDB_OK(writable_file->Append("hello"));
  delete writable_file;

  // Check for expected size.
  ASSERT_LSMDB_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(8, file_size);

  // Check that renaming works.
  ASSERT_TRUE(!env_->RenameFile("/dir/non_existent", "/dir/g").ok());
  ASSERT_LSMDB_OK(env_->RenameFile("/dir/f", "/dir/g"));
  ASSERT_TRUE(!env_->FileExists("/dir/f"));
  ASSERT_TRUE(env_->FileExists("/dir/g"));
  ASSERT_LSMDB_OK(env_->GetFileSize("/dir/g", &file_size));
  ASSERT_EQ(8, file_size);

  // Check that opening non-existent file fails.
  SequentialFile* seq_file;
  RandomAccessFile* random_access_file;
  ASSERT_TRUE(!env_->NewSequentialFile("/dir/non_existent", &seq_file).ok());
  ASSERT_TRUE(!seq_file);
  ASSERT_TRUE(
      !env_->NewRandomAccessFile("/dir/non_existent", &random_access_file)
           .ok());
  ASSERT_TRUE(!random_access_file);

  // Check that deleting works.
  ASSERT_TRUE(!env_->RemoveFile("/dir/non_existent").ok());
  ASSERT_LSMDB_OK(env_->RemoveFile("/dir/g"));
  ASSERT_TRUE(!env_->FileExists("/dir/g"));
  ASSERT_LSMDB_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0, children.size());
  ASSERT_LSMDB_OK(env_->RemoveDir("/dir"));
}

TEST_F(MemEnvTest, ReadWrite) {
  WritableFile* writable_file;
  SequentialFile* sequential_file;
  RandomAccessFile* random_access_file;
  Slice result;
  char scratch[100];

  ASSERT_LSMDB_OK(env_->CreateDir("/dir"));

  ASSERT_LSMDB_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_LSMDB_OK(writable_file->Append("hello "));
  ASSERT_LSMDB_OK(writable_file->Append("world"));
  delete writable_file;

  // Read sequentially.
  ASSERT_LSMDB_OK(env_->NewSequentialFile("/dir/f", &sequential_file));
  ASSERT_LSMDB_OK(sequential_file->Read(5, &result, scratch));  // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_LSMDB_OK(sequential_file->Skip(1));
  ASSERT_LSMDB_OK(
      sequential_file->Read(1000, &result, scratch));  // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_LSMDB_OK(
      sequential_file->Read(100, &result, scratch));  // Try reading past EOF.
  ASSERT_EQ(0, result.size());
  ASSERT_LSMDB_OK(sequential_file->Skip(100));  // Try to skip past end of file.
  ASSERT_LSMDB_OK(sequential_file->Read(1000, &result, scratch));
  delete sequential_file;

  // Random reads.
  ASSERT_LSMDB_OK(env_->NewRandomAccessFile("/dir/f", &random_access_file));
  ASSERT_LSMDB_OK(
      random_access_file->Read(6, 5, &result, scratch));  // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_LSMDB_OK(
      random_access_file->Read(0, 5, &result, scratch));  // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_LSMDB_OK(
      random_access_file->Read(10, 100, &result, scratch));  // Read "d".
  ASSERT_EQ(0, result.compare("d"));

  // Too high offset.
  ASSERT_TRUE(!random_access_file->Read(1000, 5, &result, scratch).ok());
  delete random_access_file;
}

TEST_F(MemEnvTest, Locks) {
  FileLock* lock;

  // These are no-ops, but we test they return success.
  ASSERT_LSMDB_OK(env_->LockFile("some file", &lock));
  ASSERT_LSMDB_OK(env_->UnlockFile(lock));
}

TEST_F(MemEnvTest, Misc) {
  std::string test_dir;
  ASSERT_LSMDB_OK(env_->GetTestDirectory(&test_dir));
  ASSERT_TRUE(!test_dir.empty());

  WritableFile* writable_file;
  ASSERT_LSMDB_OK(env_->NewWritableFile("/a/b", &writable_file));

  // These are no-ops, but we test they return success.
  ASSERT_LSMDB_OK(writable_file->Sync());
  ASSERT_LSMDB_OK(writable_file->Flush());
  ASSERT_LSMDB_OK(writable_file->Close());
  delete writable_file;
}

TEST_F(MemEnvTest, LargeWrite) {
  const size_t kWriteSize = 300 * 1024;
  char* scratch = new char[kWriteSize * 2];

  std::string write_data;
  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  WritableFile* writable_file;
  ASSERT_LSMDB_OK(env_->NewWritableFile("/dir/f", &writable_file));
  ASSERT_LSMDB_OK(writable_file->Append("foo"));
  ASSERT_LSMDB_OK(writable_file->Append(write_data));
  delete writable_file;

  SequentialFile* sequential_file;
  Slice result;
  ASSERT_LSMDB_OK(env_->NewSequentialFile("/dir/f", &sequential_file));
  ASSERT_LSMDB_OK(sequential_file->Read(3, &result, scratch));  // Read "foo".
  ASSERT_EQ(0, result.compare("foo"));

  size_t read = 0;
  std::string read_data;
  while (read < kWriteSize) {
    ASSERT_LSMDB_OK(
        sequential_file->Read(kWriteSize - read, &result, scratch));
    read_data.append(result.data(), result.size());
    read += result.size();
  }
  ASSERT_TRUE(write_data == read_data);
  delete sequential_file;
  delete[] scratch;
}

TEST_F(MemEnvTest, OverwriteOpenFile) {
  const char kWrite1Data[] = "Write #1 data";
  const size_t kFileDataLen = sizeof(kWrite1Data) - 1;
  const std::string kTestFileName = testing::TempDir() + "leveldb-TestFile.dat";

  ASSERT_LSMDB_OK(WriteStringToFile(env_, kWrite1Data, kTestFileName));

  RandomAccessFile* random_access_file;
  ASSERT_LSMDB_OK(
      env_->NewRandomAccessFile(kTestFileName, &random_access_file));

  const char kWrite2Data[] = "Write #2 data";
  ASSERT_LSMDB_OK(WriteStringToFile(env_, kWrite2Data, kTestFileName));

  // Verify that overwriting an open file will result in the new file data
  // being read from files opened before the write.
  Slice result;
  char scratch[kFileDataLen];
  ASSERT_LSMDB_OK(random_access_file->Read(0, kFileDataLen, &result, scratch));
  ASSERT_EQ(0, result.compare(kWrite2Data));

  delete random_access_file;
}

// todo(DBTest)
// TEST_F(MemEnvTest, DBTest) {
// }

}  // namespace lsmdb

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}