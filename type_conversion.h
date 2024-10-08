#pragma once

#include <amxxmodule.h>
#include <rehlds_api.h>

#define INDEXENT edictByIndex
#define ENTINDEX indexOfEdict
#define AMX_NULLENT -1

class IGameClient;

extern edict_t* g_pEdicts;
extern IRehldsServerStatic* g_RehldsSvs;

inline size_t indexOfEdict(const edict_t* ed)
{
	return ed ? ed - g_pEdicts : 0;
}

inline size_t indexOfEdict(const entvars_t* pev)
{
	return indexOfEdict(pev->pContainingEntity);
}

// safe to nullptr
inline size_t indexOfEdictAmx(const entvars_t* pev)
{
	return likely(pev != nullptr) ? indexOfEdict(pev) : AMX_NULLENT;
}

inline size_t indexOfEdictAmx(const edict_t *pEdict)
{
	return likely(pEdict != nullptr) ? indexOfEdict(pEdict) : AMX_NULLENT;
}

// fast
inline edict_t* edictByIndex(const int index)
{
	return likely(g_pEdicts == nullptr) ? nullptr : g_pEdicts + index;
}

inline IGameClient* clientByIndex(const int index)
{
	return likely(index > 0) ? g_RehldsSvs->GetClient(index - 1) : nullptr;
}

// safe to index -1
inline edict_t* edictByIndexAmx(const int index)
{
	return likely(index >= 0) ? g_pEdicts + index : nullptr;
}

template<typename T>
inline T* getPrivate(const int index)
{
	return likely(index >= 0) ? (T*)g_pEdicts[index].pvPrivateData : (T*)nullptr;
}

template<typename T>
inline T* getPrivate(const edict_t *pEdict)
{
	return likely(pEdict != nullptr) ? (T*)pEdict->pvPrivateData : (T*)nullptr;
}

inline entvars_t* PEV(const int index)
{
	return likely(index >= 0) ? &g_pEdicts[index].v : nullptr;
}

template<typename T>
inline size_t indexOfPDataAmx(const T* pdata)
{
	return likely(pdata != nullptr) ? indexOfEdict(pdata->pev) : AMX_NULLENT;
}

inline cell getAmxVector(Vector& vec)
{
	return g_amxxapi.PrepareCellArrayA(reinterpret_cast<cell *>(&vec), 3, true);
}

// HLTypeConversion.h -> AMXModX
template <typename T>
inline T &ref_member(void *ptr, int offset, int element = 0)
{
	return *reinterpret_cast<T *>((reinterpret_cast<int8 *>(ptr) + offset + (element * sizeof(T))));
}

template <typename T>
inline T &get_member(void *ptr, int offset, int element = 0)
{
	return ref_member<T>(ptr, offset, element);
}

template <typename T>
inline T &get_member(edict_t *pEntity, int offset, int element = 0)
{
	return get_member<T>(pEntity->pvPrivateData, offset, element);
}

template <typename T>
inline void set_member(void *ptr, int offset, T value, int element = 0)
{
	ref_member<T>(ptr, offset, element) = value;
}

template <typename T>
inline void set_member(edict_t *pEntity, int offset, T value, int element = 0)
{
	set_member<T>(pEntity->pvPrivateData, offset, value, element);
}

template <typename T>
inline T* get_member_direct(void *ptr, int offset, int element = 0, int size = sizeof(T))
{
	return reinterpret_cast<T *>(reinterpret_cast<int8 *>(ptr) + offset + (element * size));
}

template <typename T>
inline T get_member_direct(edict_t *pEntity, int offset, int element = 0, int size = 0)
{
	return get_member_direct<T>(pEntity->pvPrivateData, offset, element, size);
}

class CTempStrings
{
public:
	CTempStrings();
	char* push(struct tagAMX* amx);
	void pop(size_t count);

	enum
	{
		STRINGS_MAX = 16,
		STRING_SIZE = 1024,
		STRING_LEN = STRING_SIZE - 1
	};

private:
	size_t m_current;
	char m_strings[STRINGS_MAX][STRING_SIZE];
};
