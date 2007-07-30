m4_ignore([dnl
#include <sys/types.h>

#include <db.h>

int
main()
{
	DB_ENV *dbenv;
	u_int32_t flags;
	int mode;
])
m4_indent([dnl
dbenv-__GT__set_data_dir(dbenv, "/b/data2");
dbenv-__GT__set_data_dir(dbenv, "data1");
dbenv-__GT__open(dbenv, "/a/database", flags, mode);])
m4_ignore([dnl
	return (0);
}])
