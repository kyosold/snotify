EXEC		= daemontools src

SNOTIFY_DIR	= /var/snotify

SERVICE_DIR	= $(SNOTIFY_DIR)/service
CONTROL_DIR	= $(SNOTIFY_DIR)/conf

default:	snotify

snotify:
	for SUBDIR in $(EXEC); do \
		cd $$SUBDIR; make; cd ..; \
		done

clean:
	for SUBDIR in $(EXEC); do \
		cd $$SUBDIR; make clean; cd ..; \
		done

install:
	test -d $(SERVICE_DIR) || mkdir -p $(SERVICE_DIR)
	cp -r service/* $(SERVICE_DIR)/
	test -d $(SNOTIFY_DIR)/bin || mkdir -p $(SNOTIFY_DIR)/bin;
	cd daemontools; make prefix=$(SNOTIFY_DIR) install; cd ..;
	cd src; make prefix=$(SNOTIFY_DIR) install; cd ..;
	test -d $(SNOTIFY_DIR)/mess || mkdir -p $(SNOTIFY_DIR)/mess
	test -d $(SNOTIFY_DIR)/todo || mkdir -p $(SNOTIFY_DIR)/todo
	test -d $(SNOTIFY_DIR)/queue || mkdir -p $(SNOTIFY_DIR)/queue
	test -d $(SNOTIFY_DIR)/lock || mkdir -p $(SNOTIFY_DIR)/lock
	if [ -f $(SNOTIFY_DIR)/lock/trigger ]; then \
		mknod $(SNOTIFY_DIR)/lock/trigger p; \
	fi;

uninstall:
	rm -rf $(SNOTIFY_DIR)/bin

remove:
	rm -rf $(SNOTIFY_DIR)

