#include "container.h"

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
