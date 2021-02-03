#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hodor_config.h"

/**
 * 重要的事情说三遍:
 * 为了尽量和播放器内部的error不冲突，cache 的错误码范围设定为  (-4000, -1000]
 * 为了尽量和播放器内部的error不冲突，cache 的错误码范围设定为  (-4000, -1000]
 * 为了尽量和播放器内部的error不冲突，cache 的错误码范围设定为  (-4000, -1000]
 */

#define AcResultType int32_t

#define AWESOME_CACHE_ERROR_CODES(X, Y)                                          \
    X(0,    kResultOK,                      "kResultOK",            "No error at all!") \
    X(-1000,    kCacheErrorCodeMax,     "kCacheErrorCodeMax",     "目前cache相关的error不会大于这个值") \
    X(-1001,    kResultExceptionNetIO,     "kResultExceptionNetIO",       "Deprecated") \
    X(-1003,    kResultExceptionWriteFailed,     "kResultExceptionWriteFailed",    "写文件失败") \
    X(-1004,    kResultExceptionDataSourcePositionOutOfRange,     "kResultExceptionDataSourcePositionOutOfRange",   "文件长度超过预期") \
    X(-1005,    kResultExceptionSourceNotOpened_0,     "kResultExceptionSourceNotOpened_0",       "数据源未打开") \
    X(-1006,    kResultExceptionSourceNotOpened_1,     "kResultExceptionSourceNotOpened_1",       "数据源未打开") \
    X(-1007,    kResultExceptionSourceNotOpened_2,     "kResultExceptionSourceNotOpened_2",       "数据源未打开") \
    X(-1008,    kResultExceptionSourceNotOpened_3,     "kResultExceptionSourceNotOpened_3",       "数据源未打开") \
    X(-1009,    kResultExceptionDataSourceSkipDataFail,     "kResultExceptionDataSourceSkipDataFail",       "Deprecated") \
    X(-1010,    kResultEndOfInput,           "kResultEndOfInput",     "A return value for methods where the end of an input was encountered.") \
    X(-1011,    kResultMaxLenghtExceeded,    "kResultMaxLenghtExceeded",   "A return value for methods where the length of parsed data exceeds the maximum length allowed.") \
    X(-1012,    kResultNothingRead,          "kResultNothingRead",       "A return value for methods where nothing was read.") \
    X(-1013,    kResultBufferRead,           "kResultBufferRead",       "A return value for methods where a buffer was read.") \
    X(-1014,    kResultFormatRead,     "kResultFormatRead",       " A return value for methods where a format was read.") \
    X(-1015,    kResultContentAlreadyCached,     "kResultContentAlreadyCached",       "文件已缓存") \
    X(-1016,    kResultEndOfInputAlreadyReadAllData,     "kResultEndOfInputAlreadyReadAllData",       "the end of an input was encountered and all data has been read") \
    Y("CacheException related, kResultCacheExceptionStart ~ kResultCacheExceptionEnd 都是cache相关的exception") \
    X(-1100,    kResultCacheExceptionStart,     "kResultCacheExceptionStart",       "CacheDataSource的错误码起始码") \
    X(-1101,    kResultFFmpegAdapterInnerError,     "kResultFFmpegAdapterInnerError",       "Hodor 适配层内部错误") \
    X(-1102,    kResultSyncCacheDataSourceInnerError,     "kResultSyncCacheDataSourceInnerError",       "") \
    Y("file error related  1110 ~ 1129") \
    X(-1110,    kResultCacheExceptionTouchSpan,     "kResultCacheExceptionTouchSpan",      "") \
    X(-1111,    kResultFileExceptionCreateDirFail,     "kResultFileExceptionCreateDirFail",      "") \
    X(-1113,    kResultFileExceptionIO,     "kResultFileExceptionIO",      "FileWriterMemoryDataSource的错误") \
    X(-1114,    kResultAdvanceDataSinkCloseFlushFail,     "kResultAdvanceDataSinkCloseFlushFail",      "") \
    X(-1115,    kResultAdvanceDataSinkWriteFail,     "kResultAdvanceDataSinkWriteFail",      "") \
    X(-1116,    kResultCachedContentWriteToStreamFail,     "kResultCachedContentWriteToStreamFail",      "") \
    Y("FileDataSource") \
    X(-1120,    kResultFileDataSourceIOError_0,     "kResultFileDataSourceIOError_0",      "") \
    X(-1121,    kResultFileDataSourceIOError_1,     "kResultFileDataSourceIOError_1",      "") \
    X(-1122,    kResultFileDataSourceIOError_2,     "kResultFileDataSourceIOError_2",      "") \
    X(-1123,    kResultFileDataSourceIOError_3,     "kResultFileDataSourceIOError_3",      "") \
    X(-1124,    kResultFileDataSourceIOError_4,     "kResultFileDataSourceIOError_4",      "") \
    Y("~~") \
    X(-1130,    kResultCacheExceptionWriteExceedSpecLength,     "kResultCacheExceptionWriteExceedSpecLength",      "") \
    X(-1131,    kResultAdvanceDataSinkInnerError,     "kResultAdvanceDataSinkInnerError",      "") \
    X(-1132,    kResultTeeDataSinkInnerError,     "kResultTeeDataSinkInnerError",      "") \
    X(-1133,    kResultDefaultHttpDataSourceInnerError,     "kResultDefaultHttpDataSourceInnerError",      "") \
    X(-1134,    kResultTeeDataSinkInnerError_2,     "kResultTeeDataSinkInnerError_2",      "") \
    Y("CachedContentIndex") \
    X(-1140,    kResultCachedContentIndexStoreStartWriteFail,     "kResultCachedContentIndexStoreStartWriteFail",      "") \
    X(-1141,    kResultCachedContentIndexStoreOutputBroken,     "kResultCachedContentIndexStoreOutputBroken",      "") \
    X(-1142,    kResultCachedContentIndexStoreOutputBroken_2,     "kResultCachedContentIndexStoreOutputBroken_2",      "") \
    X(-1199,    kResultCacheExceptionEnd,     "kResultCacheExceptionEnd",      "") \
    Y("~~") \
    X(-1200,    kResultCacheCreateSpanEntryFail,     "kResultCacheCreateSpanEntryFail",      "") \
    Y("Cache Data Source Fails") \
    X(-1300,    kResultCacheDataSourceCreateFail,     "kResultCacheDataSourceCreateFail",      "") \
    X(-1301,    kResultCacheNoMemory,     "kResultCacheNoMemory",      "可能是") \
    X(-1303,    kResultAdapterDataSourceNeverOpened,     "kResultAdapterDataSourceNeverOpened",      "") \
    Y("~~") \
    X(-1400,    kResultSpecExceptionLengthUnset,     "kResultSpecExceptionLengthUnset",      "") \
    X(-1401,    kResultSpecExceptionKeyUnset,     "kResultSpecExceptionKeyUnset",      "") \
    X(-1402,    kResultSpecExceptionUriUnset,     "kResultSpecExceptionUriUnset",      "") \
    Y("ffmpeg adapter") \
    X(-1410,    kResultAdapterReadNoData,     "kResultAdapterReadNoData",      "") \
    X(-1411,    kResultAdapterSeekFail,     "kResultAdapterSeekFail",      "") \
    X(-1412,    kResultAdapterSeekOutOfRange,     "kResultAdapterSeekOutOfRange",      "") \
    X(-1413,    kResultAdapterOpenNoData,     "kResultAdapterOpenNoData",      "") \
    X(-1414,    kResultAdapterReOpenNoData,     "kResultAdapterReOpenNoData",      "") \
    X(-1415,    kResultAdapterReadOpaqueNull,     "kResultAdapterReadOpaqueNull",      "") \
    X(-1416,    kResultAdapterReadDataSourceNull,     "kResultAdapterReadDataSourceNull",      "") \
    Y("BufferDataSource") \
    X(-1420,    kResultBufferDataSourceReadNoData,     "kResultBufferDataSourceReadNoData",      "") \
    X(-1421,    kResultBufferDataSourceOpenDataSourceLengthZero,     "kResultBufferDataSourceOpenDataSourceLengthZero",      "") \
    X(-1422,    kResultBufferDataSourceSeekNoData,     "kResultBufferDataSourceSeekNoData",      "") \
    X(-1423,    kResultBufferDataSourceFillBufferNoData,     "kResultBufferDataSourceFillBufferNoData",      "") \
    Y("BlockingInputStream") \
    X(-1430,    kResultBlockingInputStreamWriteFail,     "kResultBlockingInputStreamWriteFail",      "") \
    X(-1431,    kResultBlockingInputStreamReadInitFail,     "kResultBlockingInputStreamReadInitFail",      "") \
    X(-1432,    kResultBlockingInputStreamReadInterrupted,     "kResultBlockingInputStreamReadInterrupted",      "") \
    X(-1433,    kResultBlockingInputStreamReadInvalidArgs,     "kResultBlockingInputStreamReadInvalidArgs",      "") \
    X(-1434,    kResultBlockingInputStreamReadInnerError,     "kResultBlockingInputStreamReadInnerError",      "") \
    X(-1435,    kResultBlockingInputStreamReadReturnZero,     "kResultBlockingInputStreamReadReturnZero",      "") \
    X(-1436,    kResultBlockingInputStreamEndOfStram,     "kResultBlockingInputStreamEndOfStram",      "") \
    \
    X(-1500,    kResultOfflineCacheUnSupported,     "kResultOfflineCacheUnSupported",      "") \
    \
    X(-1599,    kResultLiveNoData, "kResultLiveNoData", "直播数据源read返回了0") \
    \
    Y("FFUrlDataSource") \
    X(-1600,    kResultFFurlProtocolNotFound, "kResultFFurlProtocolNotFound", "") \
    X(-1601,    kResultFFurlIOError, "kResultFFurlIOError", "") \
    X(-1602,    kResultFFurlTimeout, "kResultFFurlTimeout", "") \
    X(-1603,    kResultFFurlHttp4xx, "kResultFFurlHttp4xx", "") \
    X(-1604,    kResultFFurlHttp5xx, "kResultFFurlHttp5xx", "") \
    X(-1605,    kResultFFurlInvalidData, "kResultFFurlInvalidData", "") \
    X(-1606,    kResultFFurlExit, "kResultFFurlExit", "") \
    X(-1620,    kResultFFurlUnknown, "kResultFFurlUnknown", "") \
    Y("[-1899 ~ 4000)目前都是网络错误") \
    X(-1901,    kResultExceptionNetDataSourceReadTimeout,     "kResultExceptionNetDataSourceReadTimeout",      "") \
    X(-1902,    kResultExceptionHttpDataSourceNoContentLength,     "kResultExceptionHttpDataSourceNoContentLength",      "") \
    X(-1903,    kResultExceptionHttpDataSourceInvalidContentLength,     "kResultExceptionHttpDataSourceInvalidContentLength",      "") \
    X(-1904,    kResultExceptionHttpDataSourceInvalidContentLengthForDrop,     "kResultExceptionHttpDataSourceInvalidContentLengthForDrop",      "") \
    X(-1905,    kResultExceptionHttpDataSourcePositionOutOfRange,     "kResultExceptionHttpDataSourcePositionOutOfRange",      "") \
    X(-1906,    kResultExceptionHttpDataSourceCurlInitFail,     "kResultExceptionHttpDataSourceCurlInitFail",      "") \
    X(-1907,    kResultExceptionHttpDataSourceHeaderNotParsed,     "kResultExceptionHttpDataSourceHeaderNotParsed",      "一般遇到404会遇到这个错误（ParseHeader没机会执行）") \
    X(-1908,    kResultExceptionHttpDataSourceReadNoData,     "kResultExceptionHttpDataSourceReadNoData",      "") \
    X(-1909,    kResultExceptionHttpDataSourceByteRangeInvalid,     "kResultExceptionHttpDataSourceByteRangeInvalid",      "") \
    X(-1910,    kResultExceptionHttpDataSourceByteRangeLenInvalid,     "kResultExceptionHttpDataSourceByteRangeLenInvalid",      "") \
    Y("几个网络错误的base code") \
    X(-2000,    kHttpInvalidResponseCodeBase,     "kHttpInvalidResponseCodeBase",      "") \
    X(-2800,    kLibcurlErrorBase,      "kLibcurlErrorBase",      "libCurl相关的下载错误码偏移值") \
    X(-3000,    kNsUrlErrorBase,        "kNsUrlErrorBase",      "ios_download_task的下载错误偏移值，deprecated") \
    X(-3100,    kCurlHttpNoEngouthMemory,        "kCurlHttpNoEngouthMemory",      "内存不足") \
    X(-3101,    kCurlHttpServerNotSupportRange,        "kCurlHttpServerNotSupportRange",      "服务器不支持range请求") \
    X(-3102,    kCurlHttpResponseRangeInvalid,        "kCurlHttpResponseRangeInvalid",      "服务器返回的range不符合预期") \
    X(-3103,    kCurlHttpResponseHeaderInvalid,        "kCurlHttpResponseHeaderInvalid",      "服务器返回的header不合法") \
    X(-3104,    kCurlHttpResponseCodeFail,        "kCurlHttpResponseCodeFail",      "服务器返回的response code表示失败了") \
    X(-3105,    kCurlHttpInternalError_1,        "kCurlHttpInternalError_1",      "WriteCallback的数据比预期的多") \
    X(-3106,    kCurlHttpStartThreadFail,        "kCurlHttpStartThreadFail",      "CurlHttp开线程失败") \
    X(-3107,    kCurlHttpPositionOverflowContentLength,        "kCurlHttpPositionOverflowContentLength",      "请求的position大于文件总长度") \
    X(-3120,    kAsyncCacheV2NoMemory,        "kAsyncCacheV2NoMemory",      "AsyncCacheV2无内存") \
    X(-3121,    kAsyncCacheInnerError_1,        "kAsyncCacheInnerError_1",      "所有InnerError都是不应该走到的地方却走到了") \
    X(-3122,    kAsyncCacheInnerError_2,        "kAsyncCacheInnerError_2",      "kAsyncCacheInnerError_2") \
    X(-3123,    kAsyncCacheInnerError_3,        "kAsyncCacheInnerError_3",      "kAsyncCacheInnerError_3") \
    X(-3124,    kAsyncCacheInnerError_4,        "kAsyncCacheInnerError_4",      "kAsyncCacheInnerError_4") \
    X(-3125,    kAsyncCacheSeePosOverflow,        "kAsyncCacheSeePosOverflow",      "seek的pos比超过最大长度") \
    X(-3126,    kAsyncCacheInnerError_5,        "kAsyncCacheInnerError_5",      "kAsyncCacheInnerError_5") \
    X(-3127,    kAsyncCacheInnerError_6,        "kAsyncCacheInnerError_6",      "kAsyncCacheInnerError_6") \
    X(-3128,    kAsyncCacheInnerError_7,        "kAsyncCacheInnerError_7",      "kAsyncCacheInnerError_7") \
    X(-3129,    kAsyncCacheInnerError_8,        "kAsyncCacheInnerError_8",      "kAsyncCacheInnerError_8") \
    X(-3130,    kAsyncCacheInnerError_9,        "kAsyncCacheInnerError_9",      "kAsyncCacheInnerError_9") \
    X(-3131,    kAsyncCacheInnerError_10,        "kAsyncCacheInnerError_10",      "kAsyncCacheInnerError_10") \
    X(-3132,    kAsyncCacheSpecPositionOverflow1,        "kAsyncCacheSpecPositionOverflow",      "输入给AsyncCacheDataSource的position超过了文件最大长度") \
    X(-3133,    kAsyncCacheSpecPositionOverflow2,        "kAsyncCacheSpecPositionOverflow2",      "输入给AsyncCacheDataSource的position超过了文件最大长度") \
    X(-3140,    kAsyncScopeSeekPosOverflow,        "kAsyncScopeSeekPosOverflow",      "AsyncScope的seek位置超出范围了") \
    X(-3141,    kAsyncScopeSeekPosExceedInputExpectLen,        "kAsyncScopeSeekPosExceedInputExpectLen",      "AsyncScope的seek位置一开始约定好的要下载的范围，属于inner error") \
    X(-3142,    kAsyncScopeEOF,        "kAsyncScopeEof",      "AsyncScope read时候遇到EOF，属于inner error") \
    X(-3143,    kAsyncScopeDecryptFail,        "kAsyncScopeDecryptFail",      "AsyncScope内的aes解码失败") \
    X(-3144,    kAsyncScopeInnererror_1,        "kAsyncScopeInnererror_1",      "kAsyncScopeInnererror_1") \
    X(-3145,    kAsyncScopeInnererror_2,        "kAsyncScopeInnererror_2",      "kAsyncScopeInnererror_2") \
    X(-3146,    kAsyncScopeInnererror_3,        "kAsyncScopeInnererror_3",      "解码长度InnerError") \
    X(-3147,    kAsyncScopeAesEcryptSourceLengthInvalid,        "kAsyncScopeAesEcryptSourceLengthInvalid",      "加密数据源不是16字节整数倍") \
    X(-3150,    kCacheScopeSinkPathEmpty,  "kCacheScopeSinkPathEmpty",      "CacheScopeSink文件名为空") \
    X(-3151,    kCacheScopeSinkOpenFail,  "kCacheScopeSinkOpenFail",      "CacheScopeSink打开文件失败") \
    X(-3152,    kCacheScopeSinkWriteFail,  "kCacheScopeSinkWriteFail",      "CacheScopeSink写入文件失败") \
    X(-3153,    kCacheScopeSinkWriteOverflow,  "kCacheScopeSinkWriteOverflow",      "CacheScopeSink写入超长了，可能内部逻辑有问题") \
    X(-3160,    kCacheContentV2DataSinkContentLengthInvalid,  "kCacheContentV2DataSinkContentLengthInvalid",      "输入的ContentLength有问题") \
    X(-3161,    kCacheContentV2DataSinkNotSupportPositionNonZero,  "kCacheContentV2DataSinkNotSupportPositionNonZero",      "CacheContentV2DataSink不支持的操作") \
    X(-3162,    kCacheContentV2DataSinkNotOverflow,  "kCacheContentV2DataSinkNotOverflow",      "CacheContentV2DataSink write溢出") \
    X(-3163,    kCacheContentV2DataSinkInnerError_1,  "kCacheContentV2DataSinkInnerError_1",      "kCacheContentV2DataSinkInnerError_1") \
    X(-3164,    kCacheContentV2DataSinkInnerError_2,  "kCacheContentV2DataSinkInnerError_2",      "kCacheContentV2DataSinkInnerError_2") \
    X(-3170,    kCacheIndexV2TryStore_XXXX_FIXME,  "",      "索引文件写入失败") \
    X(-3180,    kCacheContentV2WithScopeMD5InitFail,  "",      "MD5初始化失败") \
    X(-3181,    kCacheContentV2WithScopeMD5VerfityFail,  "",      "MD5校验失败") \
    X(-3182,    kCacheContentV2WithScopeMD5ContentLengthInvalid,  "",      "MD5初始化失败") \
    X(-3190,    kCacheUtilFileNotExist,  "kCacheUtilFileNotExist",      "CacheUtil读取文件不存在") \
    X(-3191,    kCacheUtilReadFail,  "kCacheUtilReadFail",      "CacheUtil读取文件失败") \
    X(-3192,    kCacheUtilReadOverflow,  "kCacheUtilReadOverflow",      "CacheUtil读取文件越界") \
    X(-3193,    kCacheUtilFilePathEmpty,  "kCacheUtilFilePathEmpty",      "CacheUtil读取文件Path为空") \
    X(-3200,    kCacheDirManagerMediaCommitScopeFileFail,  "kCacheDirManagerMediaCommitScopeFileFail",      "DirManagerMedia OnCacheScopeFileFlushed失败，属于比较严重的inner error") \
    X(-3300,    kHodorCurlStepTaskScopeListInnerError_1,  "kHodorCurlStepTaskScopeListInnerError_1",      "kHodorCurlStepTaskScopeListInnerError_1") \
    X(-3301,    kHodorCurlStepTaskScopeListInnerError_2,  "kHodorCurlStepTaskScopeListInnerError_2",      "kHodorCurlStepTaskScopeListInnerError_2") \
    X(-3310,    kHodorSingFileDownloadTaskFlushFileFail,  "kHodorSingFileDownloadTaskFlushFileFail",      "静态资源下载写文件失败") \
    X(-3311,    kHodorSingFileDownloadTaskOOM,  "kHodorSingFileDownloadTaskOOM",      "静态资源下载分配内存失败") \
    Y("[-3900, -4000) 给Cronet HTTP Task") \
    X(-3900,    kCronetCreateFailed, "kCronetCreateFailed", "Cronet初始化错误") \
    X(-3901,    kCronetHttpResponseHeaderInvalid, "kCronetHttpResponseHeaderInvalid", "服务器返回的http头有问题") \
    Y("(-4000, -5000) 都给Cronet内部错误") \
    X(-4000,    kCronetInternalErrorBase, "kCronetInternalErrorBase", "Cronet相关的下载错误码偏移值") \
    X(-5000,    kCacheErrorCodeMin,     "kCacheErrorCodeMin",     "目前cache相关的error不会小于这个值")

/**
 * Cache task related.
 */
static const int kTaskSuccess = 0;
static const int kTaskFailReasonWriteFile = 1;
static const int kTaskFailReasonOpenDataSource = 2;
static const int kTaskFailReasonReadFail = 3;
static const int kTaskFailReasonCreateTaskFail = 4;
static const int kTaskFailReasonCancel = 5;


#define AWESOME_CACHE_ERROR_ENUM(ID, NAME, ERR_MSG, ERR_DESC) NAME = ID,
#define DIVIDER_AND_COMMENT(a)

typedef enum AwesomeCacheErrorCode {
    AWESOME_CACHE_ERROR_CODES(AWESOME_CACHE_ERROR_ENUM, DIVIDER_AND_COMMENT)
} AwesomeCacheErrorCode;

#ifdef __cplusplus
extern "C" {
#endif

HODOR_EXPORT bool is_cache_error(int error);

HODOR_EXPORT bool is_cache_abort_by_callback_error_code(int error);

HODOR_EXPORT const char* cache_error_msg(int code);

HODOR_EXPORT const char* cache_error_desc(int code);

#ifdef __cplusplus
}
#endif
