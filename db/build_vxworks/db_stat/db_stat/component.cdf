/* component.cdf - dynamically updated configuration */

/*
 * NOTE: you may edit this file to alter the configuration
 * But all non-configuration information, including comments,
 * will be lost upon rebuilding this project.
 */

/* Component information */

Component INCLUDE_DB_STAT {
	ENTRY_POINTS	ALL_GLOBAL_SYMBOLS
	MODULES		db_stat.o
	NAME		db_stat
	PREF_DOMAIN	ANY
	_INIT_ORDER	usrComponentsInit
}

/* EntryPoint information */

/* Module information */

Module db_stat.o {

	NAME		db_stat.o
	SRC_PATH_NAME	$PRJ_DIR/../db_stat.c
}

/* Parameter information */

