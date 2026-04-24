/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * rpicam_vid.cpp - libcamera video record app.
 */

#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/stat.h>

#include "core/rpicam_encoder.hpp"
#include "image/image.hpp"
#include "output/output.hpp"

using namespace std::placeholders;

// Some keypress/signal handling.

static int signal_received;
static void default_signal_handler(int signal_number)
{
	signal_received = signal_number;
	LOG(1, "Received signal " << signal_number);
}

static int get_key_or_signal(VideoOptions const *options, pollfd p[1])
{
	int key = 0;
	if (signal_received == SIGINT)
		return 'x';
	if (options->Get().keypress)
	{
		poll(p, 1, 0);
		if (p[0].revents & POLLIN)
		{
			char *user_string = nullptr;
			size_t len;
			[[maybe_unused]] size_t r = getline(&user_string, &len, stdin);
			key = user_string[0];
		}
	}
	if (options->Get().signal)
	{
		if (signal_received == SIGUSR1)
			key = '\n';
		else if ((signal_received == SIGUSR2) || (signal_received == SIGPIPE))
			key = 'x';
		signal_received = 0;
	}
	return key;
}

static int get_colourspace_flags(std::string const &codec)
{
	if (codec == "mjpeg" || codec == "yuv420")
		return RPiCamEncoder::FLAG_VIDEO_JPEG_COLOURSPACE;
	else
		return RPiCamEncoder::FLAG_VIDEO_NONE;
}

// The main even loop for the application.

static void event_loop(RPiCamEncoder &app)
{
	VideoOptions const *options = app.GetOptions();
	std::unique_ptr<Output> output = std::unique_ptr<Output>(Output::Create(options));
	app.SetEncodeOutputReadyCallback(std::bind(&Output::OutputReady, output.get(), _1, _2, _3, _4));
	app.SetMetadataReadyCallback(std::bind(&Output::MetadataReady, output.get(), _1));

	// Screenshot support: when a new segment file is opened, save a JPEG of the next raw frame.
	std::atomic<bool> pending_screenshot{ false };
	std::mutex screenshot_mutex;
	std::string screenshot_path;
	// Pre-allocated buffer reused across screenshots to avoid repeated heap allocation.
	std::vector<uint8_t> screenshot_buf;

	if (!options->Get().screenshot.empty())
	{
		output->SetNewFileCallback([&](const std::string &video_filename)
		{
			std::string stem = std::filesystem::path(video_filename).stem().string();
			std::string tmpl = options->Get().screenshot;
			size_t pos;
			while ((pos = tmpl.find("%V")) != std::string::npos)
				tmpl.replace(pos, 2, stem);
			{
				std::lock_guard<std::mutex> lock(screenshot_mutex);
				screenshot_path = tmpl;
			}
			pending_screenshot = true;
		});
	}

	app.OpenCamera();
	app.ConfigureVideo(get_colourspace_flags(options->Get().codec));
	app.StartEncoder();
	app.StartCamera();
	auto start_time = std::chrono::high_resolution_clock::now();

	// Monitoring for keypresses and signals.
	signal(SIGUSR1, default_signal_handler);
	signal(SIGUSR2, default_signal_handler);
	signal(SIGINT, default_signal_handler);
	// SIGPIPE gets raised when trying to write to an already closed socket. This can happen, when
	// you're using TCP to stream to VLC and the user presses the stop button in VLC. Catching the
	// signal to be able to react on it, otherwise the app terminates.
	signal(SIGPIPE, default_signal_handler);
	pollfd p[1] = { { STDIN_FILENO, POLLIN, 0 } };

	for (unsigned int count = 0; ; count++)
	{
		RPiCamEncoder::Msg msg = app.Wait();
		if (msg.type == RPiCamApp::MsgType::Timeout)
		{
			LOG_ERROR("ERROR: Device timeout detected, attempting a restart!!!");
			app.StopCamera();
			app.ConfigureVideo(get_colourspace_flags(options->Get().codec));
			app.StartCamera();
			continue;
		}
		if (msg.type == RPiCamEncoder::MsgType::Quit)
			return;
		else if (msg.type != RPiCamEncoder::MsgType::RequestComplete)
			throw std::runtime_error("unrecognised message!");
		int key = get_key_or_signal(options, p);
		if (key == '\n')
			output->Signal();

		LOG(2, "Viewfinder frame " << count);
		auto now = std::chrono::high_resolution_clock::now();
		bool timeout = !options->Get().frames && options->Get().timeout &&
					   ((now - start_time) > options->Get().timeout.value);
		bool frameout = options->Get().frames && count >= options->Get().frames;
		if (timeout || frameout || key == 'x' || key == 'X')
		{
			if (timeout)
				LOG(1, "Halting: reached timeout of " << options->Get().timeout.get<std::chrono::milliseconds>()
													  << " milliseconds.");
			app.StopCamera(); // stop complains if encoder very slow to close
			app.StopEncoder();
			return;
		}
		CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);
		auto *stream = app.VideoStream();

		// Screenshot is taken before EncodeBuffer so the DMA buffer is still
		// exclusively in CPU-read mode (before V4L2 QBUF hands it to the
		// hardware encoder's DMA engine).
		if (pending_screenshot.exchange(false))
		{
			std::string path;
			{
				std::lock_guard<std::mutex> lock(screenshot_mutex);
				path = screenshot_path;
			}
			try
			{
				StreamInfo info = app.GetStreamInfo(stream);
				auto it = completed_request->buffers.find(stream);
				if (it == completed_request->buffers.end() || !it->second)
					throw std::runtime_error("video stream buffer not found");
				auto *buffer = it->second;
				BufferReadSync r(&app, buffer);
				auto &spans = r.Get();
				if (!spans.empty() && spans[0].size() > 0)
				{
					// Copy frame data into pre-allocated buffer (reused across screenshots).
					screenshot_buf.resize(spans[0].size());
					std::memcpy(screenshot_buf.data(), spans[0].data(), spans[0].size());
					// JPEG encoding (~100ms) runs in a detached thread with its own copy.
					std::thread([data = screenshot_buf, info, path]()
					{
						try
						{
							libcamera::Span<uint8_t> span(const_cast<uint8_t *>(data.data()), data.size());
							jpeg_save_simple({ span }, info, path, 85);
							LOG(1, "Screenshot saved to " << path);
						}
						catch (std::exception const &e)
						{
							LOG_ERROR("Failed to save screenshot: " << e.what());
						}
					}).detach();
				}
			}
			catch (std::exception const &e)
			{
				LOG_ERROR("Failed to prepare screenshot: " << e.what());
			}
		}

		if (!app.EncodeBuffer(completed_request, stream))
		{
			// Keep advancing our "start time" if we're still waiting to start recording (e.g.
			// waiting for synchronisation with another camera).
			start_time = now;
			count = 0; // reset the "frames encoded" counter too
		}

		app.ShowPreview(completed_request, stream);
	}
}

int main(int argc, char *argv[])
{
	try
	{
		RPiCamEncoder app;
		VideoOptions *options = app.GetOptions();
		if (options->Parse(argc, argv))
		{
			if (options->Get().verbose >= 2)
				options->Get().Print();

			event_loop(app);
		}
	}
	catch (std::exception const &e)
	{
		LOG_ERROR("ERROR: *** " << e.what() << " ***");
		return -1;
	}
	return 0;
}
