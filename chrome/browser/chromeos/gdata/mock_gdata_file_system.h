// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_FILE_SYSTEM_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gdata {

// Mock for GDataFileSystemInterface.
class MockGDataFileSystem : public GDataFileSystemInterface {
 public:
  MockGDataFileSystem();
  virtual ~MockGDataFileSystem();

  // GDataFileSystemInterface overrides.
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD1(Authenticate, void(const AuthStatusCallback& callback));
  MOCK_METHOD2(FindFileByPathAsync,
               void(const FilePath& file_path,
                    const FindFileCallback& callback));
  MOCK_METHOD2(FindFileByPathSync, void(const FilePath& file_path,
                                        FindFileDelegate* delegate));
  MOCK_METHOD3(Copy, void(const FilePath& src_file_path,
                          const FilePath& dest_file_path,
                          const FileOperationCallback& callback));
  MOCK_METHOD3(Move, void(const FilePath& src_file_path,
                          const FilePath& dest_file_path,
                          const FileOperationCallback& callback));
  MOCK_METHOD3(Remove, void(const FilePath& file_path,
                            bool is_recursive,
                            const FileOperationCallback& callback));
  MOCK_METHOD4(CreateDirectory,
               void(const FilePath& directory_path,
                    bool is_exclusive,
                    bool is_recursive,
                    const FileOperationCallback& callback));
  MOCK_METHOD2(GetFile, void(const FilePath& file_path,
                             const GetFileCallback& callback));
  MOCK_METHOD2(GetFromCacheForPath,
               void(const FilePath& gdata_file_path,
                    const GetFromCacheCallback& callback));
  MOCK_METHOD0(GetProgressStatusList,
               std::vector<GDataOperationRegistry::ProgressStatus>());
  MOCK_METHOD1(CancelOperation, bool(const FilePath& file_path));
  MOCK_METHOD1(AddOperationObserver,
               void(GDataOperationRegistry::Observer* observer));
  MOCK_METHOD1(RemoveOperationObserver,
               void(GDataOperationRegistry::Observer* observer));
  MOCK_METHOD3(GetCacheState, void(const std::string& resource_id,
                                   const std::string& md5,
                                   const GetCacheStateCallback& callback));
  MOCK_METHOD2(GetFileInfoFromPath, bool(const FilePath& gdata_file_path,
                                         base::PlatformFileInfo* file_info));
  MOCK_METHOD0(GetGDataCacheTmpDirectory, FilePath());
  MOCK_METHOD1(GetAvailableSpace,
               void(const GetAvailableSpaceCallback& callback));
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_FILE_SYSTEM_H_
