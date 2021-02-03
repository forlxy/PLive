//
//  data_source_extend.h
//  IJKMediaPlayer
//
//  Created by 帅龙成 on 2018/6/13.
//  Copyright © 2018 kuaishou. All rights reserved.
//

#ifndef data_source_seekable_h
#define data_source_seekable_h

#include "data_source.h"

namespace kuaishou {
namespace cache {

class DataSourceSeekable : public DataSource {
  public:
    virtual ~DataSourceSeekable() {};

    virtual int64_t Seek(int64_t pos) = 0;

    virtual int64_t ReOpen() = 0;
};

}
}
#endif /* data_source_extend_h */
