UPDATE mysql.user SET Password = PASSWORD('{{MYSQLD_PASSWORD_ROOT}}') WHERE User = 'root';
FLUSH PRIVILEGES;
