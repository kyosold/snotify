
default = daemontools

SNOTIFY_DIR = /var/snotify
COMPILE_DIR = compile
COMMAND_DIR = $(SHARK_DIR)/command/

COMMANDS = svscan svscanboot supervise svc svok svstat fghack pgrphack readproctitle multilog tai64n tai64nlocal softlimit setuidgid envuidgid envdir setlock

daemontools:
	@(echo "start to compile daemontools");
	cp -r src $(COMPILE_DIR); 
	echo $(THIS_DIR) > $(COMPILE_DIR)/home
	@(set -e; cd $(COMPILE_DIR); \
		$(MAKE);) || exit 1;

install:
	for cmd in $(COMMANDS); do \
		cp -fp $(COMPILE_DIR)/$$cmd $(prefix)/bin/$$cmd; \
		done
	# $(prefix)/bin/setlock /etc/inittab $(COMPILE_DIR)/run.inittab

clean:
	rm -rf $(COMPILE_DIR)
