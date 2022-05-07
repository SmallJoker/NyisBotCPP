#pragma once
#include "types.h"
#include "channel.h"

// ============ User ID / Channel ID ============

struct UserIdBasic : IImplId {
	UserIdBasic(cstr_t &nick) : nickptr(&nick) {}

	virtual IImplId *copy(void *parent) const
	{
		UserInstance *ui = (UserInstance *)parent;
		ui->nickname = *nickptr;
		return new UserIdBasic(ui->nickname);
	}

	bool is(const IImplId *other) const
	{ return *nickptr == *((UserIdBasic *)other)->nickptr; }

	std::string idStr() const { return *nickptr; }
	std::string nameStr() const { return *nickptr; }

	// TODO: move main nickname here (from UserInstance)
	const std::string *nickptr;
};

struct ChannelIdBasic : IImplId {
	ChannelIdBasic(cstr_t &name) : name(name) {}

	virtual IImplId *copy(void *parent) const
	{
		return new ChannelIdBasic(name);
	}

	bool is(const IImplId *other) const
	{ return name == ((ChannelIdBasic *)other)->name; }

	std::string idStr() const { return name; }
	std::string nameStr() const { return name; }

	std::string name;
};
