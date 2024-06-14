// Get a setinfo value from a player entity.
inline char *ENTITY_KEYVALUE(edict_t *entity, const char *key) {
	char *ifbuf=GET_INFOKEYBUFFER(entity);
	return(INFOKEY_VALUE(ifbuf, key));
}

// Set a setinfo value for a player entity.
inline void ENTITY_SET_KEYVALUE(edict_t *entity, const char *key, const char *value) {
	char *ifbuf=GET_INFOKEYBUFFER(entity);
	SET_CLIENT_KEYVALUE(ENTINDEX(entity), ifbuf, key, value);
}

// Get a "serverinfo" value.
inline char *SERVERINFO(const char *key) {
	edict_t *server=INDEXENT(0);
	return(ENTITY_KEYVALUE(server, key));
}

// Set a "serverinfo" value.
inline void SET_SERVERINFO(const char *key, const char *value) {
	edict_t *server=INDEXENT(0);
	char *ifbuf=GET_INFOKEYBUFFER(server);
	SET_SERVER_KEYVALUE(ifbuf, key, value);
}

// Get a "localinfo" value.
inline char *LOCALINFO(const char *key) {
	edict_t *server=NULL;
	return(ENTITY_KEYVALUE(server, (char *)key));
}

// Set a "localinfo" value.
inline void SET_LOCALINFO(const char *key, const char *value) {
	edict_t *server=NULL;
	char *ifbuf=GET_INFOKEYBUFFER(server);
	SET_SERVER_KEYVALUE(ifbuf, key, value);
}