/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2021, Raspberry Pi (Trading) Limited
 *
 * auto_update_text_stage.cpp - automatically update text from file
 */

#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

#include "core/rpicam_app.hpp"
#include "core/logging.hpp"
#include "post_processing_stages/post_processing_stage.hpp"

using namespace std::chrono_literals;

class AutoUpdateTextStage : public PostProcessingStage
{
public:
	AutoUpdateTextStage(RPiCamApp *app) 
		: PostProcessingStage(app), 
		  running_(false),
		  last_read_time_(std::chrono::steady_clock::now())
	{
	}

	~AutoUpdateTextStage()
	{
		Stop();
	}

	char const *Name() const override;

	void Read(boost::property_tree::ptree const &params) override;

	void Start() override;

	bool Process(CompletedRequestPtr &completed_request) override;

	void Stop() override;

private:
	void UpdateThreadFunc();
	void ReadTextFromFile();
	std::string GetFilePath();
	void SetFilePath(const std::string& path);

	std::string file_path_;
	std::string current_text_;
	std::mutex text_mutex_;
	std::mutex file_path_mutex_;
	std::mutex time_mutex_;
	std::atomic<bool> running_;
	std::thread update_thread_;
	std::chrono::steady_clock::time_point last_read_time_;
	static constexpr std::chrono::seconds update_interval_{5};
};

#define NAME "auto_update_text"

char const *AutoUpdateTextStage::Name() const
{
	return NAME;
}

std::string AutoUpdateTextStage::GetFilePath()
{
	std::lock_guard<std::mutex> lock(file_path_mutex_);
	return file_path_;
}

void AutoUpdateTextStage::SetFilePath(const std::string& path)
{
	std::lock_guard<std::mutex> lock(file_path_mutex_);
	file_path_ = path;
}

void AutoUpdateTextStage::Read(boost::property_tree::ptree const &params)
{
	// Можно задать путь к файлу в конфиге (опционально)
	std::string path = params.get<std::string>("file", "");
	SetFilePath(path);
	LOG(2, "AutoUpdateTextStage: configured with file: " << path);
}

void AutoUpdateTextStage::Start()
{
	running_ = true;
	{
		std::lock_guard<std::mutex> lock(time_mutex_);
		last_read_time_ = std::chrono::steady_clock::now() - update_interval_; // Читаем сразу при старте
	}
	update_thread_ = std::thread(&AutoUpdateTextStage::UpdateThreadFunc, this);
}

void AutoUpdateTextStage::Stop()
{
	if (running_)
	{
		running_ = false;
		if (update_thread_.joinable())
			update_thread_.join();
	}
}

void AutoUpdateTextStage::UpdateThreadFunc()
{
	while (running_)
	{
		bool should_read = false;
		{
			std::lock_guard<std::mutex> lock(time_mutex_);
			auto now = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_read_time_);
			
			if (elapsed >= update_interval_)
			{
				should_read = true;
				last_read_time_ = now;
			}
		}

		if (should_read)
		{
			ReadTextFromFile();
		}

		// Спим чтобы не нагружать процессор
		std::this_thread::sleep_for(1s);
	}
}

void AutoUpdateTextStage::ReadTextFromFile()
{
	std::string path = GetFilePath();
	
	if (path.empty()) {
		LOG(2, "AutoUpdateTextStage: path empty");
		return;
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		LOG(2, "AutoUpdateTextStage: can not open file " << path);
		return;
	}

	std::string new_text;
	std::string line;
	while (std::getline(file, line))
	{
		if (!new_text.empty())
			new_text += "\n";
		new_text += line;
	}
	file.close();
	
	// Обновляем текст потокобезопасно
	std::lock_guard<std::mutex> lock(text_mutex_);
	current_text_ = new_text;
}

bool AutoUpdateTextStage::Process(CompletedRequestPtr &completed_request)
{	
	// Получаем путь к файлу из метаданных, если он там есть
	std::string file_from_metadata;
	if (completed_request->post_process_metadata.Get("auto_update_text.file", file_from_metadata) == 0)
	{
		std::string current_path = GetFilePath();
		// Если путь изменился, обновляем его
		if (file_from_metadata != current_path)
		{
			LOG(2, "AutoUpdateTextStage: changing file path from " << current_path << " to " << file_from_metadata);
			SetFilePath(file_from_metadata);
			// Сбрасываем время последнего чтения, чтобы прочитать новый файл немедленно
			std::lock_guard<std::mutex> lock(time_mutex_);
			last_read_time_ = std::chrono::steady_clock::now() - update_interval_;
		}
	}

	// Если поток обновления не запущен (например, в rpicam-jpeg), 
	// читаем файл прямо здесь с соблюдением интервала
	if (!running_)
	{
		bool should_read = false;
		{
			std::lock_guard<std::mutex> lock(time_mutex_);
			auto now = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_read_time_);
			if (elapsed >= update_interval_)
			{
				should_read = true;
				last_read_time_ = now;
			}
		}
		
		if (should_read)
		{
			ReadTextFromFile();
		}
	}

	// Получаем текущий текст потокобезопасно
	std::string text;
	{
		std::lock_guard<std::mutex> lock(text_mutex_);
		text = current_text_;
	}

	// Устанавливаем текст в метаданных для AnnotateCvStage
	if (!text.empty())
	{
		completed_request->post_process_metadata.Set("annotate.text", text);
	}

	return false; // Не отбрасываем запрос
}

static PostProcessingStage *Create(RPiCamApp *app)
{
	return new AutoUpdateTextStage(app);
}

static RegisterStage reg(NAME, &Create);
