#include "utils.h"

// Shameless copy
std::string trim(cstr_t &str)
{
	size_t front = 0;

	while (std::isspace(str[front]))
		++front;

	size_t back = str.size();
	while (back > front && std::isspace(str[back - 1]))
		--back;

	return str.substr(front, back - front);
}

std::vector<std::string> strsplit(cstr_t &input, char delim)
{
	std::vector<std::string> parts;

	for (size_t pos = 0; pos < input.size(); ) {
		size_t delim_pos = input.find(delim, pos);
		if (delim_pos == std::string::npos)
			delim_pos = input.size();

		if (delim_pos == pos)
			continue; // Ignore empty strings

		parts.emplace_back(input.substr(pos, delim_pos - pos));
		pos = delim_pos + 1;
	}

	return parts;
}

std::string get_next_part(std::string &input)
{
	auto pos = input.find(' ');
	std::string value;
	if (pos == std::string::npos) {
		std::swap(value, input);
		return value;
	}

	value = input.substr(0, pos);
	input = input.substr(pos + 1);
	return value;
}


Containers::~Containers()
{
	for (auto &it : m_data) {
		delete it.second;
		it.second = nullptr;
	}
}

bool Containers::add(const void *owner, IContainer *data)
{
	if (get(owner))
		return false;

	m_data.insert({owner, data});
	return true;
}

IContainer *Containers::get(const void *owner) const
{
	auto it = m_data.find(owner);
	return (it == m_data.end()) ? nullptr : it->second;
}

bool Containers::remove(const void *owner)
{
	auto it = m_data.find(owner);
	if (it == m_data.end())
		return false;

	delete it->second;
	m_data.erase(it);
	return true;
}

bool Containers::move(const void *old_owner, const void *new_owner)
{
	auto it = m_data.find(old_owner);
	if (it == m_data.end() || m_data.find(new_owner) != m_data.end())
		return false;

	IContainer *c = it->second;
	m_data.erase(it);
	m_data.insert({new_owner, c});
	return true;
}
