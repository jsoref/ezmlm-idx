dependon warn-auto.sh conf-qmail VERSION Makefile tests/*-*
formake '(cat warn-auto.sh; \'
formake 'echo VER=\"`head -n 1 VERSION`\"; \'
formake 'echo CONF_BIN=\"`head -n 1 conf-bin`\"; \'
formake 'cat tests/*-* ) >ezmlm-test;'
formake 'chmod 755 ezmlm-test'
