/* FLAC sample loader
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#ifdef HAS_LIBFLAC

// hide POSIX warning for fileno()
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"
#include "../libflac/FLAC/stream_decoder.h"

static bool sample16Bit;
static int16_t stereoSampleLoadMode = -1;
static uint32_t numChannels, bitDepth, sampleLength, sampleRate, samplesRead;
static sample_t *s;

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

bool loadFLAC(FILE *f, uint32_t filesize)
{
	s = &tmpSmp;

	s->volume = 64;
	s->panning = 128;

	numChannels = 0;
	bitDepth = 0;
	sampleLength = 0;
	sampleRate = 0;
	samplesRead = 0;

	FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
	if (decoder == NULL)
	{
		loaderMsgBox("Error loading sample: Unable to allocate FLAC decoder!");
		goto error;
	}

	FLAC__stream_decoder_set_metadata_respond_all(decoder);

	FLAC__StreamDecoderInitStatus initStatus =
		FLAC__stream_decoder_init_stream
		(
			decoder,
			read_callback, seek_callback,
			tell_callback, length_callback,
			eof_callback, write_callback,
			metadata_callback, error_callback,
			f
		);

	if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		loaderMsgBox("Error loading sample: Unable to initialize FLAC decoder!");
		goto error;
	}

	if (!FLAC__stream_decoder_process_until_end_of_stream(decoder))
	{
		loaderMsgBox("Error loading sample: Unable to decode FLAC!");
		goto error;
	}

	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);

	tuneSample(s, sampleRate, audio.linearPeriodsFlag);

	return true;

error:
	if (decoder != NULL) FLAC__stream_decoder_delete(decoder);

	return false;

	(void)filesize;
}

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	FILE *file = (FILE *)client_data;
	if (*bytes > 0)
	{
		*bytes = fread(buffer, sizeof (FLAC__byte), *bytes, file);
		if (ferror(file))
			return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		else if (*bytes == 0)
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		else
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}
	else
	{
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	(void)decoder;
}

static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	FILE *file = (FILE *)client_data;

	if (absolute_byte_offset > INT32_MAX)
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

	if (fseek(file, (int32_t)absolute_byte_offset, SEEK_SET) < 0)
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	else
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;

	(void)decoder;
}

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	FILE *file = (FILE *)client_data;
	int32_t pos = ftell(file);

	if (pos < 0)
	{
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	}
	else
	{
		*absolute_byte_offset = (FLAC__uint64)pos;
		return FLAC__STREAM_DECODER_TELL_STATUS_OK;
	}

	(void)decoder;
}

static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	FILE *file = (FILE *)client_data;
	struct stat filestats;

	if (fstat(fileno(file), &filestats) != 0)
	{
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}
	else
	{
		*stream_length = (FLAC__uint64)filestats.st_size;
		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}

	(void)decoder;
}

static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
	FILE *file = (FILE *)client_data;
	return feof(file) ? true : false;

	(void)decoder;
}

static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && metadata->data.stream_info.total_samples != 0)
	{
		bitDepth = metadata->data.stream_info.bits_per_sample;
		numChannels = metadata->data.stream_info.channels;
		sampleRate = metadata->data.stream_info.sample_rate;

		sample16Bit = (bitDepth != 8);

		int64_t tmp64 = metadata->data.stream_info.total_samples;
		if (tmp64 > MAX_SAMPLE_LEN)
			tmp64 = MAX_SAMPLE_LEN;

		sampleLength = (uint32_t)tmp64;

		s->length = sampleLength;
		if (sample16Bit)
			s->flags |= SAMPLE_16BIT;

		stereoSampleLoadMode = -1;
		if (numChannels == 2)
			stereoSampleLoadMode = loaderSysReq(5, "System request", "This is a stereo sample...");
	}

	// check for RIFF chunks (loop/vol/pan information)
	else if (metadata->type == FLAC__METADATA_TYPE_APPLICATION && !memcmp(metadata->data.application.id, "riff", 4))
	{
		const uint8_t *data = (const uint8_t *)metadata->data.application.data;

		uint32_t chunkID  = *(uint32_t *)data; data += 4;
		uint32_t chunkLen = *(uint32_t *)data; data += 4;

		if (chunkID == 0x61727478 && chunkLen >= 8) // "xtra"
		{
			uint32_t xtraFlags = *(uint32_t *)data; data += 4;

			// panning (0..256)
			if (xtraFlags & 0x20) // set panning flag
			{
				uint16_t tmpPan = *(uint16_t *)data;
				if (tmpPan > 255)
					tmpPan = 255;

				s->panning = (uint8_t)tmpPan;
			}
			data += 2;

			// volume (0..256)
			uint16_t tmpVol = *(uint16_t *)data;
			if (tmpVol > 256)
				tmpVol = 256;

			s->volume = (uint8_t)((tmpVol + 2) / 4); // 0..256 -> 0..64 (rounded)
		}

		if (chunkID == 0x6C706D73 && chunkLen > 52) // "smpl"
		{
			data += 28; // seek to first wanted byte

			uint32_t numLoops = *(uint32_t *)data; data += 4;
			if (numLoops == 1)
			{
				data += 4+4; // skip "samplerData" and "identifier"

				uint32_t loopType  = *(uint32_t *)data; data += 4;
				uint32_t loopStart = *(uint32_t *)data; data += 4;
				uint32_t loopEnd   = *(uint32_t *)data; data += 4;

				s->loopStart = loopStart;
				s->loopLength = (loopEnd+1) - loopStart;
				s->flags |= (loopType == 0) ? LOOP_FWD : LOOP_BIDI;
			}
		}
	}

	else if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
	{
		uint32_t tmpSampleRate = 0, loopStart = 0, loopLength = 0;
		for (uint32_t i = 0; i < metadata->data.vorbis_comment.num_comments; i++)
		{
			const char *tag = (const char *)metadata->data.vorbis_comment.comments[i].entry;
			uint32_t length = metadata->data.vorbis_comment.comments[i].length;

			if (length > 6 && !memcmp(tag, "TITLE=", 6))
			{
				length -= 6;
				if (length > 22)
					length = 22;

				memcpy(s->name, &tag[6], length);
				s->name[22] = '\0';

				smpFilenameSet = true;
			}

			// the following tags haven't been tested!
			else if (length > 11 && !memcmp(tag, "SAMPLERATE=", 11))
			{
				tmpSampleRate = atoi(&tag[11]);
			}
			else if (length > 10 && !memcmp(tag, "LOOPSTART=", 10))
			{
				loopLength = atoi(&tag[10]);
			}
			else if (length > 11 && !memcmp(tag, "LOOPLENGTH=", 11))
			{
				loopLength = atoi(&tag[11]);
			}

			if (loopLength > 0)
			{
				s->loopStart = loopLength;
				s->loopLength = loopStart;

				DISABLE_LOOP(s->flags);
				s->flags |= LOOP_FWD;
			}

			if (tmpSampleRate > 0)
				sampleRate = tmpSampleRate;
		}
	}

	(void)client_data;
	(void)decoder;
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	if (sampleLength == 0 || numChannels == 0)
	{
		loaderMsgBox("Error loading sample: The sample is empty or corrupt!");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (numChannels > 2)
	{
		loaderMsgBox("Error loading sample: Only mono/stereo FLACs are supported!");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24)
	{
		loaderMsgBox("Error loading sample: Only FLACs with a bitdepth of 8/16/24 are supported!");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (frame->header.number.sample_number == 0)
	{
		if (!allocateSmpData(s, sampleLength, sample16Bit))
		{
			loaderMsgBox("Error loading sample: Out of memory!");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}

		samplesRead = 0;
	}

	uint32_t blockSize = frame->header.blocksize;

	const uint32_t samplesAllocated = sampleLength;
	if (samplesRead+blockSize > samplesAllocated)
		blockSize = samplesAllocated-samplesRead;

	if (stereoSampleLoadMode == STEREO_SAMPLE_CONVERT) // mix to mono
	{
		const int32_t *src32_L = buffer[0];
		const int32_t *src32_R = buffer[1];

		switch (bitDepth)
		{
			case 8:
			{
				int8_t *dst8 = s->dataPtr + samplesRead;
				for (uint32_t i = 0; i < blockSize; i++)
					dst8[i] = (int8_t)((src32_L[i] + src32_R[i]) >> 1);
			}
			break;

			case 16:
			{
				int16_t *dst16 = (int16_t *)s->dataPtr + samplesRead;
				for (uint32_t i = 0; i < blockSize; i++)
					dst16[i] = (int16_t)((src32_L[i] + src32_R[i]) >> 1);
			}
			break;

			case 24:
			{
				int16_t *dst16 = (int16_t *)s->dataPtr + samplesRead;
				for (uint32_t i = 0; i < blockSize; i++)
					dst16[i] = (int16_t)((src32_L[i] + src32_R[i]) >> (16+1));
			}
			break;

			default: break;
		}
	}
	else // mono sample
	{
		const int32_t *src32 = (stereoSampleLoadMode == STEREO_SAMPLE_READ_RIGHT) ? buffer[1] : buffer[0];

		switch (bitDepth)
		{
			case 8:
			{
				int8_t *dst8 = s->dataPtr + samplesRead;
				for (uint32_t i = 0; i < blockSize; i++)
					dst8[i] = (int8_t)src32[i];
			}
			break;

			case 16:
			{
				int16_t *dst16 = (int16_t *)s->dataPtr + samplesRead;
				for (uint32_t i = 0; i < blockSize; i++)
					dst16[i] = (int16_t)src32[i];
			}
			break;

			case 24:
			{
				int16_t *dst16 = (int16_t *)s->dataPtr + samplesRead;
				for (uint32_t i = 0; i < blockSize; i++)
					dst16[i] = (int16_t)(src32[i] >> 8);
			}
			break;

			default: break;
		}
	}

	samplesRead += blockSize;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	(void)client_data;
	(void)decoder;
}

static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)status;
	(void)decoder;
	(void)client_data;
}

#endif
