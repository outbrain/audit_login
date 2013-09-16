audit_login
===========

audit_login is a MySQL plugin developed at Outbrain which audits successful/failed logins to the server.

It is an audit plugin for MySQL. It can be loaded/unloaded as follows:

  - Dynamically:
    install plugin SIMPLE_LOGIN_AUDIT soname 'audit_login.so';
    uninstall plugin SIMPLE_LOGIN_AUDIT;

  - Statically: in my.cnf, add
    plugin_load=audit_login.so


MySQL plugins are shared libraries; compiled against the particular version of the MySQL server. Find appropriate binaries under (TODO)
Plugin source file is audit_login.c


When loaded, the plugin generates a log file named audit_login.log under the data directory (@@datadir). The following is a sample output:

```JavaScript
{"ts":"2013-09-11 09:11:47","type":"successful_login","myhost":"gromit03","thread":"3","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:11:55","type":"failed_login","myhost":"gromit03","thread":"4","user":"msandbox","priv_user":"","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:11:57","type":"failed_login","myhost":"gromit03","thread":"5","user":"msandbox","priv_user":"","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:12:48","type":"successful_login","myhost":"gromit03","thread":"10","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:13:26","type":"successful_login","myhost":"gromit03","thread":"12","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:13:44","type":"successful_login","myhost":"gromit03","thread":"1","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:13:51","type":"successful_login","myhost":"gromit03","thread":"2","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 09:14:09","type":"successful_login","myhost":"gromit03","thread":"6","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 10:55:25","type":"successful_login","myhost":"gromit03","thread":"8","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
{"ts":"2013-09-11 10:55:59","type":"successful_login","myhost":"gromit03","thread":"1","user":"msandbox","priv_user":"msandbox","host":"localhost","ip":"(null)"}
```

Fields are:

 - ts: local timestamp on MySQL server
 - type: successful_login/failed_login
 - myhost: MySQL server being audited
 - thread: MySQL connection ID
 - user: username attempted as credential
 - priv_user: username assigned by MySQL if successful; empty otherwise
 - host: host from which connection originated
 - ip: IP from which connection originated

Each row is a valid JSON object.



Configuration
-------------

The plugin supports two ways of configuration:

  - via global variables: the plugin supports the simple_login_audit_enabled variable (boolean) which enables/disables logging to file. Use set global simple_login_audit_enabled := 0; for example, to disable the log (default: 1/enabled).
  - via config file: the plugin reads the file audit_login.cnf (if exists) in the data directory. Sample file content:
        enabled=1
        skip_users=collectd,nagios
    The file is read upon plugin initialization (system startup or INSTALL PLUGIN).
    - enabled takes the values 0/1.
    - skip_users instructs the plugin to avoid logging specific users. list must be comma delimited, no spaces allowed between tokens.


Compiling plugin
----------------

Extract MySQL source distribution
    bash$ cd <path-to-extracted-mysql-source>
    bash$ mkdir -p plugin/audit_login/ && cp ~/git/audit_login/*.* plugin/audit_login/
    bash$ sh BUILD/autorun.sh
    bash$ ./configure
    bash$ make

find result shared library as plugin/audit_login/audit_login.so

Use cases
---------

The audit_login.log file is useful in:

 - Detecting failed logins (due to bad passwords)
 - Detecting port-scans (null users)
 - Finding accounts which are never used (users in mysql.user which do not appear on audit log)
 - Counting per-user logins
 - Counting successive failed logins per user





Released under the GPL (v2) license

Copyright (c) 2013 Outbrain

Written by Shlomi Noach, Outbrain

