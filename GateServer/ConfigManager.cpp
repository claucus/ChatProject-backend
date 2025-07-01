#include "ConfigManager.h"
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <spdlog/spdlog.h>

SectionInfo::SectionInfo(const SectionInfo& src)
{
	_section = src._section;
}

SectionInfo& SectionInfo::operator=(const SectionInfo& src)
{
	if (&src != this) {
		this->_section = src._section;
	}
	return *this;
}

std::string SectionInfo::operator[](const std::string& key)
{
	if (_section.find(key) == _section.end()) {
		return "";
	}
	return _section[key];
}

SectionInfo::~SectionInfo()
{
	_section.clear();
}

ConfigManager::~ConfigManager()
{
	_configMap.clear();
}

ConfigManager& ConfigManager::GetInstance()
{
	static ConfigManager configManager;
	return configManager;
}

ConfigManager::ConfigManager()
{
	auto currentPath = boost::filesystem::current_path();
	auto configPath = currentPath / "config.ini";

	boost::property_tree::ptree pt;
	boost::property_tree::read_ini(configPath.string(), pt);


	for (const auto& sectionPair : pt) {
		const std::string& sectionName = sectionPair.first;
		const boost::property_tree::ptree& sectionTree = sectionPair.second;

		std::map<std::string, std::string> sectionConfig;
		for (const auto& node : sectionTree) {
			const std::string& key = node.first;
			const std::string& value = node.second.get_value<std::string>();
			sectionConfig[key] = value;
		}

		SectionInfo sectionInfo;
		sectionInfo._section = sectionConfig;
		_configMap[sectionName] = sectionInfo;
	}
    spdlog::info("[ConfigManager] Loading config.ini Success");
}

SectionInfo ConfigManager::operator[](const std::string& section)
{
	if (_configMap.find(section) == _configMap.end()) {
		return SectionInfo();
	}

	return _configMap[section];
}

