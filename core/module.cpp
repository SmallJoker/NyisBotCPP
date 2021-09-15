
#include "channel.h"
#include "chatcommand.h"
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
	bool load(Client *cli);
	void unload(Network *net);

	void *dll_handle;
	IModule *module;
	std::string name;
	std::string path;
};


// ================= IModule =================

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

ModuleMgr::ModuleMgr(Client *cli)
{
	m_client = cli;
	m_commands = new ChatCommand(nullptr);

	m_last_step = std::chrono::high_resolution_clock::now();
}

ModuleMgr::~ModuleMgr()
{
	unloadModules();
	delete m_commands;
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
	IModule *expired_ptr = mi->module;

	// Remove all invalid module-registered commands
	m_commands->remove(mi->module);

	mi->unload(keep_data ? nullptr : net);
	bool ok = mi->load(m_client);

	if (ok) {
		mi->module->initCommands(*m_commands);
		if (m_client)
			mi->module->onClientReady();
	} else {
		// Well shit. Now it's too late to free the containers
		keep_data = false;
		m_modules.erase(mi);
	}

	if (net && keep_data) {
		// Update and re-assign old references
		// let's hope the two classes were not modified between the module loads

		auto &users = net->getAllUsers();
		for (auto ui : users)
			ui->move(expired_ptr, mi->module);

		for (Channel *c : net->getAllChannels())
			c->getContainers()->move(expired_ptr, mi->module);
	}

	return ok;
}

void ModuleMgr::unloadModules()
{
	if (m_modules.empty())
		return;

	LOG("Unloading modules...");

	Network *net = m_client ? m_client->getNetwork() : nullptr;
	for (ModuleInternal *mi : m_modules) {
		mi->unload(net);
		delete mi;
	}
	m_modules.clear();

	*m_commands = ChatCommand(nullptr); // reset
}

bool ModuleMgr::loadSingleModule(cstr_t &module_name, cstr_t &path)
{
	ModuleInternal *mi = new ModuleInternal();
	mi->name = module_name;
	mi->path = path;
	bool ok = mi->load(m_client);

	if (ok) {
		m_modules.insert(mi);

		mi->module->initCommands(*m_commands);
		if (m_client)
			mi->module->onClientReady();
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

void ModuleMgr::onStep(float time)
{
	MutexLock _(m_lock);

	auto time_now = std::chrono::high_resolution_clock::now();
	time = std::chrono::duration<float>(time_now - m_last_step).count();

	if (time < 0.2f)
		return;

	m_last_step = time_now;

	for (ModuleInternal *mi : m_modules)
		mi->module->onStep(time);
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

bool ModuleMgr::onUserSay(Channel *c, UserInstance *ui, std::string &msg)
{
	MutexLock _(m_lock);

	std::string backup(msg);
	if (m_commands->run(c, ui, msg))
		return true;

	for (ModuleInternal *mi : m_modules) {
		if (msg.size() != backup.size())
			msg.assign(backup);

		if (mi->module->onUserSay(c, ui, msg))
			return true;
	}
	return false;
}


// ================= ModuleInternal =================

bool ModuleInternal::load(Client *cli)
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
		unload(nullptr);
		return false;
	}
	module->m_path = &path;
	module->m_client = cli;
	return true;
}

void ModuleInternal::unload(Network *net)
{
	module->onModuleUnload();

	if (net) {
		// Remove this module data from all locations before the destructor turns invalid

		auto &users = net->getAllUsers();
		for (auto ui : users)
			ui->remove(module);

		for (Channel *c : net->getAllChannels())
			c->getContainers()->remove(module);
	}

	delete module;
	dlclose(dll_handle);

	module = nullptr;
	dll_handle = nullptr;
}
