/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * file_output.cpp - Write output to file.
 */

#include <ctime>
#include <string>

#include "file_output.hpp"

// Apply strftime-style datetime substitution for known time specifiers (%Y, %m,
// %d, %H, %M, %S, %F, %T), leaving all other % sequences (e.g. %04d for the
// segment counter) intact so that snprintf can process them afterwards.
static std::string applyDatetimeToTemplate(const std::string &tmpl)
{
	time_t now = time(nullptr);
	struct tm *tm_info = localtime(&now);

	std::string result;
	result.reserve(tmpl.size() + 32);

	for (size_t i = 0; i < tmpl.size(); i++)
	{
		if (tmpl[i] != '%' || i + 1 >= tmpl.size())
		{
			result += tmpl[i];
			continue;
		}

		char spec = tmpl[i + 1];
		char buf[32];

		// Only expand known datetime specifiers; leave everything else for snprintf.
		switch (spec)
		{
		case 'Y': strftime(buf, sizeof(buf), "%Y", tm_info); result += buf; i++; break;
		case 'y': strftime(buf, sizeof(buf), "%y", tm_info); result += buf; i++; break;
		case 'm': strftime(buf, sizeof(buf), "%m", tm_info); result += buf; i++; break;
		case 'H': strftime(buf, sizeof(buf), "%H", tm_info); result += buf; i++; break;
		case 'M': strftime(buf, sizeof(buf), "%M", tm_info); result += buf; i++; break;
		case 'S': strftime(buf, sizeof(buf), "%S", tm_info); result += buf; i++; break;
		case 'F': strftime(buf, sizeof(buf), "%F", tm_info); result += buf; i++; break;
		case 'T': strftime(buf, sizeof(buf), "%T", tm_info); result += buf; i++; break;
		default:
			// Leave the '%' in place for snprintf (handles %04d, %%, etc.)
			result += tmpl[i];
			break;
		}
	}

	return result;
}

FileOutput::FileOutput(VideoOptions const *options)
	: Output(options), fp_(nullptr), count_(0), file_start_time_ms_(0)
{
}

FileOutput::~FileOutput()
{
	closeFile();
}

void FileOutput::outputBuffer(void *mem, size_t size, int64_t timestamp_us, uint32_t flags)
{
	// We need to open a new file if we're in "segment" mode and our segment is full
	// (though we have to wait for the next I frame), or if we're in "split" mode
	// and recording is being restarted (this is necessarily an I-frame already).
	if (fp_ == nullptr ||
		(options_->Get().segment && (flags & FLAG_KEYFRAME) &&
		 timestamp_us / 1000 - file_start_time_ms_ > options_->Get().segment) ||
		(options_->Get().split && (flags & FLAG_RESTART)))
	{
		closeFile();
		openFile(timestamp_us);
	}

	LOG(2, "FileOutput: output buffer " << mem << " size " << size);
	if (fp_ && size)
	{
		if (fwrite(mem, size, 1, fp_) != 1)
			throw std::runtime_error("failed to write output bytes");
		if (options_->Get().flush)
			fflush(fp_);
	}
}

void FileOutput::openFile(int64_t timestamp_us)
{
	if (options_->Get().output == "-")
		fp_ = stdout;
	else if (!options_->Get().output.empty())
	{
		// Generate the next output file name.
		// First expand datetime specifiers (%Y, %m, %H, %M, %S, etc.), then
		// expand the segment counter via snprintf (%04d and similar).
		std::string tmpl = applyDatetimeToTemplate(options_->Get().output);
		char filename[256];
		int n = snprintf(filename, sizeof(filename), tmpl.c_str(), count_);
		count_++;
		if (options_->Get().wrap)
			count_ = count_ % options_->Get().wrap;
		if (n < 0)
			throw std::runtime_error("failed to generate filename");

		fp_ = fopen(filename, "w");
		if (!fp_)
			throw std::runtime_error("failed to open output file " + std::string(filename));
		LOG(2, "FileOutput: opened output file " << filename);

		file_start_time_ms_ = timestamp_us / 1000;

		if (new_file_callback_)
			new_file_callback_(std::string(filename));
	}
}

void FileOutput::closeFile()
{
	if (fp_)
	{
		if (options_->Get().flush)
			fflush(fp_);
		if (fp_ != stdout)
			fclose(fp_);
		fp_ = nullptr;
	}
}
