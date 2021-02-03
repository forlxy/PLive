//
//  ContentAvioContext.h
//  IJKMediaFramework
//
//  Created by 李金海 on 2019/7/5.
//  Copyright © 2019 kuaishou. All rights reserved.
//

#pragma once

#include <stdio.h>
#include "libavformat/avio.h"

AVIOContext* ContentAVIOContext_create(char* content);
void ContentAVIOContext_releasep(AVIOContext** pp_avio);

