#include "container.h"
#include "logger.h"

Containers::~Containers()
{
	for (auto &it : m_data) {
		delete it.second;
		it.second = nullptr;
	}
}

void Containers::set(const void *owner, IContainer *data)
{
	IContainer *existing = get(owner);
	if (existing != data) {
		WARN("Overwriting existing container data!");
		delete existing;
	}

	m_data.insert({owner, data});
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
