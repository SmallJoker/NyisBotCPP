#include "container.h"
#include "logger.h"

Containers::~Containers()
{
	for (auto &it : m_data) {
		delete it.second;
		it.second = nullptr;
	}
}

void Containers::set(const ContainerOwner *owner, IContainer *data)
{
	IContainer *existing = get(owner);
	if (existing && existing != data) {
		WARN("Overwriting existing container data! "
			<< "old=" << existing->dump()
			<< ", new=" << data->dump());
		delete existing;
	}

	VERBOSE(data->dump());
	m_data.insert({owner, data});
}

IContainer *Containers::get(const ContainerOwner *owner) const
{
	auto it = m_data.find(owner);
	return (it == m_data.end()) ? nullptr : it->second;
}

bool Containers::remove(const ContainerOwner *owner)
{
	auto it = m_data.find(owner);
	if (it == m_data.end()) {
		VERBOSE("Attempt to remove non-existent container");
		return false;
	}

	VERBOSE(it->second->dump());
	delete it->second;
	m_data.erase(it);
	return true;
}

bool Containers::move(const ContainerOwner *old_owner, const ContainerOwner *new_owner)
{
	auto it = m_data.find(old_owner);
	if (it == m_data.end() || m_data.find(new_owner) != m_data.end())
		return false;

	IContainer *c = it->second;
	m_data.erase(it);
	m_data.insert({new_owner, c});
	return true;
}
