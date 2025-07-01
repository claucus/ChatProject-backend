#pragma once

#include <map>
#include <string>

struct SectionInfo {
	std::map < std::string, std::string > _section;

	SectionInfo() = default;
	~SectionInfo();
	SectionInfo(const SectionInfo&);
	SectionInfo& operator=(const SectionInfo&);
	std::string operator[](const std::string&);

};
class ConfigManager
{
public:
	~ConfigManager();

	static ConfigManager& GetInstance();

	ConfigManager(const ConfigManager&) = delete;
	ConfigManager& operator=(const ConfigManager&) = delete;

	SectionInfo operator[](const std::string&);

private:
	ConfigManager();
	std::map < std::string, SectionInfo >_configMap;
};

