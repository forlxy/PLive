//
// Created by wangtao03 on 2019/3/29.
//

#ifndef KWAI_PRIV_AAC_PARSER_H
#define KWAI_PRIV_AAC_PARSER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "ff_ffplay_def.h"

void handlePrivDataInAac(FFPlayer* ffp, AVPacket* pkt);

#endif //KWAI_PRIV_AAC_PARSER_H
