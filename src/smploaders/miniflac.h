/* SPDX-License-Identifier: 0BSD
Copyright (C) 2022 John Regan <john@jrjrtech.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef MINIFLAC_H
#define MINIFLAC_H

/*

Building
========

In one C file, define MINIFLAC_IMPLEMENTATION before including this header, like:
#define MINIFLAC_IMPLEMENTATION
#include "miniflac.h"

Usage
=====

You'll want to allocate a decoder, initialize it with miniflac_init, then start
pushing data into it with functions like miniflac_sync or miniflac_decode.

All the "feeding" functions take the same first 4 parameters:
  1. miniflac* pFlac - a pointer to a miniflac struct.
  2. const uint8_t* data - a pointer to a stream of bytes to push.
  3. uint32_t length - the length of your stream of bytes.
  4. uint32_t* out_length - an out-variable, it will be updated with the
                            number of bytes consumed.
You'll to save any un-consumed bytes for your next call.

All the "feeding" / public functions return a MINIFLAC_RESULT enum, these are the
important values:
  < 0 : error
  0 : more data is required (MINIFLAC_CONTINUE)
  1 : success (MINIFLAC_OK)

You can use miniflac_sync to sync to a block boundary. It will automatically parse
a metadata block header or frame header, so you can inspect it and check on things
like the block size, bits-per-sample, etc. See the various struct definitions down
below. The example program in "examples/basic-decoder.c" does this. Keep calling
it until you get something other than MINIFLAC_CONTINUE (0). MINIFLAC_OK (1) means
you've found a block boundary. Anything < 0 is an error.

You can look at the .state field in the miniflac struct to see if you're in a
metadata block or audio frame, and proceed accordingly.

Decoding audio is done with miniflac_decode. You don't *have* to use miniflac_sync first,
you could just call miniflac_decode (this is done in the example program
"examples/single-byte-decoder.c"). The advantage of miniflac_sync is you can
look at the frame properties in the header before you start decoding.

miniflac_decode behaves similarly to miniflac_sync - when it returns MINIFLAC_OK (1),
your output buffer will have decoded audio samples in it. You'll be at the end
of the audio frame, so you can either call miniflac_sync to check the header of the next
frame, or call miniflac_decode to continue on.

*/

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#if !defined(MINIFLAC_API)
    #ifdef MINIFLAC_DLL
        #ifdef _WIN32
            #define MINIFLAC_DLL_IMPORT  __declspec(dllimport)
            #define MINIFLAC_DLL_EXPORT  __declspec(dllexport)
            #define MINIFLAC_DLL_PRIVATE static
        #else
            #if defined(__GNUC__) && __GNUC__ >= 4
                #define MINIFLAC_DLL_IMPORT  __attribute__((visibility("default")))
                #define MINIFLAC_DLL_EXPORT  __attribute__((visibility("default")))
                #define MINIFLAC_DLL_PRIVATE __attribute__((visibility("hidden")))
            #else
                #define MINIFLAC_DLL_IMPORT
                #define MINIFLAC_DLL_EXPORT
                #define MINIFLAC_DLL_PRIVATE static
            #endif
        #endif

        #ifdef MINIFLAC_IMPLEMENTATION
            #define MINIFLAC_API  MINIFLAC_DLL_EXPORT
        #else
            #define MINIFLAC_API  MINIFLAC_DLL_IMPORT
        #endif
        #define MINIFLAC_PRIVATE MINIFLAC_DLL_PRIVATE
    #else
        #define MINIFLAC_API extern
        #define MINIFLAC_PRIVATE static
    #endif
#endif

#if defined(__GNUC__) && __GNUC__ > 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5)
#define MINIFLAC_CONST __attribute__((__const__))
#endif

#define MINIFLAC_APPLICATION_H
#define MINIFLAC_COMMON_H
#define MINIFLAC_BITREADER_H
#define MINIFLAC_CUESHEET_H
#define MINIFLAC_FRAME_H
#define MINIFLAC_FRAMEHEADER_H
#define MINIFLAC_METADATA_H
#define MINIFLAC_METADATA_HEADER_H
#define MINIFLAC_OGG_H
#define MINIFLAC_OGGHEADER_H
#define MINIFLAC_PADDING_H
#define MINIFLAC_PICTURE_H
#define MINIFLAC_RESIDUAL_H
#define MINIFLAC_SEEKTABLE_H
#define MINIFLAC_STREAMINFO_H
#define MINIFLAC_STREAMMARKER_H
#define MINIFLAC_SUBFRAME_CONSTANT_H
#define MINIFLAC_SUBFRAME_FIXED_H
#define MINIFLAC_SUBFRAME_H
#define MINIFLAC_SUBFRAME_HEADER_H
#define MINIFLAC_SUBFRAME_LPC_H
#define MINIFLAC_SUBFRAME_VERBATIM_H
#define MINIFLAC_UNPACK_H
#define MINIFLAC_VORBIS_COMMENT_H
#define MFLAC_H

#ifndef MINIFLAC_CONST
#define MINIFLAC_CONST
#endif

enum MINIFLAC_RESULT {
    MINIFLAC_OGG_HEADER_NOTFLAC                = -18, /* attempted to read an Ogg header packet that isn't a FLAC-in-Ogg packet */
    MINIFLAC_SUBFRAME_RESERVED_TYPE            = -17, /* subframe header specified a reserved type */
    MINIFLAC_SUBFRAME_RESERVED_BIT             = -16, /* subframe header found a non-zero value in the reserved bit */
    MINIFLAC_STREAMMARKER_INVALID              = -15, /* encountered an illegal value while parsing the fLaC stream marker */
    MINIFLAC_RESERVED_CODING_METHOD            = -14, /* a residual block used a reserved coding method */
    MINIFLAC_METADATA_TYPE_RESERVED            = -13, /* a metadata header used a reserved type */
    MINIFLAC_METADATA_TYPE_INVALID             = -12, /* a metadata header used a invalid type */
    MINIFLAC_FRAME_RESERVED_SAMPLE_SIZE        = -11, /* the frame header lists reserved sample size */
    MINIFLAC_FRAME_RESERVED_CHANNEL_ASSIGNMENT = -10, /* the frame header lists reserved channel assignment */
    MINIFLAC_FRAME_INVALID_SAMPLE_SIZE         =  -9, /* the frame header sample rate was invalid */
    MINIFLAC_FRAME_INVALID_SAMPLE_RATE         =  -8, /* the frame header sample rate was invalid */
    MINIFLAC_FRAME_RESERVED_BLOCKSIZE          =  -7, /* the frame header lists a reserved block size */
    MINIFLAC_FRAME_RESERVED_BIT2               =  -6, /* the second reserved bit was non-zero when parsing the frame header */
    MINIFLAC_FRAME_RESERVED_BIT1               =  -5, /* the first reserved bit was non-zero when parsing the frame header */
    MINIFLAC_FRAME_SYNCCODE_INVALID            =  -4, /* error when parsing a header sync code */
    MINIFLAC_FRAME_CRC16_INVALID               =  -3, /* error in crc16 while decoding frame footer */
    MINIFLAC_FRAME_CRC8_INVALID                =  -2, /* error in crc8 while decoding frame header */
    MINIFLAC_ERROR                             =  -1, /* generic error, likely in an invalid state */
    MINIFLAC_CONTINUE                          =   0, /* needs more data, otherwise fine */
    MINIFLAC_OK                                =   1, /* generic "OK" */
    MINIFLAC_METADATA_END                      =   2, /* used to signify end-of-data in a metadata block */
};

enum MINIFLAC_OGGHEADER_STATE {
    MINIFLAC_OGGHEADER_PACKETTYPE,
    MINIFLAC_OGGHEADER_F,
    MINIFLAC_OGGHEADER_L,
    MINIFLAC_OGGHEADER_A,
    MINIFLAC_OGGHEADER_C,
    MINIFLAC_OGGHEADER_MAJOR,
    MINIFLAC_OGGHEADER_MINOR,
    MINIFLAC_OGGHEADER_HEADERPACKETS,
};

enum MINIFLAC_OGG_STATE {
    MINIFLAC_OGG_CAPTUREPATTERN_O,
    MINIFLAC_OGG_CAPTUREPATTERN_G1,
    MINIFLAC_OGG_CAPTUREPATTERN_G2,
    MINIFLAC_OGG_CAPTUREPATTERN_S,
    MINIFLAC_OGG_VERSION,
    MINIFLAC_OGG_HEADERTYPE,
    MINIFLAC_OGG_GRANULEPOS,
    MINIFLAC_OGG_SERIALNO,
    MINIFLAC_OGG_PAGENO,
    MINIFLAC_OGG_CHECKSUM,
    MINIFLAC_OGG_PAGESEGMENTS,
    MINIFLAC_OGG_SEGMENTTABLE,
    MINIFLAC_OGG_DATA,
    MINIFLAC_OGG_SKIP,
};

enum MINIFLAC_STREAMMARKER_STATE {
    MINIFLAC_STREAMMARKER_F,
    MINIFLAC_STREAMMARKER_L,
    MINIFLAC_STREAMMARKER_A,
    MINIFLAC_STREAMMARKER_C,
};

enum MINIFLAC_METADATA_TYPE {
    MINIFLAC_METADATA_STREAMINFO     = 0,
    MINIFLAC_METADATA_PADDING        = 1,
    MINIFLAC_METADATA_APPLICATION    = 2,
    MINIFLAC_METADATA_SEEKTABLE      = 3,
    MINIFLAC_METADATA_VORBIS_COMMENT = 4,
    MINIFLAC_METADATA_CUESHEET       = 5,
    MINIFLAC_METADATA_PICTURE        = 6,
    MINIFLAC_METADATA_INVALID      = 127,
    MINIFLAC_METADATA_UNKNOWN      = 128,
};

enum MINIFLAC_METADATA_HEADER_STATE {
    MINIFLAC_METADATA_LAST_FLAG,
    MINIFLAC_METADATA_BLOCK_TYPE,
    MINIFLAC_METADATA_LENGTH,
};

enum MINIFLAC_STREAMINFO_STATE {
    MINIFLAC_STREAMINFO_MINBLOCKSIZE,
    MINIFLAC_STREAMINFO_MAXBLOCKSIZE,
    MINIFLAC_STREAMINFO_MINFRAMESIZE,
    MINIFLAC_STREAMINFO_MAXFRAMESIZE,
    MINIFLAC_STREAMINFO_SAMPLERATE,
    MINIFLAC_STREAMINFO_CHANNELS,
    MINIFLAC_STREAMINFO_BPS,
    MINIFLAC_STREAMINFO_TOTALSAMPLES,
    MINIFLAC_STREAMINFO_MD5,
};

enum MINIFLAC_VORBISCOMMENT_STATE {
    MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH,
    MINIFLAC_VORBISCOMMENT_VENDOR_STRING,
    MINIFLAC_VORBISCOMMENT_TOTAL_COMMENTS,
    MINIFLAC_VORBISCOMMENT_COMMENT_LENGTH,
    MINIFLAC_VORBISCOMMENT_COMMENT_STRING,
};

enum MINIFLAC_PICTURE_STATE {
    MINIFLAC_PICTURE_TYPE,
    MINIFLAC_PICTURE_MIME_LENGTH,
    MINIFLAC_PICTURE_MIME_STRING,
    MINIFLAC_PICTURE_DESCRIPTION_LENGTH,
    MINIFLAC_PICTURE_DESCRIPTION_STRING,
    MINIFLAC_PICTURE_WIDTH,
    MINIFLAC_PICTURE_HEIGHT,
    MINIFLAC_PICTURE_COLORDEPTH,
    MINIFLAC_PICTURE_TOTALCOLORS,
    MINIFLAC_PICTURE_PICTURE_LENGTH,
    MINIFLAC_PICTURE_PICTURE_DATA,
};

enum MINIFLAC_CUESHEET_STATE {
    MINIFLAC_CUESHEET_CATALOG,
    MINIFLAC_CUESHEET_LEADIN,
    MINIFLAC_CUESHEET_CDFLAG,
    MINIFLAC_CUESHEET_SHEET_RESERVE,
    MINIFLAC_CUESHEET_TRACKS,
    MINIFLAC_CUESHEET_TRACKOFFSET,
    MINIFLAC_CUESHEET_TRACKNUMBER,
    MINIFLAC_CUESHEET_TRACKISRC,
    MINIFLAC_CUESHEET_TRACKTYPE,
    MINIFLAC_CUESHEET_TRACKPREEMPH,
    MINIFLAC_CUESHEET_TRACK_RESERVE,
    MINIFLAC_CUESHEET_TRACKPOINTS,
    MINIFLAC_CUESHEET_INDEX_OFFSET,
    MINIFLAC_CUESHEET_INDEX_NUMBER,
    MINIFLAC_CUESHEET_INDEX_RESERVE,
};

enum MINIFLAC_SEEKTABLE_STATE {
    MINIFLAC_SEEKTABLE_SAMPLE_NUMBER,
    MINIFLAC_SEEKTABLE_SAMPLE_OFFSET,
    MINIFLAC_SEEKTABLE_SAMPLES,
};

enum MINIFLAC_APPLICATION_STATE {
    MINIFLAC_APPLICATION_ID,
    MINIFLAC_APPLICATION_DATA,
};

enum MINIFLAC_METADATA_STATE {
    MINIFLAC_METADATA_HEADER,
    MINIFLAC_METADATA_DATA,
};

enum MINIFLAC_RESIDUAL_STATE {
    MINIFLAC_RESIDUAL_CODING_METHOD,
    MINIFLAC_RESIDUAL_PARTITION_ORDER,
    MINIFLAC_RESIDUAL_RICE_PARAMETER,
    MINIFLAC_RESIDUAL_RICE_SIZE, /* used when rice_parameter is an escape code */
    MINIFLAC_RESIDUAL_RICE_VALUE, /* used when rice_parameter is an escape code */
    MINIFLAC_RESIDUAL_MSB, /* used when reading MSB bits */
    MINIFLAC_RESIDUAL_LSB, /* used when reading MSB bits */
};

enum MINIFLAC_SUBFRAME_FIXED_STATE {
    MINIFLAC_SUBFRAME_FIXED_DECODE,
};

enum MINIFLAC_SUBFRAME_LPC_STATE {
    MINIFLAC_SUBFRAME_LPC_PRECISION,
    MINIFLAC_SUBFRAME_LPC_SHIFT,
    MINIFLAC_SUBFRAME_LPC_COEFF,
};

enum MINIFLAC_SUBFRAME_CONSTANT_STATE {
    MINIFLAC_SUBFRAME_CONSTANT_DECODE,
};

enum MINIFLAC_SUBFRAME_VERBATIM_STATE {
    MINIFLAC_SUBFRAME_VERBATIM_DECODE,
};

enum MINIFLAC_SUBFRAME_TYPE {
    MINIFLAC_SUBFRAME_TYPE_UNKNOWN,
    MINIFLAC_SUBFRAME_TYPE_CONSTANT,
    MINIFLAC_SUBFRAME_TYPE_FIXED,
    MINIFLAC_SUBFRAME_TYPE_LPC,
    MINIFLAC_SUBFRAME_TYPE_VERBATIM,
};

enum MINIFLAC_SUBFRAME_HEADER_STATE {
    MINIFLAC_SUBFRAME_HEADER_RESERVEBIT1,
    MINIFLAC_SUBFRAME_HEADER_KIND,
    MINIFLAC_SUBFRAME_HEADER_WASTED_BITS,
    MINIFLAC_SUBFRAME_HEADER_UNARY,
};

enum MINIFLAC_SUBFRAME_STATE {
    MINIFLAC_SUBFRAME_HEADER,
    MINIFLAC_SUBFRAME_CONSTANT,
    MINIFLAC_SUBFRAME_VERBATIM,
    MINIFLAC_SUBFRAME_FIXED,
    MINIFLAC_SUBFRAME_LPC,
};

enum MINIFLAC_CHASSGN {
    MINIFLAC_CHASSGN_NONE,
    MINIFLAC_CHASSGN_LEFT_SIDE,
    MINIFLAC_CHASSGN_RIGHT_SIDE,
    MINIFLAC_CHASSGN_MID_SIDE,
};

enum MINIFLAC_FRAME_HEADER_STATE {
    MINIFLAC_FRAME_HEADER_SYNC,
    MINIFLAC_FRAME_HEADER_RESERVEBIT_1,
    MINIFLAC_FRAME_HEADER_BLOCKINGSTRATEGY,
    MINIFLAC_FRAME_HEADER_BLOCKSIZE,
    MINIFLAC_FRAME_HEADER_SAMPLERATE,
    MINIFLAC_FRAME_HEADER_CHANNELASSIGNMENT,
    MINIFLAC_FRAME_HEADER_SAMPLESIZE,
    MINIFLAC_FRAME_HEADER_RESERVEBIT_2,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_1,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_2,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_3,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_4,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_5,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_6,
    MINIFLAC_FRAME_HEADER_SAMPLENUMBER_7,
    MINIFLAC_FRAME_HEADER_BLOCKSIZE_MAYBE,
    MINIFLAC_FRAME_HEADER_SAMPLERATE_MAYBE,
    MINIFLAC_FRAME_HEADER_CRC8,
};

enum MINIFLAC_FRAME_STATE {
    MINIFLAC_FRAME_HEADER,
    MINIFLAC_FRAME_SUBFRAME,
    MINIFLAC_FRAME_FOOTER,
};

enum MINIFLAC_STATE {
    MINIFLAC_OGGHEADER, /* will try to find an ogg header */
    MINIFLAC_STREAMMARKER_OR_FRAME, /* poke for a stream marker or audio frame
    */
    MINIFLAC_STREAMMARKER, /* reading a stream marker */
    MINIFLAC_METADATA_OR_FRAME, /* will try to find a frame sync code or try to parse a metadata block */
    MINIFLAC_METADATA, /* currently reading a metadata block */
    MINIFLAC_FRAME,    /* currently reading an audio frame */
};

enum MINIFLAC_CONTAINER {
    MINIFLAC_CONTAINER_UNKNOWN,
    MINIFLAC_CONTAINER_NATIVE,
    MINIFLAC_CONTAINER_OGG,
};

enum MFLAC_RESULT {
    MFLAC_EOF          = 0,
    MFLAC_OK           = 1,
    MFLAC_METADATA_END = 2,
};


typedef size_t (*mflac_readcb)(uint8_t* buffer, size_t bytes, void* userdata);

struct miniflac_bitreader_s {
    uint64_t val;
    uint8_t  bits;
    uint8_t  crc8;
    uint16_t crc16;
    uint32_t pos;
    uint32_t len;
    const uint8_t* buffer;
    uint32_t tot; /* total bytes read since last reset */
};

struct miniflac_oggheader_s {
    enum MINIFLAC_OGGHEADER_STATE state;
};

struct miniflac_ogg_s {
    enum MINIFLAC_OGG_STATE state;
    struct miniflac_bitreader_s br; /* maintain our own bitreader */
    uint8_t version;
    uint8_t headertype;
    int64_t granulepos;
    int32_t serialno;
    uint32_t pageno;
    uint8_t segments;
    uint8_t curseg; /* current position within the segment table */
    uint16_t length; /* length of data within page */
    uint16_t pos; /* where we are within page */
};

struct miniflac_streammarker_s {
    enum MINIFLAC_STREAMMARKER_STATE state;
};

struct miniflac_metadata_header_s {
    enum MINIFLAC_METADATA_HEADER_STATE    state;
    uint8_t                         is_last;
    uint8_t                        type_raw;
    enum MINIFLAC_METADATA_TYPE             type;
    uint32_t                         length;
};

struct miniflac_streaminfo_s {
    enum MINIFLAC_STREAMINFO_STATE state;
    uint8_t                     pos;
    uint32_t            sample_rate;
    uint8_t                     bps;
};

struct miniflac_vorbis_comment_s {
    enum MINIFLAC_VORBISCOMMENT_STATE    state;
    uint32_t len; /* length of the current string we're decoding */
    uint32_t pos; /* position within current string */
    uint32_t tot; /* total comments */
    uint32_t cur; /* current comment being decoded */
};

struct miniflac_picture_s {
    enum MINIFLAC_PICTURE_STATE    state;
    uint32_t len; /* length of the current string/data we're decoding */
    uint32_t pos; /* position within current string */
};

struct miniflac_cuesheet_s {
    enum MINIFLAC_CUESHEET_STATE state;
    uint32_t pos;
    uint8_t track;
    uint8_t tracks;
    uint8_t point;
    uint8_t points;
};

struct miniflac_seektable_s {
    enum MINIFLAC_SEEKTABLE_STATE    state;
    uint32_t len; /* number of seekpoints */
    uint32_t pos; /* current seekpoint */
};

struct miniflac_application_s {
    enum MINIFLAC_APPLICATION_STATE state;
    uint32_t len; /* length of data */
    uint32_t pos; /* current byte */
};

struct miniflac_padding_s {
    uint32_t len; /* length of data */
    uint32_t pos; /* current byte */
};

struct miniflac_metadata_s {
    enum MINIFLAC_METADATA_STATE               state;
    uint32_t                                     pos;
    struct miniflac_metadata_header_s         header;
    struct miniflac_streaminfo_s          streaminfo;
    struct miniflac_vorbis_comment_s  vorbis_comment;
    struct miniflac_picture_s                picture;
    struct miniflac_cuesheet_s              cuesheet;
    struct miniflac_seektable_s            seektable;
    struct miniflac_application_s        application;
    struct miniflac_padding_s                padding;
};

struct miniflac_residual_s {
    enum MINIFLAC_RESIDUAL_STATE state;
    uint8_t coding_method;
    uint8_t partition_order;
    uint8_t rice_parameter;
    uint8_t rice_size;
    uint32_t msb; /* unsure what the max for this is */
    uint8_t rice_parameter_size; /* 4 or 5 based on coding method */
    int32_t value; /* current residual value */

    uint32_t partition; /* current partition */
    uint32_t partition_total; /* total partitions */

    uint32_t residual; /* current residual within partition */
    uint32_t residual_total; /* total residuals in partition */
};

struct miniflac_subframe_fixed_s {
    enum MINIFLAC_SUBFRAME_FIXED_STATE state;
    uint32_t pos;
};

struct miniflac_subframe_lpc_s {
    enum MINIFLAC_SUBFRAME_LPC_STATE state;
    uint32_t pos;
    uint8_t precision;
    uint8_t shift;
    uint8_t coeff;
    int32_t coefficients[32];
};

struct miniflac_subframe_constant_s {
    enum MINIFLAC_SUBFRAME_CONSTANT_STATE state;
};

struct miniflac_subframe_verbatim_s {
    enum MINIFLAC_SUBFRAME_VERBATIM_STATE state;
    uint32_t pos;
};

struct miniflac_subframe_header_s {
    enum MINIFLAC_SUBFRAME_HEADER_STATE state;
    enum MINIFLAC_SUBFRAME_TYPE type;
    uint8_t order;
    uint8_t wasted_bits;
    uint8_t type_raw;
};

struct miniflac_subframe_s {
    enum MINIFLAC_SUBFRAME_STATE state;
    uint8_t bps; /* effective bps for this subframe */
    struct miniflac_subframe_header_s header;
    struct miniflac_subframe_constant_s constant;
    struct miniflac_subframe_verbatim_s verbatim;
    struct miniflac_subframe_fixed_s fixed;
    struct miniflac_subframe_lpc_s lpc;
    struct miniflac_residual_s residual;
};

struct miniflac_frame_header_s {
    enum MINIFLAC_FRAME_HEADER_STATE state;
    uint8_t block_size_raw; /* block size value direct from header */
    uint8_t sample_rate_raw; /* sample rate value direct from header */
    uint8_t channel_assignment_raw; /* channel assignment value direct from header */
    uint8_t  blocking_strategy;
    uint16_t block_size; /* calculated/parsed block size */
    uint32_t sample_rate; /* calculated/parsed sample rate */
    enum MINIFLAC_CHASSGN channel_assignment;
    uint8_t  channels;
    uint8_t  bps;
    union {
        uint64_t sample_number;
        uint32_t frame_number;
    };
    uint8_t crc8;
    size_t size; /* size of the frame header, in bytes, only valid after sync */
};

struct miniflac_frame_s {
    enum MINIFLAC_FRAME_STATE state;
    uint8_t cur_subframe;
    uint16_t crc16;
    size_t size; /* size of the frame, in bytes, only valid after decode */
    struct miniflac_frame_header_s header;
    struct miniflac_subframe_s subframe;
};

struct miniflac_s {
    enum MINIFLAC_STATE state;
    enum MINIFLAC_CONTAINER container;
    struct miniflac_bitreader_s br;
    struct miniflac_ogg_s ogg;
    struct miniflac_oggheader_s oggheader;
    struct miniflac_streammarker_s streammarker;
    struct miniflac_metadata_s metadata;
    struct miniflac_frame_s frame;
    int32_t oggserial;
    uint8_t oggserial_set;
    uint64_t bytes_read_flac; /* total bytes of flac data read */
    uint64_t bytes_read_ogg; /* total bytes of ogg data read */
};

struct mflac_s {
    struct miniflac_s flac;
    mflac_readcb read;
    void* userdata;
    size_t bufpos;
    size_t buflen;
#ifndef MFLAC_BUFFER_SIZE
#define MFLAC_BUFFER_SIZE 16384
#endif
    uint8_t buffer[MFLAC_BUFFER_SIZE];
};


typedef struct miniflac_bitreader_s miniflac_bitreader_t;
typedef struct miniflac_oggheader_s miniflac_oggheader_t;
typedef struct miniflac_ogg_s miniflac_ogg_t;
typedef struct miniflac_streammarker_s miniflac_streammarker_t;
typedef struct miniflac_metadata_header_s miniflac_metadata_header_t;
typedef struct miniflac_streaminfo_s miniflac_streaminfo_t;
typedef struct miniflac_vorbis_comment_s miniflac_vorbis_comment_t;
typedef struct miniflac_picture_s miniflac_picture_t;
typedef struct miniflac_cuesheet_s miniflac_cuesheet_t;
typedef struct miniflac_seektable_s miniflac_seektable_t;
typedef struct miniflac_application_s miniflac_application_t;
typedef struct miniflac_padding_s miniflac_padding_t;
typedef struct miniflac_metadata_s miniflac_metadata_t;
typedef struct miniflac_residual_s miniflac_residual_t;
typedef struct miniflac_subframe_fixed_s miniflac_subframe_fixed_t;
typedef struct miniflac_subframe_lpc_s miniflac_subframe_lpc_t;
typedef struct miniflac_subframe_constant_s miniflac_subframe_constant_t;
typedef struct miniflac_subframe_verbatim_s miniflac_subframe_verbatim_t;
typedef struct miniflac_subframe_header_s miniflac_subframe_header_t;
typedef struct miniflac_subframe_s miniflac_subframe_t;
typedef struct miniflac_frame_header_s miniflac_frame_header_t;
typedef struct miniflac_frame_s miniflac_frame_t;
typedef struct miniflac_s miniflac_t;
typedef struct mflac_s mflac_t;

typedef enum MINIFLAC_RESULT MINIFLAC_RESULT;
typedef enum MINIFLAC_OGGHEADER_STATE MINIFLAC_OGGHEADER_STATE;
typedef enum MINIFLAC_OGG_STATE MINIFLAC_OGG_STATE;
typedef enum MINIFLAC_STREAMMARKER_STATE MINIFLAC_STREAMMARKER_STATE;
typedef enum MINIFLAC_METADATA_TYPE MINIFLAC_METADATA_TYPE;
typedef enum MINIFLAC_METADATA_HEADER_STATE MINIFLAC_METADATA_HEADER_STATE;
typedef enum MINIFLAC_STREAMINFO_STATE MINIFLAC_STREAMINFO_STATE;
typedef enum MINIFLAC_VORBISCOMMENT_STATE MINIFLAC_VORBISCOMMENT_STATE;
typedef enum MINIFLAC_PICTURE_STATE MINIFLAC_PICTURE_STATE;
typedef enum MINIFLAC_CUESHEET_STATE MINIFLAC_CUESHEET_STATE;
typedef enum MINIFLAC_SEEKTABLE_STATE MINIFLAC_SEEKTABLE_STATE;
typedef enum MINIFLAC_APPLICATION_STATE MINIFLAC_APPLICATION_STATE;
typedef enum MINIFLAC_METADATA_STATE MINIFLAC_METADATA_STATE;
typedef enum MINIFLAC_RESIDUAL_STATE MINIFLAC_RESIDUAL_STATE;
typedef enum MINIFLAC_SUBFRAME_FIXED_STATE MINIFLAC_SUBFRAME_FIXED_STATE;
typedef enum MINIFLAC_SUBFRAME_LPC_STATE MINIFLAC_SUBFRAME_LPC_STATE;
typedef enum MINIFLAC_SUBFRAME_CONSTANT_STATE MINIFLAC_SUBFRAME_CONSTANT_STATE;
typedef enum MINIFLAC_SUBFRAME_VERBATIM_STATE MINIFLAC_SUBFRAME_VERBATIM_STATE;
typedef enum MINIFLAC_SUBFRAME_TYPE MINIFLAC_SUBFRAME_TYPE;
typedef enum MINIFLAC_SUBFRAME_HEADER_STATE MINIFLAC_SUBFRAME_HEADER_STATE;
typedef enum MINIFLAC_SUBFRAME_STATE MINIFLAC_SUBFRAME_STATE;
typedef enum MINIFLAC_CHASSGN MINIFLAC_CHASSGN;
typedef enum MINIFLAC_FRAME_HEADER_STATE MINIFLAC_FRAME_HEADER_STATE;
typedef enum MINIFLAC_FRAME_STATE MINIFLAC_FRAME_STATE;
typedef enum MINIFLAC_STATE MINIFLAC_STATE;
typedef enum MINIFLAC_CONTAINER MINIFLAC_CONTAINER;
typedef enum MFLAC_RESULT MFLAC_RESULT;

#ifdef __cplusplus
extern "C" {
#endif

/* returns the number of bytes needed for the miniflac struct (for malloc, etc) */
MINIFLAC_API
MINIFLAC_CONST
size_t
miniflac_size(void);

/* give the container type if you know the kind of container,
 * otherwise 0 for unknown */
MINIFLAC_API
void
miniflac_init(miniflac_t* pFlac, MINIFLAC_CONTAINER container);

/* performs a reset to a particular state. Resetting to anything
 * besides MINIFLAC_FRAME is equivalent to performing miniflac_init (except
 * the container and ogg-related settings are kept).
 * Resetting to MINIFLAC_FRAME will keep decoded streaminfo data, this function
 * is meant to prepare for decoding frames after doing a seek */
MINIFLAC_API
void
miniflac_reset(miniflac_t* pFlac, MINIFLAC_STATE state);

/* sync to the next metadata block or frame, parses the metadata header or frame header */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_sync(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length);

/* decode a frame of audio, automatically skips metadata if needed */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_decode(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, int32_t** samples);

/* functions to query the state without inspecting structs,
 * only valid to call after miniflac_sync returns MINIFLAC_OK */
MINIFLAC_API
uint8_t
miniflac_is_native(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_is_ogg(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_is_metadata(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_is_frame(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_last(miniflac_t* pFlac);

MINIFLAC_API
MINIFLAC_METADATA_TYPE
miniflac_metadata_type(miniflac_t* pFlac);

MINIFLAC_API
uint32_t
miniflac_metadata_length(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_streaminfo(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_padding(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_application(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_seektable(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_vorbis_comment(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_cuesheet(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_metadata_is_picture(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_frame_blocking_strategy(miniflac_t* pFlac);

MINIFLAC_API
uint16_t
miniflac_frame_block_size(miniflac_t* pFlac);

MINIFLAC_API
uint32_t
miniflac_frame_sample_rate(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_frame_channels(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_frame_bps(miniflac_t* pFlac);

MINIFLAC_API
uint64_t
miniflac_frame_sample_number(miniflac_t* pFlac);

MINIFLAC_API
uint32_t
miniflac_frame_frame_number(miniflac_t* pFlac);

MINIFLAC_API
uint32_t
miniflac_frame_header_size(miniflac_t* pFlac);

MINIFLAC_API
uint64_t
miniflac_bytes_read_flac(miniflac_t* pFlac);

MINIFLAC_API
uint64_t
miniflac_bytes_read_ogg(miniflac_t* pFlac);

MINIFLAC_API
int32_t
miniflac_ogg_serial(miniflac_t* pFlac);

/* get the minimum block size from a streaminfo block */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_min_block_size(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint16_t* min_block_size);

/* get the maximum block size */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_max_block_size(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint16_t* max_block_size);

/* get the minimum frame size */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_min_frame_size(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* min_frame_size);

/* get the maximum frame size */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_max_frame_size(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* max_frame_size);

/* get the sample rate */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_sample_rate(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* sample_rate);

/* get the channel count */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_channels(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* channels);

/* get the bits per sample */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_bps(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* bps);

/* get the total samples */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_total_samples(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint64_t* total_samples);

/* get the md5 length (always 16) */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_md5_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* md5_length);

/* get the md5 string */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_streaminfo_md5_data(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* get the length of the vendor string, automatically skips metadata blocks, throws an error on audio frames */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_vorbis_comment_vendor_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* vendor_length);

/* get the vendor string, automatically skips metadata blocks, throws an error on audio frames */
/* will NOT be NULL-terminated! */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_vorbis_comment_vendor_string(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* get the total number of comments, automatically skips metadata blocks, throws an error on audio frames */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_vorbis_comment_total(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* total_comments);

/* get the next comment length, automatically skips metadata blocks, throws an error on audio frames */
/* returns MINIFLAC_METADATA_END when out of comments */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_vorbis_comment_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* comment_length);

/* get the next comment string, automatically skips metadata blocks, throws an error on audio frames */
/* will NOT be NULL-terminated! */
/* returns MINIFLAC_METADATA_END when out of comments */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_vorbis_comment_string(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* read a picture type */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_type(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_type);

/* read a picture mime string length */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_mime_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_mime_length);

/* read a picture mime string */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_mime_string(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* read a picture description string length */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_description_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_description_length);

/* read a picture description string */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_description_string(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* read a picture width */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_width(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_width);

/* read a picture height */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_height(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_height);

/* read a picture colordepth */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_colordepth(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_colordepth);

/* read a picture totalcolors */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_totalcolors(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_totalcolors);

/* read a picture data length */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* picture_length);

/* read a picture data */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_picture_data(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* read a cuesheet catalog length (128 bytes) */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_catalog_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* catalog_length);

/* read a cuesheet catalog number */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_catalog_string(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, char* buffer, uint32_t buffer_length, uint32_t* outlen);

/* read a cuesheet leadin value */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_leadin(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint64_t* leadin);

/* read a cuesheet "is this a cd" flag */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_cd_flag(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* cd_flag);

/* read a cuesheet total tracks */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_tracks(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* tracks);

/* read the next track offset (can return MINIFLAC_METADATA_END) */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_offset(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint64_t* track_offset);

/* read the next track number */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_number(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* track_number);

/* read the next track isrc length */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_isrc_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* isrc_length);

/* read the next track isrc string */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_isrc_string(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, char* buffer, uint32_t buffer_length, uint32_t* outlen);

/* read the next track type flag (0 = audio, 1 = non-audio) */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_audio_flag(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* track_audio_flag);

/* read the track pre-emphasis flag */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_preemph_flag(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* track_preemph_flag);

/* read the total number of track index points */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_track_indexpoints(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* track_indexpoints);

/* read the next index point offset */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_index_point_offset(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint64_t* index_point_offset);

/* read the next index point number */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_cuesheet_index_point_number(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* index_point_number);

/* get the number of seekpoints */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_seektable_seekpoints(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* seekpoints);

/* read the next seekpoint sample number */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_seektable_sample_number(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint64_t* sample_number);

/* read the next seekpoint sample offset */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_seektable_sample_offset(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint64_t* sample_offset);

/* read the next seekpoint # of samples in seekpoint */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_seektable_samples(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint16_t* samples);

/* read an application block's ID */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_application_id(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* id);

/* read an application block's data length */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_application_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* application_length);

/* read an application block's data */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_application_data(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* buffer, uint32_t buffer_length, uint32_t* outlen);

/* read a padding block's data length */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_padding_length(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint32_t* padding_length);

/* read a padding block's data */
MINIFLAC_API
MINIFLAC_RESULT
miniflac_padding_data(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, uint8_t* buffer, uint32_t buffer_length, uint32_t* outlen);

MINIFLAC_API
unsigned int
miniflac_version_major(void);

MINIFLAC_API
unsigned int
miniflac_version_minor(void);

MINIFLAC_API
unsigned int
miniflac_version_patch(void);

MINIFLAC_API
const char*
miniflac_version_string(void);

MINIFLAC_API
MINIFLAC_CONST
size_t
mflac_size(void);

MINIFLAC_API
void
mflac_init(mflac_t* m, MINIFLAC_CONTAINER container, mflac_readcb read, void* userdata);

MINIFLAC_API
void
mflac_reset(mflac_t* m, MINIFLAC_STATE state);

MINIFLAC_API
MFLAC_RESULT
mflac_sync(mflac_t* m);

MINIFLAC_API
MFLAC_RESULT
mflac_decode(mflac_t* m, int32_t** samples);

/* functions to query the state without inspecting structs,
 * only valid to call after mflac_sync returns MFLAC_OK */
MINIFLAC_API
uint8_t
mflac_is_native(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_is_ogg(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_is_frame(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_is_metadata(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_last(mflac_t* m);

MINIFLAC_API
MINIFLAC_METADATA_TYPE
miniflac_metadata_type(miniflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_streaminfo(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_padding(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_application(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_seektable(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_vorbis_comment(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_cuesheet(mflac_t* m);

MINIFLAC_API
uint8_t
mflac_metadata_is_picture(mflac_t* m);

MINIFLAC_API
int32_t
mflac_ogg_serial(mflac_t* m);

MINIFLAC_API
uint32_t
mflac_frame_header_size(mflac_t* m);

MINIFLAC_API
uint64_t
mflac_bytes_read_flac(mflac_t* m);

MINIFLAC_API
uint64_t
mflac_bytes_read_ogg(mflac_t* m);

/*
 * METADATA FUNCTIONS
 * ==================
 *
 * The below metadata-related functions are grouped based on metadata blocks,
 * for conveience I've listed the miniflac enum label and value for each type */
/*
 * MINIFLAC_METADATA_STREAMINFO (0)
 * ================================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_streaminfo_min_block_size, but - if you do, you have to call it before
 * mflac_streaminfo_max_block_size.
 */
MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_min_block_size(mflac_t* m, uint16_t* min_block_size);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_max_block_size(mflac_t* m, uint16_t* max_block_size);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_min_frame_size(mflac_t* m, uint32_t* min_frame_size);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_max_frame_size(mflac_t* m, uint32_t* max_frame_size);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_sample_rate(mflac_t* m, uint32_t* sample_rate);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_channels(mflac_t* m, uint8_t* channels);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_bps(mflac_t* m, uint8_t* bps);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_total_samples(mflac_t* m, uint64_t* total_samples);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_md5_length(mflac_t* m, uint32_t* md5_length);

MINIFLAC_API
MFLAC_RESULT
mflac_streaminfo_md5_data(mflac_t* m, uint8_t* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/*
 * MINIFLAC_METADATA_PADDING (1)
 * =============================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_padding_length, but - if you do, you have to call it before
 * mflac_padding_data.
 */
/* gets the length of the PADDING block */
MINIFLAC_API
MFLAC_RESULT
mflac_padding_length(mflac_t* m, uint32_t* length);

/* gets the data of the PADDING block */
MINIFLAC_API
MFLAC_RESULT
mflac_padding_data(mflac_t* m, uint8_t*buffer, uint32_t buffer_length, uint32_t* buffer_used);

/*
 * MINIFLAC_METADATA_APPLICATION (2)
 * =================================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_application_id, but - if you do, you have to call it before
 * mflac_application_length and mflac_application_data.
 */
/* gets the id of the APPLICATION block */
MINIFLAC_API
MFLAC_RESULT
mflac_application_id(mflac_t* m, uint32_t* id);

/* gets the length of the APPLICATION block */
MINIFLAC_API
MFLAC_RESULT
mflac_application_length(mflac_t* m, uint32_t* length);

/* gets the data of the APPLICATION block */
MINIFLAC_API
MFLAC_RESULT
mflac_application_data(mflac_t* m, uint8_t*buffer, uint32_t buffer_length, uint32_t* buffer_used);

/*
 * MINIFLAC_METADATA_SEEKTABLE (3)
 * ===============================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_seektable_seekpoints, but - if you do, you have to call it before
 * mflac_seektable_sample_number.
 */
MINIFLAC_API
MFLAC_RESULT
mflac_seektable_seekpoints(mflac_t* m, uint32_t* seekpoints);

MINIFLAC_API
MFLAC_RESULT
mflac_seektable_sample_number(mflac_t* m, uint64_t* sample_number);

MINIFLAC_API
MFLAC_RESULT
mflac_seektable_sample_offset(mflac_t* m, uint64_t* sample_offset);

MINIFLAC_API
MFLAC_RESULT
mflac_seektable_samples(mflac_t* m, uint16_t* samples);

/*
 * MINIFLAC_METADATA_VORBIS_COMMENT (4)
 * ===================================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_vorbis_comment_vendor_length, but - if you do, you have to call it before
 * mflac_vorbis_comment_vendor_string
 */
/* gets the length of the vendor string - excludes the null terminator */
MINIFLAC_API
MFLAC_RESULT
mflac_vorbis_comment_vendor_length(mflac_t* m, uint32_t* length);

/* gets the vendor string - will not be terminated */
MINIFLAC_API
MFLAC_RESULT
mflac_vorbis_comment_vendor_string(mflac_t* m, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/* gets the total number of comments */
MINIFLAC_API
MFLAC_RESULT
mflac_vorbis_comment_total(mflac_t* m, uint32_t* total);

/* gets the length number of the next comment - does not include null terminator */
MINIFLAC_API
MFLAC_RESULT
mflac_vorbis_comment_length(mflac_t* m, uint32_t* length);

/* gets the next comment  - will not be null-terminated! */
MINIFLAC_API
MFLAC_RESULT
mflac_vorbis_comment_string(mflac_t* m, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

/*
 * MINIFLAC_METADATA_CUESHEET (5)
 * ==============================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_cuesheet_catalog_length, but - if you do, you have to call it before
 * mflac_cuesheet_catalog_string.
 */
MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_catalog_length(mflac_t* m, uint32_t* length);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_catalog_string(mflac_t* m, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_leadin(mflac_t* m, uint64_t* leadin);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_cd_flag(mflac_t* m, uint8_t* cd_flag);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_tracks(mflac_t* m, uint8_t* tracks);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_offset(mflac_t* m, uint64_t* track_offset);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_number(mflac_t* m, uint8_t* track_number);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_isrc_length(mflac_t* m, uint32_t* length);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_isrc_string(mflac_t* m, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_audio_flag(mflac_t* m, uint8_t* track_audio_flag);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_preemph_flag(mflac_t* m, uint8_t* track_preemph_flag);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_track_indexpoints(mflac_t* m, uint8_t* track_indexpoints);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_index_point_offset(mflac_t* m, uint64_t* index_point_offset);

MINIFLAC_API
MFLAC_RESULT
mflac_cuesheet_index_point_number(mflac_t* m, uint8_t* index_point_number);

/*
 * MINIFLAC_METADATA_PICTURE (6)
 * ==============================
 *
 * Functions are listed in the order they should be called, but you can skip
 * ones you don't need. For example, you don't have to call
 * mflac_picture_type, but - if you do, you have to call it before
 * mflac_pictue_mime_length.
 */
MINIFLAC_API
MFLAC_RESULT
mflac_picture_type(mflac_t* m, uint32_t* picture_type);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_mime_length(mflac_t* m, uint32_t* picture_mime_length);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_mime_string(mflac_t* m, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_description_length(mflac_t* m, uint32_t* picture_description_length);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_description_string(mflac_t* m, char* buffer, uint32_t buffer_length, uint32_t* buffer_used);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_width(mflac_t* m, uint32_t* picture_width);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_height(mflac_t* m, uint32_t* picture_height);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_colordepth(mflac_t* m, uint32_t* picture_colordepth);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_totalcolors(mflac_t* m, uint32_t* picture_totalcolors);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_length(mflac_t* m, uint32_t* picture_length);

MINIFLAC_API
MFLAC_RESULT
mflac_picture_data(mflac_t* m, uint8_t* buffer, uint32_t buffer_length, uint32_t* buffer_used);

MINIFLAC_API
unsigned int
mflac_version_major(void);

MINIFLAC_API
unsigned int
mflac_version_minor(void);

MINIFLAC_API
unsigned int
mflac_version_patch(void);

MINIFLAC_API
const char*
mflac_version_string(void);


#ifdef __cplusplus
}
#endif

#endif

#ifdef MINIFLAC_IMPLEMENTATION

#ifdef MINIFLAC_ABORT_ON_ERROR
#include <stdlib.h>
#define miniflac_abort() abort()
#else
#define miniflac_abort()
#endif

MINIFLAC_PRIVATE
uint32_t
miniflac_unpack_uint32le(uint8_t buffer[4]);

MINIFLAC_PRIVATE
int32_t
miniflac_unpack_int32le(uint8_t buffer[4]);

MINIFLAC_PRIVATE
uint64_t
miniflac_unpack_uint64le(uint8_t buffer[8]);

MINIFLAC_PRIVATE
int64_t
miniflac_unpack_int64le(uint8_t buffer[8]);

MINIFLAC_PRIVATE
void
miniflac_bitreader_init(miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
int
miniflac_bitreader_fill(miniflac_bitreader_t* br, uint8_t bits);

MINIFLAC_PRIVATE
int
miniflac_bitreader_fill_nocrc(miniflac_bitreader_t* br, uint8_t bits);

MINIFLAC_PRIVATE
uint64_t
miniflac_bitreader_read(miniflac_bitreader_t* br, uint8_t bits);

MINIFLAC_PRIVATE
int64_t
miniflac_bitreader_read_signed(miniflac_bitreader_t* br, uint8_t bits);

MINIFLAC_PRIVATE
uint64_t
miniflac_bitreader_peek(miniflac_bitreader_t* br, uint8_t bits);

MINIFLAC_PRIVATE
void
miniflac_bitreader_discard(miniflac_bitreader_t* br, uint8_t bits);

MINIFLAC_PRIVATE
void
miniflac_bitreader_align(miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_bitreader_reset_crc(miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_oggheader_init(miniflac_oggheader_t* oggheader);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_oggheader_decode(miniflac_oggheader_t* oggheader, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_ogg_init(miniflac_ogg_t* ogg);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_ogg_sync(miniflac_ogg_t* ogg, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_streammarker_init(miniflac_streammarker_t* streammarker);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streammarker_decode(miniflac_streammarker_t* streammarker, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_metadata_header_init(miniflac_metadata_header_t* header);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_metadata_header_decode(miniflac_metadata_header_t* header, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_streaminfo_init(miniflac_streaminfo_t* streaminfo);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_min_block_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint16_t* min_block_size);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_max_block_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint16_t* max_block_size);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_min_frame_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* min_frame_size);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_max_frame_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* max_frame_size);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_sample_rate(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* sample_rate);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_channels(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint8_t* channels);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_bps(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint8_t* bps);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_total_samples(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint64_t* total_samples);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_md5_length(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* md5_length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_md5_data(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
void
miniflac_vorbis_comment_init(miniflac_vorbis_comment_t* vorbis_comment);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_vendor_length(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_vendor_string(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_total(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, uint32_t* total);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_length(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_string(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
void
miniflac_picture_init(miniflac_picture_t* picture);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_type(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* type);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_mime_length(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_mime_string(miniflac_picture_t* picture, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_description_length(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_description_string(miniflac_picture_t* picture, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_width(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_height(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_colordepth(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_totalcolors(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_length(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_data(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
void
miniflac_cuesheet_init(miniflac_cuesheet_t* cuesheet);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_catalog_length(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint32_t* catalog_length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_catalog_string(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_leadin(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint64_t* leadin);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_cd_flag(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* cd_flag);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_tracks(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* tracks);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_offset(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint64_t* track_offset);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_number(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_number);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_isrc_length(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint32_t* track_isrc_length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_isrc_string(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_audio_flag(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_audio_flag);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_preemph_flag(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_preemph_flag);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_indexpoints(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_indexpoints);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_index_point_offset(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint64_t* index_point_offset);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_index_point_number(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* index_point_number);

MINIFLAC_PRIVATE
void
miniflac_seektable_init(miniflac_seektable_t* seektable);

/* read the number of seekpoints */
MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_seekpoints(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint32_t* seekpoints);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_sample_number(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint64_t* sample_number);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_sample_offset(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint64_t* sample_offset);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_samples(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint16_t* samples);

MINIFLAC_PRIVATE
void
miniflac_application_init(miniflac_application_t* application);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_application_read_id(miniflac_application_t* application, miniflac_bitreader_t* br, uint32_t* id);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_application_read_length(miniflac_application_t* application, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_application_read_data(miniflac_application_t* application, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
void
miniflac_padding_init(miniflac_padding_t* padding);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_padding_read_length(miniflac_padding_t* padding, miniflac_bitreader_t* br, uint32_t* length);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_padding_read_data(miniflac_padding_t* padding, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen);

MINIFLAC_PRIVATE
void
miniflac_metadata_init(miniflac_metadata_t* metadata);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_metadata_sync(miniflac_metadata_t* metadata, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_metadata_decode(miniflac_metadata_t* metadata, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_residual_init(miniflac_residual_t* residual);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_residual_decode(miniflac_residual_t* residual, miniflac_bitreader_t* br, uint32_t* pos, uint32_t block_size, uint8_t predictor_order, int32_t *out);

MINIFLAC_PRIVATE
void
miniflac_subframe_fixed_init(miniflac_subframe_fixed_t* c);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_fixed_decode(miniflac_subframe_fixed_t* c, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps, miniflac_residual_t* residual, uint8_t predictor_order);

MINIFLAC_PRIVATE
void
miniflac_subframe_lpc_init(miniflac_subframe_lpc_t* l);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_lpc_decode(miniflac_subframe_lpc_t* l, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps, miniflac_residual_t* residual, uint8_t predictor_order);

MINIFLAC_PRIVATE
void miniflac_subframe_constant_init(miniflac_subframe_constant_t* c);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_constant_decode(miniflac_subframe_constant_t* c, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps);

MINIFLAC_PRIVATE
void
miniflac_subframe_verbatim_init(miniflac_subframe_verbatim_t* c);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_verbatim_decode(miniflac_subframe_verbatim_t* c, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps);

MINIFLAC_PRIVATE
void
miniflac_subframe_header_init(miniflac_subframe_header_t* subframeheader);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_header_decode(miniflac_subframe_header_t* subframeheader, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void
miniflac_subframe_init(miniflac_subframe_t* subframe);

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_decode(miniflac_subframe_t* subframe, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps);

MINIFLAC_PRIVATE
void
miniflac_frame_header_init(miniflac_frame_header_t* frame_header);

MINIFLAC_PRIVATE
MINIFLAC_RESULT miniflac_frame_header_decode(miniflac_frame_header_t* frame_header, miniflac_bitreader_t* br);

MINIFLAC_PRIVATE
void miniflac_frame_init(miniflac_frame_t* frame);

/* ensures we've just read the audio frame header and are ready to decode */
MINIFLAC_PRIVATE
MINIFLAC_RESULT miniflac_frame_sync(miniflac_frame_t* frame, miniflac_bitreader_t* br, miniflac_streaminfo_t* info);

MINIFLAC_PRIVATE
MINIFLAC_RESULT miniflac_frame_decode(miniflac_frame_t* frame, miniflac_bitreader_t* br, miniflac_streaminfo_t* info, int32_t** output);



#define MFLAC_PASTE(a,b) a ## b

#define MFLAC_FUNC_BODY(a) \
    while( (res = a) == MINIFLAC_CONTINUE ) { \
        received = m->read(m->buffer, MFLAC_BUFFER_SIZE, m->userdata); \
        if(received == 0) return MFLAC_EOF; \
        m->buflen = received; \
        m->bufpos = 0; \
    } \
    if(res < MINIFLAC_OK) { \
        return (MFLAC_RESULT)res; \
    } \
    m->bufpos += used; \
    m->buflen -= used;


#define MFLAC_GET0_BODY(var) MFLAC_FUNC_BODY(miniflac_ ## var (&m->flac, &m->buffer[m->bufpos], m->buflen, &used) )
#define MFLAC_GET1_BODY(var, a) MFLAC_FUNC_BODY(miniflac_ ## var(&m->flac, &m->buffer[m->bufpos], m->buflen, &used, a) )
#define MFLAC_GET3_BODY(var, a, b, c) MFLAC_FUNC_BODY(miniflac_ ## var(&m->flac, &m->buffer[m->bufpos], m->buflen, &used, a, b, c) )

#define MFLAC_FUNC(sig,body) \
MINIFLAC_API \
MFLAC_RESULT \
sig { \
    MINIFLAC_RESULT res = MINIFLAC_OK; \
    uint32_t used = 0; \
    size_t received = 0; \
    body \
    return (MFLAC_RESULT)res; \
}

#define MFLAC_GET0_FUNC(var) MFLAC_FUNC(MFLAC_PASTE(mflac_,var)(mflac_t* m),MFLAC_GET0_BODY(var))
#define MFLAC_GET1_FUNC(var, typ) MFLAC_FUNC(MFLAC_PASTE(mflac_,var)(mflac_t* m, typ p1),MFLAC_GET1_BODY(var, p1))
#define MFLAC_GET3_FUNC(var, typ) MFLAC_FUNC(MFLAC_PASTE(mflac_,var)(mflac_t* m, typ p1, uint32_t p2, uint32_t* p3),MFLAC_GET3_BODY(var, p1, p2, p3))

MINIFLAC_API
MINIFLAC_CONST
size_t
mflac_size(void) {
    return sizeof(mflac_t);
}

MINIFLAC_API
void
mflac_init(mflac_t* m, MINIFLAC_CONTAINER container, mflac_readcb read, void *userdata) {
    miniflac_init(&m->flac, container);
    m->read = read;
    m->userdata = userdata;
    m->bufpos = 0;
    m->buflen = 0;
}

MINIFLAC_API
void
mflac_reset(mflac_t* m, MINIFLAC_STATE state) {
    miniflac_reset(&m->flac, state);
    m->bufpos = 0;
    m->buflen = 0;
}

MFLAC_GET0_FUNC(sync)

MFLAC_GET1_FUNC(decode,int32_t**)

MFLAC_GET1_FUNC(streaminfo_min_block_size, uint16_t*)
MFLAC_GET1_FUNC(streaminfo_max_block_size, uint16_t*)
MFLAC_GET1_FUNC(streaminfo_min_frame_size, uint32_t*)
MFLAC_GET1_FUNC(streaminfo_max_frame_size, uint32_t*)
MFLAC_GET1_FUNC(streaminfo_sample_rate, uint32_t*)
MFLAC_GET1_FUNC(streaminfo_channels, uint8_t*)
MFLAC_GET1_FUNC(streaminfo_bps, uint8_t*)
MFLAC_GET1_FUNC(streaminfo_total_samples, uint64_t*)
MFLAC_GET1_FUNC(streaminfo_md5_length, uint32_t*)
MFLAC_GET3_FUNC(streaminfo_md5_data, uint8_t*)

MFLAC_GET1_FUNC(vorbis_comment_vendor_length, uint32_t*)
MFLAC_GET3_FUNC(vorbis_comment_vendor_string, char*)
MFLAC_GET1_FUNC(vorbis_comment_total, uint32_t*)
MFLAC_GET1_FUNC(vorbis_comment_length, uint32_t*)
MFLAC_GET3_FUNC(vorbis_comment_string, char*)

MFLAC_GET1_FUNC(padding_length, uint32_t*)
MFLAC_GET3_FUNC(padding_data, uint8_t*)

MFLAC_GET1_FUNC(application_id, uint32_t*)
MFLAC_GET1_FUNC(application_length, uint32_t*)
MFLAC_GET3_FUNC(application_data, uint8_t*)

MFLAC_GET1_FUNC(seektable_seekpoints, uint32_t*)
MFLAC_GET1_FUNC(seektable_sample_number, uint64_t*)
MFLAC_GET1_FUNC(seektable_sample_offset, uint64_t*)
MFLAC_GET1_FUNC(seektable_samples, uint16_t*)

MFLAC_GET1_FUNC(cuesheet_catalog_length, uint32_t*)
MFLAC_GET3_FUNC(cuesheet_catalog_string, char*)
MFLAC_GET1_FUNC(cuesheet_leadin, uint64_t*)
MFLAC_GET1_FUNC(cuesheet_cd_flag, uint8_t*)
MFLAC_GET1_FUNC(cuesheet_tracks, uint8_t*)
MFLAC_GET1_FUNC(cuesheet_track_offset, uint64_t*)
MFLAC_GET1_FUNC(cuesheet_track_number, uint8_t*)
MFLAC_GET1_FUNC(cuesheet_track_isrc_length, uint32_t*)
MFLAC_GET3_FUNC(cuesheet_track_isrc_string, char*)
MFLAC_GET1_FUNC(cuesheet_track_audio_flag, uint8_t*)
MFLAC_GET1_FUNC(cuesheet_track_preemph_flag, uint8_t*)
MFLAC_GET1_FUNC(cuesheet_track_indexpoints, uint8_t*)
MFLAC_GET1_FUNC(cuesheet_index_point_offset, uint64_t*)
MFLAC_GET1_FUNC(cuesheet_index_point_number, uint8_t*)

MFLAC_GET1_FUNC(picture_type, uint32_t*)
MFLAC_GET1_FUNC(picture_mime_length, uint32_t*)
MFLAC_GET3_FUNC(picture_mime_string, char*)
MFLAC_GET1_FUNC(picture_description_length, uint32_t*)
MFLAC_GET3_FUNC(picture_description_string, char*)
MFLAC_GET1_FUNC(picture_width, uint32_t*)
MFLAC_GET1_FUNC(picture_height, uint32_t*)
MFLAC_GET1_FUNC(picture_colordepth, uint32_t*)
MFLAC_GET1_FUNC(picture_totalcolors, uint32_t*)
MFLAC_GET1_FUNC(picture_length, uint32_t*)
MFLAC_GET3_FUNC(picture_data, uint8_t*)

MINIFLAC_API
uint8_t
mflac_is_native(mflac_t* m) {
    return m->flac.container == MINIFLAC_CONTAINER_NATIVE;
}

MINIFLAC_API
uint8_t
mflac_is_ogg(mflac_t* m) {
    return m->flac.container == MINIFLAC_CONTAINER_OGG;
}

MINIFLAC_API
uint8_t
mflac_is_frame(mflac_t* m) {
    return m->flac.state == MINIFLAC_FRAME;
}

MINIFLAC_API
uint8_t
mflac_is_metadata(mflac_t* m) {
    return m->flac.state == MINIFLAC_METADATA;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_last(mflac_t* m) {
    return m->flac.metadata.header.is_last;
}

MINIFLAC_API
MINIFLAC_METADATA_TYPE
mflac_metadata_type(mflac_t* m) {
    return m->flac.metadata.header.type;
}

MINIFLAC_API
uint32_t
mflac_metadata_length(mflac_t* m) {
    return m->flac.metadata.header.length;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_streaminfo(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_STREAMINFO;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_padding(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_PADDING;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_application(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_APPLICATION;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_seektable(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_SEEKTABLE;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_vorbis_comment(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_VORBIS_COMMENT;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_cuesheet(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_CUESHEET;
}

MINIFLAC_API
uint8_t
mflac_metadata_is_picture(mflac_t* m) {
    return m->flac.metadata.header.type == MINIFLAC_METADATA_PICTURE;
}

MINIFLAC_API
uint8_t
mflac_frame_blocking_strategy(mflac_t* m) {
    return m->flac.frame.header.blocking_strategy;
}

MINIFLAC_API
uint16_t
mflac_frame_block_size(mflac_t* m) {
    return m->flac.frame.header.block_size;
}

MINIFLAC_API
uint32_t
mflac_frame_sample_rate(mflac_t* m) {
    return m->flac.frame.header.sample_rate;
}

MINIFLAC_API
uint8_t
mflac_frame_channels(mflac_t* m) {
    return m->flac.frame.header.channels;
}

MINIFLAC_API
uint8_t
mflac_frame_bps(mflac_t* m) {
    return m->flac.frame.header.bps;
}

MINIFLAC_API
uint64_t
mflac_frame_sample_number(mflac_t* m) {
    return m->flac.frame.header.sample_number;
}

MINIFLAC_API
uint32_t
mflac_frame_frame_number(mflac_t* m) {
    return m->flac.frame.header.frame_number;
}

MINIFLAC_API
uint32_t
mflac_frame_header_size(mflac_t* m) {
    return m->flac.frame.header.size;
}

MINIFLAC_API
int32_t
mflac_ogg_serial(mflac_t* m) {
    return m->flac.oggserial;
}

MINIFLAC_API
uint64_t
mflac_bytes_read_flac(mflac_t* m) {
    return m->flac.bytes_read_flac;
}

MINIFLAC_API
uint64_t
mflac_bytes_read_ogg(mflac_t* m) {
    return m->flac.bytes_read_ogg;
}

MINIFLAC_API
unsigned int
mflac_version_major(void) {
    return miniflac_version_major();
}

MINIFLAC_API
unsigned int
mflac_version_minor(void) {
    return miniflac_version_minor();
}

MINIFLAC_API
unsigned int
mflac_version_patch(void) {
    return miniflac_version_patch();
}

MINIFLAC_API
const char*
mflac_version_string(void) {
    return miniflac_version_string();
}

#undef MFLAC_PASTE
#undef MFLAC_FUNC_BODY
#undef MFLAC_GET0_BODY
#undef MFLAC_GET1_BODY
#undef MFLAC_GET3_BODY
#undef MFLAC_FUNC
#undef MFLAC_GET0_FUNC
#undef MFLAC_GET1_FUNC
#undef MFLAC_GET3_FUNC

#define MINIFLAC_VERSION_MAJOR 1
#define MINIFLAC_VERSION_MINOR 1
#define MINIFLAC_VERSION_PATCH 3

#define MINIFLAC_STR(x) #x
#define MINIFLAC_XSTR(x) MINIFLAC_STR(x)

#define MINIFLAC_VERSION_STRING MINIFLAC_XSTR(MINIFLAC_VERSION_MAJOR) "." MINIFLAC_XSTR(MINIFLAC_VERSION_MINOR) "." MINIFLAC_XSTR(MINIFLAC_VERSION_PATCH)


MINIFLAC_API
unsigned int
miniflac_version_major(void) {
    return MINIFLAC_VERSION_MAJOR;
}

MINIFLAC_API
unsigned int
miniflac_version_minor(void) {
    return MINIFLAC_VERSION_MINOR;
}

MINIFLAC_API
unsigned int
miniflac_version_patch(void) {
    return MINIFLAC_VERSION_PATCH;
}

MINIFLAC_API
const char*
miniflac_version_string(void) {
    return MINIFLAC_VERSION_STRING;
}

static
void
miniflac_oggreset(miniflac_t* pFlac) {
    miniflac_bitreader_init(&pFlac->br);
    miniflac_oggheader_init(&pFlac->oggheader);
    miniflac_streammarker_init(&pFlac->streammarker);
    miniflac_metadata_init(&pFlac->metadata);
    miniflac_frame_init(&pFlac->frame);
    pFlac->state = MINIFLAC_OGGHEADER;
}

static
MINIFLAC_RESULT
miniflac_oggfunction_start(miniflac_t* pFlac, const uint8_t* data, const uint8_t** packet, uint32_t* packet_length) {
    MINIFLAC_RESULT r;

    while(pFlac->ogg.state != MINIFLAC_OGG_DATA) {
        r = miniflac_ogg_sync(&pFlac->ogg,&pFlac->ogg.br);
        if(r != MINIFLAC_OK) return r;

        if(pFlac->oggserial_set == 0) {
            if(pFlac->ogg.headertype & 0x02) {
                miniflac_oggreset(pFlac);
            }
        } else {
            if(pFlac->oggserial != pFlac->ogg.serialno) pFlac->ogg.state = MINIFLAC_OGG_SKIP;
        }
    }

    *packet = &data[pFlac->ogg.br.pos];
    *packet_length = pFlac->ogg.br.len - pFlac->ogg.br.pos;
    if(*packet_length > (uint32_t)(pFlac->ogg.length - pFlac->ogg.pos)) {
        *packet_length = (uint32_t)pFlac->ogg.length - pFlac->ogg.pos;
    }

    return MINIFLAC_OK;
}

static
void
miniflac_oggfunction_end(miniflac_t* pFlac, uint32_t packet_used) {
    pFlac->ogg.br.pos += packet_used;
    pFlac->ogg.pos    += packet_used;

    if(pFlac->ogg.pos == pFlac->ogg.length) {
        pFlac->ogg.state = MINIFLAC_OGG_CAPTUREPATTERN_O;
        if(pFlac->ogg.headertype & 0x04) {
            if(pFlac->oggserial_set == 1 && pFlac->oggserial == pFlac->ogg.serialno) {
                pFlac->oggserial_set = 0;
                pFlac->oggserial = 0;
            }
        }
    }
}

MINIFLAC_API
MINIFLAC_CONST
size_t
miniflac_size(void) {
    return sizeof(miniflac_t);
}

MINIFLAC_API
void
miniflac_reset(miniflac_t* pFlac, MINIFLAC_STATE state) {
    uint32_t sample_rate = 0;
    uint8_t bps = 0;

    if(state == MINIFLAC_FRAME) {
        sample_rate = pFlac->metadata.streaminfo.sample_rate;
        bps = pFlac->metadata.streaminfo.bps;
    }

    miniflac_bitreader_init(&pFlac->br);
    miniflac_ogg_init(&pFlac->ogg);
    miniflac_oggheader_init(&pFlac->oggheader);
    miniflac_streammarker_init(&pFlac->streammarker);
    miniflac_metadata_init(&pFlac->metadata);
    miniflac_frame_init(&pFlac->frame);
    pFlac->bytes_read_flac = 0;
    pFlac->bytes_read_ogg = 0;
    pFlac->state = state;

    if(state == MINIFLAC_FRAME) {
        pFlac->metadata.streaminfo.sample_rate = sample_rate;
        pFlac->metadata.streaminfo.bps = bps;
    }

    /* if we're using an ogg container we need to look for an ogg header
     * no matter what */
    if(pFlac->container == MINIFLAC_CONTAINER_OGG) {
        pFlac->state = MINIFLAC_OGGHEADER;
    }

}

MINIFLAC_API
void
miniflac_init(miniflac_t* pFlac, MINIFLAC_CONTAINER container) {
    pFlac->container = container;
    pFlac->oggserial = -1;
    pFlac->oggserial_set = 0;

    switch(pFlac->container) {
        case MINIFLAC_CONTAINER_UNKNOWN: {
            miniflac_reset(pFlac, MINIFLAC_STREAMMARKER);
            break;
        }
        case MINIFLAC_CONTAINER_NATIVE: {
            miniflac_reset(pFlac, MINIFLAC_STREAMMARKER_OR_FRAME);
            break;
        }
        case MINIFLAC_CONTAINER_OGG: {
            miniflac_reset(pFlac, MINIFLAC_OGGHEADER);
            break;
        }
    }
}

static
MINIFLAC_RESULT
miniflac_sync_internal(miniflac_t* pFlac, miniflac_bitreader_t* br) {
    MINIFLAC_RESULT r;
    unsigned char c;
    uint16_t peek;

    switch(pFlac->state) {
        case MINIFLAC_OGGHEADER: {
            r = miniflac_oggheader_decode(&pFlac->oggheader,br);
            if (r != MINIFLAC_OK) return r;
            pFlac->oggserial_set = 1;
            pFlac->oggserial = pFlac->ogg.serialno;
            pFlac->state = MINIFLAC_STREAMMARKER;
            goto miniflac_sync_streammarker;
        }
        case MINIFLAC_STREAMMARKER_OR_FRAME: {
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            c = (unsigned char)miniflac_bitreader_peek(br,8);
            if( (char)c == 'f') {
                pFlac->state = MINIFLAC_STREAMMARKER;
                goto miniflac_sync_streammarker;
            } else if(c == 0xFF) {
                pFlac->state = MINIFLAC_FRAME;
                goto miniflac_sync_frame;
            }
            miniflac_abort();
            return MINIFLAC_ERROR;
        }

        case MINIFLAC_STREAMMARKER: {
            miniflac_sync_streammarker:
            r = miniflac_streammarker_decode(&pFlac->streammarker,br);
            if(r != MINIFLAC_OK) return r;
            pFlac->state = MINIFLAC_METADATA_OR_FRAME;
        }

        /* fall-through */
        case MINIFLAC_METADATA_OR_FRAME: {
            miniflac_sync_metadata_or_frame:
            if(miniflac_bitreader_fill(br,16)) return MINIFLAC_CONTINUE;
            peek = (uint16_t)miniflac_bitreader_peek(br,14);
            if(peek == 0x3FFE) {
                pFlac->state = MINIFLAC_FRAME;
                goto miniflac_sync_frame;
            }
            pFlac->state = MINIFLAC_METADATA;
            goto miniflac_sync_metadata;
        }

        /* fall-through */
        case MINIFLAC_METADATA: {
            miniflac_sync_metadata:
            while(pFlac->metadata.state != MINIFLAC_METADATA_HEADER) {
                r = miniflac_metadata_decode(&pFlac->metadata,br);
                if(r != MINIFLAC_OK) return r;
                /* if we're here, it means we were in the middle of
                 * a metadata block and finished decoding, so the
                 * next block could be a metadata block or frame */
                pFlac->state = MINIFLAC_METADATA_OR_FRAME;
                goto miniflac_sync_metadata_or_frame;
            }
            return miniflac_metadata_sync(&pFlac->metadata,br);
        }

        case MINIFLAC_FRAME: {
            miniflac_sync_frame:
            while(pFlac->frame.state != MINIFLAC_FRAME_HEADER) {
                r = miniflac_frame_decode(&pFlac->frame,br,&pFlac->metadata.streaminfo,NULL);
                if(r != MINIFLAC_OK) return r;
            }

            return miniflac_frame_sync(&pFlac->frame,br,&pFlac->metadata.streaminfo);
        }
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

static
MINIFLAC_RESULT
miniflac_sync_native(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length) {
    MINIFLAC_RESULT r;
    pFlac->br.buffer = data;
    pFlac->br.len    = length;
    pFlac->br.pos    = 0;

    r = miniflac_sync_internal(pFlac,&pFlac->br);

    *out_length = pFlac->br.pos;
    pFlac->bytes_read_flac += pFlac->br.pos;
    return r;
}

static
MINIFLAC_RESULT
miniflac_decode_native(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, int32_t** samples) {
    MINIFLAC_RESULT r;
    pFlac->br.buffer = data;
    pFlac->br.len    = length;
    pFlac->br.pos    = 0;

    while(pFlac->state != MINIFLAC_FRAME) {
        r = miniflac_sync_internal(pFlac,&pFlac->br);
        if(r != MINIFLAC_OK) goto miniflac_decode_exit;
    }

    r = miniflac_frame_decode(&pFlac->frame,&pFlac->br,&pFlac->metadata.streaminfo,samples);

    miniflac_decode_exit:
    *out_length = pFlac->br.pos;
    pFlac->bytes_read_flac += pFlac->br.pos;
    return r;
}

static
MINIFLAC_RESULT
miniflac_sync_ogg(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length) {
    MINIFLAC_RESULT r = MINIFLAC_CONTINUE;

    const uint8_t* packet = NULL;
    uint32_t packet_length = 0;
    uint32_t packet_used   = 0;

    pFlac->ogg.br.buffer = data;
    pFlac->ogg.br.len = length;
    pFlac->ogg.br.pos = 0;

    do {
        r = miniflac_oggfunction_start(pFlac,data,&packet,&packet_length);
        if(r != MINIFLAC_OK) break;

        r = miniflac_sync_native(pFlac,packet,packet_length,&packet_used);
        miniflac_oggfunction_end(pFlac,packet_used);

        if(r == MINIFLAC_OGG_HEADER_NOTFLAC) {
            /* try reading more data */
            pFlac->ogg.state = MINIFLAC_OGG_SKIP;
            r = MINIFLAC_CONTINUE;
        }
    } while(r == MINIFLAC_CONTINUE && pFlac->ogg.br.pos < length);

    *out_length = pFlac->ogg.br.pos;
    pFlac->bytes_read_ogg += pFlac->ogg.br.pos;
    return r;
}

static
MINIFLAC_RESULT
miniflac_decode_ogg(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, int32_t** samples) {
    MINIFLAC_RESULT r = MINIFLAC_CONTINUE;

    const uint8_t* packet = NULL;
    uint32_t packet_length = 0;
    uint32_t packet_used   = 0;

    pFlac->ogg.br.buffer = data;
    pFlac->ogg.br.len = length;
    pFlac->ogg.br.pos = 0;

    do {
        r = miniflac_oggfunction_start(pFlac,data,&packet,&packet_length);
        if(r  != MINIFLAC_OK) break;

        r = miniflac_decode_native(pFlac,packet,packet_length,&packet_used,samples);
        miniflac_oggfunction_end(pFlac,packet_used);
    } while(r == MINIFLAC_CONTINUE && pFlac->ogg.br.pos < length);

    *out_length = pFlac->ogg.br.pos;
    pFlac->bytes_read_ogg += pFlac->ogg.br.pos;
    return r;
}

static
MINIFLAC_RESULT
miniflac_probe(miniflac_t* pFlac, const uint8_t* data, uint32_t length) {
    if(length == 0) return MINIFLAC_CONTINUE;
    switch(data[0]) {
        case 'f': {
            pFlac->container = MINIFLAC_CONTAINER_NATIVE;
            pFlac->state = MINIFLAC_STREAMMARKER;
            break;
        }
        case 'O': {
            pFlac->container = MINIFLAC_CONTAINER_OGG;
            pFlac->state = MINIFLAC_OGGHEADER;
            break;
        }
        default: return MINIFLAC_ERROR;
    }
    return MINIFLAC_OK;
}


MINIFLAC_API
MINIFLAC_RESULT
miniflac_decode(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, int32_t** samples) {
    MINIFLAC_RESULT r;

    if(pFlac->container == MINIFLAC_CONTAINER_UNKNOWN) {
        r = miniflac_probe(pFlac,data,length);
        if(r != MINIFLAC_OK) return r;
    }

    if(pFlac->container == MINIFLAC_CONTAINER_NATIVE) {
        r = miniflac_decode_native(pFlac,data,length,out_length,samples);
    } else {
        r = miniflac_decode_ogg(pFlac,data,length,out_length,samples);
    }

    return r;
}

MINIFLAC_API
MINIFLAC_RESULT
miniflac_sync(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length) {
    MINIFLAC_RESULT r;

    if(pFlac->container == MINIFLAC_CONTAINER_UNKNOWN) {
        r = miniflac_probe(pFlac,data,length);
        if(r != MINIFLAC_OK) return r;
    }

    if(pFlac->container == MINIFLAC_CONTAINER_NATIVE) {
        r = miniflac_sync_native(pFlac,data,length,out_length);
    } else {
        r = miniflac_sync_ogg(pFlac,data,length,out_length);
    }

    return r;
}

MINIFLAC_API
uint8_t
miniflac_is_native(miniflac_t* pFlac) {
    return pFlac->container == MINIFLAC_CONTAINER_NATIVE;
}

MINIFLAC_API
uint8_t
miniflac_is_ogg(miniflac_t* pFlac) {
    return pFlac->container == MINIFLAC_CONTAINER_OGG;
}

MINIFLAC_API
uint8_t
miniflac_is_ogg(miniflac_t* pFlac);

MINIFLAC_API
uint8_t
miniflac_is_metadata(miniflac_t* pFlac) {
    return pFlac->state == MINIFLAC_METADATA;
}

MINIFLAC_API
uint8_t
miniflac_is_frame(miniflac_t* pFlac) {
    return pFlac->state == MINIFLAC_FRAME;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_last(miniflac_t* pFlac) {
    return pFlac->metadata.header.is_last;
}

MINIFLAC_API
MINIFLAC_METADATA_TYPE
miniflac_metadata_type(miniflac_t* pFlac) {
    return pFlac->metadata.header.type;
}

MINIFLAC_API
uint32_t
miniflac_metadata_length(miniflac_t* pFlac) {
    return pFlac->metadata.header.length;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_streaminfo(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_STREAMINFO;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_padding(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_PADDING;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_application(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_APPLICATION;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_seektable(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_SEEKTABLE;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_vorbis_comment(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_VORBIS_COMMENT;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_cuesheet(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_CUESHEET;
}

MINIFLAC_API
uint8_t
miniflac_metadata_is_picture(miniflac_t* pFlac) {
    return pFlac->metadata.header.type == MINIFLAC_METADATA_PICTURE;
}

MINIFLAC_API
uint8_t
miniflac_frame_blocking_strategy(miniflac_t* pFlac) {
    return pFlac->frame.header.blocking_strategy;
}

MINIFLAC_API
uint16_t
miniflac_frame_block_size(miniflac_t* pFlac) {
    return pFlac->frame.header.block_size;
}

MINIFLAC_API
uint32_t
miniflac_frame_sample_rate(miniflac_t* pFlac) {
    return pFlac->frame.header.sample_rate;
}

MINIFLAC_API
uint8_t
miniflac_frame_channels(miniflac_t* pFlac) {
    return pFlac->frame.header.channels;
}

MINIFLAC_API
uint8_t
miniflac_frame_bps(miniflac_t* pFlac) {
    return pFlac->frame.header.bps;
}

MINIFLAC_API
uint64_t
miniflac_frame_sample_number(miniflac_t* pFlac) {
    return pFlac->frame.header.sample_number;
}

MINIFLAC_API
uint32_t
miniflac_frame_frame_number(miniflac_t* pFlac) {
    return pFlac->frame.header.frame_number;
}

MINIFLAC_API
uint32_t
miniflac_frame_header_size(miniflac_t* pFlac) {
    return pFlac->frame.header.size;
}

MINIFLAC_API
int32_t
miniflac_ogg_serial(miniflac_t* pFlac) {
    return pFlac->oggserial;
}

MINIFLAC_API
uint64_t
miniflac_bytes_read_flac(miniflac_t* pFlac) {
    return pFlac->bytes_read_flac;
}

MINIFLAC_API
uint64_t
miniflac_bytes_read_ogg(miniflac_t* pFlac) {
    return pFlac->bytes_read_ogg;
}

#define MINIFLAC_SUBSYS(subsys) &pFlac->metadata.subsys

#define MINIFLAC_GEN_NATIVE_FUNC1(mt,subsys,val,t) \
static \
MINIFLAC_RESULT \
miniflac_ ## subsys ## _ ## val ## _native(miniflac_t *pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, t* outvar) { \
    MINIFLAC_RESULT r; \
    pFlac->br.buffer = data; \
    pFlac->br.len    = length; \
    pFlac->br.pos    = 0; \
    while(pFlac->state != MINIFLAC_METADATA) { \
        r = miniflac_sync_internal(pFlac,&pFlac->br); \
        if(r != MINIFLAC_OK) goto miniflac_ ## subsys ## _ ## val ##_exit; \
    } \
    while(pFlac->metadata.header.type != MINIFLAC_METADATA_ ## mt) { \
        r = miniflac_sync_internal(pFlac,&pFlac->br); \
        if(r != MINIFLAC_OK) goto miniflac_## subsys ## _ ## val ## _exit; \
        if(pFlac->state != MINIFLAC_METADATA) { \
            r = MINIFLAC_ERROR; \
            goto miniflac_ ## subsys ## _ ## val ## _exit; \
        } \
    } \
    r = miniflac_ ## subsys ## _read_ ## val(MINIFLAC_SUBSYS(subsys),&pFlac->br, outvar); \
    miniflac_ ## subsys ## _ ## val ## _exit: \
    *out_length = pFlac->br.pos; \
    pFlac->bytes_read_flac += pFlac->br.pos; \
    return r; \
}

#define MINIFLAC_GEN_OGG_FUNC1(subsys,val,t) \
static \
MINIFLAC_RESULT \
miniflac_ ## subsys ## _ ## val ## _ogg(miniflac_t *pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, t* outvar) { \
    MINIFLAC_RESULT r = MINIFLAC_CONTINUE; \
    const uint8_t* packet = NULL; \
    uint32_t packet_length = 0; \
    uint32_t packet_used   = 0; \
    pFlac->ogg.br.buffer = data; \
    pFlac->ogg.br.len = length; \
    pFlac->ogg.br.pos = 0; \
    do { \
        r = miniflac_oggfunction_start(pFlac,data,&packet,&packet_length); \
        if(r  != MINIFLAC_OK) break; \
        r = miniflac_ ## subsys ## _ ## val ## _native(pFlac,packet,packet_length,&packet_used,outvar); \
        miniflac_oggfunction_end(pFlac,packet_used); \
    } while(r == MINIFLAC_CONTINUE && pFlac->ogg.br.pos < length); \
    *out_length = pFlac->ogg.br.pos; \
    pFlac->bytes_read_ogg += pFlac->ogg.br.pos; \
    return r; \
} \

#define MINIFLAC_GEN_FUNC1(mt,subsys,val,t) \
MINIFLAC_GEN_NATIVE_FUNC1(mt,subsys,val,t) \
MINIFLAC_GEN_OGG_FUNC1(subsys,val,t) \
MINIFLAC_API \
MINIFLAC_RESULT \
miniflac_ ## subsys ## _ ## val(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, t* outvar) { \
  MINIFLAC_RESULT r; \
  if(pFlac->container == MINIFLAC_CONTAINER_UNKNOWN) { \
        r = miniflac_probe(pFlac,data,length); \
        if(r != MINIFLAC_OK) return r; \
    } \
    if(pFlac->container == MINIFLAC_CONTAINER_NATIVE) { \
        r = miniflac_ ## subsys ## _ ## val ## _native(pFlac,data,length,out_length,outvar); \
    } else { \
        r = miniflac_ ## subsys ## _ ## val ## _ogg(pFlac,data,length,out_length,outvar); \
    } \
    return r; \
}

#define MINIFLAC_GEN_NATIVE_FUNCSTR(mt,subsys,val,t) \
static \
MINIFLAC_RESULT \
miniflac_ ## subsys ## _ ## val ## _native(miniflac_t *pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, t* buffer, uint32_t bufferlen, uint32_t* outlen)  { \
    MINIFLAC_RESULT r; \
    pFlac->br.buffer = data; \
    pFlac->br.len    = length; \
    pFlac->br.pos    = 0; \
    while(pFlac->state != MINIFLAC_METADATA) { \
        r = miniflac_sync_internal(pFlac,&pFlac->br); \
        if(r != MINIFLAC_OK) goto miniflac_ ## subsys ## _ ## val ##_exit; \
    } \
    while(pFlac->metadata.header.type != MINIFLAC_METADATA_ ## mt) { \
        r = miniflac_sync_internal(pFlac,&pFlac->br); \
        if(r != MINIFLAC_OK) goto miniflac_## subsys ## _ ## val ## _exit; \
        if(pFlac->state != MINIFLAC_METADATA) { \
            r = MINIFLAC_ERROR; \
            goto miniflac_ ## subsys ## _ ## val ## _exit; \
        } \
    } \
    r = miniflac_ ## subsys ## _read_ ## val(MINIFLAC_SUBSYS(subsys),&pFlac->br, buffer, bufferlen, outlen); \
    miniflac_ ## subsys ## _ ## val ## _exit: \
    *out_length = pFlac->br.pos; \
    pFlac->bytes_read_flac += pFlac->br.pos; \
    return r; \
}

#define MINIFLAC_GEN_OGG_FUNCSTR(subsys,val,t) \
static \
MINIFLAC_RESULT \
miniflac_ ## subsys ## _ ## val ## _ogg(miniflac_t *pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, t* buffer, uint32_t bufferlen, uint32_t* outlen)  { \
    MINIFLAC_RESULT r = MINIFLAC_CONTINUE; \
    const uint8_t* packet = NULL; \
    uint32_t packet_length = 0; \
    uint32_t packet_used   = 0; \
    pFlac->ogg.br.buffer = data; \
    pFlac->ogg.br.len = length; \
    pFlac->ogg.br.pos = 0; \
    do { \
        r = miniflac_oggfunction_start(pFlac,data,&packet,&packet_length); \
        if(r  != MINIFLAC_OK) break; \
        r = miniflac_ ## subsys ## _ ## val ##_native(pFlac,packet,packet_length,&packet_used,buffer,bufferlen,outlen); \
        miniflac_oggfunction_end(pFlac,packet_used); \
    } while(r == MINIFLAC_CONTINUE && pFlac->ogg.br.pos < length); \
    *out_length = pFlac->ogg.br.pos; \
    pFlac->bytes_read_ogg += pFlac->ogg.br.pos; \
    return r; \
} \

#define MINIFLAC_GEN_FUNCSTR(mt,subsys,val,t) \
MINIFLAC_GEN_NATIVE_FUNCSTR(mt,subsys,val,t) \
MINIFLAC_GEN_OGG_FUNCSTR(subsys,val,t) \
MINIFLAC_API \
MINIFLAC_RESULT \
miniflac_ ## subsys ## _ ## val(miniflac_t* pFlac, const uint8_t* data, uint32_t length, uint32_t* out_length, t* output, uint32_t buffer_length, uint32_t* outlen) { \
  MINIFLAC_RESULT r; \
  if(pFlac->container == MINIFLAC_CONTAINER_UNKNOWN) { \
        r = miniflac_probe(pFlac,data,length); \
        if(r != MINIFLAC_OK) return r; \
    } \
    if(pFlac->container == MINIFLAC_CONTAINER_NATIVE) { \
        r = miniflac_ ## subsys ## _ ## val ## _native(pFlac,data,length,out_length,output,buffer_length,outlen); \
    } else { \
        r = miniflac_ ## subsys ## _ ## val ## _ogg(pFlac,data,length,out_length,output,buffer_length,outlen); \
    } \
    return r; \
}

MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,min_block_size,uint16_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,max_block_size,uint16_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,min_frame_size,uint32_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,max_frame_size,uint32_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,sample_rate,uint32_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,channels,uint8_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,bps,uint8_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,total_samples,uint64_t)
MINIFLAC_GEN_FUNC1(STREAMINFO,streaminfo,md5_length,uint32_t)
MINIFLAC_GEN_FUNCSTR(STREAMINFO,streaminfo,md5_data,uint8_t)

MINIFLAC_GEN_FUNC1(VORBIS_COMMENT,vorbis_comment,vendor_length,uint32_t)
MINIFLAC_GEN_FUNCSTR(VORBIS_COMMENT,vorbis_comment,vendor_string,char)
MINIFLAC_GEN_FUNC1(VORBIS_COMMENT,vorbis_comment,total,uint32_t)
MINIFLAC_GEN_FUNC1(VORBIS_COMMENT,vorbis_comment,length,uint32_t)
MINIFLAC_GEN_FUNCSTR(VORBIS_COMMENT,vorbis_comment,string,char)

MINIFLAC_GEN_FUNC1(PICTURE,picture,type,uint32_t)
MINIFLAC_GEN_FUNC1(PICTURE,picture,mime_length,uint32_t)
MINIFLAC_GEN_FUNCSTR(PICTURE,picture,mime_string,char)
MINIFLAC_GEN_FUNC1(PICTURE,picture,description_length,uint32_t)
MINIFLAC_GEN_FUNCSTR(PICTURE,picture,description_string,char)
MINIFLAC_GEN_FUNC1(PICTURE,picture,width,uint32_t)
MINIFLAC_GEN_FUNC1(PICTURE,picture,height,uint32_t)
MINIFLAC_GEN_FUNC1(PICTURE,picture,colordepth,uint32_t)
MINIFLAC_GEN_FUNC1(PICTURE,picture,totalcolors,uint32_t)
MINIFLAC_GEN_FUNC1(PICTURE,picture,length,uint32_t)
MINIFLAC_GEN_FUNCSTR(PICTURE,picture,data,uint8_t)

MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,catalog_length,uint32_t)
MINIFLAC_GEN_FUNCSTR(CUESHEET,cuesheet,catalog_string,char)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,leadin,uint64_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,cd_flag,uint8_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,tracks,uint8_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,track_offset,uint64_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,track_number,uint8_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,track_isrc_length,uint32_t)
MINIFLAC_GEN_FUNCSTR(CUESHEET,cuesheet,track_isrc_string,char)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,track_audio_flag,uint8_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,track_preemph_flag,uint8_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,track_indexpoints,uint8_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,index_point_offset,uint64_t)
MINIFLAC_GEN_FUNC1(CUESHEET,cuesheet,index_point_number,uint8_t)

MINIFLAC_GEN_FUNC1(SEEKTABLE,seektable,seekpoints,uint32_t)
MINIFLAC_GEN_FUNC1(SEEKTABLE,seektable,sample_number,uint64_t)
MINIFLAC_GEN_FUNC1(SEEKTABLE,seektable,sample_offset,uint64_t)
MINIFLAC_GEN_FUNC1(SEEKTABLE,seektable,samples,uint16_t)

MINIFLAC_GEN_FUNC1(APPLICATION,application,id,uint32_t)
MINIFLAC_GEN_FUNC1(APPLICATION,application,length,uint32_t)
MINIFLAC_GEN_FUNCSTR(APPLICATION,application,data,uint8_t)

MINIFLAC_GEN_FUNC1(PADDING,padding,length,uint32_t)
MINIFLAC_GEN_FUNCSTR(PADDING,padding,data,uint8_t)
MINIFLAC_PRIVATE
uint32_t
miniflac_unpack_uint32le(uint8_t buffer[4]) {
    return (
      (((uint32_t)buffer[0]) << 0 ) |
      (((uint32_t)buffer[1]) << 8 ) |
      (((uint32_t)buffer[2]) << 16) |
      (((uint32_t)buffer[3]) << 24));
}

MINIFLAC_PRIVATE
int32_t
miniflac_unpack_int32le(uint8_t buffer[4]) {
    return (int32_t)miniflac_unpack_uint32le(buffer);
}

MINIFLAC_PRIVATE
uint64_t
miniflac_unpack_uint64le(uint8_t buffer[8]) {
    return (
      (((uint64_t)buffer[0]) << 0 ) |
      (((uint64_t)buffer[1]) << 8 ) |
      (((uint64_t)buffer[2]) << 16) |
      (((uint64_t)buffer[3]) << 24) |
      (((uint64_t)buffer[4]) << 32) |
      (((uint64_t)buffer[5]) << 40) |
      (((uint64_t)buffer[6]) << 48) |
      (((uint64_t)buffer[7]) << 56));
}

MINIFLAC_PRIVATE
int64_t
miniflac_unpack_int64le(uint8_t buffer[8]) {
    return (int64_t)miniflac_unpack_uint64le(buffer);
}

static const uint8_t miniflac_crc8_table[256] = {
  0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15,
  0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
  0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65,
  0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
  0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5,
  0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
  0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
  0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
  0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2,
  0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
  0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2,
  0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
  0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32,
  0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
  0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42,
  0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
  0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c,
  0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
  0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec,
  0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
  0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
  0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
  0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c,
  0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
  0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b,
  0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
  0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b,
  0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
  0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb,
  0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
  0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb,
  0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3,
};

static const uint16_t miniflac_crc16_table[256] = {
  0x0000, 0x8005, 0x800f, 0x000a, 0x801b, 0x001e, 0x0014, 0x8011,
  0x8033, 0x0036, 0x003c, 0x8039, 0x0028, 0x802d, 0x8027, 0x0022,
  0x8063, 0x0066, 0x006c, 0x8069, 0x0078, 0x807d, 0x8077, 0x0072,
  0x0050, 0x8055, 0x805f, 0x005a, 0x804b, 0x004e, 0x0044, 0x8041,
  0x80c3, 0x00c6, 0x00cc, 0x80c9, 0x00d8, 0x80dd, 0x80d7, 0x00d2,
  0x00f0, 0x80f5, 0x80ff, 0x00fa, 0x80eb, 0x00ee, 0x00e4, 0x80e1,
  0x00a0, 0x80a5, 0x80af, 0x00aa, 0x80bb, 0x00be, 0x00b4, 0x80b1,
  0x8093, 0x0096, 0x009c, 0x8099, 0x0088, 0x808d, 0x8087, 0x0082,
  0x8183, 0x0186, 0x018c, 0x8189, 0x0198, 0x819d, 0x8197, 0x0192,
  0x01b0, 0x81b5, 0x81bf, 0x01ba, 0x81ab, 0x01ae, 0x01a4, 0x81a1,
  0x01e0, 0x81e5, 0x81ef, 0x01ea, 0x81fb, 0x01fe, 0x01f4, 0x81f1,
  0x81d3, 0x01d6, 0x01dc, 0x81d9, 0x01c8, 0x81cd, 0x81c7, 0x01c2,
  0x0140, 0x8145, 0x814f, 0x014a, 0x815b, 0x015e, 0x0154, 0x8151,
  0x8173, 0x0176, 0x017c, 0x8179, 0x0168, 0x816d, 0x8167, 0x0162,
  0x8123, 0x0126, 0x012c, 0x8129, 0x0138, 0x813d, 0x8137, 0x0132,
  0x0110, 0x8115, 0x811f, 0x011a, 0x810b, 0x010e, 0x0104, 0x8101,
  0x8303, 0x0306, 0x030c, 0x8309, 0x0318, 0x831d, 0x8317, 0x0312,
  0x0330, 0x8335, 0x833f, 0x033a, 0x832b, 0x032e, 0x0324, 0x8321,
  0x0360, 0x8365, 0x836f, 0x036a, 0x837b, 0x037e, 0x0374, 0x8371,
  0x8353, 0x0356, 0x035c, 0x8359, 0x0348, 0x834d, 0x8347, 0x0342,
  0x03c0, 0x83c5, 0x83cf, 0x03ca, 0x83db, 0x03de, 0x03d4, 0x83d1,
  0x83f3, 0x03f6, 0x03fc, 0x83f9, 0x03e8, 0x83ed, 0x83e7, 0x03e2,
  0x83a3, 0x03a6, 0x03ac, 0x83a9, 0x03b8, 0x83bd, 0x83b7, 0x03b2,
  0x0390, 0x8395, 0x839f, 0x039a, 0x838b, 0x038e, 0x0384, 0x8381,
  0x0280, 0x8285, 0x828f, 0x028a, 0x829b, 0x029e, 0x0294, 0x8291,
  0x82b3, 0x02b6, 0x02bc, 0x82b9, 0x02a8, 0x82ad, 0x82a7, 0x02a2,
  0x82e3, 0x02e6, 0x02ec, 0x82e9, 0x02f8, 0x82fd, 0x82f7, 0x02f2,
  0x02d0, 0x82d5, 0x82df, 0x02da, 0x82cb, 0x02ce, 0x02c4, 0x82c1,
  0x8243, 0x0246, 0x024c, 0x8249, 0x0258, 0x825d, 0x8257, 0x0252,
  0x0270, 0x8275, 0x827f, 0x027a, 0x826b, 0x026e, 0x0264, 0x8261,
  0x0220, 0x8225, 0x822f, 0x022a, 0x823b, 0x023e, 0x0234, 0x8231,
  0x8213, 0x0216, 0x021c, 0x8219, 0x0208, 0x820d, 0x8207, 0x0202,
};

MINIFLAC_PRIVATE
void
miniflac_bitreader_init(miniflac_bitreader_t* br) {
    br->val = 0;
    br->bits = 0;
    br->crc8 = 0;
    br->crc16 = 0;
    br->pos = 0;
    br->len = 0;
    br->buffer = NULL;
    br->tot = 0;
}

MINIFLAC_PRIVATE
int
miniflac_bitreader_fill(miniflac_bitreader_t* br, uint8_t bits) {
    uint8_t byte = 0;
    assert(bits <= 64);
    if(bits == 0) return 0;
    while(br->bits < bits && br->pos < br->len) {
        byte = br->buffer[br->pos++];
        br->val = (br->val << 8) | byte;
        br->bits += 8;
        br->crc8 = miniflac_crc8_table[br->crc8 ^ byte];
        br->crc16 = miniflac_crc16_table[ (br->crc16 >> 8) ^ byte ] ^ (( br->crc16 & 0x00FF ) << 8);
        br->tot++;
    }
    return br->bits < bits;
}

MINIFLAC_PRIVATE
int
miniflac_bitreader_fill_nocrc(miniflac_bitreader_t* br, uint8_t bits) {
    uint8_t byte = 0;
    assert(bits <= 64);
    if(bits == 0) return 0;
    while(br->bits < bits && br->pos < br->len) {
        byte = br->buffer[br->pos++];
        br->val = (br->val << 8) | byte;
        br->bits += 8;
        br->tot++;
    }
    return br->bits < bits;
}


MINIFLAC_PRIVATE
uint64_t
miniflac_bitreader_read(miniflac_bitreader_t* br, uint8_t bits) {
    uint64_t mask = -1LL;
    uint64_t imask = -1LL;
    uint64_t r;

    assert(bits <= br->bits);
    if(bits == 0) return 0;

    mask >>= (64 - bits);
    br->bits -= bits;
    r = br->val >> br->bits & mask;
    if(br->bits == 0) {
        imask = 0;
    } else {
        imask >>= (64 - br->bits);
    }
    br->val &= imask;

    return r;
}

MINIFLAC_PRIVATE
int64_t
miniflac_bitreader_read_signed(miniflac_bitreader_t* br, uint8_t bits) {
    uint64_t t;
    uint64_t mask = -1LL;
    if(bits == 0) return 0;
    mask <<= bits;

    t = miniflac_bitreader_read(br,bits);
    if( (t & ( 1 << (bits - 1))) ) {
        t |= mask;
    }
    return t;
}


MINIFLAC_PRIVATE
uint64_t
miniflac_bitreader_peek(miniflac_bitreader_t* br, uint8_t bits) {
    uint64_t mask = -1LL;
    uint64_t r;

    assert(bits <= br->bits);
    if(bits == 0) return 0;

    mask >>= (64 - bits);
    r = br->val >> (br->bits - bits) & mask;
    return r;
}

MINIFLAC_PRIVATE
void
miniflac_bitreader_discard(miniflac_bitreader_t* br, uint8_t bits) {
    uint64_t imask = -1LL;

    assert(bits <= br->bits);
    if(bits == 0) return;

    br->bits -= bits;

    if(br->bits == 0) {
        imask = 0;
    } else {
        imask >>= (64 - br->bits);
    }
    br->val &= imask;
}

MINIFLAC_PRIVATE
void
miniflac_bitreader_align(miniflac_bitreader_t* br) {
    assert(br->bits < 8);
    br->bits = 0;
    br->val = 0;
}

MINIFLAC_PRIVATE
void
miniflac_bitreader_reset_crc(miniflac_bitreader_t* br) {
    uint64_t val = br->val;
    uint8_t bits = br->bits;

    uint8_t byte;
    uint64_t mask;
    uint64_t imask;

    br->crc8 = 0;
    br->crc16 = 0;
    br->tot = 0;

    while(bits > 0) {
        mask = -1LL;
        imask = -1LL;

        mask >>= (64 - 8);
        bits -= 8;
        byte = val >> bits & mask;
        if(bits == 0) {
            imask = 0;
        } else {
            imask >>= (64 - bits);
        }
        val &=  imask;
        br->crc8 = miniflac_crc8_table[br->crc8 ^ byte];
        br->crc16 = miniflac_crc16_table[ (br->crc16 >> 8) ^ byte ] ^ (( br->crc16 & 0x00FF ) << 8);
        br->tot++;
    }
}

MINIFLAC_PRIVATE
void
miniflac_oggheader_init(miniflac_oggheader_t* oggheader) {
    oggheader->state = MINIFLAC_OGGHEADER_PACKETTYPE;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_oggheader_decode(miniflac_oggheader_t* oggheader, miniflac_bitreader_t* br) {
    switch(oggheader->state) {
        case MINIFLAC_OGGHEADER_PACKETTYPE: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((uint8_t)miniflac_bitreader_read(br,8) != 0x7F) {
                return MINIFLAC_OGG_HEADER_NOTFLAC;
            }
            oggheader->state = MINIFLAC_OGGHEADER_F;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_F: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((char)miniflac_bitreader_read(br,8) != 'F') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            oggheader->state = MINIFLAC_OGGHEADER_L;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_L: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((char)miniflac_bitreader_read(br,8) != 'L') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            oggheader->state = MINIFLAC_OGGHEADER_A;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_A: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((char)miniflac_bitreader_read(br,8) != 'A') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            oggheader->state = MINIFLAC_OGGHEADER_C;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_C: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((char)miniflac_bitreader_read(br,8) != 'C') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            oggheader->state = MINIFLAC_OGGHEADER_MAJOR;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_MAJOR: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((uint8_t)miniflac_bitreader_read(br,8) != 0x01) {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            oggheader->state = MINIFLAC_OGGHEADER_MINOR;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_MINOR: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            if((uint8_t)miniflac_bitreader_read(br,8) != 0x00) {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            oggheader->state = MINIFLAC_OGGHEADER_HEADERPACKETS;
        }
        /* fall-through */
        case MINIFLAC_OGGHEADER_HEADERPACKETS: {
            if(miniflac_bitreader_fill_nocrc(br,16)) return MINIFLAC_CONTINUE;
            miniflac_bitreader_discard(br,16);
            oggheader->state = MINIFLAC_OGGHEADER_PACKETTYPE;
        }
        /* fall-through */
        default: break;
    }
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_ogg_init(miniflac_ogg_t* ogg) {
    ogg->state = MINIFLAC_OGG_CAPTUREPATTERN_O;
    ogg->version = 0;
    ogg->headertype = 0;
    ogg->granulepos = 0;
    ogg->serialno = 0;
    ogg->pageno = 0;
    ogg->segments = 0;
    ogg->curseg = 0;
    ogg->length = 0;
    ogg->pos = 0;
    miniflac_bitreader_init(&ogg->br);
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_ogg_sync(miniflac_ogg_t* ogg,miniflac_bitreader_t* br) {
    unsigned char c;
    uint8_t buffer[8];

    switch(ogg->state) {
        case MINIFLAC_OGG_SKIP: /* fall-through */
        case MINIFLAC_OGG_DATA: {
            while(ogg->pos < ogg->length) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                miniflac_bitreader_discard(br,8);
                ogg->pos++;
            }
            ogg->state = MINIFLAC_OGG_CAPTUREPATTERN_O;
        }
        /* fall-through */
        case MINIFLAC_OGG_CAPTUREPATTERN_O: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            c = (unsigned char)miniflac_bitreader_read(br,8);
            if(c != 'O') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            ogg->state = MINIFLAC_OGG_CAPTUREPATTERN_G1;
        }
        /* fall-through */
        case MINIFLAC_OGG_CAPTUREPATTERN_G1: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            c = (unsigned char)miniflac_bitreader_read(br,8);
            if(c != 'g') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            ogg->state = MINIFLAC_OGG_CAPTUREPATTERN_G2;
        }
        /* fall-through */
        case MINIFLAC_OGG_CAPTUREPATTERN_G2: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            c = (unsigned char)miniflac_bitreader_read(br,8);
            if(c != 'g') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            ogg->state = MINIFLAC_OGG_CAPTUREPATTERN_S;
        }
        /* fall-through */
        case MINIFLAC_OGG_CAPTUREPATTERN_S: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            c = (unsigned char)miniflac_bitreader_read(br,8);
            if(c != 'S') {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            ogg->state = MINIFLAC_OGG_VERSION;
        }
        /* fall-through */
        case MINIFLAC_OGG_VERSION: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            ogg->version = (uint8_t)miniflac_bitreader_read(br,8);
            if(ogg->version != 0) {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            ogg->state = MINIFLAC_OGG_HEADERTYPE;
        }
        /* fall-through */
        case MINIFLAC_OGG_HEADERTYPE: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            ogg->headertype = (uint8_t)miniflac_bitreader_read(br,8);
            ogg->state = MINIFLAC_OGG_GRANULEPOS;
        }
        /* fall-through */
        case MINIFLAC_OGG_GRANULEPOS: {
            if(miniflac_bitreader_fill_nocrc(br,64)) return MINIFLAC_CONTINUE;
            buffer[0] = miniflac_bitreader_read(br,8);
            buffer[1] = miniflac_bitreader_read(br,8);
            buffer[2] = miniflac_bitreader_read(br,8);
            buffer[3] = miniflac_bitreader_read(br,8);
            buffer[4] = miniflac_bitreader_read(br,8);
            buffer[5] = miniflac_bitreader_read(br,8);
            buffer[6] = miniflac_bitreader_read(br,8);
            buffer[7] = miniflac_bitreader_read(br,8);
            ogg->granulepos = miniflac_unpack_int64le(buffer);
            ogg->state = MINIFLAC_OGG_SERIALNO;
        }
        /* fall-through */
        case MINIFLAC_OGG_SERIALNO: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            buffer[0] = miniflac_bitreader_read(br,8);
            buffer[1] = miniflac_bitreader_read(br,8);
            buffer[2] = miniflac_bitreader_read(br,8);
            buffer[3] = miniflac_bitreader_read(br,8);
            ogg->serialno = miniflac_unpack_int32le(buffer);
            ogg->state = MINIFLAC_OGG_PAGENO;
        }
        /* fall-through */
        case MINIFLAC_OGG_PAGENO: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            buffer[0] = miniflac_bitreader_read(br,8);
            buffer[1] = miniflac_bitreader_read(br,8);
            buffer[2] = miniflac_bitreader_read(br,8);
            buffer[3] = miniflac_bitreader_read(br,8);
            ogg->pageno = miniflac_unpack_uint32le(buffer);
            ogg->state = MINIFLAC_OGG_CHECKSUM;
        }
        /* fall-through */
        case MINIFLAC_OGG_CHECKSUM: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            miniflac_bitreader_discard(br,32);
            ogg->state = MINIFLAC_OGG_PAGESEGMENTS;
        }
        /* fall-through */
        case MINIFLAC_OGG_PAGESEGMENTS: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            ogg->segments = (uint8_t) miniflac_bitreader_read(br,8);
            ogg->curseg = 0;
            ogg->length = 0;
            ogg->state = MINIFLAC_OGG_SEGMENTTABLE;
        }
        /* fall-through */
        case MINIFLAC_OGG_SEGMENTTABLE: {
            while(ogg->curseg < ogg->segments) {
              if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
              ogg->length += miniflac_bitreader_read(br,8);
              ogg->curseg++;
            }
            ogg->pos = 0;
            ogg->state = MINIFLAC_OGG_DATA;
            return MINIFLAC_OK;
        }
    }
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_frame_init(miniflac_frame_t* frame) {
    frame->crc16 = 0;
    frame->cur_subframe = 0;
    frame->state = MINIFLAC_FRAME_HEADER;
    miniflac_frame_header_init(&frame->header);
    miniflac_subframe_init(&frame->subframe);
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_frame_sync(miniflac_frame_t* frame, miniflac_bitreader_t* br, miniflac_streaminfo_t* info) {
    MINIFLAC_RESULT r;
    assert(frame->state == MINIFLAC_FRAME_HEADER);
    r = miniflac_frame_header_decode(&frame->header,br);
    if(r != MINIFLAC_OK) return r;

    if(frame->header.sample_rate == 0) {
        if(info->sample_rate == 0) return MINIFLAC_FRAME_INVALID_SAMPLE_RATE;
        frame->header.sample_rate = info->sample_rate;
    }

    if(frame->header.bps == 0) {
        if(info->bps == 0) return MINIFLAC_FRAME_INVALID_SAMPLE_SIZE;
        frame->header.bps = info->bps;
    }

    frame->state = MINIFLAC_FRAME_SUBFRAME;
    frame->cur_subframe = 0;
    miniflac_subframe_init(&frame->subframe);
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_frame_decode(miniflac_frame_t* frame, miniflac_bitreader_t* br, miniflac_streaminfo_t* info, int32_t** output) {
    MINIFLAC_RESULT r;
    uint32_t bps;
    uint32_t i;
    uint64_t m,s;
    uint16_t t;
    switch(frame->state) {
        case MINIFLAC_FRAME_HEADER: {
            r = miniflac_frame_sync(frame,br,info);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_FRAME_SUBFRAME: {
            while(frame->cur_subframe < frame->header.channels) {
                bps = frame->header.bps;
                if(frame->header.channel_assignment == MINIFLAC_CHASSGN_LEFT_SIDE || frame->header.channel_assignment == MINIFLAC_CHASSGN_MID_SIDE) {
                    if(frame->cur_subframe == 1) bps += 1;
                } else if(frame->header.channel_assignment == MINIFLAC_CHASSGN_RIGHT_SIDE) {
                    if(frame->cur_subframe == 0) bps += 1;
                }
                r = miniflac_subframe_decode(&frame->subframe,br,output == NULL ? NULL : output[frame->cur_subframe],frame->header.block_size,bps);
                if(r != MINIFLAC_OK) return r;

                miniflac_subframe_init(&frame->subframe);
                frame->cur_subframe++;
            }

            miniflac_bitreader_align(br);
            frame->crc16 = br->crc16;
            frame->state = MINIFLAC_FRAME_FOOTER;
        }
        /* fall-through */
        case MINIFLAC_FRAME_FOOTER: {
            if(miniflac_bitreader_fill(br,16)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,16);
            if(frame->crc16 != t) {
                miniflac_abort();
                return MINIFLAC_FRAME_CRC16_INVALID;
            }
            frame->size = br->tot;
            if(output != NULL) {
                switch(frame->header.channel_assignment) {
                    case MINIFLAC_CHASSGN_LEFT_SIDE: {
                        for(i=0;i<frame->header.block_size;i++) {
                            output[1][i] = output[0][i] - output[1][i];
                        }
                        break;
                    }
                    case MINIFLAC_CHASSGN_RIGHT_SIDE: {
                        for(i=0;i<frame->header.block_size;i++) {
                            output[0][i] = output[0][i] + output[1][i];
                        }
                        break;
                    }
                    case MINIFLAC_CHASSGN_MID_SIDE: {
                        for(i=0;i<frame->header.block_size;i++) {
                            m = (uint64_t)output[0][i];
                            s = (uint64_t)output[1][i];
                            m = (m << 1) | (s & 0x01);
                            output[0][i] = (int32_t)((m + s) >> 1 );
                            output[1][i] = (int32_t)((m - s) >> 1 );
                        }
                        break;
                    }
                    default: break;
                }
            }
            break;
        }
        default: {
            /* invalid state */
            miniflac_abort();
            return MINIFLAC_ERROR;
        }
    }

    assert(br->bits == 0);
    br->crc8 = 0;
    br->crc16 = 0;
    frame->cur_subframe = 0;
    frame->state = MINIFLAC_FRAME_HEADER;
    miniflac_subframe_init(&frame->subframe);
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_frame_header_init(miniflac_frame_header_t* header) {
    header->state = MINIFLAC_FRAME_HEADER_SYNC;
    header->block_size_raw = 0;
    header->sample_rate_raw = 0;
    header->channel_assignment_raw = 0;
    header->sample_rate = 0;
    header->blocking_strategy = 0;
    header->block_size = 0;
    header->sample_rate = 0;
    header->channel_assignment = MINIFLAC_CHASSGN_NONE;
    header->channels = 0;
    header->bps = 0;
    header->sample_number = 0;
    header->crc8 = 0;
    header->size = 0;
}


MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_frame_header_decode(miniflac_frame_header_t* header, miniflac_bitreader_t* br) {
    uint64_t t;

    switch(header->state) {
        case MINIFLAC_FRAME_HEADER_SYNC: {
            miniflac_bitreader_reset_crc(br);
            if(miniflac_bitreader_fill(br,14)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,14);
            if(t != 0x3FFE) {
                miniflac_abort();
                return MINIFLAC_FRAME_SYNCCODE_INVALID;
            }
            miniflac_frame_header_init(header);
            header->state = MINIFLAC_FRAME_HEADER_RESERVEBIT_1;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_RESERVEBIT_1: {
            if(miniflac_bitreader_fill(br,1)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,1);
            if(t != 0) {
                miniflac_abort();
                return MINIFLAC_FRAME_RESERVED_BIT1;
            }
            header->state = MINIFLAC_FRAME_HEADER_BLOCKINGSTRATEGY;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_BLOCKINGSTRATEGY: {
            if(miniflac_bitreader_fill(br,1)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,1);
            header->blocking_strategy = t;
            header->state = MINIFLAC_FRAME_HEADER_BLOCKSIZE;
            header->size += 2;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_BLOCKSIZE: {
            if(miniflac_bitreader_fill(br,4)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,4);
            header->block_size_raw = t;
            header->block_size = 0;
            switch(header->block_size_raw) {
                case 0: {
                    miniflac_abort();
                    return MINIFLAC_FRAME_RESERVED_BLOCKSIZE;
                }
                case 1: {
                    header->block_size = 192;
                    break;
                }
                case 2: {
                    header->block_size = 576;
                    break;
                }
                case 3: {
                    header->block_size = 1152;
                    break;
                }
                case 4: {
                    header->block_size = 2304;
                    break;
                }
                case 5: {
                    header->block_size = 4608;
                    break;
                }
                case 8: {
                    header->block_size = 256;
                    break;
                }
                case 9: {
                    header->block_size = 512;
                    break;
                }
                case 10: {
                    header->block_size = 1024;
                    break;
                }
                case 11: {
                    header->block_size = 2048;
                    break;
                }
                case 12: {
                    header->block_size = 4096;
                    break;
                }
                case 13: {
                    header->block_size = 8192;
                    break;
                }
                case 14: {
                    header->block_size = 16384;
                    break;
                }
                case 15: {
                    header->block_size = 32768;
                    break;
                }
                default: break;
            }
            header->state = MINIFLAC_FRAME_HEADER_SAMPLERATE;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLERATE: {
            if(miniflac_bitreader_fill(br,4)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,4);
            header->sample_rate_raw = t;
            switch(header->sample_rate_raw) {
                case 0: {
                    header->sample_rate = 0;
                    break;
                }
                case 1: {
                    header->sample_rate = 88200;
                    break;
                }
                case 2: {
                    header->sample_rate = 176400;
                    break;
                }
                case 3: {
                    header->sample_rate = 192000;
                    break;
                }
                case 4: {
                    header->sample_rate = 8000;
                    break;
                }
                case 5: {
                    header->sample_rate = 16000;
                    break;
                }
                case 6: {
                    header->sample_rate = 22050;
                    break;
                }
                case 7: {
                    header->sample_rate = 24000;
                    break;
                }
                case 8: {
                    header->sample_rate = 32000;
                    break;
                }
                case 9: {
                    header->sample_rate = 44100;
                    break;
                }
                case 10: {
                    header->sample_rate = 48000;
                    break;
                }
                case 11: {
                    header->sample_rate = 96000;
                    break;
                }
                case 12: /* fall-through */
                case 13: /* fall-through */
                case 14: {
                    header->sample_rate = 0;
                    break;
                }
                case 15: {
                    miniflac_abort();
                    return MINIFLAC_FRAME_INVALID_SAMPLE_RATE;
                }
                default: break;
            }

            header->state = MINIFLAC_FRAME_HEADER_CHANNELASSIGNMENT;
            header->size += 1;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_CHANNELASSIGNMENT: {
            if(miniflac_bitreader_fill(br,4)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,4);
            if(t > 10) {
                miniflac_abort();
                return MINIFLAC_FRAME_RESERVED_CHANNEL_ASSIGNMENT;
            }

            if(t < 8) {
                header->channels = t + 1;
                header->channel_assignment = MINIFLAC_CHASSGN_NONE;
            } else {
                switch(t) {
                    case 8: {
                        header->channel_assignment = MINIFLAC_CHASSGN_LEFT_SIDE; break;
                    }
                    case 9: {
                        header->channel_assignment = MINIFLAC_CHASSGN_RIGHT_SIDE; break;
                    }
                    case 10: {
                        header->channel_assignment = MINIFLAC_CHASSGN_MID_SIDE; break;
                    }
                    default: break;
                }
                header->channels = 2;
            }

            header->channel_assignment_raw = t;
            header->state = MINIFLAC_FRAME_HEADER_SAMPLESIZE;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLESIZE: {
            if(miniflac_bitreader_fill(br,3)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,3);
            switch(t) {
                case 0: header->bps = 0; break;
                case 1: header->bps = 8; break;
                case 2: header->bps = 12; break;
                case 3: {
                    miniflac_abort();
                    return MINIFLAC_FRAME_RESERVED_SAMPLE_SIZE;
                }
                case 4: header->bps = 16; break;
                case 5: header->bps = 20; break;
                case 6: header->bps = 24; break;
                case 7: {
                    miniflac_abort();
                    return MINIFLAC_FRAME_RESERVED_SAMPLE_SIZE;
                }
            }
            header->state = MINIFLAC_FRAME_HEADER_RESERVEBIT_2;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_RESERVEBIT_2: {
            if(miniflac_bitreader_fill(br,1)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,1);
            if(t != 0) {
                miniflac_abort();
                return MINIFLAC_FRAME_RESERVED_BIT2;
            }
            header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_1;
            header->size += 1;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_1: {
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);

            if((t & 0x80) == 0x00) {
                header->sample_number = t;
                header->state = MINIFLAC_FRAME_HEADER_BLOCKSIZE_MAYBE;
                header->size += 1;
                goto flac_frame_blocksize_maybe;
            }
            else if((t & 0xE0) == 0xC0) {
                header->sample_number = (t & 0x1F) << 6;
                header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_7;
                header->size += 2;
                goto flac_frame_samplenumber_7;
            } else if((t & 0xF0) == 0xE0) {
                header->sample_number = (t & 0x0F) << 12;
                header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_6;
                header->size += 3;
                goto flac_frame_samplenumber_6;
            } else if((t & 0xF8) == 0xF0) {
                header->sample_number = (t & 0x07) << 18;
                header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_5;
                header->size += 4;
                goto flac_frame_samplenumber_5;
            } else if((t & 0xFC) == 0xF8) {
                header->sample_number = (t & 0x03) << 24;
                header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_4;
                header->size += 5;
                goto flac_frame_samplenumber_4;
            } else if((t & 0xFE) == 0xFC) {
                header->sample_number = (t & 0x01) << 30;
                header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_3;
                header->size += 6;
                goto flac_frame_samplenumber_3;
            } else if((t & 0xFF) == 0xFE) {
                /* untested, requires a variable blocksize stream with a lot of samples, YMMV */
                header->sample_number = 0;
                header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_2;
                header->size += 7;
                goto flac_frame_samplenumber_2;
            }
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_2: {
            flac_frame_samplenumber_2:
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            header->sample_number += (t & 0x3F) << 30;
            header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_3;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_3: {
            flac_frame_samplenumber_3:
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            header->sample_number += (t & 0x3F) << 24;
            header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_4;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_4: {
            flac_frame_samplenumber_4:
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            header->sample_number += (t & 0x3F) << 18;
            header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_5;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_5: {
            flac_frame_samplenumber_5:
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            header->sample_number += (t & 0x3F) << 12;
            header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_6;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_6: {
            flac_frame_samplenumber_6:
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            header->sample_number += (t & 0x3F) << 6;
            header->state = MINIFLAC_FRAME_HEADER_SAMPLENUMBER_7;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLENUMBER_7: {
            flac_frame_samplenumber_7:
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            header->sample_number += (t & 0x3F);
            header->state = MINIFLAC_FRAME_HEADER_BLOCKSIZE_MAYBE;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_BLOCKSIZE_MAYBE: {
            flac_frame_blocksize_maybe:
            switch(header->block_size_raw) {
                case 6: {
                    if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
                    t = miniflac_bitreader_read(br,8) + 1;
                    header->block_size = t;
                    header->size += 1;
                    break;
                }
                case 7: {
                    if(miniflac_bitreader_fill(br,16)) return MINIFLAC_CONTINUE;
                    t = miniflac_bitreader_read(br,16) + 1;
                    header->block_size = t;
                    header->size += 2;
                    break;
                }
                default: break;
            }
            header->state = MINIFLAC_FRAME_HEADER_SAMPLERATE_MAYBE;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_SAMPLERATE_MAYBE: {
            switch(header->sample_rate_raw) {
                case 12: {
                    if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
                    t = miniflac_bitreader_read(br,8);
                    header->sample_rate = t * 1000;
                    header->size += 1;
                    break;
                }
                case 13: {
                    if(miniflac_bitreader_fill(br,16)) return MINIFLAC_CONTINUE;
                    t = miniflac_bitreader_read(br,16);
                    header->sample_rate = t;
                    header->size += 2;
                    break;
                }
                case 14: {
                    if(miniflac_bitreader_fill(br,16)) return MINIFLAC_CONTINUE;
                    t = miniflac_bitreader_read(br,16);
                    header->sample_rate = t * 10;
                    header->size += 2;
                    break;
                }
                default: break;
            }

            /* grab crc8 from bitreader before fill */
            header->crc8 = br->crc8;
            header->state = MINIFLAC_FRAME_HEADER_CRC8;
        }
        /* fall-through */
        case MINIFLAC_FRAME_HEADER_CRC8: {
            if(miniflac_bitreader_fill(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            if(header->crc8 != t) {
                miniflac_abort();
                return MINIFLAC_FRAME_CRC8_INVALID;
            }
            header->size += 1;
        }
        /* fall-through */
        default: break;
    }

    header->state = MINIFLAC_FRAME_HEADER_SYNC;
    return MINIFLAC_OK;
}


MINIFLAC_PRIVATE
void
miniflac_vorbis_comment_init(miniflac_vorbis_comment_t* vorbis_comment) {
    vorbis_comment->state = MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH;
    vorbis_comment->len = 0;
    vorbis_comment->pos = 0;
    vorbis_comment->tot = 0;
    vorbis_comment->cur = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_vendor_length(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, uint32_t* length) {
    uint8_t buffer[4];
    switch(vorbis_comment->state) {
        case MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            buffer[0] = miniflac_bitreader_read(br,8);
            buffer[1] = miniflac_bitreader_read(br,8);
            buffer[2] = miniflac_bitreader_read(br,8);
            buffer[3] = miniflac_bitreader_read(br,8);
            vorbis_comment->len = miniflac_unpack_uint32le(buffer);
            if(length != NULL) *length = vorbis_comment->len;
            vorbis_comment->state = MINIFLAC_VORBISCOMMENT_VENDOR_STRING;
            return MINIFLAC_OK;
        } default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}


MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_vendor_string(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    char c;

    switch(vorbis_comment->state) {
        case MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH: {
            r = miniflac_vorbis_comment_read_vendor_length(vorbis_comment,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_VORBISCOMMENT_VENDOR_STRING: {
            while(vorbis_comment->pos < vorbis_comment->len) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (char)miniflac_bitreader_read(br,8);
                if(output != NULL && vorbis_comment->pos < length) {
                    output[vorbis_comment->pos] = c;
                }
                vorbis_comment->pos++;
            }
            if(outlen != NULL) {
                *outlen = vorbis_comment->len <= length ? vorbis_comment->len : length;
            }
            vorbis_comment->state = MINIFLAC_VORBISCOMMENT_TOTAL_COMMENTS;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}


MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_total(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, uint32_t* total) {
    uint8_t buffer[4];
    MINIFLAC_RESULT r = MINIFLAC_ERROR;

    switch(vorbis_comment->state) {
        case MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH: /* fall-through */
        case MINIFLAC_VORBISCOMMENT_VENDOR_STRING: {
            r = miniflac_vorbis_comment_read_vendor_string(vorbis_comment,br,NULL,0,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_VORBISCOMMENT_TOTAL_COMMENTS: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            buffer[0] = miniflac_bitreader_read(br,8);
            buffer[1] = miniflac_bitreader_read(br,8);
            buffer[2] = miniflac_bitreader_read(br,8);
            buffer[3] = miniflac_bitreader_read(br,8);
            vorbis_comment->tot = miniflac_unpack_uint32le(buffer);
            if(total != NULL) *total = vorbis_comment->tot;
            vorbis_comment->state = MINIFLAC_VORBISCOMMENT_COMMENT_LENGTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_length(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, uint32_t* length) {
    uint8_t buffer[4];
    MINIFLAC_RESULT r = MINIFLAC_ERROR;

    switch(vorbis_comment->state) {
        case MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH: /* fall-through */
        case MINIFLAC_VORBISCOMMENT_VENDOR_STRING: /* fall-through */
        case MINIFLAC_VORBISCOMMENT_TOTAL_COMMENTS: {
            r = miniflac_vorbis_comment_read_total(vorbis_comment,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_VORBISCOMMENT_COMMENT_LENGTH: {
            case_miniflac_vorbis_comment_comment_length:
            if(vorbis_comment->cur == vorbis_comment->tot) {
                return MINIFLAC_METADATA_END;
            }

            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            buffer[0] = miniflac_bitreader_read(br,8);
            buffer[1] = miniflac_bitreader_read(br,8);
            buffer[2] = miniflac_bitreader_read(br,8);
            buffer[3] = miniflac_bitreader_read(br,8);
            vorbis_comment->len = miniflac_unpack_uint32le(buffer);
            vorbis_comment->pos = 0;
            if(length != NULL) *length = vorbis_comment->len;
            vorbis_comment->state = MINIFLAC_VORBISCOMMENT_COMMENT_STRING;
            return MINIFLAC_OK;
        }
        case MINIFLAC_VORBISCOMMENT_COMMENT_STRING: {
            r = miniflac_vorbis_comment_read_string(vorbis_comment,br,NULL,0,NULL);
            if(r != MINIFLAC_OK) return r;
            goto case_miniflac_vorbis_comment_comment_length;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_vorbis_comment_read_string(miniflac_vorbis_comment_t* vorbis_comment, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    char c;

    switch(vorbis_comment->state) {
        case MINIFLAC_VORBISCOMMENT_VENDOR_LENGTH:
        case MINIFLAC_VORBISCOMMENT_VENDOR_STRING:
        case MINIFLAC_VORBISCOMMENT_TOTAL_COMMENTS:
        case MINIFLAC_VORBISCOMMENT_COMMENT_LENGTH: {
            r = miniflac_vorbis_comment_read_length(vorbis_comment,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_VORBISCOMMENT_COMMENT_STRING: {
            while(vorbis_comment->pos < vorbis_comment->len) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (char)miniflac_bitreader_read(br,8);
                if(output != NULL && vorbis_comment->pos < length) {
                    output[vorbis_comment->pos] = c;
                }
                vorbis_comment->pos++;
            }
            if(outlen != NULL) {
                *outlen = vorbis_comment->len <= length ? vorbis_comment->len : length;
            }
            vorbis_comment->cur++;
            vorbis_comment->state = MINIFLAC_VORBISCOMMENT_COMMENT_LENGTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_picture_init(miniflac_picture_t* picture) {
    picture->state = MINIFLAC_PICTURE_TYPE;
    picture->len = 0;
    picture->pos = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_type(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* type) {
    uint32_t t = 0;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,32);
            if(type != NULL) *type = t;
            picture->state = MINIFLAC_PICTURE_MIME_LENGTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_mime_length(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: {
            r = miniflac_picture_read_type(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            picture->len = miniflac_bitreader_read(br,32);
            picture->pos = 0;
            if(length != NULL) *length = picture->len;
            picture->state = MINIFLAC_PICTURE_MIME_STRING;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_mime_string(miniflac_picture_t* picture, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    char c;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: {
            r = miniflac_picture_read_mime_length(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: {
            while(picture->pos < picture->len) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (char)miniflac_bitreader_read(br,8);
                if(output != NULL && picture->pos < length) {
                    output[picture->pos] = c;
                }
                picture->pos++;
            }
            if(outlen != NULL) {
                *outlen = picture->len <= length ? picture->len : length;
            }
            picture->state = MINIFLAC_PICTURE_DESCRIPTION_LENGTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_description_length(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: {
            r = miniflac_picture_read_mime_string(picture,br,NULL,0,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            picture->len = miniflac_bitreader_read(br,32);
            picture->pos = 0;
            if(length != NULL) *length = picture->len;
            picture->state = MINIFLAC_PICTURE_DESCRIPTION_STRING;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_description_string(miniflac_picture_t* picture, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    char c;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: {
            r = miniflac_picture_read_description_length(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: {
            while(picture->pos < picture->len) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (char)miniflac_bitreader_read(br,8);
                if(output != NULL && picture->pos < length) {
                    output[picture->pos] = c;
                }
                picture->pos++;
            }
            if(outlen != NULL) {
                *outlen = picture->len <= length ? picture->len : length;
            }
            picture->state = MINIFLAC_PICTURE_WIDTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_width(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* width) {
    uint32_t t = 0;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: {
            r = miniflac_picture_read_description_string(picture,br,NULL,0,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_WIDTH: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,32);
            if(width != NULL) *width = t;
            picture->state = MINIFLAC_PICTURE_HEIGHT;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_height(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* height) {
    uint32_t t = 0;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: /* fall-through */
        case MINIFLAC_PICTURE_WIDTH: {
            r = miniflac_picture_read_width(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_HEIGHT: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,32);
            if(height != NULL) *height = t;
            picture->state = MINIFLAC_PICTURE_COLORDEPTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_colordepth(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* colordepth) {
    uint32_t t = 0;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: /* fall-through */
        case MINIFLAC_PICTURE_WIDTH: /* fall-through */
        case MINIFLAC_PICTURE_HEIGHT: {
            r = miniflac_picture_read_height(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_COLORDEPTH: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,32);
            if(colordepth != NULL) *colordepth = t;
            picture->state = MINIFLAC_PICTURE_TOTALCOLORS;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_totalcolors(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* totalcolors) {
    uint32_t t = 0;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: /* fall-through */
        case MINIFLAC_PICTURE_WIDTH: /* fall-through */
        case MINIFLAC_PICTURE_HEIGHT: /* fall-through */
        case MINIFLAC_PICTURE_COLORDEPTH: {
            r = miniflac_picture_read_colordepth(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_TOTALCOLORS: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,32);
            if(totalcolors != NULL) *totalcolors = t;
            picture->state = MINIFLAC_PICTURE_PICTURE_LENGTH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_length(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint32_t* length) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: /* fall-through */
        case MINIFLAC_PICTURE_WIDTH: /* fall-through */
        case MINIFLAC_PICTURE_HEIGHT: /* fall-through */
        case MINIFLAC_PICTURE_COLORDEPTH: /* fall-through */
        case MINIFLAC_PICTURE_TOTALCOLORS: {
            r = miniflac_picture_read_totalcolors(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_PICTURE_LENGTH: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            picture->len = miniflac_bitreader_read(br,32);
            picture->pos = 0;
            if(length != NULL) *length = picture->len;
            picture->state = MINIFLAC_PICTURE_PICTURE_DATA;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_picture_read_data(miniflac_picture_t* picture, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t c;
    switch(picture->state) {
        case MINIFLAC_PICTURE_TYPE: /* fall-through */
        case MINIFLAC_PICTURE_MIME_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_MIME_STRING: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_LENGTH: /* fall-through */
        case MINIFLAC_PICTURE_DESCRIPTION_STRING: /* fall-through */
        case MINIFLAC_PICTURE_WIDTH: /* fall-through */
        case MINIFLAC_PICTURE_HEIGHT: /* fall-through */
        case MINIFLAC_PICTURE_COLORDEPTH: /* fall-through */
        case MINIFLAC_PICTURE_TOTALCOLORS: /* fall-through */
        case MINIFLAC_PICTURE_PICTURE_LENGTH: {
            r = miniflac_picture_read_length(picture,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_PICTURE_PICTURE_DATA: {
            if(picture->pos == picture->len) return MINIFLAC_METADATA_END;
            while(picture->pos < picture->len) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (uint8_t)miniflac_bitreader_read(br,8);
                if(output != NULL && picture->pos < length) {
                    output[picture->pos] = c;
                }
                picture->pos++;
            }
            if(outlen != NULL) {
                *outlen = picture->len <= length ? picture->len : length;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_cuesheet_init(miniflac_cuesheet_t* cuesheet) {
    cuesheet->state = MINIFLAC_CUESHEET_CATALOG;
    cuesheet->pos = 0;
    cuesheet->track = 0;
    cuesheet->tracks = 0;
    cuesheet->point = 0;
    cuesheet->points = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_catalog_length(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint32_t* catalog_length) {
    (void)br;
    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: {
            if(catalog_length != NULL) {
                *catalog_length = 128;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_catalog_string(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen) {
    char c;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: {
            while(cuesheet->pos < 128) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (char)miniflac_bitreader_read(br,8);
                if(output != NULL && cuesheet->pos < length) {
                    output[cuesheet->pos] = c;
                }
                cuesheet->pos++;
            }
            if(outlen != NULL) {
                *outlen = cuesheet->pos < length ? cuesheet->pos : length;
            }
            cuesheet->pos = 0;
            cuesheet->state = MINIFLAC_CUESHEET_LEADIN;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_leadin(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint64_t* leadin) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint64_t t = 0;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: {
            r = miniflac_cuesheet_read_catalog_string(cuesheet,br, NULL, 0, NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: {
            if(miniflac_bitreader_fill_nocrc(br,64)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,64);
            if(leadin != NULL) {
                *leadin = t;
            }
            cuesheet->state = MINIFLAC_CUESHEET_CDFLAG;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_cd_flag(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* flag) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t f = 0;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: {
            r = miniflac_cuesheet_read_leadin(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: {
            if(miniflac_bitreader_fill_nocrc(br,1)) return MINIFLAC_CONTINUE;
            f = (uint8_t)miniflac_bitreader_read(br,1);
            if(flag != NULL) {
                *flag = f;
            }
            miniflac_bitreader_discard(br,7);
            cuesheet->state = MINIFLAC_CUESHEET_SHEET_RESERVE;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_tracks(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* tracks) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: {
            r = miniflac_cuesheet_read_cd_flag(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: {
            while(cuesheet->pos < 258) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                miniflac_bitreader_discard(br,8);
                cuesheet->pos++;
            }
            cuesheet->pos = 0;
            cuesheet->state = MINIFLAC_CUESHEET_TRACKS;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            cuesheet->tracks = (uint8_t)miniflac_bitreader_read(br,8);
            if(tracks != NULL) {
                *tracks = cuesheet->tracks;
            }
            cuesheet->track = 0;
            cuesheet->state = MINIFLAC_CUESHEET_TRACKOFFSET;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_offset(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint64_t* track_offset) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint64_t t = 0;
    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: {
            r = miniflac_cuesheet_read_tracks(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: {
            case_miniflac_cuesheet_trackoffset:
            if(cuesheet->track == cuesheet->tracks) return MINIFLAC_METADATA_END;
            if(miniflac_bitreader_fill_nocrc(br,64)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,64);
            if(track_offset != NULL) {
                *track_offset = t;
            }
            cuesheet->state = MINIFLAC_CUESHEET_TRACKNUMBER;
            return MINIFLAC_OK;
        }
        case MINIFLAC_CUESHEET_INDEX_OFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_NUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_RESERVE: {
            r = miniflac_cuesheet_read_index_point_offset(cuesheet,br,NULL);
            if(r != MINIFLAC_METADATA_END) return r;
            /* cuesheet state was changed to TRACKOFFSET for us */
            goto case_miniflac_cuesheet_trackoffset;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_number(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_number) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t t = 0;
    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: {
            r = miniflac_cuesheet_read_track_offset(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            t = (uint8_t)miniflac_bitreader_read(br,8);
            if(track_number != NULL) {
                *track_number = t;
            }
            cuesheet->pos = 0;
            cuesheet->state = MINIFLAC_CUESHEET_TRACKISRC;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_isrc_length(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint32_t* isrc_length) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: {
            r = miniflac_cuesheet_read_track_number(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: {
            if(isrc_length != NULL) {
                *isrc_length = 12;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_isrc_string(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, char* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    char c;
    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: {
            r = miniflac_cuesheet_read_track_number(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: {
            while(cuesheet->pos < 12) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                c = (char)miniflac_bitreader_read(br,8);
                if(output != NULL && cuesheet->pos < length) {
                    output[cuesheet->pos] = c;
                }
                cuesheet->pos++;
            }
            if(outlen != NULL) {
                *outlen = cuesheet->pos < length ? cuesheet->pos : length;
            }
            cuesheet->pos = 0;
            cuesheet->state = MINIFLAC_CUESHEET_TRACKTYPE;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_audio_flag(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_audio_flag) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t f = 0;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: {
            r = miniflac_cuesheet_read_track_isrc_string(cuesheet,br,NULL,0,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKTYPE: {
            if(miniflac_bitreader_fill_nocrc(br,1)) return MINIFLAC_CONTINUE;
            f = (uint8_t)miniflac_bitreader_read(br,1);
            if(track_audio_flag != NULL) {
                *track_audio_flag = f;
            }
            cuesheet->state = MINIFLAC_CUESHEET_TRACKPREEMPH;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_preemph_flag(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_preemph_flag) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t f = 0;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKTYPE: {
            r = miniflac_cuesheet_read_track_audio_flag(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPREEMPH: {
            if(miniflac_bitreader_fill_nocrc(br,1)) return MINIFLAC_CONTINUE;
            f = (uint8_t)miniflac_bitreader_read(br,1);
            if(track_preemph_flag != NULL) {
                *track_preemph_flag = f;
            }
            miniflac_bitreader_discard(br,6);
            cuesheet->pos = 0;
            cuesheet->state = MINIFLAC_CUESHEET_TRACK_RESERVE;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_track_indexpoints(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* track_indexpoints) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_INDEX_OFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_NUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_RESERVE: {
            /* finish reading indexpoints */
            while(cuesheet->state != MINIFLAC_CUESHEET_TRACKOFFSET) {
                r = miniflac_cuesheet_read_index_point_offset(cuesheet,br,NULL);
                if(r != MINIFLAC_OK) break;
            }
            if(r != MINIFLAC_METADATA_END) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKTYPE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPREEMPH: {
            r = miniflac_cuesheet_read_track_preemph_flag(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACK_RESERVE: {
            while(cuesheet->pos < 13) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                miniflac_bitreader_discard(br,8);
                cuesheet->pos++;
            }
            cuesheet->state = MINIFLAC_CUESHEET_TRACKPOINTS;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPOINTS: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            cuesheet->points = (uint8_t)miniflac_bitreader_read(br,8);
            if(track_indexpoints != NULL) {
                *track_indexpoints = cuesheet->points;
            }
            cuesheet->point = 0;
            cuesheet->state = MINIFLAC_CUESHEET_INDEX_OFFSET;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_index_point_offset(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint64_t* index_point_offset) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint64_t t = 0;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_INDEX_NUMBER: {
            r = miniflac_cuesheet_read_index_point_number(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_RESERVE: {
            while(cuesheet->pos < 3) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                miniflac_bitreader_discard(br,8);
                cuesheet->pos++;
            }
            cuesheet->point++;
            cuesheet->state = MINIFLAC_CUESHEET_INDEX_OFFSET;
            goto case_miniflac_cuesheet_index_offset;
        }
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKTYPE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPREEMPH: /* fall-through */
        case MINIFLAC_CUESHEET_TRACK_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPOINTS: {
            r = miniflac_cuesheet_read_track_indexpoints(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_OFFSET: {
            case_miniflac_cuesheet_index_offset:
            if(cuesheet->point == cuesheet->points) {
                /* done reading track */
                cuesheet->track++;
                cuesheet->state = MINIFLAC_CUESHEET_TRACKOFFSET;
                return MINIFLAC_METADATA_END;
            }
            if(miniflac_bitreader_fill_nocrc(br,64)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,64);
            if(index_point_offset != NULL) {
                *index_point_offset = t;
            }
            cuesheet->state = MINIFLAC_CUESHEET_INDEX_NUMBER;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_cuesheet_read_index_point_number(miniflac_cuesheet_t* cuesheet, miniflac_bitreader_t* br, uint8_t* index_point_number) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t t = 0;

    switch(cuesheet->state) {
        case MINIFLAC_CUESHEET_CATALOG: /* fall-through */
        case MINIFLAC_CUESHEET_LEADIN: /* fall-through */
        case MINIFLAC_CUESHEET_CDFLAG: /* fall-through */
        case MINIFLAC_CUESHEET_SHEET_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKS: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKOFFSET: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKNUMBER: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKISRC: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKTYPE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPREEMPH: /* fall-through */
        case MINIFLAC_CUESHEET_TRACK_RESERVE: /* fall-through */
        case MINIFLAC_CUESHEET_TRACKPOINTS:
        case MINIFLAC_CUESHEET_INDEX_OFFSET: {
            r = miniflac_cuesheet_read_index_point_offset(cuesheet,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_CUESHEET_INDEX_NUMBER: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,8);
            if(index_point_number != NULL) {
                *index_point_number = t;
            }
            cuesheet->pos = 0;
            cuesheet->state = MINIFLAC_CUESHEET_INDEX_RESERVE;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_seektable_init(miniflac_seektable_t* seektable) {
    seektable->state = MINIFLAC_SEEKTABLE_SAMPLE_NUMBER;
    seektable->len = 0;
    seektable->pos = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_seekpoints(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint32_t* seekpoints) {
    (void)br;

    switch(seektable->state) {
        case MINIFLAC_SEEKTABLE_SAMPLE_NUMBER: {
            if(seekpoints != NULL) {
                *seekpoints = seektable->len;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_sample_number(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint64_t* sample_number) {
    uint64_t t = 0;

    switch(seektable->state) {
        case MINIFLAC_SEEKTABLE_SAMPLE_NUMBER: {
            if(seektable->pos == seektable->len) return MINIFLAC_METADATA_END;
            if(miniflac_bitreader_fill_nocrc(br,64)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,64);
            if(sample_number != NULL) {
                *sample_number = t;
            }
            seektable->state = MINIFLAC_SEEKTABLE_SAMPLE_OFFSET;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_sample_offset(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint64_t* sample_offset) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint64_t t = 0;

    switch(seektable->state) {
        case MINIFLAC_SEEKTABLE_SAMPLE_NUMBER: {
            r = miniflac_seektable_read_sample_number(seektable,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_SEEKTABLE_SAMPLE_OFFSET: {
            if(miniflac_bitreader_fill_nocrc(br,64)) return MINIFLAC_CONTINUE;
            t = miniflac_bitreader_read(br,64);
            if(sample_offset != NULL) {
                *sample_offset = t;
            }
            seektable->state = MINIFLAC_SEEKTABLE_SAMPLES;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_seektable_read_samples(miniflac_seektable_t* seektable, miniflac_bitreader_t* br, uint16_t* samples) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint16_t t = 0;

    switch(seektable->state) {
        case MINIFLAC_SEEKTABLE_SAMPLE_NUMBER: /* fall-through */
        case MINIFLAC_SEEKTABLE_SAMPLE_OFFSET: {
            r = miniflac_seektable_read_sample_offset(seektable,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_SEEKTABLE_SAMPLES: {
            if(miniflac_bitreader_fill_nocrc(br,16)) return MINIFLAC_CONTINUE;
            t = (uint16_t)miniflac_bitreader_read(br,16);
            if(samples != NULL) {
                *samples = t;
            }
            seektable->pos++;
            seektable->state = MINIFLAC_SEEKTABLE_SAMPLE_NUMBER;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_application_init(miniflac_application_t* application) {
    application->state = MINIFLAC_APPLICATION_ID;
    application->len = 0;
    application->pos = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_application_read_id(miniflac_application_t* application, miniflac_bitreader_t* br, uint32_t* id) {
    uint32_t t;
    switch(application->state) {
        case MINIFLAC_APPLICATION_ID: {
            if(miniflac_bitreader_fill_nocrc(br,32)) return MINIFLAC_CONTINUE;
            t = (uint32_t)miniflac_bitreader_read(br,32);
            if(id != NULL) {
                *id = t;
            }
            application->state = MINIFLAC_APPLICATION_DATA;
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_application_read_length(miniflac_application_t* application, miniflac_bitreader_t* br, uint32_t* length) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(application->state) {
        case MINIFLAC_APPLICATION_ID: {
            r = miniflac_application_read_id(application,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_APPLICATION_DATA: {
            if(length != NULL) {
                *length = application->len;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_application_read_data(miniflac_application_t* application, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t d;
    switch(application->state) {
        case MINIFLAC_APPLICATION_ID: {
            r = miniflac_application_read_id(application,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_APPLICATION_DATA: {
            while(application->pos < application->len) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                d = (uint8_t) miniflac_bitreader_read(br,8);
                if(output != NULL && application->pos < length) {
                    output[application->pos] = d;
                }
                application->pos++;
            }
            if(outlen != NULL) {
                *outlen = application->len <= length ? application->len : length;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }
    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_padding_init(miniflac_padding_t* padding) {
    padding->len = 0;
    padding->pos = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_padding_read_length(miniflac_padding_t* padding, miniflac_bitreader_t* br, uint32_t* length) {
    (void)padding;
    (void)br;
    if(length != NULL) {
        *length = padding->len;
    }
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_padding_read_data(miniflac_padding_t* padding, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen) {
    uint8_t d;
    while(padding->pos < padding->len) {
        if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
        d = (uint8_t) miniflac_bitreader_read(br,8);
        if(output != NULL && padding->pos < length) {
            output[padding->pos] = d;
        }
        padding->pos++;
    }
    if(outlen != NULL) {
        *outlen = padding->len <= length ? padding->len : length;
    }
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_metadata_init(miniflac_metadata_t* metadata) {
    metadata->state = MINIFLAC_METADATA_HEADER;
    metadata->pos = 0;
    miniflac_metadata_header_init(&metadata->header);
    miniflac_streaminfo_init(&metadata->streaminfo);
    miniflac_vorbis_comment_init(&metadata->vorbis_comment);
    miniflac_picture_init(&metadata->picture);
    miniflac_seektable_init(&metadata->seektable);
    miniflac_application_init(&metadata->application);
    miniflac_cuesheet_init(&metadata->cuesheet);
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_metadata_sync(miniflac_metadata_t* metadata, miniflac_bitreader_t* br) {
    MINIFLAC_RESULT r;
    assert(metadata->state == MINIFLAC_METADATA_HEADER);
    r = miniflac_metadata_header_decode(&metadata->header,br);
    if(r != MINIFLAC_OK) return r;

    switch(metadata->header.type) {
        case MINIFLAC_METADATA_STREAMINFO: {
            miniflac_streaminfo_init(&metadata->streaminfo);
            break;
        }
        case MINIFLAC_METADATA_VORBIS_COMMENT: {
            miniflac_vorbis_comment_init(&metadata->vorbis_comment);
            break;
        }
        case MINIFLAC_METADATA_PICTURE: {
            miniflac_picture_init(&metadata->picture);
            break;
        }
        case MINIFLAC_METADATA_CUESHEET: {
            miniflac_cuesheet_init(&metadata->cuesheet);
            break;
        }
        case MINIFLAC_METADATA_SEEKTABLE: {
            miniflac_seektable_init(&metadata->seektable);
            metadata->seektable.len = metadata->header.length / 18;
            break;
        }
        case MINIFLAC_METADATA_APPLICATION: {
            miniflac_application_init(&metadata->application);
            metadata->application.len = metadata->header.length - 4;
            break;
        }
        case MINIFLAC_METADATA_PADDING: {
            miniflac_padding_init(&metadata->padding);
            metadata->padding.len = metadata->header.length;
            break;
        }
        default: break;
    }

    metadata->state = MINIFLAC_METADATA_DATA;
    metadata->pos = 0;
    return MINIFLAC_OK;
}

static
MINIFLAC_RESULT
miniflac_metadata_skip(miniflac_metadata_t* metadata, miniflac_bitreader_t* br) {
    while(metadata->pos < metadata->header.length) {
        if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
        miniflac_bitreader_discard(br,8);
        metadata->pos++;
    }
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_metadata_decode(miniflac_metadata_t* metadata, miniflac_bitreader_t* br) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(metadata->state) {
        case MINIFLAC_METADATA_HEADER: {
            r = miniflac_metadata_sync(metadata,br);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_METADATA_DATA: {
            switch(metadata->header.type) {
                case MINIFLAC_METADATA_STREAMINFO: {
                    r = miniflac_streaminfo_read_md5_data(&metadata->streaminfo,br,NULL,0,NULL);
                    break;
                }
                case MINIFLAC_METADATA_VORBIS_COMMENT: {
                    do {
                        r = miniflac_vorbis_comment_read_length(&metadata->vorbis_comment,br,NULL);
                    } while(r == MINIFLAC_OK);
                    break;
                }
                case MINIFLAC_METADATA_PICTURE: {
                    r = miniflac_picture_read_data(&metadata->picture,br,NULL,0,NULL);
                    break;
                }
                case MINIFLAC_METADATA_CUESHEET: {
                    do {
                      r = miniflac_cuesheet_read_track_indexpoints(&metadata->cuesheet,br,NULL);
                    } while(r == MINIFLAC_OK);
                    break;
                }
                case MINIFLAC_METADATA_SEEKTABLE: {
                    do {
                      r = miniflac_seektable_read_samples(&metadata->seektable,br,NULL);
                    } while(r == MINIFLAC_OK);
                    break;
                }
                case MINIFLAC_METADATA_APPLICATION: {
                    r = miniflac_application_read_data(&metadata->application,br,NULL,0,NULL);
                    break;
                }
                case MINIFLAC_METADATA_PADDING: {
                    r = miniflac_padding_read_data(&metadata->padding,br,NULL,0,NULL);
                    break;
                }
                default: {
                    r = miniflac_metadata_skip(metadata,br);
                }
            }
        }
        /* fall-through */
        default: break;
    }

    if(r == MINIFLAC_METADATA_END) {
        r = MINIFLAC_OK;
    }

    if(r != MINIFLAC_OK) return r;

    assert(br->bits == 0);
    br->crc8  = 0;
    br->crc16 = 0;
    metadata->state = MINIFLAC_METADATA_HEADER;
    metadata->pos   = 0;
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_metadata_header_init(miniflac_metadata_header_t* header) {
    header->state = MINIFLAC_METADATA_LAST_FLAG;
    header->type = MINIFLAC_METADATA_UNKNOWN;
    header->is_last = 0;
    header->type_raw = 0;
    header->length = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_metadata_header_decode(miniflac_metadata_header_t* header, miniflac_bitreader_t* br) {
    switch(header->state) {
        case MINIFLAC_METADATA_LAST_FLAG: {
            if(miniflac_bitreader_fill(br,1)) return MINIFLAC_CONTINUE;
            miniflac_metadata_header_init(header);
            header->is_last = (uint8_t)miniflac_bitreader_read(br,1);
            header->state = MINIFLAC_METADATA_BLOCK_TYPE;
        }
        /* fall-through */
        case MINIFLAC_METADATA_BLOCK_TYPE: {
            if(miniflac_bitreader_fill(br,7)) return MINIFLAC_CONTINUE;
            header->type_raw = (uint8_t)miniflac_bitreader_read(br,7);
            switch(header->type_raw) {
                case 0: header->type = MINIFLAC_METADATA_STREAMINFO; break;
                case 1: header->type = MINIFLAC_METADATA_PADDING; break;
                case 2: header->type = MINIFLAC_METADATA_APPLICATION; break;
                case 3: header->type = MINIFLAC_METADATA_SEEKTABLE; break;
                case 4: header->type = MINIFLAC_METADATA_VORBIS_COMMENT; break;
                case 5: header->type = MINIFLAC_METADATA_CUESHEET; break;
                case 6: header->type = MINIFLAC_METADATA_PICTURE; break;
                case 127: {
                    header->type = MINIFLAC_METADATA_INVALID;
                    miniflac_abort();
                    return MINIFLAC_METADATA_TYPE_INVALID;
                }
                default: {
                    header->type = MINIFLAC_METADATA_UNKNOWN;
                    miniflac_abort();
                    return MINIFLAC_METADATA_TYPE_RESERVED;
                }
            }
            header->state = MINIFLAC_METADATA_LENGTH;
        }
        /* fall-through */
        case MINIFLAC_METADATA_LENGTH: {
            if(miniflac_bitreader_fill(br,24)) return MINIFLAC_CONTINUE;
            header->length = (uint32_t) miniflac_bitreader_read(br,24);
            header->state = MINIFLAC_METADATA_LAST_FLAG;
            break;
        }
        default: break;
    }
    return MINIFLAC_OK;
}

static const uint8_t escape_codes[2] = {
    15,
    31,
};

MINIFLAC_PRIVATE
void
miniflac_residual_init(miniflac_residual_t* residual) {
    residual->coding_method = 0;
    residual->partition_order = 0;
    residual->rice_parameter = 0;
    residual->rice_size = 0;
    residual->msb = 0;
    residual->rice_parameter_size = 0;
    residual->value = 0;
    residual->partition = 0;
    residual->partition_total = 0;
    residual->residual = 0;
    residual->residual_total = 0;
    residual->state = MINIFLAC_RESIDUAL_CODING_METHOD;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_residual_decode(miniflac_residual_t* residual, miniflac_bitreader_t* br, uint32_t* pos, uint32_t block_size, uint8_t predictor_order, int32_t *output) {
    uint64_t temp;
    uint32_t temp_32;

    switch(residual->state) {
        case MINIFLAC_RESIDUAL_CODING_METHOD: {
            if(miniflac_bitreader_fill(br,2)) return MINIFLAC_CONTINUE;
            temp = miniflac_bitreader_read(br,2);
            if(temp > 1) {
                miniflac_abort();
                return MINIFLAC_RESERVED_CODING_METHOD;
            }
            residual->coding_method = temp;
            switch(residual->coding_method) {
                case 0: {
                    residual->rice_parameter_size = 4;
                    break;
                }
                case 1: {
                    residual->rice_parameter_size = 5;
                    break;
                }
            }
            residual->msb = 0;
            residual->state = MINIFLAC_RESIDUAL_PARTITION_ORDER;
        }
        /* fall-through */
        case MINIFLAC_RESIDUAL_PARTITION_ORDER: {
            if(miniflac_bitreader_fill(br,4)) return MINIFLAC_CONTINUE;
            residual->partition_order = miniflac_bitreader_read(br,4);
            residual->partition_total = 1 << residual->partition_order;
            residual->state = MINIFLAC_RESIDUAL_RICE_PARAMETER;
        }
        /* fall-through */
        case MINIFLAC_RESIDUAL_RICE_PARAMETER: {
            miniflac_residual_rice_parameter:
            if(miniflac_bitreader_fill(br,residual->rice_parameter_size)) return MINIFLAC_CONTINUE;
            residual->rice_parameter = miniflac_bitreader_read(br,residual->rice_parameter_size);

            residual->residual = 0;
            residual->residual_total = block_size >> residual->partition_order;
            if(residual->partition == 0) {
                if(residual->residual_total < predictor_order) {
                    miniflac_abort();
                    return MINIFLAC_ERROR;
                }
                residual->residual_total -= predictor_order;
            }

            if(residual->rice_parameter == escape_codes[residual->coding_method]) {
                residual->state = MINIFLAC_RESIDUAL_RICE_SIZE;
                goto miniflac_residual_rice_size;
            }
            residual->state = MINIFLAC_RESIDUAL_MSB;
            goto miniflac_residual_msb;
        }
        /* fall-through */
        case MINIFLAC_RESIDUAL_RICE_SIZE: {
            miniflac_residual_rice_size:
            if(miniflac_bitreader_fill(br,5)) return MINIFLAC_CONTINUE;
            residual->rice_size = miniflac_bitreader_read(br,5);
            residual->state = MINIFLAC_RESIDUAL_RICE_VALUE;
        }
        /* fall-through */
        case MINIFLAC_RESIDUAL_RICE_VALUE: {
            miniflac_residual_rice_value:
            if(miniflac_bitreader_fill(br,residual->rice_size)) return MINIFLAC_CONTINUE;
            residual->value = miniflac_bitreader_read_signed(br,residual->rice_size);
            if(output != NULL) {
                output[*pos] = residual->value;
            }
            *pos += 1;
            residual->residual++;
            if(residual->residual < residual->residual_total) {
                residual->state = MINIFLAC_RESIDUAL_RICE_VALUE;
                goto miniflac_residual_rice_value;
            }
            goto miniflac_residual_nextpart;
        }
        /* fall-through */
        case MINIFLAC_RESIDUAL_MSB: {
            miniflac_residual_msb:
            while(!miniflac_bitreader_fill(br,1)) {
                if(miniflac_bitreader_read(br,1)) {
                    residual->state = MINIFLAC_RESIDUAL_LSB;
                    goto miniflac_residual_lsb;
                }
                residual->msb++;
            }
            return MINIFLAC_CONTINUE;
        }
        case MINIFLAC_RESIDUAL_LSB: {
            miniflac_residual_lsb:
            if(miniflac_bitreader_fill(br,residual->rice_parameter)) return MINIFLAC_CONTINUE;
            temp_32 = (residual->msb << residual->rice_parameter) | ((uint32_t)miniflac_bitreader_read(br,residual->rice_parameter));
            residual->value = (temp_32 >> 1) ^ -(temp_32 & 1);

            if(output != NULL) {
                output[*pos] = residual->value;
            }
            *pos += 1;

            residual->msb = 0;
            residual->residual++;
            if(residual->residual < residual->residual_total) {
                residual->state = MINIFLAC_RESIDUAL_MSB;
                goto miniflac_residual_msb;
            }

            miniflac_residual_nextpart:
            residual->residual = 0;
            residual->partition++;
            if(residual->partition < residual->partition_total) {
                residual->state = MINIFLAC_RESIDUAL_RICE_PARAMETER;
                goto miniflac_residual_rice_parameter;
            }
            break;
        }
        default: break;
    }

    miniflac_residual_init(residual);
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_streaminfo_init(miniflac_streaminfo_t* streaminfo) {
    streaminfo->state = MINIFLAC_STREAMINFO_MINBLOCKSIZE;
    streaminfo->pos = 0;
    streaminfo->sample_rate = 0;
    streaminfo->bps = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_min_block_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint16_t* min_block_size) {
    uint16_t t;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: {
            if(miniflac_bitreader_fill_nocrc(br,16)) return MINIFLAC_CONTINUE;
            t = (uint16_t) miniflac_bitreader_read(br,16);
            if(min_block_size != NULL) {
                *min_block_size = t;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_MAXBLOCKSIZE;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_max_block_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint16_t* max_block_size) {
    uint16_t t;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: {
            r = miniflac_streaminfo_read_min_block_size(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: {
            if(miniflac_bitreader_fill_nocrc(br,16)) return MINIFLAC_CONTINUE;
            t = (uint16_t) miniflac_bitreader_read(br,16);
            if(max_block_size != NULL) {
                *max_block_size = t;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_MINFRAMESIZE;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}


MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_min_frame_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* min_frame_size) {
    uint32_t t;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: {
            r = miniflac_streaminfo_read_max_block_size(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: {
            if(miniflac_bitreader_fill_nocrc(br,24)) return MINIFLAC_CONTINUE;
            t = (uint32_t) miniflac_bitreader_read(br,24);
            if(min_frame_size != NULL) {
                *min_frame_size = t;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_MAXFRAMESIZE;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_max_frame_size(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* max_frame_size) {
    uint32_t t;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: {
            r = miniflac_streaminfo_read_min_frame_size(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: {
            if(miniflac_bitreader_fill_nocrc(br,24)) return MINIFLAC_CONTINUE;
            t = (uint32_t) miniflac_bitreader_read(br,24);
            if(max_frame_size != NULL) {
                *max_frame_size = t;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_SAMPLERATE;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_sample_rate(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* sample_rate) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: {
            r = miniflac_streaminfo_read_max_frame_size(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_SAMPLERATE: {
            if(miniflac_bitreader_fill_nocrc(br,20)) return MINIFLAC_CONTINUE;
            streaminfo->sample_rate = (uint32_t) miniflac_bitreader_read(br,20);
            if(sample_rate != NULL) {
                *sample_rate = streaminfo->sample_rate;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_CHANNELS;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_channels(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint8_t* channels) {
    uint8_t t;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_SAMPLERATE: {
            r = miniflac_streaminfo_read_sample_rate(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_CHANNELS: {
            if(miniflac_bitreader_fill_nocrc(br,3)) return MINIFLAC_CONTINUE;
            t = (uint8_t) miniflac_bitreader_read(br,3) + 1;
            if(channels != NULL) {
                *channels = t;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_BPS;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_bps(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint8_t* bps) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_SAMPLERATE: /* fall-through */
        case MINIFLAC_STREAMINFO_CHANNELS: {
            r = miniflac_streaminfo_read_channels(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_BPS: {
            if(miniflac_bitreader_fill_nocrc(br,5)) return MINIFLAC_CONTINUE;
            streaminfo->bps = (uint8_t) miniflac_bitreader_read(br,5) + 1;
            if(bps != NULL) {
                *bps = streaminfo->bps;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_TOTALSAMPLES;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_total_samples(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint64_t* total_samples) {
    uint64_t t;
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_SAMPLERATE: /* fall-through */
        case MINIFLAC_STREAMINFO_CHANNELS: /* fall-through */
        case MINIFLAC_STREAMINFO_BPS: {
            r = miniflac_streaminfo_read_bps(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_TOTALSAMPLES: {
            if(miniflac_bitreader_fill_nocrc(br,36)) return MINIFLAC_CONTINUE;
            t = (uint64_t) miniflac_bitreader_read(br,36);
            if(total_samples != NULL) {
                *total_samples = t;
            }
            streaminfo->state = MINIFLAC_STREAMINFO_MD5;
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_md5_length(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint32_t* md5_len) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_SAMPLERATE: /* fall-through */
        case MINIFLAC_STREAMINFO_CHANNELS: /* fall-through */
        case MINIFLAC_STREAMINFO_BPS: /* fall-through */
        case MINIFLAC_STREAMINFO_TOTALSAMPLES: {
            r = miniflac_streaminfo_read_total_samples(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        default: break;
    }
    if(md5_len != NULL) {
        *md5_len = 16;
    }
    return MINIFLAC_OK;
}


MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streaminfo_read_md5_data(miniflac_streaminfo_t* streaminfo, miniflac_bitreader_t* br, uint8_t* output, uint32_t length, uint32_t* outlen) {
    MINIFLAC_RESULT r = MINIFLAC_ERROR;
    uint8_t t;
    switch(streaminfo->state) {
        case MINIFLAC_STREAMINFO_MINBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXBLOCKSIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MINFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_MAXFRAMESIZE: /* fall-through */
        case MINIFLAC_STREAMINFO_SAMPLERATE: /* fall-through */
        case MINIFLAC_STREAMINFO_CHANNELS: /* fall-through */
        case MINIFLAC_STREAMINFO_BPS: /* fall-through */
        case MINIFLAC_STREAMINFO_TOTALSAMPLES: {
            r = miniflac_streaminfo_read_total_samples(streaminfo,br,NULL);
            if(r != MINIFLAC_OK) return r;
        }
        /* fall-through */
        case MINIFLAC_STREAMINFO_MD5: {
            if(streaminfo->pos == 16) return MINIFLAC_METADATA_END;
            while(streaminfo->pos < 16) {
                if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
                t = (uint8_t)miniflac_bitreader_read(br,8);
                if(output != NULL && streaminfo->pos < length) {
                    output[streaminfo->pos] = t;
                }
                streaminfo->pos++;
            }
            if(outlen != NULL) {
                *outlen = 16 < length ? 16 : length;
            }
            return MINIFLAC_OK;
        }
        default: break;
    }

    miniflac_abort();
    return MINIFLAC_ERROR;
}

MINIFLAC_PRIVATE
void
miniflac_streammarker_init(miniflac_streammarker_t* streammarker) {
    streammarker->state = MINIFLAC_STREAMMARKER_F;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_streammarker_decode(miniflac_streammarker_t* streammarker, miniflac_bitreader_t* br) {
    char t;
    switch(streammarker->state) {
        case MINIFLAC_STREAMMARKER_F: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            t = (char)miniflac_bitreader_read(br,8);
            if(t != 'f') {
                miniflac_abort();
                return MINIFLAC_STREAMMARKER_INVALID;
            }
            streammarker->state = MINIFLAC_STREAMMARKER_L;
        }
        /* fall-through */
        case MINIFLAC_STREAMMARKER_L: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            t = (char)miniflac_bitreader_read(br,8);
            if(t != 'L') {
                miniflac_abort();
                return MINIFLAC_STREAMMARKER_INVALID;
            }
            streammarker->state = MINIFLAC_STREAMMARKER_A;
        }
        /* fall-through */
        case MINIFLAC_STREAMMARKER_A: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            t = (char)miniflac_bitreader_read(br,8);
            if(t != 'a') {
                miniflac_abort();
                return MINIFLAC_STREAMMARKER_INVALID;
            }
            streammarker->state = MINIFLAC_STREAMMARKER_C;
        }
        /* fall-through */
        case MINIFLAC_STREAMMARKER_C: {
            if(miniflac_bitreader_fill_nocrc(br,8)) return MINIFLAC_CONTINUE;
            t = (char)miniflac_bitreader_read(br,8);
            if(t != 'C') {
                miniflac_abort();
                return MINIFLAC_STREAMMARKER_INVALID;
            }
            break;
        }
        default: {
            miniflac_abort();
            return MINIFLAC_ERROR;
        }
    }

    miniflac_streammarker_init(streammarker);

    assert(br->bits == 0);
    br->crc8  = 0;
    br->crc16 = 0;

    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_subframe_init(miniflac_subframe_t* subframe) {
    subframe->bps = 0;
    subframe->state = MINIFLAC_SUBFRAME_HEADER;
    miniflac_subframe_header_init(&subframe->header);
    miniflac_subframe_constant_init(&subframe->constant);
    miniflac_subframe_verbatim_init(&subframe->verbatim);
    miniflac_subframe_fixed_init(&subframe->fixed);
    miniflac_subframe_lpc_init(&subframe->lpc);
    miniflac_residual_init(&subframe->residual);
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_decode(miniflac_subframe_t* subframe, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps) {
    MINIFLAC_RESULT r;
    uint32_t i;

    switch(subframe->state) {
        case MINIFLAC_SUBFRAME_HEADER: {
            r = miniflac_subframe_header_decode(&subframe->header,br);
            if(r != MINIFLAC_OK) return r;

            if(subframe->header.wasted_bits >= bps) {
                miniflac_abort();
                return MINIFLAC_ERROR;
            }
            subframe->bps = bps - subframe->header.wasted_bits;

            switch(subframe->header.type) {
                case MINIFLAC_SUBFRAME_TYPE_CONSTANT: {
                    miniflac_subframe_constant_init(&subframe->constant);
                    subframe->state = MINIFLAC_SUBFRAME_CONSTANT;
                    goto miniflac_subframe_constant;
                }
                case MINIFLAC_SUBFRAME_TYPE_VERBATIM: {
                    miniflac_subframe_verbatim_init(&subframe->verbatim);
                    subframe->state = MINIFLAC_SUBFRAME_VERBATIM;
                    goto miniflac_subframe_verbatim;
                }
                case MINIFLAC_SUBFRAME_TYPE_FIXED: {
                    miniflac_residual_init(&subframe->residual);
                    miniflac_subframe_fixed_init(&subframe->fixed);
                    subframe->state = MINIFLAC_SUBFRAME_FIXED;
                    goto miniflac_subframe_fixed;
                }
                case MINIFLAC_SUBFRAME_TYPE_LPC: {
                    miniflac_residual_init(&subframe->residual);
                    miniflac_subframe_lpc_init(&subframe->lpc);
                    subframe->state = MINIFLAC_SUBFRAME_LPC;
                    goto miniflac_subframe_lpc;
                }
                default: {
                    miniflac_abort();
                    return MINIFLAC_ERROR;
                }
            }
            break;
        }

        case MINIFLAC_SUBFRAME_CONSTANT: {
            miniflac_subframe_constant:
            r = miniflac_subframe_constant_decode(&subframe->constant,br,output,block_size,subframe->bps);
            if(r != MINIFLAC_OK) return r;
            break;
        }
        case MINIFLAC_SUBFRAME_VERBATIM: {
            miniflac_subframe_verbatim:
            r = miniflac_subframe_verbatim_decode(&subframe->verbatim,br,output,block_size,subframe->bps);
            if(r != MINIFLAC_OK) return r;
            break;
        }
        case MINIFLAC_SUBFRAME_FIXED: {
            miniflac_subframe_fixed:
            r = miniflac_subframe_fixed_decode(&subframe->fixed,br,output,block_size,subframe->bps,&subframe->residual,subframe->header.order);
            if(r != MINIFLAC_OK) return r;
            break;
        }
        case MINIFLAC_SUBFRAME_LPC: {
            miniflac_subframe_lpc:
            r = miniflac_subframe_lpc_decode(&subframe->lpc,br,output,block_size,subframe->bps,&subframe->residual,subframe->header.order);
            if(r != MINIFLAC_OK) return r;
            break;
        }
        default: break;
    }

    if(output != NULL && subframe->header.wasted_bits > 0) {
        for(i=0;i<block_size;i++) {
            output[i] <<= subframe->header.wasted_bits;
        }
    }

    miniflac_subframe_init(subframe);
    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_subframe_constant_init(miniflac_subframe_constant_t* c) {
    c->state = MINIFLAC_SUBFRAME_CONSTANT_DECODE;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_constant_decode(miniflac_subframe_constant_t* c, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps) {
    int32_t sample;
    uint32_t i;
    (void)c;

    if(miniflac_bitreader_fill(br,bps)) return MINIFLAC_CONTINUE;
    sample = (int32_t) miniflac_bitreader_read_signed(br,bps);

    if(output != NULL) {
        for(i=0;i<block_size;i++) {
            output[i] = sample;
        }
    }

    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_subframe_fixed_init(miniflac_subframe_fixed_t* f) {
    f->pos   = 0;
    f->state = MINIFLAC_SUBFRAME_FIXED_DECODE;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_fixed_decode(miniflac_subframe_fixed_t* f, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps, miniflac_residual_t* residual, uint8_t predictor_order) {
    int32_t sample;

    int64_t sample1;
    int64_t sample2;
    int64_t sample3;
    int64_t sample4;
    int64_t current_residual;

    MINIFLAC_RESULT r;
    while(f->pos < predictor_order) {
        if(miniflac_bitreader_fill(br,bps)) return MINIFLAC_CONTINUE;
        sample = (int32_t) miniflac_bitreader_read_signed(br,bps);
        if(output != NULL) {
            output[f->pos] = sample;
        }
        f->pos++;
    }
    r = miniflac_residual_decode(residual,br,&f->pos,block_size,predictor_order,output);
    if(r != MINIFLAC_OK) return r;

    if(output != NULL) {
        switch(predictor_order) {
            case 0:
#if 0
                /* this is here for reference but not actually needed */
                for(f->pos = predictor_order; f->pos < block_size; f->pos++) {
                    current_residual = output[f->pos];
                    output[f->pos] = (int32_t)current_residual;
                }
#endif
                break;
            case 1: {
                for(f->pos = predictor_order; f->pos < block_size; f->pos++) {
                    current_residual = output[f->pos];
                    sample1  = output[f->pos-1];
                    output[f->pos] = (int32_t)(sample1 + current_residual);
                }
                break;
            }
            case 2: {
                for(f->pos = predictor_order; f->pos < block_size; f->pos++) {
                    current_residual = output[f->pos];
                    sample1  = output[f->pos-1];
                    sample2  = output[f->pos-2];

                    sample1 *= 2;

                    output[f->pos] = (int32_t)(sample1 - sample2 + current_residual);
                }
                break;
            }
            case 3: {
                for(f->pos = predictor_order; f->pos < block_size; f->pos++) {
                    current_residual = output[f->pos];
                    sample1  = output[f->pos-1];
                    sample2  = output[f->pos-2];
                    sample3  = output[f->pos-3];

                    sample1 *= 3;
                    sample2 *= 3;

                    output[f->pos] = (int32_t)(sample1 - sample2 + sample3 + current_residual);
                }
                break;
            }
            case 4: {
                for(f->pos = predictor_order; f->pos < block_size; f->pos++) {
                    current_residual = output[f->pos];
                    sample1  = output[f->pos-1];
                    sample2  = output[f->pos-2];
                    sample3  = output[f->pos-3];
                    sample4  = output[f->pos-4];

                    sample1 *= 4;
                    sample2 *= 6;
                    sample3 *= 4;

                    output[f->pos] = (int32_t)(sample1  - sample2 + sample3 - sample4 + current_residual);
                }
                break;
            }
            default: break;
        }
    }

    return MINIFLAC_OK;

}

MINIFLAC_PRIVATE
void
miniflac_subframe_header_init(miniflac_subframe_header_t* subframeheader) {
    subframeheader->state       = MINIFLAC_SUBFRAME_HEADER_RESERVEBIT1;
    subframeheader->type        = MINIFLAC_SUBFRAME_TYPE_UNKNOWN;
    subframeheader->order       = 0;
    subframeheader->wasted_bits = 0;
    subframeheader->type_raw    = 0;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_header_decode(miniflac_subframe_header_t* subframeheader, miniflac_bitreader_t* br) {
    uint64_t t = 0;
    switch(subframeheader->state) {
        case MINIFLAC_SUBFRAME_HEADER_RESERVEBIT1: {
            if(miniflac_bitreader_fill(br,1)) {
                break;
            }
            t = miniflac_bitreader_read(br,1);
            if(t != 0) {
                miniflac_abort();
                return MINIFLAC_SUBFRAME_RESERVED_BIT;
            }
            subframeheader->state = MINIFLAC_SUBFRAME_HEADER_KIND;
        }
        /* fall-through */
        case MINIFLAC_SUBFRAME_HEADER_KIND: {
            if(miniflac_bitreader_fill(br,6)) {
                break;
            }
            t = (uint8_t)miniflac_bitreader_read(br,6);
            subframeheader->type_raw = t;
            if(t == 0) {
                subframeheader->type = MINIFLAC_SUBFRAME_TYPE_CONSTANT;
            } else if(t == 1) {
                subframeheader->type = MINIFLAC_SUBFRAME_TYPE_VERBATIM;
            } else if(t < 8) {
                miniflac_abort();
                return MINIFLAC_SUBFRAME_RESERVED_TYPE;
            } else if(t < 13) {
                subframeheader->type = MINIFLAC_SUBFRAME_TYPE_FIXED;
                subframeheader->order = t - 8;
            } else if(t < 32) {
                miniflac_abort();
                return MINIFLAC_SUBFRAME_RESERVED_TYPE;
            } else {
                subframeheader->type = MINIFLAC_SUBFRAME_TYPE_LPC;
                subframeheader->order = t - 31;
            }
            subframeheader->state = MINIFLAC_SUBFRAME_HEADER_WASTED_BITS;
        }
        /* fall-through */
        case MINIFLAC_SUBFRAME_HEADER_WASTED_BITS: {
            if(miniflac_bitreader_fill(br,1)) {
                break;
            }
            subframeheader->wasted_bits = 0;

            t = miniflac_bitreader_read(br,1);
            if(t == 0) { /* no wasted bits, we're done */
                subframeheader->state = MINIFLAC_SUBFRAME_HEADER_RESERVEBIT1;
                return MINIFLAC_OK;
            }
            subframeheader->state = MINIFLAC_SUBFRAME_HEADER_UNARY;
        }
        /* fall-through */
        case MINIFLAC_SUBFRAME_HEADER_UNARY: {
            while(!miniflac_bitreader_fill(br,1)) {
                subframeheader->wasted_bits++;
                t = miniflac_bitreader_read(br,1);

                if(t == 1) {
                    /* no more wasted bits, we're done */
                    subframeheader->state = MINIFLAC_SUBFRAME_HEADER_RESERVEBIT1;
                    return MINIFLAC_OK;
                }
            }
        }
        /* fall-through */
        default: break;
    }
    return MINIFLAC_CONTINUE;
}

MINIFLAC_PRIVATE
void
miniflac_subframe_lpc_init(miniflac_subframe_lpc_t* l) {
    unsigned int i;
    l->pos   = 0;
    l->precision = 0;
    l->shift = 0;
    l->coeff = 0;
    for(i = 0; i < 32; i++) {
        l->coefficients[i] = 0;
    }
    l->state = MINIFLAC_SUBFRAME_LPC_PRECISION;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_lpc_decode(miniflac_subframe_lpc_t* l, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps, miniflac_residual_t* residual, uint8_t predictor_order) {
    int32_t sample;
    int64_t temp;
    int64_t prediction;
    uint32_t i,j;
    MINIFLAC_RESULT r;

    while(l->pos < predictor_order) {
        if(miniflac_bitreader_fill(br,bps)) return MINIFLAC_CONTINUE;
        sample = (int32_t) miniflac_bitreader_read_signed(br,bps);
        if(output != NULL) {
            output[l->pos] = sample;
        }
        l->pos++;
        l->state = MINIFLAC_SUBFRAME_LPC_PRECISION;
    }

    if(l->state == MINIFLAC_SUBFRAME_LPC_PRECISION) {
        if(miniflac_bitreader_fill(br,4)) return MINIFLAC_CONTINUE;
        l->precision = miniflac_bitreader_read(br,4) + 1;
        l->state = MINIFLAC_SUBFRAME_LPC_SHIFT;
    }

    if(l->state == MINIFLAC_SUBFRAME_LPC_SHIFT) {
        if(miniflac_bitreader_fill(br,5)) return MINIFLAC_CONTINUE;
        temp = miniflac_bitreader_read_signed(br,5);
        if(temp < 0) temp = 0;
        l->shift = temp;
        l->state = MINIFLAC_SUBFRAME_LPC_COEFF;
    }

    if(l->state == MINIFLAC_SUBFRAME_LPC_COEFF) {
        while(l->coeff < predictor_order) {
            if(miniflac_bitreader_fill(br,l->precision)) return MINIFLAC_CONTINUE;
            sample = (int32_t) miniflac_bitreader_read_signed(br,l->precision);
            l->coefficients[l->coeff++] = sample;
        }
    }

    r = miniflac_residual_decode(residual,br,&l->pos,block_size,predictor_order,output);
    if(r != MINIFLAC_OK) return r;

    if(output != NULL) {
        for(i=predictor_order;i<block_size;i++) {
            prediction = 0;
            for(j=0;j<predictor_order;j++) {
                temp = output[i - j - 1];
                temp *= l->coefficients[j];
                prediction += temp;
            }
            prediction >>= l->shift;
            prediction += output[i];
            output[i] = prediction;
        }
    }

    return MINIFLAC_OK;
}

MINIFLAC_PRIVATE
void
miniflac_subframe_verbatim_init(miniflac_subframe_verbatim_t* c) {
    c->pos   = 0;
    c->state = MINIFLAC_SUBFRAME_VERBATIM_DECODE;
}

MINIFLAC_PRIVATE
MINIFLAC_RESULT
miniflac_subframe_verbatim_decode(miniflac_subframe_verbatim_t* c, miniflac_bitreader_t* br, int32_t* output, uint32_t block_size, uint8_t bps) {
    int32_t sample;

    while(c->pos < block_size) {
        if(miniflac_bitreader_fill(br,bps)) return MINIFLAC_CONTINUE;
        sample = (int32_t) miniflac_bitreader_read_signed(br,bps);
        if(output != NULL) {
            output[c->pos] = sample;
        }
        c->pos++;
    }

    c->pos = 0;
    return MINIFLAC_OK;

}

#endif
