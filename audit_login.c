/*
   Authored by Shlomi Noach, Outbrain

   Copyright (c) 2013, Outbrain Inc. All rights reserved

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  */

/*
   Forked from Oracle sample code, provided with MySQL sources, licensed as follows:
 */

/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <mysql.h>
#include <my_sys.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif


#define MAX_SERVER_HOSTNAME_LENGTH	128
#define MAX_CONFIG_LINE_LENGTH		1024
#define MAX_SKIP_USERS				64

/* for SHOW STATUS, see below */
static volatile int number_of_successful_logins;
static volatile int number_of_failed_logins;
static volatile int number_of_logouts;

/* internals */
static char plugin_enabled;
static FILE * audit_login_file;
static char server_hostname[MAX_SERVER_HOSTNAME_LENGTH];
static char skip_users_config_value[MAX_CONFIG_LINE_LENGTH];
static char * skip_users[MAX_SKIP_USERS];
static int num_skip_users;

/*
  Initialize the plugin at server start or plugin installation.

  SYNOPSIS
    audit_login_plugin_init()

  DESCRIPTION
    Initializes counters, opens logfile

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int audit_login_plugin_init(void *arg __attribute__((unused))) {
	FILE * config_file;
	char config_line[MAX_CONFIG_LINE_LENGTH];
	char * config_param = NULL;
	char * config_value = NULL;
	char * config_value_token = NULL;

	number_of_successful_logins = 0;
	number_of_failed_logins = 0;
	number_of_logouts = 0;
	num_skip_users = 0;

	config_file = fopen("audit_login.cnf", "r");
	if (config_file != NULL) {
		while (fgets(config_line, MAX_CONFIG_LINE_LENGTH - 1, config_file)
				!= NULL) {
			strtok(config_line, "\n");
			config_param = strtok(config_line, "=");
			config_value = strtok(NULL, "=");
			if (!strcmp(config_param, "enabled")) {
				if (!strcmp(config_value, "0")) {
					plugin_enabled = 0;
				}
			}
			if (!strcmp(config_param, "skip_users")) {
				strcpy(skip_users_config_value, config_value);
				config_value_token = strtok(skip_users_config_value, ",");
				while (config_value_token != NULL) {
					skip_users[num_skip_users] = config_value_token;
					num_skip_users++;
					config_value_token = strtok(NULL, ",");
				}
			}
		}
		fclose(config_file);
	}

	if (!plugin_enabled)
		return (0);

	server_hostname[MAX_SERVER_HOSTNAME_LENGTH - 1] = 0;
	gethostname(server_hostname, MAX_SERVER_HOSTNAME_LENGTH - 1);

	audit_login_file = NULL;
	audit_login_file = fopen("audit_login.log", "a");

	return (0);
}


/*
  Terminate the plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    audit_login_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int audit_login_plugin_deinit(void *arg __attribute__((unused)))
{
  if (audit_login_file != NULL)
  {
    fclose(audit_login_file);
  }
  return(0);
}



static void audit_login_entry(
		const struct mysql_event_connection *event_connection,
		char * event_type_description) {
	char time_str[20];
	int index;

	if (audit_login_file == NULL)
		return;

	for (index = 0; index < num_skip_users; index++) {
		// skip this user?
		if (!strcmp(event_connection->user, skip_users[index]))
			return;
	}

	//my_ulonglong now = my_getsystime() / 10000000;
	get_date(time_str, GETDATE_DATE_TIME | GETDATE_FIXEDLENGTH, 0);
	fprintf(audit_login_file,
			"{\"ts\":\"%s\",\"type\":\"%s\",\"myhost\":\"%s\",\"thread\":\"%lu\",\"user\":\"%s\",\"priv_user\":\"%s\",\"host\":\"%s\",\"ip\":\"%s\"}\n",
			time_str, event_type_description, server_hostname,
			event_connection->thread_id, event_connection->user,
			event_connection->priv_user, event_connection->host,
			event_connection->ip);
	fflush(audit_login_file);
}


/*
  Foo

  SYNOPSIS
    audit_login_notify()
      thd                connection context

  DESCRIPTION
*/

static void audit_login_notify(MYSQL_THD thd __attribute__((unused)),
		unsigned int event_class, const void *event) {

	if (!plugin_enabled)
		return;
	if (event_class != MYSQL_AUDIT_CONNECTION_CLASS)
		return;

	const struct mysql_event_connection *event_connection =
			(const struct mysql_event_connection *) event;
	char * event_type_description = "";
	int is_failed_login = 0;

	switch (event_connection->event_subclass) {
	case MYSQL_AUDIT_CONNECTION_CONNECT:
		is_failed_login = (event_connection->priv_user == NULL)
				|| !strlen(event_connection->priv_user);
		if (is_failed_login) {
			event_type_description = "failed_login";
			number_of_failed_logins++;
		} else {
			event_type_description = "successful_login";
			number_of_successful_logins++;
		}
		if (audit_login_file != NULL) {
			audit_login_entry(event_connection, event_type_description);
		}
		break;
	case MYSQL_AUDIT_CONNECTION_DISCONNECT:
		event_type_description = "logout";
		number_of_logouts++;
		break;
	default:
		break;
	}
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_audit audit_login_descriptor=
{
  MYSQL_AUDIT_INTERFACE_VERSION,                    /* interface version    */
  NULL,                                             /* release_thd function */
  audit_login_notify,                                /* notify function      */
  { (unsigned long) MYSQL_AUDIT_CONNECTION_CLASSMASK } /* class mask           */
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var audit_login_status[]=
{
  { "Audit_login_count_successful_logins", (char *) &number_of_successful_logins, SHOW_INT },
  { "Audit_login_count_failed_logins",     (char *) &number_of_failed_logins, SHOW_INT },
  { "Audit_login_count_logouts",           (char *) &number_of_logouts, SHOW_INT },
  { "Audit_login_count_skipped_users",     (char *) &num_skip_users, SHOW_INT },
  { "Audit_login_enabled",                 (char *) &plugin_enabled, SHOW_INT },
  { 0, 0, 0}
};

/*
   Plugin system variables for SHOW VARIABLES
*/
static MYSQL_SYSVAR_BOOL(enabled, plugin_enabled, PLUGIN_VAR_NOCMDARG,
		"enable/disable the plugin's operation, namely writing to file", NULL, NULL, 1);

static struct st_mysql_sys_var * audit_login_sysvars[] = {
    MYSQL_SYSVAR(enabled),
    NULL
};
/*
  Plugin library descriptor
*/
mysql_declare_plugin(audit_login)
{
  MYSQL_AUDIT_PLUGIN,          /* type                            */
  &audit_login_descriptor,     /* descriptor                      */
  "SIMPLE_LOGIN_AUDIT",        /* name                            */
  "Shlomi Noach",              /* author                          */
  "MySQL login/logout audit",  /* description                     */
  PLUGIN_LICENSE_GPL,
  audit_login_plugin_init,     /* init function (when loaded)     */
  audit_login_plugin_deinit,   /* deinit function (when unloaded) */
  0x0002,                      /* version                         */
  audit_login_status,               /* status variables                */
  audit_login_sysvars,              /* system variables                */
  NULL,
  0,
}

mysql_declare_plugin_end;

