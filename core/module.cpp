
#include "channel.h"
#include "client.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include "utils.h"

#include <filesystem>
// Unix only
#include <dlfcn.h>

static std::string NAME_PREFIX("libnbm_");


struct ModuleInternal {
	bool load();
	void unload();

	void *dll_handle;
	IModule *module;
	std::string name;
	std::string path;
};


// ================= IModule =================

void IModule::setClient(Client *cli)
{
	m_client = cli;
	if (cli)
		onClientReady();
}

ModuleMgr *IModule::getModuleMgr() const
{
	return m_client->getModuleMgr();
}

Network *IModule::getNetwork() const
{
	return m_client->getNetwork();
}

void IModule::sendRaw(cstr_t &what) const
{
	m_client->sendRaw(what);
}


// ================= ModuleMgr =================

ModuleMgr::~ModuleMgr()
{
	unloadModules();
}

bool ModuleMgr::loadModules()
{
	if (m_modules.size() > 0)
		return false;

	bool good = true;
	// Find modules in path, named "libnbm_?????.so"
	for (const auto &entry : std::filesystem::directory_iterator("modules")) {
		const std::string &filename = entry.path().filename();
		size_t cut_a = filename.rfind(NAME_PREFIX);
		size_t cut_b = filename.find('.');
		if (cut_a == std::string::npos || cut_b == std::string::npos)
			continue;
		if (entry.path().extension() == "so")
			continue;

		cut_a += NAME_PREFIX.size();
		std::string module_name(filename.substr(cut_a, cut_b - cut_a));

		if (!loadSingleModule(module_name, entry.path().string())) {
			good = false;
			break;
		}
	}

	if (!good)
		unloadModules();
	return good;
}

bool ModuleMgr::reloadModule(std::string name, bool keep_data)
{
	bool can_lock = m_lock.try_lock();
	if (!can_lock) {
		LOG("Delaying module reload");
		m_client->addTodo(ClientTodo {
			.type = ClientTodo::CTT_RELOAD_MODULE,
			.reload_module = {
				.path = new std::string(name),
				.keep_data = keep_data
			}
		});
		return true;
	}
	// This is really ugly
	m_lock.unlock();
	MutexLock _(m_lock);

	name = NAME_PREFIX + name;

	ModuleInternal *mi = nullptr;
	for (ModuleInternal *it : m_modules) {
		if (it->path.find(name) == std::string::npos)
			continue;

		mi = it;
		break;
	}

	if (!mi)
		return false;

	Network *net = m_client ? m_client->getNetwork() : nullptr;
	if (net && !keep_data) {
		// Remove this module data from all locations before the destructor turns invalid

		auto &users = net->getAllUsers();
		for (auto ui : users)
			ui->data->remove(mi->module);

		for (Channel *c : net->getAllChannels())
			c->getContainers()->remove(mi->module);
	}

	IModule *expired_ptr = mi->module;
	mi->unload();
	bool ok = mi->load();

	if (ok) {
		mi->module->setClient(m_client);
	} else {
		keep_data = false;
		m_modules.erase(mi);
	}

	if (net && keep_data) {
		// Update and re-assign old references
		// let's hope the two classes were not modified between the module loads

		auto &users = net->getAllUsers();
		for (auto ui : users)
			ui->data->move(expired_ptr, mi->module);

		for (Channel *c : net->getAllChannels())
			c->getContainers()->move(expired_ptr, mi->module);
	}

	return ok;
}

void ModuleMgr::unloadModules()
{
	LOG("Unloading modules...");
	for (ModuleInternal *mi : m_modules) {
		mi->unload();
		delete mi;
	}
	m_modules.clear();
}

bool ModuleMgr::loadSingleModule(cstr_t &module_name, cstr_t &path)
{
	ModuleInternal *mi = new ModuleInternal();
	mi->name = module_name;
	mi->path = path;
	bool ok = mi->load();

	if (ok) {
		m_modules.insert(mi);
		mi->module->setClient(m_client);
	} else {
		delete mi;
	}

	return ok;
}

Settings *ModuleMgr::getSettings(IModule *module) const
{
	ModuleInternal *mi = nullptr;
	for (ModuleInternal *it : m_modules) {
		if (it->module == module) {
			mi = it;
			break;
		}
	}
	if (!mi)
		return nullptr;

	return new Settings("config/modules.conf", nullptr, mi->name);
}

void ModuleMgr::onClientReady()
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onClientReady();
}

void ModuleMgr::onChannelJoin(Channel *c)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onChannelJoin(c);
}

void ModuleMgr::onChannelLeave(Channel *c)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onChannelLeave(c);
}

void ModuleMgr::onUserJoin(Channel *c, UserInstance *ui)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserJoin(c, ui);
}

void ModuleMgr::onUserLeave(Channel *c, UserInstance *ui)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserLeave(c, ui);
}

void ModuleMgr::onUserRename(UserInstance *ui, cstr_t &old_name)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserRename(ui, old_name);
}

bool ModuleMgr::onUserSay(Channel *c, ChatInfo info)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules) {
		if (mi->module->onUserSay(c, info))
			return true;
	}
	return false;
}


// ================= ModuleInternal =================

bool ModuleInternal::load()
{
	LOG("Loading module " << path);
	void *handle = dlopen(path.c_str(), RTLD_NOW);
	if (!handle) {
		ERROR("Failed to load module: " << dlerror());
		return false;
	}

	using InitType = IModule*(*)();
	// "_Z8nbm_initv" for C++, or "nbm_init" for extern "C"
	auto func = reinterpret_cast<InitType>(dlsym(handle, "nbm_init"));
	if (!func) {
		ERROR("Missing init function");
		dlclose(handle);
		return false;
	}

	dll_handle = handle;
	module = func();
	if (!module) {
		unload();
		return false;
	}
	module->m_path = &path;
	return true;
}

void ModuleInternal::unload()
{
	delete module;
	dlclose(dll_handle);

	module = nullptr;
	dll_handle = nullptr;
}
